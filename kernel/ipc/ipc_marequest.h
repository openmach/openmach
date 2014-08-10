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
 *	File:	ipc/ipc_marequest.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for msg-accepted requests.
 */

#ifndef	_IPC_IPC_MAREQUEST_H_
#define _IPC_IPC_MAREQUEST_H_

#include <mach_ipc_debug.h>

#include <mach/kern_return.h>
#include <mach/port.h>

/*
 *	A msg-accepted request is made when MACH_SEND_NOTIFY is used
 *	to force a message to a send right.  The IE_BITS_MAREQUEST bit
 *	in an entry indicates the entry is blocked because MACH_SEND_NOTIFY
 *	has already been used to force a message.  The kmsg holds
 *	a pointer to the marequest; it is destroyed when the kmsg
 *	is received/destroyed.  (If the send right is destroyed,
 *	this just changes imar_name.  If the space is destroyed,
 *	the marequest is left unchanged.)
 *
 *	Locking considerations:  The imar_space field is read-only and
 *	points to the space which locks the imar_name field.  imar_soright
 *	is read-only.  Normally it is a non-null send-once right for
 *	the msg-accepted notification, but in compat mode it is null
 *	and the notification goes to the space's notify port.  Normally
 *	imar_name is non-null, but if the send right is destroyed then
 *	it is changed to be null.  imar_next is locked by a bucket lock;
 *	imar_name is read-only when the request is in a bucket.  (So lookups
 *	in the bucket can safely check imar_space and imar_name.)
 *	imar_space and imar_soright both hold references.
 */

typedef struct ipc_marequest {
	struct ipc_space *imar_space;
	mach_port_t imar_name;
	struct ipc_port *imar_soright;
	struct ipc_marequest *imar_next;
} *ipc_marequest_t;

#define	IMAR_NULL		((ipc_marequest_t) 0)


extern void
ipc_marequest_init();

#if	MACH_IPC_DEBUG

extern unsigned int
ipc_marequest_info(/* unsigned int *, hash_info_bucket_t *, unsigned int */);

#endif	MACH_IPC_DEBUG

extern mach_msg_return_t
ipc_marequest_create(/* ipc_space_t space, mach_port_t name,
			ipc_port_t soright, ipc_marequest_t *marequestp */);

extern void
ipc_marequest_cancel(/* ipc_space_t space, mach_port_t name */);

extern void
ipc_marequest_rename(/* ipc_space_t space,
			mach_port_t old, mach_port_t new */);

extern void
ipc_marequest_destroy(/* ipc_marequest_t marequest */);

#endif	_IPC_IPC_MAREQUEST_H_
