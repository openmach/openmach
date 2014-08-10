/* 
 * Mach Operating System
 * Copyright (c) 1993-1987 Carnegie Mellon University
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
 *	Machine-independent task information structures and definitions.
 *
 *	The definitions in this file are exported to the user.  The kernel
 *	will translate its internal data structures to these structures
 *	as appropriate.
 *
 */

#ifndef	_MACH_TASK_INFO_H_
#define	_MACH_TASK_INFO_H_

#include <mach/machine/vm_types.h>
#include <mach/time_value.h>

/*
 *	Generic information structure to allow for expansion.
 */
typedef	integer_t	*task_info_t;		/* varying array of int */

#define	TASK_INFO_MAX	(1024)		/* maximum array size */
typedef	integer_t	task_info_data_t[TASK_INFO_MAX];

/*
 *	Currently defined information structures.
 */
#define	TASK_BASIC_INFO		1	/* basic information */

struct task_basic_info {
	integer_t	suspend_count;	/* suspend count for task */
	integer_t	base_priority;	/* base scheduling priority */
	vm_size_t	virtual_size;	/* number of virtual pages */
	vm_size_t	resident_size;	/* number of resident pages */
	time_value_t	user_time;	/* total user run time for
					   terminated threads */
	time_value_t	system_time;	/* total system run time for
					   terminated threads */
};

typedef struct task_basic_info		task_basic_info_data_t;
typedef struct task_basic_info		*task_basic_info_t;
#define	TASK_BASIC_INFO_COUNT	\
		(sizeof(task_basic_info_data_t) / sizeof(natural_t))


#define	TASK_EVENTS_INFO	2	/* various event counts */

struct task_events_info {
	natural_t		faults;		/* number of page faults */
	natural_t		zero_fills;	/* number of zero fill pages */
	natural_t		reactivations;	/* number of reactivated pages */
	natural_t		pageins;	/* number of actual pageins */
	natural_t		cow_faults;	/* number of copy-on-write faults */
	natural_t		messages_sent;	/* number of messages sent */
	natural_t		messages_received; /* number of messages received */
};
typedef struct task_events_info		task_events_info_data_t;
typedef struct task_events_info		*task_events_info_t;
#define	TASK_EVENTS_INFO_COUNT	\
		(sizeof(task_events_info_data_t) / sizeof(natural_t))

#define	TASK_THREAD_TIMES_INFO	3	/* total times for live threads -
					   only accurate if suspended */

struct task_thread_times_info {
	time_value_t	user_time;	/* total user run time for
					   live threads */
	time_value_t	system_time;	/* total system run time for
					   live threads */
};

typedef struct task_thread_times_info	task_thread_times_info_data_t;
typedef struct task_thread_times_info	*task_thread_times_info_t;
#define	TASK_THREAD_TIMES_INFO_COUNT	\
		(sizeof(task_thread_times_info_data_t) / sizeof(natural_t))

/*
 * Flavor definitions for task_ras_control
 */
#define TASK_RAS_CONTROL_PURGE_ALL			0
#define TASK_RAS_CONTROL_PURGE_ONE			1
#define TASK_RAS_CONTROL_PURGE_ALL_AND_INSTALL_ONE	2
#define TASK_RAS_CONTROL_INSTALL_ONE			3

#endif	/* _MACH_TASK_INFO_H_ */

