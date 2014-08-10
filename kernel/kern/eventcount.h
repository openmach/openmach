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
 *	File:	eventcount.c
 *	Author:	Alessandro Forin
 *	Date:	10/91
 *
 *	Eventcounters, for user-level drivers synchronization
 *
 */

#ifndef	_KERN_EVENTCOUNT_H_
#define	_KERN_EVENTCOUNT_H_	1

/* kernel visible only */

typedef struct evc {
	int		count;
	thread_t	waiting_thread;
	natural_t	ev_id;
	struct evc	*sanity;
	decl_simple_lock_data(,	lock)
} *evc_t;

extern	void	evc_init(evc_t ev),
		evc_destroy(evc_t ev),
		evc_signal(evc_t ev),
  		evc_notify_abort(thread_t thread);

/* kernel and user visible */

extern	kern_return_t	evc_wait(natural_t ev_id);

#endif	/* _KERN_EVENTCOUNT_H_ */
