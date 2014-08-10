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
 *	File:	vm/vm_object.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Virtual memory object module.
 */

#include <norma_vm.h>
#include <mach_pagemap.h>

#if	NORMA_VM
#include <norma/xmm_server_rename.h>
#endif	/* NORMA_VM */

#include <mach/memory_object.h>
#include "memory_object_default.h"
#include "memory_object_user.h"
#include "vm_param.h"
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/assert.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/xpr.h>
#include <kern/zalloc.h>
#include <vm/memory_object.h>
#include <vm/vm_fault.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>


void memory_object_release(
	ipc_port_t	pager,
	pager_request_t	pager_request,
	ipc_port_t	pager_name); /* forward */

void vm_object_deactivate_pages(vm_object_t);

/*
 *	Virtual memory objects maintain the actual data
 *	associated with allocated virtual memory.  A given
 *	page of memory exists within exactly one object.
 *
 *	An object is only deallocated when all "references"
 *	are given up.  Only one "reference" to a given
 *	region of an object should be writeable.
 *
 *	Associated with each object is a list of all resident
 *	memory pages belonging to that object; this list is
 *	maintained by the "vm_page" module, but locked by the object's
 *	lock.
 *
 *	Each object also records the memory object port
 *	that is used by the kernel to request and write
 *	back data (the memory object port, field "pager"),
 *	and the ports provided to the memory manager, the server that
 *	manages that data, to return data and control its
 *	use (the memory object control port, field "pager_request")
 *	and for naming (the memory object name port, field "pager_name").
 *
 *	Virtual memory objects are allocated to provide
 *	zero-filled memory (vm_allocate) or map a user-defined
 *	memory object into a virtual address space (vm_map).
 *
 *	Virtual memory objects that refer to a user-defined
 *	memory object are called "permanent", because all changes
 *	made in virtual memory are reflected back to the
 *	memory manager, which may then store it permanently.
 *	Other virtual memory objects are called "temporary",
 *	meaning that changes need be written back only when
 *	necessary to reclaim pages, and that storage associated
 *	with the object can be discarded once it is no longer
 *	mapped.
 *
 *	A permanent memory object may be mapped into more
 *	than one virtual address space.  Moreover, two threads
 *	may attempt to make the first mapping of a memory
 *	object concurrently.  Only one thread is allowed to
 *	complete this mapping; all others wait for the
 *	"pager_initialized" field is asserted, indicating
 *	that the first thread has initialized all of the
 *	necessary fields in the virtual memory object structure.
 *
 *	The kernel relies on a *default memory manager* to
 *	provide backing storage for the zero-filled virtual
 *	memory objects.  The memory object ports associated
 *	with these temporary virtual memory objects are only
 *	generated and passed to the default memory manager
 *	when it becomes necessary.  Virtual memory objects
 *	that depend on the default memory manager are called
 *	"internal".  The "pager_created" field is provided to
 *	indicate whether these ports have ever been allocated.
 *	
 *	The kernel may also create virtual memory objects to
 *	hold changed pages after a copy-on-write operation.
 *	In this case, the virtual memory object (and its
 *	backing storage -- its memory object) only contain
 *	those pages that have been changed.  The "shadow"
 *	field refers to the virtual memory object that contains
 *	the remainder of the contents.  The "shadow_offset"
 *	field indicates where in the "shadow" these contents begin.
 *	The "copy" field refers to a virtual memory object
 *	to which changed pages must be copied before changing
 *	this object, in order to implement another form
 *	of copy-on-write optimization.
 *
 *	The virtual memory object structure also records
 *	the attributes associated with its memory object.
 *	The "pager_ready", "can_persist" and "copy_strategy"
 *	fields represent those attributes.  The "cached_list"
 *	field is used in the implementation of the persistence
 *	attribute.
 *
 * ZZZ Continue this comment.
 */

zone_t		vm_object_zone;		/* vm backing store zone */

/*
 *	All wired-down kernel memory belongs to a single virtual
 *	memory object (kernel_object) to avoid wasting data structures.
 */
vm_object_t	kernel_object;

/*
 *	Virtual memory objects that are not referenced by
 *	any address maps, but that are allowed to persist
 *	(an attribute specified by the associated memory manager),
 *	are kept in a queue (vm_object_cached_list).
 *
 *	When an object from this queue is referenced again,
 *	for example to make another address space mapping,
 *	it must be removed from the queue.  That is, the
 *	queue contains *only* objects with zero references.
 *
 *	The kernel may choose to terminate objects from this
 *	queue in order to reclaim storage.  The current policy
 *	is to permit a fixed maximum number of unreferenced
 *	objects (vm_object_cached_max).
 *
 *	A simple lock (accessed by routines
 *	vm_object_cache_{lock,lock_try,unlock}) governs the
 *	object cache.  It must be held when objects are
 *	added to or removed from the cache (in vm_object_terminate).
 *	The routines that acquire a reference to a virtual
 *	memory object based on one of the memory object ports
 *	must also lock the cache.
 *
 *	Ideally, the object cache should be more isolated
 *	from the reference mechanism, so that the lock need
 *	not be held to make simple references.
 */
queue_head_t	vm_object_cached_list;
int		vm_object_cached_count;
int		vm_object_cached_max = 100;	/* may be patched*/

decl_simple_lock_data(,vm_object_cached_lock_data)

#define vm_object_cache_lock()		\
		simple_lock(&vm_object_cached_lock_data)
#define vm_object_cache_lock_try()	\
		simple_lock_try(&vm_object_cached_lock_data)
#define vm_object_cache_unlock()	\
		simple_unlock(&vm_object_cached_lock_data)

/*
 *	Virtual memory objects are initialized from
 *	a template (see vm_object_allocate).
 *
 *	When adding a new field to the virtual memory
 *	object structure, be sure to add initialization
 *	(see vm_object_init).
 */
vm_object_t	vm_object_template;

/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

vm_object_t _vm_object_allocate(
	vm_size_t		size)
{
	register vm_object_t object;

	object = (vm_object_t) zalloc(vm_object_zone);

	*object = *vm_object_template;
	queue_init(&object->memq);
	vm_object_lock_init(object);
	object->size = size;

	return object;
}

vm_object_t vm_object_allocate(
	vm_size_t	size)
{
	register vm_object_t object;
	register ipc_port_t port;

	object = _vm_object_allocate(size);
#if	!NORMA_VM
	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("vm_object_allocate");
	object->pager_name = port;
	ipc_kobject_set(port, (ipc_kobject_t) object, IKOT_PAGING_NAME);
#endif	/* !NORMA_VM */

	return object;
}

/*
 *	vm_object_bootstrap:
 *
 *	Initialize the VM objects module.
 */
void vm_object_bootstrap(void)
{
	vm_object_zone = zinit((vm_size_t) sizeof(struct vm_object),
				round_page(512*1024),
				round_page(12*1024),
				0, "objects");

	queue_init(&vm_object_cached_list);
	simple_lock_init(&vm_object_cached_lock_data);

	/*
	 *	Fill in a template object, for quick initialization
	 */

	vm_object_template = (vm_object_t) zalloc(vm_object_zone);
	bzero((char *) vm_object_template, sizeof *vm_object_template);

	vm_object_template->ref_count = 1;
	vm_object_template->size = 0;
	vm_object_template->resident_page_count = 0;
	vm_object_template->copy = VM_OBJECT_NULL;
	vm_object_template->shadow = VM_OBJECT_NULL;
	vm_object_template->shadow_offset = (vm_offset_t) 0;

	vm_object_template->pager = IP_NULL;
	vm_object_template->paging_offset = 0;
	vm_object_template->pager_request = PAGER_REQUEST_NULL;
	vm_object_template->pager_name = IP_NULL;

	vm_object_template->pager_created = FALSE;
	vm_object_template->pager_initialized = FALSE;
	vm_object_template->pager_ready = FALSE;

	vm_object_template->copy_strategy = MEMORY_OBJECT_COPY_NONE;
		/* ignored if temporary, will be reset before
		 * permanent object becomes ready */
	vm_object_template->use_shared_copy = FALSE;
	vm_object_template->shadowed = FALSE;

	vm_object_template->absent_count = 0;
	vm_object_template->all_wanted = 0; /* all bits FALSE */

	vm_object_template->paging_in_progress = 0;
	vm_object_template->can_persist = FALSE;
	vm_object_template->internal = TRUE;
	vm_object_template->temporary = TRUE;
	vm_object_template->alive = TRUE;
	vm_object_template->lock_in_progress = FALSE;
	vm_object_template->lock_restart = FALSE;
	vm_object_template->use_old_pageout = TRUE; /* XXX change later */
	vm_object_template->last_alloc = (vm_offset_t) 0;

#if	MACH_PAGEMAP
	vm_object_template->existence_info = VM_EXTERNAL_NULL;
#endif	/* MACH_PAGEMAP */

		/*
	 *	Initialize the "kernel object"
	 */

	kernel_object = _vm_object_allocate(
		VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS);

	/*
	 *	Initialize the "submap object".  Make it as large as the
	 *	kernel object so that no limit is imposed on submap sizes.
	 */

	vm_submap_object = _vm_object_allocate(
		VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS);

#if	MACH_PAGEMAP
	vm_external_module_initialize();
#endif	/* MACH_PAGEMAP */
}

void vm_object_init(void)
{
#if	!NORMA_VM
	/*
	 *	Finish initializing the kernel object.
	 *	The submap object doesn't need a name port.
	 */

	kernel_object->pager_name = ipc_port_alloc_kernel();
	ipc_kobject_set(kernel_object->pager_name,
			(ipc_kobject_t) kernel_object,
			IKOT_PAGING_NAME);
#endif	/* !NORMA_VM */
}

/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
void vm_object_reference(
	register vm_object_t	object)
{
	if (object == VM_OBJECT_NULL)
		return;

	vm_object_lock(object);
	assert(object->ref_count > 0);
	object->ref_count++;
	vm_object_unlock(object);
}

/*
 *	vm_object_deallocate:
 *
 *	Release a reference to the specified object,
 *	gained either through a vm_object_allocate
 *	or a vm_object_reference call.  When all references
 *	are gone, storage associated with this object
 *	may be relinquished.
 *
 *	No object may be locked.
 */
void vm_object_deallocate(
	register vm_object_t	object)
{
	vm_object_t	temp;

	while (object != VM_OBJECT_NULL) {

		/*
		 *	The cache holds a reference (uncounted) to
		 *	the object; we must lock it before removing
		 *	the object.
		 */

		vm_object_cache_lock();

		/*
		 *	Lose the reference
		 */
		vm_object_lock(object);
		if (--(object->ref_count) > 0) {

			/*
			 *	If there are still references, then
			 *	we are done.
			 */
			vm_object_unlock(object);
			vm_object_cache_unlock();
			return;
		}

		/*
		 *	See whether this object can persist.  If so, enter
		 *	it in the cache, then deactivate all of its
		 *	pages.
		 */
		if (object->can_persist) {
			boolean_t	overflow;

			/*
			 *	Enter the object onto the queue
			 *	of "cached" objects.  Remember whether
			 *	we've caused the queue to overflow,
			 *	as a hint.
			 */

			queue_enter(&vm_object_cached_list, object,
				vm_object_t, cached_list);
			overflow = (++vm_object_cached_count > vm_object_cached_max);
			vm_object_cache_unlock();

			vm_object_deactivate_pages(object);
			vm_object_unlock(object);

			/*
			 *	If we didn't overflow, or if the queue has
			 *	been reduced back to below the specified
			 *	minimum, then quit.
			 */
			if (!overflow)
				return;

			while (TRUE) {
				vm_object_cache_lock();
				if (vm_object_cached_count <=
				    vm_object_cached_max) {
					vm_object_cache_unlock();
					return;
				}

				/*
				 *	If we must trim down the queue, take
				 *	the first object, and proceed to
				 *	terminate it instead of the original
				 *	object.	 Have to wait for pager init.
				 *  if it's in progress.
				 */
				object= (vm_object_t)
				    queue_first(&vm_object_cached_list);
				vm_object_lock(object);

				if (!(object->pager_created &&
				    !object->pager_initialized)) {

					/*
					 *  Ok to terminate, hang on to lock.
					 */
					break;
				}

				vm_object_assert_wait(object,
					VM_OBJECT_EVENT_INITIALIZED, FALSE);
				vm_object_unlock(object);
				vm_object_cache_unlock();
				thread_block((void (*)()) 0);

				/*
				 *  Continue loop to check if cache still
				 *  needs to be trimmed.
				 */
			}

			/*
			 *	Actually remove object from cache.
			 */

			queue_remove(&vm_object_cached_list, object,
					vm_object_t, cached_list);
			vm_object_cached_count--;

			assert(object->ref_count == 0);
		}
		else {
			if (object->pager_created &&
			    !object->pager_initialized) {

				/*
				 *	Have to wait for initialization.
				 *	Put reference back and retry
				 *	when it's initialized.
				 */
				object->ref_count++;
				vm_object_assert_wait(object,
					VM_OBJECT_EVENT_INITIALIZED, FALSE);
				vm_object_unlock(object);
				vm_object_cache_unlock();
				thread_block((void (*)()) 0);
				continue;
			  }
		}

		/*
		 *	Take the reference to the shadow object
		 *	out of the object to be destroyed.
		 */

		temp = object->shadow;

		/*
		 *	Destroy the object; the cache lock will
		 *	be released in the process.
		 */

		vm_object_terminate(object);

		/*
		 *	Deallocate the reference to the shadow
		 *	by continuing the loop with that object
		 *	in place of the original.
		 */

		object = temp;
	}
}

boolean_t	vm_object_terminate_remove_all = FALSE;

/*
 *	Routine:	vm_object_terminate
 *	Purpose:
 *		Free all resources associated with a vm_object.
 *	In/out conditions:
 *		Upon entry, the object and the cache must be locked,
 *		and the object must have no references.
 *
 *		The shadow object reference is left alone.
 *
 *		Upon exit, the cache will be unlocked, and the
 *		object will cease to exist.
 */
void vm_object_terminate(
	register vm_object_t	object)
{
	register vm_page_t	p;
	vm_object_t		shadow_object;

	/*
	 *	Make sure the object isn't already being terminated
	 */

	assert(object->alive);
	object->alive = FALSE;

	/*
	 *	Make sure no one can look us up now.
	 */

	vm_object_remove(object);
	vm_object_cache_unlock();

	/*
	 *	Detach the object from its shadow if we are the shadow's
	 *	copy.
	 */
	if ((shadow_object = object->shadow) != VM_OBJECT_NULL) {
		vm_object_lock(shadow_object);
		assert((shadow_object->copy == object) ||
		       (shadow_object->copy == VM_OBJECT_NULL));
		shadow_object->copy = VM_OBJECT_NULL;
		vm_object_unlock(shadow_object);
	}

	/*
	 *	The pageout daemon might be playing with our pages.
	 *	Now that the object is dead, it won't touch any more
	 *	pages, but some pages might already be on their way out.
	 *	Hence, we wait until the active paging activities have ceased.
	 */

	vm_object_paging_wait(object, FALSE);

	/*
	 *	Clean or free the pages, as appropriate.
	 *	It is possible for us to find busy/absent pages,
	 *	if some faults on this object were aborted.
	 */

	if ((object->temporary) || (object->pager == IP_NULL)) {
		while (!queue_empty(&object->memq)) {
			p = (vm_page_t) queue_first(&object->memq);

			VM_PAGE_CHECK(p);

			if (p->busy && !p->absent)
				panic("vm_object_terminate.2 0x%x 0x%x",
				      object, p);

			VM_PAGE_FREE(p);
		}
	} else while (!queue_empty(&object->memq)) {
		p = (vm_page_t) queue_first(&object->memq);

		VM_PAGE_CHECK(p);

		if (p->busy && !p->absent)
			panic("vm_object_terminate.3 0x%x 0x%x", object, p);

		vm_page_lock_queues();
		VM_PAGE_QUEUES_REMOVE(p);
		vm_page_unlock_queues();

		if (p->absent || p->private) {

			/*
			 *	For private pages, VM_PAGE_FREE just
			 *	leaves the page structure around for
			 *	its owner to clean up.  For absent
			 *	pages, the structure is returned to
			 *	the appropriate pool.
			 */

			goto free_page;
		}

		if (p->fictitious)
			panic("vm_object_terminate.4 0x%x 0x%x", object, p);

		if (!p->dirty)
			p->dirty = pmap_is_modified(p->phys_addr);

		if (p->dirty || p->precious) {
			p->busy = TRUE;
			vm_pageout_page(p, FALSE, TRUE); /* flush page */
		} else {
		    free_page:
		    	VM_PAGE_FREE(p);
		}
	}

	assert(object->ref_count == 0);
	assert(object->paging_in_progress == 0);

	/*
	 *	Throw away port rights... note that they may
	 *	already have been thrown away (by vm_object_destroy
	 *	or memory_object_destroy).
	 *
	 *	Instead of destroying the control and name ports,
	 *	we send all rights off to the memory manager instead,
	 *	using memory_object_terminate.
	 */

	vm_object_unlock(object);

	if (object->pager != IP_NULL) {
		/* consumes our rights for pager, pager_request, pager_name */
		memory_object_release(object->pager,
					     object->pager_request,
					     object->pager_name);
	} else if (object->pager_name != IP_NULL) {
		/* consumes our right for pager_name */
#if	NORMA_VM
		ipc_port_release_send(object->pager_name);
#else	/* NORMA_VM */
		ipc_port_dealloc_kernel(object->pager_name);
#endif	/* NORMA_VM */
	}

#if	MACH_PAGEMAP
	vm_external_destroy(object->existence_info);
#endif	/* MACH_PAGEMAP */

	/*
	 *	Free the space for the object.
	 */

	zfree(vm_object_zone, (vm_offset_t) object);
}

/*
 *	Routine:	vm_object_pager_wakeup
 *	Purpose:	Wake up anyone waiting for IKOT_PAGER_TERMINATING
 */

void
vm_object_pager_wakeup(
	ipc_port_t	pager)
{
	boolean_t someone_waiting;

	/*
	 *	If anyone was waiting for the memory_object_terminate
	 *	to be queued, wake them up now.
	 */
	vm_object_cache_lock();
	assert(ip_kotype(pager) == IKOT_PAGER_TERMINATING);
	someone_waiting = (pager->ip_kobject != IKO_NULL);
	if (ip_active(pager))
		ipc_kobject_set(pager, IKO_NULL, IKOT_NONE);
	vm_object_cache_unlock();
	if (someone_waiting) {
		thread_wakeup((event_t) pager);
	}
}

/*
 *	Routine:	memory_object_release
 *	Purpose:	Terminate the pager and release port rights,
 *			just like memory_object_terminate, except
 *			that we wake up anyone blocked in vm_object_enter
 *			waiting for termination message to be queued
 *			before calling memory_object_init.
 */
void memory_object_release(
	ipc_port_t	pager,
	pager_request_t	pager_request,
	ipc_port_t	pager_name)
{

	/*
	 *	Keep a reference to pager port;
	 *	the terminate might otherwise release all references.
	 */
	ip_reference(pager);

	/*
	 *	Terminate the pager.
	 */
	(void) memory_object_terminate(pager, pager_request, pager_name);

	/*
	 *	Wakeup anyone waiting for this terminate
	 */
	vm_object_pager_wakeup(pager);

	/*
	 *	Release reference to pager port.
	 */
	ip_release(pager);
}

/*
 *	Routine:	vm_object_abort_activity [internal use only]
 *	Purpose:
 *		Abort paging requests pending on this object.
 *	In/out conditions:
 *		The object is locked on entry and exit.
 */
void vm_object_abort_activity(
	vm_object_t	object)
{
	register
	vm_page_t	p;
	vm_page_t	next;

	/*
	 *	Abort all activity that would be waiting
	 *	for a result on this memory object.
	 *
	 *	We could also choose to destroy all pages
	 *	that we have in memory for this object, but
	 *	we don't.
	 */

	p = (vm_page_t) queue_first(&object->memq);
	while (!queue_end(&object->memq, (queue_entry_t) p)) {
		next = (vm_page_t) queue_next(&p->listq);

		/*
		 *	If it's being paged in, destroy it.
		 *	If an unlock has been requested, start it again.
		 */

		if (p->busy && p->absent) {
			VM_PAGE_FREE(p);
		}
		 else {
		 	if (p->unlock_request != VM_PROT_NONE)
			 	p->unlock_request = VM_PROT_NONE;
			PAGE_WAKEUP(p);
		}
		
		p = next;
	}

	/*
	 *	Wake up threads waiting for the memory object to
	 *	become ready.
	 */

	object->pager_ready = TRUE;
	vm_object_wakeup(object, VM_OBJECT_EVENT_PAGER_READY);
}

/*
 *	Routine:	memory_object_destroy [user interface]
 *	Purpose:
 *		Shut down a memory object, despite the
 *		presence of address map (or other) references
 *		to the vm_object.
 *	Note:
 *		This routine may be called either from the user interface,
 *		or from port destruction handling (via vm_object_destroy).
 */
kern_return_t memory_object_destroy(
	register
	vm_object_t	object,
	kern_return_t	reason)
{
	ipc_port_t	old_object,  old_name;
	pager_request_t	old_control;

#ifdef	lint
	reason++;
#endif	/* lint */

	if (object == VM_OBJECT_NULL)
		return KERN_SUCCESS;

	/*
	 *	Remove the port associations immediately.
	 *
	 *	This will prevent the memory manager from further
	 *	meddling.  [If it wanted to flush data or make
	 *	other changes, it should have done so before performing
	 *	the destroy call.]
	 */

	vm_object_cache_lock();
	vm_object_lock(object);
	vm_object_remove(object);
	object->can_persist = FALSE;
	vm_object_cache_unlock();

	/*
	 *	Rip out the ports from the vm_object now... this
	 *	will prevent new memory_object calls from succeeding.
	 */

	old_object = object->pager;
	object->pager = IP_NULL;
	
	old_control = object->pager_request;
	object->pager_request = PAGER_REQUEST_NULL;

	old_name = object->pager_name;
	object->pager_name = IP_NULL;


	/*
	 *	Wait for existing paging activity (that might
	 *	have the old ports) to subside.
	 */

	vm_object_paging_wait(object, FALSE);
	vm_object_unlock(object);

	/*
	 *	Shut down the ports now.
	 *
	 *	[Paging operations may be proceeding concurrently --
	 *	they'll get the null values established above.]
	 */

	if (old_object != IP_NULL) {
		/* consumes our rights for object, control, name */
		memory_object_release(old_object, old_control,
					     old_name);
	} else if (old_name != IP_NULL) {
		/* consumes our right for name */
#if	NORMA_VM
		ipc_port_release_send(object->pager_name);
#else	/* NORMA_VM */
		ipc_port_dealloc_kernel(object->pager_name);
#endif	/* NORMA_VM */
	}

	/*
	 *	Lose the reference that was donated for this routine
	 */

	vm_object_deallocate(object);

	return KERN_SUCCESS;
}

/*
 *	vm_object_deactivate_pages
 *
 *	Deactivate all pages in the specified object.  (Keep its pages
 *	in memory even though it is no longer referenced.)
 *
 *	The object must be locked.
 */
void vm_object_deactivate_pages(
	register vm_object_t	object)
{
	register vm_page_t	p;

	queue_iterate(&object->memq, p, vm_page_t, listq) {
		vm_page_lock_queues();
		if (!p->busy)
			vm_page_deactivate(p);
		vm_page_unlock_queues();
	}
}


/*
 *	Routine:	vm_object_pmap_protect
 *
 *	Purpose:
 *		Reduces the permission for all physical
 *		pages in the specified object range.
 *
 *		If removing write permission only, it is
 *		sufficient to protect only the pages in
 *		the top-level object; only those pages may
 *		have write permission.
 *
 *		If removing all access, we must follow the
 *		shadow chain from the top-level object to
 *		remove access to all pages in shadowed objects.
 *
 *		The object must *not* be locked.  The object must
 *		be temporary/internal.  
 *
 *              If pmap is not NULL, this routine assumes that
 *              the only mappings for the pages are in that
 *              pmap.
 */
boolean_t vm_object_pmap_protect_by_page = FALSE;

void vm_object_pmap_protect(
	register vm_object_t	object,
	register vm_offset_t	offset,
	vm_offset_t		size,
	pmap_t			pmap,
	vm_offset_t		pmap_start,
	vm_prot_t		prot)
{
	if (object == VM_OBJECT_NULL)
	    return;

	vm_object_lock(object);

	assert(object->temporary && object->internal);

	while (TRUE) {
	    if (object->resident_page_count > atop(size) / 2 &&
		    pmap != PMAP_NULL) {
		vm_object_unlock(object);
		pmap_protect(pmap, pmap_start, pmap_start + size, prot);
		return;
	    }

	    {
		register vm_page_t	p;
		register vm_offset_t	end;

		end = offset + size;

		queue_iterate(&object->memq, p, vm_page_t, listq) {
		    if (!p->fictitious &&
			(offset <= p->offset) &&
			(p->offset < end)) {
			if ((pmap == PMAP_NULL) ||
			    vm_object_pmap_protect_by_page) {
			    pmap_page_protect(p->phys_addr,
					      prot & ~p->page_lock);
			} else {
			    vm_offset_t	start =
					pmap_start +
					(p->offset - offset);

			    pmap_protect(pmap,
					 start,
					 start + PAGE_SIZE,
					 prot);
			}
		    }
		}
	    }

	    if (prot == VM_PROT_NONE) {
		/*
		 * Must follow shadow chain to remove access
		 * to pages in shadowed objects.
		 */
		register vm_object_t	next_object;

		next_object = object->shadow;
		if (next_object != VM_OBJECT_NULL) {
		    offset += object->shadow_offset;
		    vm_object_lock(next_object);
		    vm_object_unlock(object);
		    object = next_object;
		}
		else {
		    /*
		     * End of chain - we are done.
		     */
		    break;
		}
	    }
	    else {
		/*
		 * Pages in shadowed objects may never have
		 * write permission - we may stop here.
		 */
		break;
	    }
	}

	vm_object_unlock(object);
}

/*
 *	vm_object_pmap_remove:
 *
 *	Removes all physical pages in the specified
 *	object range from all physical maps.
 *
 *	The object must *not* be locked.
 */
void vm_object_pmap_remove(
	register vm_object_t	object,
	register vm_offset_t	start,
	register vm_offset_t	end)
{
	register vm_page_t	p;

	if (object == VM_OBJECT_NULL)
		return;

	vm_object_lock(object);
	queue_iterate(&object->memq, p, vm_page_t, listq) {
		if (!p->fictitious &&
		    (start <= p->offset) &&
		    (p->offset < end))
			pmap_page_protect(p->phys_addr, VM_PROT_NONE);
	}
	vm_object_unlock(object);
}

/*
 *	Routine:	vm_object_copy_slowly
 *
 *	Description:
 *		Copy the specified range of the source
 *		virtual memory object without using
 *		protection-based optimizations (such
 *		as copy-on-write).  The pages in the
 *		region are actually copied.
 *
 *	In/out conditions:
 *		The caller must hold a reference and a lock
 *		for the source virtual memory object.  The source
 *		object will be returned *unlocked*.
 *
 *	Results:
 *		If the copy is completed successfully, KERN_SUCCESS is
 *		returned.  If the caller asserted the interruptible
 *		argument, and an interruption occurred while waiting
 *		for a user-generated event, MACH_SEND_INTERRUPTED is
 *		returned.  Other values may be returned to indicate
 *		hard errors during the copy operation.
 *
 *		A new virtual memory object is returned in a
 *		parameter (_result_object).  The contents of this
 *		new object, starting at a zero offset, are a copy
 *		of the source memory region.  In the event of
 *		an error, this parameter will contain the value
 *		VM_OBJECT_NULL.
 */
kern_return_t vm_object_copy_slowly(
	register
	vm_object_t	src_object,
	vm_offset_t	src_offset,
	vm_size_t	size,
	boolean_t	interruptible,
	vm_object_t	*_result_object)	/* OUT */
{
	vm_object_t	new_object;
	vm_offset_t	new_offset;

	if (size == 0) {
		vm_object_unlock(src_object);
		*_result_object = VM_OBJECT_NULL;
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 *	Prevent destruction of the source object while we copy.
	 */

	assert(src_object->ref_count > 0);
	src_object->ref_count++;
	vm_object_unlock(src_object);

	/*
	 *	Create a new object to hold the copied pages.
	 *	A few notes:
	 *		We fill the new object starting at offset 0,
	 *		 regardless of the input offset.
	 *		We don't bother to lock the new object within
	 *		 this routine, since we have the only reference.
	 */

	new_object = vm_object_allocate(size);
	new_offset = 0;

	assert(size == trunc_page(size));	/* Will the loop terminate? */

	for ( ;
	    size != 0 ;
	    src_offset += PAGE_SIZE, new_offset += PAGE_SIZE, size -= PAGE_SIZE
	    ) {
		vm_page_t	new_page;
		vm_fault_return_t result;

		while ((new_page = vm_page_alloc(new_object, new_offset))
				== VM_PAGE_NULL) {
			VM_PAGE_WAIT((void (*)()) 0);
		}

		do {
			vm_prot_t	prot = VM_PROT_READ;
			vm_page_t	_result_page;
			vm_page_t	top_page;
			register
			vm_page_t	result_page;

			vm_object_lock(src_object);
			src_object->paging_in_progress++;

			result = vm_fault_page(src_object, src_offset,
				VM_PROT_READ, FALSE, interruptible,
				&prot, &_result_page, &top_page,
				FALSE, (void (*)()) 0);

			switch(result) {
				case VM_FAULT_SUCCESS:
					result_page = _result_page;

					/*
					 *	We don't need to hold the object
					 *	lock -- the busy page will be enough.
					 *	[We don't care about picking up any
					 *	new modifications.]
					 *
					 *	Copy the page to the new object.
					 *
					 *	POLICY DECISION:
					 *		If result_page is clean,
					 *		we could steal it instead
					 *		of copying.
					 */

					vm_object_unlock(result_page->object);
					vm_page_copy(result_page, new_page);

					/*
					 *	Let go of both pages (make them
					 *	not busy, perform wakeup, activate).
					 */

					new_page->busy = FALSE;
					new_page->dirty = TRUE;
					vm_object_lock(result_page->object);
					PAGE_WAKEUP_DONE(result_page);

					vm_page_lock_queues();
					if (!result_page->active &&
					    !result_page->inactive)
						vm_page_activate(result_page);
					vm_page_activate(new_page);
					vm_page_unlock_queues();

					/*
					 *	Release paging references and
					 *	top-level placeholder page, if any.
					 */

					vm_fault_cleanup(result_page->object,
							top_page);

					break;
				
				case VM_FAULT_RETRY:
					break;

				case VM_FAULT_MEMORY_SHORTAGE:
					VM_PAGE_WAIT((void (*)()) 0);
					break;

				case VM_FAULT_FICTITIOUS_SHORTAGE:
					vm_page_more_fictitious();
					break;

				case VM_FAULT_INTERRUPTED:
					vm_page_free(new_page);
					vm_object_deallocate(new_object);
					vm_object_deallocate(src_object);
					*_result_object = VM_OBJECT_NULL;
					return MACH_SEND_INTERRUPTED;

				case VM_FAULT_MEMORY_ERROR:
					/*
					 * A policy choice:
					 *	(a) ignore pages that we can't
					 *	    copy
					 *	(b) return the null object if
					 *	    any page fails [chosen]
					 */

					vm_page_free(new_page);
					vm_object_deallocate(new_object);
					vm_object_deallocate(src_object);
					*_result_object = VM_OBJECT_NULL;
					return KERN_MEMORY_ERROR;
			}
		} while (result != VM_FAULT_SUCCESS);
	}

	/*
	 *	Lose the extra reference, and return our object.
	 */

	vm_object_deallocate(src_object);
	*_result_object = new_object;
	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_object_copy_temporary
 *
 *	Purpose:
 *		Copy the specified range of the source virtual
 *		memory object, if it can be done without blocking.
 *
 *	Results:
 *		If the copy is successful, the copy is returned in
 *		the arguments; otherwise, the arguments are not
 *		affected.
 *
 *	In/out conditions:
 *		The object should be unlocked on entry and exit.
 */

vm_object_t	vm_object_copy_delayed();	/* forward declaration */

boolean_t vm_object_copy_temporary(
	vm_object_t	*_object,		/* INOUT */
	vm_offset_t	*_offset,		/* INOUT */
	boolean_t	*_src_needs_copy,	/* OUT */
	boolean_t	*_dst_needs_copy)	/* OUT */
{
	vm_object_t	object = *_object;

#ifdef	lint
	++*_offset;
#endif	/* lint */

	if (object == VM_OBJECT_NULL) {
		*_src_needs_copy = FALSE;
		*_dst_needs_copy = FALSE;
		return TRUE;
	}

	/*
	 *	If the object is temporary, we can perform
	 *	a symmetric copy-on-write without asking.
	 */

	vm_object_lock(object);
	if (object->temporary) {

		/*
		 *	Shared objects use delayed copy
		 */
		if (object->use_shared_copy) {

			/*
			 *	Asymmetric copy strategy.  Destination
			 *	must be copied (to allow copy object reuse).
			 *	Source is unaffected.
			 */
			vm_object_unlock(object);
			object = vm_object_copy_delayed(object);
			*_object = object;
			*_src_needs_copy = FALSE;
			*_dst_needs_copy = TRUE;
			return TRUE;
		}

		/*
		 *	Make another reference to the object.
		 *
		 *	Leave object/offset unchanged.
		 */

		assert(object->ref_count > 0);
		object->ref_count++;
		object->shadowed = TRUE;
		vm_object_unlock(object);

		/*
		 *	Both source and destination must make
		 *	shadows, and the source must be made
		 *	read-only if not already.
		 */

		*_src_needs_copy = TRUE;
		*_dst_needs_copy = TRUE;
		return TRUE;
	}

	if (object->pager_ready &&
	    (object->copy_strategy == MEMORY_OBJECT_COPY_DELAY)) {
	    	/* XXX Do something intelligent (see temporary code above) */
	}
	vm_object_unlock(object);

	return FALSE;
}

/*
 *	Routine:	vm_object_copy_call [internal]
 *
 *	Description:
 *		Copy the specified (src_offset, size) portion
 *		of the source object (src_object), using the
 *		user-managed copy algorithm.
 *
 *	In/out conditions:
 *		The source object must be locked on entry.  It
 *		will be *unlocked* on exit.
 *
 *	Results:
 *		If the copy is successful, KERN_SUCCESS is returned.
 *		This routine is interruptible; if a wait for
 *		a user-generated event is interrupted, MACH_SEND_INTERRUPTED
 *		is returned.  Other return values indicate hard errors
 *		in creating the user-managed memory object for the copy.
 *
 *		A new object that represents the copied virtual
 *		memory is returned in a parameter (*_result_object).
 *		If the return value indicates an error, this parameter
 *		is not valid.
 */
kern_return_t vm_object_copy_call(
	vm_object_t	src_object,
	vm_offset_t	src_offset,
	vm_size_t	size,
	vm_object_t	*_result_object)	/* OUT */
{
	vm_offset_t	src_end = src_offset + size;
	ipc_port_t	new_memory_object;
	vm_object_t	new_object;
	vm_page_t	p;

	/*
	 *	Set the backing object for the new
	 *	temporary object.
	 */

	assert(src_object->ref_count > 0);
	src_object->ref_count++;
	vm_object_paging_begin(src_object);
	vm_object_unlock(src_object);

	/*
	 *	Create a memory object port to be associated
	 *	with this new vm_object.
	 *
	 *	Since the kernel has the only rights to this
	 *	port, we need not hold the cache lock.
	 *
	 *	Since we have the only object reference, we
	 *	need not be worried about collapse operations.
	 *
	 */

	new_memory_object = ipc_port_alloc_kernel();
	if (new_memory_object == IP_NULL) {
		panic("vm_object_copy_call: allocate memory object port");
		/* XXX Shouldn't panic here. */
	}

	/* we hold a naked receive right for new_memory_object */
	(void) ipc_port_make_send(new_memory_object);
	/* now we also hold a naked send right for new_memory_object */

	/*
	 *	Let the memory manager know that a copy operation
	 *	is in progress.  Note that we're using the old
	 *	memory object's ports (for which we're holding
	 *	a paging reference)... the memory manager cannot
	 *	yet affect the new memory object.
	 */

	(void) memory_object_copy(src_object->pager,
				src_object->pager_request,
				src_offset, size,
				new_memory_object);
	/* no longer hold the naked receive right for new_memory_object */

	vm_object_lock(src_object);
	vm_object_paging_end(src_object);

	/*
	 *	Remove write access from all of the pages of
	 *	the old memory object that we can.
	 */

	queue_iterate(&src_object->memq, p, vm_page_t, listq) {
	    if (!p->fictitious &&
		(src_offset <= p->offset) &&
		(p->offset < src_end) &&
		!(p->page_lock & VM_PROT_WRITE)) {
		p->page_lock |= VM_PROT_WRITE;
		pmap_page_protect(p->phys_addr, VM_PROT_ALL & ~p->page_lock);
	    }
	}

	vm_object_unlock(src_object);
		
	/*
	 *	Initialize the rest of the paging stuff
	 */

	new_object = vm_object_enter(new_memory_object, size, FALSE);
	new_object->shadow = src_object;
	new_object->shadow_offset = src_offset;

	/*
	 *	Drop the reference for new_memory_object taken above.
	 */

	ipc_port_release_send(new_memory_object);
	/* no longer hold the naked send right for new_memory_object */

	*_result_object = new_object;
	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_object_copy_delayed [internal]
 *
 *	Description:
 *		Copy the specified virtual memory object, using
 *		the asymmetric copy-on-write algorithm.
 *
 *	In/out conditions:
 *		The object must be unlocked on entry.
 *
 *		This routine will not block waiting for user-generated
 *		events.  It is not interruptible.
 */
vm_object_t vm_object_copy_delayed(
	vm_object_t	src_object)
{
	vm_object_t	new_copy;
	vm_object_t	old_copy;
	vm_page_t	p;

	/*
	 *	The user-level memory manager wants to see
	 *	all of the changes to this object, but it
	 *	has promised not to make any changes on its own.
	 *
	 *	Perform an asymmetric copy-on-write, as follows:
	 *		Create a new object, called a "copy object"
	 *		 to hold pages modified by the new mapping
	 *		 (i.e., the copy, not the original mapping).
	 *		Record the original object as the backing
	 *		 object for the copy object.  If the
	 *		 original mapping does not change a page,
	 *		 it may be used read-only by the copy.
	 *		Record the copy object in the original
	 *		 object.  When the original mapping causes
	 *		 a page to be modified, it must be copied
	 *		 to a new page that is "pushed" to the
	 *		 copy object.
	 *		Mark the new mapping (the copy object)
	 *		 copy-on-write.  This makes the copy
	 *		 object itself read-only, allowing it
	 *		 to be reused if the original mapping
	 *		 makes no changes, and simplifying the
	 *		 synchronization required in the "push"
	 *		 operation described above.
	 *
	 *	The copy-on-write is said to be assymetric because
	 *	the original object is *not* marked copy-on-write.
	 *	A copied page is pushed to the copy object, regardless
	 *	which party attempted to modify the page.
	 *
	 *	Repeated asymmetric copy operations may be done.
	 *	If the original object has not been changed since
	 *	the last copy, its copy object can be reused.
	 *	Otherwise, a new copy object can be inserted
	 *	between the original object and its previous
	 *	copy object.  Since any copy object is read-only,
	 *	this cannot affect the contents of the previous copy
	 *	object.
	 *
	 *	Note that a copy object is higher in the object
	 *	tree than the original object; therefore, use of
	 *	the copy object recorded in the original object
	 *	must be done carefully, to avoid deadlock.
	 */

	/*
	 *	Allocate a new copy object before locking, even
	 *	though we may not need it later.
	 */

	new_copy = vm_object_allocate(src_object->size);

	vm_object_lock(src_object);

	/*
	 *	See whether we can reuse the result of a previous
	 *	copy operation.
	 */
 Retry:
	old_copy = src_object->copy;
	if (old_copy != VM_OBJECT_NULL) {
		/*
		 *	Try to get the locks (out of order)
		 */
		if (!vm_object_lock_try(old_copy)) {
			vm_object_unlock(src_object);

			simple_lock_pause();	/* wait a bit */

			vm_object_lock(src_object);
			goto Retry;
		}

		/*
		 *	Determine whether the old copy object has
		 *	been modified.
		 */

		if (old_copy->resident_page_count == 0 &&
		    !old_copy->pager_created) {
			/*
			 *	It has not been modified.
			 *
			 *	Return another reference to
			 *	the existing copy-object.
			 */
			assert(old_copy->ref_count > 0);
			old_copy->ref_count++;
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);

			vm_object_deallocate(new_copy);

			return old_copy;
		}

		/*
		 *	The copy-object is always made large enough to
		 *	completely shadow the original object, since
		 *	it may have several users who want to shadow
		 *	the original object at different points.
		 */

		assert((old_copy->shadow == src_object) &&
		    (old_copy->shadow_offset == (vm_offset_t) 0));

		/*
		 *	Make the old copy-object shadow the new one.
		 *	It will receive no more pages from the original
		 *	object.
		 */

		src_object->ref_count--;	/* remove ref. from old_copy */
		assert(src_object->ref_count > 0);
		old_copy->shadow = new_copy;
		assert(new_copy->ref_count > 0);
		new_copy->ref_count++;
		vm_object_unlock(old_copy);	/* done with old_copy */
	}

	/*
	 *	Point the new copy at the existing object.
	 */

	new_copy->shadow = src_object;
	new_copy->shadow_offset = 0;
	new_copy->shadowed = TRUE;	/* caller must set needs_copy */
	assert(src_object->ref_count > 0);
	src_object->ref_count++;
	src_object->copy = new_copy;

	/*
	 *	Mark all pages of the existing object copy-on-write.
	 *	This object may have a shadow chain below it, but
	 *	those pages will already be marked copy-on-write.
	 */

	queue_iterate(&src_object->memq, p, vm_page_t, listq) {
	    if (!p->fictitious)
		pmap_page_protect(p->phys_addr, 
				  (VM_PROT_ALL & ~VM_PROT_WRITE &
				   ~p->page_lock));
	}

	vm_object_unlock(src_object);
	
	return new_copy;
}

/*
 *	Routine:	vm_object_copy_strategically
 *
 *	Purpose:
 *		Perform a copy according to the source object's
 *		declared strategy.  This operation may block,
 *		and may be interrupted.
 */
kern_return_t	vm_object_copy_strategically(
	register
	vm_object_t	src_object,
	vm_offset_t	src_offset,
	vm_size_t	size,
	vm_object_t	*dst_object,	/* OUT */
	vm_offset_t	*dst_offset,	/* OUT */
	boolean_t	*dst_needs_copy) /* OUT */
{
	kern_return_t	result = KERN_SUCCESS;	/* to quiet gcc warnings */
	boolean_t	interruptible = TRUE; /* XXX */

	assert(src_object != VM_OBJECT_NULL);

	vm_object_lock(src_object);

	/* XXX assert(!src_object->temporary);  JSB FIXME */

	/*
	 *	The copy strategy is only valid if the memory manager
	 *	is "ready".
	 */

	while (!src_object->pager_ready) {
		vm_object_wait(	src_object,
				VM_OBJECT_EVENT_PAGER_READY,
				interruptible);
		if (interruptible &&
		    (current_thread()->wait_result != THREAD_AWAKENED)) {
			*dst_object = VM_OBJECT_NULL;
			*dst_offset = 0;
			*dst_needs_copy = FALSE;
			return MACH_SEND_INTERRUPTED;
		}
		vm_object_lock(src_object);
	}

	/*
	 *	The object may be temporary (even though it is external).
	 *	If so, do a symmetric copy.
	 */

	if (src_object->temporary) {
		/*
		 *	XXX
		 *	This does not count as intelligent!
		 *	This buys us the object->temporary optimizations,
		 *	but we aren't using a symmetric copy,
		 *	which may confuse the vm code. The correct thing
		 *	to do here is to figure out what to call to get
		 *	a temporary shadowing set up.
		 */
		src_object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
	}

	/*
	 *	The object is permanent. Use the appropriate copy strategy.
	 */

	switch (src_object->copy_strategy) {
	    case MEMORY_OBJECT_COPY_NONE:
		if ((result = vm_object_copy_slowly(
					src_object,
					src_offset,
					size,
					interruptible,
					dst_object))
		    == KERN_SUCCESS) {
			*dst_offset = 0;
			*dst_needs_copy = FALSE;
		}
		break;

	    case MEMORY_OBJECT_COPY_CALL:
		if ((result = vm_object_copy_call(	
				src_object,
				src_offset,
				size,
				dst_object))
		    == KERN_SUCCESS) {
			*dst_offset = 0;
			*dst_needs_copy = FALSE;
		}
		break;

	    case MEMORY_OBJECT_COPY_DELAY:
		vm_object_unlock(src_object);
		*dst_object = vm_object_copy_delayed(src_object);
		*dst_offset = src_offset;
		*dst_needs_copy = TRUE;

		result = KERN_SUCCESS;
		break;
	}

	return result;
}

/*
 *	vm_object_shadow:
 *
 *	Create a new object which is backed by the
 *	specified existing object range.  The source
 *	object reference is deallocated.
 *
 *	The new object and offset into that object
 *	are returned in the source parameters.
 */

void vm_object_shadow(
	vm_object_t	*object,	/* IN/OUT */
	vm_offset_t	*offset,	/* IN/OUT */
	vm_size_t	length)
{
	register vm_object_t	source;
	register vm_object_t	result;

	source = *object;

	/*
	 *	Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(length)) == VM_OBJECT_NULL)
		panic("vm_object_shadow: no object for shadowing");

	/*
	 *	The new object shadows the source object, adding
	 *	a reference to it.  Our caller changes his reference
	 *	to point to the new object, removing a reference to
	 *	the source object.  Net result: no change of reference
	 *	count.
	 */
	result->shadow = source;
	
	/*
	 *	Store the offset into the source object,
	 *	and fix up the offset into the new object.
	 */

	result->shadow_offset = *offset;

	/*
	 *	Return the new things
	 */

	*offset = 0;
	*object = result;
}

/*
 *	The relationship between vm_object structures and
 *	the memory_object ports requires careful synchronization.
 *
 *	All associations are created by vm_object_enter.  All three
 *	port fields are filled in, as follows:
 *		pager:	the memory_object port itself, supplied by
 *			the user requesting a mapping (or the kernel,
 *			when initializing internal objects); the
 *			kernel simulates holding send rights by keeping
 *			a port reference;
 *		pager_request:
 *		pager_name:
 *			the memory object control and name ports,
 *			created by the kernel; the kernel holds
 *			receive (and ownership) rights to these
 *			ports, but no other references.
 *	All of the ports are referenced by their global names.
 *
 *	When initialization is complete, the "initialized" field
 *	is asserted.  Other mappings using a particular memory object,
 *	and any references to the vm_object gained through the
 *	port association must wait for this initialization to occur.
 *
 *	In order to allow the memory manager to set attributes before
 *	requests (notably virtual copy operations, but also data or
 *	unlock requests) are made, a "ready" attribute is made available.
 *	Only the memory manager may affect the value of this attribute.
 *	Its value does not affect critical kernel functions, such as
 *	internal object initialization or destruction.  [Furthermore,
 *	memory objects created by the kernel are assumed to be ready
 *	immediately; the default memory manager need not explicitly
 *	set the "ready" attribute.]
 *
 *	[Both the "initialized" and "ready" attribute wait conditions
 *	use the "pager" field as the wait event.]
 *
 *	The port associations can be broken down by any of the
 *	following routines:
 *		vm_object_terminate:
 *			No references to the vm_object remain, and
 *			the object cannot (or will not) be cached.
 *			This is the normal case, and is done even
 *			though one of the other cases has already been
 *			done.
 *		vm_object_destroy:
 *			The memory_object port has been destroyed,
 *			meaning that the kernel cannot flush dirty
 *			pages or request new data or unlock existing
 *			data.
 *		memory_object_destroy:
 *			The memory manager has requested that the
 *			kernel relinquish rights to the memory object
 *			port.  [The memory manager may not want to
 *			destroy the port, but may wish to refuse or
 *			tear down existing memory mappings.]
 *	Each routine that breaks an association must break all of
 *	them at once.  At some later time, that routine must clear
 *	the vm_object port fields and release the port rights.
 *	[Furthermore, each routine must cope with the simultaneous
 *	or previous operations of the others.]
 *
 *	In addition to the lock on the object, the vm_object_cache_lock
 *	governs the port associations.  References gained through the
 *	port association require use of the cache lock.
 *
 *	Because the port fields may be cleared spontaneously, they
 *	cannot be used to determine whether a memory object has
 *	ever been associated with a particular vm_object.  [This
 *	knowledge is important to the shadow object mechanism.]
 *	For this reason, an additional "created" attribute is
 *	provided.
 *
 *	During various paging operations, the port values found in the
 *	vm_object must be valid.  To prevent these port rights from being
 *	released, and to prevent the port associations from changing
 *	(other than being removed, i.e., made null), routines may use
 *	the vm_object_paging_begin/end routines [actually, macros].
 *	The implementation uses the "paging_in_progress" and "wanted" fields.
 *	[Operations that alter the validity of the port values include the
 *	termination routines and vm_object_collapse.]
 */

vm_object_t vm_object_lookup(
	ipc_port_t	port)
{
	vm_object_t	object = VM_OBJECT_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
#if	NORMA_VM
		    (ip_kotype(port) == IKOT_PAGER)) {
#else	/* NORMA_VM */
		    (ip_kotype(port) == IKOT_PAGING_REQUEST)) {
#endif	/* NORMA_VM */
			vm_object_cache_lock();
			object = (vm_object_t) port->ip_kobject;
			vm_object_lock(object);

			assert(object->alive);

			if (object->ref_count == 0) {
				queue_remove(&vm_object_cached_list, object,
					     vm_object_t, cached_list);
				vm_object_cached_count--;
			}

			object->ref_count++;
			vm_object_unlock(object);
			vm_object_cache_unlock();
		}
		ip_unlock(port);
	}

	return object;
}

vm_object_t vm_object_lookup_name(
	ipc_port_t	port)
{
	vm_object_t	object = VM_OBJECT_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_PAGING_NAME)) {
			vm_object_cache_lock();
			object = (vm_object_t) port->ip_kobject;
			vm_object_lock(object);

			assert(object->alive);

			if (object->ref_count == 0) {
				queue_remove(&vm_object_cached_list, object,
					     vm_object_t, cached_list);
				vm_object_cached_count--;
			}

			object->ref_count++;
			vm_object_unlock(object);
			vm_object_cache_unlock();
		}
		ip_unlock(port);
	}

	return object;
}

void vm_object_destroy(
	ipc_port_t	pager)
{
	vm_object_t	object;
	pager_request_t	old_request;
	ipc_port_t	old_name;

	/*
	 *	Perform essentially the same operations as in vm_object_lookup,
	 *	except that this time we look up based on the memory_object
	 *	port, not the control port.
	 */
	vm_object_cache_lock();
	if (ip_kotype(pager) != IKOT_PAGER) {
		vm_object_cache_unlock();
		return;
	}

	object = (vm_object_t) pager->ip_kobject;
	vm_object_lock(object);
	if (object->ref_count == 0) {
		queue_remove(&vm_object_cached_list, object,
				vm_object_t, cached_list);
		vm_object_cached_count--;
	}
	object->ref_count++;

	object->can_persist = FALSE;

	assert(object->pager == pager);

	/*
	 *	Remove the port associations.
	 *
	 *	Note that the memory_object itself is dead, so
	 *	we don't bother with it.
	 */

	object->pager = IP_NULL;
	vm_object_remove(object);

	old_request = object->pager_request;
	object->pager_request = PAGER_REQUEST_NULL;

	old_name = object->pager_name;
	object->pager_name = IP_NULL;

	vm_object_unlock(object);
	vm_object_cache_unlock();

	/*
	 *	Clean up the port references.  Note that there's no
	 *	point in trying the memory_object_terminate call
	 *	because the memory_object itself is dead.
	 */

	ipc_port_release_send(pager);
#if	!NORMA_VM
	if (old_request != IP_NULL)
		ipc_port_dealloc_kernel(old_request);
#endif	/* !NORMA_VM */
	if (old_name != IP_NULL)
#if	NORMA_VM
		ipc_port_release_send(old_name);
#else	/* NORMA_VM */
		ipc_port_dealloc_kernel(old_name);
#endif	/* NORMA_VM */

	/*
	 *	Restart pending page requests
	 */

	vm_object_abort_activity(object);

	/*
	 *	Lose the object reference.
	 */

	vm_object_deallocate(object);
}

boolean_t	vm_object_accept_old_init_protocol = FALSE;

/*
 *	Routine:	vm_object_enter
 *	Purpose:
 *		Find a VM object corresponding to the given
 *		pager; if no such object exists, create one,
 *		and initialize the pager.
 */
vm_object_t vm_object_enter(
	ipc_port_t	pager,
	vm_size_t	size,
	boolean_t	internal)
{
	register
	vm_object_t	object;
	vm_object_t	new_object;
	boolean_t	must_init;
	ipc_kobject_type_t po;

restart:
	if (!IP_VALID(pager))
		return vm_object_allocate(size);

	new_object = VM_OBJECT_NULL;
	must_init = FALSE;

	/*
	 *	Look for an object associated with this port.
	 */

	vm_object_cache_lock();
	for (;;) {
		po = ip_kotype(pager);

		/*
		 *	If a previous object is being terminated,
		 *	we must wait for the termination message
		 *	to be queued.
		 *
		 *	We set kobject to a non-null value to let the
		 *	terminator know that someone is waiting.
		 *	Among the possibilities is that the port
		 *	could die while we're waiting.  Must restart
		 *	instead of continuing the loop.
		 */

		if (po == IKOT_PAGER_TERMINATING) {
			pager->ip_kobject = (ipc_kobject_t) pager;
			assert_wait((event_t) pager, FALSE);
			vm_object_cache_unlock();
			thread_block((void (*)()) 0);
			goto restart;
		}

		/*
		 *	Bail if there is already a kobject associated
		 *	with the pager port.
		 */
		if (po != IKOT_NONE) {
			break;
		}

		/*
		 *	We must unlock to create a new object;
		 *	if we do so, we must try the lookup again.
		 */

		if (new_object == VM_OBJECT_NULL) {
			vm_object_cache_unlock();
			new_object = vm_object_allocate(size);
			vm_object_cache_lock();
		} else {
			/*
			 *	Lookup failed twice, and we have something
			 *	to insert; set the object.
			 */

			ipc_kobject_set(pager,
					(ipc_kobject_t) new_object,
					IKOT_PAGER);
			new_object = VM_OBJECT_NULL;
			must_init = TRUE;
		}
	}

	if (internal)
		must_init = TRUE;

	/*
	 *	It's only good if it's a VM object!
	 */

	object = (po == IKOT_PAGER) ? (vm_object_t) pager->ip_kobject
				    : VM_OBJECT_NULL;

	if ((object != VM_OBJECT_NULL) && !must_init) {
		vm_object_lock(object);
		if (object->ref_count == 0) {
			queue_remove(&vm_object_cached_list, object,
					vm_object_t, cached_list);
			vm_object_cached_count--;
		}
		object->ref_count++;
		vm_object_unlock(object);

		vm_stat.hits++;
	}
	assert((object == VM_OBJECT_NULL) || (object->ref_count > 0) ||
		((object->paging_in_progress != 0) && internal));

	vm_stat.lookups++;

	vm_object_cache_unlock();

	/*
	 *	If we raced to create a vm_object but lost, let's
	 *	throw away ours.
	 */

	if (new_object != VM_OBJECT_NULL)
		vm_object_deallocate(new_object);

	if (object == VM_OBJECT_NULL)
		return(object);

	if (must_init) {
		/*
		 *	Copy the naked send right we were given.
		 */

		pager = ipc_port_copy_send(pager);
		if (!IP_VALID(pager))
			panic("vm_object_enter: port died"); /* XXX */

		object->pager_created = TRUE;
		object->pager = pager;

#if	NORMA_VM

		/*
		 *	Let the xmm system know that we want to use the pager.
		 *
		 *	Name port will be provided by the xmm system
		 *	when set_attributes_common is called.
		 */

		object->internal = internal;
		object->pager_ready = internal;
		if (internal) {
			assert(object->temporary);
		} else {
			object->temporary = FALSE;
		}
		object->pager_name = IP_NULL;

		(void) xmm_memory_object_init(object);
#else	/* NORMA_VM */

		/*
		 *	Allocate request port.
		 */

		object->pager_request = ipc_port_alloc_kernel();
		if (object->pager_request == IP_NULL)
			panic("vm_object_enter: pager request alloc");

		ipc_kobject_set(object->pager_request,
				(ipc_kobject_t) object,
				IKOT_PAGING_REQUEST);

		/*
		 *	Let the pager know we're using it.
		 */

		if (internal) {
			/* acquire a naked send right for the DMM */
			ipc_port_t DMM = memory_manager_default_reference();

			/* mark the object internal */
			object->internal = TRUE;
			assert(object->temporary);

			/* default-pager objects are ready immediately */
			object->pager_ready = TRUE;

			/* consumes the naked send right for DMM */
			(void) memory_object_create(DMM,
				pager,
				object->size,
				object->pager_request,
				object->pager_name,
				PAGE_SIZE);
		} else {
			/* the object is external and not temporary */
			object->internal = FALSE;
			object->temporary = FALSE;

			/* user pager objects are not ready until marked so */
			object->pager_ready = FALSE;

			(void) memory_object_init(pager,
				object->pager_request,
				object->pager_name,
				PAGE_SIZE);

		}
#endif	/* NORMA_VM */

		vm_object_lock(object);
		object->pager_initialized = TRUE;

		if (vm_object_accept_old_init_protocol)
			object->pager_ready = TRUE;

		vm_object_wakeup(object, VM_OBJECT_EVENT_INITIALIZED);
	} else {
		vm_object_lock(object);
	}
	/*
	 *	[At this point, the object must be locked]
	 */

	/*
	 *	Wait for the work above to be done by the first
	 *	thread to map this object.
	 */

	while (!object->pager_initialized) {
		vm_object_wait(	object,
				VM_OBJECT_EVENT_INITIALIZED,
				FALSE);
		vm_object_lock(object);
	}
	vm_object_unlock(object);

	return object;
}

/*
 *	Routine:	vm_object_pager_create
 *	Purpose:
 *		Create a memory object for an internal object.
 *	In/out conditions:
 *		The object is locked on entry and exit;
 *		it may be unlocked within this call.
 *	Limitations:
 *		Only one thread may be performing a
 *		vm_object_pager_create on an object at
 *		a time.  Presumably, only the pageout
 *		daemon will be using this routine.
 */
void vm_object_pager_create(
	register
	vm_object_t	object)
{
	ipc_port_t	pager;

	if (object->pager_created) {
		/*
		 *	Someone else got to it first...
		 *	wait for them to finish initializing
		 */

		while (!object->pager_initialized) {
			vm_object_wait(	object,
					VM_OBJECT_EVENT_PAGER_READY,
					FALSE);
			vm_object_lock(object);
		}
		return;
	}

	/*
	 *	Indicate that a memory object has been assigned
	 *	before dropping the lock, to prevent a race.
	 */

	object->pager_created = TRUE;
		
	/*
	 *	Prevent collapse or termination by
	 *	holding a paging reference
	 */

	vm_object_paging_begin(object);
	vm_object_unlock(object);

#if	MACH_PAGEMAP
	object->existence_info = vm_external_create(
					object->size +
					object->paging_offset);
	assert((object->size + object->paging_offset) >=
		object->size);
#endif	/* MACH_PAGEMAP */

	/*
	 *	Create the pager, and associate with it
	 *	this object.
	 *
	 *	Note that we only make the port association
	 *	so that vm_object_enter can properly look up
	 *	the object to complete the initialization...
	 *	we do not expect any user to ever map this
	 *	object.
	 *
	 *	Since the kernel has the only rights to the
	 *	port, it's safe to install the association
	 *	without holding the cache lock.
	 */

	pager = ipc_port_alloc_kernel();
	if (pager == IP_NULL)
		panic("vm_object_pager_create: allocate pager port");

	(void) ipc_port_make_send(pager);
	ipc_kobject_set(pager, (ipc_kobject_t) object, IKOT_PAGER);

	/*
	 *	Initialize the rest of the paging stuff
	 */

	if (vm_object_enter(pager, object->size, TRUE) != object)
		panic("vm_object_pager_create: mismatch");

	/*
	 *	Drop the naked send right taken above.
	 */

	ipc_port_release_send(pager);

	/*
	 *	Release the paging reference
	 */

	vm_object_lock(object);
	vm_object_paging_end(object);
}

/*
 *	Routine:	vm_object_remove
 *	Purpose:
 *		Eliminate the pager/object association
 *		for this pager.
 *	Conditions:
 *		The object cache must be locked.
 */
void vm_object_remove(
	vm_object_t	object)
{
	ipc_port_t port;

	if ((port = object->pager) != IP_NULL) {
		if (ip_kotype(port) == IKOT_PAGER)
			ipc_kobject_set(port, IKO_NULL,
					IKOT_PAGER_TERMINATING);
		 else if (ip_kotype(port) != IKOT_NONE)
			panic("vm_object_remove: bad object port");
	}
#if	!NORMA_VM
	if ((port = object->pager_request) != IP_NULL) {
		if (ip_kotype(port) == IKOT_PAGING_REQUEST)
			ipc_kobject_set(port, IKO_NULL, IKOT_NONE);
		 else if (ip_kotype(port) != IKOT_NONE)
			panic("vm_object_remove: bad request port");
	}
	if ((port = object->pager_name) != IP_NULL) {
		if (ip_kotype(port) == IKOT_PAGING_NAME)
			ipc_kobject_set(port, IKO_NULL, IKOT_NONE);
		 else if (ip_kotype(port) != IKOT_NONE)
			panic("vm_object_remove: bad name port");
	}
#endif	/* !NORMA_VM */
}

/*
 *	Global variables for vm_object_collapse():
 *
 *		Counts for normal collapses and bypasses.
 *		Debugging variables, to watch or disable collapse.
 */
long	object_collapses = 0;
long	object_bypasses  = 0;

int		vm_object_collapse_debug = 0;
boolean_t	vm_object_collapse_allowed = TRUE;
boolean_t	vm_object_collapse_bypass_allowed = TRUE;

/*
 *	vm_object_collapse:
 *
 *	Collapse an object with the object backing it.
 *	Pages in the backing object are moved into the
 *	parent, and the backing object is deallocated.
 *
 *	Requires that the object be locked and the page
 *	queues be unlocked.  May unlock/relock the object,
 *	so the caller should hold a reference for the object.
 */
void vm_object_collapse(
	register vm_object_t	object)
{
	register vm_object_t	backing_object;
	register vm_offset_t	backing_offset;
	register vm_size_t	size;
	register vm_offset_t	new_offset;
	register vm_page_t	p, pp;
	ipc_port_t old_name_port;

	if (!vm_object_collapse_allowed)
		return;

	while (TRUE) {
		/*
		 *	Verify that the conditions are right for collapse:
		 *
		 *	The object exists and no pages in it are currently
		 *	being paged out (or have ever been paged out).
		 *
		 *	This check is probably overkill -- if a memory
		 *	object has not been created, the fault handler
		 *	shouldn't release the object lock while paging
		 *	is in progress or absent pages exist.
		 */
		if (object == VM_OBJECT_NULL ||
		    object->pager_created ||
		    object->paging_in_progress != 0 ||
		    object->absent_count != 0)
			return;

		/*
		 *		There is a backing object, and
		 */
	
		if ((backing_object = object->shadow) == VM_OBJECT_NULL)
			return;
	
		vm_object_lock(backing_object);
		/*
		 *	...
		 *		The backing object is not read_only,
		 *		and no pages in the backing object are
		 *		currently being paged out.
		 *		The backing object is internal.
		 *
		 *	XXX It may be sufficient for the backing
		 *	XXX object to be temporary.
		 */
	
		if (!backing_object->internal ||
		    backing_object->paging_in_progress != 0) {
			vm_object_unlock(backing_object);
			return;
		}
	
		/*
		 *	The backing object can't be a copy-object:
		 *	the shadow_offset for the copy-object must stay
		 *	as 0.  Furthermore (for the 'we have all the
		 *	pages' case), if we bypass backing_object and
		 *	just shadow the next object in the chain, old
		 *	pages from that object would then have to be copied
		 *	BOTH into the (former) backing_object and into the
		 *	parent object.
		 */
		if (backing_object->shadow != VM_OBJECT_NULL &&
		    backing_object->shadow->copy != VM_OBJECT_NULL) {
			vm_object_unlock(backing_object);
			return;
		}

		/*
		 *	We know that we can either collapse the backing
		 *	object (if the parent is the only reference to
		 *	it) or (perhaps) remove the parent's reference
		 *	to it.
		 */

		backing_offset = object->shadow_offset;
		size = object->size;

		/*
		 *	If there is exactly one reference to the backing
		 *	object, we can collapse it into the parent.
		 */
	
		if (backing_object->ref_count == 1) {
			if (!vm_object_cache_lock_try()) {
				vm_object_unlock(backing_object);
				return;
			}

			/*
			 *	We can collapse the backing object.
			 *
			 *	Move all in-memory pages from backing_object
			 *	to the parent.  Pages that have been paged out
			 *	will be overwritten by any of the parent's
			 *	pages that shadow them.
			 */

			while (!queue_empty(&backing_object->memq)) {

				p = (vm_page_t)
					queue_first(&backing_object->memq);

				new_offset = (p->offset - backing_offset);

				assert(!p->busy || p->absent);

				/*
				 *	If the parent has a page here, or if
				 *	this page falls outside the parent,
				 *	dispose of it.
				 *
				 *	Otherwise, move it as planned.
				 */

				if (p->offset < backing_offset ||
				    new_offset >= size) {
					vm_page_lock_queues();
					vm_page_free(p);
					vm_page_unlock_queues();
				} else {
				    pp = vm_page_lookup(object, new_offset);
				    if (pp != VM_PAGE_NULL && !pp->absent) {
					/*
					 *	Parent object has a real page.
					 *	Throw away the backing object's
					 *	page.
					 */
					vm_page_lock_queues();
					vm_page_free(p);
					vm_page_unlock_queues();
				    }
				    else {
					if (pp != VM_PAGE_NULL) {
					    /*
					     *	Parent has an absent page...
					     *	it's not being paged in, so
					     *	it must really be missing from
					     *	the parent.
					     *
					     *	Throw out the absent page...
					     *	any faults looking for that
					     *	page will restart with the new
					     *	one.
					     */

					    /*
					     *	This should never happen -- the
					     *	parent cannot have ever had an
					     *	external memory object, and thus
					     *	cannot have absent pages.
					     */
					    panic("vm_object_collapse: bad case");

					    vm_page_lock_queues();
					    vm_page_free(pp);
					    vm_page_unlock_queues();

					    /*
					     *	Fall through to move the backing
					     *	object's page up.
					     */
					}
					/*
					 *	Parent now has no page.
					 *	Move the backing object's page up.
					 */
					vm_page_rename(p, object, new_offset);
				    }
				}
			}

			/*
			 *	Move the pager from backing_object to object.
			 *
			 *	XXX We're only using part of the paging space
			 *	for keeps now... we ought to discard the
			 *	unused portion.
			 */

			switch (vm_object_collapse_debug) {
			    case 0:
			    	break;
			    case 1:
				if ((backing_object->pager == IP_NULL) &&
				    (backing_object->pager_request ==
				     PAGER_REQUEST_NULL))
				    break;
				/* Fall through to... */

			    default:
				printf("vm_object_collapse: %#x (pager %#x, request %#x) up to %#x\n",
					backing_object, backing_object->pager, backing_object->pager_request,
					object);
				if (vm_object_collapse_debug > 2)
				    Debugger("vm_object_collapse");
			}

			object->pager = backing_object->pager;
			if (object->pager != IP_NULL)
				ipc_kobject_set(object->pager,
						(ipc_kobject_t) object,
						IKOT_PAGER);
			object->pager_initialized = backing_object->pager_initialized;
			object->pager_ready = backing_object->pager_ready;
			object->pager_created = backing_object->pager_created;

			object->pager_request = backing_object->pager_request;
#if	NORMA_VM
			old_name_port = object->pager_name;
			object->pager_name = backing_object->pager_name;
#else	/* NORMA_VM */
			if (object->pager_request != IP_NULL)
				ipc_kobject_set(object->pager_request,
						(ipc_kobject_t) object,
						IKOT_PAGING_REQUEST);
			old_name_port = object->pager_name;
			if (old_name_port != IP_NULL)
				ipc_kobject_set(old_name_port,
						IKO_NULL, IKOT_NONE);
			object->pager_name = backing_object->pager_name;
			if (object->pager_name != IP_NULL)
				ipc_kobject_set(object->pager_name,
						(ipc_kobject_t) object,
						IKOT_PAGING_NAME);
#endif	/* NORMA_VM */

			vm_object_cache_unlock();

			/*
			 * If there is no pager, leave paging-offset alone.
			 */
			if (object->pager != IP_NULL)
				object->paging_offset =
					backing_object->paging_offset +
						backing_offset;

#if	MACH_PAGEMAP
			assert(object->existence_info == VM_EXTERNAL_NULL);
			object->existence_info = backing_object->existence_info;
#endif	/* MACH_PAGEMAP */

			/*
			 *	Object now shadows whatever backing_object did.
			 *	Note that the reference to backing_object->shadow
			 *	moves from within backing_object to within object.
			 */

			object->shadow = backing_object->shadow;
			object->shadow_offset += backing_object->shadow_offset;
			if (object->shadow != VM_OBJECT_NULL &&
			    object->shadow->copy != VM_OBJECT_NULL) {
				panic("vm_object_collapse: we collapsed a copy-object!");
			}
			/*
			 *	Discard backing_object.
			 *
			 *	Since the backing object has no pages, no
			 *	pager left, and no object references within it,
			 *	all that is necessary is to dispose of it.
			 */

			assert(
				(backing_object->ref_count == 1) &&
				(backing_object->resident_page_count == 0) &&
				(backing_object->paging_in_progress == 0)
			);

			assert(backing_object->alive);
			backing_object->alive = FALSE;
			vm_object_unlock(backing_object);

			vm_object_unlock(object);
			if (old_name_port != IP_NULL)
#if	NORMA_VM
				ipc_port_release_send(old_name_port);
#else	/* NORMA_VM */
				ipc_port_dealloc_kernel(old_name_port);
#endif	/* NORMA_VM */
			zfree(vm_object_zone, (vm_offset_t) backing_object);
			vm_object_lock(object);

			object_collapses++;
		}
		else {
			if (!vm_object_collapse_bypass_allowed) {
				vm_object_unlock(backing_object);
				return;
			}

			/*
			 *	If all of the pages in the backing object are
			 *	shadowed by the parent object, the parent
			 *	object no longer has to shadow the backing
			 *	object; it can shadow the next one in the
			 *	chain.
			 *
			 *	The backing object must not be paged out - we'd
			 *	have to check all of the paged-out pages, as
			 *	well.
			 */

			if (backing_object->pager_created) {
				vm_object_unlock(backing_object);
				return;
			}

			/*
			 *	Should have a check for a 'small' number
			 *	of pages here.
			 */

			queue_iterate(&backing_object->memq, p,
				      vm_page_t, listq)
			{
				new_offset = (p->offset - backing_offset);

				/*
				 *	If the parent has a page here, or if
				 *	this page falls outside the parent,
				 *	keep going.
				 *
				 *	Otherwise, the backing_object must be
				 *	left in the chain.
				 */

				if (p->offset >= backing_offset &&
				    new_offset <= size &&
				    (pp = vm_page_lookup(object, new_offset))
				      == VM_PAGE_NULL) {
					/*
					 *	Page still needed.
					 *	Can't go any further.
					 */
					vm_object_unlock(backing_object);
					return;
				}
			}

			/*
			 *	Make the parent shadow the next object
			 *	in the chain.  Deallocating backing_object
			 *	will not remove it, since its reference
			 *	count is at least 2.
			 */

			vm_object_reference(object->shadow = backing_object->shadow);
			object->shadow_offset += backing_object->shadow_offset;

			/*
			 *	Backing object might have had a copy pointer
			 *	to us.  If it did, clear it. 
			 */
			if (backing_object->copy == object)
				backing_object->copy = VM_OBJECT_NULL;

			/*
			 *	Drop the reference count on backing_object.
			 *	Since its ref_count was at least 2, it
			 *	will not vanish; so we don't need to call
			 *	vm_object_deallocate.
			 */
			backing_object->ref_count--;
			assert(backing_object->ref_count > 0);
			vm_object_unlock(backing_object);

			object_bypasses ++;

		}

		/*
		 *	Try again with this object's new backing object.
		 */
	}
}

/*
 *	Routine:	vm_object_page_remove: [internal]
 *	Purpose:
 *		Removes all physical pages in the specified
 *		object range from the object's list of pages.
 *
 *	In/out conditions:
 *		The object must be locked.
 */
unsigned int vm_object_page_remove_lookup = 0;
unsigned int vm_object_page_remove_iterate = 0;

void vm_object_page_remove(
	register vm_object_t	object,
	register vm_offset_t	start,
	register vm_offset_t	end)
{
	register vm_page_t	p, next;

	/*
	 *	One and two page removals are most popular.
	 *	The factor of 16 here is somewhat arbitrary.
	 *	It balances vm_object_lookup vs iteration.
	 */

	if (atop(end - start) < (unsigned)object->resident_page_count/16) {
		vm_object_page_remove_lookup++;

		for (; start < end; start += PAGE_SIZE) {
			p = vm_page_lookup(object, start);
			if (p != VM_PAGE_NULL) {
				if (!p->fictitious)
					pmap_page_protect(p->phys_addr,
							  VM_PROT_NONE);
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
			}
		}
	} else {
		vm_object_page_remove_iterate++;

		p = (vm_page_t) queue_first(&object->memq);
		while (!queue_end(&object->memq, (queue_entry_t) p)) {
			next = (vm_page_t) queue_next(&p->listq);
			if ((start <= p->offset) && (p->offset < end)) {
				if (!p->fictitious)
				    pmap_page_protect(p->phys_addr,
						      VM_PROT_NONE);
				vm_page_lock_queues();
				vm_page_free(p);
				vm_page_unlock_queues();
			}
			p = next;
		}
	}
}

/*
 *	Routine:	vm_object_coalesce
 *	Function:	Coalesces two objects backing up adjoining
 *			regions of memory into a single object.
 *
 *	returns TRUE if objects were combined.
 *
 *	NOTE:	Only works at the moment if the second object is NULL -
 *		if it's not, which object do we lock first?
 *
 *	Parameters:
 *		prev_object	First object to coalesce
 *		prev_offset	Offset into prev_object
 *		next_object	Second object into coalesce
 *		next_offset	Offset into next_object
 *
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to next_object
 *
 *	Conditions:
 *	The object must *not* be locked.
 */

boolean_t vm_object_coalesce(
	register vm_object_t prev_object,
	vm_object_t	next_object,
	vm_offset_t	prev_offset,
	vm_offset_t	next_offset,
	vm_size_t	prev_size,
	vm_size_t	next_size)
{
	vm_size_t	newsize;

#ifdef	lint
	next_offset++;
#endif	/* lint */

	if (next_object != VM_OBJECT_NULL) {
		return FALSE;
	}

	if (prev_object == VM_OBJECT_NULL) {
		return TRUE;
	}

	vm_object_lock(prev_object);

	/*
	 *	Try to collapse the object first
	 */
	vm_object_collapse(prev_object);

	/*
	 *	Can't coalesce if pages not mapped to
	 *	prev_entry may be in use anyway:
	 *	. more than one reference
	 *	. paged out
	 *	. shadows another object
	 *	. has a copy elsewhere
	 *	. paging references (pages might be in page-list)
	 */

	if ((prev_object->ref_count > 1) ||
	    prev_object->pager_created ||
	    (prev_object->shadow != VM_OBJECT_NULL) ||
	    (prev_object->copy != VM_OBJECT_NULL) ||
	    (prev_object->paging_in_progress != 0)) {
		vm_object_unlock(prev_object);
		return FALSE;
	}

	/*
	 *	Remove any pages that may still be in the object from
	 *	a previous deallocation.
	 */

	vm_object_page_remove(prev_object,
			prev_offset + prev_size,
			prev_offset + prev_size + next_size);

	/*
	 *	Extend the object if necessary.
	 */
	newsize = prev_offset + prev_size + next_size;
	if (newsize > prev_object->size)
		prev_object->size = newsize;

	vm_object_unlock(prev_object);
	return TRUE;
}

vm_object_t	vm_object_request_object(
	ipc_port_t	p)
{
	return vm_object_lookup(p);
}

/*
 *	Routine:	vm_object_name
 *	Purpose:
 *		Returns a naked send right to the "name" port associated
 *		with this object.
 */
ipc_port_t	vm_object_name(
	vm_object_t	object)
{
	ipc_port_t	p;

	if (object == VM_OBJECT_NULL)
		return IP_NULL;

	vm_object_lock(object);

	while (object->shadow != VM_OBJECT_NULL) {
		vm_object_t	new_object = object->shadow;
		vm_object_lock(new_object);
		vm_object_unlock(object);
		object = new_object;
	}

	p = object->pager_name;
	if (p != IP_NULL)
#if	NORMA_VM
		p = ipc_port_copy_send(p);
#else	/* NORMA_VM */
		p = ipc_port_make_send(p);
#endif	/* NORMA_VM */
	vm_object_unlock(object);

	return p;
}

/*
 *	Attach a set of physical pages to an object, so that they can
 *	be mapped by mapping the object.  Typically used to map IO memory.
 *
 *	The mapping function and its private data are used to obtain the
 *	physical addresses for each page to be mapped.
 */
void
vm_object_page_map(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_size_t	size,
	vm_offset_t	(*map_fn)(void *, vm_offset_t),
	void *		map_fn_data)	/* private to map_fn */
{
	int	num_pages;
	int	i;
	vm_page_t	m;
	vm_page_t	old_page;
	vm_offset_t	addr;

	num_pages = atop(size);

	for (i = 0; i < num_pages; i++, offset += PAGE_SIZE) {

	    addr = (*map_fn)(map_fn_data, offset);

	    while ((m = vm_page_grab_fictitious()) == VM_PAGE_NULL)
		vm_page_more_fictitious();

	    vm_object_lock(object);
	    if ((old_page = vm_page_lookup(object, offset))
			!= VM_PAGE_NULL)
	    {
		vm_page_lock_queues();
		vm_page_free(old_page);
		vm_page_unlock_queues();
	    }

	    vm_page_init(m, addr);
	    m->private = TRUE;		/* don`t free page */
	    m->wire_count = 1;
	    vm_page_lock_queues();
	    vm_page_insert(m, object, offset);
	    vm_page_unlock_queues();

	    PAGE_WAKEUP_DONE(m);
	    vm_object_unlock(object);
	}
}

#include <mach_kdb.h>


#if	MACH_KDB
#define printf	kdbprintf

boolean_t	vm_object_print_pages = FALSE;

/*
 *	vm_object_print:	[ debug ]
 */
void vm_object_print(
	vm_object_t	object)
{
	register vm_page_t	p;
	extern indent;

	register int count;

	if (object == VM_OBJECT_NULL)
		return;

	iprintf("Object 0x%X: size=0x%X",
		(vm_offset_t) object, (vm_offset_t) object->size);
	 printf(", %d references, %d resident pages,", object->ref_count,
		object->resident_page_count);
	 printf(" %d absent pages,", object->absent_count);
	 printf(" %d paging ops\n", object->paging_in_progress);
	indent += 2;
	iprintf("memory object=0x%X (offset=0x%X),",
		 (vm_offset_t) object->pager, (vm_offset_t) object->paging_offset);
	 printf("control=0x%X, name=0x%X\n",
	 	(vm_offset_t) object->pager_request, (vm_offset_t) object->pager_name);
	iprintf("%s%s",
	 	object->pager_ready ? " ready" : "",
	 	object->pager_created ? " created" : "");
	 printf("%s,%s ",
	 	object->pager_initialized ? "" : "uninitialized",
		object->temporary ? "temporary" : "permanent");
	 printf("%s%s,",
		object->internal ? "internal" : "external",
	 	object->can_persist ? " cacheable" : "");
	 printf("copy_strategy=%d\n", (vm_offset_t)object->copy_strategy);
	iprintf("shadow=0x%X (offset=0x%X),",
		(vm_offset_t) object->shadow, (vm_offset_t) object->shadow_offset);
	 printf("copy=0x%X\n", (vm_offset_t) object->copy);

	indent += 2;

	if (vm_object_print_pages) {
		count = 0;
		p = (vm_page_t) queue_first(&object->memq);
		while (!queue_end(&object->memq, (queue_entry_t) p)) {
			if (count == 0) iprintf("memory:=");
			else if (count == 4) {printf("\n"); iprintf(" ..."); count = 0;}
			else printf(",");
			count++;

			printf("(off=0x%X,page=0x%X)", p->offset, (vm_offset_t) p);
			p = (vm_page_t) queue_next(&p->listq);
		}
		if (count != 0)
			printf("\n");
	}
	indent -= 4;
}

#endif	/* MACH_KDB */
