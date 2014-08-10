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

#include "cpu.h"
#include "idt.h"

void idt_irq_init()
{
	int i;

#define irq(pic,picnum,irqnum)								\
	{ extern void rv86_reflect_irq##irqnum();					\
	  fill_idt_gate(&cpu[0], irq_##pic##_base + picnum,				\
	  	(vm_offset_t)rv86_reflect_irq##irqnum, KERNEL_CS,			\
		ACC_PL_K|ACC_INTR_GATE);						\
	}
#include "irq_list.h"
#undef irq
}

