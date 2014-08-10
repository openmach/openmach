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

#include <sys/varargs.h>

/* This version of printf is implemented in terms of putchar and puts.  */

#define	PRINTF_BUFMAX	128

struct printf_state {
	char buf[PRINTF_BUFMAX];
	unsigned int index;
};

static void
flush(struct printf_state *state)
{
	int i;

	for (i = 0; i < state->index; i++)
		putchar(state->buf[i]);

	state->index = 0;
}

static void
printf_char(arg, c)
	char *arg;
	int c;
{
	struct printf_state *state = (struct printf_state *) arg;

	if (c == '\n')
	{
		state->buf[state->index] = 0;
		puts(state->buf);
		state->index = 0;
	}
	else if ((c == 0) || (state->index >= PRINTF_BUFMAX))
	{
		flush(state);
		putchar(c);
	}
	else
	{
		state->buf[state->index] = c;
		state->index++;
	}
}

/*
 * Printing (to console)
 */
vprintf(fmt, args)
	char *fmt;
	va_list args;
{
	struct printf_state state;

	state.index = 0;
	_doprnt(fmt, args, 0, (void (*)())printf_char, (char *) &state);

	if (state.index != 0)
	    flush(&state);
}

int
printf(fmt, va_alist)
	char *fmt;
	va_dcl
{
	va_list	args;

	va_start(args);
	vprintf(fmt, args);
	va_end(args);
}

