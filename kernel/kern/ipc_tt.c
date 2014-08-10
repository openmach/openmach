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
 * File:	ipc_tt.c
 * Purpose:
 *	Task and thread related IPC functions.
 */

#include <mach_ipc_compat.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/mach_param.h>
#include <mach/task_special_ports.h>
#include <mach/thread_special_ports.h>
#include <vm/vm_kern.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/ipc_kobject.h>
#include <kern/ipc_tt.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_table.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_right.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_object.h>



/*
 *	Routine:	ipc_task_init
 *	Purpose:
 *		Initialize a task's IPC state.
 *
 *		If non-null, some state will be inherited from the parent.
 *		The parent must be appropriately initialized.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_task_init(
	task_t		task,
	task_t		parent)
{
	ipc_space_t space;
	ipc_port_t kport;
	kern_return_t kr;
	int i;


	kr = ipc_space_create(&ipc_table_entries[0], &space);
	if (kr != KERN_SUCCESS)
		panic("ipc_task_init");


	kport = ipc_port_alloc_kernel();
	if (kport == IP_NULL)
		panic("ipc_task_init");

	itk_lock_init(task);
	task->itk_self = kport;
	task->itk_sself = ipc_port_make_send(kport);
	task->itk_space = space;

	if (parent == TASK_NULL) {
		task->itk_exception = IP_NULL;
		task->itk_bootstrap = IP_NULL;
		for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
			task->itk_registered[i] = IP_NULL;
	} else {
		itk_lock(parent);
		assert(parent->itk_self != IP_NULL);

		/* inherit registered ports */

		for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
			task->itk_registered[i] =
				ipc_port_copy_send(parent->itk_registered[i]);

		/* inherit exception and bootstrap ports */

		task->itk_exception =
			ipc_port_copy_send(parent->itk_exception);
		task->itk_bootstrap =
			ipc_port_copy_send(parent->itk_bootstrap);

		itk_unlock(parent);
	}
}

/*
 *	Routine:	ipc_task_enable
 *	Purpose:
 *		Enable a task for IPC access.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_task_enable(
	task_t		task)
{
	ipc_port_t kport;

	itk_lock(task);
	kport = task->itk_self;
	if (kport != IP_NULL)
		ipc_kobject_set(kport, (ipc_kobject_t) task, IKOT_TASK);
	itk_unlock(task);
}

/*
 *	Routine:	ipc_task_disable
 *	Purpose:
 *		Disable IPC access to a task.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_task_disable(
	task_t		task)
{
	ipc_port_t kport;

	itk_lock(task);
	kport = task->itk_self;
	if (kport != IP_NULL)
		ipc_kobject_set(kport, IKO_NULL, IKOT_NONE);
	itk_unlock(task);
}

/*
 *	Routine:	ipc_task_terminate
 *	Purpose:
 *		Clean up and destroy a task's IPC state.
 *	Conditions:
 *		Nothing locked.  The task must be suspended.
 *		(Or the current thread must be in the task.)
 */

void
ipc_task_terminate(
	task_t		task)
{
	ipc_port_t kport;
	int i;

	itk_lock(task);
	kport = task->itk_self;

	if (kport == IP_NULL) {
		/* the task is already terminated (can this happen?) */
		itk_unlock(task);
		return;
	}

	task->itk_self = IP_NULL;
	itk_unlock(task);

	/* release the naked send rights */

	if (IP_VALID(task->itk_sself))
		ipc_port_release_send(task->itk_sself);
	if (IP_VALID(task->itk_exception))
		ipc_port_release_send(task->itk_exception);
	if (IP_VALID(task->itk_bootstrap))
		ipc_port_release_send(task->itk_bootstrap);

	for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
		if (IP_VALID(task->itk_registered[i]))
			ipc_port_release_send(task->itk_registered[i]);

	/* destroy the space, leaving just a reference for it */

	ipc_space_destroy(task->itk_space);

	/* destroy the kernel port */

	ipc_port_dealloc_kernel(kport);
}

/*
 *	Routine:	ipc_thread_init
 *	Purpose:
 *		Initialize a thread's IPC state.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_thread_init(thread)
	thread_t thread;
{
	ipc_port_t kport;

	kport = ipc_port_alloc_kernel();
	if (kport == IP_NULL)
		panic("ipc_thread_init");

	ipc_thread_links_init(thread);
	ipc_kmsg_queue_init(&thread->ith_messages);

	ith_lock_init(thread);
	thread->ith_self = kport;
	thread->ith_sself = ipc_port_make_send(kport);
	thread->ith_exception = IP_NULL;

	thread->ith_mig_reply = MACH_PORT_NULL;
	thread->ith_rpc_reply = IP_NULL;

#if	MACH_IPC_COMPAT
    {
	ipc_space_t space = thread->task->itk_space;
	ipc_port_t port;
	mach_port_t name;
	kern_return_t kr;

	kr = ipc_port_alloc_compat(space, &name, &port);
	if (kr != KERN_SUCCESS)
		panic("ipc_thread_init");
	/* port is locked and active */

	/*
	 *	Now we have a reply port.  We need to make a naked
	 *	send right to stash in ith_reply.  We can't use
	 *	ipc_port_make_send, because we can't unlock the port
	 *	before making the right.  Also we don't want to
	 *	increment ip_mscount.  The net effect of all this
	 *	is the same as doing
	 *		ipc_port_alloc_kernel		get the port
	 *		ipc_port_make_send		make the send right
	 *		ipc_object_copyin_from_kernel	grab receive right
	 *		ipc_object_copyout_compat	and give to user
	 */

	port->ip_srights++;
	ip_reference(port);
	ip_unlock(port);

	thread->ith_reply = port;
    }
#endif	MACH_IPC_COMPAT
}

/*
 *	Routine:	ipc_thread_enable
 *	Purpose:
 *		Enable a thread for IPC access.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_thread_enable(thread)
	thread_t thread;
{
	ipc_port_t kport;

	ith_lock(thread);
	kport = thread->ith_self;
	if (kport != IP_NULL)
		ipc_kobject_set(kport, (ipc_kobject_t) thread, IKOT_THREAD);
	ith_unlock(thread);
}

/*
 *	Routine:	ipc_thread_disable
 *	Purpose:
 *		Disable IPC access to a thread.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_thread_disable(thread)
	thread_t thread;
{
	ipc_port_t kport;

	ith_lock(thread);
	kport = thread->ith_self;
	if (kport != IP_NULL)
		ipc_kobject_set(kport, IKO_NULL, IKOT_NONE);
	ith_unlock(thread);
}

/*
 *	Routine:	ipc_thread_terminate
 *	Purpose:
 *		Clean up and destroy a thread's IPC state.
 *	Conditions:
 *		Nothing locked.  The thread must be suspended.
 *		(Or be the current thread.)
 */

void
ipc_thread_terminate(thread)
	thread_t thread;
{
	ipc_port_t kport;

	ith_lock(thread);
	kport = thread->ith_self;

	if (kport == IP_NULL) {
		/* the thread is already terminated (can this happen?) */
		ith_unlock(thread);
		return;
	}

	thread->ith_self = IP_NULL;
	ith_unlock(thread);

	assert(ipc_kmsg_queue_empty(&thread->ith_messages));

	/* release the naked send rights */

	if (IP_VALID(thread->ith_sself))
		ipc_port_release_send(thread->ith_sself);
	if (IP_VALID(thread->ith_exception))
		ipc_port_release_send(thread->ith_exception);

#if	MACH_IPC_COMPAT
	if (IP_VALID(thread->ith_reply)) {
		ipc_space_t space = thread->task->itk_space;
		ipc_port_t port = thread->ith_reply;
		ipc_entry_t entry;
		mach_port_t name;

		/* destroy any rights the task may have for the port */

		is_write_lock(space);
		if (space->is_active &&
		    ipc_right_reverse(space, (ipc_object_t) port,
				      &name, &entry)) {
			/* reply port is locked and active */
			ip_unlock(port);

			(void) ipc_right_destroy(space, name, entry);
			/* space is unlocked */
		} else
			is_write_unlock(space);

		ipc_port_release_send(port);
	}

	/*
	 *	Note we do *not* destroy any rights the space may have
	 *	for the thread's kernel port.  The old IPC code did this,
	 *	to avoid generating a notification when the port is
	 *	destroyed.  However, this isn't a good idea when
	 *	the kernel port is interposed, because then it doesn't
	 *	happen, exposing the interposition to the task.
	 *	Because we don't need the efficiency hack, I flushed
	 *	this behaviour, introducing a small incompatibility
	 *	with the old IPC code.
	 */
#endif	MACH_IPC_COMPAT

	/* destroy the kernel port */

	ipc_port_dealloc_kernel(kport);
}

#if	0
/*
 *	Routine:	retrieve_task_self
 *	Purpose:
 *		Return a send right (possibly null/dead)
 *		for the task's user-visible self port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_task_self(task)
	task_t task;
{
	ipc_port_t port;

	assert(task != TASK_NULL);

	itk_lock(task);
	if (task->itk_self != IP_NULL)
		port = ipc_port_copy_send(task->itk_sself);
	else
		port = IP_NULL;
	itk_unlock(task);

	return port;
}

/*
 *	Routine:	retrieve_thread_self
 *	Purpose:
 *		Return a send right (possibly null/dead)
 *		for the thread's user-visible self port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_thread_self(thread)
	thread_t thread;
{
	ipc_port_t port;

	assert(thread != ITH_NULL);

	ith_lock(thread);
	if (thread->ith_self != IP_NULL)
		port = ipc_port_copy_send(thread->ith_sself);
	else
		port = IP_NULL;
	ith_unlock(thread);

	return port;
}
#endif	0

/*
 *	Routine:	retrieve_task_self_fast
 *	Purpose:
 *		Optimized version of retrieve_task_self,
 *		that only works for the current task.
 *
 *		Return a send right (possibly null/dead)
 *		for the task's user-visible self port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_task_self_fast(
	register task_t		task)
{
	register ipc_port_t port;

	assert(task == current_task());

	itk_lock(task);
	assert(task->itk_self != IP_NULL);

	if ((port = task->itk_sself) == task->itk_self) {
		/* no interposing */

		ip_lock(port);
		assert(ip_active(port));
		ip_reference(port);
		port->ip_srights++;
		ip_unlock(port);
	} else
		port = ipc_port_copy_send(port);
	itk_unlock(task);

	return port;
}

/*
 *	Routine:	retrieve_thread_self_fast
 *	Purpose:
 *		Optimized version of retrieve_thread_self,
 *		that only works for the current thread.
 *
 *		Return a send right (possibly null/dead)
 *		for the thread's user-visible self port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_thread_self_fast(thread)
	register thread_t thread;
{
	register ipc_port_t port;

	assert(thread == current_thread());

	ith_lock(thread);
	assert(thread->ith_self != IP_NULL);

	if ((port = thread->ith_sself) == thread->ith_self) {
		/* no interposing */

		ip_lock(port);
		assert(ip_active(port));
		ip_reference(port);
		port->ip_srights++;
		ip_unlock(port);
	} else
		port = ipc_port_copy_send(port);
	ith_unlock(thread);

	return port;
}

#if	0
/*
 *	Routine:	retrieve_task_exception
 *	Purpose:
 *		Return a send right (possibly null/dead)
 *		for the task's exception port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_task_exception(task)
	task_t task;
{
	ipc_port_t port;

	assert(task != TASK_NULL);

	itk_lock(task);
	if (task->itk_self != IP_NULL)
		port = ipc_port_copy_send(task->itk_exception);
	else
		port = IP_NULL;
	itk_unlock(task);

	return port;
}

/*
 *	Routine:	retrieve_thread_exception
 *	Purpose:
 *		Return a send right (possibly null/dead)
 *		for the thread's exception port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_thread_exception(thread)
	thread_t thread;
{
	ipc_port_t port;

	assert(thread != ITH_NULL);

	ith_lock(thread);
	if (thread->ith_self != IP_NULL)
		port = ipc_port_copy_send(thread->ith_exception);
	else
		port = IP_NULL;
	ith_unlock(thread);

	return port;
}
#endif	0

/*
 *	Routine:	mach_task_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his own task port.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

mach_port_t
mach_task_self(void)
{
	task_t task = current_task();
	ipc_port_t sright;

	sright = retrieve_task_self_fast(task);
	return ipc_port_copyout_send(sright, task->itk_space);
}

/*
 *	Routine:	mach_thread_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his own thread port.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

mach_port_t
mach_thread_self()
{
	thread_t thread = current_thread();
	task_t task = thread->task;
	ipc_port_t sright;

	sright = retrieve_thread_self_fast(thread);
	return ipc_port_copyout_send(sright, task->itk_space);
}

/*
 *	Routine:	mach_reply_port [mach trap]
 *	Purpose:
 *		Allocate a port for the caller.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

mach_port_t
mach_reply_port(void)
{
	ipc_port_t port;
	mach_port_t name;
	kern_return_t kr;

	kr = ipc_port_alloc(current_task()->itk_space, &name, &port);
	if (kr == KERN_SUCCESS)
		ip_unlock(port);
	else
		name = MACH_PORT_NULL;

	return name;
}

#if	MACH_IPC_COMPAT

/*
 *	Routine:	retrieve_task_notify
 *	Purpose:
 *		Return a reference (or null) for
 *		the task's notify port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_task_notify(task)
	task_t task;
{
	ipc_space_t space = task->itk_space;
	ipc_port_t port;

	is_read_lock(space);
	if (space->is_active) {
		port = space->is_notify;
		if (IP_VALID(port))
			ipc_port_reference(port);
	} else
		port = IP_NULL;
	is_read_unlock(space);

	return port;
}

/*
 *	Routine:	retrieve_thread_reply
 *	Purpose:
 *		Return a reference (or null) for
 *		the thread's reply port.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
retrieve_thread_reply(thread)
	thread_t thread;
{
	ipc_port_t port;

	ith_lock(thread);
	if (thread->ith_self != IP_NULL) {
		port = thread->ith_reply;
		if (IP_VALID(port))
			ipc_port_reference(port);
	} else
		port = IP_NULL;
	ith_unlock(thread);

	return port;
}

/*
 *	Routine:	task_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his task port.
 *		If new, the send right is marked with IE_BITS_COMPAT.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

port_name_t
task_self()
{
	task_t task = current_task();
	ipc_port_t sright;
	mach_port_t name;

	sright = retrieve_task_self_fast(task);
	name = ipc_port_copyout_send_compat(sright, task->itk_space);
	return (port_name_t) name;
}

/*
 *	Routine:	task_notify [mach trap]
 *	Purpose:
 *		Give the caller the name of his own notify port.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there isn't a notify port,
 *		if it is dead, or if the caller doesn't hold
 *		receive rights for it.
 */

port_name_t
task_notify()
{
	task_t task = current_task();
	ipc_port_t notify;
	mach_port_t name;

	notify = retrieve_task_notify(task);
	name = ipc_port_copyout_receiver(notify, task->itk_space);
	return (port_name_t) name;
}

/*
 *	Routine:	thread_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his own thread port.
 *		If new, the send right is marked with IE_BITS_COMPAT.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

port_name_t
thread_self()
{
	thread_t thread = current_thread();
	task_t task = thread->task;
	ipc_port_t sright;
	mach_port_t name;

	sright = retrieve_thread_self_fast(thread);
	name = ipc_port_copyout_send_compat(sright, task->itk_space);
	return (port_name_t) name;
}

/*
 *	Routine:	thread_reply [mach trap]
 *	Purpose:
 *		Give the caller the name of his own reply port.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there isn't a reply port,
 *		if it is dead, or if the caller doesn't hold
 *		receive rights for it.
 */

port_name_t
thread_reply()
{
	task_t task = current_task();
	thread_t thread = current_thread();
	ipc_port_t reply;
	mach_port_t name;

	reply = retrieve_thread_reply(thread);
	name = ipc_port_copyout_receiver(reply, task->itk_space);
	return (port_name_t) name;
}

#endif	MACH_IPC_COMPAT

/*
 *	Routine:	task_get_special_port [kernel call]
 *	Purpose:
 *		Clones a send right for one of the task's
 *		special ports.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Extracted a send right.
 *		KERN_INVALID_ARGUMENT	The task is null.
 *		KERN_FAILURE		The task/space is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
task_get_special_port(
	task_t		task,
	int		which,
	ipc_port_t	*portp)
{
	ipc_port_t *whichp;
	ipc_port_t port;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case TASK_NOTIFY_PORT: {
		ipc_space_t space = task->itk_space;

		is_read_lock(space);
		if (!space->is_active) {
			is_read_unlock(space);
			return KERN_FAILURE;
		}

		port = ipc_port_copy_send(space->is_notify);
		is_read_unlock(space);

		*portp = port;
		return KERN_SUCCESS;
	    }
#endif	MACH_IPC_COMPAT

	    case TASK_KERNEL_PORT:
		whichp = &task->itk_sself;
		break;

	    case TASK_EXCEPTION_PORT:
		whichp = &task->itk_exception;
		break;

	    case TASK_BOOTSTRAP_PORT:
		whichp = &task->itk_bootstrap;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	itk_lock(task);
	if (task->itk_self == IP_NULL) {
		itk_unlock(task);
		return KERN_FAILURE;
	}

	port = ipc_port_copy_send(*whichp);
	itk_unlock(task);

	*portp = port;
	return KERN_SUCCESS;
}

/*
 *	Routine:	task_set_special_port [kernel call]
 *	Purpose:
 *		Changes one of the task's special ports,
 *		setting it to the supplied send right.
 *	Conditions:
 *		Nothing locked.  If successful, consumes
 *		the supplied send right.
 *	Returns:
 *		KERN_SUCCESS		Changed the special port.
 *		KERN_INVALID_ARGUMENT	The task is null.
 *		KERN_FAILURE		The task/space is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
task_set_special_port(
	task_t		task,
	int		which,
	ipc_port_t	port)
{
	ipc_port_t *whichp;
	ipc_port_t old;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case TASK_NOTIFY_PORT: {
		ipc_space_t space = task->itk_space;

		is_write_lock(space);
		if (!space->is_active) {
			is_write_unlock(space);
			return KERN_FAILURE;
		}

		old = space->is_notify;
		space->is_notify = port;
		is_write_unlock(space);

		if (IP_VALID(old))
			ipc_port_release_send(old);
		return KERN_SUCCESS;
	    }
#endif	MACH_IPC_COMPAT

	    case TASK_KERNEL_PORT:
		whichp = &task->itk_sself;
		break;

	    case TASK_EXCEPTION_PORT:
		whichp = &task->itk_exception;
		break;

	    case TASK_BOOTSTRAP_PORT:
		whichp = &task->itk_bootstrap;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	itk_lock(task);
	if (task->itk_self == IP_NULL) {
		itk_unlock(task);
		return KERN_FAILURE;
	}

	old = *whichp;
	*whichp = port;
	itk_unlock(task);

	if (IP_VALID(old))
		ipc_port_release_send(old);
	return KERN_SUCCESS;
}

/*
 *	Routine:	thread_get_special_port [kernel call]
 *	Purpose:
 *		Clones a send right for one of the thread's
 *		special ports.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Extracted a send right.
 *		KERN_INVALID_ARGUMENT	The thread is null.
 *		KERN_FAILURE		The thread is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
thread_get_special_port(thread, which, portp)
	thread_t thread;
	int which;
	ipc_port_t *portp;
{
	ipc_port_t *whichp;
	ipc_port_t port;

	if (thread == ITH_NULL)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case THREAD_REPLY_PORT:
		whichp = &thread->ith_reply;
		break;
#endif	MACH_IPC_COMPAT

	    case THREAD_KERNEL_PORT:
		whichp = &thread->ith_sself;
		break;

	    case THREAD_EXCEPTION_PORT:
		whichp = &thread->ith_exception;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	ith_lock(thread);
	if (thread->ith_self == IP_NULL) {
		ith_unlock(thread);
		return KERN_FAILURE;
	}

	port = ipc_port_copy_send(*whichp);
	ith_unlock(thread);

	*portp = port;
	return KERN_SUCCESS;
}

/*
 *	Routine:	thread_set_special_port [kernel call]
 *	Purpose:
 *		Changes one of the thread's special ports,
 *		setting it to the supplied send right.
 *	Conditions:
 *		Nothing locked.  If successful, consumes
 *		the supplied send right.
 *	Returns:
 *		KERN_SUCCESS		Changed the special port.
 *		KERN_INVALID_ARGUMENT	The thread is null.
 *		KERN_FAILURE		The thread is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
thread_set_special_port(thread, which, port)
	thread_t thread;
	int which;
	ipc_port_t port;
{
	ipc_port_t *whichp;
	ipc_port_t old;

	if (thread == ITH_NULL)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case THREAD_REPLY_PORT:
		whichp = &thread->ith_reply;
		break;
#endif	MACH_IPC_COMPAT

	    case THREAD_KERNEL_PORT:
		whichp = &thread->ith_sself;
		break;

	    case THREAD_EXCEPTION_PORT:
		whichp = &thread->ith_exception;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	ith_lock(thread);
	if (thread->ith_self == IP_NULL) {
		ith_unlock(thread);
		return KERN_FAILURE;
	}

	old = *whichp;
	*whichp = port;
	ith_unlock(thread);

	if (IP_VALID(old))
		ipc_port_release_send(old);
	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_ports_register [kernel call]
 *	Purpose:
 *		Stash a handful of port send rights in the task.
 *		Child tasks will inherit these rights, but they
 *		must use mach_ports_lookup to acquire them.
 *
 *		The rights are supplied in a (wired) kalloc'd segment.
 *		Rights which aren't supplied are assumed to be null.
 *	Conditions:
 *		Nothing locked.  If successful, consumes
 *		the supplied rights and memory.
 *	Returns:
 *		KERN_SUCCESS		Stashed the port rights.
 *		KERN_INVALID_ARGUMENT	The task is null.
 *		KERN_INVALID_ARGUMENT	The task is dead.
 *		KERN_INVALID_ARGUMENT	Too many port rights supplied.
 */

kern_return_t
mach_ports_register(
	task_t			task,
	mach_port_array_t	memory,
	mach_msg_type_number_t	portsCnt)
{
	ipc_port_t ports[TASK_PORT_REGISTER_MAX];
	int i;

	if ((task == TASK_NULL) ||
	    (portsCnt > TASK_PORT_REGISTER_MAX))
		return KERN_INVALID_ARGUMENT;

	/*
	 *	Pad the port rights with nulls.
	 */

	for (i = 0; i < portsCnt; i++)
		ports[i] = memory[i];
	for (; i < TASK_PORT_REGISTER_MAX; i++)
		ports[i] = IP_NULL;

	itk_lock(task);
	if (task->itk_self == IP_NULL) {
		itk_unlock(task);
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 *	Replace the old send rights with the new.
	 *	Release the old rights after unlocking.
	 */

	for (i = 0; i < TASK_PORT_REGISTER_MAX; i++) {
		ipc_port_t old;

		old = task->itk_registered[i];
		task->itk_registered[i] = ports[i];
		ports[i] = old;
	}

	itk_unlock(task);

	for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
		if (IP_VALID(ports[i]))
			ipc_port_release_send(ports[i]);

	/*
	 *	Now that the operation is known to be successful,
	 *	we can free the memory.
	 */

	if (portsCnt != 0)
		kfree((vm_offset_t) memory,
		      (vm_size_t) (portsCnt * sizeof(mach_port_t)));

	return KERN_SUCCESS;
}

/*
 *	Routine:	mach_ports_lookup [kernel call]
 *	Purpose:
 *		Retrieves (clones) the stashed port send rights.
 *	Conditions:
 *		Nothing locked.  If successful, the caller gets
 *		rights and memory.
 *	Returns:
 *		KERN_SUCCESS		Retrieved the send rights.
 *		KERN_INVALID_ARGUMENT	The task is null.
 *		KERN_INVALID_ARGUMENT	The task is dead.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
mach_ports_lookup(task, portsp, portsCnt)
	task_t task;
	ipc_port_t **portsp;
	mach_msg_type_number_t *portsCnt;
{
	vm_offset_t memory;
	vm_size_t size;
	ipc_port_t *ports;
	int i;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	size = (vm_size_t) (TASK_PORT_REGISTER_MAX * sizeof(ipc_port_t));

	memory = kalloc(size);
	if (memory == 0)
		return KERN_RESOURCE_SHORTAGE;

	itk_lock(task);
	if (task->itk_self == IP_NULL) {
		itk_unlock(task);

		kfree(memory, size);
		return KERN_INVALID_ARGUMENT;
	}

	ports = (ipc_port_t *) memory;

	/*
	 *	Clone port rights.  Because kalloc'd memory
	 *	is wired, we won't fault while holding the task lock.
	 */

	for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
		ports[i] = ipc_port_copy_send(task->itk_registered[i]);

	itk_unlock(task);

	*portsp = (mach_port_array_t) ports;
	*portsCnt = TASK_PORT_REGISTER_MAX;
	return KERN_SUCCESS;
}

/*
 *	Routine:	convert_port_to_task
 *	Purpose:
 *		Convert from a port to a task.
 *		Doesn't consume the port ref; produces a task ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

task_t
convert_port_to_task(
	ipc_port_t	port)
{
	task_t task = TASK_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_TASK)) {
			task = (task_t) port->ip_kobject;
			task_reference(task);
		}
		ip_unlock(port);
	}

	return task;
}

/*
 *	Routine:	convert_port_to_space
 *	Purpose:
 *		Convert from a port to a space.
 *		Doesn't consume the port ref; produces a space ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

ipc_space_t
convert_port_to_space(
	ipc_port_t	port)
{
	ipc_space_t space = IS_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_TASK)) {
			space = ((task_t) port->ip_kobject)->itk_space;
			is_reference(space);
		}
		ip_unlock(port);
	}

	return space;
}

/*
 *	Routine:	convert_port_to_map
 *	Purpose:
 *		Convert from a port to a map.
 *		Doesn't consume the port ref; produces a map ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

vm_map_t
convert_port_to_map(port)
	ipc_port_t port;
{
	vm_map_t map = VM_MAP_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_TASK)) {
			map = ((task_t) port->ip_kobject)->map;
			vm_map_reference(map);
		}
		ip_unlock(port);
	}

	return map;
}

/*
 *	Routine:	convert_port_to_thread
 *	Purpose:
 *		Convert from a port to a thread.
 *		Doesn't consume the port ref; produces a thread ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

thread_t
convert_port_to_thread(port)
	ipc_port_t port;
{
	thread_t thread = THREAD_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_THREAD)) {
			thread = (thread_t) port->ip_kobject;
			thread_reference(thread);
		}
		ip_unlock(port);
	}

	return thread;
}

/*
 *	Routine:	convert_task_to_port
 *	Purpose:
 *		Convert from a task to a port.
 *		Consumes a task ref; produces a naked send right
 *		which may be invalid.  
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_task_to_port(task)
	task_t task;
{
	ipc_port_t port;

	itk_lock(task);
	if (task->itk_self != IP_NULL)
		port = ipc_port_make_send(task->itk_self);
	else
		port = IP_NULL;
	itk_unlock(task);

	task_deallocate(task);
	return port;
}

/*
 *	Routine:	convert_thread_to_port
 *	Purpose:
 *		Convert from a thread to a port.
 *		Consumes a thread ref; produces a naked send right
 *		which may be invalid.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_thread_to_port(thread)
	thread_t thread;
{
	ipc_port_t port;

	ith_lock(thread);
	if (thread->ith_self != IP_NULL)
		port = ipc_port_make_send(thread->ith_self);
	else
		port = IP_NULL;
	ith_unlock(thread);

	thread_deallocate(thread);
	return port;
}

/*
 *	Routine:	space_deallocate
 *	Purpose:
 *		Deallocate a space ref produced by convert_port_to_space.
 *	Conditions:
 *		Nothing locked.
 */

void
space_deallocate(space)
	ipc_space_t space;
{
	if (space != IS_NULL)
		is_release(space);
}
