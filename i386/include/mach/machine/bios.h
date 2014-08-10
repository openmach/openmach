/* 
 * Copyright (c) 1994 The University of Utah and
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

#ifndef _MACH_MACHINE_BIOS_
#define _MACH_MACHINE_BIOS_

/*
 * To make a call to a 16-bit BIOS entrypoint,
 * fill in one of these structures and call bios_call().
 */
struct bios_call_params
{
	union
	{
		struct
		{
			unsigned short ax;
			unsigned short bx;
			unsigned short cx;
			unsigned short dx;
		} w;
		struct
		{
			unsigned char al;
			unsigned char ah;
			unsigned char bl;
			unsigned char bh;
			unsigned char cl;
			unsigned char ch;
			unsigned char dl;
			unsigned char dh;
		} b;
	} u;
	unsigned short si;
	unsigned short di;
	unsigned short bp;
	unsigned short ds;
	unsigned short es;
	unsigned short flags;
};

void bios_call(unsigned char int_num, struct bios_call_params *bcp);

#endif _MACH_MACHINE_BIOS_
