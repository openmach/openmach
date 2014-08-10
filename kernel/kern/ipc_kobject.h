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
 *	File:	kern/ipc_kobject.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations for letting a port represent a kernel object.
 */

#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_types.h>

#ifndef	_KERN_IPC_KOBJECT_H_
#define _KERN_IPC_KOBJECT_H_

#include <mach/machine/vm_types.h>

typedef vm_offset_t ipc_kobject_t;

#define	IKO_NULL	((ipc_kobject_t) 0)

typedef unsigned int ipc_kobject_type_t;

#define	IKOT_NONE		0
#define IKOT_THREAD		1
#define	IKOT_TASK		2
#define	IKOT_HOST		3
#define	IKOT_HOST_PRIV		4
#define	IKOT_PROCESSOR		5
#define	IKOT_PSET		6
#define	IKOT_PSET_NAME		7
#define	IKOT_PAGER		8
#define	IKOT_PAGING_REQUEST	9
#define	IKOT_DEVICE		10
#define	IKOT_XMM_OBJECT		11
#define	IKOT_XMM_PAGER		12
#define	IKOT_XMM_KERNEL		13
#define	IKOT_XMM_REPLY		14
#define	IKOT_PAGER_TERMINATING	15
#define IKOT_PAGING_NAME	16
#define IKOT_HOST_SECURITY	17
#define	IKOT_LEDGER		18
#define IKOT_MASTER_DEVICE	19
#define IKOT_ACT		20
#define IKOT_SUBSYSTEM		21
#define IKOT_IO_DONE_QUEUE	22
#define IKOT_SEMAPHORE		23
#define IKOT_LOCK_SET		24
#define IKOT_CLOCK		25
#define IKOT_CLOCK_CTRL		26
					/* << new entries here	*/
#define	IKOT_UNKNOWN		27	/* magic catchall	*/
#define	IKOT_MAX_TYPE		28	/* # of IKOT_ types	*/
 /* Please keep ipc/ipc_object.c:ikot_print_array up to date	*/

#define is_ipc_kobject(ikot)	(ikot != IKOT_NONE)

/*
 *	Define types of kernel objects that use page lists instead
 *	of entry lists for copyin of out of line memory.
 */

#define ipc_kobject_vm_page_list(ikot) 			\
	((ikot == IKOT_PAGING_REQUEST) || (ikot == IKOT_DEVICE))

#define ipc_kobject_vm_page_steal(ikot)	(ikot == IKOT_PAGING_REQUEST)

/* Initialize kernel server dispatch table */
/* XXX
extern void mig_init(void);
*/

/* Dispatch a kernel server function */
extern ipc_kmsg_t ipc_kobject_server(
	ipc_kmsg_t	request);

/* Make a port represent a kernel object of the given type */
extern void ipc_kobject_set(
	ipc_port_t		port,
	ipc_kobject_t		kobject,
	ipc_kobject_type_t	type);

/* Release any kernel object resources associated with a port */
extern void ipc_kobject_destroy(
	ipc_port_t		port);

#define	null_conversion(port)	(port)

#endif	/* _KERN_IPC_KOBJECT_H_ */
