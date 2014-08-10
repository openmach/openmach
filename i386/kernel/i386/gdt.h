/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON, IBM, AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON, IBM, AND CSL DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#ifndef _I386_GDT_
#define _I386_GDT_

#include "seg.h"

/*
 * Kernel descriptors for Mach - 32-bit flat address space.
 */
#define	KERNEL_CS	0x08		/* kernel code */
#define	KERNEL_DS	0x10		/* kernel data */
#define	KERNEL_LDT	0x18		/* master LDT */
#define	KERNEL_TSS	0x20		/* master TSS (uniprocessor) */
#define	USER_LDT	0x28		/* place for per-thread LDT */
#define	USER_TSS	0x30		/* place for per-thread TSS
					   that holds IO bitmap */
#define	FPE_CS		0x38		/* floating-point emulator code */
#define	USER_FPREGS	0x40		/* user-mode access to saved
					   floating-point registers */

#ifdef PS2
#define ABIOS_INT_RET   0x48            /* 16 bit return selector for ABIOS */
#define ABIOS_TH_RET    0x50            /* 16 bit return selector for ABIOS */
#define	ABIOS_INT_SS	0x58		/* ABIOS interrupt stack selector */
#define	ABIOS_TH_SS	0x60		/* ABIOS current stack selector */
#define	ABIOS_FIRST_AVAIL_SEL \
			0x68		/* first selector for ABIOS
					   to allocate */
#define GDTSZ           0x300           /* size of gdt table */
#else   /* PS2 */
#define	GDTSZ		11
#endif  /* PS2 */


extern struct real_descriptor gdt[GDTSZ];

/* Fill a segment descriptor in the GDT.  */
#define fill_gdt_descriptor(segment, base, limit, access, sizebits) \
	fill_descriptor(&gdt[segment/8], base, limit, access, sizebits)

#endif _I386_GDT_
