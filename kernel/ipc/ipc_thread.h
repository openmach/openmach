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
 */
/*
 *	File:	ipc/ipc_thread.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for the IPC component of threads.
 */

#ifndef	_IPC_IPC_THREAD_H_
#define _IPC_IPC_THREAD_H_

#include <kern/thread.h>

typedef thread_t ipc_thread_t;

#define	ITH_NULL		THREAD_NULL

#define	ith_lock_init(thread)	simple_lock_init(&(thread)->ith_lock_data)
#define	ith_lock(thread)	simple_lock(&(thread)->ith_lock_data)
#define	ith_unlock(thread)	simple_unlock(&(thread)->ith_lock_data)

typedef struct ipc_thread_queue {
	ipc_thread_t ithq_base;
} *ipc_thread_queue_t;

#define	ITHQ_NULL		((ipc_thread_queue_t) 0)


#define	ipc_thread_links_init(thread)		\
MACRO_BEGIN					\
	(thread)->ith_next = (thread);		\
	(thread)->ith_prev = (thread);		\
MACRO_END

#define	ipc_thread_queue_init(queue)		\
MACRO_BEGIN					\
	(queue)->ithq_base = ITH_NULL;		\
MACRO_END

#define	ipc_thread_queue_empty(queue)	((queue)->ithq_base == ITH_NULL)

#define	ipc_thread_queue_first(queue)	((queue)->ithq_base)

#define	ipc_thread_rmqueue_first_macro(queue, thread)			\
MACRO_BEGIN								\
	register ipc_thread_t _next;					\
									\
	assert((queue)->ithq_base == (thread));				\
									\
	_next = (thread)->ith_next;					\
	if (_next == (thread)) {					\
		assert((thread)->ith_prev == (thread));			\
		(queue)->ithq_base = ITH_NULL;				\
	} else {							\
		register ipc_thread_t _prev = (thread)->ith_prev;	\
									\
		(queue)->ithq_base = _next;				\
		_next->ith_prev = _prev;				\
		_prev->ith_next = _next;				\
		ipc_thread_links_init(thread);				\
	}								\
MACRO_END

#define	ipc_thread_enqueue_macro(queue, thread)				\
MACRO_BEGIN								\
	register ipc_thread_t _first = (queue)->ithq_base;		\
									\
	if (_first == ITH_NULL) {					\
		(queue)->ithq_base = (thread);				\
		assert((thread)->ith_next == (thread));			\
		assert((thread)->ith_prev == (thread));			\
	} else {							\
		register ipc_thread_t _last = _first->ith_prev;		\
									\
		(thread)->ith_next = _first;				\
		(thread)->ith_prev = _last;				\
		_first->ith_prev = (thread);				\
		_last->ith_next = (thread);				\
	}								\
MACRO_END

/* Enqueue a thread on a message queue */
extern void ipc_thread_enqueue(
	ipc_thread_queue_t	queue,
	ipc_thread_t		thread);

/* Dequeue a thread from a message queue */
extern ipc_thread_t ipc_thread_dequeue(
	ipc_thread_queue_t	queue);

/* Remove a thread from a message queue */
extern void ipc_thread_rmqueue(
	ipc_thread_queue_t	queue,
	ipc_thread_t		thread);

#endif	/* _IPC_IPC_THREAD_H_ */
