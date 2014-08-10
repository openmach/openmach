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
 *	Definitions of general Mach system traps.
 *
 *	IPC traps are defined in <mach/message.h>.
 *	Kernel RPC functions are defined in <mach/mach_interface.h>.
 */

#ifndef	_MACH_MACH_TRAPS_H_
#define _MACH_MACH_TRAPS_H_

#ifdef	MACH_KERNEL
#include <mach_ipc_compat.h>
#endif	/* MACH_KERNEL */

#include <mach/port.h>

mach_port_t	mach_reply_port
#ifdef	LINTLIBRARY
			()
	 { return MACH_PORT_NULL; }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

mach_port_t	mach_thread_self
#ifdef	LINTLIBRARY
			()
	 { return MACH_PORT_NULL; }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

#ifdef	__386BSD__
#undef mach_task_self
#endif
mach_port_t	mach_task_self
#ifdef	LINTLIBRARY
			()
	 { return MACH_PORT_NULL; }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

mach_port_t	mach_host_self
#ifdef	LINTLIBRARY
			()
	 { return MACH_PORT_NULL; }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */


/* Definitions for the old IPC interface. */

#if	MACH_IPC_COMPAT

port_t		task_self
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

port_t		task_notify
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

port_t		thread_self
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

port_t		thread_reply
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

port_t		host_self
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

port_t		host_priv_self
#ifdef	LINTLIBRARY
			()
	 { return(PORT_NULL); }
#else	/* LINTLIBRARY */
			();
#endif	/* LINTLIBRARY */

#endif	/* MACH_IPC_COMPAT */

#endif	/* _MACH_MACH_TRAPS_H_ */
