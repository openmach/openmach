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
#ifndef _I16_BIOS_H_
#define _I16_BIOS_H_

#include <mach/inline.h>


MACH_INLINE void i16_bios_putchar(int c)
{
	asm volatile("int $0x10" : : "a" (0x0e00 | (c & 0xff)), "b" (0x07));
}

MACH_INLINE int i16_bios_getchar()
{
	int c;
	asm volatile("int $0x16" : "=a" (c) : "a" (0x0000));
	c &= 0xff;
	return c;
}

MACH_INLINE void i16_bios_warm_boot(void)
{
	asm volatile("
		cli
		movw	$0x40,%ax
		movw	%ax,%ds
		movw	$0x1234,0x72
		ljmp	$0xffff,$0x0000
	");
}

MACH_INLINE void i16_bios_cold_boot(void)
{
	asm volatile("
		cli
		movw	$0x40,%ax
		movw	%ax,%ds
		movw	$0x0000,0x72
		ljmp	$0xffff,$0x0000
	");
}

MACH_INLINE unsigned char i16_bios_copy_ext_mem(
	unsigned src_la, unsigned dest_la, unsigned short word_count)
{
	char buf[48];
	unsigned short i, rc;

	/* Initialize the descriptor structure.  */
	for (i = 0; i < sizeof(buf); i++)
		buf[i] = 0;
	*((unsigned short*)(buf+0x10)) = 0xffff; /* source limit */
	*((unsigned long*)(buf+0x12)) = src_la; /* source linear address */
	*((unsigned char*)(buf+0x15)) = 0x93; /* source access rights */
	*((unsigned short*)(buf+0x18)) = 0xffff; /* dest limit */
	*((unsigned long*)(buf+0x1a)) = dest_la; /* dest linear address */
	*((unsigned char*)(buf+0x1d)) = 0x93; /* dest access rights */

#if 0
	i16_puts("buf:");
	for (i = 0; i < sizeof(buf); i++)
		i16_writehexb(buf[i]);
	i16_puts("");
#endif

	/* Make the BIOS call to perform the copy.  */
	asm volatile("
		int	$0x15
	" : "=a" (rc)
	  : "a" ((unsigned short)0x8700),
	    "c" (word_count),
	    "S" ((unsigned short)(unsigned)buf));

	return rc >> 8;
}

#endif _I16_BIOS_H_
