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
#include <sys/time.h>

#include "dos_io.h"
#include "debug.h"

int dos_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	static int daysofmonth[12] = {31, 28, 31, 30, 31, 30, 31,
				      31, 30, 31, 30, 31};

	struct real_call_data real_call_data;
	int err;

	dos_init_rcd(&real_call_data);

	if (tv)
	{
		int year, month, day, hour, min, sec, hund;

		real_call_data.eax = 0x2a00;
		real_int(0x21, &real_call_data);
		year = real_call_data.ecx & 0xffff;
		month = (real_call_data.edx >> 8) & 0xff;
		day = real_call_data.edx & 0xff;
		real_call_data.eax = 0x2c00;
		real_int(0x21, &real_call_data);
		if (err = dos_check_err(&real_call_data))
			return err;

		hour = (real_call_data.ecx >> 8) & 0xff;
		min = real_call_data.ecx & 0xff;
		sec = (real_call_data.edx >> 8) & 0xff;
		hund = real_call_data.edx & 0xff;

		tv->tv_sec = (year - 1970) * (365 * 24 * 60 * 60);
		tv->tv_sec += (year - 1970) / 4 * (24 * 60 * 60); /* XXX??? */
		tv->tv_sec += daysofmonth[month-1] * (24 * 60 * 60);
		if ((((year - 1970) % 4) == 0) && (month > 2)) /* XXX??? */
			tv->tv_sec += 24 * 60 * 60;
		tv->tv_sec += day * 24 * 60 * 60;
		tv->tv_sec += hour * 60 * 60;
		tv->tv_sec += min * 60;
		tv->tv_sec += sec;
		tv->tv_usec = hund * (1000000 / 100);
	}
	if (tz)
		return EINVAL; /*XXX*/

	assert(tz == 0);
	return 0;
}

