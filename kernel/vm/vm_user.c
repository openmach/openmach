/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 *	File:	vm/vm_user.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 * 
 *	User-exported virtual memory functions.
 */

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>	/* to get vm_address_t */
#include <mach/memory_object.h>
#include <mach/std_types.h>	/* to get pointer_t */
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <kern/host.h>
#include <kern/task.h>
#include <vm/vm_fault.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>



vm_statistics_data_t	vm_stat;

/*
 *	vm_allocate allocates "zero fill" memory in the specfied
 *	map.
 */
kern_return_t vm_allocate(map, addr, size, anywhere)
	register vm_map_t	map;
	register vm_offset_t	*addr;
	register vm_size_t	size;
	boolean_t		anywhere;
{
	kern_return_t	result;

	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);
	if (size == 0) {
		*addr = 0;
		return(KERN_SUCCESS);
	}

	if (anywhere)
		*addr = vm_map_min(map);
	else
		*addr = trunc_page(*addr);
	size = round_page(size);

	result = vm_map_enter(
			map,
			addr,
			size,
			(vm_offset_t)0,
			anywhere,
			VM_OBJECT_NULL,
			(vm_offset_t)0,
			FALSE,
			VM_PROT_DEFAULT,
			VM_PROT_ALL,
			VM_INHERIT_DEFAULT);

	return(result);
}

/*
 *	vm_deallocate deallocates the specified range of addresses in the
 *	specified address map.
 */
kern_return_t vm_deallocate(map, start, size)
	register vm_map_t	map;
	vm_offset_t		start;
	vm_size_t		size;
{
	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size == (vm_offset_t) 0)
		return(KERN_SUCCESS);

	return(vm_map_remove(map, trunc_page(start), round_page(start+size)));
}

/*
 *	vm_inherit sets the inheritance of the specified range in the
 *	specified map.
 */
kern_return_t vm_inherit(map, start, size, new_inheritance)
	register vm_map_t	map;
	vm_offset_t		start;
	vm_size_t		size;
	vm_inherit_t		new_inheritance;
{
	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

        switch (new_inheritance) {
        case VM_INHERIT_NONE:
        case VM_INHERIT_COPY:
        case VM_INHERIT_SHARE:
                break;
        default:
                return(KERN_INVALID_ARGUMENT);
        }

	/*Check if range includes projected buffer;
	  user is not allowed direct manipulation in that case*/
	if (projected_buffer_in_range(map, start, start+size))
		return(KERN_INVALID_ARGUMENT);

	return(vm_map_inherit(map,
			      trunc_page(start),
			      round_page(start+size),
			      new_inheritance));
}

/*
 *	vm_protect sets the protection of the specified range in the
 *	specified map.
 */

kern_return_t vm_protect(map, start, size, set_maximum, new_protection)
	register vm_map_t	map;
	vm_offset_t		start;
	vm_size_t		size;
	boolean_t		set_maximum;
	vm_prot_t		new_protection;
{
	if ((map == VM_MAP_NULL) || 
		(new_protection & ~(VM_PROT_ALL|VM_PROT_NOTIFY)))
		return(KERN_INVALID_ARGUMENT);

	/*Check if range includes projected buffer;
	  user is not allowed direct manipulation in that case*/
	if (projected_buffer_in_range(map, start, start+size))
		return(KERN_INVALID_ARGUMENT);

	return(vm_map_protect(map,
			      trunc_page(start),
			      round_page(start+size),
			      new_protection,
			      set_maximum));
}

kern_return_t vm_statistics(map, stat)
	vm_map_t	map;
	vm_statistics_data_t	*stat;
{
	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

	*stat = vm_stat;

	stat->pagesize = PAGE_SIZE;
	stat->free_count = vm_page_free_count;
	stat->active_count = vm_page_active_count;
	stat->inactive_count = vm_page_inactive_count;
	stat->wire_count = vm_page_wire_count;

	return(KERN_SUCCESS);
}

/*
 * Handle machine-specific attributes for a mapping, such
 * as cachability, migrability, etc.
 */
kern_return_t vm_machine_attribute(map, address, size, attribute, value)
	vm_map_t	map;
	vm_address_t	address;
	vm_size_t	size;
	vm_machine_attribute_t	attribute;
	vm_machine_attribute_val_t* value;		/* IN/OUT */
{
	extern kern_return_t	vm_map_machine_attribute();

	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*Check if range includes projected buffer;
	  user is not allowed direct manipulation in that case*/
	if (projected_buffer_in_range(map, address, address+size))
		return(KERN_INVALID_ARGUMENT);

	return vm_map_machine_attribute(map, address, size, attribute, value);
}

kern_return_t vm_read(map, address, size, data, data_size)
	vm_map_t	map;
	vm_address_t	address;
	vm_size_t	size;
	pointer_t	*data;
	vm_size_t	*data_size;
{
	kern_return_t	error;
	vm_map_copy_t	ipc_address;

	if (map == VM_MAP_NULL)
		return(KERN_INVALID_ARGUMENT);

	if ((error = vm_map_copyin(map,
				address,
				size,
				FALSE,	/* src_destroy */
				&ipc_address)) == KERN_SUCCESS) {
		*data = (pointer_t) ipc_address;
		*data_size = size;
	}
	return(error);
}

kern_return_t vm_write(map, address, data, size)
	vm_map_t	map;
	vm_address_t	address;
	pointer_t	data;
	vm_size_t	size;
{
	if (map == VM_MAP_NULL)
		return KERN_INVALID_ARGUMENT;

	return vm_map_copy_overwrite(map, address, (vm_map_copy_t) data,
				     FALSE /* interruptible XXX */);
}

kern_return_t vm_copy(map, source_address, size, dest_address)
	vm_map_t	map;
	vm_address_t	source_address;
	vm_size_t	size;
	vm_address_t	dest_address;
{
	vm_map_copy_t copy;
	kern_return_t kr;

	if (map == VM_MAP_NULL)
		return KERN_INVALID_ARGUMENT;

	kr = vm_map_copyin(map, source_address, size,
			   FALSE, &copy);
	if (kr != KERN_SUCCESS)
		return kr;

	kr = vm_map_copy_overwrite(map, dest_address, copy,
				   FALSE /* interruptible XXX */);
	if (kr != KERN_SUCCESS) {
		vm_map_copy_discard(copy);
		return kr;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_map
 */
kern_return_t vm_map(
		target_map,
		address, size, mask, anywhere,
		memory_object, offset,
		copy,
		cur_protection, max_protection,	inheritance)
	vm_map_t	target_map;
	vm_offset_t	*address;
	vm_size_t	size;
	vm_offset_t	mask;
	boolean_t	anywhere;
	ipc_port_t	memory_object;
	vm_offset_t	offset;
	boolean_t	copy;
	vm_prot_t	cur_protection;
	vm_prot_t	max_protection;
	vm_inherit_t	inheritance;
{
	register
	vm_object_t	object;
	register
	kern_return_t	result;

	if ((target_map == VM_MAP_NULL) ||
	    (cur_protection & ~VM_PROT_ALL) ||
	    (max_protection & ~VM_PROT_ALL))
		return(KERN_INVALID_ARGUMENT);

        switch (inheritance) {
        case VM_INHERIT_NONE:
        case VM_INHERIT_COPY:
        case VM_INHERIT_SHARE:
                break;
        default:
                return(KERN_INVALID_ARGUMENT);
        }

	*address = trunc_page(*address);
	size = round_page(size);

	if (!IP_VALID(memory_object)) {
		object = VM_OBJECT_NULL;
		offset = 0;
		copy = FALSE;
	} else if ((object = vm_object_enter(memory_object, size, FALSE))
			== VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Perform the copy if requested
	 */

	if (copy) {
		vm_object_t	new_object;
		vm_offset_t	new_offset;

		result = vm_object_copy_strategically(object, offset, size,
				&new_object, &new_offset,
				&copy);

		/*
		 *	Throw away the reference to the
		 *	original object, as it won't be mapped.
		 */

		vm_object_deallocate(object);

		if (result != KERN_SUCCESS)
			return (result);

		object = new_object;
		offset = new_offset;
	}

	if ((result = vm_map_enter(target_map,
				address, size, mask, anywhere,
				object, offset,
				copy,
				cur_protection, max_protection, inheritance
				)) != KERN_SUCCESS)
		vm_object_deallocate(object);
	return(result);
}

/*
 *	Specify that the range of the virtual address space
 *	of the target task must not cause page faults for
 *	the indicated accesses.
 *
 *	[ To unwire the pages, specify VM_PROT_NONE. ]
 */
kern_return_t vm_wire(host, map, start, size, access)
	host_t			host;
	register vm_map_t	map;
	vm_offset_t		start;
	vm_size_t		size;
	vm_prot_t		access;
{
	if (host == HOST_NULL)
		return KERN_INVALID_HOST;

	if (map == VM_MAP_NULL)
		return KERN_INVALID_TASK;

	if (access & ~VM_PROT_ALL)
		return KERN_INVALID_ARGUMENT;

	/*Check if range includes projected buffer;
	  user is not allowed direct manipulation in that case*/
	if (projected_buffer_in_range(map, start, start+size))
		return(KERN_INVALID_ARGUMENT);

	return vm_map_pageable_user(map,
				    trunc_page(start),
				    round_page(start+size),
				    access);
}
