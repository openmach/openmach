/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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

#include "mach_kdb.h"
#if MACH_KDB

#include <mach_debug.h>

#if	MACH_DEBUG

#include <mach/mach_types.h>	/* vm_address_t */
#include <mach/std_types.h>	/* pointer_t */
#include <mach/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <kern/host.h>
#include <kern/task.h>
#include <ddb/db_sym.h>



/*
 *	Loads a symbol table for an external file into the kernel debugger.
 *	The symbol table data is an array of characters.  It is assumed that
 *	the caller and the kernel debugger agree on its format.
 */
kern_return_t
host_load_symbol_table(host, task, name, symtab, symtab_count)
	host_t		host;
	task_t		task;
	char *		name;
	pointer_t	symtab;
	unsigned int	symtab_count;
{
	kern_return_t	result;
	vm_offset_t	symtab_start;
	vm_offset_t	symtab_end;
	vm_map_t	map;
	vm_map_copy_t	symtab_copy_object;

	if (host == HOST_NULL)
	    return (KERN_INVALID_ARGUMENT);

	/*
	 * Copy the symbol table array into the kernel.
	 * We make a copy of the copy object, and clear
	 * the old one, so that returning error will not
	 * deallocate the data twice.
	 */
	symtab_copy_object = (vm_map_copy_t) symtab;
	result = vm_map_copyout(
			kernel_map,
			&symtab_start,
			vm_map_copy_copy(symtab_copy_object));
	if (result != KERN_SUCCESS)
	    return (result);

	symtab_end = symtab_start + symtab_count;

	/*
	 * Add the symbol table.
	 * Do not keep a reference for the task map.	XXX
	 */
	if (task == TASK_NULL)
	    map = VM_MAP_NULL;
	else
	    map = task->map;
	if (!X_db_sym_init((char *)symtab_start,
			(char *)symtab_end,
			name,
			(char *)map))
	{
	    /*
	     * Not enough room for symbol table - failure.
	     */
	    (void) vm_deallocate(kernel_map,
			symtab_start,
			symtab_count);
	    return (KERN_FAILURE);
	}

	/*
	 * Wire down the symbol table
	 */
	(void) vm_map_pageable(kernel_map,
		symtab_start,
		round_page(symtab_end),
		VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * Discard the original copy object
	 */
	vm_map_copy_discard(symtab_copy_object);

	return (KERN_SUCCESS);
}

#endif	/* MACH_DEBUG */

#endif MACH_KDB
