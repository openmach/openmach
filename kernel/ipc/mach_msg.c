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
 *	File:	ipc/mach_msg.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Exported message traps.  See mach/message.h.
 */

#include <mach_ipc_compat.h>
#include <norma_ipc.h>

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/message.h>
#include <kern/assert.h>
#include <kern/counters.h>
#include <kern/lock.h>
#include <kern/sched_prim.h>
#include <kern/ipc_sched.h>
#include <vm/vm_map.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_marequest.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_thread.h>
#include <ipc/ipc_entry.h>
#include <ipc/mach_msg.h>



extern void exception_raise_continue();
extern void exception_raise_continue_fast();
#ifndef CONTINUATIONS
#define mach_msg_receive_continue	0
#define msg_receive_continue		0
#endif

/*
 *	Routine:	mach_msg_send
 *	Purpose:
 *		Send a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	Sent the message.
 *		MACH_SEND_MSG_TOO_SMALL	Message smaller than a header.
 *		MACH_SEND_NO_BUFFER	Couldn't allocate buffer.
 *		MACH_SEND_INVALID_DATA	Couldn't copy message data.
 *		MACH_SEND_INVALID_HEADER
 *			Illegal value in the message header bits.
 *		MACH_SEND_INVALID_DEST	The space is dead.
 *		MACH_SEND_INVALID_NOTIFY	Bad notify port.
 *		MACH_SEND_INVALID_DEST	Can't copyin destination port.
 *		MACH_SEND_INVALID_REPLY	Can't copyin reply port.
 *		MACH_SEND_TIMED_OUT	Timeout expired without delivery.
 *		MACH_SEND_INTERRUPTED	Delivery interrupted.
 *		MACH_SEND_NO_NOTIFY	Can't allocate a msg-accepted request.
 *		MACH_SEND_WILL_NOTIFY	Msg-accepted notif. requested.
 *		MACH_SEND_NOTIFY_IN_PROGRESS
 *			This space has already forced a message to this port.
 */

mach_msg_return_t
mach_msg_send(msg, option, send_size, time_out, notify)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_timeout_t time_out;
	mach_port_t notify;
{
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_kmsg_t kmsg;
	mach_msg_return_t mr;

	mr = ipc_kmsg_get(msg, send_size, &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		return mr;

	if (option & MACH_SEND_CANCEL) {
		if (notify == MACH_PORT_NULL)
			mr = MACH_SEND_INVALID_NOTIFY;
		else
			mr = ipc_kmsg_copyin(kmsg, space, map, notify);
	} else
		mr = ipc_kmsg_copyin(kmsg, space, map, MACH_PORT_NULL);
	if (mr != MACH_MSG_SUCCESS) {
		ikm_free(kmsg);
		return mr;
	}

	if (option & MACH_SEND_NOTIFY) {
		mr = ipc_mqueue_send(kmsg, MACH_SEND_TIMEOUT,
				     ((option & MACH_SEND_TIMEOUT) ?
				      time_out : MACH_MSG_TIMEOUT_NONE));
		if (mr == MACH_SEND_TIMED_OUT) {
			ipc_port_t dest = (ipc_port_t)
				kmsg->ikm_header.msgh_remote_port;

			if (notify == MACH_PORT_NULL)
				mr = MACH_SEND_INVALID_NOTIFY;
			else
				mr = ipc_marequest_create(space, dest,
						notify, &kmsg->ikm_marequest);
			if (mr == MACH_MSG_SUCCESS) {
				ipc_mqueue_send_always(kmsg);
				return MACH_SEND_WILL_NOTIFY;
			}
		}
	} else
		mr = ipc_mqueue_send(kmsg, option & MACH_SEND_TIMEOUT,
				     time_out);

	if (mr != MACH_MSG_SUCCESS) {
		mr |= ipc_kmsg_copyout_pseudo(kmsg, space, map);

		assert(kmsg->ikm_marequest == IMAR_NULL);
		(void) ipc_kmsg_put(msg, kmsg, kmsg->ikm_header.msgh_size);
	}

	return mr;
}

/*
 *	Routine:	mach_msg_receive
 *	Purpose:
 *		Receive a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	Received a message.
 *		MACH_RCV_INVALID_NAME	The name doesn't denote a right,
 *			or the denoted right is not receive or port set.
 *		MACH_RCV_IN_SET		Receive right is a member of a set.
 *		MACH_RCV_TOO_LARGE	Message wouldn't fit into buffer.
 *		MACH_RCV_TIMED_OUT	Timeout expired without a message.
 *		MACH_RCV_INTERRUPTED	Reception interrupted.
 *		MACH_RCV_PORT_DIED	Port/set died while receiving.
 *		MACH_RCV_PORT_CHANGED	Port moved into set while receiving.
 *		MACH_RCV_INVALID_DATA	Couldn't copy to user buffer.
 *		MACH_RCV_INVALID_NOTIFY	Bad notify port.
 *		MACH_RCV_HEADER_ERROR
 */

mach_msg_return_t
mach_msg_receive(msg, option, rcv_size, rcv_name, time_out, notify)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t rcv_size;
	mach_port_t rcv_name;
	mach_msg_timeout_t time_out;
	mach_port_t notify;
{
	ipc_thread_t self = current_thread();
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_object_t object;
	ipc_mqueue_t mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	mr = ipc_mqueue_copyin(space, rcv_name, &mqueue, &object);
	if (mr != MACH_MSG_SUCCESS)
		return mr;
	/* hold ref for object; mqueue is locked */

#ifdef CONTINUATIONS
	/*
	 *	ipc_mqueue_receive may not return, because if we block
	 *	then our kernel stack may be discarded.  So we save
	 *	state here for mach_msg_receive_continue to pick up.
	 */

	self->ith_msg = msg;
	self->ith_option = option;
	self->ith_rcv_size = rcv_size;
	self->ith_timeout = time_out;
	self->ith_notify = notify;
	self->ith_object = object;
	self->ith_mqueue = mqueue;
#endif

	if (option & MACH_RCV_LARGE) {
		mr = ipc_mqueue_receive(mqueue, option & MACH_RCV_TIMEOUT,
					rcv_size, time_out,
					FALSE, mach_msg_receive_continue,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		ipc_object_release(object);
		if (mr != MACH_MSG_SUCCESS) {
			if (mr == MACH_RCV_TOO_LARGE) {
				mach_msg_size_t real_size =
					(mach_msg_size_t) (natural_t) kmsg;

				assert(real_size > rcv_size);

				(void) copyout((vm_offset_t) &real_size,
					       (vm_offset_t) &msg->msgh_size,
					       sizeof(mach_msg_size_t));
			}

			return mr;
		}

		kmsg->ikm_header.msgh_seqno = seqno;
		assert(kmsg->ikm_header.msgh_size <= rcv_size);
	} else {
		mr = ipc_mqueue_receive(mqueue, option & MACH_RCV_TIMEOUT,
					MACH_MSG_SIZE_MAX, time_out,
					FALSE, mach_msg_receive_continue,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		ipc_object_release(object);
		if (mr != MACH_MSG_SUCCESS)
			return mr;

		kmsg->ikm_header.msgh_seqno = seqno;
		if (kmsg->ikm_header.msgh_size > rcv_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			return MACH_RCV_TOO_LARGE;
		}
	}

	if (option & MACH_RCV_NOTIFY) {
		if (notify == MACH_PORT_NULL)
			mr = MACH_RCV_INVALID_NOTIFY;
		else
			mr = ipc_kmsg_copyout(kmsg, space, map, notify);
	} else
		mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
	if (mr != MACH_MSG_SUCCESS) {
		if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
			(void) ipc_kmsg_put(msg, kmsg,
					    kmsg->ikm_header.msgh_size);
		} else {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
		}

		return mr;
	}

	return ipc_kmsg_put(msg, kmsg, kmsg->ikm_header.msgh_size);
}

#ifdef CONTINUATIONS
/*
 *	Routine:	mach_msg_receive_continue
 *	Purpose:
 *		Continue after blocking for a message.
 *	Conditions:
 *		Nothing locked.  We are running on a new kernel stack,
 *		with the receive state saved in the thread.  From here
 *		control goes back to user space.
 */

void
mach_msg_receive_continue()
{
	ipc_thread_t self = current_thread();
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	mach_msg_header_t *msg = self->ith_msg;
	mach_msg_option_t option = self->ith_option;
	mach_msg_size_t rcv_size = self->ith_rcv_size;
	mach_msg_timeout_t time_out = self->ith_timeout;
	mach_port_t notify = self->ith_notify;
	ipc_object_t object = self->ith_object;
	ipc_mqueue_t mqueue = self->ith_mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	if (option & MACH_RCV_LARGE) {
		mr = ipc_mqueue_receive(mqueue, option & MACH_RCV_TIMEOUT,
					rcv_size, time_out,
					TRUE, mach_msg_receive_continue,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		ipc_object_release(object);
		if (mr != MACH_MSG_SUCCESS) {
			if (mr == MACH_RCV_TOO_LARGE) {
				mach_msg_size_t real_size =
					(mach_msg_size_t) (natural_t) kmsg;

				assert(real_size > rcv_size);

				(void) copyout((vm_offset_t) &real_size,
					       (vm_offset_t) &msg->msgh_size,
					       sizeof(mach_msg_size_t));
			}

			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		kmsg->ikm_header.msgh_seqno = seqno;
		assert(kmsg->ikm_header.msgh_size <= rcv_size);
	} else {
		mr = ipc_mqueue_receive(mqueue, option & MACH_RCV_TIMEOUT,
					MACH_MSG_SIZE_MAX, time_out,
					TRUE, mach_msg_receive_continue,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		ipc_object_release(object);
		if (mr != MACH_MSG_SUCCESS) {
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		kmsg->ikm_header.msgh_seqno = seqno;
		if (kmsg->ikm_header.msgh_size > rcv_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			thread_syscall_return(MACH_RCV_TOO_LARGE);
			/*NOTREACHED*/
		}
	}

	if (option & MACH_RCV_NOTIFY) {
		if (notify == MACH_PORT_NULL)
			mr = MACH_RCV_INVALID_NOTIFY;
		else
			mr = ipc_kmsg_copyout(kmsg, space, map, notify);
	} else
		mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
	if (mr != MACH_MSG_SUCCESS) {
		if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
			(void) ipc_kmsg_put(msg, kmsg,
					    kmsg->ikm_header.msgh_size);
		} else {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
		}

		thread_syscall_return(mr);
		/*NOTREACHED*/
	}

	mr = ipc_kmsg_put(msg, kmsg, kmsg->ikm_header.msgh_size);
	thread_syscall_return(mr);
	/*NOTREACHED*/
}
#endif /* CONTINUATIONS */

/*
 *	Routine:	mach_msg_trap [mach trap]
 *	Purpose:
 *		Possibly send a message; possibly receive a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		All of mach_msg_send and mach_msg_receive error codes.
 */

mach_msg_return_t
mach_msg_trap(msg, option, send_size, rcv_size, rcv_name, time_out, notify)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_size_t rcv_size;
	mach_port_t rcv_name;
	mach_msg_timeout_t time_out;
	mach_port_t notify;
{
	mach_msg_return_t mr;

#ifdef	CONTINUATIONS
	/* first check for common cases */

	if (option == (MACH_SEND_MSG|MACH_RCV_MSG)) {
		register ipc_thread_t self = current_thread();
		ipc_space_t space = self->task->itk_space;
		register ipc_kmsg_t kmsg;
		register ipc_port_t dest_port;
		ipc_object_t rcv_object;
		register ipc_mqueue_t rcv_mqueue;
		mach_msg_size_t reply_size;

		/*
		 *	This case is divided into ten sections, each
		 *	with a label.  There are five optimized
		 *	sections and six unoptimized sections, which
		 *	do the same thing but handle all possible
		 *	cases and are slower.
		 *
		 *	The five sections for an RPC are
		 *	    1) Get request message into a buffer.
		 *		(fast_get or slow_get)
		 *	    2) Copyin request message and rcv_name.
		 *		(fast_copyin or slow_copyin)
		 *	    3) Enqueue request and dequeue reply.
		 *		(fast_send_receive or
		 *		 slow_send and slow_receive)
		 *	    4) Copyout reply message.
		 *		(fast_copyout or slow_copyout)
		 *	    5) Put reply message to user's buffer.
		 *		(fast_put or slow_put)
		 *
		 *	Keep the locking hierarchy firmly in mind.
		 *	(First spaces, then ports, then port sets,
		 *	then message queues.)  Only a non-blocking
		 *	attempt can be made to acquire locks out of
		 *	order, or acquire two locks on the same level.
		 *	Acquiring two locks on the same level will
		 *	fail if the objects are really the same,
		 *	unless simple locking is disabled.  This is OK,
		 *	because then the extra unlock does nothing.
		 *
		 *	There are two major reasons these RPCs can't use
		 *	ipc_thread_switch, and use slow_send/slow_receive:
		 *		1) Kernel RPCs.
		 *		2) Servers fall behind clients, so
		 *		client doesn't find a blocked server thread and
		 *		server finds waiting messages and can't block.
		 */

	/*
	    fast_get:
	*/
		/*
		 *	optimized ipc_kmsg_get
		 *
		 *	No locks, references, or messages held.
		 *	We must clear ikm_cache before copyinmsg.
		 */

		if ((send_size > IKM_SAVED_MSG_SIZE) ||
		    (send_size < sizeof(mach_msg_header_t)) ||
		    (send_size & 3) ||
		    ((kmsg = ikm_cache()) == IKM_NULL))
			goto slow_get;

		ikm_cache() = IKM_NULL;
		ikm_check_initialized(kmsg, IKM_SAVED_KMSG_SIZE);

		if (copyinmsg((vm_offset_t) msg, (vm_offset_t) &kmsg->ikm_header,
			      send_size)) {
			ikm_free(kmsg);
			goto slow_get;
		}

		kmsg->ikm_header.msgh_size = send_size;

	    fast_copyin:
		/*
		 *	optimized ipc_kmsg_copyin/ipc_mqueue_copyin
		 *
		 *	We have the request message data in kmsg.
		 *	Must still do copyin, send, receive, etc.
		 *
		 *	If the message isn't simple, we can't combine
		 *	ipc_kmsg_copyin_header and ipc_mqueue_copyin,
		 *	because copyin of the message body might
		 *	affect rcv_name.
		 */

		switch (kmsg->ikm_header.msgh_bits) {
		    case MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
					MACH_MSG_TYPE_MAKE_SEND_ONCE): {
			register ipc_entry_t table;
			register ipc_entry_num_t size;
			register ipc_port_t reply_port;

			/* sending a request message */

		    {
			register mach_port_index_t index;
			register mach_port_gen_t gen;

		    {
			register mach_port_t reply_name =
				kmsg->ikm_header.msgh_local_port;

			if (reply_name != rcv_name)
				goto slow_copyin;

			/* optimized ipc_entry_lookup of reply_name */

			index = MACH_PORT_INDEX(reply_name);
			gen = MACH_PORT_GEN(reply_name);
		    }

			is_read_lock(space);
			assert(space->is_active);

			size = space->is_table_size;
			table = space->is_table;

			if (index >= size)
				goto abort_request_copyin;

		    {
			register ipc_entry_t entry;
			register ipc_entry_bits_t bits;

			entry = &table[index];
			bits = entry->ie_bits;

			/* check generation number and type bit */

			if ((bits & (IE_BITS_GEN_MASK|
				     MACH_PORT_TYPE_RECEIVE)) !=
			    (gen | MACH_PORT_TYPE_RECEIVE))
				goto abort_request_copyin;

			reply_port = (ipc_port_t) entry->ie_object;
			assert(reply_port != IP_NULL);
		    }
		    }

			/* optimized ipc_entry_lookup of dest_name */

		    {
			register mach_port_index_t index;
			register mach_port_gen_t gen;

		    {
			register mach_port_t dest_name =
				kmsg->ikm_header.msgh_remote_port;

			index = MACH_PORT_INDEX(dest_name);
			gen = MACH_PORT_GEN(dest_name);
		    }

			if (index >= size)
				goto abort_request_copyin;

		    {
			register ipc_entry_t entry;
			register ipc_entry_bits_t bits;

			entry = &table[index];
			bits = entry->ie_bits;

			/* check generation number and type bit */

			if ((bits & (IE_BITS_GEN_MASK|MACH_PORT_TYPE_SEND)) !=
			    (gen | MACH_PORT_TYPE_SEND))
				goto abort_request_copyin;

			assert(IE_BITS_UREFS(bits) > 0);

			dest_port = (ipc_port_t) entry->ie_object;
			assert(dest_port != IP_NULL);
		    }
		    }

			/*
			 *	To do an atomic copyin, need simultaneous
			 *	locks on both ports and the space.  If
			 *	dest_port == reply_port, and simple locking is
			 *	enabled, then we will abort.  Otherwise it's
			 *	OK to unlock twice.
			 */

			ip_lock(dest_port);
			if (!ip_active(dest_port) ||
			    !ip_lock_try(reply_port)) {
				ip_unlock(dest_port);
				goto abort_request_copyin;
			}
			is_read_unlock(space);

			assert(dest_port->ip_srights > 0);
			dest_port->ip_srights++;
			ip_reference(dest_port);

			assert(ip_active(reply_port));
			assert(reply_port->ip_receiver_name ==
			       kmsg->ikm_header.msgh_local_port);
			assert(reply_port->ip_receiver == space);

			reply_port->ip_sorights++;
			ip_reference(reply_port);

			kmsg->ikm_header.msgh_bits =
				MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND,
					       MACH_MSG_TYPE_PORT_SEND_ONCE);
			kmsg->ikm_header.msgh_remote_port =
					(mach_port_t) dest_port;
			kmsg->ikm_header.msgh_local_port =
					(mach_port_t) reply_port;

			/* make sure we can queue to the destination */

			if (dest_port->ip_receiver == ipc_space_kernel) {
				/*
				 * The kernel server has a reference to
				 * the reply port, which it hands back
				 * to us in the reply message.  We do
				 * not need to keep another reference to
				 * it.
				 */
				ip_unlock(reply_port);

				assert(ip_active(dest_port));
				ip_unlock(dest_port);
				goto kernel_send;
			}

#if	NORMA_IPC
			if (IP_NORMA_IS_PROXY(dest_port)) {
				ip_unlock(dest_port);
				ip_unlock(reply_port);
				goto norma_send;
		        }
#endif	NORMA_IPC

			if (dest_port->ip_msgcount >= dest_port->ip_qlimit)
				goto abort_request_send_receive;

			/* optimized ipc_mqueue_copyin */

			if (reply_port->ip_pset != IPS_NULL)
				goto abort_request_send_receive;

			rcv_object = (ipc_object_t) reply_port;
			io_reference(rcv_object);
			rcv_mqueue = &reply_port->ip_messages;
			imq_lock(rcv_mqueue);
			io_unlock(rcv_object);
			goto fast_send_receive;

		    abort_request_copyin:
			is_read_unlock(space);
			goto slow_copyin;

		    abort_request_send_receive:
			ip_unlock(dest_port);
			ip_unlock(reply_port);
			goto slow_send;
		    }

		    case MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0): {
			register ipc_entry_num_t size;
			register ipc_entry_t table;

			/* sending a reply message */

		    {
			register mach_port_t reply_name =
				kmsg->ikm_header.msgh_local_port;

			if (reply_name != MACH_PORT_NULL)
				goto slow_copyin;
		    }

			is_write_lock(space);
			assert(space->is_active);

			/* optimized ipc_entry_lookup */

			size = space->is_table_size;
			table = space->is_table;

		    {
			register ipc_entry_t entry;
			register mach_port_gen_t gen;
			register mach_port_index_t index;

		    {
			register mach_port_t dest_name =
				kmsg->ikm_header.msgh_remote_port;

			index = MACH_PORT_INDEX(dest_name);
			gen = MACH_PORT_GEN(dest_name);
		    }

			if (index >= size)
				goto abort_reply_dest_copyin;

			entry = &table[index];

			/* check generation, collision bit, and type bit */

			if ((entry->ie_bits & (IE_BITS_GEN_MASK|
					       IE_BITS_COLLISION|
					       MACH_PORT_TYPE_SEND_ONCE)) !=
			    (gen | MACH_PORT_TYPE_SEND_ONCE))
				goto abort_reply_dest_copyin;

			/* optimized ipc_right_copyin */

			assert(IE_BITS_TYPE(entry->ie_bits) ==
						MACH_PORT_TYPE_SEND_ONCE);
			assert(IE_BITS_UREFS(entry->ie_bits) == 1);
			assert((entry->ie_bits & IE_BITS_MAREQUEST) == 0);

			if (entry->ie_request != 0)
				goto abort_reply_dest_copyin;

			dest_port = (ipc_port_t) entry->ie_object;
			assert(dest_port != IP_NULL);

			ip_lock(dest_port);
			if (!ip_active(dest_port)) {
				ip_unlock(dest_port);
				goto abort_reply_dest_copyin;
			}

			assert(dest_port->ip_sorights > 0);

			/* optimized ipc_entry_dealloc */

			entry->ie_next = table->ie_next;
			table->ie_next = index;
			entry->ie_bits = gen;
			entry->ie_object = IO_NULL;
		    }

			kmsg->ikm_header.msgh_bits =
				MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE,
					       0);
			kmsg->ikm_header.msgh_remote_port =
					(mach_port_t) dest_port;

			/* make sure we can queue to the destination */

			assert(dest_port->ip_receiver != ipc_space_kernel);
#if	NORMA_IPC
			if (IP_NORMA_IS_PROXY(dest_port)) {
				is_write_unlock(space);
				ip_unlock(dest_port);
				goto norma_send;
			}
#endif	NORMA_IPC

			/* optimized ipc_entry_lookup/ipc_mqueue_copyin */

		    {
			register ipc_entry_t entry;
			register ipc_entry_bits_t bits;

		    {
			register mach_port_index_t index;
			register mach_port_gen_t gen;

			index = MACH_PORT_INDEX(rcv_name);
			gen = MACH_PORT_GEN(rcv_name);

			if (index >= size)
				goto abort_reply_rcv_copyin;

			entry = &table[index];
			bits = entry->ie_bits;

			/* check generation number */

			if ((bits & IE_BITS_GEN_MASK) != gen)
				goto abort_reply_rcv_copyin;
		    }

			/* check type bits; looking for receive or set */

			if (bits & MACH_PORT_TYPE_PORT_SET) {
				register ipc_pset_t rcv_pset;

				rcv_pset = (ipc_pset_t) entry->ie_object;
				assert(rcv_pset != IPS_NULL);

				ips_lock(rcv_pset);
				assert(ips_active(rcv_pset));

				rcv_object = (ipc_object_t) rcv_pset;
				rcv_mqueue = &rcv_pset->ips_messages;
			} else if (bits & MACH_PORT_TYPE_RECEIVE) {
				register ipc_port_t rcv_port;

				rcv_port = (ipc_port_t) entry->ie_object;
				assert(rcv_port != IP_NULL);

				if (!ip_lock_try(rcv_port))
					goto abort_reply_rcv_copyin;
				assert(ip_active(rcv_port));

				if (rcv_port->ip_pset != IPS_NULL) {
					ip_unlock(rcv_port);
					goto abort_reply_rcv_copyin;
				}

				rcv_object = (ipc_object_t) rcv_port;
				rcv_mqueue = &rcv_port->ip_messages;
			} else
				goto abort_reply_rcv_copyin;
		    }

			is_write_unlock(space);
			io_reference(rcv_object);
			imq_lock(rcv_mqueue);
			io_unlock(rcv_object);
			goto fast_send_receive;

		    abort_reply_dest_copyin:
			is_write_unlock(space);
			goto slow_copyin;

		    abort_reply_rcv_copyin:
			ip_unlock(dest_port);
			is_write_unlock(space);
			goto slow_send;
		    }

		    default:
			goto slow_copyin;
		}
		/*NOTREACHED*/

	    fast_send_receive:
		/*
		 *	optimized ipc_mqueue_send/ipc_mqueue_receive
		 *
		 *	Finished get/copyin of kmsg and copyin of rcv_name.
		 *	space is unlocked, dest_port is locked,
		 *	we can queue kmsg to dest_port,
		 *	rcv_mqueue is locked, rcv_object holds a ref,
		 *	if rcv_object is a port it isn't in a port set
		 *
		 *	Note that if simple locking is turned off,
		 *	then we could have dest_mqueue == rcv_mqueue
		 *	and not abort when we try to lock dest_mqueue.
		 */

		assert(ip_active(dest_port));
		assert(dest_port->ip_receiver != ipc_space_kernel);
#if	NORMA_IPC
		assert(! IP_NORMA_IS_PROXY(dest_port));
#endif	NORMA_IPC
		assert((dest_port->ip_msgcount < dest_port->ip_qlimit) ||
		       (MACH_MSGH_BITS_REMOTE(kmsg->ikm_header.msgh_bits) ==
						MACH_MSG_TYPE_PORT_SEND_ONCE));
		assert((kmsg->ikm_header.msgh_bits &
						MACH_MSGH_BITS_CIRCULAR) == 0);

	    {
		register ipc_mqueue_t dest_mqueue;
		register ipc_thread_t receiver;

	    {
		register ipc_pset_t dest_pset;

		dest_pset = dest_port->ip_pset;
		if (dest_pset == IPS_NULL)
			dest_mqueue = &dest_port->ip_messages;
		else
			dest_mqueue = &dest_pset->ips_messages;
	    }

		if (!imq_lock_try(dest_mqueue)) {
		    abort_send_receive:
			ip_unlock(dest_port);
			imq_unlock(rcv_mqueue);
			ipc_object_release(rcv_object);
			goto slow_send;
		}

		receiver = ipc_thread_queue_first(&dest_mqueue->imq_threads);
		if ((receiver == ITH_NULL) ||
		    (ipc_kmsg_queue_first(&rcv_mqueue->imq_messages)
								!= IKM_NULL)) {
			imq_unlock(dest_mqueue);
			goto abort_send_receive;
		}

		/*
		 *	There is a receiver thread waiting, and
		 *	there is no reply message for us to pick up.
		 *	We have hope of hand-off, so save state.
		 */

		self->ith_msg = msg;
		self->ith_rcv_size = rcv_size;
		self->ith_object = rcv_object;
		self->ith_mqueue = rcv_mqueue;

		if ((receiver->swap_func == (void (*)()) mach_msg_continue) &&
		    thread_handoff(self, mach_msg_continue, receiver)) {
			assert(current_thread() == receiver);

			/*
			 *	We can use the optimized receive code,
			 *	because the receiver is using no options.
			 */
		} else if ((receiver->swap_func ==
				(void (*)()) exception_raise_continue) &&
			   thread_handoff(self, mach_msg_continue, receiver)) {
			counter(c_mach_msg_trap_block_exc++);
			assert(current_thread() == receiver);

			/*
			 *	We are a reply message coming back through
			 *	the optimized exception-handling path.
			 *	Finish with rcv_mqueue and dest_mqueue,
			 *	and then jump to exception code with
			 *	dest_port still locked.  We don't bother
			 *	with a sequence number in this case.
			 */

			ipc_thread_enqueue_macro(
				&rcv_mqueue->imq_threads, self);
			self->ith_state = MACH_RCV_IN_PROGRESS;
			self->ith_msize = MACH_MSG_SIZE_MAX;
			imq_unlock(rcv_mqueue);

			ipc_thread_rmqueue_first_macro(
				&dest_mqueue->imq_threads, receiver);
			imq_unlock(dest_mqueue);

			exception_raise_continue_fast(dest_port, kmsg);
			/*NOTREACHED*/
			return MACH_MSG_SUCCESS;
		} else if ((send_size <= receiver->ith_msize) &&
			   thread_handoff(self, mach_msg_continue, receiver)) {
			assert(current_thread() == receiver);

			if ((receiver->swap_func ==
				(void (*)()) mach_msg_receive_continue) &&
			    ((receiver->ith_option & MACH_RCV_NOTIFY) == 0)) {
				/*
				 *	We can still use the optimized code.
				 */
			} else {
				counter(c_mach_msg_trap_block_slow++);
				/*
				 *	We are running as the receiver,
				 *	but we can't use the optimized code.
				 *	Finish send/receive processing.
				 */

				dest_port->ip_msgcount++;
				ip_unlock(dest_port);

				ipc_thread_enqueue_macro(
					&rcv_mqueue->imq_threads, self);
				self->ith_state = MACH_RCV_IN_PROGRESS;
				self->ith_msize = MACH_MSG_SIZE_MAX;
				imq_unlock(rcv_mqueue);

				ipc_thread_rmqueue_first_macro(
					&dest_mqueue->imq_threads, receiver);
				receiver->ith_state = MACH_MSG_SUCCESS;
				receiver->ith_kmsg = kmsg;
				receiver->ith_seqno = dest_port->ip_seqno++;
				imq_unlock(dest_mqueue);

				/*
				 *	Call the receiver's continuation.
				 */

				receiver->wait_result = THREAD_AWAKENED;
				(*receiver->swap_func)();
				/*NOTREACHED*/
				return MACH_MSG_SUCCESS;
			}
		} else {
			/*
			 *	The receiver can't accept the message,
			 *	or we can't switch to the receiver.
			 */

			imq_unlock(dest_mqueue);
			goto abort_send_receive;
		}
		counter(c_mach_msg_trap_block_fast++);

		/*
		 *	Safe to unlock dest_port now that we are
		 *	committed to this path, because we hold
		 *	dest_mqueue locked.  We never bother changing
		 *	dest_port->ip_msgcount.
		 */

		ip_unlock(dest_port);

		/*
		 *	We need to finish preparing self for its
		 *	time asleep in rcv_mqueue.
		 */

		ipc_thread_enqueue_macro(&rcv_mqueue->imq_threads, self);
		self->ith_state = MACH_RCV_IN_PROGRESS;
		self->ith_msize = MACH_MSG_SIZE_MAX;
		imq_unlock(rcv_mqueue);

		/*
		 *	Finish extracting receiver from dest_mqueue.
		 */

		ipc_thread_rmqueue_first_macro(
			&dest_mqueue->imq_threads, receiver);
		kmsg->ikm_header.msgh_seqno = dest_port->ip_seqno++;
		imq_unlock(dest_mqueue);

		/*
		 *	We don't have to do any post-dequeue processing of
		 *	the message.  We never incremented ip_msgcount, we
		 *	know it has no msg-accepted request, and blocked
		 *	senders aren't a worry because we found the port
		 *	with a receiver waiting.
		 */

		self = receiver;
		space = self->task->itk_space;

		msg = self->ith_msg;
		rcv_size = self->ith_rcv_size;
		rcv_object = self->ith_object;

		/* inline ipc_object_release */
		io_lock(rcv_object);
		io_release(rcv_object);
		io_check_unlock(rcv_object);
	    }

	    fast_copyout:
		/*
		 *	Nothing locked and no references held, except
		 *	we have kmsg with msgh_seqno filled in.  Must
		 *	still check against rcv_size and do
		 *	ipc_kmsg_copyout/ipc_kmsg_put.
		 */

		assert((ipc_port_t) kmsg->ikm_header.msgh_remote_port
						== dest_port);

		reply_size = kmsg->ikm_header.msgh_size;
		if (rcv_size < reply_size)
			goto slow_copyout;

		/* optimized ipc_kmsg_copyout/ipc_kmsg_copyout_header */

		switch (kmsg->ikm_header.msgh_bits) {
		    case MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND,
					MACH_MSG_TYPE_PORT_SEND_ONCE): {
			ipc_port_t reply_port =
				(ipc_port_t) kmsg->ikm_header.msgh_local_port;
			mach_port_t dest_name, reply_name;

			/* receiving a request message */

			if (!IP_VALID(reply_port))
				goto slow_copyout;

			is_write_lock(space);
			assert(space->is_active);

			/*
			 *	To do an atomic copyout, need simultaneous
			 *	locks on both ports and the space.  If
			 *	dest_port == reply_port, and simple locking is
			 *	enabled, then we will abort.  Otherwise it's
			 *	OK to unlock twice.
			 */

			ip_lock(dest_port);
			if (!ip_active(dest_port) ||
			    !ip_lock_try(reply_port))
				goto abort_request_copyout;

			if (!ip_active(reply_port)) {
				ip_unlock(reply_port);
				goto abort_request_copyout;
			}

			assert(reply_port->ip_sorights > 0);
			ip_unlock(reply_port);

		    {
			register ipc_entry_t table;
			register ipc_entry_t entry;
			register mach_port_index_t index;

			/* optimized ipc_entry_get */

			table = space->is_table;
			index = table->ie_next;

			if (index == 0)
				goto abort_request_copyout;

			entry = &table[index];
			table->ie_next = entry->ie_next;
			entry->ie_request = 0;

		    {
			register mach_port_gen_t gen;

			assert((entry->ie_bits &~ IE_BITS_GEN_MASK) == 0);
			gen = entry->ie_bits + IE_BITS_GEN_ONE;

			reply_name = MACH_PORT_MAKE(index, gen);

			/* optimized ipc_right_copyout */

			entry->ie_bits = gen | (MACH_PORT_TYPE_SEND_ONCE | 1);
		    }

			assert(MACH_PORT_VALID(reply_name));
			entry->ie_object = (ipc_object_t) reply_port;
			is_write_unlock(space);
		    }

			/* optimized ipc_object_copyout_dest */

			assert(dest_port->ip_srights > 0);
			ip_release(dest_port);

			if (dest_port->ip_receiver == space)
				dest_name = dest_port->ip_receiver_name;
			else
				dest_name = MACH_PORT_NULL;

			if ((--dest_port->ip_srights == 0) &&
			    (dest_port->ip_nsrequest != IP_NULL)) {
				ipc_port_t nsrequest;
				mach_port_mscount_t mscount;

				/* a rather rare case */

				nsrequest = dest_port->ip_nsrequest;
				mscount = dest_port->ip_mscount;
				dest_port->ip_nsrequest = IP_NULL;
				ip_unlock(dest_port);

				ipc_notify_no_senders(nsrequest, mscount);
			} else
				ip_unlock(dest_port);

			kmsg->ikm_header.msgh_bits =
				MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE,
					       MACH_MSG_TYPE_PORT_SEND);
			kmsg->ikm_header.msgh_remote_port = reply_name;
			kmsg->ikm_header.msgh_local_port = dest_name;
			goto fast_put;

		    abort_request_copyout:
			ip_unlock(dest_port);
			is_write_unlock(space);
			goto slow_copyout;
		    }

		    case MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0): {
			register mach_port_t dest_name;

			/* receiving a reply message */

			ip_lock(dest_port);
			if (!ip_active(dest_port))
				goto slow_copyout;

			/* optimized ipc_object_copyout_dest */

			assert(dest_port->ip_sorights > 0);

			if (dest_port->ip_receiver == space) {
				ip_release(dest_port);
				dest_port->ip_sorights--;
				dest_name = dest_port->ip_receiver_name;
				ip_unlock(dest_port);
			} else {
				ip_unlock(dest_port);

				ipc_notify_send_once(dest_port);
				dest_name = MACH_PORT_NULL;
			}

			kmsg->ikm_header.msgh_bits =
				MACH_MSGH_BITS(0,
					       MACH_MSG_TYPE_PORT_SEND_ONCE);
			kmsg->ikm_header.msgh_remote_port = MACH_PORT_NULL;
			kmsg->ikm_header.msgh_local_port = dest_name;
			goto fast_put;
		    }

		    case MACH_MSGH_BITS_COMPLEX|
			 MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0): {
			register mach_port_t dest_name;

			/* receiving a complex reply message */

			ip_lock(dest_port);
			if (!ip_active(dest_port))
				goto slow_copyout;

			/* optimized ipc_object_copyout_dest */

			assert(dest_port->ip_sorights > 0);

			if (dest_port->ip_receiver == space) {
				ip_release(dest_port);
				dest_port->ip_sorights--;
				dest_name = dest_port->ip_receiver_name;
				ip_unlock(dest_port);
			} else {
				ip_unlock(dest_port);

				ipc_notify_send_once(dest_port);
				dest_name = MACH_PORT_NULL;
			}

			kmsg->ikm_header.msgh_bits =
				MACH_MSGH_BITS_COMPLEX |
				MACH_MSGH_BITS(0,
					       MACH_MSG_TYPE_PORT_SEND_ONCE);
			kmsg->ikm_header.msgh_remote_port = MACH_PORT_NULL;
			kmsg->ikm_header.msgh_local_port = dest_name;

			mr = ipc_kmsg_copyout_body(
				(vm_offset_t) (&kmsg->ikm_header + 1),
				(vm_offset_t) &kmsg->ikm_header
					+ kmsg->ikm_header.msgh_size,
				space,
				current_map());

			if (mr != MACH_MSG_SUCCESS) {
				(void) ipc_kmsg_put(msg, kmsg,
					kmsg->ikm_header.msgh_size);
				return mr | MACH_RCV_BODY_ERROR;
			}
			goto fast_put;
		    }

		    default:
			goto slow_copyout;
		}
		/*NOTREACHED*/

	    fast_put:
		/*
		 *	We have the reply message data in kmsg,
		 *	and the reply message size in reply_size.
		 *	Just need to copy it out to the user and free kmsg.
		 *	We must check ikm_cache after copyoutmsg.
		 */

		ikm_check_initialized(kmsg, kmsg->ikm_size);

		if ((kmsg->ikm_size != IKM_SAVED_KMSG_SIZE) ||
		    copyoutmsg((vm_offset_t) &kmsg->ikm_header, (vm_offset_t) msg,
			       reply_size) ||
		    (ikm_cache() != IKM_NULL))
			goto slow_put;

		ikm_cache() = kmsg;
		thread_syscall_return(MACH_MSG_SUCCESS);
		/*NOTREACHED*/
		return MACH_MSG_SUCCESS; /* help for the compiler */

		/*
		 *	The slow path has a few non-register temporary
		 *	variables used only for call-by-reference.
		 */

	    {
		ipc_kmsg_t temp_kmsg;
		mach_port_seqno_t temp_seqno;
		ipc_object_t temp_rcv_object;
		ipc_mqueue_t temp_rcv_mqueue;

	    slow_get:
		/*
		 *	No locks, references, or messages held.
		 *	Still have to get the request, send it,
		 *	receive reply, etc.
		 */

		mr = ipc_kmsg_get(msg, send_size, &temp_kmsg);
		if (mr != MACH_MSG_SUCCESS) {
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}
		kmsg = temp_kmsg;

		/* try to get back on optimized path */
		goto fast_copyin;

	    slow_copyin:
		/*
		 *	We have the message data in kmsg, but
		 *	we still need to copyin, send it,
		 *	receive a reply, and do copyout.
		 */

		mr = ipc_kmsg_copyin(kmsg, space, current_map(),
				     MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			ikm_free(kmsg);
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		/* try to get back on optimized path */

		if (kmsg->ikm_header.msgh_bits & MACH_MSGH_BITS_CIRCULAR)
			goto slow_send;

		dest_port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
		assert(IP_VALID(dest_port));

		ip_lock(dest_port);
		if (dest_port->ip_receiver == ipc_space_kernel) {
			assert(ip_active(dest_port));
			ip_unlock(dest_port);
			goto kernel_send;
		}

		if (ip_active(dest_port) &&
#if	NORMA_IPC
		    (! IP_NORMA_IS_PROXY(dest_port)) &&
#endif	NORMA_IPC
		    ((dest_port->ip_msgcount < dest_port->ip_qlimit) ||
		     (MACH_MSGH_BITS_REMOTE(kmsg->ikm_header.msgh_bits) ==
					MACH_MSG_TYPE_PORT_SEND_ONCE)))
		{
		    /*
		     *	Try an optimized ipc_mqueue_copyin.
		     *	It will work if this is a request message.
		     */

		    register ipc_port_t reply_port;

		    reply_port = (ipc_port_t)
					kmsg->ikm_header.msgh_local_port;
		    if (IP_VALID(reply_port)) {
			if (ip_lock_try(reply_port)) {
			    if (ip_active(reply_port) &&
				reply_port->ip_receiver == space &&
				reply_port->ip_receiver_name == rcv_name &&
				reply_port->ip_pset == IPS_NULL)
			    {
				/* Grab a reference to the reply port. */
				rcv_object = (ipc_object_t) reply_port;
				io_reference(rcv_object);
				rcv_mqueue = &reply_port->ip_messages;
				imq_lock(rcv_mqueue);
				io_unlock(rcv_object);
				goto fast_send_receive;
			    }
			    ip_unlock(reply_port);
			}
		    }
		}

		ip_unlock(dest_port);
		goto slow_send;

#if	NORMA_IPC
	    norma_send:
		/*
		 *	Nothing is locked.  We have acquired kmsg, but
		 *	we still need to send it and receive a reply.
		 */

		mr = norma_ipc_send(kmsg);
		if (mr != MACH_MSG_SUCCESS) {
			mr |= ipc_kmsg_copyout_pseudo(kmsg, space,
						      current_map());

			assert(kmsg->ikm_marequest == IMAR_NULL);
			(void) ipc_kmsg_put(msg, kmsg,
					    kmsg->ikm_header.msgh_size);
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		goto slow_get_rcv_port;
#endif	NORMA_IPC

	    kernel_send:
		/*
		 *	Special case: send message to kernel services.
		 *	The request message has been copied into the
		 *	kmsg.  Nothing is locked.
		 */

	    {
		register ipc_port_t	reply_port;

		/*
		 * Perform the kernel function.
		 */

		kmsg = ipc_kobject_server(kmsg);
		if (kmsg == IKM_NULL) {
			/*
			 * No reply.  Take the
			 * slow receive path.
			 */
			goto slow_get_rcv_port;
		}

		/*
		 * Check that:
		 *	the reply port is alive
		 *	we hold the receive right
		 *	the name has not changed.
		 *	the port is not in a set
		 * If any of these are not true,
		 * we cannot directly receive the reply
		 * message.
		 */
		reply_port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
		ip_lock(reply_port);

		if ((!ip_active(reply_port)) ||
		    (reply_port->ip_receiver != space) ||
		    (reply_port->ip_receiver_name != rcv_name) ||
		    (reply_port->ip_pset != IPS_NULL))
		{
			ip_unlock(reply_port);
			ipc_mqueue_send_always(kmsg);
			goto slow_get_rcv_port;
		}

		rcv_mqueue = &reply_port->ip_messages;
		imq_lock(rcv_mqueue);
		/* keep port locked, and don`t change ref count yet */

		/*
		 * If there are messages on the port
		 * or other threads waiting for a message,
		 * we cannot directly receive the reply.
		 */
		if ((ipc_thread_queue_first(&rcv_mqueue->imq_threads)
			!= ITH_NULL) ||
		    (ipc_kmsg_queue_first(&rcv_mqueue->imq_messages)
			!= IKM_NULL))
		{
			imq_unlock(rcv_mqueue);
			ip_unlock(reply_port);
			ipc_mqueue_send_always(kmsg);
			goto slow_get_rcv_port;
		}

		/*
		 * We can directly receive this reply.
		 * Since the kernel reply never blocks,
		 * it holds no message_accepted request.
		 * Since there were no messages queued
		 * on the reply port, there should be
		 * no threads blocked waiting to send.
		 */

		assert(kmsg->ikm_marequest == IMAR_NULL);
		assert(ipc_thread_queue_first(&reply_port->ip_blocked)
				== ITH_NULL);

		dest_port = reply_port;
		kmsg->ikm_header.msgh_seqno = dest_port->ip_seqno++;
		imq_unlock(rcv_mqueue);

		/*
		 * inline ipc_object_release.
		 * Port is still locked.
		 * Reference count was not incremented.
		 */
		ip_check_unlock(reply_port);

		/* copy out the kernel reply */
		goto fast_copyout;
	    }

	    slow_send:
		/*
		 *	Nothing is locked.  We have acquired kmsg, but
		 *	we still need to send it and receive a reply.
		 */

		mr = ipc_mqueue_send(kmsg, MACH_MSG_OPTION_NONE,
				     MACH_MSG_TIMEOUT_NONE);
		if (mr != MACH_MSG_SUCCESS) {
			mr |= ipc_kmsg_copyout_pseudo(kmsg, space,
						      current_map());

			assert(kmsg->ikm_marequest == IMAR_NULL);
			(void) ipc_kmsg_put(msg, kmsg,
					    kmsg->ikm_header.msgh_size);
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

	    slow_get_rcv_port:
		/*
		 * We have sent the message.  Copy in the receive port.
		 */
		mr = ipc_mqueue_copyin(space, rcv_name,
				       &temp_rcv_mqueue, &temp_rcv_object);
		if (mr != MACH_MSG_SUCCESS) {
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}
		rcv_mqueue = temp_rcv_mqueue;
		rcv_object = temp_rcv_object;
		/* hold ref for rcv_object; rcv_mqueue is locked */

	/*
	    slow_receive:
	*/
		/*
		 *	Now we have sent the request and copied in rcv_name,
		 *	so rcv_mqueue is locked and hold ref for rcv_object.
		 *	Just receive a reply and try to get back to fast path.
		 *
		 *	ipc_mqueue_receive may not return, because if we block
		 *	then our kernel stack may be discarded.  So we save
		 *	state here for mach_msg_continue to pick up.
		 */

		self->ith_msg = msg;
		self->ith_rcv_size = rcv_size;
		self->ith_object = rcv_object;
		self->ith_mqueue = rcv_mqueue;

		mr = ipc_mqueue_receive(rcv_mqueue,
					MACH_MSG_OPTION_NONE,
					MACH_MSG_SIZE_MAX,
					MACH_MSG_TIMEOUT_NONE,
					FALSE, mach_msg_continue,
		       			&temp_kmsg, &temp_seqno);
		/* rcv_mqueue is unlocked */
		ipc_object_release(rcv_object);
		if (mr != MACH_MSG_SUCCESS) {
			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		(kmsg = temp_kmsg)->ikm_header.msgh_seqno = temp_seqno;
		dest_port = (ipc_port_t) kmsg->ikm_header.msgh_remote_port;
		goto fast_copyout;

	    slow_copyout:
		/*
		 *	Nothing locked and no references held, except
		 *	we have kmsg with msgh_seqno filled in.  Must
		 *	still check against rcv_size and do
		 *	ipc_kmsg_copyout/ipc_kmsg_put.
		 */

		reply_size = kmsg->ikm_header.msgh_size;
		if (rcv_size < reply_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			thread_syscall_return(MACH_RCV_TOO_LARGE);
			/*NOTREACHED*/
		}

		mr = ipc_kmsg_copyout(kmsg, space, current_map(),
				      MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
				(void) ipc_kmsg_put(msg, kmsg,
						kmsg->ikm_header.msgh_size);
			} else {
				ipc_kmsg_copyout_dest(kmsg, space);
				(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			}

			thread_syscall_return(mr);
			/*NOTREACHED*/
		}

		/* try to get back on optimized path */

		goto fast_put;

	    slow_put:
		mr = ipc_kmsg_put(msg, kmsg, reply_size);
		thread_syscall_return(mr);
		/*NOTREACHED*/
	    }
	} else if (option == MACH_SEND_MSG) {
		ipc_space_t space = current_space();
		vm_map_t map = current_map();
		ipc_kmsg_t kmsg;

		mr = ipc_kmsg_get(msg, send_size, &kmsg);
		if (mr != MACH_MSG_SUCCESS)
			return mr;

		mr = ipc_kmsg_copyin(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			ikm_free(kmsg);
			return mr;
		}

		mr = ipc_mqueue_send(kmsg, MACH_MSG_OPTION_NONE,
				     MACH_MSG_TIMEOUT_NONE);
		if (mr != MACH_MSG_SUCCESS) {
			mr |= ipc_kmsg_copyout_pseudo(kmsg, space, map);

			assert(kmsg->ikm_marequest == IMAR_NULL);
			(void) ipc_kmsg_put(msg, kmsg,
					    kmsg->ikm_header.msgh_size);
		}

		return mr;
	} else if (option == MACH_RCV_MSG) {
		ipc_thread_t self = current_thread();
		ipc_space_t space = current_space();
		vm_map_t map = current_map();
		ipc_object_t object;
		ipc_mqueue_t mqueue;
		ipc_kmsg_t kmsg;
		mach_port_seqno_t seqno;

		mr = ipc_mqueue_copyin(space, rcv_name, &mqueue, &object);
		if (mr != MACH_MSG_SUCCESS)
			return mr;
		/* hold ref for object; mqueue is locked */

		/*
		 *	ipc_mqueue_receive may not return, because if we block
		 *	then our kernel stack may be discarded.  So we save
		 *	state here for mach_msg_continue to pick up.
		 */

		self->ith_msg = msg;
		self->ith_rcv_size = rcv_size;
		self->ith_object = object;
		self->ith_mqueue = mqueue;

		mr = ipc_mqueue_receive(mqueue,
					MACH_MSG_OPTION_NONE,
					MACH_MSG_SIZE_MAX,
					MACH_MSG_TIMEOUT_NONE,
					FALSE, mach_msg_continue,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		ipc_object_release(object);
		if (mr != MACH_MSG_SUCCESS)
			return mr;

		kmsg->ikm_header.msgh_seqno = seqno;
		if (rcv_size < kmsg->ikm_header.msgh_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			return MACH_RCV_TOO_LARGE;
		}

		mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
				(void) ipc_kmsg_put(msg, kmsg,
						kmsg->ikm_header.msgh_size);
			} else {
				ipc_kmsg_copyout_dest(kmsg, space);
				(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
			}

			return mr;
		}

		return ipc_kmsg_put(msg, kmsg, kmsg->ikm_header.msgh_size);
	} else if (option == MACH_MSG_OPTION_NONE) {
		/*
		 *	We can measure the "null mach_msg_trap"
		 *	(syscall entry and thread_syscall_return exit)
		 *	with this path.
		 */

		thread_syscall_return(MACH_MSG_SUCCESS);
		/*NOTREACHED*/
	}
#endif	/* CONTINUATIONS */

	if (option & MACH_SEND_MSG) {
		mr = mach_msg_send(msg, option, send_size,
				   time_out, notify);
		if (mr != MACH_MSG_SUCCESS)
			return mr;
	}

	if (option & MACH_RCV_MSG) {
		mr = mach_msg_receive(msg, option, rcv_size, rcv_name,
				      time_out, notify);
		if (mr != MACH_MSG_SUCCESS)
			return mr;
	}

	return MACH_MSG_SUCCESS;
}

#ifdef CONTINUATIONS
/*
 *	Routine:	mach_msg_continue
 *	Purpose:
 *		Continue after blocking for a message.
 *	Conditions:
 *		Nothing locked.  We are running on a new kernel stack,
 *		with the receive state saved in the thread.  From here
 *		control goes back to user space.
 */

void
mach_msg_continue()
{
	ipc_thread_t thread = current_thread();
	task_t task = thread->task;
	ipc_space_t space = task->itk_space;
	vm_map_t map = task->map;
	mach_msg_header_t *msg = thread->ith_msg;
	mach_msg_size_t rcv_size = thread->ith_rcv_size;
	ipc_object_t object = thread->ith_object;
	ipc_mqueue_t mqueue = thread->ith_mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	mr = ipc_mqueue_receive(mqueue, MACH_MSG_OPTION_NONE,
				MACH_MSG_SIZE_MAX, MACH_MSG_TIMEOUT_NONE,
				TRUE, mach_msg_continue, &kmsg, &seqno);
	/* mqueue is unlocked */
	ipc_object_release(object);
	if (mr != MACH_MSG_SUCCESS) {
		thread_syscall_return(mr);
		/*NOTREACHED*/
	}

	kmsg->ikm_header.msgh_seqno = seqno;
	if (kmsg->ikm_header.msgh_size > rcv_size) {
		ipc_kmsg_copyout_dest(kmsg, space);
		(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
		thread_syscall_return(MACH_RCV_TOO_LARGE);
		/*NOTREACHED*/
	}

	mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
	if (mr != MACH_MSG_SUCCESS) {
		if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
			(void) ipc_kmsg_put(msg, kmsg,
					kmsg->ikm_header.msgh_size);
		} else {
			ipc_kmsg_copyout_dest(kmsg, space);
			(void) ipc_kmsg_put(msg, kmsg, sizeof *msg);
		}

		thread_syscall_return(mr);
		/*NOTREACHED*/
	}

	mr = ipc_kmsg_put(msg, kmsg, kmsg->ikm_header.msgh_size);
	thread_syscall_return(mr);
	/*NOTREACHED*/
}

/*
 *	Routine:	mach_msg_interrupt
 *	Purpose:
 *		Attempts to force a thread waiting at mach_msg_continue or
 *		mach_msg_receive_continue into a clean point.  Returns TRUE
 *		if this was possible.
 *	Conditions:
 *		Nothing locked.  The thread must NOT be runnable.
 */

boolean_t
mach_msg_interrupt(thread)
	thread_t thread;
{
	ipc_mqueue_t mqueue;

	assert((thread->swap_func == (void (*)()) mach_msg_continue) ||
	       (thread->swap_func == (void (*)()) mach_msg_receive_continue));

	mqueue = thread->ith_mqueue;
	imq_lock(mqueue);
	if (thread->ith_state != MACH_RCV_IN_PROGRESS) {
		/*
		 *	The thread is no longer waiting for a message.
		 *	It may have a message sitting in ith_kmsg.
		 *	We can't clean this up.
		 */

		imq_unlock(mqueue);
		return FALSE;
	}
	ipc_thread_rmqueue(&mqueue->imq_threads, thread);
	imq_unlock(mqueue);

	ipc_object_release(thread->ith_object);

	thread_set_syscall_return(thread, MACH_RCV_INTERRUPTED);
	thread->swap_func = thread_exception_return;
	return TRUE;
}
#endif /* CONTINUATIONS */

#if	MACH_IPC_COMPAT

/*
 *	Routine:	msg_return_translate
 *	Purpose:
 *		Translate from new error code to old error code.
 */

msg_return_t
msg_return_translate(mr)
	mach_msg_return_t mr;
{
	switch (mr &~ MACH_MSG_MASK) {
	    case MACH_MSG_SUCCESS:
		return 0;	/* SEND_SUCCESS/RCV_SUCCESS/RPC_SUCCESS */

	    case MACH_SEND_NO_BUFFER:
	    case MACH_SEND_NO_NOTIFY:
		printf("msg_return_translate: %x -> interrupted\n", mr);
		return SEND_INTERRUPTED;

	    case MACH_SEND_MSG_TOO_SMALL:
		return SEND_MSG_TOO_SMALL;
	    case MACH_SEND_INVALID_DATA:
	    case MACH_SEND_INVALID_MEMORY:
		return SEND_INVALID_MEMORY;
	    case MACH_SEND_TIMED_OUT:
		return SEND_TIMED_OUT;
	    case MACH_SEND_INTERRUPTED:
		return SEND_INTERRUPTED;
	    case MACH_SEND_INVALID_DEST:
	    case MACH_SEND_INVALID_REPLY:
	    case MACH_SEND_INVALID_RIGHT:
	    case MACH_SEND_INVALID_TYPE:
		return SEND_INVALID_PORT;
	    case MACH_SEND_WILL_NOTIFY:
		return SEND_WILL_NOTIFY;
	    case MACH_SEND_NOTIFY_IN_PROGRESS:
		return SEND_NOTIFY_IN_PROGRESS;

	    case MACH_RCV_INVALID_NAME:
	    case MACH_RCV_IN_SET:
	    case MACH_RCV_PORT_DIED:
		return RCV_INVALID_PORT;
	    case MACH_RCV_TOO_LARGE:
		return RCV_TOO_LARGE;
	    case MACH_RCV_TIMED_OUT:
		return RCV_TIMED_OUT;
	    case MACH_RCV_INTERRUPTED:
		return RCV_INTERRUPTED;
	    case MACH_RCV_PORT_CHANGED:
		return RCV_PORT_CHANGE;
	    case MACH_RCV_INVALID_DATA:
		return RCV_INVALID_MEMORY;

	    case MACH_SEND_IN_PROGRESS:
	    case MACH_SEND_INVALID_NOTIFY:
	    case MACH_SEND_INVALID_HEADER:
	    case MACH_RCV_IN_PROGRESS:
	    case MACH_RCV_INVALID_NOTIFY:
	    case MACH_RCV_HEADER_ERROR:
	    case MACH_RCV_BODY_ERROR:
	    default:
#if MACH_ASSERT
		assert(!"msg_return_translate");
#else
		panic("msg_return_translate");
#endif
	}
}

/*
 *	Routine:	msg_send_trap [mach trap]
 *	Purpose:
 *		Send a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 */

msg_return_t
msg_send_trap(msg, option, send_size, time_out)
	msg_header_t *msg;
	msg_option_t option;
	msg_size_t send_size;
	msg_timeout_t time_out;
{
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_kmsg_t kmsg;
	mach_msg_return_t mr;

	send_size = (send_size + 3) & ~3; /* round up */

	if (send_size > MSG_SIZE_MAX)
		return SEND_MSG_TOO_LARGE;

	mr = ipc_kmsg_get((mach_msg_header_t *) msg,
			  (mach_msg_size_t) send_size,
			  &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		return msg_return_translate(mr);

	mr = ipc_kmsg_copyin_compat(kmsg, space, map);
	if (mr != MACH_MSG_SUCCESS) {
		ikm_free(kmsg);
		return msg_return_translate(mr);
	}

	if (option & SEND_NOTIFY) {
		mr = ipc_mqueue_send(kmsg, MACH_SEND_TIMEOUT,
				     ((option & SEND_TIMEOUT) ?
				      (mach_msg_timeout_t) time_out :
				      MACH_MSG_TIMEOUT_NONE));
		if (mr == MACH_SEND_TIMED_OUT) {
			ipc_port_t dest = (ipc_port_t)
				kmsg->ikm_header.msgh_remote_port;

			mr = ipc_marequest_create(space, dest, MACH_PORT_NULL,
						  &kmsg->ikm_marequest);
			if (mr == MACH_MSG_SUCCESS) {
				ipc_mqueue_send_always(kmsg);
				return SEND_WILL_NOTIFY;
			}
		}
	} else
		mr = ipc_mqueue_send(kmsg,
				     ((option & SEND_TIMEOUT) ?
				      MACH_SEND_TIMEOUT :
				      MACH_MSG_OPTION_NONE),
				     (mach_msg_timeout_t) time_out);

	if (mr != MACH_MSG_SUCCESS)
		ipc_kmsg_destroy(kmsg);

	return msg_return_translate(mr);
}

/*
 *	Routine:	msg_receive_trap [mach trap]
 *	Purpose:
 *		Receive a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 */

msg_return_t
msg_receive_trap(msg, option, rcv_size, rcv_name, time_out)
	msg_header_t *msg;
	msg_option_t option;
	msg_size_t rcv_size;
	port_name_t rcv_name;
	msg_timeout_t time_out;
{
	ipc_thread_t self;
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_object_t object;
	ipc_mqueue_t mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	mr = ipc_mqueue_copyin(space, (mach_port_t) rcv_name,
			       &mqueue, &object);
	if (mr != MACH_MSG_SUCCESS)
		return msg_return_translate(mr);
	/* hold ref for object; mqueue is locked */

#ifdef CONTINUATIONS
	/*
	 *	ipc_mqueue_receive may not return, because if we block
	 *	then our kernel stack may be discarded.  So we save
	 *	state here for msg_receive_continue to pick up.
	 */

	self = current_thread();
	self->ith_msg = (mach_msg_header_t *) msg;
	self->ith_option = (mach_msg_option_t) option;
	self->ith_rcv_size = (mach_msg_size_t) rcv_size;
	self->ith_timeout = (mach_msg_timeout_t) time_out;
	self->ith_object = object;
	self->ith_mqueue = mqueue;
#endif /* CONTINUATIONS */

	mr = ipc_mqueue_receive(mqueue,
				(option & RCV_TIMEOUT) ?
				MACH_RCV_TIMEOUT : MACH_MSG_OPTION_NONE,
				(mach_msg_size_t) rcv_size,
				(mach_msg_timeout_t) time_out,
				FALSE, msg_receive_continue,
				&kmsg, &seqno);
	/* mqueue is unlocked */
	ipc_object_release(object);
	if (mr != MACH_MSG_SUCCESS) {
		if (mr == MACH_RCV_TOO_LARGE) {
			msg_size_t real_size =
				(msg_size_t) (mach_msg_size_t) kmsg;

			assert(real_size > rcv_size);

			(void) copyout((vm_offset_t) &real_size,
				       (vm_offset_t) &msg->msg_size,
				       sizeof(msg_size_t));
		}

		return msg_return_translate(mr);
	}

	assert(kmsg->ikm_header.msgh_size <= (mach_msg_size_t) rcv_size);

	mr = ipc_kmsg_copyout_compat(kmsg, space, map);
	assert(mr == MACH_MSG_SUCCESS);

	mr = ipc_kmsg_put((mach_msg_header_t *) msg, kmsg,
			  kmsg->ikm_header.msgh_size);
	return msg_return_translate(mr);
}

/*
 *	Routine:	msg_rpc_trap [mach trap]
 *	Purpose:
 *		Send and receive a message.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 */

msg_return_t
msg_rpc_trap(msg, option, send_size, rcv_size, send_timeout, rcv_timeout)
	msg_header_t *msg;
	msg_option_t option;
	msg_size_t send_size;
	msg_size_t rcv_size;
	msg_timeout_t send_timeout;
	msg_timeout_t rcv_timeout;
{
	ipc_thread_t self;
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_port_t reply;
	ipc_pset_t pset;
	ipc_mqueue_t mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	/*
	 *	Instead of using msg_send_trap and msg_receive_trap,
	 *	we implement msg_rpc_trap directly.  The difference
	 *	is how the reply port is handled.  Instead of using
	 *	ipc_mqueue_copyin, we save a reference for the reply
	 *	port carried in the sent message.  For example,
	 *	consider a rename kernel call which changes the name
	 *	of the call's own reply port.  This is the behaviour
	 *	of the Mach 2.5 msg_rpc_trap.
	 */

	send_size = (send_size + 3) & ~3; /* round up */

	if (send_size > MSG_SIZE_MAX)
		return SEND_MSG_TOO_LARGE;

	mr = ipc_kmsg_get((mach_msg_header_t *) msg,
			  (mach_msg_size_t) send_size,
			  &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		return msg_return_translate(mr);

	mr = ipc_kmsg_copyin_compat(kmsg, space, map);
	if (mr != MACH_MSG_SUCCESS) {
		ikm_free(kmsg);
		return msg_return_translate(mr);
	}

	reply = (ipc_port_t) kmsg->ikm_header.msgh_local_port;
	if (IP_VALID(reply))
		ipc_port_reference(reply);

	if (option & SEND_NOTIFY) {
		mr = ipc_mqueue_send(kmsg, MACH_SEND_TIMEOUT,
				     ((option & SEND_TIMEOUT) ?
				      (mach_msg_timeout_t) send_timeout :
				      MACH_MSG_TIMEOUT_NONE));
		if (mr == MACH_SEND_TIMED_OUT) {
			ipc_port_t dest = (ipc_port_t)
				kmsg->ikm_header.msgh_remote_port;

			mr = ipc_marequest_create(space, dest, MACH_PORT_NULL,
						  &kmsg->ikm_marequest);
			if (mr == MACH_MSG_SUCCESS) {
				ipc_mqueue_send_always(kmsg);
				if (IP_VALID(reply))
					ipc_port_release(reply);
				return SEND_WILL_NOTIFY;
			}
		}
	} else
		mr = ipc_mqueue_send(kmsg,
				     ((option & SEND_TIMEOUT) ?
				      MACH_SEND_TIMEOUT :
				      MACH_MSG_OPTION_NONE),
				     (mach_msg_timeout_t) send_timeout);

	if (mr != MACH_MSG_SUCCESS) {
		ipc_kmsg_destroy(kmsg);
		if (IP_VALID(reply))
			ipc_port_release(reply);
		return msg_return_translate(mr);
	}

	if (!IP_VALID(reply))
		return RCV_INVALID_PORT;

	ip_lock(reply);
	if (reply->ip_receiver != space) {
		ip_release(reply);
		ip_check_unlock(reply);
		return RCV_INVALID_PORT;
	}

	assert(ip_active(reply));
	pset = reply->ip_pset;

	if (pset != IPS_NULL) {
		ips_lock(pset);
		if (ips_active(pset)) {
			ips_unlock(pset);
			ip_release(reply);
			ip_unlock(reply);
			return RCV_INVALID_PORT;
		}

		ipc_pset_remove(pset, reply);
		ips_check_unlock(pset);
		assert(reply->ip_pset == IPS_NULL);
	}

	mqueue = &reply->ip_messages;
	imq_lock(mqueue);
	ip_unlock(reply);

#ifdef CONTINUATIONS
	/*
	 *	ipc_mqueue_receive may not return, because if we block
	 *	then our kernel stack may be discarded.  So we save
	 *	state here for msg_receive_continue to pick up.
	 */

	self = current_thread();
	self->ith_msg = (mach_msg_header_t *) msg;
	self->ith_option = (mach_msg_option_t) option;
	self->ith_rcv_size = (mach_msg_size_t) rcv_size;
	self->ith_timeout = (mach_msg_timeout_t) rcv_timeout;
	self->ith_object = (ipc_object_t) reply;
	self->ith_mqueue = mqueue;
#endif /* CONTINUATIONS */

	mr = ipc_mqueue_receive(mqueue,
				(option & RCV_TIMEOUT) ?
				MACH_RCV_TIMEOUT : MACH_MSG_OPTION_NONE,
				(mach_msg_size_t) rcv_size,
				(mach_msg_timeout_t) rcv_timeout,
				FALSE, msg_receive_continue,
				&kmsg, &seqno);
	/* mqueue is unlocked */
	ipc_port_release(reply);
	if (mr != MACH_MSG_SUCCESS) {
		if (mr == MACH_RCV_TOO_LARGE) {
			msg_size_t real_size =
				(msg_size_t) (mach_msg_size_t) kmsg;

			assert(real_size > rcv_size);

			(void) copyout((vm_offset_t) &real_size,
				       (vm_offset_t) &msg->msg_size,
				       sizeof(msg_size_t));
		}

		return msg_return_translate(mr);
	}

	assert(kmsg->ikm_header.msgh_size <= (mach_msg_size_t) rcv_size);

	mr = ipc_kmsg_copyout_compat(kmsg, space, map);
	assert(mr == MACH_MSG_SUCCESS);

	mr = ipc_kmsg_put((mach_msg_header_t *) msg,
			  kmsg, kmsg->ikm_header.msgh_size);
	return msg_return_translate(mr);
}

#ifdef CONTINUATIONS
/*
 *	Routine:	msg_receive_continue
 *	Purpose:
 *		Continue after blocking for a message.
 *	Conditions:
 *		Nothing locked.  We are running on a new kernel stack,
 *		with the receive state saved in the thread.  From here
 *		control goes back to user space.
 */

void
msg_receive_continue()
{
	ipc_thread_t self = current_thread();
	msg_header_t *msg = (msg_header_t *) self->ith_msg;
	msg_option_t option = (msg_option_t) self->ith_option;
	msg_size_t rcv_size = (msg_size_t) self->ith_rcv_size;
	msg_timeout_t time_out = (msg_timeout_t) self->ith_timeout;
	ipc_object_t object = self->ith_object;
	ipc_mqueue_t mqueue = self->ith_mqueue;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	mr = ipc_mqueue_receive(mqueue,
				(option & RCV_TIMEOUT) ?
				MACH_RCV_TIMEOUT : MACH_MSG_OPTION_NONE,
				(mach_msg_size_t) rcv_size,
				(mach_msg_timeout_t) time_out,
				TRUE, msg_receive_continue,
				&kmsg, &seqno);
	/* mqueue is unlocked */
	ipc_object_release(object);
	if (mr != MACH_MSG_SUCCESS) {
		if (mr == MACH_RCV_TOO_LARGE) {
			msg_size_t real_size =
				(msg_size_t) (mach_msg_size_t) kmsg;

			assert(real_size > rcv_size);

			(void) copyout((vm_offset_t) &real_size,
				       (vm_offset_t) &msg->msg_size,
				       sizeof(msg_size_t));
		}

		thread_syscall_return(msg_return_translate(mr));
		/*NOTREACHED*/
	}

	assert(kmsg->ikm_header.msgh_size <= (mach_msg_size_t) rcv_size);

	mr = ipc_kmsg_copyout_compat(kmsg, current_space(), current_map());
	assert(mr == MACH_MSG_SUCCESS);

	mr = ipc_kmsg_put((mach_msg_header_t *) msg, kmsg,
			  kmsg->ikm_header.msgh_size);
	thread_syscall_return(msg_return_translate(mr));
	/*NOTREACHED*/
}
#endif /* CONTINUATIONS */

#endif	MACH_IPC_COMPAT
