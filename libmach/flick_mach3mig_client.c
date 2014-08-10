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
 * Client-side helper routines for the Flick Mach3 backend
 */

#if 0  /* Brian didn't have it working yet. */

#include <mach/port.h>
#include <mach/message.h>
#include <flick_mach3mig_glue.h>

mach_msg_return_t flick_mach3mig_rpc_grow_buf(struct flick_mach3mig_rpc_desc *rpc)
{
	mig_reply_header_t *newbuf;

	rpc->msg_buf_size *= 2;
	if (!(newbuf = malloc(rpc->msg_buf_size)))
		return FLICK_NO_MEMORY;
	memcpy(newbuf, rpc->msg_buf, rpc->send_end_ofs);
	if (rpc->msg_buf != &rpc->init_buf)
		free(rpc->msg_buf);
	rpc->msg_buf = newbuf;

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t flick_mach3mig_rpc(struct flick_mach3mig_rpc_desc *rpc)
{
	mach_msg_return_t mr;
	mach_port_t reply_port;

	/* Grab a port to receive the reply on.  */
	reply_port = mig_get_reply_port();
	rpc->msg_buf->Head.msgh_local_port = reply_port;

	/*
	 * Consider the following cases:
	 *	1) Errors in pseudo-receive (eg, MACH_SEND_INTERRUPTED
	 *	plus special bits).
	 *	2) Use of MACH_SEND_INTERRUPT/MACH_RCV_INTERRUPT options.
	 *	3) RPC calls with interruptions in one/both halves.
	 *	4) Exception reply messages that are bigger
	 *	   than the expected non-exception reply message.
	 *
	 * We refrain from passing the option bits that we implement
	 * to the kernel.  This prevents their presence from inhibiting
	 * the kernel's fast paths (when it checks the option value).
	 */

  print_message(rpc->msg_buf, rpc->send_end_ofs);
	mr = mach_msg_trap(&rpc->msg_buf->Head, MACH_SEND_MSG|MACH_RCV_MSG|MACH_RCV_LARGE,
			   rpc->send_end_ofs, rpc->msg_buf_size, reply_port, 0, 0);

	if (mr != MACH_MSG_SUCCESS)
	{
		while (mr == MACH_SEND_INTERRUPTED)
		{
			/* Retry both the send and the receive.  */
			mr = mach_msg_trap(&rpc->msg_buf->Head,
				MACH_SEND_MSG|MACH_RCV_MSG|MACH_RCV_LARGE,
				rpc->send_end_ofs, rpc->msg_buf_size, reply_port, 0, 0);
		}

		while ((mr == MACH_RCV_INTERRUPTED) || (mr == MACH_RCV_TOO_LARGE))
		{
			if (mr == MACH_RCV_TOO_LARGE)
			{
				/* Oops, message too large - grow the buffer.  */
				rpc->msg_buf_size = rpc->msg_buf->Head.msgh_size;
				if (rpc->msg_buf != &rpc->init_buf)
					free(rpc->msg_buf);
				if (!(rpc->msg_buf = malloc(rpc->msg_buf_size)))
				{
					mig_dealloc_reply_port(reply_port);
					return FLICK_NO_MEMORY;
				}
				rpc->msg_buf->Head = *(mach_msg_header_t*)(rpc->msg_buf);
			}

			/* Retry the receive only
			   (the request message has already been sent successfully).  */
			mr = mach_msg_trap(&rpc->msg_buf->Head,
				MACH_RCV_MSG|MACH_RCV_LARGE,
				0, rpc->msg_buf_size, reply_port, 0, 0);
		}

		if (mr != MACH_MSG_SUCCESS)
		{
			if ((mr == MACH_SEND_INVALID_REPLY) ||
			    (mr == MACH_SEND_INVALID_MEMORY) ||
			    (mr == MACH_SEND_INVALID_RIGHT) ||
			    (mr == MACH_SEND_INVALID_TYPE) ||
			    (mr == MACH_SEND_MSG_TOO_SMALL) ||
			    (mr == MACH_RCV_INVALID_NAME))
				mig_dealloc_reply_port(reply_port);
			else
				mig_put_reply_port(reply_port);
			return mr;
		}
	}

	/* Stash the reply port again for future use.  */
	mig_put_reply_port(reply_port);

	if (rpc->msg_buf->RetCode != KERN_SUCCESS)
	{
		/* XXX typecheck */
		return rpc->msg_buf->RetCode;
	}

	rpc->rcv_ofs = sizeof(mig_reply_header_t);
	rpc->rcv_end_ofs = rpc->msg_buf->Head.msgh_size;

	return MACH_MSG_SUCCESS;
}

#endif 0
