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

#ifdef ENABLE_IMMEDIATE_CONSOLE

/* This is a special "feature" (read: kludge)
   intended for use only for kernel debugging.
   It enables an extremely simple console output mechanism
   that sends text straight to CGA/EGA/VGA video memory.
   It has the nice property of being functional right from the start,
   so it can be used to debug things that happen very early
   before any devices are initialized.  */

int immediate_console_enable = 1;

void
immc_cnputc(unsigned char c)
{
	static int ofs = -1;

	if (!immediate_console_enable)
		return;
	if (ofs < 0)
	{
		ofs = 0;
		immc_cnputc('\n');
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
			immc_cnputc('\r');
			immc_cnputc('\n');
		}

		p = (void*)0xb8000 + 80*2*24 + ofs*2;
		p[0] = c;
		p[1] = 0x0f;
		ofs++;
	}
}

int immc_cnmaygetc(void)
{
	return -1;
}

#endif ENABLE_IMMEDIATE_CONSOLE

