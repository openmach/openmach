/*
 * Mach Operating System
 * Copyright (c) 1994,1990,1989,1988,1987 Carnegie Mellon University.
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
 *	File:	vm_fault.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Page fault handling module.
 */
#include <mach_pagemap.h>
#include <mach_kdb.h>
#include <mach_pcsample.h>


#include <vm/vm_fault.h>
#include <mach/kern_return.h>
#include <mach/message.h>	/* for error codes */
#include <kern/counters.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <mach/vm_statistics.h>
#include <vm/vm_pageout.h>
#include <mach/vm_param.h>
#include <mach/memory_object.h>
#include "memory_object_user.h"
				/* For memory_object_data_{request,unlock} */
#include <kern/mach_param.h>
#include <kern/macro_help.h>
#include <kern/zalloc.h>

#if	MACH_PCSAMPLE
#include <kern/pc_sample.h>
#endif



/*
 *	State needed by vm_fault_continue.
 *	This is a little hefty to drop directly
 *	into the thread structure.
 */
typedef struct vm_fault_state {
	struct vm_map *vmf_map;
	vm_offset_t vmf_vaddr;
	vm_prot_t vmf_fault_type;
	boolean_t vmf_change_wiring;
	void (*vmf_continuation)();
	vm_map_version_t vmf_version;
	boolean_t vmf_wired;
	struct vm_object *vmf_object;
	vm_offset_t vmf_offset;
	vm_prot_t vmf_prot;

	boolean_t vmfp_backoff;
	struct vm_object *vmfp_object;
	vm_offset_t vmfp_offset;
	struct vm_page *vmfp_first_m;
	vm_prot_t vmfp_access;
} vm_fault_state_t;

zone_t		vm_fault_state_zone = 0;

int		vm_object_absent_max = 50;

int		vm_fault_debug = 0;

boolean_t	vm_fault_dirty_handling = FALSE;
boolean_t	vm_fault_interruptible = TRUE;

boolean_t	software_reference_bits = TRUE;

#if	MACH_KDB
extern struct db_watchpoint *db_watchpoint_list;
#endif	MACH_KDB

/*
 *	Routine:	vm_fault_init
 *	Purpose:
 *		Initialize our private data structures.
 */
void vm_fault_init()
{
	vm_fault_state_zone = zinit(sizeof(vm_fault_state_t),
				    THREAD_MAX * sizeof(vm_fault_state_t),
				    sizeof(vm_fault_state_t),
				    0, "vm fault state");
}

/*
 *	Routine:	vm_fault_cleanup
 *	Purpose:
 *		Clean up the result of vm_fault_page.
 *	Results:
 *		The paging reference for "object" is released.
 *		"object" is unlocked.
 *		If "top_page" is not null,  "top_page" is
 *		freed and the paging reference for the object
 *		containing it is released.
 *
 *	In/out conditions:
 *		"object" must be locked.
 */
void
vm_fault_cleanup(object, top_page)
	register vm_object_t	object;
	register vm_page_t	top_page;
{
	vm_object_paging_end(object);
	vm_object_unlock(object);

	if (top_page != VM_PAGE_NULL) {
	    object = top_page->object;
	    vm_object_lock(object);
	    VM_PAGE_FREE(top_page);
	    vm_object_paging_end(object);
	    vm_object_unlock(object);
	}
}


#if	MACH_PCSAMPLE
/*
 *	Do PC sampling on current thread, assuming
 *	that it is the thread taking this page fault.
 *
 *	Must check for THREAD_NULL, since faults
 *	can occur before threads are running.
 */

#define	vm_stat_sample(flavor) \
    MACRO_BEGIN \
      thread_t _thread_ = current_thread(); \
 \
      if (_thread_ != THREAD_NULL) \
	  take_pc_sample_macro(_thread_, (flavor)); \
    MACRO_END

#else
#define	vm_stat_sample(x)
#endif	/* MACH_PCSAMPLE */



/*
 *	Routine:	vm_fault_page
 *	Purpose:
 *		Find the resident page for the virtual memory
 *		specified by the given virtual memory object
 *		and offset.
 *	Additional arguments:
 *		The required permissions for the page is given
 *		in "fault_type".  Desired permissions are included
 *		in "protection".
 *
 *		If the desired page is known to be resident (for
 *		example, because it was previously wired down), asserting
 *		the "unwiring" parameter will speed the search.
 *
 *		If the operation can be interrupted (by thread_abort
 *		or thread_terminate), then the "interruptible"
 *		parameter should be asserted.
 *
 *	Results:
 *		The page containing the proper data is returned
 *		in "result_page".
 *
 *	In/out conditions:
 *		The source object must be locked and referenced,
 *		and must donate one paging reference.  The reference
 *		is not affected.  The paging reference and lock are
 *		consumed.
 *
 *		If the call succeeds, the object in which "result_page"
 *		resides is left locked and holding a paging reference.
 *		If this is not the original object, a busy page in the
 *		original object is returned in "top_page", to prevent other
 *		callers from pursuing this same data, along with a paging
 *		reference for the original object.  The "top_page" should
 *		be destroyed when this guarantee is no longer required.
 *		The "result_page" is also left busy.  It is not removed
 *		from the pageout queues.
 */
vm_fault_return_t vm_fault_page(first_object, first_offset,
				fault_type, must_be_resident, interruptible,
				protection,
				result_page, top_page,
				resume, continuation)
 /* Arguments: */
	vm_object_t	first_object;	/* Object to begin search */
	vm_offset_t	first_offset;	/* Offset into object */
	vm_prot_t	fault_type;	/* What access is requested */
	boolean_t	must_be_resident;/* Must page be resident? */
	boolean_t	interruptible;	/* May fault be interrupted? */
 /* Modifies in place: */
	vm_prot_t	*protection;	/* Protection for mapping */
 /* Returns: */
	vm_page_t	*result_page;	/* Page found, if successful */
	vm_page_t	*top_page;	/* Page in top object, if
					 * not result_page.
					 */
 /* More arguments: */
	boolean_t	resume;		/* We are restarting. */
	void		(*continuation)(); /* Continuation for blocking. */
{
	register
	vm_page_t	m;
	register
	vm_object_t	object;
	register
	vm_offset_t	offset;
	vm_page_t	first_m;
	vm_object_t	next_object;
	vm_object_t	copy_object;
	boolean_t	look_for_page;
	vm_prot_t	access_required;

#ifdef CONTINUATIONS
	if (resume) {
		register vm_fault_state_t *state =
			(vm_fault_state_t *) current_thread()->ith_other;

		if (state->vmfp_backoff)
			goto after_block_and_backoff;

		object = state->vmfp_object;
		offset = state->vmfp_offset;
		first_m = state->vmfp_first_m;
		access_required = state->vmfp_access;
		goto after_thread_block;
	}
#else /* not CONTINUATIONS */
	assert(continuation == 0);
	assert(!resume);
#endif /* not CONTINUATIONS */

	vm_stat_sample(SAMPLED_PC_VM_FAULTS_ANY);
	vm_stat.faults++;		/* needs lock XXX */

/*
 *	Recovery actions
 */
#define RELEASE_PAGE(m)					\
	MACRO_BEGIN					\
	PAGE_WAKEUP_DONE(m);				\
	vm_page_lock_queues();				\
	if (!m->active && !m->inactive)			\
		vm_page_activate(m);			\
	vm_page_unlock_queues();			\
	MACRO_END

	if (vm_fault_dirty_handling
#if	MACH_KDB
		/*
		 *	If there are watchpoints set, then
		 *	we don't want to give away write permission
		 *	on a read fault.  Make the task write fault,
		 *	so that the watchpoint code notices the access.
		 */
	    || db_watchpoint_list
#endif	MACH_KDB
	    ) {
		/*
		 *	If we aren't asking for write permission,
		 *	then don't give it away.  We're using write
		 *	faults to set the dirty bit.
		 */
		if (!(fault_type & VM_PROT_WRITE))
			*protection &= ~VM_PROT_WRITE;
	}

	if (!vm_fault_interruptible)
		interruptible = FALSE;

	/*
	 *	INVARIANTS (through entire routine):
	 *
	 *	1)	At all times, we must either have the object
	 *		lock or a busy page in some object to prevent
	 *		some other thread from trying to bring in
	 *		the same page.
	 *
	 *		Note that we cannot hold any locks during the
	 *		pager access or when waiting for memory, so
	 *		we use a busy page then.
	 *
	 *		Note also that we aren't as concerned about more than
	 *		one thread attempting to memory_object_data_unlock
	 *		the same page at once, so we don't hold the page
	 *		as busy then, but do record the highest unlock
	 *		value so far.  [Unlock requests may also be delivered
	 *		out of order.]
	 *
	 *	2)	To prevent another thread from racing us down the
	 *		shadow chain and entering a new page in the top
	 *		object before we do, we must keep a busy page in
	 *		the top object while following the shadow chain.
	 *
	 *	3)	We must increment paging_in_progress on any object
	 *		for which we have a busy page, to prevent
	 *		vm_object_collapse from removing the busy page
	 *		without our noticing.
	 *
	 *	4)	We leave busy pages on the pageout queues.
	 *		If the pageout daemon comes across a busy page,
	 *		it will remove the page from the pageout queues.
	 */

	/*
	 *	Search for the page at object/offset.
	 */

	object = first_object;
	offset = first_offset;
	first_m = VM_PAGE_NULL;
	access_required = fault_type;

	/*
	 *	See whether this page is resident
	 */

	while (TRUE) {
		m = vm_page_lookup(object, offset);
		if (m != VM_PAGE_NULL) {
			/*
			 *	If the page is being brought in,
			 *	wait for it and then retry.
			 *
			 *	A possible optimization: if the page
			 *	is known to be resident, we can ignore
			 *	pages that are absent (regardless of
			 *	whether they're busy).
			 */

			if (m->busy) {
				kern_return_t	wait_result;

				PAGE_ASSERT_WAIT(m, interruptible);
				vm_object_unlock(object);
#ifdef CONTINUATIONS
				if (continuation != (void (*)()) 0) {
					register vm_fault_state_t *state =
						(vm_fault_state_t *) current_thread()->ith_other;

					/*
					 *	Save variables in case
					 *	thread_block discards
					 *	our kernel stack.
					 */

					state->vmfp_backoff = FALSE;
					state->vmfp_object = object;
					state->vmfp_offset = offset;
					state->vmfp_first_m = first_m;
					state->vmfp_access =
						access_required;
					state->vmf_prot = *protection;

					counter(c_vm_fault_page_block_busy_user++);
					thread_block(continuation);
				} else
#endif /* CONTINUATIONS */
				{
					counter(c_vm_fault_page_block_busy_kernel++);
					thread_block((void (*)()) 0);
				}
			    after_thread_block:
				wait_result = current_thread()->wait_result;
				vm_object_lock(object);
				if (wait_result != THREAD_AWAKENED) {
					vm_fault_cleanup(object, first_m);
					if (wait_result == THREAD_RESTART)
						return(VM_FAULT_RETRY);
					else
						return(VM_FAULT_INTERRUPTED);
				}
				continue;
			}

			/*
			 *	If the page is in error, give up now.
			 */

			if (m->error) {
				VM_PAGE_FREE(m);
				vm_fault_cleanup(object, first_m);
				return(VM_FAULT_MEMORY_ERROR);
			}

			/*
			 *	If the page isn't busy, but is absent,
			 *	then it was deemed "unavailable".
			 */

			if (m->absent) {
				/*
				 * Remove the non-existent page (unless it's
				 * in the top object) and move on down to the
				 * next object (if there is one).
				 */

				offset += object->shadow_offset;
				access_required = VM_PROT_READ;
				next_object = object->shadow;
				if (next_object == VM_OBJECT_NULL) {
					vm_page_t real_m;

					assert(!must_be_resident);

					/*
					 * Absent page at bottom of shadow
					 * chain; zero fill the page we left
					 * busy in the first object, and flush
					 * the absent page.  But first we
					 * need to allocate a real page.
					 */

					real_m = vm_page_grab();
					if (real_m == VM_PAGE_NULL) {
						vm_fault_cleanup(object, first_m);
						return(VM_FAULT_MEMORY_SHORTAGE);
					}

					if (object != first_object) {
						VM_PAGE_FREE(m);
						vm_object_paging_end(object);
						vm_object_unlock(object);
						object = first_object;
						offset = first_offset;
						m = first_m;
						first_m = VM_PAGE_NULL;
						vm_object_lock(object);
					}

					VM_PAGE_FREE(m);
					assert(real_m->busy);
					vm_page_lock_queues();
					vm_page_insert(real_m, object, offset);
					vm_page_unlock_queues();
					m = real_m;

					/*
					 *  Drop the lock while zero filling
					 *  page.  Then break because this
					 *  is the page we wanted.  Checking
					 *  the page lock is a waste of time;
					 *  this page was either absent or
					 *  newly allocated -- in both cases
					 *  it can't be page locked by a pager.
					 */
					vm_object_unlock(object);

					vm_page_zero_fill(m);

					vm_stat_sample(SAMPLED_PC_VM_ZFILL_FAULTS);
					
					vm_stat.zero_fill_count++;
					vm_object_lock(object);
					pmap_clear_modify(m->phys_addr);
					break;
				} else {
					if (must_be_resident) {
						vm_object_paging_end(object);
					} else if (object != first_object) {
						vm_object_paging_end(object);
						VM_PAGE_FREE(m);
					} else {
						first_m = m;
						m->absent = FALSE;
						vm_object_absent_release(object);
						m->busy = TRUE;

						vm_page_lock_queues();
						VM_PAGE_QUEUES_REMOVE(m);
						vm_page_unlock_queues();
					}
					vm_object_lock(next_object);
					vm_object_unlock(object);
					object = next_object;
					vm_object_paging_begin(object);
					continue;
				}
			}

			/*
			 *	If the desired access to this page has
			 *	been locked out, request that it be unlocked.
			 */

			if (access_required & m->page_lock) {
				if ((access_required & m->unlock_request) != access_required) {
					vm_prot_t	new_unlock_request;
					kern_return_t	rc;
					
					if (!object->pager_ready) {
						vm_object_assert_wait(object,
							VM_OBJECT_EVENT_PAGER_READY,
							interruptible);
						goto block_and_backoff;
					}

					new_unlock_request = m->unlock_request =
						(access_required | m->unlock_request);
					vm_object_unlock(object);
					if ((rc = memory_object_data_unlock(
						object->pager,
						object->pager_request,
						offset + object->paging_offset,
						PAGE_SIZE,
						new_unlock_request))
					     != KERN_SUCCESS) {
					     	printf("vm_fault: memory_object_data_unlock failed\n");
						vm_object_lock(object);
						vm_fault_cleanup(object, first_m);
						return((rc == MACH_SEND_INTERRUPTED) ?
							VM_FAULT_INTERRUPTED :
							VM_FAULT_MEMORY_ERROR);
					}
					vm_object_lock(object);
					continue;
				}

				PAGE_ASSERT_WAIT(m, interruptible);
				goto block_and_backoff;
			}

			/*
			 *	We mark the page busy and leave it on
			 *	the pageout queues.  If the pageout
			 *	deamon comes across it, then it will
			 *	remove the page.
			 */

			if (!software_reference_bits) {
				vm_page_lock_queues();
				if (m->inactive)  {
				    	vm_stat_sample(SAMPLED_PC_VM_REACTIVATION_FAULTS);
					vm_stat.reactivations++;
				}

				VM_PAGE_QUEUES_REMOVE(m);
				vm_page_unlock_queues();
			}

			assert(!m->busy);
			m->busy = TRUE;
			assert(!m->absent);
			break;
		}

		look_for_page =
			(object->pager_created)
#if	MACH_PAGEMAP
			&& (vm_external_state_get(object->existence_info, offset + object->paging_offset) !=
			 VM_EXTERNAL_STATE_ABSENT)
#endif	MACH_PAGEMAP
			 ;

		if ((look_for_page || (object == first_object))
				 && !must_be_resident) {
			/*
			 *	Allocate a new page for this object/offset
			 *	pair.
			 */

			m = vm_page_grab_fictitious();
			if (m == VM_PAGE_NULL) {
				vm_fault_cleanup(object, first_m);
				return(VM_FAULT_FICTITIOUS_SHORTAGE);
			}

			vm_page_lock_queues();
			vm_page_insert(m, object, offset);
			vm_page_unlock_queues();
		}

		if (look_for_page && !must_be_resident) {
			kern_return_t	rc;

			/*
			 *	If the memory manager is not ready, we
			 *	cannot make requests.
			 */
			if (!object->pager_ready) {
				vm_object_assert_wait(object,
					VM_OBJECT_EVENT_PAGER_READY,
					interruptible);
				VM_PAGE_FREE(m);
				goto block_and_backoff;
			}

			if (object->internal) {
				/*
				 *	Requests to the default pager
				 *	must reserve a real page in advance,
				 *	because the pager's data-provided
				 *	won't block for pages.
				 */

				if (m->fictitious && !vm_page_convert(m)) {
					VM_PAGE_FREE(m);
					vm_fault_cleanup(object, first_m);
					return(VM_FAULT_MEMORY_SHORTAGE);
				}
			} else if (object->absent_count >
						vm_object_absent_max) {
				/*
				 *	If there are too many outstanding page
				 *	requests pending on this object, we
				 *	wait for them to be resolved now.
				 */

				vm_object_absent_assert_wait(object, interruptible);
				VM_PAGE_FREE(m);
				goto block_and_backoff;
			}

			/*
			 *	Indicate that the page is waiting for data
			 *	from the memory manager.
			 */

			m->absent = TRUE;
			object->absent_count++;

			/*
			 *	We have a busy page, so we can
			 *	release the object lock.
			 */
			vm_object_unlock(object);

			/*
			 *	Call the memory manager to retrieve the data.
			 */

			vm_stat.pageins++;
		    	vm_stat_sample(SAMPLED_PC_VM_PAGEIN_FAULTS);

			if ((rc = memory_object_data_request(object->pager, 
				object->pager_request,
				m->offset + object->paging_offset, 
				PAGE_SIZE, access_required)) != KERN_SUCCESS) {
				if (rc != MACH_SEND_INTERRUPTED)
					printf("%s(0x%x, 0x%x, 0x%x, 0x%x, 0x%x) failed, %d\n",
						"memory_object_data_request",
						object->pager,
						object->pager_request,
						m->offset + object->paging_offset, 
						PAGE_SIZE, access_required, rc);
				/*
				 *	Don't want to leave a busy page around,
				 *	but the data request may have blocked,
				 *	so check if it's still there and busy.
				 */
				vm_object_lock(object);
				if (m == vm_page_lookup(object,offset) &&
				    m->absent && m->busy)
					VM_PAGE_FREE(m);
				vm_fault_cleanup(object, first_m);
				return((rc == MACH_SEND_INTERRUPTED) ?
					VM_FAULT_INTERRUPTED :
					VM_FAULT_MEMORY_ERROR);
			}
			
			/*
			 * Retry with same object/offset, since new data may
			 * be in a different page (i.e., m is meaningless at
			 * this point).
			 */
			vm_object_lock(object);
			continue;
		}

		/*
		 * For the XP system, the only case in which we get here is if
		 * object has no pager (or unwiring).  If the pager doesn't
		 * have the page this is handled in the m->absent case above
		 * (and if you change things here you should look above).
		 */
		if (object == first_object)
			first_m = m;
		else
		{
			assert(m == VM_PAGE_NULL);
		}

		/*
		 *	Move on to the next object.  Lock the next
		 *	object before unlocking the current one.
		 */
		access_required = VM_PROT_READ;

		offset += object->shadow_offset;
		next_object = object->shadow;
		if (next_object == VM_OBJECT_NULL) {
			assert(!must_be_resident);

			/*
			 *	If there's no object left, fill the page
			 *	in the top object with zeros.  But first we
			 *	need to allocate a real page.
			 */

			if (object != first_object) {
				vm_object_paging_end(object);
				vm_object_unlock(object);

				object = first_object;
				offset = first_offset;
				vm_object_lock(object);
			}

			m = first_m;
			assert(m->object == object);
			first_m = VM_PAGE_NULL;

			if (m->fictitious && !vm_page_convert(m)) {
				VM_PAGE_FREE(m);
				vm_fault_cleanup(object, VM_PAGE_NULL);
				return(VM_FAULT_MEMORY_SHORTAGE);
			}

			vm_object_unlock(object);
			vm_page_zero_fill(m);
			vm_stat_sample(SAMPLED_PC_VM_ZFILL_FAULTS);
			vm_stat.zero_fill_count++;
			vm_object_lock(object);
			pmap_clear_modify(m->phys_addr);
			break;
		}
		else {
			vm_object_lock(next_object);
			if ((object != first_object) || must_be_resident)
				vm_object_paging_end(object);
			vm_object_unlock(object);
			object = next_object;
			vm_object_paging_begin(object);
		}
	}

	/*
	 *	PAGE HAS BEEN FOUND.
	 *
	 *	This page (m) is:
	 *		busy, so that we can play with it;
	 *		not absent, so that nobody else will fill it;
	 *		possibly eligible for pageout;
	 *
	 *	The top-level page (first_m) is:
	 *		VM_PAGE_NULL if the page was found in the
	 *		 top-level object;
	 *		busy, not absent, and ineligible for pageout.
	 *
	 *	The current object (object) is locked.  A paging
	 *	reference is held for the current and top-level
	 *	objects.
	 */

#if	EXTRA_ASSERTIONS
	assert(m->busy && !m->absent);
	assert((first_m == VM_PAGE_NULL) ||
		(first_m->busy && !first_m->absent &&
		 !first_m->active && !first_m->inactive));
#endif	EXTRA_ASSERTIONS

	/*
	 *	If the page is being written, but isn't
	 *	already owned by the top-level object,
	 *	we have to copy it into a new page owned
	 *	by the top-level object.
	 */

	if (object != first_object) {
	    	/*
		 *	We only really need to copy if we
		 *	want to write it.
		 */

	    	if (fault_type & VM_PROT_WRITE) {
			vm_page_t copy_m;

			assert(!must_be_resident);

			/*
			 *	If we try to collapse first_object at this
			 *	point, we may deadlock when we try to get
			 *	the lock on an intermediate object (since we
			 *	have the bottom object locked).  We can't
			 *	unlock the bottom object, because the page
			 *	we found may move (by collapse) if we do.
			 *
			 *	Instead, we first copy the page.  Then, when
			 *	we have no more use for the bottom object,
			 *	we unlock it and try to collapse.
			 *
			 *	Note that we copy the page even if we didn't
			 *	need to... that's the breaks.
			 */

			/*
			 *	Allocate a page for the copy
			 */
			copy_m = vm_page_grab();
			if (copy_m == VM_PAGE_NULL) {
				RELEASE_PAGE(m);
				vm_fault_cleanup(object, first_m);
				return(VM_FAULT_MEMORY_SHORTAGE);
			}

			vm_object_unlock(object);
			vm_page_copy(m, copy_m);
			vm_object_lock(object);

			/*
			 *	If another map is truly sharing this
			 *	page with us, we have to flush all
			 *	uses of the original page, since we
			 *	can't distinguish those which want the
			 *	original from those which need the
			 *	new copy.
			 *
			 *	XXXO If we know that only one map has
			 *	access to this page, then we could
			 *	avoid the pmap_page_protect() call.
			 */

			vm_page_lock_queues();
			vm_page_deactivate(m);
			pmap_page_protect(m->phys_addr, VM_PROT_NONE);
			vm_page_unlock_queues();

			/*
			 *	We no longer need the old page or object.
			 */

			PAGE_WAKEUP_DONE(m);
			vm_object_paging_end(object);
			vm_object_unlock(object);

			vm_stat.cow_faults++;
			vm_stat_sample(SAMPLED_PC_VM_COW_FAULTS);
			object = first_object;
			offset = first_offset;

			vm_object_lock(object);
			VM_PAGE_FREE(first_m);
			first_m = VM_PAGE_NULL;
			assert(copy_m->busy);
			vm_page_lock_queues();
			vm_page_insert(copy_m, object, offset);
			vm_page_unlock_queues();
			m = copy_m;

			/*
			 *	Now that we've gotten the copy out of the
			 *	way, let's try to collapse the top object.
			 *	But we have to play ugly games with
			 *	paging_in_progress to do that...
			 */

			vm_object_paging_end(object);
			vm_object_collapse(object);
			vm_object_paging_begin(object);
		}
		else {
		    	*protection &= (~VM_PROT_WRITE);
		}
	}

	/*
	 *	Now check whether the page needs to be pushed into the
	 *	copy object.  The use of asymmetric copy on write for
	 *	shared temporary objects means that we may do two copies to
	 *	satisfy the fault; one above to get the page from a
	 *	shadowed object, and one here to push it into the copy.
	 */

	while ((copy_object = first_object->copy) != VM_OBJECT_NULL) {
		vm_offset_t	copy_offset;
		vm_page_t	copy_m;

		/*
		 *	If the page is being written, but hasn't been
		 *	copied to the copy-object, we have to copy it there.
		 */

		if ((fault_type & VM_PROT_WRITE) == 0) {
			*protection &= ~VM_PROT_WRITE;
			break;
		}

		/*
		 *	If the page was guaranteed to be resident,
		 *	we must have already performed the copy.
		 */

		if (must_be_resident)
			break;

		/*
		 *	Try to get the lock on the copy_object.
		 */
		if (!vm_object_lock_try(copy_object)) {
			vm_object_unlock(object);

			simple_lock_pause();	/* wait a bit */

			vm_object_lock(object);
			continue;
		}

		/*
		 *	Make another reference to the copy-object,
		 *	to keep it from disappearing during the
		 *	copy.
		 */
		assert(copy_object->ref_count > 0);
		copy_object->ref_count++;

		/*
		 *	Does the page exist in the copy?
		 */
		copy_offset = first_offset - copy_object->shadow_offset;
		copy_m = vm_page_lookup(copy_object, copy_offset);
		if (copy_m != VM_PAGE_NULL) {
			if (copy_m->busy) {
				/*
				 *	If the page is being brought
				 *	in, wait for it and then retry.
				 */
				PAGE_ASSERT_WAIT(copy_m, interruptible);
				RELEASE_PAGE(m);
				copy_object->ref_count--;
				assert(copy_object->ref_count > 0);
				vm_object_unlock(copy_object);
				goto block_and_backoff;
			}
		}
		else {
			/*
			 *	Allocate a page for the copy
			 */
			copy_m = vm_page_alloc(copy_object, copy_offset);
			if (copy_m == VM_PAGE_NULL) {
				RELEASE_PAGE(m);
				copy_object->ref_count--;
				assert(copy_object->ref_count > 0);
				vm_object_unlock(copy_object);
				vm_fault_cleanup(object, first_m);
				return(VM_FAULT_MEMORY_SHORTAGE);
			}

			/*
			 *	Must copy page into copy-object.
			 */

			vm_page_copy(m, copy_m);
			
			/*
			 *	If the old page was in use by any users
			 *	of the copy-object, it must be removed
			 *	from all pmaps.  (We can't know which
			 *	pmaps use it.)
			 */

			vm_page_lock_queues();
			pmap_page_protect(m->phys_addr, VM_PROT_NONE);
			copy_m->dirty = TRUE;
			vm_page_unlock_queues();

			/*
			 *	If there's a pager, then immediately
			 *	page out this page, using the "initialize"
			 *	option.  Else, we use the copy.
			 */

		 	if (!copy_object->pager_created) {
				vm_page_lock_queues();
				vm_page_activate(copy_m);
				vm_page_unlock_queues();
				PAGE_WAKEUP_DONE(copy_m);
			} else {
				/*
				 *	The page is already ready for pageout:
				 *	not on pageout queues and busy.
				 *	Unlock everything except the
				 *	copy_object itself.
				 */

				vm_object_unlock(object);

				/*
				 *	Write the page to the copy-object,
				 *	flushing it from the kernel.
				 */

				vm_pageout_page(copy_m, TRUE, TRUE);

				/*
				 *	Since the pageout may have
				 *	temporarily dropped the
				 *	copy_object's lock, we
				 *	check whether we'll have
				 *	to deallocate the hard way.
				 */

				if ((copy_object->shadow != object) ||
				    (copy_object->ref_count == 1)) {
					vm_object_unlock(copy_object);
					vm_object_deallocate(copy_object);
					vm_object_lock(object);
					continue;
				}

				/*
				 *	Pick back up the old object's
				 *	lock.  [It is safe to do so,
				 *	since it must be deeper in the
				 *	object tree.]
				 */

				vm_object_lock(object);
			}

			/*
			 *	Because we're pushing a page upward
			 *	in the object tree, we must restart
			 *	any faults that are waiting here.
			 *	[Note that this is an expansion of
			 *	PAGE_WAKEUP that uses the THREAD_RESTART
			 *	wait result].  Can't turn off the page's
			 *	busy bit because we're not done with it.
			 */
			 
			if (m->wanted) {
				m->wanted = FALSE;
				thread_wakeup_with_result((event_t) m,
					THREAD_RESTART);
			}
		}

		/*
		 *	The reference count on copy_object must be
		 *	at least 2: one for our extra reference,
		 *	and at least one from the outside world
		 *	(we checked that when we last locked
		 *	copy_object).
		 */
		copy_object->ref_count--;
		assert(copy_object->ref_count > 0);
		vm_object_unlock(copy_object);

		break;
	}

	*result_page = m;
	*top_page = first_m;

	/*
	 *	If the page can be written, assume that it will be.
	 *	[Earlier, we restrict the permission to allow write
	 *	access only if the fault so required, so we don't
	 *	mark read-only data as dirty.]
	 */

	if (vm_fault_dirty_handling && (*protection & VM_PROT_WRITE))
		m->dirty = TRUE;

	return(VM_FAULT_SUCCESS);

    block_and_backoff:
	vm_fault_cleanup(object, first_m);

#ifdef CONTINUATIONS
	if (continuation != (void (*)()) 0) {
		register vm_fault_state_t *state =
			(vm_fault_state_t *) current_thread()->ith_other;

		/*
		 *	Save variables in case we must restart.
		 */

		state->vmfp_backoff = TRUE;
		state->vmf_prot = *protection;

		counter(c_vm_fault_page_block_backoff_user++);
		thread_block(continuation);
	} else
#endif /* CONTINUATIONS */
	{
		counter(c_vm_fault_page_block_backoff_kernel++);
		thread_block((void (*)()) 0);
	}
    after_block_and_backoff:
	if (current_thread()->wait_result == THREAD_AWAKENED)
		return VM_FAULT_RETRY;
	else
		return VM_FAULT_INTERRUPTED;

#undef	RELEASE_PAGE
}

/*
 *	Routine:	vm_fault
 *	Purpose:
 *		Handle page faults, including pseudo-faults
 *		used to change the wiring status of pages.
 *	Returns:
 *		If an explicit (expression) continuation is supplied,
 *		then we call the continuation instead of returning.
 *	Implementation:
 *		Explicit continuations make this a little icky,
 *		because it hasn't been rewritten to embrace CPS.
 *		Instead, we have resume arguments for vm_fault and
 *		vm_fault_page, to let continue the fault computation.
 *
 *		vm_fault and vm_fault_page save mucho state
 *		in the moral equivalent of a closure.  The state
 *		structure is allocated when first entering vm_fault
 *		and deallocated when leaving vm_fault.
 */

#ifdef CONTINUATIONS
void
vm_fault_continue()
{
	register vm_fault_state_t *state =
		(vm_fault_state_t *) current_thread()->ith_other;

	(void) vm_fault(state->vmf_map,
			state->vmf_vaddr,
			state->vmf_fault_type,
			state->vmf_change_wiring,
			TRUE, state->vmf_continuation);
	/*NOTREACHED*/
}
#endif /* CONTINUATIONS */

kern_return_t vm_fault(map, vaddr, fault_type, change_wiring,
		       resume, continuation)
	vm_map_t	map;
	vm_offset_t	vaddr;
	vm_prot_t	fault_type;
	boolean_t	change_wiring;
	boolean_t	resume;
	void		(*continuation)();
{
	vm_map_version_t	version;	/* Map version for verificiation */
	boolean_t		wired;		/* Should mapping be wired down? */
	vm_object_t		object;		/* Top-level object */
	vm_offset_t		offset;		/* Top-level offset */
	vm_prot_t		prot;		/* Protection for mapping */
	vm_object_t		old_copy_object; /* Saved copy object */
	vm_page_t		result_page;	/* Result of vm_fault_page */
	vm_page_t		top_page;	/* Placeholder page */
	kern_return_t		kr;

	register
	vm_page_t		m;	/* Fast access to result_page */

#ifdef CONTINUATIONS
	if (resume) {
		register vm_fault_state_t *state =
			(vm_fault_state_t *) current_thread()->ith_other;

		/*
		 *	Retrieve cached variables and
		 *	continue vm_fault_page.
		 */

		object = state->vmf_object;
		if (object == VM_OBJECT_NULL)
			goto RetryFault;
		version = state->vmf_version;
		wired = state->vmf_wired;
		offset = state->vmf_offset;
		prot = state->vmf_prot;

		kr = vm_fault_page(object, offset, fault_type,
				(change_wiring && !wired), !change_wiring,
				&prot, &result_page, &top_page,
				TRUE, vm_fault_continue);
		goto after_vm_fault_page;
	}

	if (continuation != (void (*)()) 0) {
		/*
		 *	We will probably need to save state.
		 */

		char *	state;

		/*
		 * if this assignment stmt is written as
		 * 'active_threads[cpu_number()] = zalloc()',
		 * cpu_number may be evaluated before zalloc;
		 * if zalloc blocks, cpu_number will be wrong
		 */

		state = (char *) zalloc(vm_fault_state_zone);
		current_thread()->ith_other = state;

	}
#else /* not CONTINUATIONS */
	assert(continuation == 0);
	assert(!resume);
#endif /* not CONTINUATIONS */

    RetryFault: ;

	/*
	 *	Find the backing store object and offset into
	 *	it to begin the search.
	 */

	if ((kr = vm_map_lookup(&map, vaddr, fault_type, &version,
				&object, &offset,
				&prot, &wired)) != KERN_SUCCESS) {
		goto done;
	}

	/*
	 *	If the page is wired, we must fault for the current protection
	 *	value, to avoid further faults.
	 */

	if (wired)
		fault_type = prot;

   	/*
	 *	Make a reference to this object to
	 *	prevent its disposal while we are messing with
	 *	it.  Once we have the reference, the map is free
	 *	to be diddled.  Since objects reference their
	 *	shadows (and copies), they will stay around as well.
	 */

	assert(object->ref_count > 0);
	object->ref_count++;
	vm_object_paging_begin(object);

#ifdef CONTINUATIONS
	if (continuation != (void (*)()) 0) {
		register vm_fault_state_t *state =
			(vm_fault_state_t *) current_thread()->ith_other;

		/*
		 *	Save variables, in case vm_fault_page discards
		 *	our kernel stack and we have to restart.
		 */

		state->vmf_map = map;
		state->vmf_vaddr = vaddr;
		state->vmf_fault_type = fault_type;
		state->vmf_change_wiring = change_wiring;
		state->vmf_continuation = continuation;

		state->vmf_version = version;
		state->vmf_wired = wired;
		state->vmf_object = object;
		state->vmf_offset = offset;
		state->vmf_prot = prot;

		kr = vm_fault_page(object, offset, fault_type,
				   (change_wiring && !wired), !change_wiring,
				   &prot, &result_page, &top_page,
				   FALSE, vm_fault_continue);
	} else
#endif /* CONTINUATIONS */
	{
		kr = vm_fault_page(object, offset, fault_type,
				   (change_wiring && !wired), !change_wiring,
				   &prot, &result_page, &top_page,
				   FALSE, (void (*)()) 0);
	}
    after_vm_fault_page:

	/*
	 *	If we didn't succeed, lose the object reference immediately.
	 */

	if (kr != VM_FAULT_SUCCESS)
		vm_object_deallocate(object);

	/*
	 *	See why we failed, and take corrective action.
	 */

	switch (kr) {
		case VM_FAULT_SUCCESS:
			break;
		case VM_FAULT_RETRY:
			goto RetryFault;
		case VM_FAULT_INTERRUPTED:
			kr = KERN_SUCCESS;
			goto done;
		case VM_FAULT_MEMORY_SHORTAGE:
#ifdef CONTINUATIONS
			if (continuation != (void (*)()) 0) {
				register vm_fault_state_t *state =
					(vm_fault_state_t *) current_thread()->ith_other;

				/*
				 *	Save variables in case VM_PAGE_WAIT
				 *	discards our kernel stack.
				 */

				state->vmf_map = map;
				state->vmf_vaddr = vaddr;
				state->vmf_fault_type = fault_type;
				state->vmf_change_wiring = change_wiring;
				state->vmf_continuation = continuation;
				state->vmf_object = VM_OBJECT_NULL;

				VM_PAGE_WAIT(vm_fault_continue);
			} else
#endif /* CONTINUATIONS */
				VM_PAGE_WAIT((void (*)()) 0);
			goto RetryFault;
		case VM_FAULT_FICTITIOUS_SHORTAGE:
			vm_page_more_fictitious();
			goto RetryFault;
		case VM_FAULT_MEMORY_ERROR:
			kr = KERN_MEMORY_ERROR;
			goto done;
	}

	m = result_page;

	assert((change_wiring && !wired) ?
	       (top_page == VM_PAGE_NULL) :
	       ((top_page == VM_PAGE_NULL) == (m->object == object)));

	/*
	 *	How to clean up the result of vm_fault_page.  This
	 *	happens whether the mapping is entered or not.
	 */

#define UNLOCK_AND_DEALLOCATE				\
	MACRO_BEGIN					\
	vm_fault_cleanup(m->object, top_page);		\
	vm_object_deallocate(object);			\
	MACRO_END

	/*
	 *	What to do with the resulting page from vm_fault_page
	 *	if it doesn't get entered into the physical map:
	 */

#define RELEASE_PAGE(m)					\
	MACRO_BEGIN					\
	PAGE_WAKEUP_DONE(m);				\
	vm_page_lock_queues();				\
	if (!m->active && !m->inactive)			\
		vm_page_activate(m);			\
	vm_page_unlock_queues();			\
	MACRO_END

	/*
	 *	We must verify that the maps have not changed
	 *	since our last lookup.
	 */

	old_copy_object = m->object->copy;

	vm_object_unlock(m->object);
	while (!vm_map_verify(map, &version)) {
		vm_object_t	retry_object;
		vm_offset_t	retry_offset;
		vm_prot_t	retry_prot;

		/*
		 *	To avoid trying to write_lock the map while another
		 *	thread has it read_locked (in vm_map_pageable), we
		 *	do not try for write permission.  If the page is
		 *	still writable, we will get write permission.  If it
		 *	is not, or has been marked needs_copy, we enter the
		 *	mapping without write permission, and will merely
		 *	take another fault.
		 */
		kr = vm_map_lookup(&map, vaddr,
				   fault_type & ~VM_PROT_WRITE, &version,
				   &retry_object, &retry_offset, &retry_prot,
				   &wired);

		if (kr != KERN_SUCCESS) {
			vm_object_lock(m->object);
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto done;
		}

		vm_object_unlock(retry_object);
		vm_object_lock(m->object);

		if ((retry_object != object) ||
		    (retry_offset != offset)) {
			RELEASE_PAGE(m);
			UNLOCK_AND_DEALLOCATE;
			goto RetryFault;
		}

		/*
		 *	Check whether the protection has changed or the object
		 *	has been copied while we left the map unlocked.
		 */
		prot &= retry_prot;
		vm_object_unlock(m->object);
	}
	vm_object_lock(m->object);

	/*
	 *	If the copy object changed while the top-level object
	 *	was unlocked, then we must take away write permission.
	 */

	if (m->object->copy != old_copy_object)
		prot &= ~VM_PROT_WRITE;

	/*
	 *	If we want to wire down this page, but no longer have
	 *	adequate permissions, we must start all over.
	 */

	if (wired && (prot != fault_type)) {
		vm_map_verify_done(map, &version);
		RELEASE_PAGE(m);
		UNLOCK_AND_DEALLOCATE;
		goto RetryFault;
	}

	/*
	 *	It's critically important that a wired-down page be faulted
	 *	only once in each map for which it is wired.
	 */

	vm_object_unlock(m->object);

	/*
	 *	Put this page into the physical map.
	 *	We had to do the unlock above because pmap_enter
	 *	may cause other faults.  The page may be on
	 *	the pageout queues.  If the pageout daemon comes
	 *	across the page, it will remove it from the queues.
	 */

	PMAP_ENTER(map->pmap, vaddr, m, prot, wired);

	/*
	 *	If the page is not wired down and isn't already
	 *	on a pageout queue, then put it where the
	 *	pageout daemon can find it.
	 */
	vm_object_lock(m->object);
	vm_page_lock_queues();
	if (change_wiring) {
		if (wired)
			vm_page_wire(m);
		else
			vm_page_unwire(m);
	} else if (software_reference_bits) {
		if (!m->active && !m->inactive)
			vm_page_activate(m);
		m->reference = TRUE;
	} else {
		vm_page_activate(m);
	}
	vm_page_unlock_queues();

	/*
	 *	Unlock everything, and return
	 */

	vm_map_verify_done(map, &version);
	PAGE_WAKEUP_DONE(m);
	kr = KERN_SUCCESS;
	UNLOCK_AND_DEALLOCATE;

#undef	UNLOCK_AND_DEALLOCATE
#undef	RELEASE_PAGE

    done:
#ifdef CONTINUATIONS
	if (continuation != (void (*)()) 0) {
		register vm_fault_state_t *state =
			(vm_fault_state_t *) current_thread()->ith_other;

		zfree(vm_fault_state_zone, (vm_offset_t) state);
		(*continuation)(kr);
		/*NOTREACHED*/
	}
#endif /* CONTINUATIONS */

	return(kr);
}

kern_return_t	vm_fault_wire_fast();

/*
 *	vm_fault_wire:
 *
 *	Wire down a range of virtual addresses in a map.
 */
void vm_fault_wire(map, entry)
	vm_map_t	map;
	vm_map_entry_t	entry;
{

	register vm_offset_t	va;
	register pmap_t		pmap;
	register vm_offset_t	end_addr = entry->vme_end;

	pmap = vm_map_pmap(map);

	/*
	 *	Inform the physical mapping system that the
	 *	range of addresses may not fault, so that
	 *	page tables and such can be locked down as well.
	 */

	pmap_pageable(pmap, entry->vme_start, end_addr, FALSE);

	/*
	 *	We simulate a fault to get the page and enter it
	 *	in the physical map.
	 */

	for (va = entry->vme_start; va < end_addr; va += PAGE_SIZE) {
		if (vm_fault_wire_fast(map, va, entry) != KERN_SUCCESS)
			(void) vm_fault(map, va, VM_PROT_NONE, TRUE,
					FALSE, (void (*)()) 0);
	}
}

/*
 *	vm_fault_unwire:
 *
 *	Unwire a range of virtual addresses in a map.
 */
void vm_fault_unwire(map, entry)
	vm_map_t	map;
	vm_map_entry_t	entry;
{
	register vm_offset_t	va;
	register pmap_t		pmap;
	register vm_offset_t	end_addr = entry->vme_end;
	vm_object_t		object;

	pmap = vm_map_pmap(map);

	object = (entry->is_sub_map)
			? VM_OBJECT_NULL : entry->object.vm_object;

	/*
	 *	Since the pages are wired down, we must be able to
	 *	get their mappings from the physical map system.
	 */

	for (va = entry->vme_start; va < end_addr; va += PAGE_SIZE) {
		pmap_change_wiring(pmap, va, FALSE);

		if (object == VM_OBJECT_NULL) {
			vm_map_lock_set_recursive(map);
			(void) vm_fault(map, va, VM_PROT_NONE, TRUE,
					FALSE, (void (*)()) 0);
			vm_map_lock_clear_recursive(map);
		} else {
		 	vm_prot_t	prot;
			vm_page_t	result_page;
			vm_page_t	top_page;
			vm_fault_return_t result;

			do {
				prot = VM_PROT_NONE;

				vm_object_lock(object);
				vm_object_paging_begin(object);
			 	result = vm_fault_page(object,
						entry->offset +
						  (va - entry->vme_start),
						VM_PROT_NONE, TRUE,
						FALSE, &prot,
						&result_page,
						&top_page,
						FALSE, (void (*)()) 0);
			} while (result == VM_FAULT_RETRY);

			if (result != VM_FAULT_SUCCESS)
				panic("vm_fault_unwire: failure");

			vm_page_lock_queues();
			vm_page_unwire(result_page);
			vm_page_unlock_queues();
			PAGE_WAKEUP_DONE(result_page);

			vm_fault_cleanup(result_page->object, top_page);
		}
	}

	/*
	 *	Inform the physical mapping system that the range
	 *	of addresses may fault, so that page tables and
	 *	such may be unwired themselves.
	 */

	pmap_pageable(pmap, entry->vme_start, end_addr, TRUE);
}

/*
 *	vm_fault_wire_fast:
 *
 *	Handle common case of a wire down page fault at the given address.
 *	If successful, the page is inserted into the associated physical map.
 *	The map entry is passed in to avoid the overhead of a map lookup.
 *
 *	NOTE: the given address should be truncated to the
 *	proper page address.
 *
 *	KERN_SUCCESS is returned if the page fault is handled; otherwise,
 *	a standard error specifying why the fault is fatal is returned.
 *
 *	The map in question must be referenced, and remains so.
 *	Caller has a read lock on the map.
 *
 *	This is a stripped version of vm_fault() for wiring pages.  Anything
 *	other than the common case will return KERN_FAILURE, and the caller
 *	is expected to call vm_fault().
 */
kern_return_t vm_fault_wire_fast(map, va, entry)
	vm_map_t	map;
	vm_offset_t	va;
	vm_map_entry_t	entry;
{
	vm_object_t		object;
	vm_offset_t		offset;
	register vm_page_t	m;
	vm_prot_t		prot;

	vm_stat.faults++;		/* needs lock XXX */
/*
 *	Recovery actions
 */

#undef	RELEASE_PAGE
#define RELEASE_PAGE(m)	{				\
	PAGE_WAKEUP_DONE(m);				\
	vm_page_lock_queues();				\
	vm_page_unwire(m);				\
	vm_page_unlock_queues();			\
}


#undef	UNLOCK_THINGS
#define UNLOCK_THINGS	{				\
	object->paging_in_progress--;			\
	vm_object_unlock(object);			\
}

#undef	UNLOCK_AND_DEALLOCATE
#define UNLOCK_AND_DEALLOCATE	{			\
	UNLOCK_THINGS;					\
	vm_object_deallocate(object);			\
}
/*
 *	Give up and have caller do things the hard way.
 */

#define GIVE_UP {					\
	UNLOCK_AND_DEALLOCATE;				\
	return(KERN_FAILURE);				\
}


	/*
	 *	If this entry is not directly to a vm_object, bail out.
	 */
	if (entry->is_sub_map)
		return(KERN_FAILURE);

	/*
	 *	Find the backing store object and offset into it.
	 */

	object = entry->object.vm_object;
	offset = (va - entry->vme_start) + entry->offset;
	prot = entry->protection;

   	/*
	 *	Make a reference to this object to prevent its
	 *	disposal while we are messing with it.
	 */

	vm_object_lock(object);
	assert(object->ref_count > 0);
	object->ref_count++;
	object->paging_in_progress++;

	/*
	 *	INVARIANTS (through entire routine):
	 *
	 *	1)	At all times, we must either have the object
	 *		lock or a busy page in some object to prevent
	 *		some other thread from trying to bring in
	 *		the same page.
	 *
	 *	2)	Once we have a busy page, we must remove it from
	 *		the pageout queues, so that the pageout daemon
	 *		will not grab it away.
	 *
	 */

	/*
	 *	Look for page in top-level object.  If it's not there or
	 *	there's something going on, give up.
	 */
	m = vm_page_lookup(object, offset);
	if ((m == VM_PAGE_NULL) || (m->error) ||
	    (m->busy) || (m->absent) || (prot & m->page_lock)) {
		GIVE_UP;
	}

	/*
	 *	Wire the page down now.  All bail outs beyond this
	 *	point must unwire the page.  
	 */

	vm_page_lock_queues();
	vm_page_wire(m);
	vm_page_unlock_queues();

	/*
	 *	Mark page busy for other threads.
	 */
	assert(!m->busy);
	m->busy = TRUE;
	assert(!m->absent);

	/*
	 *	Give up if the page is being written and there's a copy object
	 */
	if ((object->copy != VM_OBJECT_NULL) && (prot & VM_PROT_WRITE)) {
		RELEASE_PAGE(m);
		GIVE_UP;
	}

	/*
	 *	Put this page into the physical map.
	 *	We have to unlock the object because pmap_enter
	 *	may cause other faults.   
	 */
	vm_object_unlock(object);

	PMAP_ENTER(map->pmap, va, m, prot, TRUE);

	/*
	 *	Must relock object so that paging_in_progress can be cleared.
	 */
	vm_object_lock(object);

	/*
	 *	Unlock everything, and return
	 */

	PAGE_WAKEUP_DONE(m);
	UNLOCK_AND_DEALLOCATE;

	return(KERN_SUCCESS);

}

/*
 *	Routine:	vm_fault_copy_cleanup
 *	Purpose:
 *		Release a page used by vm_fault_copy.
 */

void	vm_fault_copy_cleanup(page, top_page)
	vm_page_t	page;
	vm_page_t	top_page;
{
	vm_object_t	object = page->object;

	vm_object_lock(object);
	PAGE_WAKEUP_DONE(page);
	vm_page_lock_queues();
	if (!page->active && !page->inactive)
		vm_page_activate(page);
	vm_page_unlock_queues();
	vm_fault_cleanup(object, top_page);
}

/*
 *	Routine:	vm_fault_copy
 *
 *	Purpose:
 *		Copy pages from one virtual memory object to another --
 *		neither the source nor destination pages need be resident.
 *
 *		Before actually copying a page, the version associated with
 *		the destination address map wil be verified.
 *
 *	In/out conditions:
 *		The caller must hold a reference, but not a lock, to
 *		each of the source and destination objects and to the
 *		destination map.
 *
 *	Results:
 *		Returns KERN_SUCCESS if no errors were encountered in
 *		reading or writing the data.  Returns KERN_INTERRUPTED if
 *		the operation was interrupted (only possible if the
 *		"interruptible" argument is asserted).  Other return values
 *		indicate a permanent error in copying the data.
 *
 *		The actual amount of data copied will be returned in the
 *		"copy_size" argument.  In the event that the destination map
 *		verification failed, this amount may be less than the amount
 *		requested.
 */
kern_return_t	vm_fault_copy(
			src_object,
			src_offset,
			src_size,
			dst_object,
			dst_offset,
			dst_map,
			dst_version,
			interruptible
			)
	vm_object_t	src_object;
	vm_offset_t	src_offset;
	vm_size_t	*src_size;		/* INOUT */
	vm_object_t	dst_object;
	vm_offset_t	dst_offset;
	vm_map_t	dst_map;
	vm_map_version_t *dst_version;
	boolean_t	interruptible;
{
	vm_page_t		result_page;
	vm_prot_t		prot;
	
	vm_page_t		src_page;
	vm_page_t		src_top_page;

	vm_page_t		dst_page;
	vm_page_t		dst_top_page;

	vm_size_t		amount_done;
	vm_object_t		old_copy_object;

#define	RETURN(x)					\
	MACRO_BEGIN					\
	*src_size = amount_done;			\
	MACRO_RETURN(x);				\
	MACRO_END

	amount_done = 0;
	do { /* while (amount_done != *src_size) */

	    RetrySourceFault: ;

		if (src_object == VM_OBJECT_NULL) {
			/*
			 *	No source object.  We will just
			 *	zero-fill the page in dst_object.
			 */

			src_page = VM_PAGE_NULL;
		} else {
			prot = VM_PROT_READ;

			vm_object_lock(src_object);
			vm_object_paging_begin(src_object);

			switch (vm_fault_page(src_object, src_offset,
					VM_PROT_READ, FALSE, interruptible,
					&prot, &result_page, &src_top_page,
					FALSE, (void (*)()) 0)) {

				case VM_FAULT_SUCCESS:
					break;
				case VM_FAULT_RETRY:
					goto RetrySourceFault;
				case VM_FAULT_INTERRUPTED:
					RETURN(MACH_SEND_INTERRUPTED);
				case VM_FAULT_MEMORY_SHORTAGE:
					VM_PAGE_WAIT((void (*)()) 0);
					goto RetrySourceFault;
				case VM_FAULT_FICTITIOUS_SHORTAGE:
					vm_page_more_fictitious();
					goto RetrySourceFault;
				case VM_FAULT_MEMORY_ERROR:
					return(KERN_MEMORY_ERROR);
			}

			src_page = result_page;

			assert((src_top_page == VM_PAGE_NULL) ==
					(src_page->object == src_object));

			assert ((prot & VM_PROT_READ) != VM_PROT_NONE);

			vm_object_unlock(src_page->object);
		}

	    RetryDestinationFault: ;

		prot = VM_PROT_WRITE;

		vm_object_lock(dst_object);
		vm_object_paging_begin(dst_object);

		switch (vm_fault_page(dst_object, dst_offset, VM_PROT_WRITE,
				FALSE, FALSE /* interruptible */,
				&prot, &result_page, &dst_top_page,
				FALSE, (void (*)()) 0)) {

			case VM_FAULT_SUCCESS:
				break;
			case VM_FAULT_RETRY:
				goto RetryDestinationFault;
			case VM_FAULT_INTERRUPTED:
				if (src_page != VM_PAGE_NULL)
					vm_fault_copy_cleanup(src_page,
							      src_top_page);
				RETURN(MACH_SEND_INTERRUPTED);
			case VM_FAULT_MEMORY_SHORTAGE:
				VM_PAGE_WAIT((void (*)()) 0);
				goto RetryDestinationFault;
			case VM_FAULT_FICTITIOUS_SHORTAGE:
				vm_page_more_fictitious();
				goto RetryDestinationFault;
			case VM_FAULT_MEMORY_ERROR:
				if (src_page != VM_PAGE_NULL)
					vm_fault_copy_cleanup(src_page,
							      src_top_page);
				return(KERN_MEMORY_ERROR);
		}
		assert ((prot & VM_PROT_WRITE) != VM_PROT_NONE);

		dst_page = result_page;

		old_copy_object = dst_page->object->copy;

		vm_object_unlock(dst_page->object);

		if (!vm_map_verify(dst_map, dst_version)) {

		 BailOut: ;

			if (src_page != VM_PAGE_NULL)
				vm_fault_copy_cleanup(src_page, src_top_page);
			vm_fault_copy_cleanup(dst_page, dst_top_page);
			break;
		}


		vm_object_lock(dst_page->object);
		if (dst_page->object->copy != old_copy_object) {
			vm_object_unlock(dst_page->object);
			vm_map_verify_done(dst_map, dst_version);
			goto BailOut;
		}
		vm_object_unlock(dst_page->object);

		/*
		 *	Copy the page, and note that it is dirty
		 *	immediately.
		 */

		if (src_page == VM_PAGE_NULL)
			vm_page_zero_fill(dst_page);
		else
			vm_page_copy(src_page, dst_page);
		dst_page->dirty = TRUE;

		/*
		 *	Unlock everything, and return
		 */

		vm_map_verify_done(dst_map, dst_version);

		if (src_page != VM_PAGE_NULL)
			vm_fault_copy_cleanup(src_page, src_top_page);
		vm_fault_copy_cleanup(dst_page, dst_top_page);

		amount_done += PAGE_SIZE;
		src_offset += PAGE_SIZE;
		dst_offset += PAGE_SIZE;

	} while (amount_done != *src_size);

	RETURN(KERN_SUCCESS);
#undef	RETURN

	/*NOTREACHED*/	
}





#ifdef	notdef

/*
 *	Routine:	vm_fault_page_overwrite
 *
 *	Description:
 *		A form of vm_fault_page that assumes that the
 *		resulting page will be overwritten in its entirety,
 *		making it unnecessary to obtain the correct *contents*
 *		of the page.
 *
 *	Implementation:
 *		XXX Untested.  Also unused.  Eventually, this technology
 *		could be used in vm_fault_copy() to advantage.
 */
vm_fault_return_t vm_fault_page_overwrite(dst_object, dst_offset, result_page)
	register
	vm_object_t	dst_object;
	vm_offset_t	dst_offset;
	vm_page_t	*result_page;	/* OUT */
{
	register
	vm_page_t	dst_page;

#define	interruptible	FALSE	/* XXX */

	while (TRUE) {
		/*
		 *	Look for a page at this offset
		 */

		while ((dst_page = vm_page_lookup(dst_object, dst_offset))
				 == VM_PAGE_NULL) {
			/*
			 *	No page, no problem... just allocate one.
			 */

			dst_page = vm_page_alloc(dst_object, dst_offset);
			if (dst_page == VM_PAGE_NULL) {
				vm_object_unlock(dst_object);
				VM_PAGE_WAIT((void (*)()) 0);
				vm_object_lock(dst_object);
				continue;
			}

			/*
			 *	Pretend that the memory manager
			 *	write-protected the page.
			 *
			 *	Note that we will be asking for write
			 *	permission without asking for the data
			 *	first.
			 */

			dst_page->overwriting = TRUE;
			dst_page->page_lock = VM_PROT_WRITE;
			dst_page->absent = TRUE;
			dst_object->absent_count++;

			break;

			/*
			 *	When we bail out, we might have to throw
			 *	away the page created here.
			 */

#define	DISCARD_PAGE						\
	MACRO_BEGIN						\
	vm_object_lock(dst_object);				\
	dst_page = vm_page_lookup(dst_object, dst_offset);	\
	if ((dst_page != VM_PAGE_NULL) && dst_page->overwriting) \
	   	VM_PAGE_FREE(dst_page);				\
	vm_object_unlock(dst_object);				\
	MACRO_END
		}

		/*
		 *	If the page is write-protected...
		 */

		if (dst_page->page_lock & VM_PROT_WRITE) {
			/*
			 *	... and an unlock request hasn't been sent
			 */

			if ( ! (dst_page->unlock_request & VM_PROT_WRITE)) {
				vm_prot_t	u;
				kern_return_t	rc;

				/*
				 *	... then send one now.
				 */

				if (!dst_object->pager_ready) {
					vm_object_assert_wait(dst_object,
						VM_OBJECT_EVENT_PAGER_READY,
						interruptible);
					vm_object_unlock(dst_object);
					thread_block((void (*)()) 0);
					if (current_thread()->wait_result !=
					    THREAD_AWAKENED) {
						DISCARD_PAGE;
						return(VM_FAULT_INTERRUPTED);
					}
					continue;
				}

				u = dst_page->unlock_request |= VM_PROT_WRITE;
				vm_object_unlock(dst_object);

				if ((rc = memory_object_data_unlock(
						dst_object->pager,
						dst_object->pager_request,
						dst_offset + dst_object->paging_offset,
						PAGE_SIZE,
						u)) != KERN_SUCCESS) {
				     	printf("vm_object_overwrite: memory_object_data_unlock failed\n");
					DISCARD_PAGE;
					return((rc == MACH_SEND_INTERRUPTED) ?
						VM_FAULT_INTERRUPTED :
						VM_FAULT_MEMORY_ERROR);
				}
				vm_object_lock(dst_object);
				continue;
			}

			/* ... fall through to wait below */
		} else {
			/*
			 *	If the page isn't being used for other
			 *	purposes, then we're done.
			 */
			if ( ! (dst_page->busy || dst_page->absent || dst_page->error) )
				break;
		}

		PAGE_ASSERT_WAIT(dst_page, interruptible);
		vm_object_unlock(dst_object);
		thread_block((void (*)()) 0);
		if (current_thread()->wait_result != THREAD_AWAKENED) {
			DISCARD_PAGE;
			return(VM_FAULT_INTERRUPTED);
		}
	}

	*result_page = dst_page;
	return(VM_FAULT_SUCCESS);

#undef	interruptible
#undef	DISCARD_PAGE
}

#endif	notdef
