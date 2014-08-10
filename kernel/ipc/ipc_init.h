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
 *	File:	ipc/ipc_init.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations of functions to initialize the IPC system.
 */

#ifndef	_IPC_IPC_INIT_H_
#define _IPC_IPC_INIT_H_

/* all IPC zones should be exhaustible */
#define IPC_ZONE_TYPE	ZONE_EXHAUSTIBLE

extern int ipc_space_max;
extern int ipc_tree_entry_max;
extern int ipc_port_max;
extern int ipc_pset_max;

/*
 * Exported interfaces
 */

/* IPC initialization needed before creation of kernel task */
extern void ipc_bootstrap(void);

/* Remaining IPC initialization */
extern void ipc_init(void);

#endif	/* _IPC_IPC_INIT_H_ */
