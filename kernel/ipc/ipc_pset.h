/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 *	File:	ipc/ipc_pset.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for port sets.
 */

#ifndef	_IPC_IPC_PSET_H_
#define _IPC_IPC_PSET_H_

#include <mach/port.h>
#include <mach/kern_return.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_mqueue.h>
#include "ipc_target.h"

typedef struct ipc_pset {
	struct ipc_target ips_target;

} *ipc_pset_t;

#define ips_object		ips_target.ipt_object
#define ips_local_name		ips_target.ipt_name
#define ips_messages		ips_target.ipt_messages
#define	ips_references		ips_object.io_references

#define	IPS_NULL		((ipc_pset_t) IO_NULL)

#define	ips_active(pset)	io_active(&(pset)->ips_object)
#define	ips_lock(pset)		io_lock(&(pset)->ips_object)
#define	ips_lock_try(pset)	io_lock_try(&(pset)->ips_object)
#define	ips_unlock(pset)	io_unlock(&(pset)->ips_object)
#define	ips_check_unlock(pset)	io_check_unlock(&(pset)->ips_object)
#define	ips_reference(pset)	io_reference(&(pset)->ips_object)
#define	ips_release(pset)	io_release(&(pset)->ips_object)

extern kern_return_t
ipc_pset_alloc(/* ipc_space_t, mach_port_t *, ipc_pset_t * */);

extern kern_return_t
ipc_pset_alloc_name(/* ipc_space_t, mach_port_t, ipc_pset_t * */);

extern void
ipc_pset_add(/* ipc_pset_t, ipc_port_t */);

extern void
ipc_pset_remove(/* ipc_pset_t, ipc_port_t */);

extern kern_return_t
ipc_pset_move(/* ipc_space_t, mach_port_t, mach_port_t */);

extern void
ipc_pset_destroy(/* ipc_pset_t */);

#define	ipc_pset_reference(pset)	\
		ipc_object_reference(&(pset)->ips_object)

#define	ipc_pset_release(pset)		\
		ipc_object_release(&(pset)->ips_object)

extern void
ipc_pset_print(/* ipc_pset_t */);

#endif	_IPC_IPC_PSET_H_
