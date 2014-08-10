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
 *	File:	ipc/ipc_notify.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations of notification-sending functions.
 */

#ifndef	_IPC_IPC_NOTIFY_H_
#define _IPC_IPC_NOTIFY_H_

#include <mach_ipc_compat.h>

extern void
ipc_notify_init();

extern void
ipc_notify_port_deleted(/* ipc_port_t, mach_port_t */);

extern void
ipc_notify_msg_accepted(/* ipc_port_t, mach_port_t */);

extern void
ipc_notify_port_destroyed(/* ipc_port_t, ipc_port_t */);

extern void
ipc_notify_no_senders(/* ipc_port_t, mach_port_mscount_t */);

extern void
ipc_notify_send_once(/* ipc_port_t */);

extern void
ipc_notify_dead_name(/* ipc_port_t, mach_port_t */);

#if	MACH_IPC_COMPAT

extern void
ipc_notify_port_deleted_compat(/* ipc_port_t, mach_port_t */);

extern void
ipc_notify_msg_accepted_compat(/* ipc_port_t, mach_port_t */);

extern void
ipc_notify_port_destroyed_compat(/* ipc_port_t, ipc_port_t */);

#endif	MACH_IPC_COMPAT
#endif	_IPC_IPC_NOTIFY_H_
