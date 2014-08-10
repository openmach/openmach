/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon rights
 * to redistribute these changes.
 */
/*
 *	Time-keeper for kernel IO devices.
 *
 *	May or may not have any relation to wall-clock time.
 */

#ifndef _MACH_SA_SYS_TIME_H_
#define _MACH_SA_SYS_TIME_H_

#include <mach/time_value.h>

extern time_value_t	time;

/*
 * Definitions to keep old code happy.
 */
#define timeval_t time_value_t
#define timeval time_value
#define	tv_sec	seconds
#define	tv_usec	microseconds

#define timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define timercmp(tvp, uvp, cmp)	\
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	 (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#define timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0

#endif _MACH_SA_SYS_TIME_H_
