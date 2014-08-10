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

#include <mach/machine/asm.h>
#include <mach/machine/eflags.h>

#include "i386_asm.h"
#include "trap.h"
#include "trap_asm.h"

	.text

ENTRY(rv86_trap_handler)
	cmpl	$T_GENERAL_PROTECTION,TR_TRAPNO(%esp)
	jz	gpf_from_v86

	UNEXPECTED_TRAP

gpf_from_v86:

	/* Load the linear/physical-address data segment,
	   for easy access to real-mode memory.  */
	movl	$LINEAR_DS,%eax
	movw	%ax,%ds

	/* Find the physical address of the trapping instruction (ebx).  */
	movzwl	TR_CS(%esp),%ebx
	shll	$4,%ebx
	addl	TR_EIP(%esp),%ebx

	/* See if we're just trying to get out of v86 mode.  */
	cmpl	%ss:EXT(rv86_return_int_pa),%ebx
	je	EXT(rv86_return)

	/* Check the instruction (al).  */
	movb	(%ebx),%al
	cmpb	$0xcd,%al
	je	gpf_int_n

	UNEXPECTED_TRAP


gpf_int_n:

	/* Bump the ip past the int instruction.  */
	addw	$2,TR_EIP(%esp)

	/* Find the real mode interrupt vector number (esi).  */
	movzbl	1(%ebx),%esi

	/* See if it's a copy-extended-memory interrupt request;
	   if so, just handle it here.  */
	cmpl	$0x15,%esi
	jne	1f
	cmpb	$0x87,TR_EAX+1(%esp)
	je	copy_ext_mem
1:

	/* XXX The stuff down here is essentially the same as in moss.  */

reflect_v86_intr:

	/* Find the address of the real mode interrupt vector (esi).  */
	shll	$2,%esi

	/* Make room for the real-mode interrupt stack frame.  */
	subw	$6,TR_ESP(%esp)

	/* Find the physical address of the v86 stack (ebx).  */
	movzwl	TR_SS(%esp),%ebx
	shll	$4,%ebx
	addl	TR_ESP(%esp),%ebx

	/* Store the return information into the v86 stack frame.  */
	movl	TR_EIP(%esp),%eax
	movw	%ax,(%ebx)
	movl	TR_CS(%esp),%eax
	movw	%ax,2(%ebx)
	movl	TR_EFLAGS(%esp),%eax
	movw	%ax,4(%ebx)

	/* Find the real-mode interrupt vector to invoke,
	   and set up the real_call_thread's kernel stack frame
	   to point to it.  */
	movl	(%esi),%eax
	movw	%ax,TR_EIP(%esp)
	shrl	$16,%eax
	movw	%ax,TR_CS(%esp)
	andl	$-1-EFL_IF-EFL_TF,TR_EFLAGS(%esp)

	/* Restore saved state and return.  */
	addl	$4*4,%esp
	popa
	addl	$4*2,%esp
	iret



/* We intercepted a copy-extended-memory software interrupt
   (int 0x15 function 0x87).
   This is used by HIMEM.SYS, for example, to manage extended memory.
   The BIOS's routine isn't going to work in v86 mode,
   so do it ourselves.  */
copy_ext_mem:

	/* Find the parameter block provided by the caller (ebx).  */
	movzwl	TR_V86_ES(%esp),%ebx
	movzwl	TR_ESI(%esp),%eax
	shll	$4,%ebx
	addl	%eax,%ebx

	/* Source address (esi).  */
	movl	0x12(%ebx),%esi
	andl	$0x00ffffff,%esi

	/* Destination address (edi).  */
	movl	0x1a(%ebx),%edi
	andl	$0x00ffffff,%edi

	/* Number of bytes (ecx).  */
	movzwl	TR_ECX(%esp),%ecx
	addl	%ecx,%ecx

	/* Use the standard i386 bcopy routine to copy the data.
	   This assumes it's "friendly" in its use of segment registers
	   (i.e. always uses ss for stack data and ds/es for the data to copy).
	   The bcopy is simple enough that this should always be true.  */
	movw	%ds,%ax
	movw	%ax,%es
	cld
	pushl	%ecx
	pushl	%edi
	pushl	%esi
	call	EXT(bcopy)
	addl	$3*4,%esp

	/* Clear the carry flag to indicate that the copy was successful.
	   AH is also cleared, below.  */
	andl	$-1-EFL_CF,TR_EFLAGS(%esp)

	/* Restore saved state and return.  */
	addl	$4*4,%esp
	popa
	xorb	%ah,%ah
	addl	$4*2,%esp
	iret

