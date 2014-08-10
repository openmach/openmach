/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University.
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
 * Compatibility definitions for the MiG-related changes
 * to various routines.
 *
 * When all user code has been relinked, this file and the xxx_
 * and yyy_ routines MUST be removed!
 */

/* from mach.defs */

#define	xxx_task_info			task_info
#ifdef MIGRATING_THREADS
#define	xxx_thread_get_state		act_get_state
#define	xxx_thread_set_state		act_set_state
#define	xxx_thread_info			act_info
#else
#define	xxx_thread_get_state		thread_get_state
#define	xxx_thread_set_state		thread_set_state
#define	xxx_thread_info			thread_info
#endif /* MIGRATING_THREADS */

/* from mach_host.defs */

#define	yyy_host_info			host_info
#define	yyy_processor_info		processor_info
#define	yyy_processor_set_info		processor_set_info
#define	yyy_processor_control		processor_control

/* from device.defs */

#define	ds_xxx_device_set_status	ds_device_set_status
#define	ds_xxx_device_get_status	ds_device_get_status
#define	ds_xxx_device_set_filter	ds_device_set_filter



