/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 * 	File: 	stack.c
 *	Author:	Eric Cooper, Carnegie Mellon University
 *	Date:	Dec, 1987
 *
 * 	C Thread stack allocation.
 *
 */

#include <mach/cthreads.h>
#include "cthread_internals.h"

#define	BYTES_TO_PAGES(b)	(((b) + vm_page_size - 1) / vm_page_size)

vm_offset_t cthread_stack_mask;
vm_size_t cthread_stack_size;
vm_address_t cthread_stack_base;
private vm_address_t next_stack_base;

/*
 * Set up a stack segment for a thread.
 * Segment has a red zone (invalid page)
 * for early detection of stack overflow.
 * The cproc_self pointer is stored at the top.
 *
 *	--------- (high address)
 *	| self	|
 *	|  ...	|
 *	|	|
 *	| stack	|
 *	|	|
 *	|  ...	|
 *	|	|
 *	---------
 *	|	|
 *	|invalid|
 *	|	|
 *	--------- (stack base)
 *	--------- (low address)
 *
 * or the reverse, if the stack grows up.
 */

private void
setup_stack(p, base)
	register cproc_t p;
	register vm_address_t base;
{
#if	defined(RED_ZONE)
	register kern_return_t r;
#endif	/* defined(RED_ZONE) */

	p->stack_base = base;
	/*
	 * Stack size is segment size minus size of self pointer
	 */
	p->stack_size = cthread_stack_size;
	/*
	 * Protect red zone.
	 */
#if	defined(RED_ZONE)
#if	defined(STACK_GROWTH_UP)
	MACH_CALL(vm_protect(mach_task_self(),
			     base + cthread_stack_size - vm_page_size, 
			     vm_page_size, FALSE, VM_PROT_NONE), r);
#else
	MACH_CALL(vm_protect(mach_task_self(),
			     base + vm_page_size,
			     vm_page_size, FALSE, VM_PROT_NONE), r);
#endif	/* defined(STACK_GROWTH_UP) */
#endif	/* defined(RED_ZONE) */
	/*
	 * Store self pointer.
	 */
	*(cproc_t *)&ur_cthread_ptr(base) = p;
}

private vm_offset_t
addr_range_check(vm_offset_t start_addr, vm_offset_t end_addr,
		 vm_prot_t desired_protection)
{
	register vm_offset_t	addr;

	addr = start_addr;
	while (addr < end_addr) {
	    vm_offset_t		r_addr;
	    vm_size_t		r_size;
	    vm_prot_t		r_protection,
				r_max_protection;
	    vm_inherit_t	r_inheritance;
	    boolean_t		r_is_shared;
	    memory_object_name_t	r_object_name;
	    vm_offset_t		r_offset;
	    kern_return_t	kr;

	    r_addr = addr;
	    kr = vm_region(mach_task_self(), &r_addr, &r_size,
			   &r_protection, &r_max_protection, &r_inheritance,
			   &r_is_shared, &r_object_name, &r_offset);
	    if ((kr == KERN_SUCCESS) && MACH_PORT_VALID(r_object_name))
		(void) mach_port_deallocate(mach_task_self(), r_object_name);

	    if ((kr != KERN_SUCCESS) ||
		(r_addr > addr) ||
		((r_protection & desired_protection) != desired_protection))
		return (0);
	    addr = r_addr + r_size;
	}
	return (addr);
}

/*
 * Probe for bottom and top of stack.
 * Assume:
 * 1. stack grows DOWN
 * 2. There is an unallocated region below the stack.
 */
private void
probe_stack(vm_offset_t *stack_bottom, vm_offset_t *stack_top)
{
	/*
	 * Since vm_region returns the region starting at
	 * or ABOVE the given address, we cannot use it
	 * directly to search downwards.  However, we
	 * also want a size that is the closest power of
	 * 2 to the stack size (so we can mask off the stack
	 * address and get the stack base).  So we probe
	 * in increasing powers of 2 until we find a gap
	 * in the stack.
	 */
	vm_offset_t	start_addr, end_addr;
	vm_offset_t	last_start_addr, last_end_addr;
	vm_size_t	stack_size;

#ifdef parisc
	/* XXX fixme */
	*stack_bottom = cthread_sp() & ~(cthread_stack_size - 1);
	*stack_top = *stack_bottom + cthread_stack_size;
#else
	/*
	 * Start with a page
	 */
	start_addr = cthread_sp() & ~(vm_page_size - 1);
	end_addr   = start_addr + vm_page_size;

	stack_size = vm_page_size;

	/*
	 * Increase the tentative stack size, by doubling each
	 * time, until we have exceeded the stack (some of the
	 * range is not valid).
	 */
	do {
	    /*
	     * Save last addresses
	     */
	    last_start_addr = start_addr;
	    last_end_addr   = end_addr;

	    /*
	     * Double the stack size
	     */
	    stack_size <<= 1;
	    start_addr = end_addr - stack_size;

	    /*
	     * Check that the entire range exists and is writable
	     */
	} while ((end_addr = addr_range_check(start_addr, end_addr,
					      VM_PROT_READ|VM_PROT_WRITE)));
	/*
	 * Back off to previous power of 2.
	 */
	*stack_bottom = last_start_addr;
	*stack_top = last_end_addr;
#endif
}

vm_offset_t
stack_init(cproc_t p)
{
	vm_offset_t	stack_bottom,
			stack_top,
			start;
	vm_size_t	size;
	kern_return_t	r;


	/*
	 * Probe for bottom and top of stack, as a power-of-2 size.
	 */
	probe_stack(&stack_bottom, &stack_top);

	/*
	 * Use the stack size found for the Cthread stack size,
	 * if not already specified.
	 */
	if (cthread_stack_size == 0)
	    cthread_stack_size = stack_top - stack_bottom;
#if	defined(STACK_GROWTH_UP)
	cthread_stack_mask = ~(cthread_stack_size - 1);
#else	/* not defined(STACK_GROWTH_UP) */
	cthread_stack_mask = cthread_stack_size - 1;
#endif	/* defined(STACK_GROWTH_UP) */

	/*
	 * Guess at first available region for stack.
	 */
	next_stack_base = cthread_stack_base;

	/*
	 * Set up stack for main thread.
	 */
	alloc_stack(p);

	/*
	 * Delete rest of old stack.
	 */

#if	defined(STACK_GROWTH_UP)
	start = (cthread_sp() | (vm_page_size - 1)) + 1 + vm_page_size;
	size = stack_top - start;
#else	/* not defined(STACK_GROWTH_UP) */
	start = stack_bottom;
	size = (cthread_sp() & ~(vm_page_size - 1)) - stack_bottom - 
	       vm_page_size;
#endif	/* defined(STACK_GROWTH_UP) */
	MACH_CALL(vm_deallocate(mach_task_self(),start,size),r);

	/*
	 * Return new stack; it gets passed back to the caller
	 * of cthread_init who must switch to it.
	 */
	return cproc_stack_base(p, sizeof(ur_cthread_t *));
}

/*
 * Allocate a stack segment for a thread.
 * Stacks are never deallocated.
 *
 * The variable next_stack_base is used to align stacks.
 * It may be updated by several threads in parallel,
 * but mutual exclusion is unnecessary: at worst,
 * the vm_allocate will fail and the thread will try again.
 */

void
alloc_stack(cproc_t p)
{
	vm_address_t base = next_stack_base;

	for (base = next_stack_base;
	     vm_allocate(mach_task_self(), &base, cthread_stack_size, FALSE) != KERN_SUCCESS;
	     base += cthread_stack_size)
		;
	next_stack_base = base + cthread_stack_size;
	setup_stack(p, base);
}

vm_offset_t
cproc_stack_base(cproc, offset)
	register cproc_t cproc;
	register int offset;
{
#if	defined(STACK_GROWTH_UP)
	return (cproc->stack_base + offset);
#else	/* not defined(STACK_GROWTH_UP) */
	return (cproc->stack_base + cproc->stack_size - offset);
#endif	/* defined(STACK_GROWTH_UP) */

}

void stack_fork_child()
/*
 * Called in the child after a fork().  Resets stack data structures to
 * coincide with the reality that we now have a single cproc and cthread.
 */
{
    next_stack_base = cthread_stack_base;
}
