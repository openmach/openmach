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
 *	File:	vm/pmap.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	Machine address mapping definitions -- machine-independent
 *	section.  [For machine-dependent section, see "machine/pmap.h".]
 */

#ifndef	_VM_PMAP_H_
#define _VM_PMAP_H_

#include <machine/pmap.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/boolean.h>

/*
 *	The following is a description of the interface to the
 *	machine-dependent "physical map" data structure.  The module
 *	must provide a "pmap_t" data type that represents the
 *	set of valid virtual-to-physical addresses for one user
 *	address space.  [The kernel address space is represented
 *	by a distinguished "pmap_t".]  The routines described manage
 *	this type, install and update virtual-to-physical mappings,
 *	and perform operations on physical addresses common to
 *	many address spaces.
 */

/*
 *	Routines used for initialization.
 *	There is traditionally also a pmap_bootstrap,
 *	used very early by machine-dependent code,
 *	but it is not part of the interface.
 */

extern vm_offset_t	pmap_steal_memory();	/* During VM initialization,
						 * steal a chunk of memory.
						 */
extern unsigned int	pmap_free_pages();	/* During VM initialization,
						 * report remaining unused
						 * physical pages.
						 */
extern void		pmap_startup();		/* During VM initialization,
						 * use remaining physical pages
						 * to allocate page frames.
						 */
extern void		pmap_init();		/* Initialization,
						 * after kernel runs
						 * in virtual memory.
						 */

#ifndef	MACHINE_PAGES
/*
 *	If machine/pmap.h defines MACHINE_PAGES, it must implement
 *	the above functions.  The pmap module has complete control.
 *	Otherwise, it must implement
 *		pmap_free_pages
 *		pmap_virtual_space
 *		pmap_next_page
 *		pmap_init
 *	and vm/vm_resident.c implements pmap_steal_memory and pmap_startup
 *	using pmap_free_pages, pmap_next_page, pmap_virtual_space,
 *	and pmap_enter.  pmap_free_pages may over-estimate the number
 *	of unused physical pages, and pmap_next_page may return FALSE
 *	to indicate that there are no more unused pages to return.
 *	However, for best performance pmap_free_pages should be accurate.
 */

extern boolean_t	pmap_next_page();	/* During VM initialization,
						 * return the next unused
						 * physical page.
						 */
extern void		pmap_virtual_space();	/* During VM initialization,
						 * report virtual space
						 * available for the kernel.
						 */
#endif	MACHINE_PAGES

/*
 *	Routines to manage the physical map data structure.
 */

/* Create a pmap_t. */
pmap_t pmap_create(vm_size_t size);

/* Return the kernel's pmap_t. */
#ifndef pmap_kernel
extern pmap_t pmap_kernel(void);
#endif pmap_kernel

/* Gain and release a reference. */
extern void pmap_reference(pmap_t pmap);	
extern void pmap_destroy(pmap_t pmap);

/* Enter a mapping */
extern void pmap_enter(pmap_t pmap, vm_offset_t va, vm_offset_t pa,
		       vm_prot_t prot, boolean_t wired);


/*
 *	Routines that operate on ranges of virtual addresses.
 */

/* Remove mappings. */
void pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva);

/* Change protections. */
void pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot);

/*
 *	Routines to set up hardware state for physical maps to be used.
 */
extern void		pmap_activate();	/* Prepare pmap_t to run
						 * on a given processor.
						 */
extern void		pmap_deactivate();	/* Release pmap_t from
						 * use on processor.
						 */


/*
 *	Routines that operate on physical addresses.
 */

/* Restrict access to page. */
void pmap_page_protect(vm_offset_t pa, vm_prot_t prot);

/*
 *	Routines to manage reference/modify bits based on
 *	physical addresses, simulating them if not provided
 *	by the hardware.
 */

/* Clear reference bit */
void pmap_clear_reference(vm_offset_t pa);

/* Return reference bit */
#ifndef pmap_is_referenced
boolean_t pmap_is_referenced(vm_offset_t pa);
#endif pmap_is_referenced

/* Clear modify bit */
void pmap_clear_modify(vm_offset_t pa);

/* Return modify bit */
boolean_t pmap_is_modified(vm_offset_t pa);


/*
 *	Statistics routines
 */
extern void		pmap_statistics();	/* Return statistics */

#ifndef	pmap_resident_count
extern int		pmap_resident_count();
#endif	pmap_resident_count

/*
 *	Sundry required routines
 */
extern vm_offset_t	pmap_extract();		/* Return a virtual-to-physical
						 * mapping, if possible.
						 */

extern boolean_t	pmap_access();		/* Is virtual address valid? */

extern void		pmap_collect();		/* Perform garbage
						 * collection, if any
						 */

extern void		pmap_change_wiring();	/* Specify pageability */

#ifndef	pmap_phys_address
extern vm_offset_t	pmap_phys_address();	/* Transform address
						 * returned by device
						 * driver mapping function
						 * to physical address
						 * known to this module.
						 */
#endif	pmap_phys_address
#ifndef	pmap_phys_to_frame
extern int		pmap_phys_to_frame();	/* Inverse of
						 * pmap_phys_address,
						 * for use by device driver
						 * mapping function in
						 * machine-independent
						 * pseudo-devices.
						 */
#endif	pmap_phys_to_frame

/*
 *	Optional routines
 */
#ifndef	pmap_copy
extern void		pmap_copy();		/* Copy range of
						 * mappings, if desired.
						 */
#endif	pmap_copy
#ifndef pmap_attribute
extern kern_return_t	pmap_attribute();	/* Get/Set special
						 * memory attributes
						 */
#endif	pmap_attribute

/*
 * Routines defined as macros.
 */
#ifndef	PMAP_ACTIVATE_USER
#define	PMAP_ACTIVATE_USER(pmap, thread, cpu) {		\
	if ((pmap) != kernel_pmap)			\
	    PMAP_ACTIVATE(pmap, thread, cpu);		\
}
#endif	PMAP_ACTIVATE_USER

#ifndef	PMAP_DEACTIVATE_USER
#define	PMAP_DEACTIVATE_USER(pmap, thread, cpu) {	\
	if ((pmap) != kernel_pmap)			\
	    PMAP_DEACTIVATE(pmap, thread, cpu);		\
}
#endif	PMAP_DEACTIVATE_USER

#ifndef	PMAP_ACTIVATE_KERNEL
#define	PMAP_ACTIVATE_KERNEL(cpu)			\
		PMAP_ACTIVATE(kernel_pmap, THREAD_NULL, cpu)
#endif	PMAP_ACTIVATE_KERNEL

#ifndef	PMAP_DEACTIVATE_KERNEL
#define	PMAP_DEACTIVATE_KERNEL(cpu)			\
		PMAP_DEACTIVATE(kernel_pmap, THREAD_NULL, cpu)
#endif	PMAP_DEACTIVATE_KERNEL

/*
 *	Exported data structures
 */

extern pmap_t	kernel_pmap;			/* The kernel's map */

#endif	_VM_PMAP_H_
