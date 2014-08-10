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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "dos_io.h"
#include "vm_param.h"
#include "debug.h"

int dos_open(const char *s, int flags, int mode, dos_fd_t *out_fh)
{
	struct real_call_data real_call_data;
	int err = EINVAL; /*XXX*/
	vm_offset_t dos_buf_phys = (vm_offset_t)kvtophys(dos_buf);

	assert(dos_buf); assert(dos_buf_phys);
	assert(dos_buf_phys < 0x100000);

	dos_init_rcd(&real_call_data);

	if (strlen(s)+1 > DOS_BUF_SIZE)
		return E2BIG;
	strcpy(dos_buf, s);
	real_call_data.ds = dos_buf_phys >> 4;
	real_call_data.edx = dos_buf_phys & 15;

	/* Possible situations:

	-	3d
	C	3d || 3c
	 T	3d (ensure that it exists), close, 3c
	CT	3c
	  X	3d
	C X	3d (ensure that it doesn't exist), 3c
	 TX	3d (ensure that it exists), close, 3c
	CTX	3d (ensure that it doesn't exist), 3c
	*/

	if ((flags & (O_CREAT | O_EXCL | O_TRUNC)) != (O_CREAT | O_TRUNC))
	{
		/* First try opening the file with function 0x3D.  */
		real_call_data.eax = 0x3d40 | (flags & O_ACCMODE);
		real_call_data.ecx = 0;
		real_int(0x21, &real_call_data);
		err = dos_check_err(&real_call_data);
		if (!err)
			*out_fh = real_call_data.eax & 0xffff;
	}

	/* Now based on the result, determine what to do next.  */
	switch (flags & (O_CREAT | O_EXCL | O_TRUNC))
	{
		case 0:
		case 0 | O_EXCL:
			if (!err)
				goto success;
			else
				return err;
		case O_CREAT:
			if (!err)
				goto success;
			else
				break;
		case O_CREAT | O_EXCL:
		case O_CREAT | O_TRUNC | O_EXCL:
			if (!err)
			{
				/* The file exists, but wasn't supposed to.
				   Close it and return an error.  */
				dos_close(real_call_data.eax & 0xffff);
				return EEXIST;
			}
			else
				break;
		case O_TRUNC:
		case O_TRUNC | O_EXCL:
			if (!err)
			{
				/* We've verified that the file exists -
				   now close it and reopen it with CREAT
				   so it'll be truncated as requested.  */
				dos_close(real_call_data.eax & 0xffff);
				break;
			}
			else
				return err;
		case O_CREAT | O_TRUNC:
			/* This is the one case in which
			   we didn't try to open the file above at all.
			   Just fall on through and open it with CREAT.  */
			break;
	}

	/* Now try opening the file with DOS's CREAT call,
	   which truncates the file if it already exists.  */
	real_call_data.eax = 0x3c00;
	real_call_data.ecx = mode & S_IWUSR ? 0 : 1;
	real_int(0x21, &real_call_data);
	if (!(err = dos_check_err(&real_call_data)))
	{
		*out_fh = real_call_data.eax & 0xffff;

		/* We don't have to worry about O_APPEND here,
		   because we know the file starts out empty.  */

		return 0;
	}

	return err;

	success:

	/* If the caller requested append access,
	   just seek to the end of the file once on open.
	   We can't implement full POSIX behavior here without help,
	   since DOS file descriptors don't have an append mode.
	   To get full POSIX behavior,
	   the caller must seek to the end of the file before every write.
	   However, seeking to the end only on open
	   is probably enough for most typical uses.  */
	if (flags & O_APPEND)
	{
		vm_offset_t newpos;
		err = dos_seek(*out_fh, 0, SEEK_END, &newpos);
		if (err)
		{
			dos_close(*out_fh);
			return err;
		}
	}

	return 0;
}

