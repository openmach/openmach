/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <stdarg.h>

static int
readchar(arg)
	void *arg;
{
	return *(*(unsigned char**)arg)++;
}

static void
unchar(c, arg)
	int c;
	void *arg;
{
	(*(unsigned char**)arg)--;
}

vsscanf(s, fmt, args)
	char *s;
	char *fmt;
	va_list args;
{
	_doscan(fmt, args, readchar, unchar, &s);
}

/*VARARGS2*/
sscanf(char *s, char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);
	vsscanf(s, fmt, args);
	va_end(args);
}

