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

#include <mach/machine/tss.h>
#include <mach/machine/proc_reg.h>

#include "cpu.h"

#ifdef ENABLE_KERNEL_TSS

void
cpu_tss_init(struct cpu *cpu)
{
	/* Only initialize once.  */
	if (!cpu->tables.tss.ss0)
	{
		/* Initialize the master TSS.  */
		cpu->tables.tss.ss0 = KERNEL_DS;
		cpu->tables.tss.esp0 = get_esp(); /* only temporary */
		cpu->tables.tss.io_bit_map_offset = sizeof(cpu->tables.tss);
	}
}

#endif

