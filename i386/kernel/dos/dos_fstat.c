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
#include <sys/stat.h>

#include "dos_io.h"

int dos_fstat(dos_fd_t fd, struct stat *st)
{
	int err;
	int actual = 0;
	struct real_call_data real_call_data;
	vm_offset_t old_pos, new_pos;

	dos_init_rcd(&real_call_data);

	bzero(st, sizeof(*st));
	st->st_nlink = 1;
	st->st_mode = S_IRWXU | S_IRWXG | S_IRWXO; /* XXX attributes */

	/* Get device information,
	   which will tell us whether this is a character device
	   or a regular file.  */
	real_call_data.eax = 0x4400;
	real_call_data.ebx = fd;
	real_int(0x21, &real_call_data);
	if (err = dos_check_err(&real_call_data))
		return err;
	if (real_call_data.edx & (1<<7))
		st->st_mode |= S_IFCHR;
	else
		st->st_mode |= S_IFREG;

	/* XXX get date/time with int 21 5700 */

	/* Get file size by seeking to the end and back.  */
	if (!dos_seek(fd, 0, 1, &old_pos)
	    && !dos_seek(fd, 0, 2, &st->st_size))
	{
		if (err = dos_seek(fd, old_pos, 0, &new_pos))
			return err;
		if (new_pos != old_pos)
			return EIO;/*XXX*/
	}

	/* Always assume 512-byte blocks for now... */
	st->st_blocks = (st->st_size + 511) / 512;
	st->st_blksize = 512;

	return 0;
}

