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
#include <mach/machine/vm_types.h>
#include <mach/machine/far_ptr.h>
#include <mach/machine/proc_reg.h>

#include "i16_bios.h"
#include "phys_mem.h"
#include "vm_param.h"
#include "debug.h"


static vm_offset_t ext_mem_phys_free_mem;
static vm_size_t ext_mem_phys_free_size;


CODE32

int ext_mem_collect(void)
{
	if (ext_mem_phys_free_mem)
	{
		phys_mem_add(ext_mem_phys_free_mem, ext_mem_phys_free_size);
		ext_mem_phys_free_mem = 0;
	}
}

CODE16

void i16_ext_mem_check()
{
	vm_offset_t ext_mem_top, ext_mem_bot;
	unsigned short ext_mem_k;

	/* Find the top of available extended memory.  */
	asm volatile("
		int	$0x15
		jnc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (ext_mem_k)
	  : "a" (0x8800));
	ext_mem_top = 0x100000 + (vm_offset_t)ext_mem_k * 1024;

	/* XXX check for >16MB memory using function 0xc7 */

	ext_mem_bot = 0x100000;

	/* Check for extended memory allocated bottom-up: method 1.
	   This uses the technique (and, loosely, the code)
	   described in the VCPI spec, version 1.0.  */
	if (ext_mem_top > ext_mem_bot)
	{
		asm volatile("
			pushw	%%es

			xorw	%%ax,%%ax
			movw	%%ax,%%es
			movw	%%es:0x19*4+2,%%ax
			movw	%%ax,%%es

			movw	$0x12,%%di
			movw	$7,%%cx
			rep
			cmpsb
			jne	1f

			xorl	%%edx,%%edx
			movb	%%es:0x2e,%%dl
			shll	$16,%%edx
			movw	%%es:0x2c,%%dx

		1:
			popw	%%es
		" : "=d" (ext_mem_bot)
		  : "d" (ext_mem_bot),
		    "S" ((unsigned short)(vm_offset_t)"VDISK V")
		  : "eax", "ecx", "esi", "edi");
	}
	i16_assert(ext_mem_bot >= 0x100000);

	/* Check for extended memory allocated bottom-up: method 2.
	   This uses the technique (and, loosely, the code)
	   described in the VCPI spec, version 1.0.  */
	if (ext_mem_top > ext_mem_bot)
	{
		struct {
			char pad1[3];
			char V;
			long DISK;
			char pad2[30-8];
			unsigned short addr;
		} buf;
		unsigned char rc;

		i16_assert(sizeof(buf) == 0x20);
		rc = i16_bios_copy_ext_mem(0x100000, kvtolin((vm_offset_t)&buf), sizeof(buf)/2);
		if ((rc == 0) && (buf.V == 'V') && (buf.DISK == 'DISK'))
		{
			vm_offset_t new_bot = (vm_offset_t)buf.addr << 10;
			i16_assert(new_bot > 0x100000);
			if (new_bot > ext_mem_bot)
				ext_mem_bot = new_bot;
		}
	}
	i16_assert(ext_mem_bot >= 0x100000);

	if (ext_mem_top > ext_mem_bot)
	{
		ext_mem_phys_free_mem = ext_mem_bot;
		ext_mem_phys_free_size = ext_mem_top - ext_mem_bot;

		/* We need to update phys_mem_max here
		   instead of just letting phys_mem_add() do it
		   when the memory is collected with phys_mem_collect(),
		   because VCPI initialization needs to know the top of physical memory
		   before phys_mem_collect() is called.
		   See i16_vcpi.c for the gross details.  */
		if (ext_mem_top > phys_mem_max)
			phys_mem_max = ext_mem_top;
	}
}

void i16_ext_mem_shutdown()
{
	/* We didn't actually allocate the memory,
	   so no need to deallocate it... */
}

