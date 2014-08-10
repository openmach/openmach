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
#include "real_tss.h"
#include "vm_param.h"
#include "config.h"

#ifdef ENABLE_REAL_TSS

static void real_tss_init()
{
	/* Only initialize once.  */
	if (!real_tss.ss0)
	{
		/* Initialize the real-mode TSS.  */
		real_tss.ss0 = KERNEL_DS;
		real_tss.esp0 = get_esp();
		real_tss.io_bit_map_offset = sizeof(real_tss);

		/* Set the last byte in the I/O bitmap to all 1's.  */
		((unsigned char*)&real_tss)[REAL_TSS_SIZE] = 0xff;
	}
}

void
cpu_gdt_init_REAL_TSS(struct cpu *cpu)
{
	real_tss_init();

	fill_gdt_descriptor(cpu, REAL_TSS,
			    kvtolin(&real_tss), REAL_TSS_SIZE-1,
			    ACC_PL_K|ACC_TSS, 0);
}

#endif ENABLE_REAL_TSS
