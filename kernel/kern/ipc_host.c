/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University.
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
 *	kern/ipc_host.c
 *
 *	Routines to implement host ports.
 */

#include <mach/message.h>
#include <kern/host.h>
#include <kern/processor.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/ipc_host.h>
#include <kern/ipc_kobject.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <machine/machspl.h>	/* for spl */



/*
 *	ipc_host_init: set up various things.
 */

void ipc_host_init(void)
{
	ipc_port_t	port;
	/*
	 *	Allocate and set up the two host ports.
	 */
	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("ipc_host_init");

	ipc_kobject_set(port, (ipc_kobject_t) &realhost, IKOT_HOST);
	realhost.host_self = port;

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("ipc_host_init");

	ipc_kobject_set(port, (ipc_kobject_t) &realhost, IKOT_HOST_PRIV);
	realhost.host_priv_self = port;

	/*
	 *	Set up ipc for default processor set.
	 */
	ipc_pset_init(&default_pset);
	ipc_pset_enable(&default_pset);

	/*
	 *	And for master processor
	 */
	ipc_processor_init(master_processor);
}

/*
 *	Routine:	mach_host_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his own host port.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

mach_port_t
mach_host_self(void)
{
	ipc_port_t sright;

	sright = ipc_port_make_send(realhost.host_self);
	return ipc_port_copyout_send(sright, current_space());
}

#if	MACH_IPC_COMPAT

/*
 *	Routine:	host_self [mach trap]
 *	Purpose:
 *		Give the caller send rights for his own host port.
 *		If new, the send right is marked with IE_BITS_COMPAT.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_PORT_NULL if there are any resource failures
 *		or other errors.
 */

port_name_t
host_self(void)
{
	ipc_port_t sright;

	sright = ipc_port_make_send(realhost.host_self);
	return (port_name_t)
		ipc_port_copyout_send_compat(sright, current_space());
}

#endif	MACH_IPC_COMPAT

/*
 *	ipc_processor_init:
 *
 *	Initialize ipc access to processor by allocating port.
 *	Enable ipc control of processor by setting port object.
 */

void
ipc_processor_init(
	processor_t	processor)
{
	ipc_port_t	port;

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("ipc_processor_init");
	processor->processor_self = port;
	ipc_kobject_set(port, (ipc_kobject_t) processor, IKOT_PROCESSOR);
}


/*
 *	ipc_pset_init:
 *
 *	Initialize ipc control of a processor set by allocating its ports.
 */

void
ipc_pset_init(
	processor_set_t		pset)
{
	ipc_port_t	port;

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("ipc_pset_init");
	pset->pset_self = port;

	port = ipc_port_alloc_kernel();
	if (port == IP_NULL)
		panic("ipc_pset_init");
	pset->pset_name_self = port;
}

/*
 *	ipc_pset_enable:
 *
 *	Enable ipc access to a processor set.
 */
void
ipc_pset_enable(
	processor_set_t		pset)
{
	pset_lock(pset);
	if (pset->active) {
		ipc_kobject_set(pset->pset_self,
				(ipc_kobject_t) pset, IKOT_PSET);
		ipc_kobject_set(pset->pset_name_self,
				(ipc_kobject_t) pset, IKOT_PSET_NAME);
		pset_ref_lock(pset);
		pset->ref_count += 2;
		pset_ref_unlock(pset);
	}
	pset_unlock(pset);
}

/*
 *	ipc_pset_disable:
 *
 *	Disable ipc access to a processor set by clearing the port objects.
 *	Caller must hold pset lock and a reference to the pset.  Ok to
 *	just decrement pset reference count as a result.
 */
void
ipc_pset_disable(
	processor_set_t		pset)
{
	ipc_kobject_set(pset->pset_self, IKO_NULL, IKOT_NONE);
	ipc_kobject_set(pset->pset_name_self, IKO_NULL, IKOT_NONE);
	pset->ref_count -= 2;
}

/*
 *	ipc_pset_terminate:
 *
 *	Processor set is dead.  Deallocate the ipc control structures.
 */
void
ipc_pset_terminate(
	processor_set_t		pset)
{
	ipc_port_dealloc_kernel(pset->pset_self);
	ipc_port_dealloc_kernel(pset->pset_name_self);
}

/*
 *	processor_set_default, processor_set_default_priv:
 *
 *	Return ports for manipulating default_processor set.  MiG code
 *	differentiates between these two routines.
 */
kern_return_t
processor_set_default(
	host_t		host,
	processor_set_t	*pset)
{
	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	*pset = &default_pset;
	pset_reference(*pset);
	return KERN_SUCCESS;
}

kern_return_t
xxx_processor_set_default_priv(
	host_t		host,
	processor_set_t	*pset)
{
	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	*pset = &default_pset;
	pset_reference(*pset);
	return KERN_SUCCESS;
}

/*
 *	Routine:	convert_port_to_host
 *	Purpose:
 *		Convert from a port to a host.
 *		Doesn't consume the port ref; the host produced may be null.
 *	Conditions:
 *		Nothing locked.
 */

host_t
convert_port_to_host(
	ipc_port_t	port)
{
	host_t host = HOST_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    ((ip_kotype(port) == IKOT_HOST) ||
		     (ip_kotype(port) == IKOT_HOST_PRIV)))
			host = (host_t) port->ip_kobject;
		ip_unlock(port);
	}

	return host;
}

/*
 *	Routine:	convert_port_to_host_priv
 *	Purpose:
 *		Convert from a port to a host.
 *		Doesn't consume the port ref; the host produced may be null.
 *	Conditions:
 *		Nothing locked.
 */

host_t
convert_port_to_host_priv(
	ipc_port_t	port)
{
	host_t host = HOST_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_HOST_PRIV))
			host = (host_t) port->ip_kobject;
		ip_unlock(port);
	}

	return host;
}

/*
 *	Routine:	convert_port_to_processor
 *	Purpose:
 *		Convert from a port to a processor.
 *		Doesn't consume the port ref;
 *		the processor produced may be null.
 *	Conditions:
 *		Nothing locked.
 */

processor_t
convert_port_to_processor(
	ipc_port_t	port)
{
	processor_t processor = PROCESSOR_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_PROCESSOR))
			processor = (processor_t) port->ip_kobject;
		ip_unlock(port);
	}

	return processor;
}

/*
 *	Routine:	convert_port_to_pset
 *	Purpose:
 *		Convert from a port to a pset.
 *		Doesn't consume the port ref; produces a pset ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

processor_set_t
convert_port_to_pset(
	ipc_port_t	port)
{
	processor_set_t pset = PROCESSOR_SET_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    (ip_kotype(port) == IKOT_PSET)) {
			pset = (processor_set_t) port->ip_kobject;
			pset_reference(pset);
		}
		ip_unlock(port);
	}

	return pset;
}

/*
 *	Routine:	convert_port_to_pset_name
 *	Purpose:
 *		Convert from a port to a pset.
 *		Doesn't consume the port ref; produces a pset ref,
 *		which may be null.
 *	Conditions:
 *		Nothing locked.
 */

processor_set_t
convert_port_to_pset_name(
	ipc_port_t	port)
{
	processor_set_t pset = PROCESSOR_SET_NULL;

	if (IP_VALID(port)) {
		ip_lock(port);
		if (ip_active(port) &&
		    ((ip_kotype(port) == IKOT_PSET) ||
		     (ip_kotype(port) == IKOT_PSET_NAME))) {
			pset = (processor_set_t) port->ip_kobject;
			pset_reference(pset);
		}
		ip_unlock(port);
	}

	return pset;
}

/*
 *	Routine:	convert_host_to_port
 *	Purpose:
 *		Convert from a host to a port.
 *		Produces a naked send right which may be invalid.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_host_to_port(
	host_t		host)
{
	ipc_port_t port;

	port = ipc_port_make_send(host->host_self);

	return port;
}

/*
 *	Routine:	convert_processor_to_port
 *	Purpose:
 *		Convert from a processor to a port.
 *		Produces a naked send right which is always valid.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_processor_to_port(processor_t processor)
{
	ipc_port_t port;

	port = ipc_port_make_send(processor->processor_self);

	return port;
}

/*
 *	Routine:	convert_pset_to_port
 *	Purpose:
 *		Convert from a pset to a port.
 *		Consumes a pset ref; produces a naked send right
 *		which may be invalid.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_pset_to_port(
	processor_set_t		pset)
{
	ipc_port_t port;

	pset_lock(pset);
	if (pset->active)
		port = ipc_port_make_send(pset->pset_self);
	else
		port = IP_NULL;
	pset_unlock(pset);

	pset_deallocate(pset);
	return port;
}

/*
 *	Routine:	convert_pset_name_to_port
 *	Purpose:
 *		Convert from a pset to a port.
 *		Consumes a pset ref; produces a naked send right
 *		which may be invalid.
 *	Conditions:
 *		Nothing locked.
 */

ipc_port_t
convert_pset_name_to_port(
	processor_set_t		pset)
{
	ipc_port_t port;

	pset_lock(pset);
	if (pset->active)
		port = ipc_port_make_send(pset->pset_name_self);
	else
		port = IP_NULL;
	pset_unlock(pset);

	pset_deallocate(pset);
	return port;
}
