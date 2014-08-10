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

#include <mach/machine/proc_reg.h>

#include "cpu.h"
#include "vm_param.h"

void cpu_tables_load(struct cpu *cpu)
{
	struct pseudo_descriptor pdesc;

	/* Load the final GDT.
	   If paging is now on,
	   then this will point the processor to the GDT
	   at its new linear address in the kernel linear space.  */
	pdesc.limit = sizeof(cpu->tables.gdt)-1;
	pdesc.linear_base = kvtolin(&cpu->tables.gdt);
	set_gdt(&pdesc);

	/* Reload all the segment registers from the new GDT.  */
	asm volatile("
		ljmp	%0,$1f
	1:
	" : : "i" (KERNEL_CS));
	set_ds(KERNEL_DS);
	set_es(KERNEL_DS);
	set_fs(0);
	set_gs(0);
	set_ss(KERNEL_DS);

	/* Load the IDT.  */
	pdesc.limit = sizeof(cpu[0].tables.idt)-1;
	pdesc.linear_base = kvtolin(&cpu->tables.idt);
	set_idt(&pdesc);

#ifdef DEFAULT_LDT
	/* Load the default LDT.  */
	set_ldt(DEFAULT_LDT);
#endif

#ifdef DEFAULT_TSS
	/* Make sure it isn't marked busy.  */
	cpu->tables.gdt[DEFAULT_TSS_IDX].access &= ~ACC_TSS_BUSY;

	/* Load the default TSS.  */
	set_tr(DEFAULT_TSS);
#endif
}

