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
#ifndef _I386_UTIL_CPU_H_
#define _I386_UTIL_CPU_H_

#include <mach/machine/tss.h>

#include "config.h"
#include "gdt.h"
#include "ldt.h"
#include "idt.h"

/*
 * Multiprocessor i386/i486 systems use a separate copy of the
 * GDT, IDT, LDT, and kernel TSS per processor.  The first three
 * are separate to avoid lock contention: the i386 uses locked
 * memory cycles to access the descriptor tables.  The TSS is
 * separate since each processor needs its own kernel stack,
 * and since using a TSS marks it busy.
 */

/* This structure holds the processor tables for this cpu.  */
struct cpu_tables
{
	struct i386_gate	idt[IDTSZ];
	struct i386_descriptor	gdt[GDTSZ];
#ifdef ENABLE_KERNEL_LDT
	struct i386_descriptor	ldt[LDTSZ];
#endif
#ifdef ENABLE_KERNEL_TSS
	struct i386_tss		tss;
#endif
};

#include_next "cpu.h"

#endif _I386_UTIL_CPU_H_
