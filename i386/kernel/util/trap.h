/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1994 The University of Utah and
 * the Center for Software Science (CSS).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF
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
#ifndef _I386_MOSS_TRAP_H_
#define _I386_MOSS_TRAP_H_

#ifndef ASSEMBLER


/* This structure corresponds to the state of user registers
   as saved upon kernel trap/interrupt entry.
   As always, it is only a default implementation;
   a well-optimized microkernel will probably want to override it
   with something that allows better optimization.  */

struct trap_state {

	/* Saved segment registers */
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;

	/* PUSHA register state frame */
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	cr2;	/* we save cr2 over esp for page faults */
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;

	unsigned int	trapno;
	unsigned int	err;

	/* Processor state frame */
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	eflags;
	unsigned int	esp;
	unsigned int	ss;

	/* Virtual 8086 segment registers */
	unsigned int	v86_es;
	unsigned int	v86_ds;
	unsigned int	v86_fs;
	unsigned int	v86_gs;
};

/* The actual trap_state frame pushed by the processor
   varies in size depending on where the trap came from.  */
#define TR_KSIZE	((int)&((struct trap_state*)0)->esp)
#define TR_USIZE	((int)&((struct trap_state*)0)->v86_es)
#define TR_V86SIZE	sizeof(struct trap_state)


#else ASSEMBLER

#include <mach/machine/asm.h>

#define UNEXPECTED_TRAP				\
	movw	%ss,%ax				;\
	movw	%ax,%ds				;\
	movw	%ax,%es				;\
	movl	%esp,%eax			;\
	pushl	%eax				;\
	call	EXT(trap_dump_die)		;\


#endif ASSEMBLER

#include <mach/machine/trap.h>

#endif _I386_MOSS_TRAP_H_
