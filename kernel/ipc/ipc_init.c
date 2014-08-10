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
 *	File:	ipc/ipc_init.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to initialize the IPC system.
 */

#include <mach/kern_return.h>
#include <kern/mach_param.h>
#include <kern/ipc_host.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_marequest.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_hash.h>
#include <ipc/ipc_init.h>



vm_map_t ipc_kernel_map;
vm_size_t ipc_kernel_map_size = 1024 * 1024;

int ipc_space_max = SPACE_MAX;
int ipc_tree_entry_max = ITE_MAX;
int ipc_port_max = PORT_MAX;
int ipc_pset_max = SET_MAX;

/*
 *	Routine:	ipc_bootstrap
 *	Purpose:
 *		Initialization needed before the kernel task
 *		can be created.
 */

void
ipc_bootstrap(void)
{
	kern_return_t kr;

	ipc_port_multiple_lock_init();

	ipc_port_timestamp_lock_init();
	ipc_port_timestamp_data = 0;

	ipc_space_zone = zinit(sizeof(struct ipc_space),
			       ipc_space_max * sizeof(struct ipc_space),
			       sizeof(struct ipc_space),
			       IPC_ZONE_TYPE, "ipc spaces");

	ipc_tree_entry_zone =
		zinit(sizeof(struct ipc_tree_entry),
			ipc_tree_entry_max * sizeof(struct ipc_tree_entry),
			sizeof(struct ipc_tree_entry),
			IPC_ZONE_TYPE, "ipc tree entries");

	ipc_object_zones[IOT_PORT] =
		zinit(sizeof(struct ipc_port),
		      ipc_port_max * sizeof(struct ipc_port),
		      sizeof(struct ipc_port),
		      ZONE_EXHAUSTIBLE, "ipc ports");

	ipc_object_zones[IOT_PORT_SET] =
		zinit(sizeof(struct ipc_pset),
		      ipc_pset_max * sizeof(struct ipc_pset),
		      sizeof(struct ipc_pset),
		      IPC_ZONE_TYPE, "ipc port sets");

	/* create special spaces */

	kr = ipc_space_create_special(&ipc_space_kernel);
	assert(kr == KERN_SUCCESS);

	kr = ipc_space_create_special(&ipc_space_reply);
	assert(kr == KERN_SUCCESS);

#if	NORMA_IPC
	kr = ipc_space_create_special(&ipc_space_remote);
	assert(kr == KERN_SUCCESS);
#endif	NORMA_IPC

	/* initialize modules with hidden data structures */

	ipc_table_init();
	ipc_notify_init();
	ipc_hash_init();
	ipc_marequest_init();
}

/*
 *	Routine:	ipc_init
 *	Purpose:
 *		Final initialization of the IPC system.
 */

void
ipc_init()
{
	vm_offset_t min, max;

	ipc_kernel_map = kmem_suballoc(kernel_map, &min, &max,
				       ipc_kernel_map_size, TRUE);

	ipc_host_init();
}
