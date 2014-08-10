/* 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#ifdef MIGRATING_THREADS

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/rpc.h>
#include <mach/notify.h>
#include <mach/mach_param.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <kern/task.h>
#include <kern/act.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_right.h>

#undef DEBUG_MPRC

/*
 * XXX need to identify if one endpoint of an RPC is the kernel to
 * ensure proper port name translation (or lack of).  This is bogus.
 */
#define ISKERNELACT(act)	((act)->task == kernel_task)

/*
 * Copy the indicated port from the task associated with the source
 * activation into the task associated with the destination activation.
 *
 * XXX on errors we should probably clear the portp to avoid leaking
 * info to the other side.
 */
kern_return_t
mach_port_rpc_copy(portp, sact, dact)
	struct rpc_port_desc *portp;
	struct Act *sact, *dact;
{
	ipc_space_t sspace, dspace;
	mach_msg_type_name_t tname;
	ipc_object_t iname;
	kern_return_t kr;

#ifdef DEBUG_MPRC
	printf("m_p_rpc_copy(portp=%x/%x, sact=%x, dact=%x): ",
	       portp->name, portp->msgt_name, sact, dact);
#endif
	sspace = sact->task->itk_space;
	dspace = dact->task->itk_space;
	if (sspace == IS_NULL || dspace == IS_NULL) {
#ifdef DEBUG_MPRC
		printf("bogus src (%x) or dst (%x) space\n", sspace, dspace);
#endif
		return KERN_INVALID_TASK;
	}

	if (!MACH_MSG_TYPE_PORT_ANY(portp->msgt_name)) {
#ifdef DEBUG_MPRC
		printf("invalid port type\n");
#endif
		return KERN_INVALID_VALUE;
	}

	if (ISKERNELACT(sact)) {
		iname = (ipc_object_t) portp->name;
		ipc_object_copyin_from_kernel(iname, portp->msgt_name);
		kr = KERN_SUCCESS;
	} else {
		kr = ipc_object_copyin(sspace, portp->name, portp->msgt_name,
				       &iname);
	}
	if (kr != KERN_SUCCESS) {
#ifdef DEBUG_MPRC
		printf("copyin returned %x\n", kr);
#endif
		return kr;
	}

	tname = ipc_object_copyin_type(portp->msgt_name);
	if (!IO_VALID(iname)) {
		portp->name = (mach_port_t) iname;
		portp->msgt_name = tname;
#ifdef DEBUG_MPRC
		printf("iport %x invalid\n", iname);
#endif
		return KERN_SUCCESS;
	}

	if (ISKERNELACT(dact)) {
		portp->name = (mach_port_t) iname;
		kr = KERN_SUCCESS;
	} else {
		kr = ipc_object_copyout(dspace, iname, tname, TRUE,
					&portp->name);
	}
	if (kr != KERN_SUCCESS) {
		ipc_object_destroy(iname, tname);

		if (kr == KERN_INVALID_CAPABILITY)
			portp->name = MACH_PORT_DEAD;
		else {
			portp->name = MACH_PORT_NULL;
#ifdef DEBUG_MPRC
			printf("copyout iport %x returned %x\n", iname);
#endif
			return kr;
		}
	}

	portp->msgt_name = tname;
#ifdef DEBUG_MPRC
	printf("portp=%x/%x, iname=%x\n", portp->name, portp->msgt_name, iname);
#endif
	return KERN_SUCCESS;
}

kern_return_t
mach_port_rpc_sig(space, name, buffer, buflen)
{
	return KERN_FAILURE;
}

#endif /* MIGRATING_THREADS */
