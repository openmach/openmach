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
 */
/*
 *	File:	kern/ipc_kobject.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions for letting a port represent a kernel object.
 */

#include <mach_debug.h>
#include <mach_ipc_test.h>
#include <mach_machine_routines.h>
#include <norma_task.h>
#include <norma_vm.h>

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/notify.h>
#include <kern/ipc_kobject.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_thread.h>

#if	MACH_MACHINE_ROUTINES
#include <machine/machine_routines.h>
#endif


/*
 *	Routine:	ipc_kobject_server
 *	Purpose:
 *		Handle a message sent to the kernel.
 *		Generates a reply message.
 *	Conditions:
 *		Nothing locked.
 */

ipc_kmsg_t
ipc_kobject_server(request)
	ipc_kmsg_t request;
{
	mach_msg_size_t reply_size = ikm_less_overhead(8192);
	ipc_kmsg_t reply;
	kern_return_t kr;
	mig_routine_t routine;
	ipc_port_t *destp;

	reply = ikm_alloc(reply_size);
	if (reply == IKM_NULL) {
		printf("ipc_kobject_server: dropping request\n");
		ipc_kmsg_destroy(request);
		return IKM_NULL;
	}
	ikm_init(reply, reply_size);

	/*
	 * Initialize reply message.
	 */
	{
#define	InP	((mach_msg_header_t *) &request->ikm_header)
#define	OutP	((mig_reply_header_t *) &reply->ikm_header)

	    static mach_msg_type_t RetCodeType = {
		/* msgt_name = */		MACH_MSG_TYPE_INTEGER_32,
		/* msgt_size = */		32,
		/* msgt_number = */		1,
		/* msgt_inline = */		TRUE,
		/* msgt_longform = */		FALSE,
		/* msgt_unused = */		0
	    };
	    OutP->Head.msgh_bits =
		MACH_MSGH_BITS(MACH_MSGH_BITS_LOCAL(InP->msgh_bits), 0);
	    OutP->Head.msgh_size = sizeof(mig_reply_header_t);
	    OutP->Head.msgh_remote_port = InP->msgh_local_port;
	    OutP->Head.msgh_local_port  = MACH_PORT_NULL;
	    OutP->Head.msgh_seqno = 0;
	    OutP->Head.msgh_id = InP->msgh_id + 100;
#if 0
	    if (InP->msgh_id) {
		    static long _calls;
		    static struct { long id, count; } _counts[512];
		    int i, id;

		    id = InP->msgh_id;
		    for (i = 0; i < 511; i++) {
			    if (_counts[i].id == 0) {
				    _counts[i].id = id;
				    _counts[i].count++;
				    break;
			    }
			    if (_counts[i].id == id) {
				    _counts[i].count++;
				    break;
			    }
		    }
		    if (i == 511) {
			    _counts[i].id = id;
			    _counts[i].count++;
		    }
		    if ((++_calls & 0x7fff) == 0)
			    for (i = 0; i < 512; i++) {
				    if (_counts[i].id == 0)
					    break;
				    printf("%d: %d\n",
					   _counts[i].id, _counts[i].count);
			    }
	    }
#endif

	    OutP->RetCodeType = RetCodeType;

#undef	InP
#undef	OutP
	}

	/*
	 * Find the server routine to call, and call it
	 * to perform the kernel function
	 */
    {
	extern mig_routine_t	mach_server_routine(),
				mach_port_server_routine(),
				mach_host_server_routine(),
				device_server_routine(),
				device_pager_server_routine(),
				mach4_server_routine();
#if	MACH_DEBUG
	extern mig_routine_t	mach_debug_server_routine();
#endif
#if	NORMA_TASK
	extern mig_routine_t	mach_norma_server_routine();
	extern mig_routine_t	norma_internal_server_routine();
#endif
#if	NORMA_VM
	extern mig_routine_t	proxy_server_routine();
#endif

#if	MACH_MACHINE_ROUTINES
	extern mig_routine_t	MACHINE_SERVER_ROUTINE();
#endif

	check_simple_locks();
	if ((routine = mach_server_routine(&request->ikm_header)) != 0
	 || (routine = mach_port_server_routine(&request->ikm_header)) != 0
	 || (routine = mach_host_server_routine(&request->ikm_header)) != 0
	 || (routine = device_server_routine(&request->ikm_header)) != 0
	 || (routine = device_pager_server_routine(&request->ikm_header)) != 0
#if	MACH_DEBUG
	 || (routine = mach_debug_server_routine(&request->ikm_header)) != 0
#endif	MACH_DEBUG
#if	NORMA_TASK
	 || (routine = mach_norma_server_routine(&request->ikm_header)) != 0
	 || (routine = norma_internal_server_routine(&request->ikm_header)) != 0
#endif	NORMA_TASK
#if	NORMA_VM
	 || (routine = proxy_server_routine(&request->ikm_header)) != 0
#endif	NORMA_VM
	 || (routine = mach4_server_routine(&request->ikm_header)) != 0
#if	MACH_MACHINE_ROUTINES
	 || (routine = MACHINE_SERVER_ROUTINE(&request->ikm_header)) != 0
#endif	MACH_MACHINE_ROUTINES
	) {
	    (*routine)(&request->ikm_header, &reply->ikm_header);
	}
	else if (!ipc_kobject_notify(&request->ikm_header,&reply->ikm_header)){
		((mig_reply_header_t *) &reply->ikm_header)->RetCode
		    = MIG_BAD_ID;
#if	MACH_IPC_TEST
		printf("ipc_kobject_server: bogus kernel message, id=%d\n",
		       request->ikm_header.msgh_id);
#endif	MACH_IPC_TEST
	}
    }
	check_simple_locks();

	/*
	 *	Destroy destination. The following code differs from
	 *	ipc_object_destroy in that we release the send-once
	 *	right instead of generating a send-once notification
	 * 	(which would bring us here again, creating a loop).
	 *	It also differs in that we only expect send or
	 *	send-once rights, never receive rights.
	 *
	 *	We set msgh_remote_port to IP_NULL so that the kmsg
	 *	destroy routines don't try to destroy the port twice.
	 */
	destp = (ipc_port_t *) &request->ikm_header.msgh_remote_port;
	switch (MACH_MSGH_BITS_REMOTE(request->ikm_header.msgh_bits)) {
		case MACH_MSG_TYPE_PORT_SEND:
		ipc_port_release_send(*destp);
		break;
		
		case MACH_MSG_TYPE_PORT_SEND_ONCE:
		ipc_port_release_sonce(*destp);
		break;
		
		default:
#if MACH_ASSERT
		assert(!"ipc_object_destroy: strange destination rights");
#else
		panic("ipc_object_destroy: strange destination rights");
#endif
	}
	*destp = IP_NULL;

	kr = ((mig_reply_header_t *) &reply->ikm_header)->RetCode;
	if ((kr == KERN_SUCCESS) || (kr == MIG_NO_REPLY)) {
		/*
		 *	The server function is responsible for the contents
		 *	of the message.  The reply port right is moved
		 *	to the reply message, and we have deallocated
		 *	the destination port right, so we just need
		 *	to free the kmsg.
		 */

		/* like ipc_kmsg_put, but without the copyout */

		ikm_check_initialized(request, request->ikm_size);
		if ((request->ikm_size == IKM_SAVED_KMSG_SIZE) &&
		    (ikm_cache() == IKM_NULL))
			ikm_cache() = request;
		else
			ikm_free(request);
	} else {
		/*
		 *	The message contents of the request are intact.
		 *	Destroy everthing except the reply port right,
		 *	which is needed in the reply message.
		 */

		request->ikm_header.msgh_local_port = MACH_PORT_NULL;
		ipc_kmsg_destroy(request);
	}

	if (kr == MIG_NO_REPLY) {
		/*
		 *	The server function will send a reply message
		 *	using the reply port right, which it has saved.
		 */

		ikm_free(reply);
		return IKM_NULL;
	} else if (!IP_VALID((ipc_port_t)reply->ikm_header.msgh_remote_port)) {
		/*
		 *	Can't queue the reply message if the destination
		 *	(the reply port) isn't valid.
		 */

		ipc_kmsg_destroy(reply);
		return IKM_NULL;
	}

	return reply;
}

/*
 *	Routine:	ipc_kobject_set
 *	Purpose:
 *		Make a port represent a kernel object of the given type.
 *		The caller is responsible for handling refs for the
 *		kernel object, if necessary.
 *	Conditions:
 *		Nothing locked.  The port must be active.
 */

void
ipc_kobject_set(port, kobject, type)
	ipc_port_t port;
	ipc_kobject_t kobject;
	ipc_kobject_type_t type;
{
	ip_lock(port);
	assert(ip_active(port));
	port->ip_bits = (port->ip_bits &~ IO_BITS_KOTYPE) | type;
	port->ip_kobject = kobject;
	ip_unlock(port);
}

/*
 *	Routine:	ipc_kobject_destroy
 *	Purpose:
 *		Release any kernel object resources associated
 *		with the port, which is being destroyed.
 *
 *		This should only be needed when resources are
 *		associated with a user's port.  In the normal case,
 *		when the kernel is the receiver, the code calling
 *		ipc_port_dealloc_kernel should clean up the resources.
 *	Conditions:
 *		The port is not locked, but it is dead.
 */

void
ipc_kobject_destroy(
	ipc_port_t	port)
{
	switch (ip_kotype(port)) {
	    case IKOT_PAGER:
		vm_object_destroy(port);
		break;

	    case IKOT_PAGER_TERMINATING:
		vm_object_pager_wakeup(port);
		break;

	    default:
#if	MACH_ASSERT
		printf("ipc_kobject_destroy: port 0x%x, kobj 0x%x, type %d\n",
		       port, port->ip_kobject, ip_kotype(port));
#endif	MACH_ASSERT
		break;
	}
}

/*
 *	Routine:	ipc_kobject_notify
 *	Purpose:
 *		Deliver notifications to kobjects that care about them.
 */

boolean_t
ipc_kobject_notify(request_header, reply_header)
	mach_msg_header_t *request_header;
	mach_msg_header_t *reply_header;
{
	ipc_port_t port = (ipc_port_t) request_header->msgh_remote_port;

	((mig_reply_header_t *) reply_header)->RetCode = MIG_NO_REPLY;
	switch (request_header->msgh_id) {
		case MACH_NOTIFY_PORT_DELETED:
		case MACH_NOTIFY_MSG_ACCEPTED:
		case MACH_NOTIFY_PORT_DESTROYED:
		case MACH_NOTIFY_NO_SENDERS:
		case MACH_NOTIFY_SEND_ONCE:
		case MACH_NOTIFY_DEAD_NAME:
		break;

		default:
		return FALSE;
	}
	switch (ip_kotype(port)) {
#if	NORMA_VM
		case IKOT_XMM_OBJECT:
		return xmm_object_notify(request_header);

		case IKOT_XMM_PAGER:
		return xmm_pager_notify(request_header);

		case IKOT_XMM_KERNEL:
		return xmm_kernel_notify(request_header);

		case IKOT_XMM_REPLY:
		return xmm_reply_notify(request_header);
#endif	NORMA_VM

		case IKOT_DEVICE:
		return ds_notify(request_header);

		default:
		return FALSE;
	}
}
