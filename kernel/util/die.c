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

#include <stdarg.h>

/* Note: These routines are deprecated and should not be used anymore.
   Use panic() instead.  */
void die(const char *fmt, ...)
{
	int exit_code = fmt != 0;

	about_to_die(exit_code);

	if (fmt)
	{
		va_list vl;

		va_start(vl, fmt);
		vprintf(fmt, vl);
		va_end(vl);

		putchar('\n');

		exit(exit_code);
	}
	else
		exit(exit_code);
}

void die_no_mem(void)
{
	die("Not enough memory.");
}

