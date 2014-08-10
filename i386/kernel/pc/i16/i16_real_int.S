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

#include "pc_asm.h"

	.text
	.code16

ENTRY(i16_real_int)
	pushf
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp

	cli

	movl	6*4(%esp),%eax
	movb	%al,1f+1

	movl	7*4(%esp),%ebp
	movl	RCD_EAX(%ebp),%eax
	movl	RCD_EBX(%ebp),%ebx
	movl	RCD_ECX(%ebp),%ecx
	movl	RCD_EDX(%ebp),%edx
	movl	RCD_ESI(%ebp),%esi
	movl	RCD_EDI(%ebp),%edi
	movw	RCD_DS(%ebp),%ds
	movw	RCD_ES(%ebp),%es
	/* XXX flags */
	movl	RCD_EBP(%ebp),%ebp

1:	int	$0

	pushl	%ebp
	movl	8*4(%esp),%ebp
	popl	RCD_EBP(%ebp)

	movl	%eax,RCD_EAX(%ebp)
	movl	%ebx,RCD_EBX(%ebp)
	movl	%ecx,RCD_ECX(%ebp)
	movl	%edx,RCD_EDX(%ebp)
	movl	%esi,RCD_ESI(%ebp)
	movl	%edi,RCD_EDI(%ebp)
	movw	%ds,RCD_DS(%ebp)
	movw	%es,RCD_ES(%ebp)

	pushf
	popl	%eax
	movw	%ax,RCD_FLAGS(%ebp)

	movw	%ss,%ax
	movw	%ax,%ds
	movw	%ax,%es

	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	popf
	ret

