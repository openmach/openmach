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
/*
 *	File:	machine/thread.h
 *
 *	This file contains the structure definitions for the thread
 *	state as applied to I386 processors.
 */

#ifndef	_I386_THREAD_H_
#define _I386_THREAD_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/fp_reg.h>

#include <kern/lock.h>

#include <i386/iopb.h>
#include <i386/tss.h>

/*
 *	i386_saved_state:
 *
 *	This structure corresponds to the state of user registers
 *	as saved upon kernel entry.  It lives in the pcb.
 *	It is also pushed onto the stack for exceptions in the kernel.
 */

struct i386_saved_state {
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	cr2;		/* kernel esp stored by pusha -
					   we save cr2 here later */
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;
	unsigned int	trapno;
	unsigned int	err;
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	efl;
	unsigned int	uesp;
	unsigned int	ss;
	struct v86_segs {
	    unsigned int v86_es;	/* virtual 8086 segment registers */
	    unsigned int v86_ds;
	    unsigned int v86_fs;
	    unsigned int v86_gs;
	} v86_segs;
};

/*
 *	i386_exception_link:
 *
 *	This structure lives at the high end of the kernel stack.
 *	It points to the current thread`s user registers.
 */
struct i386_exception_link {
	struct i386_saved_state *saved_state;
};

/*
 *	i386_kernel_state:
 *
 *	This structure corresponds to the state of kernel registers
 *	as saved in a context-switch.  It lives at the base of the stack.
 */

struct i386_kernel_state {
	int			k_ebx;	/* kernel context */
	int			k_esp;
	int			k_ebp;
	int			k_edi;
	int			k_esi;
	int			k_eip;
};

/*
 *	Save area for user floating-point state.
 *	Allocated only when necessary.
 */

struct i386_fpsave_state {
	boolean_t		fp_valid;
	struct i386_fp_save	fp_save_state;
	struct i386_fp_regs	fp_regs;
};

/*
 *	v86_assist_state:
 *
 *	This structure provides data to simulate 8086 mode
 *	interrupts.  It lives in the pcb.
 */

struct v86_assist_state {
	vm_offset_t		int_table;
	unsigned short		int_count;
	unsigned short		flags;	/* 8086 flag bits */
};
#define	V86_IF_PENDING		0x8000	/* unused bit */

/*
 *	i386_interrupt_state:
 *
 *	This structure describes the set of registers that must
 *	be pushed on the current ring-0 stack by an interrupt before
 *	we can switch to the interrupt stack.
 */

struct i386_interrupt_state {
	int	es;
	int	ds;
	int	edx;
	int	ecx;
	int	eax;
	int	eip;
	int	cs;
	int	efl;
};

/*
 *	i386_machine_state:
 *
 *	This structure corresponds to special machine state.
 *	It lives in the pcb.  It is not saved by default.
 */

struct i386_machine_state {
	iopb_tss_t		io_tss;
	struct user_ldt	*	ldt;
	struct i386_fpsave_state *ifps;
	struct v86_assist_state	v86s;
};

typedef struct pcb {
	struct i386_interrupt_state iis[2];	/* interrupt and NMI */
	struct i386_saved_state iss;
	struct i386_machine_state ims;
	decl_simple_lock_data(, lock)
} *pcb_t;

/*
 *	On the kernel stack is:
 *	stack:	...
 *		struct i386_exception_link
 *		struct i386_kernel_state
 *	stack+KERNEL_STACK_SIZE
 */

#define STACK_IKS(stack)	\
	((struct i386_kernel_state *)((stack) + KERNEL_STACK_SIZE) - 1)
#define STACK_IEL(stack)	\
	((struct i386_exception_link *)STACK_IKS(stack) - 1)

#define USER_REGS(thread)	(&(thread)->pcb->iss)


#define syscall_emulation_sync(task)	/* do nothing */


#include_next "thread.h"


#endif	_I386_THREAD_H_
