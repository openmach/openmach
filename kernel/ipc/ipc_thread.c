/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
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
 */
/*
 *	File:	ipc/ipc_thread.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	IPC operations on threads.
 */

#include <kern/assert.h>
#include <ipc/ipc_thread.h>

/*
 *	Routine:	ipc_thread_enqueue
 *	Purpose:
 *		Enqueue a thread.
 */

void
ipc_thread_enqueue(
	ipc_thread_queue_t	queue,
	ipc_thread_t		thread)
{
	ipc_thread_enqueue_macro(queue, thread);
}

/*
 *	Routine:	ipc_thread_dequeue
 *	Purpose:
 *		Dequeue and return a thread.
 */

ipc_thread_t
ipc_thread_dequeue(
	ipc_thread_queue_t	queue)
{
	ipc_thread_t first;

	first = ipc_thread_queue_first(queue);

	if (first != ITH_NULL)
		ipc_thread_rmqueue_first_macro(queue, first);

	return first;
}

/*
 *	Routine:	ipc_thread_rmqueue
 *	Purpose:
 *		Pull a thread out of a queue.
 */

void
ipc_thread_rmqueue(
	ipc_thread_queue_t	queue,
	ipc_thread_t		thread)
{
	ipc_thread_t next, prev;

	assert(queue->ithq_base != ITH_NULL);

	next = thread->ith_next;
	prev = thread->ith_prev;

	if (next == thread) {
		assert(prev == thread);
		assert(queue->ithq_base == thread);

		queue->ithq_base = ITH_NULL;
	} else {
		if (queue->ithq_base == thread)
			queue->ithq_base = next;

		next->ith_prev = prev;
		prev->ith_next = next;
		ipc_thread_links_init(thread);
	}
}
