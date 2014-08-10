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

#ifndef	_MACH_I386_TRAP_H_
#define	_MACH_I386_TRAP_H_

/*
 * Hardware trap vectors for i386.
 */
#define	T_DIVIDE_ERROR		0
#define	T_DEBUG			1
#define	T_NMI			2		/* non-maskable interrupt */
#define	T_INT3			3		/* int 3 instruction */
#define	T_OVERFLOW		4		/* overflow test */
#define	T_OUT_OF_BOUNDS		5		/* bounds check */
#define	T_INVALID_OPCODE	6		/* invalid op code */
#define	T_NO_FPU		7		/* no floating point */
#define	T_DOUBLE_FAULT		8		/* double fault */
#define	T_FPU_FAULT		9
/*				10 */
#define	T_SEGMENT_NOT_PRESENT	11
#define	T_STACK_FAULT		12
#define	T_GENERAL_PROTECTION	13
#define	T_PAGE_FAULT		14
/*				15 */
#define	T_FLOATING_POINT_ERROR	16
#define	T_WATCHPOINT		17

/*
 * Page-fault trap codes.
 */
#define	T_PF_PROT		0x1		/* protection violation */
#define	T_PF_WRITE		0x2		/* write access */
#define	T_PF_USER		0x4		/* from user state */


#endif	_MACH_I386_TRAP_H_
