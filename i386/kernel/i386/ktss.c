/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
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
 * Kernel task state segment.
 *
 * We don't use the i386 task switch mechanism.  We need a TSS
 * only to hold the kernel stack pointer for the current thread.
 *
 * XXX multiprocessor??
 */
#include "vm_param.h"
#include "seg.h"
#include "gdt.h"
#include "ktss.h"

void
ktss_init()
{
	/* XXX temporary exception stack */
	static exception_stack[1024];

	/* Initialize the master TSS descriptor.  */
	fill_gdt_descriptor(KERNEL_TSS,
			    kvtolin(&ktss), sizeof(ktss)+65536/8+1-1,
			    ACC_PL_K|ACC_TSS, 0);

	/* Initialize the master TSS.  */
	ktss.ss0 = KERNEL_DS;
	ktss.esp0 = (unsigned)(exception_stack+1024);
	ktss.io_bit_map_offset = sizeof(ktss);

	/* Set the last byte in the I/O bitmap to all 1's.  */
	((unsigned char*)&ktss)[sizeof(ktss)+65536/8] = 0xff;

	/* Load the TSS.  */
	ltr(KERNEL_TSS);
}

