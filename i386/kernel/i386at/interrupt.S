/* 
 * Copyright (c) 1995 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

#include <mach/machine/asm.h>

#include "ipl.h"
#include "pic.h"
#include "i386asm.h"

#define READ_ISR	(OCW_TEMPLATE|READ_NEXT_RD|READ_IS_ONRD)

/*
 * Generic interrupt handler.
 *
 * On entry, %eax contains the irq number.
 */
ENTRY(interrupt)
	movl	%eax,%ecx		/* save irq number */
	movb	$(NON_SPEC_EOI),%al	/* non-specific EOI */
	outb	%al,$(PIC_MASTER_ICW)	/* ack interrupt to master */
	cmpl	$8,%ecx			/* do we need to ack slave? */
	jl	1f			/* no, skip it */
	outb	%al,$(PIC_SLAVE_ICW)
1:
	shll	$2,%ecx			/* irq * 4 */
	movl	EXT(intpri)(%ecx),%edx	/* get new ipl */
	call	spl			/* set ipl */
	movl	EXT(iunit)(%ecx),%edx	/* get device unit number */
	pushl	%eax			/* push previous ipl */
	pushl	%edx			/* push unit number */
	call	*EXT(ivect)(%ecx)	/* call interrupt handler */
	addl	$4,%esp			/* pop unit number */
	call	splx_cli		/* restore previous ipl */
	cli				/* XXX no more nested interrupts */
	addl	$4,%esp			/* pop previous ipl */
	ret				/* return */
