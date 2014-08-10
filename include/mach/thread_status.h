/*
 * Mach Operating System
 * Copyright (c) 1993-1988 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *
 *	This file contains the structure definitions for the user-visible
 *	thread state.  This thread state is examined with the thread_get_state
 *	kernel call and may be changed with the thread_set_state kernel call.
 *
 */

#ifndef	_MACH_THREAD_STATUS_H_
#define	_MACH_THREAD_STATUS_H_

/*
 *	The actual structure that comprises the thread state is defined
 *	in the machine dependent module.
 */
#include <mach/machine/vm_types.h>
#include <mach/machine/thread_status.h>

/*
 *	Generic definition for machine-dependent thread status.
 */

typedef natural_t		*thread_state_t;	/* Variable-length array */

#define	THREAD_STATE_MAX	(1024)		/* Maximum array size */
typedef natural_t	thread_state_data_t[THREAD_STATE_MAX];

#define	THREAD_STATE_FLAVOR_LIST	0	/* List of valid flavors */

#endif	/* _MACH_THREAD_STATUS_H_ */
