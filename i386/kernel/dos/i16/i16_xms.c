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
#include <mach/machine/asm.h>

#include "i16_a20.h"
#include "phys_mem.h"
#include "debug.h"


struct far_pointer_16 xms_control;

#define CALL_XMS "lcallw "SEXT(xms_control)


static vm_offset_t xms_phys_free_mem;
static vm_size_t xms_phys_free_size;

static short free_handle;
static char free_handle_allocated;
static char free_handle_locked;


CODE32

void xms_mem_collect(void)
{
	if (xms_phys_free_mem)
	{
		phys_mem_add(xms_phys_free_mem, xms_phys_free_size);
		xms_phys_free_mem = 0;
	}
}

CODE16

static void i16_xms_enable_a20(void)
{
	short success;
	asm volatile(CALL_XMS : "=a" (success) : "a" (0x0500) : "ebx");
	if (!success)
		i16_die("XMS error: can't enable A20 line");
}

static void i16_xms_disable_a20(void)
{
	short success;
	asm volatile(CALL_XMS : "=a" (success) : "a" (0x0600) : "ebx");
	if (!success)
		i16_die("XMS error: can't disable A20 line");
}

void i16_xms_check()
{
	unsigned short rc;
	unsigned short free_k;

	/* Check for an XMS server.  */
	asm volatile("
		int $0x2f
	" : "=a" (rc)
	  : "a" (0x4300));
	if ((rc & 0xff) != 0x80)
		return;

	/* Get XMS driver's control function.  */
	asm volatile("
		pushl	%%ds
		pushl	%%es
		int	$0x2f
		movw	%%es,%0
		popl	%%es
		popl	%%ds
	" : "=r" (xms_control.seg), "=b" (xms_control.ofs)
	  : "a" (0x4310));

	/* See how much memory is available.  */
	asm volatile(CALL_XMS
	  : "=a" (free_k)
	  : "a" (0x0800)
	  : "ebx", "edx");
	if (free_k * 1024 == 0)
		return;

	xms_phys_free_size = (unsigned)free_k * 1024;

	/* Grab the biggest memory block we can get.  */
	asm volatile(CALL_XMS
	  : "=a" (rc), "=d" (free_handle)
	  : "a" (0x0900), "d" (free_k)
	  : "ebx");
	if (!rc)
		i16_die("XMS error: can't allocate extended memory");

	free_handle_allocated = 1;

	/* Lock it down.  */
	asm volatile(CALL_XMS "
		shll	$16,%%edx
		movw	%%bx,%%dx
	" : "=a" (rc), "=d" (xms_phys_free_mem)
	  : "a" (0x0c00), "d" (free_handle)
	  : "ebx");
	if (!rc)
		i16_die("XMS error: can't lock down extended memory");

	free_handle_locked = 1;

	/* We need to update phys_mem_max here
	   instead of just letting phys_mem_add() do it
	   when the memory is collected with phys_mem_collect(),
	   because VCPI initialization needs to know the top of physical memory
	   before phys_mem_collect() is called.
	   See i16_vcpi.c for the gross details.  */
	if (phys_mem_max < xms_phys_free_mem + xms_phys_free_size)
		phys_mem_max = xms_phys_free_mem + xms_phys_free_size;

	i16_enable_a20 = i16_xms_enable_a20;
	i16_disable_a20 = i16_xms_disable_a20;

	do_debug(i16_puts("XMS detected"));
}

void i16_xms_shutdown()
{
	unsigned short rc;

	if (free_handle_locked)
	{
		/* Unlock our memory block.  */
		asm volatile(CALL_XMS
		  : "=a" (rc)
		  : "a" (0x0d00), "d" (free_handle)
		  : "ebx");
		free_handle_locked = 0;
		if (!rc)
			i16_die("XMS error: can't unlock extended memory");
	}

	if (free_handle_allocated)
	{
		/* Free the memory block.  */
		asm volatile(CALL_XMS
		  : "=a" (rc)
		  : "a" (0x0a00), "d" (free_handle)
		  : "ebx");
		free_handle_allocated = 0;
		if (!rc)
			i16_die("XMS error: can't free extended memory");
	}
}

