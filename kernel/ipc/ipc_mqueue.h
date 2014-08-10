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
 *	File:	ipc/ipc_mqueue.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for message queues.
 */

#ifndef	_IPC_IPC_MQUEUE_H_
#define _IPC_IPC_MQUEUE_H_

#include <mach/message.h>
#include <kern/assert.h>
#include <kern/lock.h>
#include <kern/macro_help.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_thread.h>

typedef struct ipc_mqueue {
	decl_simple_lock_data(, imq_lock_data)
	struct ipc_kmsg_queue imq_messages;
	struct ipc_thread_queue imq_threads;
} *ipc_mqueue_t;

#define	IMQ_NULL		((ipc_mqueue_t) 0)

#define	imq_lock_init(mq)	simple_lock_init(&(mq)->imq_lock_data)
#define	imq_lock(mq)		simple_lock(&(mq)->imq_lock_data)
#define	imq_lock_try(mq)	simple_lock_try(&(mq)->imq_lock_data)
#define	imq_unlock(mq)		simple_unlock(&(mq)->imq_lock_data)

extern void
ipc_mqueue_init(/* ipc_mqueue_t */);

extern void
ipc_mqueue_move(/* ipc_mqueue_t, ipc_mqueue_t, ipc_port_t */);

extern void
ipc_mqueue_changed(/* ipc_mqueue_t, mach_msg_return_t */);

extern mach_msg_return_t
ipc_mqueue_send(/* ipc_kmsg_t, mach_msg_option_t, mach_msg_timeout_t */);

#define	IMQ_NULL_CONTINUE	((void (*)()) 0)

extern mach_msg_return_t
ipc_mqueue_receive(/* ipc_mqueue_t, mach_msg_option_t,
		      mach_msg_size_t, mach_msg_timeout_t,
		      boolean_t, void (*)(),
		      ipc_kmsg_t *, mach_port_seqno_t * */);

/*
 *	extern void
 *	ipc_mqueue_send_always(ipc_kmsg_t);
 *
 *	Unfortunately, to avoid warnings/lint about unused variables
 *	when assertions are turned off, we need two versions of this.
 */

#include <kern/assert.h>

#if	MACH_ASSERT

#define	ipc_mqueue_send_always(kmsg)					\
MACRO_BEGIN								\
	mach_msg_return_t mr;						\
									\
	mr = ipc_mqueue_send((kmsg), MACH_SEND_ALWAYS,			\
			     MACH_MSG_TIMEOUT_NONE);			\
	assert(mr == MACH_MSG_SUCCESS);					\
MACRO_END

#else	MACH_ASSERT

#define	ipc_mqueue_send_always(kmsg)					\
MACRO_BEGIN								\
	(void) ipc_mqueue_send((kmsg), MACH_SEND_ALWAYS,		\
			       MACH_MSG_TIMEOUT_NONE);			\
MACRO_END

#endif	/* MACH_ASSERT */

#endif	/* _IPC_IPC_MQUEUE_H_ */
