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

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>


#define	PRINTF_BUFMAX	128

struct printf_state {
	FILE *stream;
	char buf[PRINTF_BUFMAX];
	unsigned int index;
};


static void
flush(struct printf_state *state)
{
	if (state->index > 0)
	{
		__write(state->stream->fd, state->buf, state->index);
		state->index = 0;
	}
}

static void
dochar(void *arg, int c)
{
	struct printf_state *state = (struct printf_state *) arg;

	if (state->index >= PRINTF_BUFMAX)
		flush(state);

	state->buf[state->index] = c;
	state->index++;
}

/*
 * Printing (to console)
 */
vfprintf(FILE *stream, const char *fmt, va_list args)
{
	struct printf_state state;

	state.stream = stream;
	state.index = 0;
	_doprnt(fmt, args, 0, (void (*)())dochar, (char *) &state);

	flush(&state);

	return 0;
}

