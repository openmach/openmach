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

#include <mach/machine/code16.h>
#include <mach/machine/seg.h>

#include "vm_param.h"
#include "cpu.h"


CODE16

/* This 16-bit function initializes CPU 0's GDT
   just enough to get in and out of protected (and possibly paged) mode,
   with all addresses assuming identity-mapped memory.  */
void i16_gdt_init_temp()
{
	/* Create temporary kernel code and data segment descriptors.
	   (They'll be reinitialized later after paging is enabled.)  */
	i16_fill_gdt_descriptor(&cpu[0], KERNEL_CS,
				boot_image_pa, 0xffffffff,
				ACC_PL_K|ACC_CODE_R, SZ_32);
	i16_fill_gdt_descriptor(&cpu[0], KERNEL_DS,
				boot_image_pa, 0xffffffff,
				ACC_PL_K|ACC_DATA_W, SZ_32);
	i16_fill_gdt_descriptor(&cpu[0], KERNEL_16_CS,
				boot_image_pa, 0xffff,
				ACC_PL_K|ACC_CODE_R, SZ_16);
	i16_fill_gdt_descriptor(&cpu[0], KERNEL_16_DS,
				boot_image_pa, 0xffff,
				ACC_PL_K|ACC_DATA_W, SZ_16);
}

