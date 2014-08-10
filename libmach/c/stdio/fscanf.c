/*
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University.
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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

#include <stdio.h>
#include <stdarg.h>

/* XXX kludge to work around absence of ungetc() */
typedef struct state
{
	FILE *fh;
	int ch;
} state;

static int
read_char(void *arg)
{
	state *st = (state*)arg;
	if (st->ch >= 0)
	{
		int c = st->ch;
		st->ch = -1;
		return c;
	}
	else
		return fgetc(st->fh);
}

static void
unread(int c, void *arg)
{
	state *st = (state*)arg;
	st->ch = c;
}

#if 0
static int
read_char(void *arg)
{
	return fgetc((FILE*)arg);
}

static void
unread(int c, void *arg)
{
	ungetc(c, (FILE*)arg);
}
#endif

vfscanf(FILE *fh, const char *fmt, va_list args)
{
	_doscan(fmt, args, read_char, unread, fh);
}

int
fscanf(FILE *fh, const char *fmt, ...)
{
	va_list	args;
	state st;

	st.fh = fh;
	st.ch = -1;

	va_start(args, fmt);
	vfscanf(fh, fmt, args);
	va_end(args);
}

