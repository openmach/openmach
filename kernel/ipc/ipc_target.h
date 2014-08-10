/*
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 */
/*
 *	File:	ipc_target.h
 *
 *	Common part of IPC ports and port sets
 *	representing a target of messages and migrating RPCs.
 */

#ifndef	_IPC_IPC_RECEIVER_H_
#define _IPC_IPC_RECEIVER_H_

#include "ipc_mqueue.h"
#include "ipc_object.h"
#include <mach/rpc.h>

typedef struct ipc_target {

	struct ipc_object ipt_object;

	mach_port_t ipt_name;
	struct ipc_mqueue ipt_messages;

#ifdef MIGRATING_THREADS
	/*** Migrating RPC stuff ***/

	int ipt_type;

	/* User entry info for migrating RPC */
	rpc_info_t ipt_rpcinfo;

	/* List of available activations, all active but not in use.  */
	struct Act *ipt_acts;

	/* TRUE if someone is waiting for an activation from this pool.  */
	int ipt_waiting;
#endif /* MIGRATING_THREADS */

} *ipc_target_t;

#define IPT_TYPE_MESSAGE_RPC	1
#define IPT_TYPE_MIGRATE_RPC	2

void ipc_target_init(struct ipc_target *ipt, mach_port_t name);
void ipc_target_terminate(struct ipc_target *ipt);

#define ipt_lock(ipt)		io_lock(&(ipt)->ipt_object)
#define ipt_unlock(ipt)		io_unlock(&(ipt)->ipt_object)
#define ipt_reference(ipt)	io_reference(&(ipt)->ipt_object)
#define ipt_release(ipt)	io_release(&(ipt)->ipt_object)
#define ipt_check_unlock(ipt)	io_check_unlock(&(ipt)->ipt_object)

#endif	/* _IPC_IPC_RECEIVER_H_ */
