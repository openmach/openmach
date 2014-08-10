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
 *	File:	vm/vm_map.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Virtual memory mapping module.
 */

#include <norma_ipc.h>

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <kern/assert.h>
#include <kern/zalloc.h>
#include <vm/vm_fault.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_port.h>

/*
 * Macros to copy a vm_map_entry. We must be careful to correctly
 * manage the wired page count. vm_map_entry_copy() creates a new
 * map entry to the same memory - the wired count in the new entry
 * must be set to zero. vm_map_entry_copy_full() creates a new
 * entry that is identical to the old entry.  This preserves the
 * wire count; it's used for map splitting and zone changing in
 * vm_map_copyout.
 */
#define vm_map_entry_copy(NEW,OLD) \
MACRO_BEGIN                                     \
                *(NEW) = *(OLD);                \
                (NEW)->is_shared = FALSE;	\
                (NEW)->needs_wakeup = FALSE;    \
                (NEW)->in_transition = FALSE;   \
                (NEW)->wired_count = 0;         \
                (NEW)->user_wired_count = 0;    \
MACRO_END

#define vm_map_entry_copy_full(NEW,OLD)        (*(NEW) = *(OLD))

/*
 *	Virtual memory maps provide for the mapping, protection,
 *	and sharing of virtual memory objects.  In addition,
 *	this module provides for an efficient virtual copy of
 *	memory from one map to another.
 *
 *	Synchronization is required prior to most operations.
 *
 *	Maps consist of an ordered doubly-linked list of simple
 *	entries; a single hint is used to speed up lookups.
 *
 *	Sharing maps have been deleted from this version of Mach.
 *	All shared objects are now mapped directly into the respective
 *	maps.  This requires a change in the copy on write strategy;
 *	the asymmetric (delayed) strategy is used for shared temporary
 *	objects instead of the symmetric (shadow) strategy.  This is
 *	selected by the (new) use_shared_copy bit in the object.  See
 *	vm_object_copy_temporary in vm_object.c for details.  All maps
 *	are now "top level" maps (either task map, kernel map or submap
 *	of the kernel map).  
 *
 *	Since portions of maps are specified by start/end addreses,
 *	which may not align with existing map entries, all
 *	routines merely "clip" entries to these start/end values.
 *	[That is, an entry is split into two, bordering at a
 *	start or end value.]  Note that these clippings may not
 *	always be necessary (as the two resulting entries are then
 *	not changed); however, the clipping is done for convenience.
 *	No attempt is currently made to "glue back together" two
 *	abutting entries.
 *
 *	The symmetric (shadow) copy strategy implements virtual copy
 *	by copying VM object references from one map to
 *	another, and then marking both regions as copy-on-write.
 *	It is important to note that only one writeable reference
 *	to a VM object region exists in any map when this strategy
 *	is used -- this means that shadow object creation can be
 *	delayed until a write operation occurs.  The asymmetric (delayed)
 *	strategy allows multiple maps to have writeable references to
 *	the same region of a vm object, and hence cannot delay creating
 *	its copy objects.  See vm_object_copy_temporary() in vm_object.c.
 *	Copying of permanent objects is completely different; see
 *	vm_object_copy_strategically() in vm_object.c.
 */

zone_t		vm_map_zone;		/* zone for vm_map structures */
zone_t		vm_map_entry_zone;	/* zone for vm_map_entry structures */
zone_t		vm_map_kentry_zone;	/* zone for kernel entry structures */
zone_t		vm_map_copy_zone;	/* zone for vm_map_copy structures */

boolean_t	vm_map_lookup_entry();	/* forward declaration */

/*
 *	Placeholder object for submap operations.  This object is dropped
 *	into the range by a call to vm_map_find, and removed when
 *	vm_map_submap creates the submap.
 */

vm_object_t	vm_submap_object;

/*
 *	vm_map_init:
 *
 *	Initialize the vm_map module.  Must be called before
 *	any other vm_map routines.
 *
 *	Map and entry structures are allocated from zones -- we must
 *	initialize those zones.
 *
 *	There are three zones of interest:
 *
 *	vm_map_zone:		used to allocate maps.
 *	vm_map_entry_zone:	used to allocate map entries.
 *	vm_map_kentry_zone:	used to allocate map entries for the kernel.
 *
 *	The kernel allocates map entries from a special zone that is initially
 *	"crammed" with memory.  It would be difficult (perhaps impossible) for
 *	the kernel to allocate more memory to a entry zone when it became
 *	empty since the very act of allocating memory implies the creation
 *	of a new entry.
 */

vm_offset_t	kentry_data;
vm_size_t	kentry_data_size;
int		kentry_count = 256;		/* to init kentry_data_size */

void vm_map_init()
{
	vm_map_zone = zinit((vm_size_t) sizeof(struct vm_map), 40*1024,
					PAGE_SIZE, 0, "maps");
	vm_map_entry_zone = zinit((vm_size_t) sizeof(struct vm_map_entry),
					1024*1024, PAGE_SIZE*5,
					0, "non-kernel map entries");
	vm_map_kentry_zone = zinit((vm_size_t) sizeof(struct vm_map_entry),
					kentry_data_size, kentry_data_size,
					ZONE_FIXED /* XXX */, "kernel map entries");

	vm_map_copy_zone = zinit((vm_size_t) sizeof(struct vm_map_copy),
					16*1024, PAGE_SIZE, 0,
					"map copies");

	/*
	 *	Cram the kentry zone with initial data.
	 */
	zcram(vm_map_kentry_zone, kentry_data, kentry_data_size);

	/*
	 *	Submap object is initialized by vm_object_init.
	 */
}

/*
 *	vm_map_create:
 *
 *	Creates and returns a new empty VM map with
 *	the given physical map structure, and having
 *	the given lower and upper address bounds.
 */
vm_map_t vm_map_create(pmap, min, max, pageable)
	pmap_t		pmap;
	vm_offset_t	min, max;
	boolean_t	pageable;
{
	register vm_map_t	result;

	result = (vm_map_t) zalloc(vm_map_zone);
	if (result == VM_MAP_NULL)
		panic("vm_map_create");

	vm_map_first_entry(result) = vm_map_to_entry(result);
	vm_map_last_entry(result)  = vm_map_to_entry(result);
	result->hdr.nentries = 0;
	result->hdr.entries_pageable = pageable;

	result->size = 0;
	result->ref_count = 1;
	result->pmap = pmap;
	result->min_offset = min;
	result->max_offset = max;
	result->wiring_required = FALSE;
	result->wait_for_space = FALSE;
	result->first_free = vm_map_to_entry(result);
	result->hint = vm_map_to_entry(result);
	vm_map_lock_init(result);
	simple_lock_init(&result->ref_lock);
	simple_lock_init(&result->hint_lock);

	return(result);
}

/*
 *	vm_map_entry_create:	[ internal use only ]
 *
 *	Allocates a VM map entry for insertion in the
 *	given map (or map copy).  No fields are filled.
 */
#define	vm_map_entry_create(map) \
	    _vm_map_entry_create(&(map)->hdr)

#define	vm_map_copy_entry_create(copy) \
	    _vm_map_entry_create(&(copy)->cpy_hdr)

vm_map_entry_t _vm_map_entry_create(map_header)
	register struct vm_map_header *map_header;
{
	register zone_t	zone;
	register vm_map_entry_t	entry;

	if (map_header->entries_pageable)
	    zone = vm_map_entry_zone;
	else
	    zone = vm_map_kentry_zone;

	entry = (vm_map_entry_t) zalloc(zone);
	if (entry == VM_MAP_ENTRY_NULL)
		panic("vm_map_entry_create");

	return(entry);
}

/*
 *	vm_map_entry_dispose:	[ internal use only ]
 *
 *	Inverse of vm_map_entry_create.
 */
#define	vm_map_entry_dispose(map, entry) \
	_vm_map_entry_dispose(&(map)->hdr, (entry))

#define	vm_map_copy_entry_dispose(map, entry) \
	_vm_map_entry_dispose(&(copy)->cpy_hdr, (entry))

void _vm_map_entry_dispose(map_header, entry)
	register struct vm_map_header *map_header;
	register vm_map_entry_t	entry;
{
	register zone_t		zone;

	if (map_header->entries_pageable)
	    zone = vm_map_entry_zone;
	else
	    zone = vm_map_kentry_zone;

	zfree(zone, (vm_offset_t) entry);
}

/*
 *	vm_map_entry_{un,}link:
 *
 *	Insert/remove entries from maps (or map copies).
 */
#define vm_map_entry_link(map, after_where, entry)	\
	_vm_map_entry_link(&(map)->hdr, after_where, entry)

#define vm_map_copy_entry_link(copy, after_where, entry)	\
	_vm_map_entry_link(&(copy)->cpy_hdr, after_where, entry)

#define _vm_map_entry_link(hdr, after_where, entry)	\
	MACRO_BEGIN					\
	(hdr)->nentries++;				\
	(entry)->vme_prev = (after_where);		\
	(entry)->vme_next = (after_where)->vme_next;	\
	(entry)->vme_prev->vme_next =			\
	 (entry)->vme_next->vme_prev = (entry);		\
	MACRO_END

#define vm_map_entry_unlink(map, entry)			\
	_vm_map_entry_unlink(&(map)->hdr, entry)

#define vm_map_copy_entry_unlink(copy, entry)			\
	_vm_map_entry_unlink(&(copy)->cpy_hdr, entry)

#define _vm_map_entry_unlink(hdr, entry)		\
	MACRO_BEGIN					\
	(hdr)->nentries--;				\
	(entry)->vme_next->vme_prev = (entry)->vme_prev; \
	(entry)->vme_prev->vme_next = (entry)->vme_next; \
	MACRO_END

/*
 *	vm_map_reference:
 *
 *	Creates another valid reference to the given map.
 *
 */
void vm_map_reference(map)
	register vm_map_t	map;
{
	if (map == VM_MAP_NULL)
		return;

	simple_lock(&map->ref_lock);
	map->ref_count++;
	simple_unlock(&map->ref_lock);
}

/*
 *	vm_map_deallocate:
 *
 *	Removes a reference from the specified map,
 *	destroying it if no references remain.
 *	The map should not be locked.
 */
void vm_map_deallocate(map)
	register vm_map_t	map;
{
	register int		c;

	if (map == VM_MAP_NULL)
		return;

	simple_lock(&map->ref_lock);
	c = --map->ref_count;
	simple_unlock(&map->ref_lock);

	if (c > 0) {
		return;
	}

	projected_buffer_collect(map);
	(void) vm_map_delete(map, map->min_offset, map->max_offset);

	pmap_destroy(map->pmap);

	zfree(vm_map_zone, (vm_offset_t) map);
}

/*
 *	SAVE_HINT:
 *
 *	Saves the specified entry as the hint for
 *	future lookups.  Performs necessary interlocks.
 */
#define	SAVE_HINT(map,value) \
		simple_lock(&(map)->hint_lock); \
		(map)->hint = (value); \
		simple_unlock(&(map)->hint_lock);

/*
 *	vm_map_lookup_entry:	[ internal use only ]
 *
 *	Finds the map entry containing (or
 *	immediately preceding) the specified address
 *	in the given map; the entry is returned
 *	in the "entry" parameter.  The boolean
 *	result indicates whether the address is
 *	actually contained in the map.
 */
boolean_t vm_map_lookup_entry(map, address, entry)
	register vm_map_t	map;
	register vm_offset_t	address;
	vm_map_entry_t		*entry;		/* OUT */
{
	register vm_map_entry_t		cur;
	register vm_map_entry_t		last;

	/*
	 *	Start looking either from the head of the
	 *	list, or from the hint.
	 */

	simple_lock(&map->hint_lock);
	cur = map->hint;
	simple_unlock(&map->hint_lock);

	if (cur == vm_map_to_entry(map))
		cur = cur->vme_next;

	if (address >= cur->vme_start) {
	    	/*
		 *	Go from hint to end of list.
		 *
		 *	But first, make a quick check to see if
		 *	we are already looking at the entry we
		 *	want (which is usually the case).
		 *	Note also that we don't need to save the hint
		 *	here... it is the same hint (unless we are
		 *	at the header, in which case the hint didn't
		 *	buy us anything anyway).
		 */
		last = vm_map_to_entry(map);
		if ((cur != last) && (cur->vme_end > address)) {
			*entry = cur;
			return(TRUE);
		}
	}
	else {
	    	/*
		 *	Go from start to hint, *inclusively*
		 */
		last = cur->vme_next;
		cur = vm_map_first_entry(map);
	}

	/*
	 *	Search linearly
	 */

	while (cur != last) {
		if (cur->vme_end > address) {
			if (address >= cur->vme_start) {
			    	/*
				 *	Save this lookup for future
				 *	hints, and return
				 */

				*entry = cur;
				SAVE_HINT(map, cur);
				return(TRUE);
			}
			break;
		}
		cur = cur->vme_next;
	}
	*entry = cur->vme_prev;
	SAVE_HINT(map, *entry);
	return(FALSE);
}

/*
 *      Routine:     invalid_user_access
 *
 *	Verifies whether user access is valid.
 */

boolean_t
invalid_user_access(map, start, end, prot)
        vm_map_t map;
        vm_offset_t start, end;
        vm_prot_t prot;
{
        vm_map_entry_t entry;

        return (map == VM_MAP_NULL || map == kernel_map ||
		!vm_map_lookup_entry(map, start, &entry) ||
		entry->vme_end < end ||
		(prot & ~(entry->protection)));
}


/*
 *	Routine:	vm_map_find_entry
 *	Purpose:
 *		Allocate a range in the specified virtual address map,
 *		returning the entry allocated for that range.
 *		Used by kmem_alloc, etc.  Returns wired entries.
 *
 *		The map must be locked.
 *
 *		If an entry is allocated, the object/offset fields
 *		are initialized to zero.  If an object is supplied,
 *		then an existing entry may be extended.
 */
kern_return_t vm_map_find_entry(map, address, size, mask, object, o_entry)
	register vm_map_t	map;
	vm_offset_t		*address;	/* OUT */
	vm_size_t		size;
	vm_offset_t		mask;
	vm_object_t		object;
	vm_map_entry_t		*o_entry;	/* OUT */
{
	register vm_map_entry_t	entry, new_entry;
	register vm_offset_t	start;
	register vm_offset_t	end;

	/*
	 *	Look for the first possible address;
	 *	if there's already something at this
	 *	address, we have to start after it.
	 */

	if ((entry = map->first_free) == vm_map_to_entry(map))
		start = map->min_offset;
	else
		start = entry->vme_end;

	/*
	 *	In any case, the "entry" always precedes
	 *	the proposed new region throughout the loop:
	 */

	while (TRUE) {
		register vm_map_entry_t	next;

		/*
		 *	Find the end of the proposed new region.
		 *	Be sure we didn't go beyond the end, or
		 *	wrap around the address.
		 */

		if (((start + mask) & ~mask) < start)
			return(KERN_NO_SPACE);
		start = ((start + mask) & ~mask);
		end = start + size;

		if ((end > map->max_offset) || (end < start))
			return(KERN_NO_SPACE);

		/*
		 *	If there are no more entries, we must win.
		 */

		next = entry->vme_next;
		if (next == vm_map_to_entry(map))
			break;

		/*
		 *	If there is another entry, it must be
		 *	after the end of the potential new region.
		 */

		if (next->vme_start >= end)
			break;

		/*
		 *	Didn't fit -- move to the next entry.
		 */

		entry = next;
		start = entry->vme_end;
	}

	/*
	 *	At this point,
	 *		"start" and "end" should define the endpoints of the
	 *			available new range, and
	 *		"entry" should refer to the region before the new
	 *			range, and
	 *
	 *		the map should be locked.
	 */

	*address = start;

	/*
	 *	See whether we can avoid creating a new entry by
	 *	extending one of our neighbors.  [So far, we only attempt to
	 *	extend from below.]
	 */

	if ((object != VM_OBJECT_NULL) &&
	    (entry != vm_map_to_entry(map)) &&
	    (entry->vme_end == start) &&
	    (!entry->is_shared) &&
	    (!entry->is_sub_map) &&
	    (entry->object.vm_object == object) &&
	    (entry->needs_copy == FALSE) &&
	    (entry->inheritance == VM_INHERIT_DEFAULT) &&
	    (entry->protection == VM_PROT_DEFAULT) &&
	    (entry->max_protection == VM_PROT_ALL) &&
	    (entry->wired_count == 1) &&
	    (entry->user_wired_count == 0) &&
	    (entry->projected_on == 0)) {
		/*
		 *	Because this is a special case,
		 *	we don't need to use vm_object_coalesce.
		 */

		entry->vme_end = end;
		new_entry = entry;
	} else {
		new_entry = vm_map_entry_create(map);

		new_entry->vme_start = start;
		new_entry->vme_end = end;

		new_entry->is_shared = FALSE;
		new_entry->is_sub_map = FALSE;
		new_entry->object.vm_object = VM_OBJECT_NULL;
		new_entry->offset = (vm_offset_t) 0;

		new_entry->needs_copy = FALSE;

		new_entry->inheritance = VM_INHERIT_DEFAULT;
		new_entry->protection = VM_PROT_DEFAULT;
		new_entry->max_protection = VM_PROT_ALL;
		new_entry->wired_count = 1;
		new_entry->user_wired_count = 0;

		new_entry->in_transition = FALSE;
		new_entry->needs_wakeup = FALSE;
		new_entry->projected_on = 0;

		/*
		 *	Insert the new entry into the list
		 */

		vm_map_entry_link(map, entry, new_entry);
    	}

	map->size += size;

	/*
	 *	Update the free space hint and the lookup hint
	 */

	map->first_free = new_entry;
	SAVE_HINT(map, new_entry);

	*o_entry = new_entry;
	return(KERN_SUCCESS);
}

int vm_map_pmap_enter_print = FALSE;
int vm_map_pmap_enter_enable = FALSE;

/*
 *	Routine:	vm_map_pmap_enter
 *
 *	Description:
 *		Force pages from the specified object to be entered into
 *		the pmap at the specified address if they are present.
 *		As soon as a page not found in the object the scan ends.
 *
 *	Returns:
 *		Nothing.  
 *
 *	In/out conditions:
 *		The source map should not be locked on entry.
 */
void
vm_map_pmap_enter(map, addr, end_addr, object, offset, protection)
	vm_map_t	map;
	register
	vm_offset_t 	addr;
	register
	vm_offset_t	end_addr;
	register
	vm_object_t 	object;
	vm_offset_t	offset;
	vm_prot_t	protection;
{
	while (addr < end_addr) {
		register vm_page_t	m;

		vm_object_lock(object);
		vm_object_paging_begin(object);

		m = vm_page_lookup(object, offset);
		if (m == VM_PAGE_NULL || m->absent) {
			vm_object_paging_end(object);
			vm_object_unlock(object);
			return;
		}

		if (vm_map_pmap_enter_print) {
			printf("vm_map_pmap_enter:");
			printf("map: %x, addr: %x, object: %x, offset: %x\n",
				map, addr, object, offset);
		}

		m->busy = TRUE;
		vm_object_unlock(object);

		PMAP_ENTER(map->pmap, addr, m,
			   protection, FALSE);

		vm_object_lock(object);
		PAGE_WAKEUP_DONE(m);
		vm_page_lock_queues();
		if (!m->active && !m->inactive)
		    vm_page_activate(m);
		vm_page_unlock_queues();
		vm_object_paging_end(object);
		vm_object_unlock(object);

		offset += PAGE_SIZE;
		addr += PAGE_SIZE;
	}
}

/*
 *	Routine:	vm_map_enter
 *
 *	Description:
 *		Allocate a range in the specified virtual address map.
 *		The resulting range will refer to memory defined by
 *		the given memory object and offset into that object.
 *
 *		Arguments are as defined in the vm_map call.
 */
kern_return_t vm_map_enter(
		map,
		address, size, mask, anywhere,
		object, offset, needs_copy,
		cur_protection, max_protection,	inheritance)
	register
	vm_map_t	map;
	vm_offset_t	*address;	/* IN/OUT */
	vm_size_t	size;
	vm_offset_t	mask;
	boolean_t	anywhere;
	vm_object_t	object;
	vm_offset_t	offset;
	boolean_t	needs_copy;
	vm_prot_t	cur_protection;
	vm_prot_t	max_protection;
	vm_inherit_t	inheritance;
{
	register vm_map_entry_t	entry;
	register vm_offset_t	start;
	register vm_offset_t	end;
	kern_return_t		result = KERN_SUCCESS;

#define	RETURN(value)	{ result = value; goto BailOut; }

 StartAgain: ;

	start = *address;

	if (anywhere) {
		vm_map_lock(map);

		/*
		 *	Calculate the first possible address.
		 */

		if (start < map->min_offset)
			start = map->min_offset;
		if (start > map->max_offset)
			RETURN(KERN_NO_SPACE);

		/*
		 *	Look for the first possible address;
		 *	if there's already something at this
		 *	address, we have to start after it.
		 */

		if (start == map->min_offset) {
			if ((entry = map->first_free) != vm_map_to_entry(map))
				start = entry->vme_end;
		} else {
			vm_map_entry_t	tmp_entry;
			if (vm_map_lookup_entry(map, start, &tmp_entry))
				start = tmp_entry->vme_end;
			entry = tmp_entry;
		}

		/*
		 *	In any case, the "entry" always precedes
		 *	the proposed new region throughout the
		 *	loop:
		 */

		while (TRUE) {
			register vm_map_entry_t	next;

		    	/*
			 *	Find the end of the proposed new region.
			 *	Be sure we didn't go beyond the end, or
			 *	wrap around the address.
			 */

			if (((start + mask) & ~mask) < start)
				return(KERN_NO_SPACE);
			start = ((start + mask) & ~mask);
			end = start + size;

			if ((end > map->max_offset) || (end < start)) {
				if (map->wait_for_space) {
					if (size <= (map->max_offset -
						     map->min_offset)) {
						assert_wait((event_t) map, TRUE);
						vm_map_unlock(map);
						thread_block((void (*)()) 0);
						goto StartAgain;
					}
				}
				
				RETURN(KERN_NO_SPACE);
			}

			/*
			 *	If there are no more entries, we must win.
			 */

			next = entry->vme_next;
			if (next == vm_map_to_entry(map))
				break;

			/*
			 *	If there is another entry, it must be
			 *	after the end of the potential new region.
			 */

			if (next->vme_start >= end)
				break;

			/*
			 *	Didn't fit -- move to the next entry.
			 */

			entry = next;
			start = entry->vme_end;
		}
		*address = start;
	} else {
		vm_map_entry_t		temp_entry;

		/*
		 *	Verify that:
		 *		the address doesn't itself violate
		 *		the mask requirement.
		 */

		if ((start & mask) != 0)
			return(KERN_NO_SPACE);

		vm_map_lock(map);

		/*
		 *	...	the address is within bounds
		 */

		end = start + size;

		if ((start < map->min_offset) ||
		    (end > map->max_offset) ||
		    (start >= end)) {
			RETURN(KERN_INVALID_ADDRESS);
		}

		/*
		 *	...	the starting address isn't allocated
		 */

		if (vm_map_lookup_entry(map, start, &temp_entry))
			RETURN(KERN_NO_SPACE);

		entry = temp_entry;

		/*
		 *	...	the next region doesn't overlap the
		 *		end point.
		 */

		if ((entry->vme_next != vm_map_to_entry(map)) &&
		    (entry->vme_next->vme_start < end))
			RETURN(KERN_NO_SPACE);
	}

	/*
	 *	At this point,
	 *		"start" and "end" should define the endpoints of the
	 *			available new range, and
	 *		"entry" should refer to the region before the new
	 *			range, and
	 *
	 *		the map should be locked.
	 */

	/*
	 *	See whether we can avoid creating a new entry (and object) by
	 *	extending one of our neighbors.  [So far, we only attempt to
	 *	extend from below.]
	 */

	if ((object == VM_OBJECT_NULL) &&
	    (entry != vm_map_to_entry(map)) &&
	    (entry->vme_end == start) &&
	    (!entry->is_shared) &&
	    (!entry->is_sub_map) &&
	    (entry->inheritance == inheritance) &&
	    (entry->protection == cur_protection) &&
	    (entry->max_protection == max_protection) &&
	    (entry->wired_count == 0) &&  /* implies user_wired_count == 0 */
	    (entry->projected_on == 0)) { 
		if (vm_object_coalesce(entry->object.vm_object,
				VM_OBJECT_NULL,
				entry->offset,
				(vm_offset_t) 0,
				(vm_size_t)(entry->vme_end - entry->vme_start),
				(vm_size_t)(end - entry->vme_end))) {

			/*
			 *	Coalesced the two objects - can extend
			 *	the previous map entry to include the
			 *	new range.
			 */
			map->size += (end - entry->vme_end);
			entry->vme_end = end;
			RETURN(KERN_SUCCESS);
		}
	}

	/*
	 *	Create a new entry
	 */

	/**/ {
	register vm_map_entry_t	new_entry;

	new_entry = vm_map_entry_create(map);

	new_entry->vme_start = start;
	new_entry->vme_end = end;

	new_entry->is_shared = FALSE;
	new_entry->is_sub_map = FALSE;
	new_entry->object.vm_object = object;
	new_entry->offset = offset;

	new_entry->needs_copy = needs_copy;

	new_entry->inheritance = inheritance;
	new_entry->protection = cur_protection;
	new_entry->max_protection = max_protection;
	new_entry->wired_count = 0;
	new_entry->user_wired_count = 0;

	new_entry->in_transition = FALSE;
	new_entry->needs_wakeup = FALSE;
	new_entry->projected_on = 0;

	/*
	 *	Insert the new entry into the list
	 */

	vm_map_entry_link(map, entry, new_entry);
	map->size += size;

	/*
	 *	Update the free space hint and the lookup hint
	 */

	if ((map->first_free == entry) &&
	    ((entry == vm_map_to_entry(map) ? map->min_offset : entry->vme_end)
	     >= new_entry->vme_start))
		map->first_free = new_entry;

	SAVE_HINT(map, new_entry);

	vm_map_unlock(map);

	if ((object != VM_OBJECT_NULL) &&
	    (vm_map_pmap_enter_enable) &&
	    (!anywhere)	 &&
	    (!needs_copy) && 
	    (size < (128*1024))) {
		vm_map_pmap_enter(map, start, end, 
				  object, offset, cur_protection);
	}

	return(result);
	/**/ }

 BailOut: ;

	vm_map_unlock(map);
	return(result);

#undef	RETURN
}

/*
 *	vm_map_clip_start:	[ internal use only ]
 *
 *	Asserts that the given entry begins at or after
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
void _vm_map_clip_start();
#define vm_map_clip_start(map, entry, startaddr) \
	MACRO_BEGIN \
	if ((startaddr) > (entry)->vme_start) \
		_vm_map_clip_start(&(map)->hdr,(entry),(startaddr)); \
	MACRO_END

void _vm_map_copy_clip_start();
#define vm_map_copy_clip_start(copy, entry, startaddr) \
	MACRO_BEGIN \
	if ((startaddr) > (entry)->vme_start) \
		_vm_map_clip_start(&(copy)->cpy_hdr,(entry),(startaddr)); \
	MACRO_END

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
void _vm_map_clip_start(map_header, entry, start)
	register struct vm_map_header *map_header;
	register vm_map_entry_t	entry;
	register vm_offset_t	start;
{
	register vm_map_entry_t	new_entry;

	/*
	 *	Split off the front portion --
	 *	note that we must insert the new
	 *	entry BEFORE this one, so that
	 *	this entry has the specified starting
	 *	address.
	 */

	new_entry = _vm_map_entry_create(map_header);
	vm_map_entry_copy_full(new_entry, entry);

	new_entry->vme_end = start;
	entry->offset += (start - entry->vme_start);
	entry->vme_start = start;

	_vm_map_entry_link(map_header, entry->vme_prev, new_entry);

	if (entry->is_sub_map)
	 	vm_map_reference(new_entry->object.sub_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/*
 *	vm_map_clip_end:	[ internal use only ]
 *
 *	Asserts that the given entry ends at or before
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
void _vm_map_clip_end();
#define vm_map_clip_end(map, entry, endaddr) \
	MACRO_BEGIN \
	if ((endaddr) < (entry)->vme_end) \
		_vm_map_clip_end(&(map)->hdr,(entry),(endaddr)); \
	MACRO_END

void _vm_map_copy_clip_end();
#define vm_map_copy_clip_end(copy, entry, endaddr) \
	MACRO_BEGIN \
	if ((endaddr) < (entry)->vme_end) \
		_vm_map_clip_end(&(copy)->cpy_hdr,(entry),(endaddr)); \
	MACRO_END

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
void _vm_map_clip_end(map_header, entry, end)
	register struct vm_map_header *map_header;
	register vm_map_entry_t	entry;
	register vm_offset_t	end;
{
	register vm_map_entry_t	new_entry;

	/*
	 *	Create a new entry and insert it
	 *	AFTER the specified entry
	 */

	new_entry = _vm_map_entry_create(map_header);
	vm_map_entry_copy_full(new_entry, entry);

	new_entry->vme_start = entry->vme_end = end;
	new_entry->offset += (end - entry->vme_start);

	_vm_map_entry_link(map_header, entry, new_entry);

	if (entry->is_sub_map)
	 	vm_map_reference(new_entry->object.sub_map);
	else
		vm_object_reference(new_entry->object.vm_object);
}

/*
 *	VM_MAP_RANGE_CHECK:	[ internal use only ]
 *
 *	Asserts that the starting and ending region
 *	addresses fall within the valid range of the map.
 */
#define	VM_MAP_RANGE_CHECK(map, start, end)		\
		{					\
		if (start < vm_map_min(map))		\
			start = vm_map_min(map);	\
		if (end > vm_map_max(map))		\
			end = vm_map_max(map);		\
		if (start > end)			\
			start = end;			\
		}

/*
 *	vm_map_submap:		[ kernel use only ]
 *
 *	Mark the given range as handled by a subordinate map.
 *
 *	This range must have been created with vm_map_find using
 *	the vm_submap_object, and no other operations may have been
 *	performed on this range prior to calling vm_map_submap.
 *
 *	Only a limited number of operations can be performed
 *	within this rage after calling vm_map_submap:
 *		vm_fault
 *	[Don't try vm_map_copyin!]
 *
 *	To remove a submapping, one must first remove the
 *	range from the superior map, and then destroy the
 *	submap (if desired).  [Better yet, don't try it.]
 */
kern_return_t vm_map_submap(map, start, end, submap)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
	vm_map_t		submap;
{
	vm_map_entry_t		entry;
	register kern_return_t	result = KERN_INVALID_ARGUMENT;
	register vm_object_t	object;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	}
	 else
		entry = entry->vme_next;

	vm_map_clip_end(map, entry, end);

	if ((entry->vme_start == start) && (entry->vme_end == end) &&
	    (!entry->is_sub_map) &&
	    ((object = entry->object.vm_object) == vm_submap_object) &&
	    (object->resident_page_count == 0) &&
	    (object->copy == VM_OBJECT_NULL) &&
	    (object->shadow == VM_OBJECT_NULL) &&
	    (!object->pager_created)) {
		entry->object.vm_object = VM_OBJECT_NULL;
		vm_object_deallocate(object);
		entry->is_sub_map = TRUE;
		vm_map_reference(entry->object.sub_map = submap);
		result = KERN_SUCCESS;
	}
	vm_map_unlock(map);

	return(result);
}

/*
 *	vm_map_protect:
 *
 *	Sets the protection of the specified address
 *	region in the target map.  If "set_max" is
 *	specified, the maximum protection is to be set;
 *	otherwise, only the current protection is affected.
 */
kern_return_t vm_map_protect(map, start, end, new_prot, set_max)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
	register vm_prot_t	new_prot;
	register boolean_t	set_max;
{
	register vm_map_entry_t		current;
	vm_map_entry_t			entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &entry)) {
		vm_map_clip_start(map, entry, start);
	}
	 else
		entry = entry->vme_next;

	/*
	 *	Make a first pass to check for protection
	 *	violations.
	 */

	current = entry;
	while ((current != vm_map_to_entry(map)) &&
	       (current->vme_start < end)) {

		if (current->is_sub_map) {
			vm_map_unlock(map);
			return(KERN_INVALID_ARGUMENT);
		}
		if ((new_prot & (VM_PROT_NOTIFY | current->max_protection))
		    != new_prot) {
		       vm_map_unlock(map);
		       return(KERN_PROTECTION_FAILURE);
		}

		current = current->vme_next;
	}

	/*
	 *	Go back and fix up protections.
	 *	[Note that clipping is not necessary the second time.]
	 */

	current = entry;

	while ((current != vm_map_to_entry(map)) &&
	       (current->vme_start < end)) {

		vm_prot_t	old_prot;

		vm_map_clip_end(map, current, end);

		old_prot = current->protection;
		if (set_max)
			current->protection =
				(current->max_protection = new_prot) &
					old_prot;
		else
			current->protection = new_prot;

		/*
		 *	Update physical map if necessary.
		 */

		if (current->protection != old_prot) {
			pmap_protect(map->pmap, current->vme_start,
					current->vme_end,
					current->protection);
		}
		current = current->vme_next;
	}

	vm_map_unlock(map);
	return(KERN_SUCCESS);
}

/*
 *	vm_map_inherit:
 *
 *	Sets the inheritance of the specified address
 *	range in the target map.  Inheritance
 *	affects how the map will be shared with
 *	child maps at the time of vm_map_fork.
 */
kern_return_t vm_map_inherit(map, start, end, new_inheritance)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
	register vm_inherit_t	new_inheritance;
{
	register vm_map_entry_t	entry;
	vm_map_entry_t	temp_entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &temp_entry)) {
		entry = temp_entry;
		vm_map_clip_start(map, entry, start);
	}
	else
		entry = temp_entry->vme_next;

	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		vm_map_clip_end(map, entry, end);

		entry->inheritance = new_inheritance;

		entry = entry->vme_next;
	}

	vm_map_unlock(map);
	return(KERN_SUCCESS);
}

/*
 *	vm_map_pageable_common:
 *
 *	Sets the pageability of the specified address
 *	range in the target map.  Regions specified
 *	as not pageable require locked-down physical
 *	memory and physical page maps.  access_type indicates
 *	types of accesses that must not generate page faults.
 *	This is checked against protection of memory being locked-down.
 *	access_type of VM_PROT_NONE makes memory pageable.
 *
 *	The map must not be locked, but a reference
 *	must remain to the map throughout the call.
 *
 *	Callers should use macros in vm/vm_map.h (i.e. vm_map_pageable,
 *	or vm_map_pageable_user); don't call vm_map_pageable directly.
 */
kern_return_t vm_map_pageable_common(map, start, end, access_type, user_wire)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
	register vm_prot_t	access_type;
	boolean_t		user_wire;
{
	register vm_map_entry_t	entry;
	vm_map_entry_t		start_entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (vm_map_lookup_entry(map, start, &start_entry)) {
		entry = start_entry;
		/*
		 *	vm_map_clip_start will be done later.
		 */
	}
	else {
		/*
		 *	Start address is not in map; this is fatal.
		 */
		vm_map_unlock(map);
		return(KERN_FAILURE);
	}

	/*
	 *	Actions are rather different for wiring and unwiring,
	 *	so we have two separate cases.
	 */

	if (access_type == VM_PROT_NONE) {

		vm_map_clip_start(map, entry, start);

		/*
		 *	Unwiring.  First ensure that the range to be
		 *	unwired is really wired down.
		 */
		while ((entry != vm_map_to_entry(map)) &&
		       (entry->vme_start < end)) {

		    if ((entry->wired_count == 0) ||
		    	((entry->vme_end < end) && 
			 ((entry->vme_next == vm_map_to_entry(map)) ||
			  (entry->vme_next->vme_start > entry->vme_end))) ||
			(user_wire && (entry->user_wired_count == 0))) {
			    vm_map_unlock(map);
			    return(KERN_INVALID_ARGUMENT);
		    }
		    entry = entry->vme_next;
		}

		/*
		 *	Now decrement the wiring count for each region.
		 *	If a region becomes completely unwired,
		 *	unwire its physical pages and mappings.
		 */
		entry = start_entry;
		while ((entry != vm_map_to_entry(map)) &&
		       (entry->vme_start < end)) {
		    vm_map_clip_end(map, entry, end);

		    if (user_wire) {
			if (--(entry->user_wired_count) == 0)
			    entry->wired_count--;
		    }
		    else {
			entry->wired_count--;
		    }
		    
		    if (entry->wired_count == 0)
			vm_fault_unwire(map, entry);

		    entry = entry->vme_next;
		}
	}

	else {
		/*
		 *	Wiring.  We must do this in two passes:
		 *
		 *	1.  Holding the write lock, we create any shadow
		 *	    or zero-fill objects that need to be created.
		 *	    Then we clip each map entry to the region to be
		 *	    wired and increment its wiring count.  We
		 *	    create objects before clipping the map entries
		 *	    to avoid object proliferation.
		 *
		 *	2.  We downgrade to a read lock, and call
		 *	    vm_fault_wire to fault in the pages for any
		 *	    newly wired area (wired_count is 1).
		 *
		 *	Downgrading to a read lock for vm_fault_wire avoids
		 *	a possible deadlock with another thread that may have
		 *	faulted on one of the pages to be wired (it would mark
		 *	the page busy, blocking us, then in turn block on the
		 *	map lock that we hold).  Because of problems in the
		 *	recursive lock package, we cannot upgrade to a write
		 *	lock in vm_map_lookup.  Thus, any actions that require
		 *	the write lock must be done beforehand.  Because we
		 *	keep the read lock on the map, the copy-on-write
		 *	status of the entries we modify here cannot change.
		 */

		/*
		 *	Pass 1.
		 */
		while ((entry != vm_map_to_entry(map)) &&
		       (entry->vme_start < end)) {
		    vm_map_clip_end(map, entry, end);

		    if (entry->wired_count == 0) {

			/*
			 *	Perform actions of vm_map_lookup that need
			 *	the write lock on the map: create a shadow
			 *	object for a copy-on-write region, or an
			 *	object for a zero-fill region.
			 */
			if (entry->needs_copy &&
			    ((entry->protection & VM_PROT_WRITE) != 0)) {

				vm_object_shadow(&entry->object.vm_object,
						&entry->offset,
						(vm_size_t)(entry->vme_end
							- entry->vme_start));
				entry->needs_copy = FALSE;
			}
			if (entry->object.vm_object == VM_OBJECT_NULL) {
				entry->object.vm_object =
				        vm_object_allocate(
					    (vm_size_t)(entry->vme_end 
				    			- entry->vme_start));
				entry->offset = (vm_offset_t)0;
			}
		    }
		    vm_map_clip_start(map, entry, start);
		    vm_map_clip_end(map, entry, end);

		    if (user_wire) {
			if ((entry->user_wired_count)++ == 0)
			    entry->wired_count++;
		    }
		    else {
			entry->wired_count++;
		    }

		    /*
		     *	Check for holes and protection mismatch.
		     *  Holes: Next entry should be contiguous unless
		     *		this is the end of the region.
		     *	Protection: Access requested must be allowed.
		     */
		    if (((entry->vme_end < end) && 
			 ((entry->vme_next == vm_map_to_entry(map)) ||
			  (entry->vme_next->vme_start > entry->vme_end))) ||
			((entry->protection & access_type) != access_type)) {
			    /*
			     *	Found a hole or protection problem.
			     *	Object creation actions
			     *	do not need to be undone, but the
			     *	wired counts need to be restored.
			     */
			    while ((entry != vm_map_to_entry(map)) &&
				(entry->vme_end > start)) {
				    if (user_wire) {
					if (--(entry->user_wired_count) == 0)
					    entry->wired_count--;
				    }
				    else {
				       entry->wired_count--;
				    }

				    entry = entry->vme_prev;
			    }

			    vm_map_unlock(map);
			    return(KERN_FAILURE);
		    }
		    entry = entry->vme_next;
		}

		/*
		 *	Pass 2.
		 */

		/*
		 * HACK HACK HACK HACK
		 *
		 * If we are wiring in the kernel map or a submap of it,
		 * unlock the map to avoid deadlocks.  We trust that the
		 * kernel threads are well-behaved, and therefore will
		 * not do anything destructive to this region of the map
		 * while we have it unlocked.  We cannot trust user threads
		 * to do the same.
		 *
		 * HACK HACK HACK HACK
		 */
		if (vm_map_pmap(map) == kernel_pmap) {
		    vm_map_unlock(map);		/* trust me ... */
		}
		else {
		    vm_map_lock_set_recursive(map);
		    vm_map_lock_write_to_read(map);
		}

		entry = start_entry;
		while (entry != vm_map_to_entry(map) &&
			entry->vme_start < end) {
		    /*
		     *	Wiring cases:
		     *	    Kernel: wired == 1 && user_wired == 0
		     *	    User:   wired == 1 && user_wired == 1
		     *
		     *  Don't need to wire if either is > 1.  wired = 0 &&
		     *	user_wired == 1 can't happen.
		     */

		    /*
		     *	XXX This assumes that the faults always succeed.
		     */
		    if ((entry->wired_count == 1) &&
			(entry->user_wired_count <= 1)) {
			    vm_fault_wire(map, entry);
		    }
		    entry = entry->vme_next;
		}

		if (vm_map_pmap(map) == kernel_pmap) {
		    vm_map_lock(map);
		}
		else {
		    vm_map_lock_clear_recursive(map);
		}
	}

	vm_map_unlock(map);

	return(KERN_SUCCESS);
}

/*
 *	vm_map_entry_delete:	[ internal use only ]
 *
 *	Deallocate the given entry from the target map.
 */		
void vm_map_entry_delete(map, entry)
	register vm_map_t	map;
	register vm_map_entry_t	entry;
{
	register vm_offset_t	s, e;
	register vm_object_t	object;
	extern vm_object_t	kernel_object;

	s = entry->vme_start;
	e = entry->vme_end;

	/*Check if projected buffer*/
	if (map != kernel_map && entry->projected_on != 0) {
	  /*Check if projected kernel entry is persistent;
	    may only manipulate directly if it is*/
	  if (entry->projected_on->projected_on == 0)
	    entry->wired_count = 0;    /*Avoid unwire fault*/
	  else
	    return;
	}

	/*
	 *	Get the object.    Null objects cannot have pmap entries.
	 */

	if ((object = entry->object.vm_object) != VM_OBJECT_NULL) {

	    /*
	     *	Unwire before removing addresses from the pmap;
	     *	otherwise, unwiring will put the entries back in
	     *	the pmap.
	     */

	    if (entry->wired_count != 0) {
		vm_fault_unwire(map, entry);
		entry->wired_count = 0;
		entry->user_wired_count = 0;
	    }

	    /*
	     *	If the object is shared, we must remove
	     *	*all* references to this data, since we can't
	     *	find all of the physical maps which are sharing
	     *	it.
	     */

	    if (object == kernel_object) {
		vm_object_lock(object);
		vm_object_page_remove(object, entry->offset,
				entry->offset + (e - s));
		vm_object_unlock(object);
	    } else if (entry->is_shared) {
		vm_object_pmap_remove(object,
				 entry->offset,
				 entry->offset + (e - s));
	    }
	    else {
		pmap_remove(map->pmap, s, e);
	    }
        }

	/*
	 *	Deallocate the object only after removing all
	 *	pmap entries pointing to its pages.
	 */

	if (entry->is_sub_map)
		vm_map_deallocate(entry->object.sub_map);
	else
	 	vm_object_deallocate(entry->object.vm_object);

	vm_map_entry_unlink(map, entry);
	map->size -= e - s;

	vm_map_entry_dispose(map, entry);
}

/*
 *	vm_map_delete:	[ internal use only ]
 *
 *	Deallocates the given address range from the target
 *	map.
 */

kern_return_t vm_map_delete(map, start, end)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	vm_map_entry_t		entry;
	vm_map_entry_t		first_entry;

	/*
	 *	Find the start of the region, and clip it
	 */

	if (!vm_map_lookup_entry(map, start, &first_entry))
		entry = first_entry->vme_next;
	else {
		entry = first_entry;
#if	NORMA_IPC_xxx
		/*
		 * XXX Had to disable this code because:

		 _vm_map_delete(c0804b78,c2198000,c219a000,0,c219a000)+df
		 	[vm/vm_map.c:2007]
		 _vm_map_remove(c0804b78,c2198000,c219a000,c0817834,
		 c081786c)+42 [vm/vm_map.c:2094]
		 _kmem_io_map_deallocate(c0804b78,c2198000,2000,c0817834,
		 c081786c)+43 [vm/vm_kern.c:818]
		 _device_write_dealloc(c081786c)+117 [device/ds_routines.c:814]
		 _ds_write_done(c081786c,0)+2e [device/ds_routines.c:848]
		 _io_done_thread_continue(c08150c0,c21d4e14,c21d4e30,c08150c0,
		 c080c114)+14 [device/ds_routines.c:1350]

		 */
		if (start > entry->vme_start
		    && end == entry->vme_end
		    && ! entry->wired_count	/* XXX ??? */
		    && ! entry->is_shared
                    && ! entry->projected_on
		    && ! entry->is_sub_map) {
			extern vm_object_t kernel_object;
			register vm_object_t object = entry->object.vm_object;

			/*
			 * The region to be deleted lives at the end
			 * of this entry, and thus all we have to do is
			 * truncate the entry.
			 *
			 * This special case is necessary if we want
			 * coalescing to do us any good.
			 *
			 * XXX Do we have to adjust object size?
			 */
			if (object == kernel_object) {
				vm_object_lock(object);
				vm_object_page_remove(object,
						      entry->offset + start,
						      entry->offset +
						      (end - start));
				vm_object_unlock(object);
			} else if (entry->is_shared) {
				vm_object_pmap_remove(object,
						      entry->offset + start,
						      entry->offset +
						      (end - start));
			} else {
				pmap_remove(map->pmap, start, end);
			}
			object->size -= (end - start);	/* XXX */

			entry->vme_end = start;
			map->size -= (end - start);

			if (map->wait_for_space) {
				thread_wakeup((event_t) map);
			}
			return KERN_SUCCESS;
		}
#endif	NORMA_IPC
		vm_map_clip_start(map, entry, start);

		/*
		 *	Fix the lookup hint now, rather than each
		 *	time though the loop.
		 */

		SAVE_HINT(map, entry->vme_prev);
	}

	/*
	 *	Save the free space hint
	 */

	if (map->first_free->vme_start >= start)
		map->first_free = entry->vme_prev;

	/*
	 *	Step through all entries in this region
	 */

	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		vm_map_entry_t		next;

		vm_map_clip_end(map, entry, end);

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  It could be clipped
		 *	while we leave the map unlocked.
		 */
                if(entry->in_transition) {
                        /*
                         * Say that we are waiting, and wait for entry.
                         */
                        entry->needs_wakeup = TRUE;
                        vm_map_entry_wait(map, FALSE);
                        vm_map_lock(map);

                        /*
                         * The entry could have been clipped or it
                         * may not exist anymore.  look it up again.
                         */
                        if(!vm_map_lookup_entry(map, start, &entry)) {
				entry = entry->vme_next;
			}
			continue;
		}

		next = entry->vme_next;

		vm_map_entry_delete(map, entry);
		entry = next;
	}

	if (map->wait_for_space)
		thread_wakeup((event_t) map);

	return(KERN_SUCCESS);
}

/*
 *	vm_map_remove:
 *
 *	Remove the given address range from the target map.
 *	This is the exported form of vm_map_delete.
 */
kern_return_t vm_map_remove(map, start, end)
	register vm_map_t	map;
	register vm_offset_t	start;
	register vm_offset_t	end;
{
	register kern_return_t	result;

	vm_map_lock(map);
	VM_MAP_RANGE_CHECK(map, start, end);
	result = vm_map_delete(map, start, end);
	vm_map_unlock(map);

	return(result);
}


/*
 *	vm_map_copy_steal_pages:
 *
 *	Steal all the pages from a vm_map_copy page_list by copying ones
 *	that have not already been stolen.
 */
void
vm_map_copy_steal_pages(copy)
vm_map_copy_t	copy;
{
	register vm_page_t	m, new_m;
	register int		i;
	vm_object_t		object;

	for (i = 0; i < copy->cpy_npages; i++) {

		/*
		 *	If the page is not tabled, then it's already stolen.
		 */
		m = copy->cpy_page_list[i];
		if (!m->tabled)
			continue;

		/*
		 *	Page was not stolen,  get a new
		 *	one and do the copy now.
		 */
		while ((new_m = vm_page_grab()) == VM_PAGE_NULL) {
			VM_PAGE_WAIT((void(*)()) 0);
		}

		vm_page_copy(m, new_m);

		object = m->object;
		vm_object_lock(object);
		vm_page_lock_queues();
		if (!m->active && !m->inactive)
			vm_page_activate(m);
		vm_page_unlock_queues();
		PAGE_WAKEUP_DONE(m);
		vm_object_paging_end(object);
		vm_object_unlock(object);

		copy->cpy_page_list[i] = new_m;
	}
}

/*
 *	vm_map_copy_page_discard:
 *
 *	Get rid of the pages in a page_list copy.  If the pages are
 *	stolen, they are freed.  If the pages are not stolen, they
 *	are unbusied, and associated state is cleaned up.
 */
void vm_map_copy_page_discard(copy)
vm_map_copy_t	copy;
{
	while (copy->cpy_npages > 0) {
		vm_page_t	m;

		if((m = copy->cpy_page_list[--(copy->cpy_npages)]) !=
		    VM_PAGE_NULL) {

			/*
			 *	If it's not in the table, then it's
			 *	a stolen page that goes back
			 *	to the free list.  Else it belongs
			 *	to some object, and we hold a
			 *	paging reference on that object.
			 */
			if (!m->tabled) {
				VM_PAGE_FREE(m);
			}
			else {
				vm_object_t	object;

				object = m->object;

				vm_object_lock(object);
				vm_page_lock_queues();
				if (!m->active && !m->inactive)
					vm_page_activate(m);
				vm_page_unlock_queues();

				PAGE_WAKEUP_DONE(m);
				vm_object_paging_end(object);
				vm_object_unlock(object);
			}
		}
	}
}

/*
 *	Routine:	vm_map_copy_discard
 *
 *	Description:
 *		Dispose of a map copy object (returned by
 *		vm_map_copyin).
 */
void
vm_map_copy_discard(copy)
	vm_map_copy_t	copy;
{
free_next_copy:
	if (copy == VM_MAP_COPY_NULL)
		return;

	switch (copy->type) {
	case VM_MAP_COPY_ENTRY_LIST:
		while (vm_map_copy_first_entry(copy) !=
					vm_map_copy_to_entry(copy)) {
			vm_map_entry_t	entry = vm_map_copy_first_entry(copy);

			vm_map_copy_entry_unlink(copy, entry);
			vm_object_deallocate(entry->object.vm_object);
			vm_map_copy_entry_dispose(copy, entry);
		}
		break;
        case VM_MAP_COPY_OBJECT:
		vm_object_deallocate(copy->cpy_object);
		break;
	case VM_MAP_COPY_PAGE_LIST:

		/*
		 *	To clean this up, we have to unbusy all the pages
		 *	and release the paging references in their objects.
		 */
		if (copy->cpy_npages > 0)
			vm_map_copy_page_discard(copy);

		/*
		 *	If there's a continuation, abort it.  The
		 *	abort routine releases any storage.
		 */
		if (vm_map_copy_has_cont(copy)) {

			/*
			 *	Special case: recognize
			 *	vm_map_copy_discard_cont and optimize
			 *	here to avoid tail recursion.
			 */
			if (copy->cpy_cont == vm_map_copy_discard_cont) {
				register vm_map_copy_t	new_copy;

				new_copy = (vm_map_copy_t) copy->cpy_cont_args;
				zfree(vm_map_copy_zone, (vm_offset_t) copy);
				copy = new_copy;
				goto free_next_copy;
			}
			else {
				vm_map_copy_abort_cont(copy);
			}
		}

		break;
	}
	zfree(vm_map_copy_zone, (vm_offset_t) copy);
}

/*
 *	Routine:	vm_map_copy_copy
 *
 *	Description:
 *			Move the information in a map copy object to
 *			a new map copy object, leaving the old one
 *			empty.
 *
 *			This is used by kernel routines that need
 *			to look at out-of-line data (in copyin form)
 *			before deciding whether to return SUCCESS.
 *			If the routine returns FAILURE, the original
 *			copy object will be deallocated; therefore,
 *			these routines must make a copy of the copy
 *			object and leave the original empty so that
 *			deallocation will not fail.
 */
vm_map_copy_t
vm_map_copy_copy(copy)
	vm_map_copy_t	copy;
{
	vm_map_copy_t	new_copy;

	if (copy == VM_MAP_COPY_NULL)
		return VM_MAP_COPY_NULL;

	/*
	 * Allocate a new copy object, and copy the information
	 * from the old one into it.
	 */

	new_copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	*new_copy = *copy;

	if (copy->type == VM_MAP_COPY_ENTRY_LIST) {
		/*
		 * The links in the entry chain must be
		 * changed to point to the new copy object.
		 */
		vm_map_copy_first_entry(copy)->vme_prev
			= vm_map_copy_to_entry(new_copy);
		vm_map_copy_last_entry(copy)->vme_next
			= vm_map_copy_to_entry(new_copy);
	}

	/*
	 * Change the old copy object into one that contains
	 * nothing to be deallocated.
	 */
	copy->type = VM_MAP_COPY_OBJECT;
	copy->cpy_object = VM_OBJECT_NULL;

	/*
	 * Return the new object.
	 */
	return new_copy;
}

/*
 *	Routine:	vm_map_copy_discard_cont
 *
 *	Description:
 *		A version of vm_map_copy_discard that can be called
 *		as a continuation from a vm_map_copy page list.
 */
kern_return_t	vm_map_copy_discard_cont(cont_args, copy_result)
vm_map_copyin_args_t	cont_args;
vm_map_copy_t		*copy_result;	/* OUT */
{
	vm_map_copy_discard((vm_map_copy_t) cont_args);
	if (copy_result != (vm_map_copy_t *)0)
		*copy_result = VM_MAP_COPY_NULL;
	return(KERN_SUCCESS);
}

/*
 *	Routine:	vm_map_copy_overwrite
 *
 *	Description:
 *		Copy the memory described by the map copy
 *		object (copy; returned by vm_map_copyin) onto
 *		the specified destination region (dst_map, dst_addr).
 *		The destination must be writeable.
 *
 *		Unlike vm_map_copyout, this routine actually
 *		writes over previously-mapped memory.  If the
 *		previous mapping was to a permanent (user-supplied)
 *		memory object, it is preserved.
 *
 *		The attributes (protection and inheritance) of the
 *		destination region are preserved.
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 *
 *	Implementation notes:
 *		To overwrite temporary virtual memory, it is
 *		sufficient to remove the previous mapping and insert
 *		the new copy.  This replacement is done either on
 *		the whole region (if no permanent virtual memory
 *		objects are embedded in the destination region) or
 *		in individual map entries.
 *
 *		To overwrite permanent virtual memory, it is
 *		necessary to copy each page, as the external
 *		memory management interface currently does not
 *		provide any optimizations.
 *
 *		Once a page of permanent memory has been overwritten,
 *		it is impossible to interrupt this function; otherwise,
 *		the call would be neither atomic nor location-independent.
 *		The kernel-state portion of a user thread must be
 *		interruptible.
 *
 *		It may be expensive to forward all requests that might
 *		overwrite permanent memory (vm_write, vm_copy) to
 *		uninterruptible kernel threads.  This routine may be
 *		called by interruptible threads; however, success is
 *		not guaranteed -- if the request cannot be performed
 *		atomically and interruptibly, an error indication is
 *		returned.
 */
kern_return_t vm_map_copy_overwrite(dst_map, dst_addr, copy, interruptible)
	vm_map_t	dst_map;
	vm_offset_t	dst_addr;
	vm_map_copy_t	copy;
	boolean_t	interruptible;
{
	vm_size_t	size;
	vm_offset_t	start;
	vm_map_entry_t	tmp_entry;
	vm_map_entry_t	entry;

	boolean_t	contains_permanent_objects = FALSE;

	interruptible = FALSE;	/* XXX */

	/*
	 *	Check for null copy object.
	 */

	if (copy == VM_MAP_COPY_NULL)
		return(KERN_SUCCESS);

	/*
	 *	Only works for entry lists at the moment.  Will
	 *      support page lists LATER.
	 */

#if	NORMA_IPC
	vm_map_convert_from_page_list(copy);
#else
	assert(copy->type == VM_MAP_COPY_ENTRY_LIST);
#endif

	/*
	 *	Currently this routine only handles page-aligned
	 *	regions.  Eventually, it should handle misalignments
	 *	by actually copying pages.
	 */

	if (!page_aligned(copy->offset) ||
	    !page_aligned(copy->size) ||
	    !page_aligned(dst_addr))
		return(KERN_INVALID_ARGUMENT);

	size = copy->size;

	if (size == 0) {
		vm_map_copy_discard(copy);
		return(KERN_SUCCESS);
	}

	/*
	 *	Verify that the destination is all writeable
	 *	initially.
	 */
start_pass_1:
	vm_map_lock(dst_map);
	if (!vm_map_lookup_entry(dst_map, dst_addr, &tmp_entry)) {
		vm_map_unlock(dst_map);
		return(KERN_INVALID_ADDRESS);
	}
	vm_map_clip_start(dst_map, tmp_entry, dst_addr);
	for (entry = tmp_entry;;) {
		vm_size_t	sub_size = (entry->vme_end - entry->vme_start);
		vm_map_entry_t	next = entry->vme_next;

		if ( ! (entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock(dst_map);
			return(KERN_PROTECTION_FAILURE);
		}

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  Anything could happen
		 *	when we unlock the map, so start over.
		 */
                if (entry->in_transition) {

                        /*
                         * Say that we are waiting, and wait for entry.
                         */
                        entry->needs_wakeup = TRUE;
                        vm_map_entry_wait(dst_map, FALSE);

			goto start_pass_1;
		}

		if (size <= sub_size)
			break;

		if ((next == vm_map_to_entry(dst_map)) ||
		    (next->vme_start != entry->vme_end)) {
			vm_map_unlock(dst_map);
			return(KERN_INVALID_ADDRESS);
		}


		/*
		 *	Check for permanent objects in the destination.
		 */

		if ((entry->object.vm_object != VM_OBJECT_NULL) &&
			   !entry->object.vm_object->temporary)
			contains_permanent_objects = TRUE;

		size -= sub_size;
		entry = next;
	}

	/*
	 *	If there are permanent objects in the destination, then
	 *	the copy cannot be interrupted.
	 */

	if (interruptible && contains_permanent_objects)
		return(KERN_FAILURE);	/* XXX */

	/*
	 * XXXO	If there are no permanent objects in the destination,
	 * XXXO	and the source and destination map entry zones match,
	 * XXXO and the destination map entry is not shared, 
	 * XXXO	then the map entries can be deleted and replaced
	 * XXXO	with those from the copy.  The following code is the
	 * XXXO	basic idea of what to do, but there are lots of annoying
	 * XXXO	little details about getting protection and inheritance
	 * XXXO	right.  Should add protection, inheritance, and sharing checks
	 * XXXO	to the above pass and make sure that no wiring is involved.
	 */
/*
 *	if (!contains_permanent_objects &&
 *	    copy->cpy_hdr.entries_pageable == dst_map->hdr.entries_pageable) {
 *
 *		 *
 *		 *	Run over copy and adjust entries.  Steal code
 *		 *	from vm_map_copyout() to do this.
 *		 *
 *
 *		tmp_entry = tmp_entry->vme_prev;
 *		vm_map_delete(dst_map, dst_addr, dst_addr + copy->size);
 *		vm_map_copy_insert(dst_map, tmp_entry, copy);
 *
 *		vm_map_unlock(dst_map);
 *		vm_map_copy_discard(copy);
 *	}
 */
	/*
	 *
	 *	Make a second pass, overwriting the data
	 *	At the beginning of each loop iteration,
	 *	the next entry to be overwritten is "tmp_entry"
	 *	(initially, the value returned from the lookup above),
	 *	and the starting address expected in that entry
	 *	is "start".
	 */

	start = dst_addr;

	while (vm_map_copy_first_entry(copy) != vm_map_copy_to_entry(copy)) {
		vm_map_entry_t	copy_entry = vm_map_copy_first_entry(copy);
		vm_size_t	copy_size = (copy_entry->vme_end - copy_entry->vme_start);
		vm_object_t	object;
		
		entry = tmp_entry;
		size = (entry->vme_end - entry->vme_start);
		/*
		 *	Make sure that no holes popped up in the
		 *	address map, and that the protection is
		 *	still valid, in case the map was unlocked
		 *	earlier.
		 */

		if (entry->vme_start != start) {
			vm_map_unlock(dst_map);
			return(KERN_INVALID_ADDRESS);
		}
		assert(entry != vm_map_to_entry(dst_map));

		/*
		 *	Check protection again
		 */

		if ( ! (entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock(dst_map);
			return(KERN_PROTECTION_FAILURE);
		}

		/*
		 *	Adjust to source size first
		 */

		if (copy_size < size) {
			vm_map_clip_end(dst_map, entry, entry->vme_start + copy_size);
			size = copy_size;
		}

		/*
		 *	Adjust to destination size
		 */

		if (size < copy_size) {
			vm_map_copy_clip_end(copy, copy_entry,
				copy_entry->vme_start + size);
			copy_size = size;
		}

		assert((entry->vme_end - entry->vme_start) == size);
		assert((tmp_entry->vme_end - tmp_entry->vme_start) == size);
		assert((copy_entry->vme_end - copy_entry->vme_start) == size);

		/*
		 *	If the destination contains temporary unshared memory,
		 *	we can perform the copy by throwing it away and
		 *	installing the source data.
		 */

		object = entry->object.vm_object;
		if (!entry->is_shared &&
		    ((object == VM_OBJECT_NULL) || object->temporary)) {
			vm_object_t	old_object = entry->object.vm_object;
			vm_offset_t	old_offset = entry->offset;

			entry->object = copy_entry->object;
			entry->offset = copy_entry->offset;
			entry->needs_copy = copy_entry->needs_copy;
			entry->wired_count = 0;
			entry->user_wired_count = 0;

			vm_map_copy_entry_unlink(copy, copy_entry);
			vm_map_copy_entry_dispose(copy, copy_entry);

			vm_object_pmap_protect(
				old_object,
				old_offset,
				size,
				dst_map->pmap,
				tmp_entry->vme_start,
				VM_PROT_NONE);

			vm_object_deallocate(old_object);

			/*
			 *	Set up for the next iteration.  The map
			 *	has not been unlocked, so the next
			 *	address should be at the end of this
			 *	entry, and the next map entry should be
			 *	the one following it.
			 */

			start = tmp_entry->vme_end;
			tmp_entry = tmp_entry->vme_next;
		} else {
			vm_map_version_t	version;
			vm_object_t		dst_object = entry->object.vm_object;
			vm_offset_t		dst_offset = entry->offset;
			kern_return_t		r;

			/*
			 *	Take an object reference, and record
			 *	the map version information so that the
			 *	map can be safely unlocked.
			 */

			vm_object_reference(dst_object);

			version.main_timestamp = dst_map->timestamp;

			vm_map_unlock(dst_map);

			/*
			 *	Copy as much as possible in one pass
			 */

			copy_size = size;
			r = vm_fault_copy(
					copy_entry->object.vm_object,
					copy_entry->offset,
					&copy_size,
					dst_object,
					dst_offset,
					dst_map,
					&version,
					FALSE /* XXX interruptible */ );

			/*
			 *	Release the object reference
			 */

			vm_object_deallocate(dst_object);

			/*
			 *	If a hard error occurred, return it now
			 */

			if (r != KERN_SUCCESS)
				return(r);

			if (copy_size != 0) {
				/*
				 *	Dispose of the copied region
				 */

				vm_map_copy_clip_end(copy, copy_entry,
					copy_entry->vme_start + copy_size);
				vm_map_copy_entry_unlink(copy, copy_entry);
				vm_object_deallocate(copy_entry->object.vm_object);
				vm_map_copy_entry_dispose(copy, copy_entry);
			}

			/*
			 *	Pick up in the destination map where we left off.
			 *
			 *	Use the version information to avoid a lookup
			 *	in the normal case.
			 */

			start += copy_size;
			vm_map_lock(dst_map);
			if ((version.main_timestamp + 1) == dst_map->timestamp) {
				/* We can safely use saved tmp_entry value */

				vm_map_clip_end(dst_map, tmp_entry, start);
				tmp_entry = tmp_entry->vme_next;
			} else {
				/* Must do lookup of tmp_entry */

				if (!vm_map_lookup_entry(dst_map, start, &tmp_entry)) {
					vm_map_unlock(dst_map);
					return(KERN_INVALID_ADDRESS);
				}
				vm_map_clip_start(dst_map, tmp_entry, start);
			}
		}

	}
	vm_map_unlock(dst_map);

	/*
	 *	Throw away the vm_map_copy object
	 */
	vm_map_copy_discard(copy);

	return(KERN_SUCCESS);
}

/*
 *	Macro:		vm_map_copy_insert
 *	
 *	Description:
 *		Link a copy chain ("copy") into a map at the
 *		specified location (after "where").
 *	Side effects:
 *		The copy chain is destroyed.
 *	Warning:
 *		The arguments are evaluated multiple times.
 */
#define	vm_map_copy_insert(map, where, copy)				\
	MACRO_BEGIN							\
	(((where)->vme_next)->vme_prev = vm_map_copy_last_entry(copy))	\
		->vme_next = ((where)->vme_next);			\
	((where)->vme_next = vm_map_copy_first_entry(copy))		\
		->vme_prev = (where);					\
	(map)->hdr.nentries += (copy)->cpy_hdr.nentries;		\
	zfree(vm_map_copy_zone, (vm_offset_t) copy);			\
	MACRO_END

/*
 *	Routine:	vm_map_copyout
 *
 *	Description:
 *		Copy out a copy chain ("copy") into newly-allocated
 *		space in the destination map.
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 */
kern_return_t vm_map_copyout(dst_map, dst_addr, copy)
	register
	vm_map_t	dst_map;
	vm_offset_t	*dst_addr;	/* OUT */
	register
	vm_map_copy_t	copy;
{
	vm_size_t	size;
	vm_size_t	adjustment;
	vm_offset_t	start;
	vm_offset_t	vm_copy_start;
	vm_map_entry_t	last;
	register
	vm_map_entry_t	entry;

	/*
	 *	Check for null copy object.
	 */

	if (copy == VM_MAP_COPY_NULL) {
		*dst_addr = 0;
		return(KERN_SUCCESS);
	}

	/*
	 *	Check for special copy object, created
	 *	by vm_map_copyin_object.
	 */

	if (copy->type == VM_MAP_COPY_OBJECT) {
		vm_object_t object = copy->cpy_object;
		vm_size_t offset = copy->offset;
		vm_size_t tmp_size = copy->size;
		kern_return_t kr;

		*dst_addr = 0;
		kr = vm_map_enter(dst_map, dst_addr, tmp_size,
				  (vm_offset_t) 0, TRUE,
				  object, offset, FALSE,
				  VM_PROT_DEFAULT, VM_PROT_ALL,
				  VM_INHERIT_DEFAULT);
		if (kr != KERN_SUCCESS)
			return(kr);
		zfree(vm_map_copy_zone, (vm_offset_t) copy);
		return(KERN_SUCCESS);
	}

	if (copy->type == VM_MAP_COPY_PAGE_LIST)
		return(vm_map_copyout_page_list(dst_map, dst_addr, copy));

	/*
	 *	Find space for the data
	 */

	vm_copy_start = trunc_page(copy->offset);
	size =	round_page(copy->offset + copy->size) - vm_copy_start;

 StartAgain: ;

	vm_map_lock(dst_map);
	start = ((last = dst_map->first_free) == vm_map_to_entry(dst_map)) ?
		vm_map_min(dst_map) : last->vme_end;

	while (TRUE) {
		vm_map_entry_t	next = last->vme_next;
		vm_offset_t	end = start + size;

		if ((end > dst_map->max_offset) || (end < start)) {
			if (dst_map->wait_for_space) {
				if (size <= (dst_map->max_offset - dst_map->min_offset)) {
					assert_wait((event_t) dst_map, TRUE);
					vm_map_unlock(dst_map);
					thread_block((void (*)()) 0);
					goto StartAgain;
				}
			}
			vm_map_unlock(dst_map);
			return(KERN_NO_SPACE);
		}

		if ((next == vm_map_to_entry(dst_map)) ||
		    (next->vme_start >= end))
			break;

		last = next;
		start = last->vme_end;
	}

	/*
	 *	Since we're going to just drop the map
	 *	entries from the copy into the destination
	 *	map, they must come from the same pool.
	 */

	if (copy->cpy_hdr.entries_pageable != dst_map->hdr.entries_pageable) {
	    /*
	     * Mismatches occur when dealing with the default
	     * pager.
	     */
	    zone_t		old_zone;
	    vm_map_entry_t	next, new;

	    /*
	     * Find the zone that the copies were allocated from
	     */
	    old_zone = (copy->cpy_hdr.entries_pageable)
			? vm_map_entry_zone
			: vm_map_kentry_zone;
	    entry = vm_map_copy_first_entry(copy);

	    /*
	     * Reinitialize the copy so that vm_map_copy_entry_link
	     * will work.
	     */
	    copy->cpy_hdr.nentries = 0;
	    copy->cpy_hdr.entries_pageable = dst_map->hdr.entries_pageable;
	    vm_map_copy_first_entry(copy) =
	     vm_map_copy_last_entry(copy) =
		vm_map_copy_to_entry(copy);

	    /*
	     * Copy each entry.
	     */
	    while (entry != vm_map_copy_to_entry(copy)) {
		new = vm_map_copy_entry_create(copy);
		vm_map_entry_copy_full(new, entry);
		vm_map_copy_entry_link(copy,
				vm_map_copy_last_entry(copy),
				new);
		next = entry->vme_next;
		zfree(old_zone, (vm_offset_t) entry);
		entry = next;
	    }
	}

	/*
	 *	Adjust the addresses in the copy chain, and
	 *	reset the region attributes.
	 */

	adjustment = start - vm_copy_start;
	for (entry = vm_map_copy_first_entry(copy);
	     entry != vm_map_copy_to_entry(copy);
	     entry = entry->vme_next) {
		entry->vme_start += adjustment;
		entry->vme_end += adjustment;

		entry->inheritance = VM_INHERIT_DEFAULT;
		entry->protection = VM_PROT_DEFAULT;
		entry->max_protection = VM_PROT_ALL;
		entry->projected_on = 0;

		/*
		 * If the entry is now wired,
		 * map the pages into the destination map.
		 */
		if (entry->wired_count != 0) {
		    register vm_offset_t va;
		    vm_offset_t		 offset;
		    register vm_object_t object;

		    object = entry->object.vm_object;
		    offset = entry->offset;
		    va = entry->vme_start;

		    pmap_pageable(dst_map->pmap,
				  entry->vme_start,
				  entry->vme_end,
				  TRUE);

		    while (va < entry->vme_end) {
			register vm_page_t	m;

			/*
			 * Look up the page in the object.
			 * Assert that the page will be found in the
			 * top object:
			 * either
			 *	the object was newly created by
			 *	vm_object_copy_slowly, and has
			 *	copies of all of the pages from
			 *	the source object
			 * or
			 *	the object was moved from the old
			 *	map entry; because the old map
			 *	entry was wired, all of the pages
			 *	were in the top-level object.
			 *	(XXX not true if we wire pages for
			 *	 reading)
			 */
			vm_object_lock(object);
			vm_object_paging_begin(object);

			m = vm_page_lookup(object, offset);
			if (m == VM_PAGE_NULL || m->wire_count == 0 ||
			    m->absent)
			    panic("vm_map_copyout: wiring 0x%x", m);

			m->busy = TRUE;
			vm_object_unlock(object);

			PMAP_ENTER(dst_map->pmap, va, m,
				   entry->protection, TRUE);

			vm_object_lock(object);
			PAGE_WAKEUP_DONE(m);
			/* the page is wired, so we don't have to activate */
			vm_object_paging_end(object);
			vm_object_unlock(object);

			offset += PAGE_SIZE;
			va += PAGE_SIZE;
		    }
		}


	}

	/*
	 *	Correct the page alignment for the result
	 */

	*dst_addr = start + (copy->offset - vm_copy_start);

	/*
	 *	Update the hints and the map size
	 */

	if (dst_map->first_free == last)
		dst_map->first_free = vm_map_copy_last_entry(copy);
	SAVE_HINT(dst_map, vm_map_copy_last_entry(copy));

	dst_map->size += size;

	/*
	 *	Link in the copy
	 */

	vm_map_copy_insert(dst_map, last, copy);

	vm_map_unlock(dst_map);

	/*
	 * XXX	If wiring_required, call vm_map_pageable
	 */

	return(KERN_SUCCESS);
}

/*
 *
 *	vm_map_copyout_page_list:
 *
 *	Version of vm_map_copyout() for page list vm map copies.
 *
 */
kern_return_t vm_map_copyout_page_list(dst_map, dst_addr, copy)
	register
	vm_map_t	dst_map;
	vm_offset_t	*dst_addr;	/* OUT */
	register
	vm_map_copy_t	copy;
{
	vm_size_t	size;
	vm_offset_t	start;
	vm_offset_t	end;
	vm_offset_t	offset;
	vm_map_entry_t	last;
	register
	vm_object_t	object;
	vm_page_t	*page_list, m;
	vm_map_entry_t	entry;
	vm_offset_t	old_last_offset;
	boolean_t	cont_invoked, needs_wakeup = FALSE;
	kern_return_t	result = KERN_SUCCESS;
	vm_map_copy_t	orig_copy;
	vm_offset_t	dst_offset;
	boolean_t	must_wire;

	/*
	 *	Make sure the pages are stolen, because we are
	 *	going to put them in a new object.  Assume that
	 *	all pages are identical to first in this regard.
	 */

	page_list = &copy->cpy_page_list[0];
	if ((*page_list)->tabled)
		vm_map_copy_steal_pages(copy);

	/*
	 *	Find space for the data
	 */

	size =	round_page(copy->offset + copy->size) -
		trunc_page(copy->offset);
StartAgain:
	vm_map_lock(dst_map);
	must_wire = dst_map->wiring_required;

	last = dst_map->first_free;
	if (last == vm_map_to_entry(dst_map)) {
		start = vm_map_min(dst_map);
	} else {
		start = last->vme_end;
	}

	while (TRUE) {
		vm_map_entry_t next = last->vme_next;
		end = start + size;

		if ((end > dst_map->max_offset) || (end < start)) {
			if (dst_map->wait_for_space) {
				if (size <= (dst_map->max_offset -
					     dst_map->min_offset)) {
					assert_wait((event_t) dst_map, TRUE);
					vm_map_unlock(dst_map);
					thread_block((void (*)()) 0);
					goto StartAgain;
				}
			}
			vm_map_unlock(dst_map);
			return(KERN_NO_SPACE);
		}

		if ((next == vm_map_to_entry(dst_map)) ||
		    (next->vme_start >= end)) {
			break;
		}

		last = next;
		start = last->vme_end;
	}

	/*
	 *	See whether we can avoid creating a new entry (and object) by
	 *	extending one of our neighbors.  [So far, we only attempt to
	 *	extend from below.]
	 *
	 *	The code path below here is a bit twisted.  If any of the
	 *	extension checks fails, we branch to create_object.  If
	 *	it all works, we fall out the bottom and goto insert_pages.
	 */
	if (last == vm_map_to_entry(dst_map) ||
	    last->vme_end != start ||
	    last->is_shared != FALSE ||
	    last->is_sub_map != FALSE ||
	    last->inheritance != VM_INHERIT_DEFAULT ||
	    last->protection != VM_PROT_DEFAULT ||
	    last->max_protection != VM_PROT_ALL ||
	    (must_wire ? (last->wired_count != 1 ||
		    last->user_wired_count != 1) :
		(last->wired_count != 0))) {
		    goto create_object;
	}
	
	/*
	 * If this entry needs an object, make one.
	 */
	if (last->object.vm_object == VM_OBJECT_NULL) {
		object = vm_object_allocate(
			(vm_size_t)(last->vme_end - last->vme_start + size));
		last->object.vm_object = object;
		last->offset = 0;
		vm_object_lock(object);
	}
	else {
	    vm_offset_t	prev_offset = last->offset;
	    vm_size_t	prev_size = start - last->vme_start;
	    vm_size_t	new_size;

	    /*
	     *	This is basically vm_object_coalesce.
	     */

	    object = last->object.vm_object;
	    vm_object_lock(object);

	    /*
	     *	Try to collapse the object first
	     */
	    vm_object_collapse(object);

	    /*
	     *	Can't coalesce if pages not mapped to
	     *	last may be in use anyway:
	     *	. more than one reference
	     *	. paged out
	     *	. shadows another object
	     *	. has a copy elsewhere
	     *	. paging references (pages might be in page-list)
	     */

	    if ((object->ref_count > 1) ||
		object->pager_created ||
		(object->shadow != VM_OBJECT_NULL) ||
		(object->copy != VM_OBJECT_NULL) ||
		(object->paging_in_progress != 0)) {
		    vm_object_unlock(object);
		    goto create_object;
	    }

	    /*
	     *	Extend the object if necessary.  Don't have to call
	     *  vm_object_page_remove because the pages aren't mapped,
	     *	and vm_page_replace will free up any old ones it encounters.
	     */
	    new_size = prev_offset + prev_size + size;
	    if (new_size > object->size)
		object->size = new_size;
        }

	/*
	 *	Coalesced the two objects - can extend
	 *	the previous map entry to include the
	 *	new range.
	 */
	dst_map->size += size;
	last->vme_end = end;

	SAVE_HINT(dst_map, last);

	goto insert_pages;

create_object:

	/*
	 *	Create object
	 */
	object = vm_object_allocate(size);

	/*
	 *	Create entry
	 */

	entry = vm_map_entry_create(dst_map);

	entry->object.vm_object = object;
	entry->offset = 0;

	entry->is_shared = FALSE;
	entry->is_sub_map = FALSE;
	entry->needs_copy = FALSE;

	if (must_wire) {
		entry->wired_count = 1;
		entry->user_wired_count = 1;
	} else {
		entry->wired_count = 0;
		entry->user_wired_count = 0;
	}

	entry->in_transition = TRUE;
	entry->needs_wakeup = FALSE;

	entry->vme_start = start;
	entry->vme_end = start + size;
	
	entry->inheritance = VM_INHERIT_DEFAULT;
	entry->protection = VM_PROT_DEFAULT;
	entry->max_protection = VM_PROT_ALL;
	entry->projected_on = 0;

	vm_object_lock(object);

	/*
	 *	Update the hints and the map size
	 */
	if (dst_map->first_free == last) {
		dst_map->first_free = entry;
	}
	SAVE_HINT(dst_map, entry);
	dst_map->size += size;

	/*
	 *	Link in the entry
	 */
	vm_map_entry_link(dst_map, last, entry);
	last = entry;

	/*
	 *	Transfer pages into new object.  
	 *	Scan page list in vm_map_copy.
	 */
insert_pages:
	dst_offset = copy->offset & PAGE_MASK;
	cont_invoked = FALSE;
	orig_copy = copy;
	last->in_transition = TRUE;
	old_last_offset = last->offset
	    + (start - last->vme_start);

	vm_page_lock_queues();

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		m = *page_list;
		assert(m && !m->tabled);

		/*
		 *	Must clear busy bit in page before inserting it.
		 *	Ok to skip wakeup logic because nobody else
		 *	can possibly know about this page.
		 *	The page is dirty in its new object.
		 */

		assert(!m->wanted);

		m->busy = FALSE;
		m->dirty = TRUE;
		vm_page_replace(m, object, old_last_offset + offset);
		if (must_wire) {
			vm_page_wire(m);
			PMAP_ENTER(dst_map->pmap,
				   last->vme_start + m->offset - last->offset,
				   m, last->protection, TRUE);
		} else {
			vm_page_activate(m);
		}

		*page_list++ = VM_PAGE_NULL;
		if (--(copy->cpy_npages) == 0 &&
		    vm_map_copy_has_cont(copy)) {
			vm_map_copy_t	new_copy;

			/*
			 *	Ok to unlock map because entry is
			 *	marked in_transition.
			 */
			cont_invoked = TRUE;
			vm_page_unlock_queues();
			vm_object_unlock(object);
			vm_map_unlock(dst_map);
			vm_map_copy_invoke_cont(copy, &new_copy, &result);

			if (result == KERN_SUCCESS) {

				/*
				 *	If we got back a copy with real pages,
				 *	steal them now.  Either all of the
				 *	pages in the list are tabled or none
				 *	of them are; mixtures are not possible.
				 *
				 *	Save original copy for consume on
				 *	success logic at end of routine.
				 */
				if (copy != orig_copy)
					vm_map_copy_discard(copy);

				if ((copy = new_copy) != VM_MAP_COPY_NULL) {
					page_list = &copy->cpy_page_list[0];
					if ((*page_list)->tabled)
				    		vm_map_copy_steal_pages(copy);
				}
			}
			else {
				/*
				 *	Continuation failed.
				 */
				vm_map_lock(dst_map);
				goto error;
			}

			vm_map_lock(dst_map);
			vm_object_lock(object);
			vm_page_lock_queues();
		}
	}

	vm_page_unlock_queues();
	vm_object_unlock(object);

	*dst_addr = start + dst_offset;
	
	/*
	 *	Clear the in transition bits.  This is easy if we
	 *	didn't have a continuation.
	 */
error:
	if (!cont_invoked) {
		/*
		 *	We didn't unlock the map, so nobody could
		 *	be waiting.
		 */
		last->in_transition = FALSE;
		assert(!last->needs_wakeup);
		needs_wakeup = FALSE;
	}
	else {
		if (!vm_map_lookup_entry(dst_map, start, &entry))
			panic("vm_map_copyout_page_list: missing entry");

                /*
                 * Clear transition bit for all constituent entries that
                 * were in the original entry.  Also check for waiters.
                 */
                while((entry != vm_map_to_entry(dst_map)) &&
                      (entry->vme_start < end)) {
                        assert(entry->in_transition);
                        entry->in_transition = FALSE;
                        if(entry->needs_wakeup) {
                                entry->needs_wakeup = FALSE;
                                needs_wakeup = TRUE;
                        }
                        entry = entry->vme_next;
                }
	}
	
	if (result != KERN_SUCCESS)
		vm_map_delete(dst_map, start, end);

	vm_map_unlock(dst_map);

	if (needs_wakeup)
		vm_map_entry_wakeup(dst_map);

	/*
	 *	Consume on success logic.
	 */
	if (copy != orig_copy) {
		zfree(vm_map_copy_zone, (vm_offset_t) copy);
	}
	if (result == KERN_SUCCESS) {
		zfree(vm_map_copy_zone, (vm_offset_t) orig_copy);
	}
	
	return(result);
}

/*
 *	Routine:	vm_map_copyin
 *
 *	Description:
 *		Copy the specified region (src_addr, len) from the
 *		source address space (src_map), possibly removing
 *		the region from the source address space (src_destroy).
 *
 *	Returns:
 *		A vm_map_copy_t object (copy_result), suitable for
 *		insertion into another address space (using vm_map_copyout),
 *		copying over another address space region (using
 *		vm_map_copy_overwrite).  If the copy is unused, it
 *		should be destroyed (using vm_map_copy_discard).
 *
 *	In/out conditions:
 *		The source map should not be locked on entry.
 */
kern_return_t vm_map_copyin(src_map, src_addr, len, src_destroy, copy_result)
	vm_map_t	src_map;
	vm_offset_t	src_addr;
	vm_size_t	len;
	boolean_t	src_destroy;
	vm_map_copy_t	*copy_result;	/* OUT */
{
	vm_map_entry_t	tmp_entry;	/* Result of last map lookup --
					 * in multi-level lookup, this
					 * entry contains the actual
					 * vm_object/offset.
					 */

	vm_offset_t	src_start;	/* Start of current entry --
					 * where copy is taking place now
					 */
	vm_offset_t	src_end;	/* End of entire region to be
					 * copied */

	register
	vm_map_copy_t	copy;		/* Resulting copy */

	/*
	 *	Check for copies of zero bytes.
	 */

	if (len == 0) {
		*copy_result = VM_MAP_COPY_NULL;
		return(KERN_SUCCESS);
	}

	/*
	 *	Compute start and end of region
	 */

	src_start = trunc_page(src_addr);
	src_end = round_page(src_addr + len);

	/*
	 *	Check that the end address doesn't overflow
	 */

	if (src_end <= src_start)
		if ((src_end < src_start) || (src_start != 0))
			return(KERN_INVALID_ADDRESS);

	/*
	 *	Allocate a header element for the list.
	 *
	 *	Use the start and end in the header to 
	 *	remember the endpoints prior to rounding.
	 */

	copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	vm_map_copy_first_entry(copy) =
	 vm_map_copy_last_entry(copy) = vm_map_copy_to_entry(copy);
	copy->type = VM_MAP_COPY_ENTRY_LIST;
	copy->cpy_hdr.nentries = 0;
	copy->cpy_hdr.entries_pageable = TRUE;

	copy->offset = src_addr;
	copy->size = len;
	
#define	RETURN(x)						\
	MACRO_BEGIN						\
	vm_map_unlock(src_map);					\
	vm_map_copy_discard(copy);				\
	MACRO_RETURN(x);					\
	MACRO_END

	/*
	 *	Find the beginning of the region.
	 */

 	vm_map_lock(src_map);

	if (!vm_map_lookup_entry(src_map, src_start, &tmp_entry))
		RETURN(KERN_INVALID_ADDRESS);
	vm_map_clip_start(src_map, tmp_entry, src_start);

	/*
	 *	Go through entries until we get to the end.
	 */

	while (TRUE) {
		register
		vm_map_entry_t	src_entry = tmp_entry;	/* Top-level entry */
		vm_size_t	src_size;		/* Size of source
							 * map entry (in both
							 * maps)
							 */

		register
		vm_object_t	src_object;		/* Object to copy */
		vm_offset_t	src_offset;

		boolean_t	src_needs_copy;		/* Should source map
							 * be made read-only
							 * for copy-on-write?
							 */

		register
		vm_map_entry_t	new_entry;		/* Map entry for copy */
		boolean_t	new_entry_needs_copy;	/* Will new entry be COW? */

		boolean_t	was_wired;		/* Was source wired? */
		vm_map_version_t version;		/* Version before locks
							 * dropped to make copy
							 */

		/*
		 *	Verify that the region can be read.
		 */

		if (! (src_entry->protection & VM_PROT_READ))
			RETURN(KERN_PROTECTION_FAILURE);

		/*
		 *	Clip against the endpoints of the entire region.
		 */

		vm_map_clip_end(src_map, src_entry, src_end);

		src_size = src_entry->vme_end - src_start;
		src_object = src_entry->object.vm_object;
		src_offset = src_entry->offset;
		was_wired = (src_entry->wired_count != 0);

		/*
		 *	Create a new address map entry to
		 *	hold the result.  Fill in the fields from
		 *	the appropriate source entries.
		 */

		new_entry = vm_map_copy_entry_create(copy);
		vm_map_entry_copy(new_entry, src_entry);

		/*
		 *	Attempt non-blocking copy-on-write optimizations.
		 */

		if (src_destroy &&
		    (src_object == VM_OBJECT_NULL ||
		     (src_object->temporary && !src_object->use_shared_copy)))
		{
		    /*
		     * If we are destroying the source, and the object
		     * is temporary, and not shared writable,
		     * we can move the object reference
		     * from the source to the copy.  The copy is
		     * copy-on-write only if the source is.
		     * We make another reference to the object, because
		     * destroying the source entry will deallocate it.
		     */
		    vm_object_reference(src_object);

		    /*
		     * Copy is always unwired.  vm_map_copy_entry
		     * set its wired count to zero.
		     */

		    goto CopySuccessful;
		}

		if (!was_wired &&
		    vm_object_copy_temporary(
				&new_entry->object.vm_object,
				&new_entry->offset,
				&src_needs_copy,
				&new_entry_needs_copy)) {

			new_entry->needs_copy = new_entry_needs_copy;

			/*
			 *	Handle copy-on-write obligations
			 */

			if (src_needs_copy && !tmp_entry->needs_copy) {
				vm_object_pmap_protect(
					src_object,
					src_offset,
					src_size,
			      		(src_entry->is_shared ? PMAP_NULL
						: src_map->pmap),
					src_entry->vme_start,
					src_entry->protection &
						~VM_PROT_WRITE);

				tmp_entry->needs_copy = TRUE;
			}

			/*
			 *	The map has never been unlocked, so it's safe to
			 *	move to the next entry rather than doing another
			 *	lookup.
			 */

			goto CopySuccessful;
		}

		new_entry->needs_copy = FALSE;

		/*
		 *	Take an object reference, so that we may
		 *	release the map lock(s).
		 */

		assert(src_object != VM_OBJECT_NULL);
		vm_object_reference(src_object);

		/*
		 *	Record the timestamp for later verification.
		 *	Unlock the map.
		 */

		version.main_timestamp = src_map->timestamp;
		vm_map_unlock(src_map);

		/*
		 *	Perform the copy
		 */

		if (was_wired) {
			vm_object_lock(src_object);
			(void) vm_object_copy_slowly(
					src_object,
					src_offset,
					src_size,
					FALSE,
					&new_entry->object.vm_object);
			new_entry->offset = 0;
			new_entry->needs_copy = FALSE;
		} else {
			kern_return_t	result;

			result = vm_object_copy_strategically(src_object,
				src_offset,
				src_size,
				&new_entry->object.vm_object,
				&new_entry->offset,
				&new_entry_needs_copy);

			new_entry->needs_copy = new_entry_needs_copy;
			

			if (result != KERN_SUCCESS) {
				vm_map_copy_entry_dispose(copy, new_entry);

				vm_map_lock(src_map);
				RETURN(result);
			}

		}

		/*
		 *	Throw away the extra reference
		 */

		vm_object_deallocate(src_object);

		/*
		 *	Verify that the map has not substantially
		 *	changed while the copy was being made.
		 */

		vm_map_lock(src_map);	/* Increments timestamp once! */

		if ((version.main_timestamp + 1) == src_map->timestamp)
			goto CopySuccessful;

		/*
		 *	Simple version comparison failed.
		 *
		 *	Retry the lookup and verify that the
		 *	same object/offset are still present.
		 *
		 *	[Note: a memory manager that colludes with
		 *	the calling task can detect that we have
		 *	cheated.  While the map was unlocked, the
		 *	mapping could have been changed and restored.]
		 */

		if (!vm_map_lookup_entry(src_map, src_start, &tmp_entry)) {
			vm_map_copy_entry_dispose(copy, new_entry);
			RETURN(KERN_INVALID_ADDRESS);
		}

		src_entry = tmp_entry;
		vm_map_clip_start(src_map, src_entry, src_start);

		if ((src_entry->protection & VM_PROT_READ) == VM_PROT_NONE)
			goto VerificationFailed;

		if (src_entry->vme_end < new_entry->vme_end)
			src_size = (new_entry->vme_end = src_entry->vme_end) - src_start;

		if ((src_entry->object.vm_object != src_object) ||
		    (src_entry->offset != src_offset) ) {

			/*
			 *	Verification failed.
			 *
			 *	Start over with this top-level entry.
			 */

		 VerificationFailed: ;

			vm_object_deallocate(new_entry->object.vm_object);
			vm_map_copy_entry_dispose(copy, new_entry);
			tmp_entry = src_entry;
			continue;
		}

		/*
		 *	Verification succeeded.
		 */

	 CopySuccessful: ;

		/*
		 *	Link in the new copy entry.
		 */

		vm_map_copy_entry_link(copy, vm_map_copy_last_entry(copy),
				       new_entry);
		
		/*
		 *	Determine whether the entire region
		 *	has been copied.
		 */
		src_start = new_entry->vme_end;
		if ((src_start >= src_end) && (src_end != 0))
			break;

		/*
		 *	Verify that there are no gaps in the region
		 */

		tmp_entry = src_entry->vme_next;
		if (tmp_entry->vme_start != src_start)
			RETURN(KERN_INVALID_ADDRESS);
	}

	/*
	 * If the source should be destroyed, do it now, since the
	 * copy was successful. 
	 */
	if (src_destroy)
	    (void) vm_map_delete(src_map, trunc_page(src_addr), src_end);

	vm_map_unlock(src_map);

	*copy_result = copy;
	return(KERN_SUCCESS);

#undef	RETURN
}

/*
 *	vm_map_copyin_object:
 *
 *	Create a copy object from an object.
 *	Our caller donates an object reference.
 */

kern_return_t vm_map_copyin_object(object, offset, size, copy_result)
	vm_object_t	object;
	vm_offset_t	offset;		/* offset of region in object */
	vm_size_t	size;		/* size of region in object */
	vm_map_copy_t	*copy_result;	/* OUT */
{
	vm_map_copy_t	copy;		/* Resulting copy */

	/*
	 *	We drop the object into a special copy object
	 *	that contains the object directly.  These copy objects
	 *	are distinguished by entries_pageable == FALSE
	 *	and null links.
	 */

	copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	vm_map_copy_first_entry(copy) =
	 vm_map_copy_last_entry(copy) = VM_MAP_ENTRY_NULL;
	copy->type = VM_MAP_COPY_OBJECT;
	copy->cpy_object = object;
	copy->offset = offset;
	copy->size = size;

	*copy_result = copy;
	return(KERN_SUCCESS);
}

/*
 *	vm_map_copyin_page_list_cont:
 *
 *	Continuation routine for vm_map_copyin_page_list.
 *	
 *	If vm_map_copyin_page_list can't fit the entire vm range
 *	into a single page list object, it creates a continuation.
 *	When the target of the operation has used the pages in the
 *	initial page list, it invokes the continuation, which calls
 *	this routine.  If an error happens, the continuation is aborted
 *	(abort arg to this routine is TRUE).  To avoid deadlocks, the
 *	pages are discarded from the initial page list before invoking
 *	the continuation.
 *
 *	NOTE: This is not the same sort of continuation used by
 *	the scheduler.
 */

kern_return_t	vm_map_copyin_page_list_cont(cont_args, copy_result)
vm_map_copyin_args_t	cont_args;
vm_map_copy_t		*copy_result;	/* OUT */
{
	kern_return_t	result = 0; /* '=0' to quiet gcc warnings */
	register boolean_t	do_abort, src_destroy, src_destroy_only;

	/*
	 *	Check for cases that only require memory destruction.
	 */
	do_abort = (copy_result == (vm_map_copy_t *) 0);
	src_destroy = (cont_args->destroy_len != (vm_size_t) 0);
	src_destroy_only = (cont_args->src_len == (vm_size_t) 0);

	if (do_abort || src_destroy_only) {
		if (src_destroy) 
			result = vm_map_remove(cont_args->map,
			    cont_args->destroy_addr,
			    cont_args->destroy_addr + cont_args->destroy_len);
		if (!do_abort)
			*copy_result = VM_MAP_COPY_NULL;
	}
	else {
		result = vm_map_copyin_page_list(cont_args->map,
			cont_args->src_addr, cont_args->src_len, src_destroy,
			cont_args->steal_pages, copy_result, TRUE);

		if (src_destroy && !cont_args->steal_pages &&
			vm_map_copy_has_cont(*copy_result)) {
			    vm_map_copyin_args_t	new_args;
		    	    /*
			     *	Transfer old destroy info.
			     */
			    new_args = (vm_map_copyin_args_t)
			    		(*copy_result)->cpy_cont_args;
		            new_args->destroy_addr = cont_args->destroy_addr;
		            new_args->destroy_len = cont_args->destroy_len;
		}
	}
	
	vm_map_deallocate(cont_args->map);
	kfree((vm_offset_t)cont_args, sizeof(vm_map_copyin_args_data_t));

	return(result);
}

/*
 *	vm_map_copyin_page_list:
 *
 *	This is a variant of vm_map_copyin that copies in a list of pages.
 *	If steal_pages is TRUE, the pages are only in the returned list.
 *	If steal_pages is FALSE, the pages are busy and still in their
 *	objects.  A continuation may be returned if not all the pages fit:
 *	the recipient of this copy_result must be prepared to deal with it.
 */

kern_return_t vm_map_copyin_page_list(src_map, src_addr, len, src_destroy,
			    steal_pages, copy_result, is_cont)
	vm_map_t	src_map;
	vm_offset_t	src_addr;
	vm_size_t	len;
	boolean_t	src_destroy;
	boolean_t	steal_pages;
	vm_map_copy_t	*copy_result;	/* OUT */
	boolean_t	is_cont;
{
	vm_map_entry_t	src_entry;
	vm_page_t 	m;
	vm_offset_t	src_start;
	vm_offset_t	src_end;
	vm_size_t	src_size;
	register
	vm_object_t	src_object;
	register
	vm_offset_t	src_offset;
	vm_offset_t	src_last_offset;
	register
	vm_map_copy_t	copy;		/* Resulting copy */
	kern_return_t	result = KERN_SUCCESS;
	boolean_t	need_map_lookup;
        vm_map_copyin_args_t	cont_args;

	/*
	 * 	If steal_pages is FALSE, this leaves busy pages in
	 *	the object.  A continuation must be used if src_destroy
	 *	is true in this case (!steal_pages && src_destroy).
	 *
	 * XXX	Still have a more general problem of what happens
	 * XXX	if the same page occurs twice in a list.  Deadlock
	 * XXX	can happen if vm_fault_page was called.  A
	 * XXX	possible solution is to use a continuation if vm_fault_page
	 * XXX	is called and we cross a map entry boundary.
	 */

	/*
	 *	Check for copies of zero bytes.
	 */

	if (len == 0) {
		*copy_result = VM_MAP_COPY_NULL;
		return(KERN_SUCCESS);
	}

	/*
	 *	Compute start and end of region
	 */

	src_start = trunc_page(src_addr);
	src_end = round_page(src_addr + len);

	/*
	 *	Check that the end address doesn't overflow
	 */

	if (src_end <= src_start && (src_end < src_start || src_start != 0)) {
		return KERN_INVALID_ADDRESS;
	}

	/*
	 *	Allocate a header element for the page list.
	 *
	 *	Record original offset and size, as caller may not
	 *      be page-aligned.
	 */

	copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	copy->type = VM_MAP_COPY_PAGE_LIST;
	copy->cpy_npages = 0;
	copy->offset = src_addr;
	copy->size = len;
	copy->cpy_cont = ((kern_return_t (*)()) 0);
	copy->cpy_cont_args = (char *) VM_MAP_COPYIN_ARGS_NULL;
	
	/*
	 *	Find the beginning of the region.
	 */

do_map_lookup:

 	vm_map_lock(src_map);

	if (!vm_map_lookup_entry(src_map, src_start, &src_entry)) {
		result = KERN_INVALID_ADDRESS;
		goto error;
	}
	need_map_lookup = FALSE;

	/*
	 *	Go through entries until we get to the end.
	 */

	while (TRUE) {

		if (! (src_entry->protection & VM_PROT_READ)) {
			result = KERN_PROTECTION_FAILURE;
			goto error;
		}

		if (src_end > src_entry->vme_end)
			src_size = src_entry->vme_end - src_start;
		else
			src_size = src_end - src_start;

		src_object = src_entry->object.vm_object;
		src_offset = src_entry->offset +
				(src_start - src_entry->vme_start);

		/*
		 *	If src_object is NULL, allocate it now;
		 *	we're going to fault on it shortly.
		 */
		if (src_object == VM_OBJECT_NULL) {
			src_object = vm_object_allocate((vm_size_t)
				src_entry->vme_end -
				src_entry->vme_start);
			src_entry->object.vm_object = src_object;
		}

		/*
		 * Iterate over pages.  Fault in ones that aren't present.
		 */
		src_last_offset = src_offset + src_size;
		for (; (src_offset < src_last_offset && !need_map_lookup);
		       src_offset += PAGE_SIZE, src_start += PAGE_SIZE) {

			if (copy->cpy_npages == VM_MAP_COPY_PAGE_LIST_MAX) {
make_continuation:
			    /*
			     *	At this point we have the max number of
			     *  pages busy for this thread that we're
			     *  willing to allow.  Stop here and record
			     *  arguments for the remainder.  Note:
			     *  this means that this routine isn't atomic,
			     *  but that's the breaks.  Note that only
			     *  the first vm_map_copy_t that comes back
			     *  from this routine has the right offset
			     *  and size; those from continuations are
			     *  page rounded, and short by the amount
			     *	already done.
			     *
			     *	Reset src_end so the src_destroy
			     *	code at the bottom doesn't do
			     *	something stupid.
			     */

			    cont_args = (vm_map_copyin_args_t) 
			    	    kalloc(sizeof(vm_map_copyin_args_data_t));
			    cont_args->map = src_map;
			    vm_map_reference(src_map);
			    cont_args->src_addr = src_start;
			    cont_args->src_len = len - (src_start - src_addr);
			    if (src_destroy) {
			    	cont_args->destroy_addr = cont_args->src_addr;
				cont_args->destroy_len = cont_args->src_len;
			    }
			    else {
			    	cont_args->destroy_addr = (vm_offset_t) 0;
				cont_args->destroy_len = (vm_offset_t) 0;
			    }
			    cont_args->steal_pages = steal_pages;

			    copy->cpy_cont_args = (char *) cont_args;
			    copy->cpy_cont = vm_map_copyin_page_list_cont;

			    src_end = src_start;
			    vm_map_clip_end(src_map, src_entry, src_end);
			    break;
			}

			/*
			 *	Try to find the page of data.
			 */
			vm_object_lock(src_object);
			vm_object_paging_begin(src_object);
			if (((m = vm_page_lookup(src_object, src_offset)) !=
			    VM_PAGE_NULL) && !m->busy && !m->fictitious &&
			    !m->absent && !m->error) {

				/*
				 *	This is the page.  Mark it busy
				 *	and keep the paging reference on
				 *	the object whilst we do our thing.
				 */
				m->busy = TRUE;

				/*
				 *	Also write-protect the page, so
				 *	that the map`s owner cannot change
				 *	the data.  The busy bit will prevent
				 *	faults on the page from succeeding
				 *	until the copy is released; after
				 *	that, the page can be re-entered
				 *	as writable, since we didn`t alter
				 *	the map entry.  This scheme is a
				 *	cheap copy-on-write.
				 *
				 *	Don`t forget the protection and
				 *	the page_lock value!
				 *
				 *	If the source is being destroyed
				 *	AND not shared writable, we don`t
				 *	have to protect the page, since
				 *	we will destroy the (only)
				 *	writable mapping later.
				 */
				if (!src_destroy ||
				    src_object->use_shared_copy)
				{
				    pmap_page_protect(m->phys_addr,
						  src_entry->protection
						& ~m->page_lock
						& ~VM_PROT_WRITE);
				}

			}
			else {
				vm_prot_t result_prot;
				vm_page_t top_page;
				kern_return_t kr;
				
				/*
				 *	Have to fault the page in; must
				 *	unlock the map to do so.  While
				 *	the map is unlocked, anything
				 *	can happen, we must lookup the
				 *	map entry before continuing.
				 */
				vm_map_unlock(src_map);
				need_map_lookup = TRUE;
retry:
				result_prot = VM_PROT_READ;
				
				kr = vm_fault_page(src_object, src_offset,
						   VM_PROT_READ, FALSE, FALSE,
						   &result_prot, &m, &top_page,
						   FALSE, (void (*)()) 0);
				/*
				 *	Cope with what happened.
				 */
				switch (kr) {
				case VM_FAULT_SUCCESS:
					break;
				case VM_FAULT_INTERRUPTED: /* ??? */
			        case VM_FAULT_RETRY:
					vm_object_lock(src_object);
					vm_object_paging_begin(src_object);
					goto retry;
				case VM_FAULT_MEMORY_SHORTAGE:
					VM_PAGE_WAIT((void (*)()) 0);
					vm_object_lock(src_object);
					vm_object_paging_begin(src_object);
					goto retry;
				case VM_FAULT_FICTITIOUS_SHORTAGE:
					vm_page_more_fictitious();
					vm_object_lock(src_object);
					vm_object_paging_begin(src_object);
					goto retry;
				case VM_FAULT_MEMORY_ERROR:
					/*
					 *	Something broke.  If this
					 *	is a continuation, return
					 *	a partial result if possible,
					 *	else fail the whole thing.
					 *	In the continuation case, the
					 *	next continuation call will
					 *	get this error if it persists.
					 */
					vm_map_lock(src_map);
					if (is_cont &&
					    copy->cpy_npages != 0)
						goto make_continuation;

					result = KERN_MEMORY_ERROR;
					goto error;
				}
				
				if (top_page != VM_PAGE_NULL) {
					vm_object_lock(src_object);
					VM_PAGE_FREE(top_page);
					vm_object_paging_end(src_object);
					vm_object_unlock(src_object);
				 }

				 /*
				  *	We do not need to write-protect
				  *	the page, since it cannot have
				  *	been in the pmap (and we did not
				  *	enter it above).  The busy bit
				  *	will protect the page from being
				  *	entered as writable until it is
				  *	unlocked.
				  */

			}

			/*
			 *	The page is busy, its object is locked, and
			 *	we have a paging reference on it.  Either
			 *	the map is locked, or need_map_lookup is
			 *	TRUE.
			 * 
			 *	Put the page in the page list.
			 */
			copy->cpy_page_list[copy->cpy_npages++] = m;
			vm_object_unlock(m->object);
		}
			
		/*
		 *	DETERMINE whether the entire region
		 *	has been copied.
		 */
		if (src_start >= src_end && src_end != 0) {
			if (need_map_lookup)
				vm_map_lock(src_map);
			break;
		}

		/*
		 *	If need_map_lookup is TRUE, have to start over with
		 *	another map lookup.  Note that we dropped the map
		 *	lock (to call vm_fault_page) above only in this case.
		 */
		if (need_map_lookup)
			goto do_map_lookup;

		/*
		 *	Verify that there are no gaps in the region
		 */

		src_start = src_entry->vme_end;
		src_entry = src_entry->vme_next;
		if (src_entry->vme_start != src_start) {
			result = KERN_INVALID_ADDRESS;
			goto error;
		}
	}

	/*
	 *	If steal_pages is true, make sure all
	 *	pages in the copy are not in any object
	 *	We try to remove them from the original
	 *	object, but we may have to copy them.
	 *
	 *	At this point every page in the list is busy
	 *	and holds a paging reference to its object.
	 *	When we're done stealing, every page is busy,
	 *	and in no object (m->tabled == FALSE).
	 */
	src_start = trunc_page(src_addr);
	if (steal_pages) {
		register int i;
		vm_offset_t	unwire_end;

		unwire_end = src_start;
		for (i = 0; i < copy->cpy_npages; i++) {

			/*
			 *	Remove the page from its object if it
			 *	can be stolen.  It can be stolen if:
 			 *
			 *	(1) The source is being destroyed, 
			 *	      the object is temporary, and
			 *	      not shared.
			 *	(2) The page is not precious.
			 *
			 *	The not shared check consists of two
			 *	parts:  (a) there are no objects that
			 *	shadow this object.  (b) it is not the
			 *	object in any shared map entries (i.e.,
			 *	use_shared_copy is not set).
			 *
			 *	The first check (a) means that we can't
			 *	steal pages from objects that are not
			 *	at the top of their shadow chains.  This
			 *	should not be a frequent occurrence.
			 *
			 *	Stealing wired pages requires telling the
			 *	pmap module to let go of them.
			 * 
			 *	NOTE: stealing clean pages from objects
			 *  	whose mappings survive requires a call to
			 *	the pmap module.  Maybe later.
 			 */
			m = copy->cpy_page_list[i];
			src_object = m->object;
			vm_object_lock(src_object);

			if (src_destroy &&
			    src_object->temporary &&
			    (!src_object->shadowed) &&
			    (!src_object->use_shared_copy) &&
			    !m->precious) {
				vm_offset_t	page_vaddr;
				
				page_vaddr = src_start + (i * PAGE_SIZE);
				if (m->wire_count > 0) {

				    assert(m->wire_count == 1);
				    /*
				     *	In order to steal a wired
				     *	page, we have to unwire it
				     *	first.  We do this inline
				     *	here because we have the page.
				     *
				     *	Step 1: Unwire the map entry.
				     *		Also tell the pmap module
				     *		that this piece of the
				     *		pmap is pageable.
				     */
				    vm_object_unlock(src_object);
				    if (page_vaddr >= unwire_end) {
				        if (!vm_map_lookup_entry(src_map,
				            page_vaddr, &src_entry))
		    panic("vm_map_copyin_page_list: missing wired map entry");

				        vm_map_clip_start(src_map, src_entry,
						page_vaddr);
				    	vm_map_clip_end(src_map, src_entry,
						src_start + src_size);

					assert(src_entry->wired_count > 0);
				        src_entry->wired_count = 0;
				        src_entry->user_wired_count = 0;
					unwire_end = src_entry->vme_end;
				        pmap_pageable(vm_map_pmap(src_map),
					    page_vaddr, unwire_end, TRUE);
				    }

				    /*
				     *	Step 2: Unwire the page.
				     *	pmap_remove handles this for us.
				     */
				    vm_object_lock(src_object);
				}

				/*
				 *	Don't need to remove the mapping;
				 *	vm_map_delete will handle it.
				 *
				 *	Steal the page.  Setting the wire count
				 *	to zero is vm_page_unwire without
				 *	activating the page.
  				 */
				vm_page_lock_queues();
	 			vm_page_remove(m);
				if (m->wire_count > 0) {
				    m->wire_count = 0;
				    vm_page_wire_count--;
				} else {
				    VM_PAGE_QUEUES_REMOVE(m);
				}
				vm_page_unlock_queues();
			}
			else {
			        /*
				 *	Have to copy this page.  Have to
				 *	unlock the map while copying,
				 *	hence no further page stealing.
				 *	Hence just copy all the pages.
				 *	Unlock the map while copying;
				 *	This means no further page stealing.
				 */
				vm_object_unlock(src_object);
				vm_map_unlock(src_map);

				vm_map_copy_steal_pages(copy);

				vm_map_lock(src_map);
				break;
		        }

			vm_object_paging_end(src_object);
			vm_object_unlock(src_object);
	        }

		/*
		 * If the source should be destroyed, do it now, since the
		 * copy was successful.
		 */

		if (src_destroy) {
		    (void) vm_map_delete(src_map, src_start, src_end);
		}
	}
	else {
		/*
		 *	!steal_pages leaves busy pages in the map.
		 *	This will cause src_destroy to hang.  Use
		 *	a continuation to prevent this.
		 */
	        if (src_destroy && !vm_map_copy_has_cont(copy)) {
			cont_args = (vm_map_copyin_args_t) 
				kalloc(sizeof(vm_map_copyin_args_data_t));
			vm_map_reference(src_map);
			cont_args->map = src_map;
			cont_args->src_addr = (vm_offset_t) 0;
			cont_args->src_len = (vm_size_t) 0;
			cont_args->destroy_addr = src_start;
			cont_args->destroy_len = src_end - src_start;
			cont_args->steal_pages = FALSE;

			copy->cpy_cont_args = (char *) cont_args;
			copy->cpy_cont = vm_map_copyin_page_list_cont;
		}
			
	}

	vm_map_unlock(src_map);

	*copy_result = copy;
	return(result);

error:
	vm_map_unlock(src_map);
	vm_map_copy_discard(copy);
	return(result);
}

/*
 *	vm_map_fork:
 *
 *	Create and return a new map based on the old
 *	map, according to the inheritance values on the
 *	regions in that map.
 *
 *	The source map must not be locked.
 */
vm_map_t vm_map_fork(old_map)
	vm_map_t	old_map;
{
	vm_map_t	new_map;
	register
	vm_map_entry_t	old_entry;
	register
	vm_map_entry_t	new_entry;
	pmap_t		new_pmap = pmap_create((vm_size_t) 0);
	vm_size_t	new_size = 0;
	vm_size_t	entry_size;
	register
	vm_object_t	object;

	vm_map_lock(old_map);

	new_map = vm_map_create(new_pmap,
			old_map->min_offset,
			old_map->max_offset,
			old_map->hdr.entries_pageable);

	for (
	    old_entry = vm_map_first_entry(old_map);
	    old_entry != vm_map_to_entry(old_map);
	    ) {
		if (old_entry->is_sub_map)
			panic("vm_map_fork: encountered a submap");

		entry_size = (old_entry->vme_end - old_entry->vme_start);

		switch (old_entry->inheritance) {
		case VM_INHERIT_NONE:
			break;

		case VM_INHERIT_SHARE:
		        /*
			 *	New sharing code.  New map entry
			 *	references original object.  Temporary
			 *	objects use asynchronous copy algorithm for
			 *	future copies.  First make sure we have
			 *	the right object.  If we need a shadow,
			 *	or someone else already has one, then
			 *	make a new shadow and share it.
			 */

			object = old_entry->object.vm_object;
			if (object == VM_OBJECT_NULL) {
				object = vm_object_allocate(
					    (vm_size_t)(old_entry->vme_end -
							old_entry->vme_start));
				old_entry->offset = 0;
				old_entry->object.vm_object = object;
				assert(!old_entry->needs_copy);
			}
			else if (old_entry->needs_copy || object->shadowed ||
			    (object->temporary && !old_entry->is_shared &&
			     object->size > (vm_size_t)(old_entry->vme_end -
						old_entry->vme_start))) {

			    assert(object->temporary);
			    assert(!(object->shadowed && old_entry->is_shared));
			    vm_object_shadow(
			        &old_entry->object.vm_object,
			        &old_entry->offset,
			        (vm_size_t) (old_entry->vme_end -
					     old_entry->vme_start));
				
			    /*
			     *	If we're making a shadow for other than
			     *	copy on write reasons, then we have
			     *	to remove write permission.
			     */

			    if (!old_entry->needs_copy &&
				(old_entry->protection & VM_PROT_WRITE)) {
			    	pmap_protect(vm_map_pmap(old_map),
					     old_entry->vme_start,
					     old_entry->vme_end,
					     old_entry->protection &
					     	~VM_PROT_WRITE);
			    }
			    old_entry->needs_copy = FALSE;
			    object = old_entry->object.vm_object;
			}

			/*
			 *	Set use_shared_copy to indicate that
			 *	object must use shared (delayed) copy-on
			 *	write.  This is ignored for permanent objects.
			 *	Bump the reference count for the new entry
			 */

			vm_object_lock(object);
			object->use_shared_copy = TRUE;
			object->ref_count++;
			vm_object_unlock(object);

			if (old_entry->projected_on != 0) {
			  /*
			   *   If entry is projected buffer, clone the
                           *   entry exactly.
                           */

			  vm_map_entry_copy_full(new_entry, old_entry);

			} else {
			  /*
			   *	Clone the entry, using object ref from above.
			   *	Mark both entries as shared.
			   */

			  new_entry = vm_map_entry_create(new_map);
			  vm_map_entry_copy(new_entry, old_entry);
			  old_entry->is_shared = TRUE;
			  new_entry->is_shared = TRUE;
			}

			/*
			 *	Insert the entry into the new map -- we
			 *	know we're inserting at the end of the new
			 *	map.
			 */

			vm_map_entry_link(
				new_map,
				vm_map_last_entry(new_map),
				new_entry);

			/*
			 *	Update the physical map
			 */

			pmap_copy(new_map->pmap, old_map->pmap,
				new_entry->vme_start,
				entry_size,
				old_entry->vme_start);

			new_size += entry_size;
			break;

		case VM_INHERIT_COPY:
			if (old_entry->wired_count == 0) {
				boolean_t	src_needs_copy;
				boolean_t	new_entry_needs_copy;

				new_entry = vm_map_entry_create(new_map);
				vm_map_entry_copy(new_entry, old_entry);

				if (vm_object_copy_temporary(
					&new_entry->object.vm_object,
					&new_entry->offset,
					&src_needs_copy,
					&new_entry_needs_copy)) {

					/*
					 *	Handle copy-on-write obligations
					 */

					if (src_needs_copy && !old_entry->needs_copy) {
						vm_object_pmap_protect(
							old_entry->object.vm_object,
							old_entry->offset,
							entry_size,
							(old_entry->is_shared ?
								PMAP_NULL :
								old_map->pmap),
							old_entry->vme_start,
							old_entry->protection &
							    ~VM_PROT_WRITE);

						old_entry->needs_copy = TRUE;
					}

					new_entry->needs_copy = new_entry_needs_copy;

					/*
					 *	Insert the entry at the end
					 *	of the map.
					 */

					vm_map_entry_link(new_map,
						vm_map_last_entry(new_map),
						new_entry);


					new_size += entry_size;
					break;
				}

				vm_map_entry_dispose(new_map, new_entry);
			}

			/* INNER BLOCK (copy cannot be optimized) */ {

			vm_offset_t	start = old_entry->vme_start;
			vm_map_copy_t	copy;
			vm_map_entry_t	last = vm_map_last_entry(new_map);

			vm_map_unlock(old_map);
			if (vm_map_copyin(old_map,
					start,
					entry_size,
					FALSE,
					&copy) 
			    != KERN_SUCCESS) {
			    	vm_map_lock(old_map);
				if (!vm_map_lookup_entry(old_map, start, &last))
					last = last->vme_next;
				old_entry = last;
				/*
				 *	For some error returns, want to
				 *	skip to the next element.
				 */

				continue;
			}

			/*
			 *	Insert the copy into the new map
			 */

			vm_map_copy_insert(new_map, last, copy);
			new_size += entry_size;

			/*
			 *	Pick up the traversal at the end of
			 *	the copied region.
			 */

			vm_map_lock(old_map);
			start += entry_size;
			if (!vm_map_lookup_entry(old_map, start, &last))
				last = last->vme_next;
			 else
				vm_map_clip_start(old_map, last, start);
			old_entry = last;

			continue;
			/* INNER BLOCK (copy cannot be optimized) */ }
		}
		old_entry = old_entry->vme_next;
	}

	new_map->size = new_size;
	vm_map_unlock(old_map);

	return(new_map);
}

/*
 *	vm_map_lookup:
 *
 *	Finds the VM object, offset, and
 *	protection for a given virtual address in the
 *	specified map, assuming a page fault of the
 *	type specified.
 *
 *	Returns the (object, offset, protection) for
 *	this address, whether it is wired down, and whether
 *	this map has the only reference to the data in question.
 *	In order to later verify this lookup, a "version"
 *	is returned.
 *
 *	The map should not be locked; it will not be
 *	locked on exit.  In order to guarantee the
 *	existence of the returned object, it is returned
 *	locked.
 *
 *	If a lookup is requested with "write protection"
 *	specified, the map may be changed to perform virtual
 *	copying operations, although the data referenced will
 *	remain the same.
 */
kern_return_t vm_map_lookup(var_map, vaddr, fault_type, out_version,
				object, offset, out_prot, wired)
	vm_map_t		*var_map;	/* IN/OUT */
	register vm_offset_t	vaddr;
	register vm_prot_t	fault_type;

	vm_map_version_t	*out_version;	/* OUT */
	vm_object_t		*object;	/* OUT */
	vm_offset_t		*offset;	/* OUT */
	vm_prot_t		*out_prot;	/* OUT */
	boolean_t		*wired;		/* OUT */
{
	register vm_map_entry_t		entry;
	register vm_map_t		map = *var_map;
	register vm_prot_t		prot;

	RetryLookup: ;

	/*
	 *	Lookup the faulting address.
	 */

	vm_map_lock_read(map);

#define	RETURN(why) \
		{ \
		vm_map_unlock_read(map); \
		return(why); \
		}

	/*
	 *	If the map has an interesting hint, try it before calling
	 *	full blown lookup routine.
	 */

	simple_lock(&map->hint_lock);
	entry = map->hint;
	simple_unlock(&map->hint_lock);

	if ((entry == vm_map_to_entry(map)) ||
	    (vaddr < entry->vme_start) || (vaddr >= entry->vme_end)) {
		vm_map_entry_t	tmp_entry;

		/*
		 *	Entry was either not a valid hint, or the vaddr
		 *	was not contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, &tmp_entry))
			RETURN(KERN_INVALID_ADDRESS);

		entry = tmp_entry;
	}

	/*
	 *	Handle submaps.
	 */

	if (entry->is_sub_map) {
		vm_map_t	old_map = map;

		*var_map = map = entry->object.sub_map;
		vm_map_unlock_read(old_map);
		goto RetryLookup;
	}
		
	/*
	 *	Check whether this task is allowed to have
	 *	this page.
	 */

	prot = entry->protection;

	if ((fault_type & (prot)) != fault_type) 
		if ((prot & VM_PROT_NOTIFY) && (fault_type & VM_PROT_WRITE)) {
			RETURN(KERN_WRITE_PROTECTION_FAILURE);
		} else {
			RETURN(KERN_PROTECTION_FAILURE);
		}

	/*
	 *	If this page is not pageable, we have to get
	 *	it for all possible accesses.
	 */

	if (*wired = (entry->wired_count != 0))
		prot = fault_type = entry->protection;

	/*
	 *	If the entry was copy-on-write, we either ...
	 */

	if (entry->needs_copy) {
	    	/*
		 *	If we want to write the page, we may as well
		 *	handle that now since we've got the map locked.
		 *
		 *	If we don't need to write the page, we just
		 *	demote the permissions allowed.
		 */

		if (fault_type & VM_PROT_WRITE) {
			/*
			 *	Make a new object, and place it in the
			 *	object chain.  Note that no new references
			 *	have appeared -- one just moved from the
			 *	map to the new object.
			 */

			if (vm_map_lock_read_to_write(map)) {
				goto RetryLookup;
			}
			map->timestamp++;

			vm_object_shadow(
			    &entry->object.vm_object,
			    &entry->offset,
			    (vm_size_t) (entry->vme_end - entry->vme_start));
				
			entry->needs_copy = FALSE;
			
			vm_map_lock_write_to_read(map);
		}
		else {
			/*
			 *	We're attempting to read a copy-on-write
			 *	page -- don't allow writes.
			 */

			prot &= (~VM_PROT_WRITE);
		}
	}

	/*
	 *	Create an object if necessary.
	 */
	if (entry->object.vm_object == VM_OBJECT_NULL) {

		if (vm_map_lock_read_to_write(map)) {
			goto RetryLookup;
		}

		entry->object.vm_object = vm_object_allocate(
				(vm_size_t)(entry->vme_end - entry->vme_start));
		entry->offset = 0;
		vm_map_lock_write_to_read(map);
	}

	/*
	 *	Return the object/offset from this entry.  If the entry
	 *	was copy-on-write or empty, it has been fixed up.  Also
	 *	return the protection.
	 */

        *offset = (vaddr - entry->vme_start) + entry->offset;
        *object = entry->object.vm_object;
	*out_prot = prot;

	/*
	 *	Lock the object to prevent it from disappearing
	 */

	vm_object_lock(*object);

	/*
	 *	Save the version number and unlock the map.
	 */

	out_version->main_timestamp = map->timestamp;

	RETURN(KERN_SUCCESS);
	
#undef	RETURN
}

/*
 *	vm_map_verify:
 *
 *	Verifies that the map in question has not changed
 *	since the given version.  If successful, the map
 *	will not change until vm_map_verify_done() is called.
 */
boolean_t	vm_map_verify(map, version)
	register
	vm_map_t	map;
	register
	vm_map_version_t *version;	/* REF */
{
	boolean_t	result;

	vm_map_lock_read(map);
	result = (map->timestamp == version->main_timestamp);

	if (!result)
		vm_map_unlock_read(map);

	return(result);
}

/*
 *	vm_map_verify_done:
 *
 *	Releases locks acquired by a vm_map_verify.
 *
 *	This is now a macro in vm/vm_map.h.  It does a
 *	vm_map_unlock_read on the map.
 */

/*
 *	vm_region:
 *
 *	User call to obtain information about a region in
 *	a task's address map.
 */

kern_return_t	vm_region(map, address, size,
				protection, max_protection,
				inheritance, is_shared,
				object_name, offset_in_object)
	vm_map_t	map;
	vm_offset_t	*address;		/* IN/OUT */
	vm_size_t	*size;			/* OUT */
	vm_prot_t	*protection;		/* OUT */
	vm_prot_t	*max_protection;	/* OUT */
	vm_inherit_t	*inheritance;		/* OUT */
	boolean_t	*is_shared;		/* OUT */
	ipc_port_t	*object_name;		/* OUT */
	vm_offset_t	*offset_in_object;	/* OUT */
{
	vm_map_entry_t	tmp_entry;
	register
	vm_map_entry_t	entry;
	register
	vm_offset_t	tmp_offset;
	vm_offset_t	start;

	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

	start = *address;

	vm_map_lock_read(map);
	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		if ((entry = tmp_entry->vme_next) == vm_map_to_entry(map)) {
			vm_map_unlock_read(map);
		   	return(KERN_NO_SPACE);
		}
	} else {
		entry = tmp_entry;
	}

	start = entry->vme_start;
	*protection = entry->protection;
	*max_protection = entry->max_protection;
	*inheritance = entry->inheritance;
	*address = start;
	*size = (entry->vme_end - start);

	tmp_offset = entry->offset;


	if (entry->is_sub_map) {
		*is_shared = FALSE;
		*object_name = IP_NULL;
		*offset_in_object = tmp_offset;
	} else {
		*is_shared = entry->is_shared;
		*object_name = vm_object_name(entry->object.vm_object);
		*offset_in_object = tmp_offset;
	}

	vm_map_unlock_read(map);

	return(KERN_SUCCESS);
}

/*
 *	Routine:	vm_map_simplify
 *
 *	Description:
 *		Attempt to simplify the map representation in
 *		the vicinity of the given starting address.
 *	Note:
 *		This routine is intended primarily to keep the
 *		kernel maps more compact -- they generally don't
 *		benefit from the "expand a map entry" technology
 *		at allocation time because the adjacent entry
 *		is often wired down.
 */
void vm_map_simplify(map, start)
	vm_map_t	map;
	vm_offset_t	start;
{
	vm_map_entry_t	this_entry;
	vm_map_entry_t	prev_entry;

	vm_map_lock(map);
	if (
		(vm_map_lookup_entry(map, start, &this_entry)) &&
		((prev_entry = this_entry->vme_prev) != vm_map_to_entry(map)) &&

		(prev_entry->vme_end == start) &&

		(prev_entry->is_shared == FALSE) &&
		(prev_entry->is_sub_map == FALSE) &&

		(this_entry->is_shared == FALSE) &&
		(this_entry->is_sub_map == FALSE) &&

		(prev_entry->inheritance == this_entry->inheritance) &&
		(prev_entry->protection == this_entry->protection) &&
		(prev_entry->max_protection == this_entry->max_protection) &&
		(prev_entry->wired_count == this_entry->wired_count) &&
		(prev_entry->user_wired_count == this_entry->user_wired_count) &&

		(prev_entry->needs_copy == this_entry->needs_copy) &&

		(prev_entry->object.vm_object == this_entry->object.vm_object) &&
		((prev_entry->offset + (prev_entry->vme_end - prev_entry->vme_start))
		     == this_entry->offset) &&
	        (prev_entry->projected_on == 0) &&
	        (this_entry->projected_on == 0) 
	) {
		if (map->first_free == this_entry)
			map->first_free = prev_entry;

		SAVE_HINT(map, prev_entry);
		vm_map_entry_unlink(map, this_entry);
		prev_entry->vme_end = this_entry->vme_end;
	 	vm_object_deallocate(this_entry->object.vm_object);
		vm_map_entry_dispose(map, this_entry);
	}
	vm_map_unlock(map);
}


/*
 *	Routine:	vm_map_machine_attribute
 *	Purpose:
 *		Provide machine-specific attributes to mappings,
 *		such as cachability etc. for machines that provide
 *		them.  NUMA architectures and machines with big/strange
 *		caches will use this.
 *	Note:
 *		Responsibilities for locking and checking are handled here,
 *		everything else in the pmap module. If any non-volatile
 *		information must be kept, the pmap module should handle
 *		it itself. [This assumes that attributes do not
 *		need to be inherited, which seems ok to me]
 */
kern_return_t vm_map_machine_attribute(map, address, size, attribute, value)
	vm_map_t	map;
	vm_offset_t	address;
	vm_size_t	size;
	vm_machine_attribute_t	attribute;
	vm_machine_attribute_val_t* value;		/* IN/OUT */
{
	kern_return_t	ret;

	if (address < vm_map_min(map) ||
	    (address + size) > vm_map_max(map))
		return KERN_INVALID_ARGUMENT;

	vm_map_lock(map);

	ret = pmap_attribute(map->pmap, address, size, attribute, value);

	vm_map_unlock(map);

	return ret;
}

#include <mach_kdb.h>


#if	MACH_KDB

#define	printf	kdbprintf

/*
 *	vm_map_print:	[ debug ]
 */
void vm_map_print(map)
	register vm_map_t	map;
{
	register vm_map_entry_t	entry;
	extern int indent;

	iprintf("Task map 0x%X: pmap=0x%X,",
 		(vm_offset_t) map, (vm_offset_t) (map->pmap));
	 printf("ref=%d,nentries=%d,", map->ref_count, map->hdr.nentries);
	 printf("version=%d\n",	map->timestamp);
	indent += 2;
	for (entry = vm_map_first_entry(map);
	     entry != vm_map_to_entry(map);
	     entry = entry->vme_next) {
		static char *inheritance_name[3] = { "share", "copy", "none"};

		iprintf("map entry 0x%X: ", (vm_offset_t) entry);
		 printf("start=0x%X, end=0x%X, ",
			(vm_offset_t) entry->vme_start, (vm_offset_t) entry->vme_end);
		printf("prot=%X/%X/%s, ",
			entry->protection,
			entry->max_protection,
			inheritance_name[entry->inheritance]);
		if (entry->wired_count != 0) {
			printf("wired(");
			if (entry->user_wired_count != 0)
				printf("u");
			if (entry->wired_count >
			    ((entry->user_wired_count == 0) ? 0 : 1))
				printf("k");
			printf(") ");
		}
		if (entry->in_transition) {
			printf("in transition");
			if (entry->needs_wakeup)
				printf("(wake request)");
			printf(", ");
		}
		if (entry->is_sub_map) {
		 	printf("submap=0x%X, offset=0x%X\n",
				(vm_offset_t) entry->object.sub_map,
				(vm_offset_t) entry->offset);
		} else {
			printf("object=0x%X, offset=0x%X",
				(vm_offset_t) entry->object.vm_object,
				(vm_offset_t) entry->offset);
			if (entry->is_shared)
				printf(", shared");
			if (entry->needs_copy)
				printf(", copy needed");
			printf("\n");

			if ((entry->vme_prev == vm_map_to_entry(map)) ||
			    (entry->vme_prev->object.vm_object != entry->object.vm_object)) {
				indent += 2;
				vm_object_print(entry->object.vm_object);
				indent -= 2;
			}
		}
	}
	indent -= 2;
}

/*
 *	Routine:	vm_map_copy_print
 *	Purpose:
 *		Pretty-print a copy object for ddb.
 */

void vm_map_copy_print(copy)
	vm_map_copy_t copy;
{
	extern int indent;
	int i, npages;

	printf("copy object 0x%x\n", copy);

	indent += 2;

	iprintf("type=%d", copy->type);
	switch (copy->type) {
		case VM_MAP_COPY_ENTRY_LIST:
		printf("[entry_list]");
		break;
		
		case VM_MAP_COPY_OBJECT:
		printf("[object]");
		break;
		
		case VM_MAP_COPY_PAGE_LIST:
		printf("[page_list]");
		break;

		default:
		printf("[bad type]");
		break;
	}
	printf(", offset=0x%x", copy->offset);
	printf(", size=0x%x\n", copy->size);

	switch (copy->type) {
		case VM_MAP_COPY_ENTRY_LIST:
		/* XXX add stuff here */
		break;

		case VM_MAP_COPY_OBJECT:
		iprintf("object=0x%x\n", copy->cpy_object);
		break;

		case VM_MAP_COPY_PAGE_LIST:
		iprintf("npages=%d", copy->cpy_npages);
		printf(", cont=%x", copy->cpy_cont);
		printf(", cont_args=%x\n", copy->cpy_cont_args);
		if (copy->cpy_npages < 0) {
			npages = 0;
		} else if (copy->cpy_npages > VM_MAP_COPY_PAGE_LIST_MAX) {
			npages = VM_MAP_COPY_PAGE_LIST_MAX;
		} else {
			npages = copy->cpy_npages;
		}
		iprintf("copy->cpy_page_list[0..%d] = {", npages);
		for (i = 0; i < npages - 1; i++) {
			printf("0x%x, ", copy->cpy_page_list[i]);
		}
		if (npages > 0) {
			printf("0x%x", copy->cpy_page_list[npages - 1]);
		}
		printf("}\n");
		break;
	}

	indent -=2;
}
#endif	MACH_KDB

#if	NORMA_IPC
/*
 * This should one day be eliminated;
 * we should always construct the right flavor of copy object
 * the first time. Troublesome areas include vm_read, where vm_map_copyin
 * is called without knowing whom the copy object is for.
 * There are also situations where we do want a lazy data structure
 * even if we are sending to a remote port...
 */

/*
 *	Convert a copy to a page list.  The copy argument is in/out
 *	because we probably have to allocate a new vm_map_copy structure.
 *	We take responsibility for discarding the old structure and
 *	use a continuation to do so.  Postponing this discard ensures
 *	that the objects containing the pages we've marked busy will stick
 *	around.  
 */
kern_return_t
vm_map_convert_to_page_list(caller_copy)
	vm_map_copy_t	*caller_copy;
{
	vm_map_entry_t entry, next_entry;
	vm_offset_t va;
	vm_offset_t offset;
	vm_object_t object;
	kern_return_t result;
	vm_map_copy_t copy, new_copy;
	int i, num_pages = 0;

	zone_t entry_zone;

	copy = *caller_copy;

	/*
	 * We may not have to do anything,
	 * or may not be able to do anything.
	 */
	if (copy == VM_MAP_COPY_NULL || copy->type == VM_MAP_COPY_PAGE_LIST) {
		return KERN_SUCCESS;
	}
	if (copy->type == VM_MAP_COPY_OBJECT) {
		return vm_map_convert_to_page_list_from_object(caller_copy);
	}
	if (copy->type != VM_MAP_COPY_ENTRY_LIST) {
		panic("vm_map_convert_to_page_list: copy type %d!\n",
		      copy->type);
	}

	/*
	 *	Allocate the new copy.  Set its continuation to
	 *	discard the old one.
	 */
	new_copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	new_copy->type = VM_MAP_COPY_PAGE_LIST;
	new_copy->cpy_npages = 0;
	new_copy->offset = copy->offset;
	new_copy->size = copy->size;
	new_copy->cpy_cont = vm_map_copy_discard_cont;
	new_copy->cpy_cont_args = (char *) copy;

	/*
	 * Iterate over entries.
	 */
	for (entry = vm_map_copy_first_entry(copy);
	     entry != vm_map_copy_to_entry(copy);
	     entry = entry->vme_next) {

		object = entry->object.vm_object;
		offset = entry->offset;
		/*
		 * Iterate over pages.
		 */
		for (va = entry->vme_start;
		     va < entry->vme_end;
		     va += PAGE_SIZE, offset += PAGE_SIZE) {

			vm_page_t m;

			if (new_copy->cpy_npages == VM_MAP_COPY_PAGE_LIST_MAX) {
				/*
				 *	What a mess.  We need a continuation
				 *	to do the page list, but also one
				 *	to discard the old copy.  The right
				 *	thing to do is probably to copy
				 *	out the old copy into the kernel
				 *	map (or some temporary task holding
				 *	map if we're paranoid about large
				 *	copies), and then copyin the page
				 *	list that we really wanted with
				 *	src_destroy.  LATER.
				 */
				panic("vm_map_convert_to_page_list: num\n");
			}

			/*
			 *	Try to find the page of data.
			 */
			vm_object_lock(object);
			vm_object_paging_begin(object);
			if (((m = vm_page_lookup(object, offset)) !=
			     VM_PAGE_NULL) && !m->busy && !m->fictitious &&
			    !m->absent && !m->error) {

				/*
				 *	This is the page.  Mark it busy
				 *	and keep the paging reference on
				 *	the object whilst we do our thing.
				 */
				m->busy = TRUE;

				/*
				 *	Also write-protect the page, so
				 *	that the map`s owner cannot change
				 *	the data.  The busy bit will prevent
				 *	faults on the page from succeeding
				 *	until the copy is released; after
				 *	that, the page can be re-entered
				 *	as writable, since we didn`t alter
				 *	the map entry.  This scheme is a
				 *	cheap copy-on-write.
				 *
				 *	Don`t forget the protection and
				 *	the page_lock value!
				 */

				pmap_page_protect(m->phys_addr,
						  entry->protection
						& ~m->page_lock
						& ~VM_PROT_WRITE);

			}
			else {
				vm_prot_t result_prot;
				vm_page_t top_page;
				kern_return_t kr;
				
retry:
				result_prot = VM_PROT_READ;
				
				kr = vm_fault_page(object, offset,
						   VM_PROT_READ, FALSE, FALSE,
						   &result_prot, &m, &top_page,
						   FALSE, (void (*)()) 0);
				if (kr == VM_FAULT_MEMORY_SHORTAGE) {
					VM_PAGE_WAIT((void (*)()) 0);
					vm_object_lock(object);
					vm_object_paging_begin(object);
					goto retry;
				}
				if (kr != VM_FAULT_SUCCESS) {
					/* XXX what about data_error? */
					vm_object_lock(object);
					vm_object_paging_begin(object);
					goto retry;
				}
				if (top_page != VM_PAGE_NULL) {
					vm_object_lock(object);
					VM_PAGE_FREE(top_page);
					vm_object_paging_end(object);
					vm_object_unlock(object);
				}
			}
			assert(m);
			m->busy = TRUE;
			new_copy->cpy_page_list[new_copy->cpy_npages++] = m;
			vm_object_unlock(object);
		}
	}

	*caller_copy = new_copy;
	return KERN_SUCCESS;
}

kern_return_t
vm_map_convert_to_page_list_from_object(caller_copy)
	vm_map_copy_t *caller_copy;
{
	vm_object_t object;
	vm_offset_t offset;
	vm_map_copy_t	copy, new_copy;

	copy = *caller_copy;
	assert(copy->type == VM_MAP_COPY_OBJECT);
	object = copy->cpy_object;
	assert(object->size == round_page(object->size));

	/*
	 *	Allocate the new copy.  Set its continuation to
	 *	discard the old one.
	 */
	new_copy = (vm_map_copy_t) zalloc(vm_map_copy_zone);
	new_copy->type = VM_MAP_COPY_PAGE_LIST;
	new_copy->cpy_npages = 0;
	new_copy->offset = copy->offset;
	new_copy->size = copy->size;
	new_copy->cpy_cont = vm_map_copy_discard_cont;
	new_copy->cpy_cont_args = (char *) copy;

	/*
	 * XXX	memory_object_lock_request can probably bust this
	 * XXX  See continuation comment in previous routine for solution.
	 */
	assert(object->size <= VM_MAP_COPY_PAGE_LIST_MAX * PAGE_SIZE);

	for (offset = 0; offset < object->size; offset += PAGE_SIZE) {
		vm_page_t m;

		/*
		 *	Try to find the page of data.
		 */
		vm_object_lock(object);
		vm_object_paging_begin(object);
		m = vm_page_lookup(object, offset);
		if ((m != VM_PAGE_NULL) && !m->busy && !m->fictitious &&
		    !m->absent && !m->error) {
			
			/*
			 *	This is the page.  Mark it busy
			 *	and keep the paging reference on
			 *	the object whilst we do our thing.
			 */
			m->busy = TRUE;
		}
		else {
			vm_prot_t result_prot;
			vm_page_t top_page;
			kern_return_t kr;
			
retry:
			result_prot = VM_PROT_READ;
			
			kr = vm_fault_page(object, offset,
					   VM_PROT_READ, FALSE, FALSE,
					   &result_prot, &m, &top_page,
					   FALSE, (void (*)()) 0);
			if (kr == VM_FAULT_MEMORY_SHORTAGE) {
				VM_PAGE_WAIT((void (*)()) 0);
				vm_object_lock(object);
				vm_object_paging_begin(object);
				goto retry;
			}
			if (kr != VM_FAULT_SUCCESS) {
				/* XXX what about data_error? */
				vm_object_lock(object);
				vm_object_paging_begin(object);
				goto retry;
			}
			
			if (top_page != VM_PAGE_NULL) {
				vm_object_lock(object);
				VM_PAGE_FREE(top_page);
				vm_object_paging_end(object);
				vm_object_unlock(object);
			}
		}
		assert(m);
		m->busy = TRUE;
		new_copy->cpy_page_list[new_copy->cpy_npages++] = m;
		vm_object_unlock(object);
	}

	*caller_copy = new_copy;
	return (KERN_SUCCESS);
}

kern_return_t
vm_map_convert_from_page_list(copy)
	vm_map_copy_t	copy;
{
	vm_object_t object;
	int i;
	vm_map_entry_t	new_entry;
	vm_page_t	*page_list;

	/*
	 * Check type of copy object.
	 */
	if (copy->type == VM_MAP_COPY_ENTRY_LIST) {
		return KERN_SUCCESS;
	}
	if (copy->type == VM_MAP_COPY_OBJECT) {
		printf("vm_map_convert_from_page_list: COPY_OBJECT?");
		return KERN_SUCCESS;
	}
	if (copy->type != VM_MAP_COPY_PAGE_LIST) {
		panic("vm_map_convert_from_page_list 0x%x %d",
		      copy,
		      copy->type);
	}

	/*
	 *	Make sure the pages are loose.  This may be
	 *	a "Can't Happen", but just to be safe ...
	 */
	page_list = &copy->cpy_page_list[0];
	if ((*page_list)->tabled)
		vm_map_copy_steal_pages(copy);

	/*
	 * Create object, and stuff pages into it.
	 */
	object = vm_object_allocate(copy->cpy_npages);
	for (i = 0; i < copy->cpy_npages; i++) {
		register vm_page_t m = *page_list++;
		vm_page_insert(m, object, i * PAGE_SIZE);
		m->busy = FALSE;
		m->dirty = TRUE;
		vm_page_activate(m);
	}

	/*
	 * XXX	If this page list contained a continuation, then
	 * XXX	we're screwed.  The right thing to do is probably do
	 * XXX	the copyout, and then copyin the entry list we really
	 * XXX	wanted.
	 */
	if (vm_map_copy_has_cont(copy))
		panic("convert_from_page_list: continuation");

	/*
	 * Change type of copy object
	 */
	vm_map_copy_first_entry(copy) =
	    vm_map_copy_last_entry(copy) = vm_map_copy_to_entry(copy);
	copy->type = VM_MAP_COPY_ENTRY_LIST;
	copy->cpy_hdr.nentries = 0;
	copy->cpy_hdr.entries_pageable = TRUE;

	/*
	 * Allocate and initialize an entry for object
	 */
	new_entry = vm_map_copy_entry_create(copy);
	new_entry->vme_start = trunc_page(copy->offset);
	new_entry->vme_end = round_page(copy->offset + copy->size);
	new_entry->object.vm_object = object;
	new_entry->offset = 0;
	new_entry->is_shared = FALSE;
	new_entry->is_sub_map = FALSE;
	new_entry->needs_copy = FALSE;
	new_entry->protection = VM_PROT_DEFAULT;
	new_entry->max_protection = VM_PROT_ALL;
	new_entry->inheritance = VM_INHERIT_DEFAULT;
	new_entry->wired_count = 0;
	new_entry->user_wired_count = 0;
	new_entry->projected_on = 0;

	/*
	 * Insert entry into copy object, and return.
	 */
	vm_map_copy_entry_link(copy, vm_map_copy_last_entry(copy), new_entry);
	return(KERN_SUCCESS);
}
#endif	NORMA_IPC
