/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990,1989,1988,1987 Carnegie Mellon University.
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

#include <norma_ipc.h>
#include <mach_kdb.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/mig_errors.h>
#include <ipc/port.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/mach_msg.h>
#include <ipc/ipc_machdep.h>
#include <kern/counters.h>
#include <kern/ipc_tt.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <mach/machine/vm_types.h>



extern void exception();
extern void exception_try_task();
extern void exception_no_server();

extern void exception_raise();
extern kern_return_t exception_parse_reply();
extern void exception_raise_continue();
extern void exception_raise_continue_slow();
extern void exception_raise_continue_fast();

#if	MACH_KDB
extern void thread_kdb_return();
extern void db_printf();

boolean_t debug_user_with_kdb = FALSE;
#endif	/* MACH_KDB */

#ifdef	KEEP_STACKS
/*
 *	Some obsolete architectures don't support kernel stack discarding
 *	or the thread_exception_return, thread_syscall_return continuations.
 *	For these architectures, the NOTREACHED comments below are incorrect.
 *	The exception function is expected to return.
 *	So the return statements along the slow paths are important.
 */
#endif	KEEP_STACKS

/*
 *	Routine:	exception
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the thread's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_exception_return and thread_kdb_return
 *		are possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception(_exception, code, subcode)
	integer_t _exception, code, subcode;
{
	register ipc_thread_t self = current_thread();
	register ipc_port_t exc_port;

	if (_exception == KERN_SUCCESS)
		panic("exception");

	/*
	 *	Optimized version of retrieve_thread_exception.
	 */

	ith_lock(self);
	assert(self->ith_self != IP_NULL);
	exc_port = self->ith_exception;
	if (!IP_VALID(exc_port)) {
		ith_unlock(self);
		exception_try_task(_exception, code, subcode);
		/*NOTREACHED*/
		return;
	}

	ip_lock(exc_port);
	ith_unlock(self);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		exception_try_task(_exception, code, subcode);
		/*NOTREACHED*/
		return;
	}

	/*
	 *	Make a naked send right for the exception port.
	 */

	ip_reference(exc_port);
	exc_port->ip_srights++;
	ip_unlock(exc_port);

	/*
	 *	If this exception port doesn't work,
	 *	we will want to try the task's exception port.
	 *	Indicate this by saving the exception state.
	 */

	self->ith_exc = _exception;
	self->ith_exc_code = code;
	self->ith_exc_subcode = subcode;

	exception_raise(exc_port,
			retrieve_thread_self_fast(self),
			retrieve_task_self_fast(self->task),
			_exception, code, subcode);
	/*NOTREACHED*/
}

/*
 *	Routine:	exception_try_task
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the task's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_exception_return and thread_kdb_return
 *		are possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception_try_task(_exception, code, subcode)
	integer_t _exception, code, subcode;
{
	ipc_thread_t self = current_thread();
	register task_t task = self->task;
	register ipc_port_t exc_port;

	/*
	 *	Optimized version of retrieve_task_exception.
	 */

	itk_lock(task);
	assert(task->itk_self != IP_NULL);
	exc_port = task->itk_exception;
	if (!IP_VALID(exc_port)) {
		itk_unlock(task);
		exception_no_server();
		/*NOTREACHED*/
		return;
	}

	ip_lock(exc_port);
	itk_unlock(task);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		exception_no_server();
		/*NOTREACHED*/
		return;
	}

	/*
	 *	Make a naked send right for the exception port.
	 */

	ip_reference(exc_port);
	exc_port->ip_srights++;
	ip_unlock(exc_port);

	/*
	 *	This is the thread's last chance.
	 *	Clear the saved exception state.
	 */

	self->ith_exc = KERN_SUCCESS;

	exception_raise(exc_port,
			retrieve_thread_self_fast(self),
			retrieve_task_self_fast(task),
			_exception, code, subcode);
	/*NOTREACHED*/
}

/*
 *	Routine:	exception_no_server
 *	Purpose:
 *		The current thread took an exception,
 *		and no exception server took responsibility
 *		for the exception.  So good bye, charlie.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_kdb_return is possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception_no_server()
{
	register ipc_thread_t self = current_thread();

	/*
	 *	If this thread is being terminated, cooperate.
	 */

	while (thread_should_halt(self))
		thread_halt_self();

#if	MACH_KDB
	if (debug_user_with_kdb) {
		/*
		 *	Debug the exception with kdb.
		 *	If kdb handles the exception,
		 *	then thread_kdb_return won't return.
		 */

		db_printf("No exception server, calling kdb...\n");
		thread_kdb_return();
	}
#endif	MACH_KDB

	/*
	 *	All else failed; terminate task.
	 */

	(void) task_terminate(self->task);
	thread_halt_self();
	/*NOTREACHED*/
}

#define MACH_EXCEPTION_ID		2400	/* from mach/exc.defs */
#define MACH_EXCEPTION_REPLY_ID		(MACH_EXCEPTION_ID + 100)

struct mach_exception {
	mach_msg_header_t	Head;
	mach_msg_type_t		threadType;
	mach_port_t		thread;
	mach_msg_type_t		taskType;
	mach_port_t		task;
	mach_msg_type_t		exceptionType;
	integer_t		exception;
	mach_msg_type_t		codeType;
	integer_t		code;
	mach_msg_type_t		subcodeType;
	integer_t		subcode;
};

#define	INTEGER_T_SIZE_IN_BITS	(8 * sizeof(integer_t))
#define	INTEGER_T_TYPE		MACH_MSG_TYPE_INTEGER_T
					/* in mach/machine/vm_types.h */

mach_msg_type_t exc_port_proto = {
	/* msgt_name = */		MACH_MSG_TYPE_PORT_SEND,
	/* msgt_size = */		PORT_T_SIZE_IN_BITS,
	/* msgt_number = */		1,
	/* msgt_inline = */		TRUE,
	/* msgt_longform = */		FALSE,
	/* msgt_deallocate = */		FALSE,
	/* msgt_unused = */		0
};

mach_msg_type_t exc_code_proto = {
	/* msgt_name = */		INTEGER_T_TYPE,
	/* msgt_size = */		INTEGER_T_SIZE_IN_BITS,
	/* msgt_number = */		1,
	/* msgt_inline = */		TRUE,
	/* msgt_longform = */		FALSE,
	/* msgt_deallocate = */		FALSE,
	/* msgt_unused = */		0
};

/*
 *	Routine:	exception_raise
 *	Purpose:
 *		Make an exception_raise up-call to an exception server.
 *
 *		dest_port must be a valid naked send right.
 *		thread_port and task_port are naked send rights.
 *		All three are always consumed.
 *
 *		self->ith_exc, self->ith_exc_code, self->ith_exc_subcode
 *		must be appropriately initialized.
 *	Conditions:
 *		Nothing locked.  We are being called in an exception context,
 *		so thread_exception_return may be called.
 *	Returns:
 *		Doesn't return.
 */

int exception_raise_misses = 0;

void
exception_raise(dest_port, thread_port, task_port,
		_exception, code, subcode)
	ipc_port_t dest_port;
	ipc_port_t thread_port;
	ipc_port_t task_port;
	integer_t _exception, code, subcode;
{
	ipc_thread_t self = current_thread();
	ipc_thread_t receiver;
	ipc_port_t reply_port;
	ipc_mqueue_t dest_mqueue;
	ipc_mqueue_t reply_mqueue;
	ipc_kmsg_t kmsg;
	mach_msg_return_t mr;

	assert(IP_VALID(dest_port));

	/*
	 *	We will eventually need a message buffer.
	 *	Grab the buffer now, while nothing is locked.
	 *	This buffer will get handed to the exception server,
	 *	and it will give the buffer back with its reply.
	 */

	kmsg = ikm_cache();
	if (kmsg != IKM_NULL) {
		ikm_cache() = IKM_NULL;
		ikm_check_initialized(kmsg, IKM_SAVED_KMSG_SIZE);
	} else {
		kmsg = ikm_alloc(IKM_SAVED_MSG_SIZE);
		if (kmsg == IKM_NULL)
			panic("exception_raise");
		ikm_init(kmsg, IKM_SAVED_MSG_SIZE);
	}

	/*
	 *	We need a reply port for the RPC.
	 *	Check first for a cached port.
	 */

	ith_lock(self);
	assert(self->ith_self != IP_NULL);

	reply_port = self->ith_rpc_reply;
	if (reply_port == IP_NULL) {
		ith_unlock(self);
		reply_port = ipc_port_alloc_reply();
		ith_lock(self);
		if ((reply_port == IP_NULL) ||
		    (self->ith_rpc_reply != IP_NULL))
			panic("exception_raise");
		self->ith_rpc_reply = reply_port;
	}

	ip_lock(reply_port);
	assert(ip_active(reply_port));
	ith_unlock(self);

	/*
	 *	Make a naked send-once right for the reply port,
	 *	to hand to the exception server.
	 *	Make an extra reference for the reply port,
	 *	to receive on.  This protects us against
	 *	mach_msg_abort_rpc.
	 */

	reply_port->ip_sorights++;
	ip_reference(reply_port);

	ip_reference(reply_port);
	self->ith_port = reply_port;

	reply_mqueue = &reply_port->ip_messages;
	imq_lock(reply_mqueue);
	assert(ipc_kmsg_queue_empty(&reply_mqueue->imq_messages));
	ip_unlock(reply_port);

	/*
	 *	Make sure we can queue to the destination port.
	 */

	if (!ip_lock_try(dest_port)) {
		imq_unlock(reply_mqueue);
		goto slow_exception_raise;
	}

	if (!ip_active(dest_port) ||
#if	NORMA_IPC
	    IP_NORMA_IS_PROXY(dest_port) ||
#endif	NORMA_IPC
	    (dest_port->ip_receiver == ipc_space_kernel)) {
		imq_unlock(reply_mqueue);
		ip_unlock(dest_port);
		goto slow_exception_raise;
	}

	/*
	 *	Find the destination message queue.
	 */

    {
	register ipc_pset_t dest_pset;

	dest_pset = dest_port->ip_pset;
	if (dest_pset == IPS_NULL)
		dest_mqueue = &dest_port->ip_messages;
	else
		dest_mqueue = &dest_pset->ips_messages;
    }

	if (!imq_lock_try(dest_mqueue)) {
		imq_unlock(reply_mqueue);
		ip_unlock(dest_port);
		goto slow_exception_raise;
	}

	/*
	 *	Safe to unlock dest_port, because we hold
	 *	dest_mqueue locked.  We never bother changing
	 *	dest_port->ip_msgcount.
	 */

	ip_unlock(dest_port);

	receiver = ipc_thread_queue_first(&dest_mqueue->imq_threads);
	if ((receiver == ITH_NULL) ||
	    !((receiver->swap_func == (void (*)()) mach_msg_continue) ||
	      ((receiver->swap_func ==
				(void (*)()) mach_msg_receive_continue) &&
	       (sizeof(struct mach_exception) <= receiver->ith_msize) &&
	       ((receiver->ith_option & MACH_RCV_NOTIFY) == 0))) ||
	    !thread_handoff(self, exception_raise_continue, receiver)) {
		imq_unlock(reply_mqueue);
		imq_unlock(dest_mqueue);
		goto slow_exception_raise;
	}
	counter(c_exception_raise_block++);

	assert(current_thread() == receiver);

	/*
	 *	We need to finish preparing self for its
	 *	time asleep in reply_mqueue.  self is left
	 *	holding the extra ref for reply_port.
	 */

	ipc_thread_enqueue_macro(&reply_mqueue->imq_threads, self);
	self->ith_state = MACH_RCV_IN_PROGRESS;
	self->ith_msize = MACH_MSG_SIZE_MAX;
	imq_unlock(reply_mqueue);

	/*
	 *	Finish extracting receiver from dest_mqueue.
	 */

	ipc_thread_rmqueue_first_macro(
		&dest_mqueue->imq_threads, receiver);
	imq_unlock(dest_mqueue);

	/*
	 *	Release the receiver's reference for his object.
	 */
    {
	register ipc_object_t object = receiver->ith_object;

	io_lock(object);
	io_release(object);
	io_check_unlock(object);
    }

    {
	register struct mach_exception *exc =
			(struct mach_exception *) &kmsg->ikm_header;
	ipc_space_t space = receiver->task->itk_space;

	/*
	 *	We are running as the receiver now.  We hold
	 *	the following resources, which must be consumed:
	 *		kmsg, send-once right for reply_port
	 *		send rights for dest_port, thread_port, task_port
	 *	Synthesize a kmsg for copyout to the receiver.
	 */

	exc->Head.msgh_bits = (MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE,
					      MACH_MSG_TYPE_PORT_SEND) |
			       MACH_MSGH_BITS_COMPLEX);
	exc->Head.msgh_size = sizeof *exc;
     /* exc->Head.msgh_remote_port later */
     /* exc->Head.msgh_local_port later */
	exc->Head.msgh_seqno = 0;
	exc->Head.msgh_id = MACH_EXCEPTION_ID;
	exc->threadType = exc_port_proto;
     /* exc->thread later */
	exc->taskType = exc_port_proto;
     /* exc->task later */
	exc->exceptionType = exc_code_proto;
	exc->exception = _exception;
	exc->codeType = exc_code_proto;
	exc->code = code;
	exc->subcodeType = exc_code_proto;
	exc->subcode = subcode;

	/*
	 *	Check that the receiver can handle the message.
	 */

	if (receiver->ith_rcv_size < sizeof(struct mach_exception)) {
		/*
		 *	ipc_kmsg_destroy is a handy way to consume
		 *	the resources we hold, but it requires setup.
		 */

		exc->Head.msgh_bits =
			(MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND,
					MACH_MSG_TYPE_PORT_SEND_ONCE) |
			 MACH_MSGH_BITS_COMPLEX);
		exc->Head.msgh_remote_port = (mach_port_t) dest_port;
		exc->Head.msgh_local_port = (mach_port_t) reply_port;
		exc->thread = (mach_port_t) thread_port;
		exc->task = (mach_port_t) task_port;

		ipc_kmsg_destroy(kmsg);
		thread_syscall_return(MACH_RCV_TOO_LARGE);
		/*NOTREACHED*/
	}

	is_write_lock(space);
	assert(space->is_active);

	/*
	 *	To do an atomic copyout, need simultaneous
	 *	locks on both ports and the space.
	 */

	ip_lock(dest_port);
	if (!ip_active(dest_port) ||
	    !ip_lock_try(reply_port)) {
	    abort_copyout:
		ip_unlock(dest_port);
		is_write_unlock(space);

		/*
		 *	Oh well, we have to do the header the slow way.
		 *	First make it look like it's in-transit.
		 */

		exc->Head.msgh_bits =
			(MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND,
					MACH_MSG_TYPE_PORT_SEND_ONCE) |
			 MACH_MSGH_BITS_COMPLEX);
		exc->Head.msgh_remote_port = (mach_port_t) dest_port;
		exc->Head.msgh_local_port = (mach_port_t) reply_port;

		mr = ipc_kmsg_copyout_header(&exc->Head, space,
					     MACH_PORT_NULL);
		if (mr == MACH_MSG_SUCCESS)
			goto copyout_body;

		/*
		 *	Ack!  Prepare for ipc_kmsg_copyout_dest.
		 *	It will consume thread_port and task_port.
		 */

		exc->thread = (mach_port_t) thread_port;
		exc->task = (mach_port_t) task_port;

		ipc_kmsg_copyout_dest(kmsg, space);
		(void) ipc_kmsg_put(receiver->ith_msg, kmsg,
				    sizeof(mach_msg_header_t));
		thread_syscall_return(mr);
		/*NOTREACHED*/
	}

	if (!ip_active(reply_port)) {
		ip_unlock(reply_port);
		goto abort_copyout;
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
		goto abort_copyout;

	entry = &table[index];
	table->ie_next = entry->ie_next;
	entry->ie_request = 0;

    {
	register mach_port_gen_t gen;

	assert((entry->ie_bits &~ IE_BITS_GEN_MASK) == 0);
	gen = entry->ie_bits + IE_BITS_GEN_ONE;

	exc->Head.msgh_remote_port = MACH_PORT_MAKE(index, gen);

	/* optimized ipc_right_copyout */

	entry->ie_bits = gen | (MACH_PORT_TYPE_SEND_ONCE | 1);
    }

	entry->ie_object = (ipc_object_t) reply_port;
	is_write_unlock(space);
    }

	/* optimized ipc_object_copyout_dest */

	assert(dest_port->ip_srights > 0);
	ip_release(dest_port);

	exc->Head.msgh_local_port =
		((dest_port->ip_receiver == space) ?
		 dest_port->ip_receiver_name : MACH_PORT_NULL);

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

    copyout_body:
	/*
	 *	Optimized version of ipc_kmsg_copyout_body,
	 *	to handle the two ports in the body.
	 */

	mr = (ipc_kmsg_copyout_object(space, (ipc_object_t) thread_port,
				      MACH_MSG_TYPE_PORT_SEND, &exc->thread) |
	      ipc_kmsg_copyout_object(space, (ipc_object_t) task_port,
				      MACH_MSG_TYPE_PORT_SEND, &exc->task));
	if (mr != MACH_MSG_SUCCESS) {
		(void) ipc_kmsg_put(receiver->ith_msg, kmsg,
				    kmsg->ikm_header.msgh_size);
		thread_syscall_return(mr | MACH_RCV_BODY_ERROR);
		/*NOTREACHED*/
	}
    }

	/*
	 *	Optimized version of ipc_kmsg_put.
	 *	We must check ikm_cache after copyoutmsg.
	 */

	ikm_check_initialized(kmsg, kmsg->ikm_size);
	assert(kmsg->ikm_size == IKM_SAVED_KMSG_SIZE);

	if (copyoutmsg((vm_offset_t) &kmsg->ikm_header, (vm_offset_t)receiver->ith_msg,
		       sizeof(struct mach_exception)) ||
	    (ikm_cache() != IKM_NULL)) {
		mr = ipc_kmsg_put(receiver->ith_msg, kmsg,
				  kmsg->ikm_header.msgh_size);
		thread_syscall_return(mr);
		/*NOTREACHED*/
	}

	ikm_cache() = kmsg;
	thread_syscall_return(MACH_MSG_SUCCESS);
	/*NOTREACHED*/
#ifndef	__GNUC__
	return; /* help for the compiler */
#endif

    slow_exception_raise: {
	register struct mach_exception *exc =
			(struct mach_exception *) &kmsg->ikm_header;
	ipc_kmsg_t reply_kmsg;
	mach_port_seqno_t reply_seqno;

	exception_raise_misses++;

	/*
	 *	We hold the following resources, which must be consumed:
	 *		kmsg, send-once right and ref for reply_port
	 *		send rights for dest_port, thread_port, task_port
	 *	Synthesize a kmsg to send.
	 */

	exc->Head.msgh_bits = (MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND,
					      MACH_MSG_TYPE_PORT_SEND_ONCE) |
			       MACH_MSGH_BITS_COMPLEX);
	exc->Head.msgh_size = sizeof *exc;
	exc->Head.msgh_remote_port = (mach_port_t) dest_port;
	exc->Head.msgh_local_port = (mach_port_t) reply_port;
	exc->Head.msgh_seqno = 0;
	exc->Head.msgh_id = MACH_EXCEPTION_ID;
	exc->threadType = exc_port_proto;
	exc->thread = (mach_port_t) thread_port;
	exc->taskType = exc_port_proto;
	exc->task = (mach_port_t) task_port;
	exc->exceptionType = exc_code_proto;
	exc->exception = _exception;
	exc->codeType = exc_code_proto;
	exc->code = code;
	exc->subcodeType = exc_code_proto;
	exc->subcode = subcode;

	ipc_mqueue_send_always(kmsg);

	/*
	 *	We are left with a ref for reply_port,
	 *	which we use to receive the reply message.
	 */

	ip_lock(reply_port);
	if (!ip_active(reply_port)) {
		ip_unlock(reply_port);
		exception_raise_continue_slow(MACH_RCV_PORT_DIED, IKM_NULL, /*dummy*/0);
		/*NOTREACHED*/
		return;
	}

	imq_lock(reply_mqueue);
	ip_unlock(reply_port);

	mr = ipc_mqueue_receive(reply_mqueue, MACH_MSG_OPTION_NONE,
				MACH_MSG_SIZE_MAX,
				MACH_MSG_TIMEOUT_NONE,
				FALSE, exception_raise_continue,
				&reply_kmsg, &reply_seqno);
	/* reply_mqueue is unlocked */

	exception_raise_continue_slow(mr, reply_kmsg, reply_seqno);
	/*NOTREACHED*/
    }
}

mach_msg_type_t exc_RetCode_proto = {
	/* msgt_name = */		MACH_MSG_TYPE_INTEGER_32,
	/* msgt_size = */		32,
	/* msgt_number = */		1,
	/* msgt_inline = */		TRUE,
	/* msgt_longform = */		FALSE,
	/* msgt_deallocate = */		FALSE,
	/* msgt_unused = */		0
};

/*
 *	Routine:	exception_parse_reply
 *	Purpose:
 *		Parse and consume an exception reply message.
 *	Conditions:
 *		The destination port right has already been consumed.
 *		The message buffer and anything else in it is consumed.
 *	Returns:
 *		The reply return code.
 */

kern_return_t
exception_parse_reply(kmsg)
	ipc_kmsg_t kmsg;
{
	register mig_reply_header_t *msg =
			(mig_reply_header_t *) &kmsg->ikm_header;
	kern_return_t kr;

	if ((msg->Head.msgh_bits !=
			MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, 0)) ||
	    (msg->Head.msgh_size != sizeof *msg) ||
	    (msg->Head.msgh_id != MACH_EXCEPTION_REPLY_ID) ||
	    (* (int *) &msg->RetCodeType != * (int *) &exc_RetCode_proto)) {
		/*
		 *	Bozo user sent us a misformatted reply.
		 */

		kmsg->ikm_header.msgh_remote_port = MACH_PORT_NULL;
		ipc_kmsg_destroy(kmsg);
		return MIG_REPLY_MISMATCH;
	}

	kr = msg->RetCode;

	if ((kmsg->ikm_size == IKM_SAVED_KMSG_SIZE) &&
	    (ikm_cache() == IKM_NULL))
		ikm_cache() = kmsg;
	else
		ikm_free(kmsg);

	return kr;
}

/*
 *	Routine:	exception_raise_continue
 *	Purpose:
 *		Continue after blocking for an exception.
 *	Conditions:
 *		Nothing locked.  We are running on a new kernel stack,
 *		with the exception state saved in the thread.  From here
 *		control goes back to user space.
 *	Returns:
 *		Doesn't return.
 */

void
exception_raise_continue()
{
	ipc_thread_t self = current_thread();
	ipc_port_t reply_port = self->ith_port;
	ipc_mqueue_t reply_mqueue = &reply_port->ip_messages;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	mr = ipc_mqueue_receive(reply_mqueue, MACH_MSG_OPTION_NONE,
				MACH_MSG_SIZE_MAX,
				MACH_MSG_TIMEOUT_NONE,
				TRUE, exception_raise_continue,
				&kmsg, &seqno);
	/* reply_mqueue is unlocked */

	exception_raise_continue_slow(mr, kmsg, seqno);
	/*NOTREACHED*/
}

/*
 *	Routine:	exception_raise_continue_slow
 *	Purpose:
 *		Continue after finishing an ipc_mqueue_receive
 *		for an exception reply message.
 *	Conditions:
 *		Nothing locked.  We hold a ref for reply_port.
 *	Returns:
 *		Doesn't return.
 */

void
exception_raise_continue_slow(mr, kmsg, seqno)
	mach_msg_return_t mr;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
{
	ipc_thread_t self = current_thread();
	ipc_port_t reply_port = self->ith_port;
	ipc_mqueue_t reply_mqueue = &reply_port->ip_messages;

	while (mr == MACH_RCV_INTERRUPTED) {
		/*
		 *	Somebody is trying to force this thread
		 *	to a clean point.  We must cooperate
		 *	and then resume the receive.
		 */

		while (thread_should_halt(self)) {
			/* don't terminate while holding a reference */
			if (self->ast & AST_TERMINATE)
				ipc_port_release(reply_port);
			thread_halt_self();
		}

		ip_lock(reply_port);
		if (!ip_active(reply_port)) {
			ip_unlock(reply_port);
			mr = MACH_RCV_PORT_DIED;
			break;
		}

		imq_lock(reply_mqueue);
		ip_unlock(reply_port);

		mr = ipc_mqueue_receive(reply_mqueue, MACH_MSG_OPTION_NONE,
					MACH_MSG_SIZE_MAX,
					MACH_MSG_TIMEOUT_NONE,
					FALSE, exception_raise_continue,
					&kmsg, &seqno);
		/* reply_mqueue is unlocked */
	}
	ipc_port_release(reply_port);

	assert((mr == MACH_MSG_SUCCESS) ||
	       (mr == MACH_RCV_PORT_DIED));

	if (mr == MACH_MSG_SUCCESS) {
		/*
		 *	Consume the reply message.
		 */

		ipc_port_release_sonce(reply_port);
		mr = exception_parse_reply(kmsg);
	}

	if ((mr == KERN_SUCCESS) ||
	    (mr == MACH_RCV_PORT_DIED)) {
		thread_exception_return();
		/*NOTREACHED*/
		return;
	}

	if (self->ith_exc != KERN_SUCCESS) {
		exception_try_task(self->ith_exc,
				   self->ith_exc_code,
				   self->ith_exc_subcode);
		/*NOTREACHED*/
		return;
	}

	exception_no_server();
	/*NOTREACHED*/
}

/*
 *	Routine:	exception_raise_continue_fast
 *	Purpose:
 *		Special-purpose fast continuation for exceptions.
 *	Conditions:
 *		reply_port is locked and alive.
 *		kmsg is our reply message.
 *	Returns:
 *		Doesn't return.
 */

void
exception_raise_continue_fast(reply_port, kmsg)
	ipc_port_t reply_port;
	ipc_kmsg_t kmsg;
{
	ipc_thread_t self = current_thread();
	kern_return_t kr;

	assert(ip_active(reply_port));
	assert(reply_port == self->ith_port);
	assert(reply_port == (ipc_port_t) kmsg->ikm_header.msgh_remote_port);
	assert(MACH_MSGH_BITS_REMOTE(kmsg->ikm_header.msgh_bits) ==
						MACH_MSG_TYPE_PORT_SEND_ONCE);

	/*
	 *	Release the send-once right (from the message header)
	 *	and the saved reference (from self->ith_port).
	 */

	reply_port->ip_sorights--;
	ip_release(reply_port);
	ip_release(reply_port);
	ip_unlock(reply_port);

	/*
	 *	Consume the reply message.
	 */

	kr = exception_parse_reply(kmsg);
	if (kr == KERN_SUCCESS) {
		thread_exception_return();
		/*NOTREACHED*/
		return; /* help for the compiler */
	}

	if (self->ith_exc != KERN_SUCCESS) {
		exception_try_task(self->ith_exc,
				   self->ith_exc_code,
				   self->ith_exc_subcode);
		/*NOTREACHED*/
	}

	exception_no_server();
	/*NOTREACHED*/
}
