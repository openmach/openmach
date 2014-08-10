/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 */
/*
 *	File:	ipc/mach_msg.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations of internal messaging primitives.
 */

#ifndef	_IPC_MACH_MSG_H_
#define _IPC_MACH_MSG_H_

#include <mach_ipc_compat.h>

#include <mach/boolean.h>
#include <mach/message.h>

extern mach_msg_return_t
mach_msg_send(/* mach_msg_header_t *, mach_msg_option_t,
		 mach_msg_size_t, mach_msg_timeout_t, mach_port_t */);

extern mach_msg_return_t
mach_msg_receive(/* mach_msg_header_t *, mach_msg_option_t,
		    mach_msg_size_t, mach_port_t,
		    mach_msg_timeout_t, mach_port_t */);

extern void
mach_msg_receive_continue();

extern void
mach_msg_continue();

extern boolean_t
mach_msg_interrupt(/* thread_t */);

#if	MACH_IPC_COMPAT

extern void
msg_receive_continue();

#endif	MACH_IPC_COMPAT
#endif	_IPC_MACH_MSG_H_
