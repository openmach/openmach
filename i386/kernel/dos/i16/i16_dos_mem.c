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

#include "i16_dos.h"
#include "config.h"



/* These aren't static because vcpi and dpmi code need to grab DOS memory
   before we've switched to protected mode and memory has been collected.  */
vm_offset_t dos_mem_phys_free_mem;
vm_size_t dos_mem_phys_free_size;


CODE32

void dos_mem_collect(void)
{
	if (dos_mem_phys_free_mem)
	{
		phys_mem_add(dos_mem_phys_free_mem, dos_mem_phys_free_size);
		dos_mem_phys_free_mem = 0;
	}
}

CODE16

void i16_dos_mem_check()
{
	unsigned short paras = 0xf000;
	int dos_mem_seg;

	/* Allocate as big a chunk of memory as we can find.  */
	do
	{
		if (paras == 0)
			return;
		dos_mem_seg = i16_dos_alloc(&paras);
	}
	while (dos_mem_seg < 0);

	dos_mem_phys_free_mem = dos_mem_seg << 4;
	dos_mem_phys_free_size = paras << 4;

#ifdef ENABLE_CODE_CHECK
	i16_code_copy();
#endif
}


#ifdef ENABLE_CODE_CHECK

/* Big humongo kludge to help in finding random code-trashing bugs.
   We copy the entire text segment upon initialization,
   and then check it later as necessary.  */

#include <mach/machine/proc_reg.h>
#include "vm_param.h"

extern char etext[], i16_entry_2[];

static int code_copy_seg;

static int i16_code_copy()
{
	int text_size = (int)etext & ~0xf;
	int i;

	if (dos_mem_phys_free_size < text_size)
		return;

	code_copy_seg = dos_mem_phys_free_mem >> 4;
	dos_mem_phys_free_mem += text_size;
	dos_mem_phys_free_size -= text_size;

	set_fs(code_copy_seg);
	for (i = 0; i < text_size; i += 4)
		asm volatile("
			movl (%%ebx),%%eax
			movl %%eax,%%fs:(%%ebx)
		" : : "b" (i) : "eax");
}

void i16_code_check(int dummy)
{
	int text_size = (int)etext & ~0xf;
	int i, old, new;
	int found = 0;

	if (!code_copy_seg)
		return;

	set_fs(code_copy_seg);
	for (i = (int)i16_entry_2; i < text_size; i += 4)
	{
		asm volatile("
			movl (%%ebx),%%eax
			movl %%fs:(%%ebx),%%ecx
		" : "=a" (new), "=c" (old) : "b" (i));
		if (old != new)
		{
			found = 1;
			i16_writehexw(i);
			i16_putchar(' ');
			i16_writehexl(old);
			i16_putchar(' ');
			i16_writehexl(new);
			i16_putchar('\r');
			i16_putchar('\n');
		}
	}
	if (found)
	{
		code_copy_seg = 0;
		i16_writehexl((&dummy)[-1]);
		i16_die(" DOS extender code trashed!");
	}
}

CODE32

void code_check(int dummy)
{
	int text_size = (int)etext & ~0xf;
	unsigned char *new = 0, *old = (void*)phystokv(code_copy_seg*16);
	int found = 0;
	int i;

	if (!code_copy_seg)
		return;

	for (i = (int)i16_entry_2; (i < text_size) && (found < 10); i++)
	{
		/* In several places we have to self-modify an int instruction,
		   or the segment value in an absolute long jmp instruction,
		   so ignore any changes preceded by those opcodes.  */
		if ((new[i] != old[i])
		    && (old[i-1] != 0xcd)
		    && (old[i-6] != 0xea))
		{
			if (!found)
			{
				found = 1;
				about_to_die(1);
			}
			printf("%08x addr %04x was %02x now %02x\n",
				(&dummy)[-1], i, old[i], new[i]);
		}
	}
	if (found)
	{
		code_copy_seg = 0;
		die("%08x DOS extender code trashed!", (&dummy)[-1]);
	}
}

CODE16

#endif ENABLE_CODE_CHECK
