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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	ll/89
 */

#ifndef	_DEVICE_NET_IO_H_
#define	_DEVICE_NET_IO_H_

/*
 * Utilities for playing with network messages.
 */

#include <mach/machine/vm_types.h>
#include <ipc/ipc_kmsg.h>

#include <kern/macro_help.h>
#include <kern/lock.h>
#include <kern/kalloc.h>

#include <device/net_status.h>

/*
 * A network packet is wrapped in a kernel message while in
 * the kernel.
 */

#define	net_kmsg(kmsg)	((net_rcv_msg_t)&(kmsg)->ikm_header)

/*
 * Interrupt routines may allocate and free net_kmsgs with these
 * functions.  net_kmsg_get may return IKM_NULL.
 */

extern ipc_kmsg_t net_kmsg_get();
extern void net_kmsg_put();

/*
 * Network utility routines.
 */

extern void net_packet();
extern void net_filter();
extern io_return_t net_getstat();
extern io_return_t net_write();

/*
 * Non-interrupt code may allocate and free net_kmsgs with these functions.
 */

extern vm_size_t net_kmsg_size;

#define net_kmsg_alloc()	((ipc_kmsg_t) kalloc(net_kmsg_size))
#define net_kmsg_free(kmsg)	kfree((vm_offset_t) (kmsg), net_kmsg_size)

#endif	_DEVICE_NET_IO_H_
