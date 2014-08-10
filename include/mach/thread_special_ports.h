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
 *	File:	mach/thread_special_ports.h
 *
 *	Defines codes for special_purpose thread ports.  These are NOT
 *	port identifiers - they are only used for the thread_get_special_port
 *	and thread_set_special_port routines.
 *	
 */

#ifndef	_MACH_THREAD_SPECIAL_PORTS_H_
#define _MACH_THREAD_SPECIAL_PORTS_H_

#ifdef	MACH_KERNEL
#include <mach_ipc_compat.h>
#endif	/* MACH_KERNEL */

#define THREAD_KERNEL_PORT	1	/* Represents the thread to the outside
					   world.*/
#define THREAD_EXCEPTION_PORT	3	/* Exception messages for the thread
					   are sent to this port. */

/*
 *	Definitions for ease of use
 */

#define thread_get_kernel_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_KERNEL_PORT, (port)))

#define thread_set_kernel_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_KERNEL_PORT, (port)))

#define thread_get_exception_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_EXCEPTION_PORT, (port)))

#define thread_set_exception_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_EXCEPTION_PORT, (port)))


/* Definitions for the old IPC interface. */

#if	MACH_IPC_COMPAT

#define THREAD_REPLY_PORT	2	/* Default reply port for the thread's
					   use. */

#define thread_get_reply_port(thread, port)	\
		(thread_get_special_port((thread), THREAD_REPLY_PORT, (port)))

#define thread_set_reply_port(thread, port)	\
		(thread_set_special_port((thread), THREAD_REPLY_PORT, (port)))

#endif	/* MACH_IPC_COMPAT */

#endif	/* _MACH_THREAD_SPECIAL_PORTS_H_ */
