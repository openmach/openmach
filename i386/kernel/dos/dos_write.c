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

#include "dos_io.h"
#include "vm_param.h"

int dos_write(dos_fd_t fd, const void *buf, vm_size_t size, vm_size_t *out_actual)
{
	int err;
	int actual = 0;
	struct real_call_data real_call_data;
	vm_offset_t dos_buf_phys = (vm_offset_t)kvtophys(dos_buf);

	assert(dos_buf); assert(dos_buf_phys);
	assert(dos_buf_phys < 0x100000);

	dos_init_rcd(&real_call_data);

	while (size > 0)
	{
		int little_size = size;
		int little_actual;

		if (little_size > DOS_BUF_SIZE)
			little_size = DOS_BUF_SIZE;

		/* XXX don't copy if buf is <1MB */
		memcpy(dos_buf, buf, little_size);

		real_call_data.eax = 0x4000;
		real_call_data.ebx = fd;
		real_call_data.ecx = little_size;
		real_call_data.ds = dos_buf_phys >> 4;
		real_call_data.edx = dos_buf_phys & 15;
		real_int(0x21, &real_call_data);
		if (err = dos_check_err(&real_call_data))
			return err;
		little_actual = real_call_data.eax & 0xffff;
		assert(little_actual <= little_size);

		buf += little_actual;
		size -= little_actual;
		actual += little_actual;

		if (little_actual < little_size)
			break;
	}

	*out_actual = actual;
	return 0;
}

