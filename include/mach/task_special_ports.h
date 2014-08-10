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
 *	File:	mach/task_special_ports.h
 *
 *	Defines codes for special_purpose task ports.  These are NOT
 *	port identifiers - they are only used for the task_get_special_port
 *	and task_set_special_port routines.
 *	
 */

#ifndef	_MACH_TASK_SPECIAL_PORTS_H_
#define _MACH_TASK_SPECIAL_PORTS_H_

#ifdef	MACH_KERNEL
#include <mach_ipc_compat.h>
#endif	/* MACH_KERNEL */

#define TASK_KERNEL_PORT	1	/* Represents task to the outside
					   world.*/
#define TASK_EXCEPTION_PORT	3	/* Exception messages for task are
					   sent to this port. */
#define TASK_BOOTSTRAP_PORT	4	/* Bootstrap environment for task. */

/*
 *	Definitions for ease of use
 */

#define task_get_kernel_port(task, port)	\
		(task_get_special_port((task), TASK_KERNEL_PORT, (port)))

#define task_set_kernel_port(task, port)	\
		(task_set_special_port((task), TASK_KERNEL_PORT, (port)))

#define task_get_exception_port(task, port)	\
		(task_get_special_port((task), TASK_EXCEPTION_PORT, (port)))

#define task_set_exception_port(task, port)	\
		(task_set_special_port((task), TASK_EXCEPTION_PORT, (port)))

#define task_get_bootstrap_port(task, port)	\
		(task_get_special_port((task), TASK_BOOTSTRAP_PORT, (port)))

#define task_set_bootstrap_port(task, port)	\
		(task_set_special_port((task), TASK_BOOTSTRAP_PORT, (port)))


/* Definitions for the old IPC interface. */

#if	MACH_IPC_COMPAT

#define TASK_NOTIFY_PORT	2	/* Task receives kernel IPC
					   notifications here. */

#define task_get_notify_port(task, port)	\
		(task_get_special_port((task), TASK_NOTIFY_PORT, (port)))

#define task_set_notify_port(task, port)	\
		(task_set_special_port((task), TASK_NOTIFY_PORT, (port)))

#endif	/* MACH_IPC_COMPAT */

#endif	/* _MACH_TASK_SPECIAL_PORTS_H_ */
