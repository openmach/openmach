/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
#ifndef _I386_UTIL_IDT_INITTAB_H_



/* We'll be using macros to fill in a table in data hunk 2
   while writing trap entrypoint routines at the same time.
   Here's the header that comes before everything else.  */
#define IDT_INITTAB_BEGIN	\
	.data	2		;\
ENTRY(idt_inittab)		;\
	.text

/*
 * Interrupt descriptor table and code vectors for it.
 */
#define	IDT_ENTRY(n,entry,type) \
	.data	2		;\
	.long	entry		;\
	.word	n		;\
	.word	type		;\
	.text

/*
 * Terminator for the end of the table.
 */
#define IDT_INITTAB_END		\
	.data	2		;\
	.long	0		;\
	.text


#endif _I386_UTIL_IDT_INITTAB_H_
