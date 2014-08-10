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
#ifndef _I16DOS_H_
#define _I16DOS_H_

#include <mach/inline.h>
#include <mach/machine/far_ptr.h>


/* Returns 16-bit DOS version number:
   major version number in high byte, minor in low byte.  */
MACH_INLINE unsigned short i16_dos_version(void)
{
	unsigned short dos_version_swapped;
	asm volatile("int $0x21" : "=a" (dos_version_swapped) : "a" (0x3000));
	return (dos_version_swapped >> 8) | (dos_version_swapped << 8);
}

MACH_INLINE void i16_dos_putchar(int c)
{
	asm volatile("int $0x21" : : "a" (0x0200), "d" (c));
}

MACH_INLINE void i16_dos_exit(int rc)
{
	asm volatile("int $0x21" : : "a" (0x4c00 | (rc & 0xff)));
}

MACH_INLINE void i16_dos_get_int_vec(int vecnum, struct far_pointer_16 *out_vec)
{
	asm volatile("
		pushw	%%es
		int	$0x21
		movw	%%es,%0
		popw	%%es
	" : "=r" (out_vec->seg), "=b" (out_vec->ofs)
	  : "a" (0x3500 | vecnum));
}

MACH_INLINE void i16_dos_set_int_vec(int vecnum, struct far_pointer_16 *new_vec)
{
	asm volatile("
		pushw	%%ds
		movw	%1,%%ds
		int	$0x21
		popw	%%ds
	" :
	  : "a" (0x2500 | vecnum),
	    "r" (new_vec->seg), "d" (new_vec->ofs));
}

/* Open a DOS file and return the new file handle.
   Returns -1 if an error occurs.  */
MACH_INLINE int i16_dos_open(const char *filename, int access)
{
	int fh;
	asm volatile("
		int	$0x21
		jnc	1f
		movl	$-1,%%eax
	1:
	" : "=a" (fh) : "a" (0x3d00 | access), "d" (filename));
	return fh;
}

MACH_INLINE void i16_dos_close(int fh)
{
	asm volatile("int $0x21" : : "a" (0x3e00), "b" (fh));
}

MACH_INLINE int i16_dos_get_device_info(int fh)
{
	int info_word;
	asm volatile("
		int	$0x21
		jnc	1f
		movl	$-1,%%edx
	1:
	" : "=d" (info_word) : "a" (0x4400), "b" (fh), "d" (0));
	return info_word;
}

MACH_INLINE int i16_dos_get_output_status(int fh)
{
	int status;
	asm volatile("
		int	$0x21
		movzbl	%%al,%%eax
		jnc	1f
		movl	$-1,%%eax
	1:
	" : "=a" (status) : "a" (0x4407), "b" (fh));
	return status;
}

MACH_INLINE int i16_dos_alloc(unsigned short *inout_paras)
{
	int seg;
	asm volatile("
		int	$0x21
		jnc	1f
		movl	$-1,%%eax
	1:
	" : "=a" (seg), "=b" (*inout_paras)
	  : "a" (0x4800), "b" (*inout_paras));
	return seg;
}

MACH_INLINE int i16_dos_free(unsigned short seg)
{
	asm volatile("
		pushw	%%es
		movw	%1,%%es
		int	$0x21
		popw	%%es
	" : : "a" (0x4900), "r" (seg) : "eax");
}

MACH_INLINE unsigned short i16_dos_get_psp_seg(void)
{
	unsigned short psp_seg;
	asm volatile("int $0x21" : "=b" (psp_seg) : "a" (0x6200));
	return psp_seg;
}

#endif _I16DOS_H_
