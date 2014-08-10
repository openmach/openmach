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

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/termios.h>

#include "dos_io.h"

int dos_tcgetattr(dos_fd_t fd, struct termios *t)
{
	int err;
	struct real_call_data real_call_data;

	dos_init_rcd(&real_call_data);

	bzero(t, sizeof(*t));

	/* First make sure this is actually a character device.  */
	real_call_data.eax = 0x4400;
	real_call_data.ebx = fd;
	real_int(0x21, &real_call_data);
	if (err = dos_check_err(&real_call_data))
		return err;
	if (!(real_call_data.edx & (1<<7)))
		return ENOTTY;

	return 0;
}

