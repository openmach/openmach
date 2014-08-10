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
#include "i386_asm.h"
#include "pc_asm.h"
#include "trap_asm.h"

	.text
	.globl	rv86_real_int_asm
rv86_real_int_asm:

	/* Stash our protected-mode stack pointer.  */
	movl	%esp,EXT(real_tss)+TSS_ESP0

	/* Load the linear/physical-address data segment into ES,
	   for easy access to real-mode memory.  */
	movl	$LINEAR_DS,%edx
	movw	%dx,%es

	/* Find the physical address of the real-mode interrupt stack (es:ebx).
	   A 6-byte stack frame has already been allocated on it.  */
	movl	EXT(rv86_usp)+4,%ebx
	shll	$4,%ebx
	addl	EXT(rv86_usp),%ebx

	/* Store the magic return pointer into the real-mode interrupt stack frame.  */
	movl	EXT(rv86_rp),%edx
	movl	%edx,%es:(%ebx)
	movw	RCD_FLAGS(%eax),%dx
	movw	%dx,%es:4(%ebx)

	/* Find the address of the real mode interrupt vector (es:esi).  */
	shll	$2,%esi

	/* Build the v86 trap frame.  */
	xorl	%edx,%edx
	movw	RCD_GS(%eax),%dx
	pushl	%edx
	movw	RCD_FS(%eax),%dx
	pushl	%edx
	movw	RCD_DS(%eax),%dx
	pushl	%edx
	movw	RCD_ES(%eax),%dx
	pushl	%edx
	pushl	EXT(rv86_usp)+4
	pushl	EXT(rv86_usp)
	movl	$EFL_VM+EFL_IOPL_USER,%ecx
	orw	RCD_FLAGS(%eax),%cx
	andl	$-1-EFL_IF-EFL_TF,%ecx
	pushl	%ecx
	movw	%es:2(%esi),%edx
	pushl	%edx
	movw	%es:(%esi),%edx
	pushl	%edx

	/* Load the requested register state.  */
	movl	RCD_EDI(%eax),%edi
	movl	RCD_ESI(%eax),%esi
	movl	RCD_EBP(%eax),%ebp
	movl	RCD_EBX(%eax),%ebx
	movl	RCD_EDX(%eax),%edx
	movl	RCD_ECX(%eax),%ecx
	movl	RCD_EAX(%eax),%eax

	/* Drop into v86 mode.  */
	iret

ENTRY(rv86_return)

	/* Restore the kernel segment registers.  */
	movw	%ss,%ax
	movw	%ax,%ds
	movw	%ax,%es

	/* Retrieve the real_call_data pointer from rv86_real_int_asm's stack frame.  */
	movl	TR_V86SIZE+4(%esp),%eax

	/* Stash the final register state.  */
	movl	TR_EDI(%esp),%edx;	movl	%edx,RCD_EDI(%eax)
	movl	TR_ESI(%esp),%edx;	movl	%edx,RCD_ESI(%eax)
	movl	TR_EBP(%esp),%edx;	movl	%edx,RCD_EBP(%eax)
	movl	TR_EBX(%esp),%edx;	movl	%edx,RCD_EBX(%eax)
	movl	TR_EDX(%esp),%edx;	movl	%edx,RCD_EDX(%eax)
	movl	TR_ECX(%esp),%edx;	movl	%edx,RCD_ECX(%eax)
	movl	TR_EAX(%esp),%edx;	movl	%edx,RCD_EAX(%eax)
	movl	TR_EFLAGS(%esp),%edx;	movw	%dx,RCD_FLAGS(%eax)
	movl	TR_V86_ES(%esp),%edx;	movw	%dx,RCD_ES(%eax)
	movl	TR_V86_DS(%esp),%edx;	movw	%dx,RCD_DS(%eax)
	movl	TR_V86_FS(%esp),%edx;	movw	%dx,RCD_FS(%eax)
	movl	TR_V86_GS(%esp),%edx;	movw	%dx,RCD_GS(%eax)

	/* Return from the call to rv86_real_int_asm.  */
	lea	TR_V86SIZE(%esp),%esp
	ret

