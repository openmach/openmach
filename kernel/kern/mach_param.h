/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
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
/*
 *	File:	kern/mach_param.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1986
 *
 *	Mach system sizing parameters
 *
 */

#ifndef	_KERN_MACH_PARAM_H_
#define _KERN_MACH_PARAM_H_

#define THREAD_MAX	1024		/* Max number of threads */
#define THREAD_CHUNK	64		/* Allocation chunk */

#define TASK_MAX	1024		/* Max number of tasks */
#define TASK_CHUNK	64		/* Allocation chunk */

#define ACT_MAX		1024		/* Max number of acts */
#define ACT_CHUNK	64		/* Allocation chunk */

#define ACTPOOL_MAX	1024
#define ACTPOOL_CHUNK	64

#define PORT_MAX	((TASK_MAX * 3 + THREAD_MAX)	/* kernel */ \
				+ (THREAD_MAX * 2)	/* user */ \
				+ 40000)		/* slop for objects */
					/* Number of ports, system-wide */

#define SET_MAX		(TASK_MAX + THREAD_MAX + 200)
					/* Max number of port sets */

#define	ITE_MAX		(1 << 16)	/* Max number of splay tree entries */

#define	SPACE_MAX	(TASK_MAX + 5)	/* Max number of IPC spaces */

#define	IMAR_MAX	(1 << 10)	/* Max number of msg-accepted reqs */

#endif	_KERN_MACH_PARAM_H_
