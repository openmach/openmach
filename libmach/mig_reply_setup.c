/* 
 * Mach Operating System
 * Copyright (c) 1992,1991 Carnegie Mellon University
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
 * Routine to set up a MiG reply message from a request message.
 *
 * Knows about the MiG reply message ID convention:
 *	reply_id = request_id + 100
 *
 * Sets up the RetCode type field.  Does NOT set a
 * return code value.
 */

#include <mach.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/mig_support.h>

static mach_msg_type_t RetCodeType = {
	/* msgt_name = */		MACH_MSG_TYPE_INTEGER_32,
	/* msgt_size = */		32,
	/* msgt_number = */		1,
	/* msgt_inline = */		TRUE,
	/* msgt_longform = */		FALSE,
	/* msgt_unused = */		0
};

void
mig_reply_setup(const mach_msg_header_t *request, mach_msg_header_t *reply)
{
#define	InP	(request)
#define	OutP	((mig_reply_header_t *) reply)

	OutP->Head.msgh_bits =
		MACH_MSGH_BITS(MACH_MSGH_BITS_LOCAL(InP->msgh_bits), 0);
	OutP->Head.msgh_size = sizeof(mig_reply_header_t);
	OutP->Head.msgh_remote_port = InP->msgh_local_port;
	OutP->Head.msgh_local_port  = MACH_PORT_NULL;
	OutP->Head.msgh_seqno = 0;
	OutP->Head.msgh_id = InP->msgh_id + 100;

	OutP->RetCodeType = RetCodeType;
}
