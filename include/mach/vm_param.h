/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	mach/vm_param.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Machine independent virtual memory parameters.
 *
 */

#ifndef	_MACH_VM_PARAM_H_
#define _MACH_VM_PARAM_H_

#include <mach/machine/vm_param.h>
#include <mach/machine/vm_types.h>

/*
 *	The machine independent pages are refered to as PAGES.  A page
 *	is some number of hardware pages, depending on the target machine.
 *
 *	All references to the size of a page should be done
 *	with PAGE_SIZE, PAGE_SHIFT, or PAGE_MASK.
 *	They may be implemented as either constants or variables,
 *	depending on more-specific code.
 *	If they're variables, they had better be initialized
 *	by the time system-independent code starts getting called.
 *
 *	Regardless whether it is implemented with a constant or a variable,
 *	the PAGE_SIZE is assumed to be a power of two throughout the
 *	virtual memory system implementation.
 *
 *	More-specific code must at least provide PAGE_SHIFT;
 *	we can calculate the others if necessary.
 *	(However, if PAGE_SHIFT really refers to a variable,
 *	PAGE_SIZE and PAGE_MASK should also be variables
 *	so their values don't have to be constantly recomputed.)
 */
#ifndef PAGE_SHIFT
#error mach/machine/vm_param.h needs to define PAGE_SHIFT.
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE (1 << PAGE_SHIFT)
#endif

#ifndef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE-1)
#endif

/*
 *	Convert addresses to pages and vice versa.
 *	No rounding is used.
 */

#define atop(x)		(((vm_size_t)(x)) >> PAGE_SHIFT)
#define ptoa(x)		((vm_offset_t)((x) << PAGE_SHIFT))

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.
 */

#define round_page(x)	((vm_offset_t)((((vm_offset_t)(x)) + PAGE_MASK) & ~PAGE_MASK))
#define trunc_page(x)	((vm_offset_t)(((vm_offset_t)(x)) & ~PAGE_MASK))

/*
 *	Determine whether an address is page-aligned, or a count is
 *	an exact page multiple.
 */

#define	page_aligned(x)	((((vm_offset_t) (x)) & PAGE_MASK) == 0)

#endif	/* _MACH_VM_PARAM_H_ */
