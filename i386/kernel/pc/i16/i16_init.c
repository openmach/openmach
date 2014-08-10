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
#include <mach/machine/proc_reg.h>

#include "vm_param.h"


/* Code segment we originally had when we started in real mode.  */
unsigned short real_cs;

/* Virtual address of physical memory.  */
vm_offset_t phys_mem_va;

/* Physical address of start of boot image.  */
vm_offset_t boot_image_pa;

/* Upper limit of known physical memory.  */
vm_offset_t phys_mem_max;


CODE16

#include "i16_bios.h"

/* Called by i16_crt0 (or the equivalent)
   to set up our basic 16-bit runtime environment
   before calling i16_main().  */
void i16_init(void)
{
	/* Find our code/data/everything segment.  */
	real_cs = get_cs();

	/* Find out where in physical memory we got loaded.  */
	boot_image_pa = real_cs << 4;

	/* Find out where the bottom of physical memory is.
	   (We won't be able to directly use it for 32-bit accesses
	   until we actually get into 32-bit mode.)  */
	phys_mem_va = -boot_image_pa;

	/* The base of linear memory is at the same place,
	   at least until we turn paging on.  */
	linear_base_va = phys_mem_va;
}

