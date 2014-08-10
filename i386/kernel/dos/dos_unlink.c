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

int dos_unlink(const char *filename)
{
	struct real_call_data real_call_data;
	vm_offset_t dos_buf_phys = (vm_offset_t)kvtophys(dos_buf);

	dos_init_rcd(&real_call_data);

	if (strlen(filename)+1 > DOS_BUF_SIZE)
		return E2BIG;
	strcpy(dos_buf, filename);

	real_call_data.eax = 0x4100;
	real_call_data.ds = dos_buf_phys >> 4;
	real_call_data.edx = dos_buf_phys & 15;
	real_int(0x21, &real_call_data);

	return dos_check_err(&real_call_data);
}

