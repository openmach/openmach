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

#include <mach.h>
#include <device/device.h>

mach_port_t __libmach_console_port = MACH_PORT_NULL;

static int dropped_print = FALSE;

void
printf_init(device_server_port)
	mach_port_t device_server_port;
{
	(void) device_open(device_server_port,
			   (dev_mode_t)0,
			   "console",
			   &__libmach_console_port);
	return;
}

void
__libmach_console_write(char *buf, int len)
{
	int amt;

	if (__libmach_console_port == MACH_PORT_NULL)
	{
		dropped_print = TRUE;
		return;
	}

	if (dropped_print)
	{
		char *s = "Text written to the libmach console was lost\r\n"
			  "before the console was initialized.\r\n";

		dropped_print = FALSE;
		__libmach_console_write(s, strlen(s));
	}

	while (len > 0)
	{
		if (device_write_inband(__libmach_console_port, (dev_mode_t)0,
					(recnum_t)0, buf, len, &amt)
		    != D_SUCCESS)
			break;
		buf += amt;
		len -= amt;
	}
}

