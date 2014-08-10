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

#ifndef	_KERN_SYSCALL_SW_H_
#define	_KERN_SYSCALL_SW_H_

/*
 *	mach_trap_stack indicates the trap may discard
 *	its kernel stack.  Some architectures may need
 *	to save more state in the pcb for these traps.
 */

typedef struct {
	int		mach_trap_arg_count;
	int		(*mach_trap_function)();
	boolean_t	mach_trap_stack;
	int		mach_trap_unused;
} mach_trap_t;

extern mach_trap_t	mach_trap_table[];
extern int		mach_trap_count;

#define	MACH_TRAP(name, arg_count)		\
		{ (arg_count), (int (*)()) (name), FALSE, 0 }
#define	MACH_TRAP_STACK(name, arg_count)	\
		{ (arg_count), (int (*)()) (name), TRUE, 0 }

#endif	_KERN_SYSCALL_SW_H_
