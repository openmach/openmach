/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
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
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#include "cpu.h"
#include "vm_param.h"


/* Initialize the 32-bit kernel code and data segment descriptors
   to point to the base of the kernel linear space region.  */
gdt_desc_initializer(KERNEL_CS,
		     kvtolin(0), 0xffffffff,
		     ACC_PL_K|ACC_CODE_R, SZ_32);
gdt_desc_initializer(KERNEL_DS,
		     kvtolin(0), 0xffffffff,
		     ACC_PL_K|ACC_DATA_W, SZ_32);

/* Initialize the 16-bit real-mode code and data segment descriptors. */
gdt_desc_initializer(KERNEL_16_CS,
		     kvtolin(0), 0xffff,
		     ACC_PL_K|ACC_CODE_R, SZ_16);
gdt_desc_initializer(KERNEL_16_DS,
		     kvtolin(0), 0xffff,
		     ACC_PL_K|ACC_DATA_W, SZ_16);

/* Initialize the linear-space data segment descriptor. */
gdt_desc_initializer(LINEAR_CS,
		     0, 0xffffffff,
		     ACC_PL_K|ACC_CODE_R, SZ_32);
gdt_desc_initializer(LINEAR_DS,
		     0, 0xffffffff,
		     ACC_PL_K|ACC_DATA_W, SZ_32);

/* Initialize the master LDT and TSS descriptors.  */
#ifdef ENABLE_KERNEL_LDT
gdt_desc_initializer(KERNEL_LDT,
		     kvtolin(&cpu->tables.ldt), sizeof(cpu->tables.ldt)-1,
		     ACC_PL_K|ACC_LDT, 0);
#endif
#ifdef ENABLE_KERNEL_TSS
gdt_desc_initializer(KERNEL_TSS,
		     kvtolin(&cpu->tables.tss), sizeof(cpu->tables.tss)-1,
		     ACC_PL_K|ACC_TSS, 0);
#endif


void cpu_gdt_init(struct cpu *cpu)
{
	/* Initialize all the selectors of the GDT.  */
#define gdt_sel(name) cpu_gdt_init_##name(cpu);
#include "gdt_sels.h"
#undef gdt_sel
}

