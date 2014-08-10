/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	vm/vm_pageout.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	The proverbial page-out daemon.
 */

#include <mach_pagemap.h>
#include <norma_vm.h>

#include <mach/mach_types.h>
#include <mach/memory_object.h>
#include "memory_object_default.h"
#include "memory_object_user.h"
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <kern/counters.h>
#include <kern/thread.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <machine/vm_tuning.h>



#ifndef	VM_PAGEOUT_BURST_MAX
#define	VM_PAGEOUT_BURST_MAX	10		/* number of pages */
#endif	VM_PAGEOUT_BURST_MAX

#ifndef	VM_PAGEOUT_BURST_MIN
#define	VM_PAGEOUT_BURST_MIN	5		/* number of pages */
#endif	VM_PAGEOUT_BURST_MIN

#ifndef	VM_PAGEOUT_BURST_WAIT
#define	VM_PAGEOUT_BURST_WAIT	30		/* milliseconds per page */
#endif	VM_PAGEOUT_BURST_WAIT

#ifndef	VM_PAGEOUT_EMPTY_WAIT
#define VM_PAGEOUT_EMPTY_WAIT	200		/* milliseconds */
#endif	VM_PAGEOUT_EMPTY_WAIT

#ifndef	VM_PAGEOUT_PAUSE_MAX
#define	VM_PAGEOUT_PAUSE_MAX	10		/* number of pauses */
#endif	VM_PAGEOUT_PAUSE_MAX

/*
 *	To obtain a reasonable LRU approximation, the inactive queue
 *	needs to be large enough to give pages on it a chance to be
 *	referenced a second time.  This macro defines the fraction
 *	of active+inactive pages that should be inactive.
 *	The pageout daemon uses it to update vm_page_inactive_target.
 *
 *	If vm_page_free_count falls below vm_page_free_target and
 *	vm_page_inactive_count is below vm_page_inactive_target,
 *	then the pageout daemon starts running.
 */

#ifndef	VM_PAGE_INACTIVE_TARGET
#define	VM_PAGE_INACTIVE_TARGET(avail)	((avail) * 2 / 3)
#endif	VM_PAGE_INACTIVE_TARGET

/*
 *	Once the pageout daemon starts running, it keeps going
 *	until vm_page_free_count meets or exceeds vm_page_free_target.
 */

#ifndef	VM_PAGE_FREE_TARGET
#define	VM_PAGE_FREE_TARGET(free)	(15 + (free) / 80)
#endif	VM_PAGE_FREE_TARGET

/*
 *	The pageout daemon always starts running once vm_page_free_count
 *	falls below vm_page_free_min.
 */

#ifndef	VM_PAGE_FREE_MIN
#define	VM_PAGE_FREE_MIN(free)	(10 + (free) / 100)
#endif	VM_PAGE_FREE_MIN

/*
 *	When vm_page_free_count falls below vm_page_free_reserved,
 *	only vm-privileged threads can allocate pages.  vm-privilege
 *	allows the pageout daemon and default pager (and any other
 *	associated threads needed for default pageout) to continue
 *	operation by dipping into the reserved pool of pages.
 */

#ifndef	VM_PAGE_FREE_RESERVED
#define	VM_PAGE_FREE_RESERVED			15
#endif	VM_PAGE_FREE_RESERVED

/*
 *	When vm_page_free_count falls below vm_pageout_reserved_internal,
 *	the pageout daemon no longer trusts external pagers to clean pages.
 *	External pagers are probably all wedged waiting for a free page.
 *	It forcibly double-pages dirty pages belonging to external objects,
 *	getting the pages to the default pager to clean.
 */

#ifndef	VM_PAGEOUT_RESERVED_INTERNAL
#define	VM_PAGEOUT_RESERVED_INTERNAL(reserve)	((reserve) - 5)
#endif	VM_PAGEOUT_RESERVED_INTERNAL

/*
 *	When vm_page_free_count falls below vm_pageout_reserved_really,
 *	the pageout daemon stops work entirely to let the default pager
 *	catch up (assuming the default pager has pages to clean).
 *	Beyond this point, it is too dangerous to consume memory
 *	even for memory_object_data_write messages to the default pager.
 */

#ifndef	VM_PAGEOUT_RESERVED_REALLY
#define	VM_PAGEOUT_RESERVED_REALLY(reserve)	((reserve) - 10)
#endif	VM_PAGEOUT_RESERVED_REALLY

extern void vm_pageout_continue();
extern void vm_pageout_scan_continue();

unsigned int vm_pageout_reserved_internal = 0;
unsigned int vm_pageout_reserved_really = 0;

unsigned int vm_pageout_burst_max = 0;
unsigned int vm_pageout_burst_min = 0;
unsigned int vm_pageout_burst_wait = 0;		/* milliseconds per page */
unsigned int vm_pageout_empty_wait = 0;		/* milliseconds */
unsigned int vm_pageout_pause_count = 0;
unsigned int vm_pageout_pause_max = 0;

/*
 *	These variables record the pageout daemon's actions:
 *	how many pages it looks at and what happens to those pages.
 *	No locking needed because only one thread modifies the variables.
 */

unsigned int vm_pageout_active = 0;		/* debugging */
unsigned int vm_pageout_inactive = 0;		/* debugging */
unsigned int vm_pageout_inactive_nolock = 0;	/* debugging */
unsigned int vm_pageout_inactive_busy = 0;	/* debugging */
unsigned int vm_pageout_inactive_absent = 0;	/* debugging */
unsigned int vm_pageout_inactive_used = 0;	/* debugging */
unsigned int vm_pageout_inactive_clean = 0;	/* debugging */
unsigned int vm_pageout_inactive_dirty = 0;	/* debugging */
unsigned int vm_pageout_inactive_double = 0;	/* debugging */

#if	NORMA_VM
/*
 * Define them here, since they won't be defined by memory_object_user.h.
 */
extern kern_return_t memory_object_data_initialize();
extern kern_return_t memory_object_data_write();
#endif	NORMA_VM

/*
 *	Routine:	vm_pageout_setup
 *	Purpose:
 *		Set up a page for pageout.
 *
 *		Move or copy the page to a new object, as part
 *		of which it will be sent to its memory manager
 *		in a memory_object_data_write or memory_object_initialize
 *		message.
 *
 *		The "paging_offset" argument specifies the offset
 *		of the page within its external memory object.
 *
 *		The "new_object" and "new_offset" arguments
 *		indicate where the page should be moved.
 *
 *		The "flush" argument specifies whether the page
 *		should be flushed from its object.  If not, a
 *		copy of the page is moved to the new object.
 *
 *	In/Out conditions:
 *		The page in question must not be on any pageout queues,
 *		and must be busy.  The object to which it belongs
 *		must be unlocked, and the caller must hold a paging
 *		reference to it.  The new_object must not be locked.
 *
 *		If the page is flushed from its original object,
 *		this routine returns a pointer to a place-holder page,
 *		inserted at the same offset, to block out-of-order
 *		requests for the page.  The place-holder page must
 *		be freed after the data_write or initialize message
 *		has been sent.  If the page is copied,
 *		the holding page is VM_PAGE_NULL.
 *
 *		The original page is put on a paging queue and marked
 *		not busy on exit.
 */
vm_page_t
vm_pageout_setup(m, paging_offset, new_object, new_offset, flush)
	register vm_page_t	m;
	vm_offset_t		paging_offset;
	register vm_object_t	new_object;
	vm_offset_t		new_offset;
	boolean_t		flush;
{
	register vm_object_t	old_object = m->object;
	register vm_page_t	holding_page = 0; /*'=0'to quiet gcc warnings*/
	register vm_page_t	new_m;

	assert(m->busy && !m->absent && !m->fictitious);

	/*
	 *	If we are not flushing the page, allocate a
	 *	page in the object.  If we cannot get the
	 *	page, flush instead.
	 */
	if (!flush) {
		vm_object_lock(new_object);
		new_m = vm_page_alloc(new_object, new_offset);
		if (new_m == VM_PAGE_NULL)
			flush = TRUE;
		vm_object_unlock(new_object);
	}

	if (flush) {
		/*
		 *	Create a place-holder page where the old one was,
		 *	to prevent anyone from attempting to page in this
		 *	page while we`re unlocked.
		 */
		while ((holding_page = vm_page_grab_fictitious())
							== VM_PAGE_NULL)
			vm_page_more_fictitious();

		vm_object_lock(old_object);
		vm_page_lock_queues();
		vm_page_remove(m);
		vm_page_unlock_queues();
		PAGE_WAKEUP_DONE(m);

		vm_page_lock_queues();
		vm_page_insert(holding_page, old_object, m->offset);
		vm_page_unlock_queues();

		/*
		 *	Record that this page has been written out
		 */
#if	MACH_PAGEMAP
		vm_external_state_set(old_object->existence_info,
					paging_offset,
					VM_EXTERNAL_STATE_EXISTS);
#endif	MACH_PAGEMAP

		vm_object_unlock(old_object);

		vm_object_lock(new_object);

		/*
		 *	Move this page into the new object
		 */

		vm_page_lock_queues();
		vm_page_insert(m, new_object, new_offset);
		vm_page_unlock_queues();

		m->dirty = TRUE;
		m->precious = FALSE;
		m->page_lock = VM_PROT_NONE;
		m->unlock_request = VM_PROT_NONE;
	}
	else {
		/*
		 *	Copy the data into the new page,
		 *	and mark the new page as clean.
		 */
		vm_page_copy(m, new_m);

		vm_object_lock(old_object);
		m->dirty = FALSE;
		pmap_clear_modify(m->phys_addr);

		/*
		 *	Deactivate old page.
		 */
		vm_page_lock_queues();
		vm_page_deactivate(m);
		vm_page_unlock_queues();

		PAGE_WAKEUP_DONE(m);

		/*
		 *	Record that this page has been written out
		 */

#if	MACH_PAGEMAP
		vm_external_state_set(old_object->existence_info,
					paging_offset,
					VM_EXTERNAL_STATE_EXISTS);
#endif	MACH_PAGEMAP

		vm_object_unlock(old_object);

		vm_object_lock(new_object);

		/*
		 *	Use the new page below.
		 */
		m = new_m;
		m->dirty = TRUE;
		assert(!m->precious);
		PAGE_WAKEUP_DONE(m);
	}

	/*
	 *	Make the old page eligible for replacement again; if a
	 *	user-supplied memory manager fails to release the page,
	 *	it will be paged out again to the default memory manager.
	 *
	 *	Note that pages written to the default memory manager
	 *	must be wired down -- in return, it guarantees to free
	 *	this page, rather than reusing it.
	 */

	vm_page_lock_queues();
	vm_stat.pageouts++;
	if (m->laundry) {
		/*
		 *	vm_pageout_scan is telling us to put this page
		 *	at the front of the inactive queue, so it will
		 *	be immediately paged out to the default pager.
		 */

		assert(!old_object->internal);
		m->laundry = FALSE;

		queue_enter_first(&vm_page_queue_inactive, m,
				  vm_page_t, pageq);
		m->inactive = TRUE;
		vm_page_inactive_count++;
	} else if (old_object->internal) {
		m->laundry = TRUE;
		vm_page_laundry_count++;

		vm_page_wire(m);
	} else
		vm_page_activate(m);
	vm_page_unlock_queues();

	/*
	 *	Since IPC operations may block, we drop locks now.
	 *	[The placeholder page is busy, and we still have
	 *	paging_in_progress incremented.]
	 */

	vm_object_unlock(new_object);

	/*
	 *	Return the placeholder page to simplify cleanup.
	 */
	return (flush ? holding_page : VM_PAGE_NULL);
}

/*
 *	Routine:	vm_pageout_page
 *	Purpose:
 *		Causes the specified page to be written back to
 *		the appropriate memory object.
 *
 *		The "initial" argument specifies whether this
 *		data is an initialization only, and should use
 *		memory_object_data_initialize instead of
 *		memory_object_data_write.
 *
 *		The "flush" argument specifies whether the page
 *		should be flushed from the object.  If not, a
 *		copy of the data is sent to the memory object.
 *
 *	In/out conditions:
 *		The page in question must not be on any pageout queues.
 *		The object to which it belongs must be locked.
 *	Implementation:
 *		Move this page to a completely new object, if flushing;
 *		copy to a new page in a new object, if not.
 */
void
vm_pageout_page(m, initial, flush)
	register vm_page_t	m;
	boolean_t		initial;
	boolean_t		flush;
{
	vm_map_copy_t		copy;
	register vm_object_t	old_object;
	register vm_object_t	new_object;
	register vm_page_t	holding_page;
	vm_offset_t		paging_offset;
	kern_return_t		rc;
	boolean_t		precious_clean;

	assert(m->busy);

	/*
	 *	Cleaning but not flushing a clean precious page is a
	 *	no-op.  Remember whether page is clean and precious now
	 *	because vm_pageout_setup will mark it dirty and not precious.
	 *
	 * XXX Check if precious_clean && !flush can really happen.
	 */
	precious_clean = (!m->dirty) && m->precious;
	if (precious_clean && !flush) {
		PAGE_WAKEUP_DONE(m);
		return;
	}

	/*
	 *	Verify that we really want to clean this page.
	 */
	if (m->absent || m->error || (!m->dirty && !m->precious)) {
		VM_PAGE_FREE(m);
		return;
	}

	/*
	 *	Create a paging reference to let us play with the object.
	 */
	old_object = m->object;
	paging_offset = m->offset + old_object->paging_offset;
	vm_object_paging_begin(old_object);
	vm_object_unlock(old_object);

	/*
	 *	Allocate a new object into which we can put the page.
	 */
	new_object = vm_object_allocate(PAGE_SIZE);

	/*
	 *	Move the page into the new object.
	 */
	holding_page = vm_pageout_setup(m,
				paging_offset,
				new_object,
				0,		/* new offset */
				flush);		/* flush */

	rc = vm_map_copyin_object(new_object, 0, PAGE_SIZE, &copy);
	assert(rc == KERN_SUCCESS);

	if (initial || old_object->use_old_pageout) {
		rc = (*(initial ? memory_object_data_initialize
			     : memory_object_data_write))
			(old_object->pager,
			 old_object->pager_request,
			 paging_offset, (pointer_t) copy, PAGE_SIZE);
	}
	else {
		rc = memory_object_data_return(
			 old_object->pager,
			 old_object->pager_request,
			 paging_offset, (pointer_t) copy, PAGE_SIZE,
			 !precious_clean, !flush);
	}

	if (rc != KERN_SUCCESS)
		vm_map_copy_discard(copy);

	/*
	 *	Clean up.
	 */
	vm_object_lock(old_object);
	if (holding_page != VM_PAGE_NULL)
	    VM_PAGE_FREE(holding_page);
	vm_object_paging_end(old_object);
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 *	It returns with vm_page_queue_free_lock held and
 *	vm_page_free_wanted == 0.
 */

void vm_pageout_scan()
{
	unsigned int burst_count;

	/*
	 *	We want to gradually dribble pages from the active queue
	 *	to the inactive queue.  If we let the inactive queue get
	 *	very small, and then suddenly dump many pages into it,
	 *	those pages won't get a sufficient chance to be referenced
	 *	before we start taking them from the inactive queue.
	 *
	 *	We must limit the rate at which we send pages to the pagers.
	 *	data_write messages consume memory, for message buffers and
	 *	for map-copy objects.  If we get too far ahead of the pagers,
	 *	we can potentially run out of memory.
	 *
	 *	We can use the laundry count to limit directly the number
	 *	of pages outstanding to the default pager.  A similar
	 *	strategy for external pagers doesn't work, because
	 *	external pagers don't have to deallocate the pages sent them,
	 *	and because we might have to send pages to external pagers
	 *	even if they aren't processing writes.  So we also
	 *	use a burst count to limit writes to external pagers.
	 *
	 *	When memory is very tight, we can't rely on external pagers to
	 *	clean pages.  They probably aren't running, because they
	 *	aren't vm-privileged.  If we kept sending dirty pages to them,
	 *	we could exhaust the free list.  However, we can't just ignore
	 *	pages belonging to external objects, because there might be no
	 *	pages belonging to internal objects.  Hence, we get the page
	 *	into an internal object and then immediately double-page it,
	 *	sending it to the default pager.
	 *
	 *	consider_zone_gc should be last, because the other operations
	 *	might return memory to zones.  When we pause we use
	 *	vm_pageout_scan_continue as our continuation, so we will
	 *	reenter vm_pageout_scan periodically and attempt to reclaim
	 *	internal memory even if we never reach vm_page_free_target.
	 */

    Restart:
	stack_collect();
	net_kmsg_collect();
	consider_task_collect();
	consider_thread_collect();
	consider_zone_gc();

	for (burst_count = 0;;) {
		register vm_page_t m;
		register vm_object_t object;
		unsigned int free_count;

		/*
		 *	Recalculate vm_page_inactivate_target.
		 */

		vm_page_lock_queues();
		vm_page_inactive_target =
			VM_PAGE_INACTIVE_TARGET(vm_page_active_count +
						vm_page_inactive_count);

		/*
		 *	Move pages from active to inactive.
		 */

		while ((vm_page_inactive_count < vm_page_inactive_target) &&
		       !queue_empty(&vm_page_queue_active)) {
			register vm_object_t obj;

			vm_pageout_active++;
			m = (vm_page_t) queue_first(&vm_page_queue_active);
			assert(m->active && !m->inactive);

			obj = m->object;
			if (!vm_object_lock_try(obj)) {
				/*
				 *	Move page to end and continue.
				 */

				queue_remove(&vm_page_queue_active, m,
					     vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, m,
					    vm_page_t, pageq);
				vm_page_unlock_queues();
				vm_page_lock_queues();
				continue;
			}

			/*
			 *	If the page is busy, then we pull it
			 *	off the active queue and leave it alone.
			 */

			if (m->busy) {
				vm_object_unlock(obj);
				queue_remove(&vm_page_queue_active, m,
					     vm_page_t, pageq);
				m->active = FALSE;
				vm_page_active_count--;
				continue;
			}

			/*
			 *	Deactivate the page while holding the object
			 *	locked, so we know the page is still not busy.
			 *	This should prevent races between pmap_enter
			 *	and pmap_clear_reference.  The page might be
			 *	absent or fictitious, but vm_page_deactivate
			 *	can handle that.
			 */

			vm_page_deactivate(m);
			vm_object_unlock(obj);
		}

		/*
		 *	We are done if we have met our target *and*
		 *	nobody is still waiting for a page.
		 */

		simple_lock(&vm_page_queue_free_lock);
		free_count = vm_page_free_count;
		if ((free_count >= vm_page_free_target) &
		    (vm_page_free_wanted == 0)) {
			vm_page_unlock_queues();
			break;
		}
		simple_unlock(&vm_page_queue_free_lock);

		/*
		 * Sometimes we have to pause:
		 *	1) No inactive pages - nothing to do.
		 *	2) Flow control - wait for pagers to catch up.
		 *	3) Extremely low memory - sending out dirty pages
		 *	consumes memory.  We don't take the risk of doing
		 *	this if the default pager already has work to do.
		 */

		if (queue_empty(&vm_page_queue_inactive) ||
		    (burst_count >= vm_pageout_burst_max) ||
		    (vm_page_laundry_count >= vm_pageout_burst_max) ||
		    ((free_count < vm_pageout_reserved_really) &&
		     (vm_page_laundry_count > 0))) {
			unsigned int pages, msecs;

			/*
			 *	vm_pageout_burst_wait is msecs/page.
			 *	If there is nothing for us to do, we wait
			 *	at least vm_pageout_empty_wait msecs.
			 */

			if (vm_page_laundry_count > burst_count)
				pages = vm_page_laundry_count;
			else
				pages = burst_count;
			msecs = pages * vm_pageout_burst_wait;

			if (queue_empty(&vm_page_queue_inactive) &&
			    (msecs < vm_pageout_empty_wait))
				msecs = vm_pageout_empty_wait;
			vm_page_unlock_queues();

			thread_will_wait_with_timeout(current_thread(), msecs);
			counter(c_vm_pageout_scan_block++);
			thread_block(vm_pageout_scan_continue);
#ifndef CONTINUATIONS
			/*
			 *	Unfortunately, we don't have call_continuation
			 *	so we can't rely on tail-recursion.
			 */

			vm_pageout_scan_continue();
			goto Restart;
#else	/* CONTINUATIONS */
			call_continuation(vm_pageout_scan_continue);
			/*NOTREACHED*/
#endif	/* CONTINUATIONS */
		}

		vm_pageout_inactive++;
		m = (vm_page_t) queue_first(&vm_page_queue_inactive);
		assert(!m->active && m->inactive);
		object = m->object;

		/*
		 *	Try to lock object; since we've got the
		 *	page queues lock, we can only try for this one.
		 */

		if (!vm_object_lock_try(object)) {
			/*
			 *	Move page to end and continue.
			 */

			queue_remove(&vm_page_queue_inactive, m,
				     vm_page_t, pageq);
			queue_enter(&vm_page_queue_inactive, m,
				    vm_page_t, pageq);
			vm_page_unlock_queues();
			vm_pageout_inactive_nolock++;
			continue;
		}

		/*
		 *	Remove the page from the inactive list.
		 */

		queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
		vm_page_inactive_count--;
		m->inactive = FALSE;

		if (m->busy || !object->alive) {
			/*
			 *	Somebody is already playing with this page.
			 *	Leave it off the pageout queues.
			 */

			vm_page_unlock_queues();
			vm_object_unlock(object);
			vm_pageout_inactive_busy++;
			continue;
		}

		/*
		 *	If it's absent, we can reclaim the page.
		 */

		if (m->absent) {
			vm_pageout_inactive_absent++;
		    reclaim_page:
			vm_page_free(m);
			vm_page_unlock_queues();
			vm_object_unlock(object);
			continue;
		}

		/*
		 *	If it's being used, reactivate.
		 *	(Fictitious pages are either busy or absent.)
		 */

		assert(!m->fictitious);
		if (m->reference || pmap_is_referenced(m->phys_addr)) {
			vm_object_unlock(object);
			vm_page_activate(m);
			vm_stat.reactivations++;
			vm_page_unlock_queues();
			vm_pageout_inactive_used++;
			continue;
		}

		/*
		 *	Eliminate all mappings.
		 */

		m->busy = TRUE;
		pmap_page_protect(m->phys_addr, VM_PROT_NONE);
		if (!m->dirty)
			m->dirty = pmap_is_modified(m->phys_addr);

		/*
		 *	If it's clean and not precious, we can free the page.
		 */

		if (!m->dirty && !m->precious) {
			vm_pageout_inactive_clean++;
			goto reclaim_page;
		}

		/*
		 *	If we are very low on memory, then we can't
		 *	rely on an external pager to clean a dirty page,
		 *	because external pagers are not vm-privileged.
		 *
		 *	The laundry bit tells vm_pageout_setup to
		 *	put the page back at the front of the inactive
		 *	queue instead of activating the page.  Hence,
		 *	we will pick the page up again immediately and
		 *	resend it to the default pager.
		 */

		assert(!m->laundry);
		if ((free_count < vm_pageout_reserved_internal) &&
		    !object->internal) {
			m->laundry = TRUE;
			vm_pageout_inactive_double++;
		}
		vm_page_unlock_queues();

		/*
		 *	If there is no memory object for the page, create
		 *	one and hand it to the default pager.
		 *	[First try to collapse, so we don't create
		 *	one unnecessarily.]
		 */

		if (!object->pager_initialized)
			vm_object_collapse(object);
		if (!object->pager_initialized)
			vm_object_pager_create(object);
		if (!object->pager_initialized)
			panic("vm_pageout_scan");

		vm_pageout_inactive_dirty++;
		vm_pageout_page(m, FALSE, TRUE);	/* flush it */
		vm_object_unlock(object);
		burst_count++;
	}
}

void vm_pageout_scan_continue()
{
	/*
	 *	We just paused to let the pagers catch up.
	 *	If vm_page_laundry_count is still high,
	 *	then we aren't waiting long enough.
	 *	If we have paused some vm_pageout_pause_max times without
	 *	adjusting vm_pageout_burst_wait, it might be too big,
	 *	so we decrease it.
	 */

	vm_page_lock_queues();
	if (vm_page_laundry_count > vm_pageout_burst_min) {
		vm_pageout_burst_wait++;
		vm_pageout_pause_count = 0;
	} else if (++vm_pageout_pause_count > vm_pageout_pause_max) {
		vm_pageout_burst_wait = (vm_pageout_burst_wait * 3) / 4;
		if (vm_pageout_burst_wait < 1)
			vm_pageout_burst_wait = 1;
		vm_pageout_pause_count = 0;
	}
	vm_page_unlock_queues();

#ifdef	CONTINUATIONS
	vm_pageout_continue();
	/*NOTREACHED*/
#endif	/* CONTINUATIONS */
}

/*
 *	vm_pageout is the high level pageout daemon.
 */

void vm_pageout_continue()
{
	/*
	 *	The pageout daemon is never done, so loop forever.
	 *	We should call vm_pageout_scan at least once each
	 *	time we are woken, even if vm_page_free_wanted is
	 *	zero, to check vm_page_free_target and
	 *	vm_page_inactive_target.
	 */

	for (;;) {
		vm_pageout_scan();
		/* we hold vm_page_queue_free_lock now */
		assert(vm_page_free_wanted == 0);

		assert_wait(&vm_page_free_wanted, FALSE);
		simple_unlock(&vm_page_queue_free_lock);
		counter(c_vm_pageout_block++);
		thread_block(vm_pageout_continue);
	}
}

void vm_pageout()
{
	int		free_after_reserve;

	current_thread()->vm_privilege = TRUE;
	stack_privilege(current_thread());

	/*
	 *	Initialize some paging parameters.
	 */

	if (vm_pageout_burst_max == 0)
		vm_pageout_burst_max = VM_PAGEOUT_BURST_MAX;

	if (vm_pageout_burst_min == 0)
		vm_pageout_burst_min = VM_PAGEOUT_BURST_MIN;

	if (vm_pageout_burst_wait == 0)
		vm_pageout_burst_wait = VM_PAGEOUT_BURST_WAIT;

	if (vm_pageout_empty_wait == 0)
		vm_pageout_empty_wait = VM_PAGEOUT_EMPTY_WAIT;

	if (vm_page_free_reserved == 0)
		vm_page_free_reserved = VM_PAGE_FREE_RESERVED;

	if (vm_pageout_pause_max == 0)
		vm_pageout_pause_max = VM_PAGEOUT_PAUSE_MAX;

	if (vm_pageout_reserved_internal == 0)
		vm_pageout_reserved_internal =
			VM_PAGEOUT_RESERVED_INTERNAL(vm_page_free_reserved);

	if (vm_pageout_reserved_really == 0)
		vm_pageout_reserved_really =
			VM_PAGEOUT_RESERVED_REALLY(vm_page_free_reserved);

	free_after_reserve = vm_page_free_count - vm_page_free_reserved;

	if (vm_page_free_min == 0)
		vm_page_free_min = vm_page_free_reserved +
			VM_PAGE_FREE_MIN(free_after_reserve);

	if (vm_page_free_target == 0)
		vm_page_free_target = vm_page_free_reserved +
			VM_PAGE_FREE_TARGET(free_after_reserve);

	if (vm_page_free_target < vm_page_free_min + 5)
		vm_page_free_target = vm_page_free_min + 5;

	/*
	 *	vm_pageout_scan will set vm_page_inactive_target.
	 */

	vm_pageout_continue();
	/*NOTREACHED*/
}
