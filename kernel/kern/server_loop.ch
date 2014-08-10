/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	kern/server_loop.c
 *
 *	A common server loop for builtin tasks.
 */

/*
 *	Must define symbols for:
 *		SERVER_NAME		String name of this module
 *		SERVER_LOOP		Routine name for the loop
 *		SERVER_DISPATCH		MiG function(s) to handle message
 *
 *	Must redefine symbols for pager_server functions.
 */

#include <mach/port.h>
#include <mach/message.h>
#include <vm/vm_kern.h>		/* for kernel_map */

void SERVER_LOOP(rcv_set, max_size)
{
	register mach_msg_header_t *in_msg;
	register mach_msg_header_t *out_msg;
	register mach_msg_header_t *tmp_msg;
	vm_offset_t messages;
	mach_msg_return_t r;

	/*
	 *	Allocate our message buffers.
	 */

	messages = kalloc(2 * max_size);
	if (messages == 0)
		panic(SERVER_NAME);
	in_msg = (mach_msg_header_t *) messages;
	out_msg = (mach_msg_header_t *) (messages + max_size);

	/*
	 *	Service loop... receive messages and process them.
	 */

	for (;;) {
		/* receive first message */

	    receive_msg:
		r = mach_msg(in_msg, MACH_RCV_MSG, 0, max_size, rcv_set,
			     MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (r == MACH_MSG_SUCCESS)
			break;

		printf("%s: receive failed, 0x%x.\n", SERVER_NAME, r);
	}

	for (;;) {
		/* process request message */

		(void) SERVER_DISPATCH(in_msg, out_msg);

		/* send reply and receive next request */

		if (out_msg->msgh_remote_port == MACH_PORT_NULL)
			goto receive_msg;

		r = mach_msg(out_msg, MACH_SEND_MSG|MACH_RCV_MSG,
			     out_msg->msgh_size, max_size, rcv_set,
			     MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (r != MACH_MSG_SUCCESS) {
			printf("%s: send/receive failed, 0x%x.\n",
			       SERVER_NAME, r);
			goto receive_msg;
		}

		/* swap message buffers */

		tmp_msg = in_msg; in_msg = out_msg; out_msg = tmp_msg;
	}
}
