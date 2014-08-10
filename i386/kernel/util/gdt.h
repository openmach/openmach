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
#ifndef _I386_GDT_
#define _I386_GDT_

#include <mach/machine/seg.h>

/*
 * Collect and define the GDT segment selectors.
 * xxx_IDX is the index number of the selector;
 * xxx is the actual selector value (index * 8).
 */
enum gdt_idx
{
	GDT_NULL_IDX = 0,

#define gdt_sel(name) name##_IDX,
#include "gdt_sels.h"
#undef gdt_sel

	GDT_FIRST_FREE_IDX
};

enum gdt_sel
{
	GDT_NULL = 0,

#define gdt_sel(name) name = name##_IDX * 8,
#include "gdt_sels.h"
#undef gdt_sel
};

#define GDTSZ GDT_FIRST_FREE_IDX


/* If we have a KERNEL_TSS, use that as our DEFAULT_TSS if none is defined yet.
   (The DEFAULT_TSS gets loaded by cpu_tables_load() upon switching to pmode.)
   Similarly with DEFAULT_LDT.  */
#if defined(ENABLE_KERNEL_TSS) && !defined(DEFAULT_TSS)
#define DEFAULT_TSS	KERNEL_TSS
#define DEFAULT_TSS_IDX	KERNEL_TSS_IDX
#endif
#if defined(ENABLE_KERNEL_LDT) && !defined(DEFAULT_LDT)
#define DEFAULT_LDT	KERNEL_LDT
#define DEFAULT_LDT_IDX	KERNEL_LDT_IDX
#endif


/* Fill a segment descriptor in a CPU's GDT.  */
#define fill_gdt_descriptor(cpu, segment, base, limit, access, sizebits) \
	fill_descriptor(&(cpu)->tables.gdt[segment/8],			\
			base, limit, access, sizebits)

#define i16_fill_gdt_descriptor(cpu, segment, base, limit, access, sizebits) \
	i16_fill_descriptor(&(cpu)->tables.gdt[segment/8],		\
			    base, limit, access, sizebits)


/* This automatically defines GDT descriptor initialization functions.  */
#define gdt_desc_initializer(segment, base, limit, access, sizebits)	\
	void cpu_gdt_init_##segment(struct cpu *cpu)			\
	{								\
		fill_gdt_descriptor(cpu, segment, base, limit,		\
				    access, sizebits);			\
	}


#endif _I386_GDT_
