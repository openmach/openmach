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

#include <mach/machine/eflags.h>

#include "real.h"

#ifndef ENABLE_IMMCONSOLE

#include <rc.h>
int putchar(int c)
{

	if (c == '\n')
		putchar('\r');

	{
#if RCLINE >= 0
		static int serial_inited = 0;
		if (! serial_inited) {
			init_serial();
			serial_inited = 1;
		}
		serial_putc(c);
#else
		struct real_call_data rcd;
		rcd.eax = 0x0e00 | (c & 0xff);
		rcd.ebx = 0x07;
		rcd.flags = 0;
		real_int(0x10, &rcd);
#endif
	}

	return 0;
}

#else ENABLE_IMMCONSOLE

void
putchar(unsigned char c)
{
	static int ofs = -1;

	if (ofs < 0)
	{
		ofs = 0;
		putchar('\n');
	}
	if (c == '\r')
	{
		ofs = 0;
	}
	else if (c == '\n')
	{
		bcopy(0xb8000+80*2, 0xb8000, 80*2*24);
		bzero(0xb8000+80*2*24, 80*2);
		ofs = 0;
	}
	else
	{
		volatile unsigned char *p;

		if (ofs >= 80)
		{
			putchar('\r');
			putchar('\n');
		}

		p = (void*)0xb8000 + 80*2*24 + ofs*2;
		p[0] = c;
		p[1] = 0x0f;
		ofs++;
	}
}

#endif ENABLE_IMMCONSOLE
