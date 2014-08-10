/*
 * Copyright (c) 1993,1994 The University of Utah and
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

#ifndef MACH_RPC_H
#define MACH_RPC_H

#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/machine/rpc.h>

/*
 * Description of a port passed up by the leaky-register RPC path
 * when it needs to perform translation.
 */
struct rpc_port_desc {
	mach_port_t name;
	mach_msg_type_name_t msgt_name;
};

#endif	/* MACH_RPC_H */
