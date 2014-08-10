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
/*
 * Global descriptor table.
 */
#include <mach/machine/vm_types.h>

#include <platforms.h>

#include "vm_param.h"
#include "seg.h"
#include "gdt.h"

#if     PS2
extern unsigned long abios_int_return;
extern unsigned long abios_th_return;
extern char intstack[];
#endif  /* PS2 */

struct real_descriptor gdt[GDTSZ];

void
gdt_init()
{
	/* Initialize the kernel code and data segment descriptors.  */
	fill_gdt_descriptor(KERNEL_CS,
			    LINEAR_MIN_KERNEL_ADDRESS, 
			    LINEAR_MAX_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS - 1,
			    ACC_PL_K|ACC_CODE_R, SZ_32);
	fill_gdt_descriptor(KERNEL_DS,
			    LINEAR_MIN_KERNEL_ADDRESS, 
			    LINEAR_MAX_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS - 1,
			    ACC_PL_K|ACC_DATA_W, SZ_32);

	/* Load the new GDT.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = sizeof(gdt)-1;
		pdesc.linear_base = kvtolin(&gdt);
		lgdt(&pdesc);
	}

	/* Reload all the segment registers from the new GDT.
	   We must load ds and es with 0 before loading them with KERNEL_DS
	   because some processors will "optimize out" the loads
	   if the previous selector values happen to be the same.  */
	asm volatile("
		ljmp	%0,$1f
	1:
		movw	%w2,%%ds
		movw	%w2,%%es
		movw	%w2,%%fs
		movw	%w2,%%gs

		movw	%w1,%%ds
		movw	%w1,%%es
		movw	%w1,%%ss
	" : : "i" (KERNEL_CS), "r" (KERNEL_DS), "r" (0));
}

