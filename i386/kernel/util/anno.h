/* 
 * Copyright (c) 1995-1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _I386_KERNEL_UTIL_ANNO_H_
#define _I386_KERNEL_UTIL_ANNO_H_

#ifndef ASSEMBLER


struct anno_table
{
	struct anno_entry	*start;
	struct anno_entry	*end;
};

struct anno_entry
{
	int			val1;
	int			val2;
	int			val3;
	struct anno_table	*table;
};


#else /* ASSEMBLER */


/* Create an arbitrary annotation entry.
   Must switch back to an appropriate segment afterward.  */
#define ANNO_ENTRY(table, val1, val2, val3)	\
	.section .anno,"aw",@progbits		;\
	.long	val1,val2,val3,table

/* Create an annotation entry for code in a text segment.  */
#define ANNO_TEXT(table, val2, val3)		\
9:	ANNO_ENTRY(table, 9b, val2, val3)	;\
	.text



/* The following are for common annotation tables.
   These don't have to be used in any given kernel,
   and others can be defined as convenient.  */


/* The anno_intr table is generally accessed
   on hardware interrupts that occur while running in kernel mode.
   The value is a routine for the trap handler in interrupt.S
   to jump to before processing the hardware interrupt.
   This routine applies to all code from this address
   up to but not including the address of the next ANNO_INTR.
   To disable interrupt redirection for a piece of code,
   place an ANNO_INTR(0) before it.  */

#define ANNO_INTR(routine) \
	ANNO_TEXT(anno_intr, routine, 0)


/* The anno_trap table is accessed
   on processor traps that occur in kernel mode.
   If a match is found in this table,
   the specified alternate handler is run instead of the generic handler.
   A match is found only if the EIP exactly matches an ANNO_TRAP entry
   (i.e. these entries apply to individual instructions, not groups),
   and if the trap type that occurred matches the type specified.  */

#define ANNO_TRAP(type, routine) \
	ANNO_TEXT(anno_trap, type, routine)


#endif /* ASSEMBLER */

#endif _I386_KERNEL_UTIL_ANNO_H_
