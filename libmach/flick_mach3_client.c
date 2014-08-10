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

#if 0

#include <mach/port.h>
#include <mach/message.h>
#include <mach/flick_mach3_glue.h>

#define LIBMACH_OPTIONS	(MACH_SEND_INTERRUPT|MACH_RCV_INTERRUPT)

#define FLICK_NO_MEMORY		499 /*XXX*/

mach_msg_return_t flick_mach3_rpc_grow_buf(struct flick_mach3_rpc_desc *rpc)
{
	char *newbuf;

	rpc->msg_buf_size *= 2;
	if (!(newbuf = malloc(rpc->msg_buf_size)))
		return FLICK_NO_MEMORY;
	memcpy(newbuf, rpc->msg_buf, rpc->send_end_ofs);
	if (rpc->msg_buf != rpc->init_buf)
		free(rpc->msg_buf);
	rpc->msg_buf = newbuf;

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t flick_mach3_rpc(struct flick_mach3_rpc_desc *rpc,
				  mach_port_t send_target, mach_msg_bits_t send_msgh_bits)
{
	mach_msg_return_t mr;
	mach_msg_header_t *hdr;
	vm_size_t send_size, rcv_buf_size;
	mach_port_t reply_port;
	mach_msg_type_t int_type = { MACH_MSG_TYPE_INTEGER_32, 32, 1, 1, 0, 0 };

	/* The message has been encoded 4*4 bytes past the beginning of the send_msg buffer.
	   The first 8 encoded bytes past this contain the IDL ID field;
	   the next 8 after that contain the MIG message code, if the IDL ID is IDL_MIG (2).  */
	if (((int*)(rpc->msg_buf))[5] == 2)
	{
		/* It's MIG IDL - extract the actual ID and use it in the msgh_id of the header.
		   We don't need to send either the IDL ID or the message ID in the body.  */
		hdr = (mach_msg_header_t*)(rpc->msg_buf+2*4);
		hdr->msgh_id = ((int*)(rpc->msg_buf))[7];
		send_size = rpc->send_end_ofs - 2*4;
		rcv_buf_size = rpc->msg_buf_size - 2*4;
	}
	else
	{
		/* It's not MIG IDL - the message ID could be any data type(s).
		   Use the IDL ID as the MIG msgh_id.  XXX add offset???  */
		hdr = (mach_msg_header_t*)(rpc->msg_buf);
		hdr->msgh_id = ((int*)(rpc->msg_buf))[5];
		send_size = rpc->send_end_ofs;
		rcv_buf_size = rpc->msg_buf_size;
	}

	/* Grab a port to receive the reply on.  */
	reply_port = mig_get_reply_port();

	/* Finish filling in the send message header.
	   No need to set msgh_size; it's a parameter to mach_msg_trap().  */
	hdr->msgh_bits = send_msgh_bits;
	hdr->msgh_remote_port = send_target;
	hdr->msgh_local_port = reply_port;

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
	mr = mach_msg_trap(hdr, MACH_SEND_MSG|MACH_RCV_MSG|MACH_RCV_LARGE,
			   send_size, rcv_buf_size, reply_port, 0, 0);

	if (mr != MACH_MSG_SUCCESS)
	{
		while (mr == MACH_SEND_INTERRUPTED)
		{
			/* Retry both the send and the receive.  */
			mr = mach_msg_trap(hdr,
				MACH_SEND_MSG|MACH_RCV_MSG|MACH_RCV_LARGE,
				send_size, rcv_buf_size, reply_port, 0, 0);
		}

		while ((mr == MACH_RCV_INTERRUPTED) || (mr == MACH_RCV_TOO_LARGE))
		{
			if (mr == MACH_RCV_TOO_LARGE)
			{
				/* Oops, message too large - grow the buffer.  */
				rcv_buf_size = rpc->msg_buf_size = hdr->msgh_size;
				if (rpc->msg_buf != rpc->init_buf)
					free(rpc->msg_buf);
				if (!(rpc->msg_buf = malloc(rcv_buf_size)))
				{
					mig_dealloc_reply_port(reply_port);
					return FLICK_NO_MEMORY;
				}
				hdr = (mach_msg_header_t*)(rpc->msg_buf);
			}

			/* Retry the receive only
			   (the request message has already been sent successfully).  */
			mr = mach_msg_trap(hdr,
				MACH_RCV_MSG|MACH_RCV_LARGE,
				0, rcv_buf_size, reply_port, 0, 0);
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

	/* Pull apart the reply header for the stub's unmarshaling code.  */
	rpc->rcv_ofs = (vm_offset_t)hdr - (vm_offset_t)(rpc->msg_buf);
	rpc->rcv_end_ofs = rpc->rcv_ofs + hdr->msgh_size;
	if (hdr->msgh_id >= 100)
	{
		/* It's a MIG IDL reply message.  */
		mach_msg_id_t id = hdr->msgh_id;
		((mach_msg_type_t*)hdr)[2] = int_type;
		((unsigned32_t*)hdr)[3] = 2; /* IDL_MIG */
		((mach_msg_type_t*)hdr)[4] = int_type;
		((unsigned32_t*)hdr)[5] = id;
		rpc->rcv_ofs += 2*4;
	}
	else
	{
		/* It's a reply message in some other IDL.  */
		unsigned32_t idl_id = hdr->msgh_id;
		((mach_msg_type_t*)hdr)[4] = int_type;
		((unsigned32_t*)hdr)[5] = idl_id;
		rpc->rcv_ofs += 4*4;
	}

	return MACH_MSG_SUCCESS;
}

#endif 0
