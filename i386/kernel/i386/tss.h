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

#ifndef	_I386_TSS_H_
#define	_I386_TSS_H_

#include <mach/inline.h>

/*
 *	i386 Task State Segment
 */
struct i386_tss {
	int		back_link;	/* segment number of previous task,
					   if nested */
	int		esp0;		/* initial stack pointer ... */
	int		ss0;		/* and segment for ring 0 */
	int		esp1;		/* initial stack pointer ... */
	int		ss1;		/* and segment for ring 1 */
	int		esp2;		/* initial stack pointer ... */
	int		ss2;		/* and segment for ring 2 */
	int		cr3;		/* CR3 - page table directory
						 physical address */
	int		eip;
	int		eflags;
	int		eax;
	int		ecx;
	int		edx;
	int		ebx;
	int		esp;		/* current stack pointer */
	int		ebp;
	int		esi;
	int		edi;
	int		es;
	int		cs;
	int		ss;		/* current stack segment */
	int		ds;
	int		fs;
	int		gs;
	int		ldt;		/* local descriptor table segment */
	unsigned short	trace_trap;	/* trap on switch to this task */
	unsigned short	io_bit_map_offset;
					/* offset to start of IO permission
					   bit map */
};

/* Load the current task register.  */
MACH_INLINE void
ltr(unsigned short segment)
{
	__asm volatile("ltr %0" : : "r" (segment));
}

#endif	/* _I386_TSS_H_ */
