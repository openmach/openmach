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

CODE16

void i16_writehexdigit(unsigned char digit)
{
	digit &= 0xf;
	i16_putchar(digit < 10 ? digit+'0' : digit-10+'A');
}

void i16_writehexb(unsigned char val)
{
	i16_writehexdigit(val >> 4);
	i16_writehexdigit(val);
}

void i16_writehexw(unsigned short val)
{
	i16_writehexb(val >> 8);
	i16_writehexb(val);
}

void i16_writehexl(unsigned long val)
{
	i16_writehexw(val >> 16);
	i16_writehexw(val);
}

void i16_writehexll(unsigned long long val)
{
	i16_writehexl(val >> 32);
	i16_writehexl(val);
}

