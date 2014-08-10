/*
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
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
/*
 *	File:	mach/alert.h
 *
 *	Standard alert definitions
 *
 */

#ifndef	_MACH_ALERT_H_
#define _MACH_ALERT_H_

#define ALERT_BITS		32		/* Minimum; more may actually be available */

#define ALERT_ABORT_STRONG	0x00000001	/* Request to abort _all_ operations */
#define ALERT_ABORT_SAFE	0x00000002	/* Request to abort restartable operations */

#define ALERT_USER		0xffff0000	/* User-defined alert bits */

#endif	_MACH_ALERT_H_
