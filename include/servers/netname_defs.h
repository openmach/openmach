/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	File:	netname_defs.h
 *	Author: Dan Julin, Carnegie Mellon University
 *	Date:	Dec. 1986
 *
 * 	Definitions for the mig interface to the network name service.
 */

#ifndef	_NETNAME_DEFS_
#define	_NETNAME_DEFS_

#define NETNAME_SUCCESS		(0)
#define	NETNAME_PENDING		(-1)
#define NETNAME_NOT_YOURS	(1000)
#define NAME_NOT_YOURS		(1000)
#define NETNAME_NOT_CHECKED_IN	(1001)
#define NAME_NOT_CHECKED_IN	(1001)
#define NETNAME_NO_SUCH_HOST	(1002)
#define NETNAME_HOST_NOT_FOUND	(1003)
#define	NETNAME_INVALID_PORT	(1004)

typedef char netname_name_t[80];

#endif /* _NETNAME_DEFS_ */
