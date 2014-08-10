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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

FILE *
fopen(const char *name, const char *mode)
{
	int base_mode = O_RDONLY, flags = 0;
	FILE *stream;

	/* Find the appropriate Unix mode flags.  */
	switch (*mode++)
	{
		case 'w':
			base_mode = O_WRONLY;
			flags |= O_CREAT | O_TRUNC;
			break;
		case 'a':
			base_mode = O_WRONLY;
			flags |= O_CREAT | O_APPEND;
			break;
	}
	while (*mode)
	{
		if (*mode == '+')
			base_mode = O_RDWR;
		mode++;
	}

	if (!(stream = malloc(sizeof(*stream))))
		return 0;

	if ((stream->fd = open(name, base_mode | flags)) < 0)
	{
		free(stream);
		return 0;
	}

	return stream;
}

