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
 *	File:	ipc/ipc_mqueue.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to manipulate IPC message queues.
 */

#include <norma_ipc.h>

#include <mach/port.h>
#include <mach/message.h>
#include <kern/assert.h>
#include <kern/counters.h>
#include <kern/sched_prim.h>
#include <kern/ipc_sched.h>
#include <kern/ipc_kobject.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_thread.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_marequest.h>



#if	NORMA_IPC
extern ipc_mqueue_t	norma_ipc_handoff_mqueue;
extern ipc_kmsg_t	norma_ipc_handoff_msg;
extern mach_msg_size_t	norma_ipc_handoff_max_size;
extern mach_msg_size_t	norma_ipc_handoff_msg_size;
extern ipc_kmsg_t	norma_ipc_kmsg_accept();
#endif	NORMA_IPC

/*
 *	Routine:	ipc_mqueue_init
 *	Purpose:
 *		Initialize a newly-allocated message queue.
 */

void
ipc_mqueue_init(
	ipc_mqueue_t	mqueue)
{
	imq_lock_init(mqueue);
	ipc_kmsg_queue_init(&mqueue->imq_messages);
	ipc_thread_queue_init(&mqueue->imq_threads);
}

/*
 *	Routine:	ipc_mqueue_move
 *	Purpose:
 *		Move messages from one queue (source) to another (dest).
 *		Only moves messages sent to the specified port.
 *	Conditions:
 *		Both queues must be locked.
 *		(This is sufficient to manipulate port->ip_seqno.)
 */

void
ipc_mqueue_move(
	ipc_mqueue_t	dest,
	ipc_mqueue_t	source,
	ipc_port_t	port)
{
	ipc_kmsg_queue_t oldq, newq;
	ipc_thread_queue_t blockedq;
	ipc_kmsg_t kmsg, next;
	ipc_thread_t th;

	oldq = &source->imq_messages;
	newq = &dest->imq_messages;
	blockedq = &dest->imq_threads;

	for (kmsg = ipc_kmsg_queue_first(oldq);
	     kmsg != IKM_NULL; kmsg = next) {
		next = ipc_kmsg_queue_next(oldq, kmsg);

		/* only move messages sent to port */

		if (kmsg->ikm_header.msgh_remote_port != (mach_port_t) port)
			continue;

		ipc_kmsg_rmqueue(oldq, kmsg);

		/* before adding kmsg to newq, check for a blocked receiver */

		while ((th = ipc_thread_dequeue(blockedq)) != ITH_NULL) {
			assert(ipc_kmsg_queue_empty(newq));

			thread_go(th);

			/* check if the receiver can handle the message */

			if (kmsg->ikm_header.msgh_size <= th->ith_msize) {
				th->ith_state = MACH_MSG_SUCCESS;
				th->ith_kmsg = kmsg;
				th->ith_seqno = port->ip_seqno++;

				goto next_kmsg;
			}

			th->ith_state = MACH_RCV_TOO_LARGE;
			th->ith_msize = kmsg->ikm_header.msgh_size;
		}

		/* didn't find a receiver to handle the message */

		ipc_kmsg_enqueue(newq, kmsg);
	    next_kmsg:;
	}
}

/*
 *	Routine:	ipc_mqueue_changed
 *	Purpose:
 *		Wake up receivers waiting in a message queue.
 *	Conditions:
 *		The message queue is locked.
 */

void
ipc_mqueue_changed(
	ipc_mqueue_t		mqueue,
	mach_msg_return_t	mr)
{
	ipc_thread_t th;

	while ((th = ipc_thread_dequeue(&mqueue->imq_threads)) != ITH_NULL) {
		th->ith_state = mr;
		thread_go(th);
	}
}

/*
 *	Routine:	ipc_mqueue_send
 *	Purpose:
 *		Send a message to a port.  The message holds a reference
 *		for the destination port in the msgh_remote_port field.
 *
 *		If unsuccessful, the caller still has possession of
 *		the message and must do something with it.  If successful,
 *		the message is queued, given to a receiver, destroyed,
 *		or handled directly by the kernel via mach_msg.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	The message was accepted.
 *		MACH_SEND_TIMED_OUT	Caller still has message.
 *		MACH_SEND_INTERRUPTED	Caller still has message.
 */

mach_msg_return_t
ipc_mqueue_send(kmsg, option, time_out)
	ipc_kmsg_t kmsg;
	mach_msg_option_t option;
	mach_msg_timeout_t time_out;
{
	ipc_port_t port;

	port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
	assert(IP_VALID(port));

	ip_lock(port);

	if (port->ip_receiver == ipc_space_kernel) {
		ipc_kmsg_t reply;

		/*
		 *	We can check ip_receiver == ipc_space_kernel
		 *	before checking that the port is active because
		 *	ipc_port_dealloc_kernel clears ip_receiver
		 *	before destroying a kernel port.
		 */

		assert(ip_active(port));
		ip_unlock(port);

		reply = ipc_kobject_server(kmsg);
		if (reply != IKM_NULL)
			ipc_mqueue_send_always(reply);

		return MACH_MSG_SUCCESS;
	}

#if	NORMA_IPC
	if (IP_NORMA_IS_PROXY(port)) {
		mach_msg_return_t mr;

		mr = norma_ipc_send(kmsg);
		ip_unlock(port);
		return mr;
	}
#endif	NORMA_IPC

	for (;;) {
		ipc_thread_t self;

		/*
		 *	Can't deliver to a dead port.
		 *	However, we can pretend it got sent
		 *	and was then immediately destroyed.
		 */

		if (!ip_active(port)) {
			/*
			 *	We can't let ipc_kmsg_destroy deallocate
			 *	the port right, because we might end up
			 *	in an infinite loop trying to deliver
			 *	a send-once notification.
			 */

			ip_release(port);
			ip_check_unlock(port);
			kmsg->ikm_header.msgh_remote_port = MACH_PORT_NULL;
#if	NORMA_IPC
			/* XXX until ipc_kmsg_destroy is fixed... */
			norma_ipc_finish_receiving(&kmsg);
#endif	NORMA_IPC
			ipc_kmsg_destroy(kmsg);
			return MACH_MSG_SUCCESS;
		}

		/*
		 *  Don't block if:
		 *	1) We're under the queue limit.
		 *	2) Caller used the MACH_SEND_ALWAYS internal option.
		 *	3) Message is sent to a send-once right.
		 */

		if ((port->ip_msgcount < port->ip_qlimit) ||
		    (option & MACH_SEND_ALWAYS) ||
		    (MACH_MSGH_BITS_REMOTE(kmsg->ikm_header.msgh_bits) ==
						MACH_MSG_TYPE_PORT_SEND_ONCE))
			break;

		/* must block waiting for queue to clear */

		self = current_thread();

		if (option & MACH_SEND_TIMEOUT) {
			if (time_out == 0) {
				ip_unlock(port);
				return MACH_SEND_TIMED_OUT;
			}

			thread_will_wait_with_timeout(self, time_out);
		} else
			thread_will_wait(self);

		ipc_thread_enqueue(&port->ip_blocked, self);
		self->ith_state = MACH_SEND_IN_PROGRESS;

	 	ip_unlock(port);
		counter(c_ipc_mqueue_send_block++);
		thread_block((void (*)(void)) 0);
		ip_lock(port);

		/* why did we wake up? */

		if (self->ith_state == MACH_MSG_SUCCESS)
			continue;
		assert(self->ith_state == MACH_SEND_IN_PROGRESS);

		/* take ourselves off blocked queue */

		ipc_thread_rmqueue(&port->ip_blocked, self);

		/*
		 *	Thread wakeup-reason field tells us why
		 *	the wait was interrupted.
		 */

		switch (self->ith_wait_result) {
		    case THREAD_INTERRUPTED:
			/* send was interrupted - give up */

			ip_unlock(port);
			return MACH_SEND_INTERRUPTED;

		    case THREAD_TIMED_OUT:
			/* timeout expired */

			assert(option & MACH_SEND_TIMEOUT);
			time_out = 0;
			break;

		    case THREAD_RESTART:
		    default:
#if MACH_ASSERT
			assert(!"ipc_mqueue_send");
#else
			panic("ipc_mqueue_send");
#endif
		}
	}

	if (kmsg->ikm_header.msgh_bits & MACH_MSGH_BITS_CIRCULAR) {
		ip_unlock(port);

		/* don't allow the creation of a circular loop */

#if	NORMA_IPC
		/* XXX until ipc_kmsg_destroy is fixed... */
		norma_ipc_finish_receiving(&kmsg);
#endif	NORMA_IPC
		ipc_kmsg_destroy(kmsg);
		return MACH_MSG_SUCCESS;
	}

    {
	ipc_mqueue_t mqueue;
	ipc_pset_t pset;
	ipc_thread_t receiver;
	ipc_thread_queue_t receivers;

	port->ip_msgcount++;
	assert(port->ip_msgcount > 0);

	pset = port->ip_pset;
	if (pset == IPS_NULL)
		mqueue = &port->ip_messages;
	else
		mqueue = &pset->ips_messages;

	imq_lock(mqueue);
	receivers = &mqueue->imq_threads;

	/*
	 *	Can unlock the port now that the msg queue is locked
	 *	and we know the port is active.  While the msg queue
	 *	is locked, we have control of the kmsg, so the ref in
	 *	it for the port is still good.  If the msg queue is in
	 *	a set (dead or alive), then we're OK because the port
	 *	is still a member of the set and the set won't go away
	 *	until the port is taken out, which tries to lock the
	 *	set's msg queue to remove the port's msgs.
	 */

	ip_unlock(port);

	/* check for a receiver for the message */

#if	NORMA_IPC
	if (mqueue == norma_ipc_handoff_mqueue) {
		norma_ipc_handoff_msg = kmsg;
		if (kmsg->ikm_header.msgh_size <= norma_ipc_handoff_max_size) {
			imq_unlock(mqueue);
			return MACH_MSG_SUCCESS;
		}
		norma_ipc_handoff_msg_size = kmsg->ikm_header.msgh_size;
	}
#endif	NORMA_IPC
	for (;;) {
		receiver = ipc_thread_queue_first(receivers);
		if (receiver == ITH_NULL) {
			/* no receivers; queue kmsg */

			ipc_kmsg_enqueue_macro(&mqueue->imq_messages, kmsg);
			imq_unlock(mqueue);
			break;
		}

		ipc_thread_rmqueue_first_macro(receivers, receiver);
		assert(ipc_kmsg_queue_empty(&mqueue->imq_messages));

		if (kmsg->ikm_header.msgh_size <= receiver->ith_msize) {
			/* got a successful receiver */

			receiver->ith_state = MACH_MSG_SUCCESS;
			receiver->ith_kmsg = kmsg;
			receiver->ith_seqno = port->ip_seqno++;
			imq_unlock(mqueue);

			thread_go(receiver);
			break;
		}

		receiver->ith_state = MACH_RCV_TOO_LARGE;
		receiver->ith_msize = kmsg->ikm_header.msgh_size;
		thread_go(receiver);
	}
    }

	return MACH_MSG_SUCCESS;
}

/*
 *	Routine:	ipc_mqueue_copyin
 *	Purpose:
 *		Convert a name in a space to a message queue.
 *	Conditions:
 *		Nothing locked.  If successful, the message queue
 *		is returned locked and caller gets a ref for the object.
 *		This ref ensures the continued existence of the queue.
 *	Returns:
 *		MACH_MSG_SUCCESS	Found a message queue.
 *		MACH_RCV_INVALID_NAME	The space is dead.
 *		MACH_RCV_INVALID_NAME	The name doesn't denote a right.
 *		MACH_RCV_INVALID_NAME
 *			The denoted right is not receive or port set.
 *		MACH_RCV_IN_SET		Receive right is a member of a set.
 */

mach_msg_return_t
ipc_mqueue_copyin(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_mqueue_t	*mqueuep,
	ipc_object_t	*objectp)
{
	ipc_entry_t entry;
	ipc_entry_bits_t bits;
	ipc_object_t object;
	ipc_mqueue_t mqueue;

	is_read_lock(space);
	if (!space->is_active) {
		is_read_unlock(space);
		return MACH_RCV_INVALID_NAME;
	}

	entry = ipc_entry_lookup(space, name);
	if (entry == IE_NULL) {
		is_read_unlock(space);
		return MACH_RCV_INVALID_NAME;
	}

	bits = entry->ie_bits;
	object = entry->ie_object;

	if (bits & MACH_PORT_TYPE_RECEIVE) {
		ipc_port_t port;
		ipc_pset_t pset;

		port = (ipc_port_t) object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);
		is_read_unlock(space);

		pset = port->ip_pset;
		if (pset != IPS_NULL) {
			ips_lock(pset);
			if (ips_active(pset)) {
				ips_unlock(pset);
				ip_unlock(port);
				return MACH_RCV_IN_SET;
			}

			ipc_pset_remove(pset, port);
			ips_check_unlock(pset);
			assert(port->ip_pset == IPS_NULL);
		}

		mqueue = &port->ip_messages;
	} else if (bits & MACH_PORT_TYPE_PORT_SET) {
		ipc_pset_t pset;

		pset = (ipc_pset_t) object;
		assert(pset != IPS_NULL);

		ips_lock(pset);
		assert(ips_active(pset));
		assert(pset->ips_local_name == name);
		is_read_unlock(space);

		mqueue = &pset->ips_messages;
	} else {
		is_read_unlock(space);
		return MACH_RCV_INVALID_NAME;
	}

	/*
	 *	At this point, the object is locked and active,
	 *	the space is unlocked, and mqueue is initialized.
	 */

	io_reference(object);
	imq_lock(mqueue);
	io_unlock(object);

	*objectp = object;
	*mqueuep = mqueue;
	return MACH_MSG_SUCCESS;
}

/*
 *	Routine:	ipc_mqueue_receive
 *	Purpose:
 *		Receive a message from a message queue.
 *
 *		If continuation is non-zero, then we might discard
 *		our kernel stack when we block.  We will continue
 *		after unblocking by executing continuation.
 *
 *		If resume is true, then we are resuming a receive
 *		operation after a blocked receive discarded our stack.
 *	Conditions:
 *		The message queue is locked; it will be returned unlocked.
 *
 *		Our caller must hold a reference for the port or port set
 *		to which this queue belongs, to keep the queue
 *		from being deallocated.  Furthermore, the port or set
 *		must have been active when the queue was locked.
 *
 *		The kmsg is returned with clean header fields
 *		and with the circular bit turned off.
 *	Returns:
 *		MACH_MSG_SUCCESS	Message returned in kmsgp.
 *		MACH_RCV_TOO_LARGE	Message size returned in kmsgp.
 *		MACH_RCV_TIMED_OUT	No message obtained.
 *		MACH_RCV_INTERRUPTED	No message obtained.
 *		MACH_RCV_PORT_DIED	Port/set died; no message.
 *		MACH_RCV_PORT_CHANGED	Port moved into set; no msg.
 *
 */

mach_msg_return_t
ipc_mqueue_receive(
	ipc_mqueue_t		mqueue,
	mach_msg_option_t	option,
	mach_msg_size_t		max_size,
	mach_msg_timeout_t	time_out,
	boolean_t		resume,
	void			(*continuation)(void),
	ipc_kmsg_t		*kmsgp,
	mach_port_seqno_t	*seqnop)
{
	ipc_port_t port;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;

    {
	ipc_kmsg_queue_t kmsgs = &mqueue->imq_messages;
	ipc_thread_t self = current_thread();

	if (resume)
		goto after_thread_block;

	for (;;) {
		kmsg = ipc_kmsg_queue_first(kmsgs);
#if	NORMA_IPC
		/*
		 * It may be possible to make this work even when a timeout
		 * is specified.
		 *
		 * Netipc_replenish should be moved somewhere else.
		 */
		if (kmsg == IKM_NULL && ! (option & MACH_RCV_TIMEOUT)) {
			netipc_replenish(FALSE);
			*kmsgp = IKM_NULL;
			kmsg = norma_ipc_kmsg_accept(mqueue, max_size,
						     (mach_msg_size_t *)kmsgp);
			if (kmsg != IKM_NULL) {
				port = (ipc_port_t)
				    kmsg->ikm_header.msgh_remote_port;
				seqno = port->ip_seqno++;
				break;
			}
			if (*kmsgp) {
				imq_unlock(mqueue);
				return MACH_RCV_TOO_LARGE;
			}
		}
#endif	NORMA_IPC
		if (kmsg != IKM_NULL) {
			/* check space requirements */

			if (kmsg->ikm_header.msgh_size > max_size) {
				* (mach_msg_size_t *) kmsgp =
					kmsg->ikm_header.msgh_size;
				imq_unlock(mqueue);
				return MACH_RCV_TOO_LARGE;
			}

			ipc_kmsg_rmqueue_first_macro(kmsgs, kmsg);
			port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
			seqno = port->ip_seqno++;
			break;
		}

		/* must block waiting for a message */

		if (option & MACH_RCV_TIMEOUT) {
			if (time_out == 0) {
				imq_unlock(mqueue);
				return MACH_RCV_TIMED_OUT;
			}

			thread_will_wait_with_timeout(self, time_out);
		} else
			thread_will_wait(self);

		ipc_thread_enqueue_macro(&mqueue->imq_threads, self);
		self->ith_state = MACH_RCV_IN_PROGRESS;
		self->ith_msize = max_size;

		imq_unlock(mqueue);
		if (continuation != (void (*)(void)) 0) {
			counter(c_ipc_mqueue_receive_block_user++);
		} else {
			counter(c_ipc_mqueue_receive_block_kernel++);
		}
		thread_block(continuation);
	after_thread_block:
		imq_lock(mqueue);

		/* why did we wake up? */

		if (self->ith_state == MACH_MSG_SUCCESS) {
			/* pick up the message that was handed to us */

			kmsg = self->ith_kmsg;
			seqno = self->ith_seqno;
			port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
			break;
		}

		switch (self->ith_state) {
		    case MACH_RCV_TOO_LARGE:
			/* pick up size of the too-large message */

			* (mach_msg_size_t *) kmsgp = self->ith_msize;
			/* fall-through */

		    case MACH_RCV_PORT_DIED:
		    case MACH_RCV_PORT_CHANGED:
			/* something bad happened to the port/set */

			imq_unlock(mqueue);
			return self->ith_state;

		    case MACH_RCV_IN_PROGRESS:
			/*
			 *	Awakened for other than IPC completion.
			 *	Remove ourselves from the waiting queue,
			 *	then check the wakeup cause.
			 */

			ipc_thread_rmqueue(&mqueue->imq_threads, self);

			switch (self->ith_wait_result) {
			    case THREAD_INTERRUPTED:
				/* receive was interrupted - give up */

				imq_unlock(mqueue);
				return MACH_RCV_INTERRUPTED;

			    case THREAD_TIMED_OUT:
				/* timeout expired */

				assert(option & MACH_RCV_TIMEOUT);
				time_out = 0;
				break;

			    case THREAD_RESTART:
			    default:
#if MACH_ASSERT
				assert(!"ipc_mqueue_receive");
#else
				panic("ipc_mqueue_receive");
#endif
			}
			break;

		    default:
#if MACH_ASSERT
			assert(!"ipc_mqueue_receive: strange ith_state");
#else
			panic("ipc_mqueue_receive: strange ith_state");
#endif
		}
	}

	/* we have a kmsg; unlock the msg queue */

	imq_unlock(mqueue);
	assert(kmsg->ikm_header.msgh_size <= max_size);
    }

    {
	ipc_marequest_t marequest;

	marequest = kmsg->ikm_marequest;
	if (marequest != IMAR_NULL) {
		ipc_marequest_destroy(marequest);
		kmsg->ikm_marequest = IMAR_NULL;
	}
	assert((kmsg->ikm_header.msgh_bits & MACH_MSGH_BITS_CIRCULAR) == 0);

	assert(port == (ipc_port_t) kmsg->ikm_header.msgh_remote_port);
	ip_lock(port);

	if (ip_active(port)) {
		ipc_thread_queue_t senders;
		ipc_thread_t sender;

		assert(port->ip_msgcount > 0);
		port->ip_msgcount--;

		senders = &port->ip_blocked;
		sender = ipc_thread_queue_first(senders);

		if ((sender != ITH_NULL) &&
		    (port->ip_msgcount < port->ip_qlimit)) {
			ipc_thread_rmqueue(senders, sender);
			sender->ith_state = MACH_MSG_SUCCESS;
			thread_go(sender);
		}
	}

	ip_unlock(port);
    }

#if	NORMA_IPC
	norma_ipc_finish_receiving(&kmsg);
#endif	NORMA_IPC
	*kmsgp = kmsg;
	*seqnop = seqno;
	return MACH_MSG_SUCCESS;
}
