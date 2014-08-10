/* 
 * Copyright (c) 1995 The University of Utah and
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

#include <mach/machine/asm.h>
#include <mach/machine/eflags.h>

#include "trap.h"
#include "trap_asm.h"
#include "pc_asm.h"
#include "i386_asm.h"

/*
 * Define a set of interrupt handlers to reflect interrupts to v86 mode.
 */

	.text

#define master_base	0x08
#define slave_base	0x70

#define irq(pic,picnum,irqnum)				\
ENTRY(rv86_reflect_irq##irqnum)				;\
	pushl	$pic##_base+picnum			;\
	pushl	$0x80000000+irqnum /* (for debug) */	;\
	jmp	allintrs

#include "irq_list.h"


allintrs:
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs

	/* Load the normal kernel segment registers.  */
	movw	%ss,%ax
	movw	%ax,%ds
	movw	%ax,%es

	/* See if we came from v86 mode.  */
	testl	$EFL_VM,TR_EFLAGS(%esp)
	jnz	int_from_v86

	movl	TR_ERR(%esp),%eax
	pushl	$dummy_rcd
	pushl	%eax
	call	EXT(rv86_real_int)
	addl	$2*4,%esp

	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popa
	addl	$2*4,%esp
	iret

int_from_v86:

	/* Save the v86 stack pointer before handling the interrupt.
	   We need this in order to handle recursive reflected interrupts
	   possibly interspersed with protected-mode interrupts.  */
	movl	EXT(rv86_usp),%esi
	movl	EXT(rv86_usp)+4,%edi
	movl	EXT(real_tss)+TSS_ESP0,%ebx

	movl	TR_ESP(%esp),%eax
	subw	$6,%ax	/* allocate a real-mode interrupt stack frame.  */
	movl	%eax,EXT(rv86_usp)
	movl	TR_SS(%esp),%eax
	movw	%ax,EXT(rv86_usp)+4

	movl	TR_ERR(%esp),%eax
	pushl	$dummy_rcd
	pushl	%eax
	call	EXT(rv86_real_int)
	addl	$2*4,%esp

	movl	%esi,EXT(rv86_usp)
	movl	%edi,EXT(rv86_usp)+4
	movl	%ebx,EXT(real_tss)+TSS_ESP0

	addl	$4*4,%esp
	popa
	addl	$2*4,%esp
	iret

	/* Dummy real_call_data structure (always all zero)
	   to use when reflecting hardware interrupts.  */
	.comm	dummy_rcd,RCD_SIZE

