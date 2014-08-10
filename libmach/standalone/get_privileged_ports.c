/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
#if 0  /* is this file needed anymore?  */

#include <mach.h>
#include <mach/message.h>

/*
 *	This function lets the first task (/mach_servers/startup)
 *	get the privileged ports from the kernel/bootstrap.
 *
 *	It should eventually be changed to use bootstrap_privileged_ports.
 */

kern_return_t
get_privileged_ports(privileged_host_port, device_server_port)
	mach_port_t *privileged_host_port;
	mach_port_t *device_server_port;
{
	mach_port_t bootstrap_port;
	mach_port_t reply_port;
	kern_return_t kr;

	struct msg {
		mach_msg_header_t hdr;
		mach_msg_type_t port_desc_1;
		mach_port_t port_1;
		mach_msg_type_t port_desc_2;
		mach_port_t port_2;
	} msg;

	/*
	 * Get our bootstrap port.
	 */

	kr = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (kr != KERN_SUCCESS)
		return kr;

	/*
	 * Allocate a reply port.
	 */

	reply_port = mig_get_reply_port();
	if (!MACH_PORT_VALID(reply_port))
		return KERN_FAILURE;

	/*
	 * Ask for the host and device ports.
	 */

	msg.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
					   MACH_MSG_TYPE_MAKE_SEND_ONCE);
	msg.hdr.msgh_remote_port = bootstrap_port;
	msg.hdr.msgh_local_port = reply_port;
	msg.hdr.msgh_id = 999999;

	kr = mach_msg(&msg.hdr, MACH_SEND_MSG|MACH_RCV_MSG,
		      sizeof msg.hdr, sizeof msg, reply_port,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (kr != MACH_MSG_SUCCESS)
		return kr;

	*privileged_host_port = msg.port_1;
	*device_server_port = msg.port_2;
	return KERN_SUCCESS;
}
#endif
