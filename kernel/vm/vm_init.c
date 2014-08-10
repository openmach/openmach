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
 *	File:	vm/vm_init.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Initialize the Virtual Memory subsystem.
 */

#include <mach/machine/vm_types.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/memory_object.h>



/*
 *	vm_mem_bootstrap initializes the virtual memory system.
 *	This is done only by the first cpu up.
 */

void vm_mem_bootstrap()
{
	vm_offset_t	start, end;

	/*
	 *	Initializes resident memory structures.
	 *	From here on, all physical memory is accounted for,
	 *	and we use only virtual addresses.
	 */

	vm_page_bootstrap(&start, &end);

	/*
	 *	Initialize other VM packages
	 */

	zone_bootstrap();
	vm_object_bootstrap();
	vm_map_init();
	kmem_init(start, end);
	pmap_init();
	zone_init();
	kalloc_init();
	vm_fault_init();
	vm_page_module_init();
	memory_manager_default_init();
}

void vm_mem_init()
{
	vm_object_init();
}
