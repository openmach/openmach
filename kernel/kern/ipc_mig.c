/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#include <norma_vm.h>

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/thread_status.h>
#include <kern/ast.h>
#include <kern/ipc_tt.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/ipc_kobject.h>
#include <vm/vm_map.h>
#include <vm/vm_user.h>
#include <ipc/port.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_thread.h>
#include <device/device_types.h>


/*
 *	Routine:	mach_msg_send_from_kernel
 *	Purpose:
 *		Send a message from the kernel.
 *
 *		This is used by the client side of KernelUser interfaces
 *		to implement SimpleRoutines.  Currently, this includes
 *		device_reply and memory_object messages.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	Sent the message.
 *		MACH_SEND_INVALID_DATA	Bad destination port.
 */

mach_msg_return_t
mach_msg_send_from_kernel(
	mach_msg_header_t	*msg,
	mach_msg_size_t		send_size)
{
	ipc_kmsg_t kmsg;
	mach_msg_return_t mr;

	if (!MACH_PORT_VALID(msg->msgh_remote_port))
		return MACH_SEND_INVALID_DEST;

	mr = ipc_kmsg_get_from_kernel(msg, send_size, &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		panic("mach_msg_send_from_kernel");

	ipc_kmsg_copyin_from_kernel(kmsg);
	ipc_mqueue_send_always(kmsg);

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t
mach_msg_rpc_from_kernel(msg, send_size, reply_size)
	mach_msg_header_t *msg;
	mach_msg_size_t send_size;
	mach_msg_size_t reply_size;
{
	panic("mach_msg_rpc_from_kernel"); /*XXX*/
}

#if	NORMA_VM
/*
 *	Routine:	mach_msg_rpc_from_kernel
 *	Purpose:
 *		Send a message from the kernel and receive a reply.
 *		Uses ith_rpc_reply for the reply port.
 *
 *		This is used by the client side of KernelUser interfaces
 *		to implement Routines.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	Sent the message.
 *		MACH_RCV_PORT_DIED	The reply port was deallocated.
 */

mach_msg_return_t
mach_msg_rpc_from_kernel(
	mach_msg_header_t	*msg,
	mach_msg_size_t		send_size,
	mach_msg_size_t		rcv_size)
{
	ipc_thread_t self = current_thread();
	ipc_port_t reply;
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	assert(MACH_PORT_VALID(msg->msgh_remote_port));
	assert(msg->msgh_local_port == MACH_PORT_NULL);

	mr = ipc_kmsg_get_from_kernel(msg, send_size, &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		panic("mach_msg_rpc_from_kernel");

	ipc_kmsg_copyin_from_kernel(kmsg);

	ith_lock(self);
	assert(self->ith_self != IP_NULL);

	reply = self->ith_rpc_reply;
	if (reply == IP_NULL) {
		ith_unlock(self);
		reply = ipc_port_alloc_reply();
		ith_lock(self);
		if ((reply == IP_NULL) ||
		    (self->ith_rpc_reply != IP_NULL))
			panic("mach_msg_rpc_from_kernel");
		self->ith_rpc_reply = reply;
	}

	/* insert send-once right for the reply port */
	kmsg->ikm_header.msgh_local_port =
		(mach_port_t) ipc_port_make_sonce(reply);

	ipc_port_reference(reply);
	ith_unlock(self);

	ipc_mqueue_send_always(kmsg);

	for (;;) {
		ipc_mqueue_t mqueue;

		ip_lock(reply);
		if (!ip_active(reply)) {
			ip_unlock(reply);
			ipc_port_release(reply);
			return MACH_RCV_PORT_DIED;
		}

		assert(reply->ip_pset == IPS_NULL);
		mqueue = &reply->ip_messages;
		imq_lock(mqueue);
		ip_unlock(reply);

		mr = ipc_mqueue_receive(mqueue, MACH_MSG_OPTION_NONE,
					MACH_MSG_SIZE_MAX,
					MACH_MSG_TIMEOUT_NONE,
					FALSE, IMQ_NULL_CONTINUE,
					&kmsg, &seqno);
		/* mqueue is unlocked */
		if (mr == MACH_MSG_SUCCESS)
			break;

		assert((mr == MACH_RCV_INTERRUPTED) ||
		       (mr == MACH_RCV_PORT_DIED));

		while (thread_should_halt(self)) {
			/* don't terminate while holding a reference */
			if (self->ast & AST_TERMINATE)
				ipc_port_release(reply);
			thread_halt_self();
		}
	}
	ipc_port_release(reply);

	kmsg->ikm_header.msgh_seqno = seqno;

	if (rcv_size < kmsg->ikm_header.msgh_size) {
		ipc_kmsg_copyout_dest(kmsg, ipc_space_reply);
		ipc_kmsg_put_to_kernel(msg, kmsg, kmsg->ikm_header.msgh_size);
		return MACH_RCV_TOO_LARGE;
	}

	/*
	 *	We want to preserve rights and memory in reply!
	 *	We don't have to put them anywhere; just leave them
	 *	as they are.
	 */

	ipc_kmsg_copyout_to_kernel(kmsg, ipc_space_reply);
	ipc_kmsg_put_to_kernel(msg, kmsg, kmsg->ikm_header.msgh_size);
	return MACH_MSG_SUCCESS;
}
#endif	NORMA_VM

/*
 *	Routine:	mach_msg_abort_rpc
 *	Purpose:
 *		Destroy the thread's ith_rpc_reply port.
 *		This will interrupt a mach_msg_rpc_from_kernel
 *		with a MACH_RCV_PORT_DIED return code.
 *	Conditions:
 *		Nothing locked.
 */

void
mach_msg_abort_rpc(thread)
	ipc_thread_t thread;
{
	ipc_port_t reply = IP_NULL;

	ith_lock(thread);
	if (thread->ith_self != IP_NULL) {
		reply = thread->ith_rpc_reply;
		thread->ith_rpc_reply = IP_NULL;
	}
	ith_unlock(thread);

	if (reply != IP_NULL)
		ipc_port_dealloc_reply(reply);
}

/*
 *	Routine:	mach_msg
 *	Purpose:
 *		Like mach_msg_trap except that message buffers
 *		live in kernel space.  Doesn't handle any options.
 *
 *		This is used by in-kernel server threads to make
 *		kernel calls, to receive request messages, and
 *		to send reply messages.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 */

mach_msg_return_t
mach_msg(msg, option, send_size, rcv_size, rcv_name, time_out, notify)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_size_t rcv_size;
	mach_port_t rcv_name;
	mach_msg_timeout_t time_out;
	mach_port_t notify;
{
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	if (option & MACH_SEND_MSG) {
		mr = ipc_kmsg_get_from_kernel(msg, send_size, &kmsg);
		if (mr != MACH_MSG_SUCCESS)
			panic("mach_msg");

		mr = ipc_kmsg_copyin(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			ikm_free(kmsg);
			return mr;
		}

		do
			mr = ipc_mqueue_send(kmsg, MACH_MSG_OPTION_NONE,
					     MACH_MSG_TIMEOUT_NONE);
		while (mr == MACH_SEND_INTERRUPTED);
		assert(mr == MACH_MSG_SUCCESS);
	}

	if (option & MACH_RCV_MSG) {
		do {
			ipc_object_t object;
			ipc_mqueue_t mqueue;

			mr = ipc_mqueue_copyin(space, rcv_name,
					       &mqueue, &object);
			if (mr != MACH_MSG_SUCCESS)
				return mr;
			/* hold ref for object; mqueue is locked */

			mr = ipc_mqueue_receive(mqueue, MACH_MSG_OPTION_NONE,
						MACH_MSG_SIZE_MAX,
						MACH_MSG_TIMEOUT_NONE,
						FALSE, IMQ_NULL_CONTINUE,
						&kmsg, &seqno);
			/* mqueue is unlocked */
			ipc_object_release(object);
		} while (mr == MACH_RCV_INTERRUPTED);
		if (mr != MACH_MSG_SUCCESS)
			return mr;

		kmsg->ikm_header.msgh_seqno = seqno;

		if (rcv_size < kmsg->ikm_header.msgh_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			ipc_kmsg_put_to_kernel(msg, kmsg, sizeof *msg);
			return MACH_RCV_TOO_LARGE;
		}

		mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
				ipc_kmsg_put_to_kernel(msg, kmsg,
						kmsg->ikm_header.msgh_size);
			} else {
				ipc_kmsg_copyout_dest(kmsg, space);
				ipc_kmsg_put_to_kernel(msg, kmsg, sizeof *msg);
			}

			return mr;
		}

		ipc_kmsg_put_to_kernel(msg, kmsg, kmsg->ikm_header.msgh_size);
	}

	return MACH_MSG_SUCCESS;
}

/*
 *	Routine:	mig_get_reply_port
 *	Purpose:
 *		Called by client side interfaces living in the kernel
 *		to get a reply port.  This port is used for
 *		mach_msg() calls which are kernel calls.
 */

mach_port_t
mig_get_reply_port(void)
{
	ipc_thread_t self = current_thread();

	if (self->ith_mig_reply == MACH_PORT_NULL)
		self->ith_mig_reply = mach_reply_port();

	return self->ith_mig_reply;
}

/*
 *	Routine:	mig_dealloc_reply_port
 *	Purpose:
 *		Called by client side interfaces to get rid of a reply port.
 *		Shouldn't ever be called inside the kernel, because
 *		kernel calls shouldn't prompt Mig to call it.
 */

void
mig_dealloc_reply_port(
	mach_port_t	reply_port)
{
	panic("mig_dealloc_reply_port");
}

/*
 *	Routine:	mig_put_reply_port
 *	Purpose:
 *		Called by client side interfaces after each RPC to 
 *		let the client recycle the reply port if it wishes.
 */
void
mig_put_reply_port(
	mach_port_t	reply_port)
{
}

/*
 * mig_strncpy.c - by Joshua Block
 *
 * mig_strncp -- Bounded string copy.  Does what the library routine strncpy
 * OUGHT to do:  Copies the (null terminated) string in src into dest, a 
 * buffer of length len.  Assures that the copy is still null terminated
 * and doesn't overflow the buffer, truncating the copy if necessary.
 *
 * Parameters:
 * 
 *     dest - Pointer to destination buffer.
 * 
 *     src - Pointer to source string.
 * 
 *     len - Length of destination buffer.
 */
void mig_strncpy(dest, src, len)
char *dest, *src;
int len;
{
    int i;

    if (len <= 0)
	return;

    for (i=1; i<len; i++)
	if (! (*dest++ = *src++))
	    return;

    *dest = '\0';
    return;
}

#define	fast_send_right_lookup(name, port, abort)			\
MACRO_BEGIN								\
	register ipc_space_t space = current_space();			\
	register ipc_entry_t entry;					\
	register mach_port_index_t index = MACH_PORT_INDEX(name);	\
									\
	is_read_lock(space);						\
	assert(space->is_active);					\
									\
	if ((index >= space->is_table_size) ||				\
	    (((entry = &space->is_table[index])->ie_bits &		\
	      (IE_BITS_GEN_MASK|MACH_PORT_TYPE_SEND)) !=		\
	     (MACH_PORT_GEN(name) | MACH_PORT_TYPE_SEND))) {		\
		is_read_unlock(space);					\
		abort;							\
	}								\
									\
	port = (ipc_port_t) entry->ie_object;				\
	assert(port != IP_NULL);					\
									\
	ip_lock(port);							\
	/* can safely unlock space now that port is locked */		\
	is_read_unlock(space);						\
MACRO_END

device_t
port_name_to_device(name)
	mach_port_t name;
{
	register ipc_port_t port;
	register device_t device;
 
	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */
 
	/*
	 * Now map the port object to a device object.
	 * This is an inline version of dev_port_lookup().
	 */
	if (ip_active(port) && (ip_kotype(port) == IKOT_DEVICE)) {
		device = (device_t) port->ip_kobject;
		device_reference(device);
		ip_unlock(port);
		return device;                  
	}
 
	ip_unlock(port);
	return DEVICE_NULL;
 
       /*
        * The slow case.  The port wasn't easily accessible.
        */
    abort: {
	    ipc_port_t kern_port;
	    kern_return_t kr;
           
	    kr = ipc_object_copyin(current_space(), name,
				   MACH_MSG_TYPE_COPY_SEND,
				   (ipc_object_t *) &kern_port);
	    if (kr != KERN_SUCCESS)
		    return DEVICE_NULL;
 
	    device = dev_port_lookup(kern_port);
	    if (IP_VALID(kern_port))
		    ipc_port_release_send(kern_port);
	    return device;
    }
}

thread_t
port_name_to_thread(name)
	mach_port_t name;
{
	register ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_THREAD)) {
		register thread_t thread;

		thread = (thread_t) port->ip_kobject;
		assert(thread != THREAD_NULL);

		/* thread referencing is a bit complicated,
		   so don't bother to expand inline */
		thread_reference(thread);
		ip_unlock(port);

		return thread;
	}

	ip_unlock(port);
	return THREAD_NULL;

    abort: {
	thread_t thread;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return THREAD_NULL;

	thread = convert_port_to_thread(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return thread;
    }
}

task_t
port_name_to_task(name)
	mach_port_t name;
{
	register ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		register task_t task;

		task = (task_t) port->ip_kobject;
		assert(task != TASK_NULL);

		task_lock(task);
		/* can safely unlock port now that task is locked */
		ip_unlock(port);

		task->ref_count++;
		task_unlock(task);

		return task;
	}

	ip_unlock(port);
	return TASK_NULL;

    abort: {
	task_t task;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return TASK_NULL;

	task = convert_port_to_task(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return task;
    }
}

vm_map_t
port_name_to_map(
	mach_port_t	name)
{
	register ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		register vm_map_t map;

		map = ((task_t) port->ip_kobject)->map;
		assert(map != VM_MAP_NULL);

		simple_lock(&map->ref_lock);
		/* can safely unlock port now that map is locked */
		ip_unlock(port);

		map->ref_count++;
		simple_unlock(&map->ref_lock);

		return map;
	}

	ip_unlock(port);
	return VM_MAP_NULL;

    abort: {
	vm_map_t map;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return VM_MAP_NULL;

	map = convert_port_to_map(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return map;
    }
}

ipc_space_t
port_name_to_space(name)
	mach_port_t name;
{
	register ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		register ipc_space_t space;

		space = ((task_t) port->ip_kobject)->itk_space;
		assert(space != IS_NULL);

		simple_lock(&space->is_ref_lock_data);
		/* can safely unlock port now that space is locked */
		ip_unlock(port);

		space->is_references++;
		simple_unlock(&space->is_ref_lock_data);

		return space;
	}

	ip_unlock(port);
	return IS_NULL;

    abort: {
	ipc_space_t space;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return IS_NULL;

	space = convert_port_to_space(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return space;
    }
}

/*
 * Hack to translate a thread port to a thread pointer for calling
 * thread_get_state and thread_set_state.  This is only necessary
 * because the IPC message for these two operations overflows the
 * kernel stack.
 *
 * AARGH!
 */

kern_return_t thread_get_state_KERNEL(thread_port, flavor,
			old_state, old_state_count)
	mach_port_t	thread_port;	/* port right for thread */
	int		flavor;
	thread_state_t	old_state;	/* pointer to OUT array */
	natural_t	*old_state_count;	/* IN/OUT */
{
	thread_t	thread;
	kern_return_t	result;

	thread = port_name_to_thread(thread_port);
	result = thread_get_state(thread, flavor, old_state, old_state_count);
	thread_deallocate(thread);

	return result;
}

kern_return_t thread_set_state_KERNEL(thread_port, flavor,
			new_state, new_state_count)
	mach_port_t	thread_port;	/* port right for thread */
	int		flavor;
	thread_state_t	new_state;
	natural_t	new_state_count;
{
	thread_t	thread;
	kern_return_t	result;

	thread = port_name_to_thread(thread_port);
	result = thread_set_state(thread, flavor, new_state, new_state_count);
	thread_deallocate(thread);

	return result;
}

/*
 *	Things to keep in mind:
 *
 *	The idea here is to duplicate the semantics of the true kernel RPC.
 *	The destination port/object should be checked first, before anything
 *	that the user might notice (like ipc_object_copyin).  Return
 *	MACH_SEND_INTERRUPTED if it isn't correct, so that the user stub
 *	knows to fall back on an RPC.  For other return values, it won't
 *	retry with an RPC.  The retry might get a different (incorrect) rc.
 *	Return values are only set (and should only be set, with copyout)
 *	on successfull calls.
 */

kern_return_t
syscall_vm_map(
	mach_port_t	target_map,
	vm_offset_t	*address,
	vm_size_t	size,
	vm_offset_t	mask,
	boolean_t	anywhere,
	mach_port_t	memory_object,
	vm_offset_t	offset,
	boolean_t	copy,
	vm_prot_t	cur_protection,
	vm_prot_t	max_protection,
	vm_inherit_t	inheritance)
{
	vm_map_t		map;
	ipc_port_t		port;
	vm_offset_t		addr;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	if (MACH_PORT_VALID(memory_object)) {
		result = ipc_object_copyin(current_space(), memory_object,
					   MACH_MSG_TYPE_COPY_SEND,
					   (ipc_object_t *) &port);
		if (result != KERN_SUCCESS) {
			vm_map_deallocate(map);
			return result;
		}
	} else
		port = (ipc_port_t) memory_object;

	copyin((char *)address, (char *)&addr, sizeof(vm_offset_t));
	result = vm_map(map, &addr, size, mask, anywhere,
			port, offset, copy,
			cur_protection, max_protection,	inheritance);
	if (result == KERN_SUCCESS)
		copyout((char *)&addr, (char *)address, sizeof(vm_offset_t));
	if (IP_VALID(port))
		ipc_port_release_send(port);
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_vm_allocate(target_map, address, size, anywhere)
	mach_port_t		target_map;
	vm_offset_t		*address;
	vm_size_t		size;
	boolean_t		anywhere;
{
	vm_map_t		map;
	vm_offset_t		addr;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	copyin((char *)address, (char *)&addr, sizeof(vm_offset_t));
	result = vm_allocate(map, &addr, size, anywhere);
	if (result == KERN_SUCCESS)
		copyout((char *)&addr, (char *)address, sizeof(vm_offset_t));
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_vm_deallocate(target_map, start, size)
	mach_port_t		target_map;
	vm_offset_t		start;
	vm_size_t		size;
{
	vm_map_t		map;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	result = vm_deallocate(map, start, size);
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_task_create(parent_task, inherit_memory, child_task)
	mach_port_t	parent_task;
	boolean_t	inherit_memory;
	mach_port_t	*child_task;		/* OUT */
{
	task_t		t, c;
	ipc_port_t	port;
	mach_port_t 	name;
	kern_return_t	result;

	t = port_name_to_task(parent_task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_create(t, inherit_memory, &c);
	if (result == KERN_SUCCESS) {
		port = (ipc_port_t) convert_task_to_port(c);
		/* always returns a name, even for non-success return codes */
		(void) ipc_kmsg_copyout_object(current_space(),
					       (ipc_object_t) port,
					       MACH_MSG_TYPE_PORT_SEND, &name);
		copyout((char *)&name, (char *)child_task,
			sizeof(mach_port_t));
	}
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_terminate(task)
	mach_port_t	task;
{
	task_t		t;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_terminate(t);
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_suspend(task)
	mach_port_t	task;
{
	task_t		t;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_suspend(t);
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_set_special_port(task, which_port, port_name)
	mach_port_t	task;
	int		which_port;
	mach_port_t	port_name;
{
	task_t		t;
	ipc_port_t	port;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	if (MACH_PORT_VALID(port_name)) {
		result = ipc_object_copyin(current_space(), port_name,
					   MACH_MSG_TYPE_COPY_SEND,
					   (ipc_object_t *) &port);
		if (result != KERN_SUCCESS) {
			task_deallocate(t);
			return result;
		}
	} else
		port = (ipc_port_t) port_name;

	result = task_set_special_port(t, which_port, port);
	if ((result != KERN_SUCCESS) && IP_VALID(port))
		ipc_port_release_send(port);
	task_deallocate(t);

	return result;
}

kern_return_t
syscall_mach_port_allocate(task, right, namep)
	mach_port_t task;
	mach_port_right_t right;
	mach_port_t *namep;
{
	ipc_space_t space;
	mach_port_t name;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_allocate(space, right, &name);
	if (kr == KERN_SUCCESS)
		copyout((char *)&name, (char *)namep, sizeof(mach_port_t));
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_allocate_name(task, right, name)
	mach_port_t task;
	mach_port_right_t right;
	mach_port_t name;
{
	ipc_space_t space;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_allocate_name(space, right, name);
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_deallocate(task, name)
	mach_port_t task;
	mach_port_t name;
{
	ipc_space_t space;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_deallocate(space, name);
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_insert_right(task, name, right, rightType)
	mach_port_t task;
	mach_port_t name;
	mach_port_t right;
	mach_msg_type_name_t rightType;
{
	ipc_space_t space;
	ipc_object_t object;
	mach_msg_type_name_t newtype;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	if (!MACH_MSG_TYPE_PORT_ANY(rightType)) {
		is_release(space);
		return KERN_INVALID_VALUE;
	}

	if (MACH_PORT_VALID(right)) {
		kr = ipc_object_copyin(current_space(), right, rightType,
				       &object);
		if (kr != KERN_SUCCESS) {
			is_release(space);
			return kr;
		}
	} else
		object = (ipc_object_t) right;
	newtype = ipc_object_copyin_type(rightType);

	kr = mach_port_insert_right(space, name, (ipc_port_t) object, newtype);
	if ((kr != KERN_SUCCESS) && IO_VALID(object))
		ipc_object_destroy(object, newtype);
	is_release(space);

	return kr;
}

kern_return_t syscall_thread_depress_abort(thread)
	mach_port_t	thread;
{
	thread_t	t;
	kern_return_t	result;

	t = port_name_to_thread(thread);
	if (t == THREAD_NULL)
		return MACH_SEND_INTERRUPTED;

	result = thread_depress_abort(t);
	thread_deallocate(t);

	return result;
}

/*
 * Device traps -- these are way experimental.
 */

extern io_return_t ds_device_write_trap();
extern io_return_t ds_device_writev_trap();

io_return_t
syscall_device_write_request(mach_port_t	device_name,
			     mach_port_t	reply_name,
			     dev_mode_t		mode,
			     recnum_t		recnum,
			     vm_offset_t	data,
			     vm_size_t		data_count)
{
	device_t	dev;
	ipc_port_t	reply_port;
	io_return_t	res;

	/*
	 * First try to translate the device name.
	 *
	 * If this fails, return KERN_INVALID_CAPABILITY.
	 * Caller knows that this most likely means that
	 * device is not local to node and IPC should be used.
	 *
	 * If kernel doesn't do device traps, kern_invalid()
	 * will be called instead of this function which will
	 * return KERN_INVALID_ARGUMENT.
	 */
	dev = port_name_to_device(device_name);
	if (dev == DEVICE_NULL)
		return KERN_INVALID_CAPABILITY;

	/*
	 * Translate reply port.
	 */
	if (reply_name == MACH_PORT_NULL)
		reply_port = IP_NULL;
	else {
		/* Homey don't play that. */
		device_deallocate(dev);
		return KERN_INVALID_RIGHT;
	}

	/* note: doesn't take reply_port arg yet. */
	res = ds_device_write_trap(dev, /*reply_port,*/
				   mode, recnum,
				   data, data_count);

	/*
	 * Give up reference from port_name_to_device.
	 */
	device_deallocate(dev);
	return res;
}

io_return_t
syscall_device_writev_request(mach_port_t	device_name,
			      mach_port_t	reply_name,
			      dev_mode_t	mode,
			      recnum_t		recnum,
			      io_buf_vec_t	*iovec,
			      vm_size_t		iocount)
{
	device_t	dev;
	ipc_port_t	reply_port;
	io_return_t	res;

	/*
	 * First try to translate the device name.
	 *
	 * If this fails, return KERN_INVALID_CAPABILITY.
	 * Caller knows that this most likely means that
	 * device is not local to node and IPC should be used.
	 *
	 * If kernel doesn't do device traps, kern_invalid()
	 * will be called instead of this function which will
	 * return KERN_INVALID_ARGUMENT.
	 */
	dev = port_name_to_device(device_name);
	if (dev == DEVICE_NULL)
		return KERN_INVALID_CAPABILITY;

	/*
	 * Translate reply port.
	 */
	if (reply_name == MACH_PORT_NULL)
		reply_port = IP_NULL;
	else {
		/* Homey don't play that. */
		device_deallocate(dev);
		return KERN_INVALID_RIGHT;
	}

	/* note: doesn't take reply_port arg yet. */
	res = ds_device_writev_trap(dev, /*reply_port,*/
				    mode, recnum,
				    iovec, iocount);

	/*
	 * Give up reference from port_name_to_device.
	 */
	device_deallocate(dev);
	return res;
}


