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
 *	File:	ipc_target.c
 *
 *	Implementation for common part of IPC ports and port sets
 *	representing a target of messages and migrating RPCs.
 */

#include "sched_prim.h"
#include "ipc_target.h"

void
ipc_target_init(struct ipc_target *ipt, mach_port_t name)
{
	ipt->ipt_name = name;
	ipc_mqueue_init(&ipt->ipt_messages);

#ifdef MIGRATING_THREADS
	ipt->ipt_type = IPT_TYPE_MESSAGE_RPC;
	ipt->ipt_acts = 0;

	ipc_target_machine_init(ipt);
#endif
}

void
ipc_target_terminate(struct ipc_target *ipt)
{
}

#ifdef MIGRATING_THREADS
struct Act *
ipc_target_block(struct ipc_target *ipt)
{
	struct Act *act;

	ipt_lock(ipt);
	while ((act = ipt->ipt_acts) == 0) {
		/* XXX mp unsafe */
		ipt->ipt_waiting = 1;
		ipt_unlock(ipt);
		thread_wait((int)&ipt->ipt_acts, FALSE);
		ipt_lock(ipt);
	}
	ipt->ipt_acts = act->ipt_next;
	ipt_unlock(ipt);

	return act;
}

void
ipc_target_wakeup(struct ipc_target *ipt)
{
	ipt_lock(ipt);
	if (ipt->ipt_waiting) {
		thread_wakeup((int)&ipt->ipt_acts);
		ipt->ipt_waiting = 0;
	}
	ipt_unlock(ipt);
}
#endif /* MIGRATING_THREADS */

