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
 *	File:	ipc/ipc_right.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to manipulate IPC capabilities.
 */

#include <mach_ipc_compat.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/message.h>
#include <kern/assert.h>
#include <ipc/port.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_hash.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_marequest.h>
#include <ipc/ipc_right.h>
#include <ipc/ipc_notify.h>



/*
 *	Routine:	ipc_right_lookup_write
 *	Purpose:
 *		Finds an entry in a space, given the name.
 *	Conditions:
 *		Nothing locked.  If successful, the space is write-locked.
 *	Returns:
 *		KERN_SUCCESS		Found an entry.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	Name doesn't exist in space.
 */

kern_return_t
ipc_right_lookup_write(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	*entryp)
{
	ipc_entry_t entry;

	assert(space != IS_NULL);

	is_write_lock(space);

	if (!space->is_active) {
		is_write_unlock(space);
		return KERN_INVALID_TASK;
	}

	if ((entry = ipc_entry_lookup(space, name)) == IE_NULL) {
		is_write_unlock(space);
		return KERN_INVALID_NAME;
	}

	*entryp = entry;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_right_reverse
 *	Purpose:
 *		Translate (space, object) -> (name, entry).
 *		Only finds send/receive rights.
 *		Returns TRUE if an entry is found; if so,
 *		the object is locked and active.
 *	Conditions:
 *		The space must be locked (read or write) and active.
 *		Nothing else locked.
 */

boolean_t
ipc_right_reverse(
	ipc_space_t	space,
	ipc_object_t	object,
	mach_port_t	*namep,
	ipc_entry_t	*entryp)
{
	ipc_port_t port;
	mach_port_t name;
	ipc_entry_t entry;

	/* would switch on io_otype to handle multiple types of object */

	assert(space->is_active);
	assert(io_otype(object) == IOT_PORT);

	port = (ipc_port_t) object;

	ip_lock(port);
	if (!ip_active(port)) {
		ip_unlock(port);

		return FALSE;
	}

	if (port->ip_receiver == space) {
		name = port->ip_receiver_name;
		assert(name != MACH_PORT_NULL);

		entry = ipc_entry_lookup(space, name);

		assert(entry != IE_NULL);
		assert(entry->ie_bits & MACH_PORT_TYPE_RECEIVE);
		assert(port == (ipc_port_t) entry->ie_object);

		*namep = name;
		*entryp = entry;
		return TRUE;
	}

	if (ipc_hash_lookup(space, (ipc_object_t) port, namep, entryp)) {
		assert((entry = *entryp) != IE_NULL);
		assert(IE_BITS_TYPE(entry->ie_bits) == MACH_PORT_TYPE_SEND);
		assert(port == (ipc_port_t) entry->ie_object);

		return TRUE;
	}

	ip_unlock(port);
	return FALSE;
}

/*
 *	Routine:	ipc_right_dnrequest
 *	Purpose:
 *		Make a dead-name request, returning the previously
 *		registered send-once right.  If notify is IP_NULL,
 *		just cancels the previously registered request.
 *
 *		This interacts with the IE_BITS_COMPAT, because they
 *		both use ie_request.  If this is a compat entry, then
 *		previous always gets IP_NULL.  If notify is IP_NULL,
 *		then the entry remains a compat entry.  Otherwise
 *		the real dead-name request is registered and the entry
 *		is no longer a compat entry.
 *	Conditions:
 *		Nothing locked.  May allocate memory.
 *		Only consumes/returns refs if successful.
 *	Returns:
 *		KERN_SUCCESS		Made/canceled dead-name request.
 *		KERN_INVALID_TASK	The space is dead.
 *		KERN_INVALID_NAME	Name doesn't exist in space.
 *		KERN_INVALID_RIGHT	Name doesn't denote port/dead rights.
 *		KERN_INVALID_ARGUMENT	Name denotes dead name, but
 *			immediate is FALSE or notify is IP_NULL.
 *		KERN_UREFS_OVERFLOW	Name denotes dead name, but
 *			generating immediate notif. would overflow urefs.
 *		KERN_RESOURCE_SHORTAGE	Couldn't allocate memory.
 */

kern_return_t
ipc_right_dnrequest(
	ipc_space_t	space,
	mach_port_t	name,
	boolean_t	immediate,
	ipc_port_t	notify,
	ipc_port_t	*previousp)
{
	ipc_port_t previous;

	for (;;) {
		ipc_entry_t entry;
		ipc_entry_bits_t bits;
		kern_return_t kr;

		kr = ipc_right_lookup_write(space, name, &entry);
		if (kr != KERN_SUCCESS)
			return kr;
		/* space is write-locked and active */

		bits = entry->ie_bits;
		if (bits & MACH_PORT_TYPE_PORT_RIGHTS) {
			ipc_port_t port;
			ipc_port_request_index_t request;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			if (!ipc_right_check(space, port, name, entry)) {
				/* port is locked and active */

				if (notify == IP_NULL) {
#if	MACH_IPC_COMPAT
					if (bits & IE_BITS_COMPAT) {
						assert(entry->ie_request != 0);

						previous = IP_NULL;
					} else
#endif	MACH_IPC_COMPAT
					previous = ipc_right_dncancel_macro(
						space, port, name, entry);

					ip_unlock(port);
					is_write_unlock(space);
					break;
				}

				/*
				 *	If a registered soright exists,
				 *	want to atomically switch with it.
				 *	If ipc_port_dncancel finds us a
				 *	soright, then the following
				 *	ipc_port_dnrequest will reuse
				 *	that slot, so we are guaranteed
				 *	not to unlock and retry.
				 */

				previous = ipc_right_dncancel_macro(space,
							port, name, entry);

				kr = ipc_port_dnrequest(port, name, notify,
							&request);
				if (kr != KERN_SUCCESS) {
					assert(previous == IP_NULL);
					is_write_unlock(space);

					kr = ipc_port_dngrow(port);
					/* port is unlocked */
					if (kr != KERN_SUCCESS)
						return kr;

					continue;
				}

				assert(request != 0);
				ip_unlock(port);

				entry->ie_request = request;
#if	MACH_IPC_COMPAT
				entry->ie_bits = bits &~ IE_BITS_COMPAT;
#endif	MACH_IPC_COMPAT
				is_write_unlock(space);
				break;
			}

#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT) {
				is_write_unlock(space);
				return KERN_INVALID_NAME;
			}
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			assert(bits & MACH_PORT_TYPE_DEAD_NAME);
		}

		if ((bits & MACH_PORT_TYPE_DEAD_NAME) &&
		    immediate && (notify != IP_NULL)) {
			mach_port_urefs_t urefs = IE_BITS_UREFS(bits);

			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
			assert(urefs > 0);

			if (MACH_PORT_UREFS_OVERFLOW(urefs, 1)) {
				is_write_unlock(space);
				return KERN_UREFS_OVERFLOW;
			}

			entry->ie_bits = bits + 1; /* increment urefs */
			is_write_unlock(space);

			ipc_notify_dead_name(notify, name);
			previous = IP_NULL;
			break;
		}

		is_write_unlock(space);
		if (bits & MACH_PORT_TYPE_PORT_OR_DEAD)
			return KERN_INVALID_ARGUMENT;
		else
			return KERN_INVALID_RIGHT;
	}

	*previousp = previous;
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_right_dncancel
 *	Purpose:
 *		Cancel a dead-name request and return the send-once right.
 *		Afterwards, entry->ie_request == 0.
 *	Conditions:
 *		The space must be write-locked; the port must be locked.
 *		The port must be active; the space doesn't have to be.
 */

ipc_port_t
ipc_right_dncancel(
	ipc_space_t	space,
	ipc_port_t	port,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	ipc_port_t dnrequest;

	assert(ip_active(port));
	assert(port == (ipc_port_t) entry->ie_object);

	dnrequest = ipc_port_dncancel(port, name, entry->ie_request);
	entry->ie_request = 0;

#if	MACH_IPC_COMPAT
	assert(!ipr_spacep(dnrequest) == !(entry->ie_bits & IE_BITS_COMPAT));

	/* if this is actually a space ptr, just release the ref */

	if (entry->ie_bits & IE_BITS_COMPAT) {
		assert(space == ipr_space(dnrequest));

		is_release(space);
		dnrequest = IP_NULL;
	}
#endif	MACH_IPC_COMPAT

	return dnrequest;
}

/*
 *	Routine:	ipc_right_inuse
 *	Purpose:
 *		Check if an entry is being used.
 *		Returns TRUE if it is.
 *	Conditions:
 *		The space is write-locked and active.
 *		It is unlocked if the entry is inuse.
 */

boolean_t
ipc_right_inuse(space, name, entry)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
{
	ipc_entry_bits_t bits = entry->ie_bits;

	if (IE_BITS_TYPE(bits) != MACH_PORT_TYPE_NONE) {
#if	MACH_IPC_COMPAT
		mach_port_type_t type = IE_BITS_TYPE(bits);

		/*
		 *	There is yet hope.  If the port has died, we
		 *	must clean up the entry so it's as good as new.
		 */

		if ((bits & IE_BITS_COMPAT) &&
		    ((type == MACH_PORT_TYPE_SEND) ||
		     (type == MACH_PORT_TYPE_SEND_ONCE))) {
			ipc_port_t port;
			boolean_t active;

			assert(IE_BITS_UREFS(bits) > 0);
			assert(entry->ie_request != 0);

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			ip_lock(port);
			active = ip_active(port);
			ip_unlock(port);

			if (!active) {
				if (type == MACH_PORT_TYPE_SEND) {
					/* clean up msg-accepted request */

					if (bits & IE_BITS_MAREQUEST)
						ipc_marequest_cancel(
								space, name);

					ipc_hash_delete(
						space, (ipc_object_t) port,
						name, entry);
				} else {
					assert(IE_BITS_UREFS(bits) == 1);
					assert(!(bits & IE_BITS_MAREQUEST));
				}

				ipc_port_release(port);

				entry->ie_request = 0;
				entry->ie_object = IO_NULL;
				entry->ie_bits &= ~IE_BITS_RIGHT_MASK;

				return FALSE;
			}
		}
#endif	MACH_IPC_COMPAT

		is_write_unlock(space);
		return TRUE;
	}

	return FALSE;
}

/*
 *	Routine:	ipc_right_check
 *	Purpose:
 *		Check if the port has died.  If it has,
 *		clean up the entry and return TRUE.
 *	Conditions:
 *		The space is write-locked; the port is not locked.
 *		If returns FALSE, the port is also locked and active.
 *		Otherwise, entry is converted to a dead name, freeing
 *		a reference to port.
 *
 *		[MACH_IPC_COMPAT] If the port is dead, and this is a
 *		compat mode entry, then the port reference is released
 *		and the entry is destroyed.  The call returns TRUE,
 *		and the space is left locked.
 */

boolean_t
ipc_right_check(space, port, name, entry)
	ipc_space_t space;
	ipc_port_t port;
	mach_port_t name;
	ipc_entry_t entry;
{
	ipc_entry_bits_t bits;

	assert(space->is_active);
	assert(port == (ipc_port_t) entry->ie_object);

	ip_lock(port);
	if (ip_active(port))
		return FALSE;
	ip_unlock(port);

	/* this was either a pure send right or a send-once right */

	bits = entry->ie_bits;
	assert((bits & MACH_PORT_TYPE_RECEIVE) == 0);
	assert(IE_BITS_UREFS(bits) > 0);

	if (bits & MACH_PORT_TYPE_SEND) {
		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND);

		/* clean up msg-accepted request */

		if (bits & IE_BITS_MAREQUEST) {
			bits &= ~IE_BITS_MAREQUEST;

			ipc_marequest_cancel(space, name);
		}

		ipc_hash_delete(space, (ipc_object_t) port, name, entry);
	} else {
		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND_ONCE);
		assert(IE_BITS_UREFS(bits) == 1);
		assert((bits & IE_BITS_MAREQUEST) == 0);
	}

	ipc_port_release(port);

#if	MACH_IPC_COMPAT
	if (bits & IE_BITS_COMPAT) {
		assert(entry->ie_request != 0);
		entry->ie_request = 0;

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);

		return TRUE;
	}
#endif	MACH_IPC_COMPAT

	/* convert entry to dead name */

	bits = (bits &~ IE_BITS_TYPE_MASK) | MACH_PORT_TYPE_DEAD_NAME;

	if (entry->ie_request != 0) {
		assert(IE_BITS_UREFS(bits) < MACH_PORT_UREFS_MAX);

		entry->ie_request = 0;
		bits++;		/* increment urefs */
	}

	entry->ie_bits = bits;
	entry->ie_object = IO_NULL;

	return TRUE;
}

/*
 *	Routine:	ipc_right_clean
 *	Purpose:
 *		Cleans up an entry in a dead space.
 *		The entry isn't deallocated or removed
 *		from reverse hash tables.
 *	Conditions:
 *		The space is dead and unlocked.
 */

void
ipc_right_clean(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	mach_port_type_t type = IE_BITS_TYPE(bits);

	assert(!space->is_active);

	/*
	 *	We can't clean up IE_BITS_MAREQUEST when the space is dead.
	 *	This is because ipc_marequest_destroy can't turn off
	 *	the bit if the space is dead.  Hence, it might be on
	 *	even though the marequest has been destroyed.  It's OK
	 *	not to cancel the marequest, because ipc_marequest_destroy
	 *	cancels for us if the space is dead.
	 *
	 *	IE_BITS_COMPAT/ipc_right_dncancel doesn't have this
	 *	problem, because we check that the port is active.  If
	 *	we didn't cancel IE_BITS_COMPAT, ipc_port_destroy
	 *	would still work, but dead space refs would accumulate
	 *	in ip_dnrequests.  They would use up slots in
	 *	ip_dnrequests and keep the spaces from being freed.
	 */

	switch (type) {
	    case MACH_PORT_TYPE_DEAD_NAME:
		assert(entry->ie_request == 0);
		assert(entry->ie_object == IO_NULL);
		assert((bits & IE_BITS_MAREQUEST) == 0);
		break;

	    case MACH_PORT_TYPE_PORT_SET: {
		ipc_pset_t pset = (ipc_pset_t) entry->ie_object;

		assert(entry->ie_request == 0);
		assert((bits & IE_BITS_MAREQUEST) == 0);
		assert(pset != IPS_NULL);

		ips_lock(pset);
		assert(ips_active(pset));

		ipc_pset_destroy(pset); /* consumes ref, unlocks */
		break;
	    }

	    case MACH_PORT_TYPE_SEND:
	    case MACH_PORT_TYPE_RECEIVE:
	    case MACH_PORT_TYPE_SEND_RECEIVE:
	    case MACH_PORT_TYPE_SEND_ONCE: {
		ipc_port_t port = (ipc_port_t) entry->ie_object;
		ipc_port_t dnrequest;
		ipc_port_t nsrequest = IP_NULL;
		mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */

		assert(port != IP_NULL);
		ip_lock(port);

		if (!ip_active(port)) {
			ip_release(port);
			ip_check_unlock(port);
			break;
		}

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);

		if (type & MACH_PORT_TYPE_SEND) {
			assert(port->ip_srights > 0);
			if (--port->ip_srights == 0) {
				nsrequest = port->ip_nsrequest;
				if (nsrequest != IP_NULL) {
					port->ip_nsrequest = IP_NULL;
					mscount = port->ip_mscount;
				}
			}
		}

		if (type & MACH_PORT_TYPE_RECEIVE) {
			assert(port->ip_receiver_name == name);
			assert(port->ip_receiver == space);

			ipc_port_clear_receiver(port);
			ipc_port_destroy(port); /* consumes our ref, unlocks */
		} else if (type & MACH_PORT_TYPE_SEND_ONCE) {
			assert(port->ip_sorights > 0);
			ip_unlock(port);

			ipc_notify_send_once(port); /* consumes our ref */
		} else {
			assert(port->ip_receiver != space);

			ip_release(port);
			ip_unlock(port); /* port is active */
		}

		if (nsrequest != IP_NULL)
			ipc_notify_no_senders(nsrequest, mscount);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_clean: strange type");
#else
		panic("ipc_right_clean: strange type");
#endif
	}
}

/*
 *	Routine:	ipc_right_destroy
 *	Purpose:
 *		Destroys an entry in a space.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return.
 *		The space must be active.
 *	Returns:
 *		KERN_SUCCESS		The entry was destroyed.
 */

kern_return_t
ipc_right_destroy(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	entry)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	mach_port_type_t type = IE_BITS_TYPE(bits);

	assert(space->is_active);

	switch (type) {
	    case MACH_PORT_TYPE_DEAD_NAME:
		assert(entry->ie_request == 0);
		assert(entry->ie_object == IO_NULL);
		assert((bits & IE_BITS_MAREQUEST) == 0);

		ipc_entry_dealloc(space, name, entry);
		is_write_unlock(space);
		break;

	    case MACH_PORT_TYPE_PORT_SET: {
		ipc_pset_t pset = (ipc_pset_t) entry->ie_object;

		assert(entry->ie_request == 0);
		assert(pset != IPS_NULL);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);

		ips_lock(pset);
		assert(ips_active(pset));
		is_write_unlock(space);

		ipc_pset_destroy(pset); /* consumes ref, unlocks */
		break;
	    }

	    case MACH_PORT_TYPE_SEND:
	    case MACH_PORT_TYPE_RECEIVE:
	    case MACH_PORT_TYPE_SEND_RECEIVE:
	    case MACH_PORT_TYPE_SEND_ONCE: {
		ipc_port_t port = (ipc_port_t) entry->ie_object;
		ipc_port_t nsrequest = IP_NULL;
		mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */
		ipc_port_t dnrequest;

		assert(port != IP_NULL);

		if (bits & IE_BITS_MAREQUEST) {
			assert(type & MACH_PORT_TYPE_SEND_RECEIVE);

			ipc_marequest_cancel(space, name);
		}

		if (type == MACH_PORT_TYPE_SEND)
			ipc_hash_delete(space, (ipc_object_t) port,
					name, entry);

		ip_lock(port);

		if (!ip_active(port)) {
			assert((type & MACH_PORT_TYPE_RECEIVE) == 0);

			ip_release(port);
			ip_check_unlock(port);

			entry->ie_request = 0;
			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
			is_write_unlock(space);

#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				return KERN_INVALID_NAME;
#endif	MACH_IPC_COMPAT
			break;
		}

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);
		is_write_unlock(space);

		if (type & MACH_PORT_TYPE_SEND) {
			assert(port->ip_srights > 0);
			if (--port->ip_srights == 0) {
				nsrequest = port->ip_nsrequest;
				if (nsrequest != IP_NULL) {
					port->ip_nsrequest = IP_NULL;
					mscount = port->ip_mscount;
				}
			}
		}

		if (type & MACH_PORT_TYPE_RECEIVE) {
			assert(ip_active(port));
			assert(port->ip_receiver == space);

			ipc_port_clear_receiver(port);
			ipc_port_destroy(port); /* consumes our ref, unlocks */
		} else if (type & MACH_PORT_TYPE_SEND_ONCE) {
			assert(port->ip_sorights > 0);
			ip_unlock(port);

			ipc_notify_send_once(port); /* consumes our ref */
		} else {
			assert(port->ip_receiver != space);

			ip_release(port);
			ip_unlock(port);
		}

		if (nsrequest != IP_NULL)
			ipc_notify_no_senders(nsrequest, mscount);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_destroy: strange type");
#else
		panic("ipc_right_destroy: strange type");
#endif
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_right_dealloc
 *	Purpose:
 *		Releases a send/send-once/dead-name user ref.
 *		Like ipc_right_delta with a delta of -1,
 *		but looks at the entry to determine the right.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return.
 *		The space must be active.
 *	Returns:
 *		KERN_SUCCESS		A user ref was released.
 *		KERN_INVALID_RIGHT	Entry has wrong type.
 *		KERN_INVALID_NAME	[MACH_IPC_COMPAT]
 *			Caller should pretend lookup of entry failed.
 */

kern_return_t
ipc_right_dealloc(space, name, entry)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
{
	ipc_entry_bits_t bits = entry->ie_bits;
	mach_port_type_t type = IE_BITS_TYPE(bits);

	assert(space->is_active);

	switch (type) {
	    case MACH_PORT_TYPE_DEAD_NAME: {
	    dead_name:

		assert(IE_BITS_UREFS(bits) > 0);
		assert(entry->ie_request == 0);
		assert(entry->ie_object == IO_NULL);
		assert((bits & IE_BITS_MAREQUEST) == 0);

		if (IE_BITS_UREFS(bits) == 1)
			ipc_entry_dealloc(space, name, entry);
		else
			entry->ie_bits = bits-1; /* decrement urefs */

		is_write_unlock(space);
		break;
	    }

	    case MACH_PORT_TYPE_SEND_ONCE: {
		ipc_port_t port, dnrequest;

		assert(IE_BITS_UREFS(bits) == 1);
		assert((bits & IE_BITS_MAREQUEST) == 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
			goto dead_name;
		}
		/* port is locked and active */

		assert(port->ip_sorights > 0);

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);
		ip_unlock(port);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);
		is_write_unlock(space);

		ipc_notify_send_once(port);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    case MACH_PORT_TYPE_SEND: {
		ipc_port_t port;
		ipc_port_t dnrequest = IP_NULL;
		ipc_port_t nsrequest = IP_NULL;
		mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
			goto dead_name;
		}
		/* port is locked and active */

		assert(port->ip_srights > 0);

		if (IE_BITS_UREFS(bits) == 1) {
			if (--port->ip_srights == 0) {
				nsrequest = port->ip_nsrequest;
				if (nsrequest != IP_NULL) {
					port->ip_nsrequest = IP_NULL;
					mscount = port->ip_mscount;
				}
			}

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			ipc_hash_delete(space, (ipc_object_t) port,
					name, entry);

			if (bits & IE_BITS_MAREQUEST)
				ipc_marequest_cancel(space, name);

			ip_release(port);
			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
		} else
			entry->ie_bits = bits-1; /* decrement urefs */

		ip_unlock(port); /* even if dropped a ref, port is active */
		is_write_unlock(space);

		if (nsrequest != IP_NULL)
			ipc_notify_no_senders(nsrequest, mscount);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    case MACH_PORT_TYPE_SEND_RECEIVE: {
		ipc_port_t port;
		ipc_port_t nsrequest = IP_NULL;
		mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);
		assert(port->ip_srights > 0);

		if (IE_BITS_UREFS(bits) == 1) {
			if (--port->ip_srights == 0) {
				nsrequest = port->ip_nsrequest;
				if (nsrequest != IP_NULL) {
					port->ip_nsrequest = IP_NULL;
					mscount = port->ip_mscount;
				}
			}

			entry->ie_bits = bits &~ (IE_BITS_UREFS_MASK|
						  MACH_PORT_TYPE_SEND);
		} else
			entry->ie_bits = bits-1; /* decrement urefs */

		ip_unlock(port);
		is_write_unlock(space);

		if (nsrequest != IP_NULL)
			ipc_notify_no_senders(nsrequest, mscount);
		break;
	    }

	    default:
		is_write_unlock(space);
		return KERN_INVALID_RIGHT;
	}

	return KERN_SUCCESS;

#if	MACH_IPC_COMPAT
    invalid_name:
	is_write_unlock(space);
	return KERN_INVALID_NAME;
#endif	MACH_IPC_COMPAT
}

/*
 *	Routine:	ipc_right_delta
 *	Purpose:
 *		Modifies the user-reference count for a right.
 *		May deallocate the right, if the count goes to zero.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return.
 *		The space must be active.
 *	Returns:
 *		KERN_SUCCESS		Count was modified.
 *		KERN_INVALID_RIGHT	Entry has wrong type.
 *		KERN_INVALID_VALUE	Bad delta for the right.
 *		KERN_UREFS_OVERFLOW	OK delta, except would overflow.
 *		KERN_INVALID_NAME	[MACH_IPC_COMPAT]
 *			Caller should pretend lookup of entry failed.
 */

kern_return_t
ipc_right_delta(space, name, entry, right, delta)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
	mach_port_right_t right;
	mach_port_delta_t delta;
{
	ipc_entry_bits_t bits = entry->ie_bits;

	assert(space->is_active);
	assert(right < MACH_PORT_RIGHT_NUMBER);

	/* Rights-specific restrictions and operations. */

	switch (right) {
	    case MACH_PORT_RIGHT_PORT_SET: {
		ipc_pset_t pset;

		if ((bits & MACH_PORT_TYPE_PORT_SET) == 0)
			goto invalid_right;

		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_PORT_SET);
		assert(IE_BITS_UREFS(bits) == 0);
		assert((bits & IE_BITS_MAREQUEST) == 0);
		assert(entry->ie_request == 0);

		if (delta == 0)
			goto success;

		if (delta != -1)
			goto invalid_value;

		pset = (ipc_pset_t) entry->ie_object;
		assert(pset != IPS_NULL);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);

		ips_lock(pset);
		assert(ips_active(pset));
		is_write_unlock(space);

		ipc_pset_destroy(pset); /* consumes ref, unlocks */
		break;
	    }

	    case MACH_PORT_RIGHT_RECEIVE: {
		ipc_port_t port;
		ipc_port_t dnrequest = IP_NULL;

		if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
			goto invalid_right;

		if (delta == 0)
			goto success;

		if (delta != -1)
			goto invalid_value;

		if (bits & IE_BITS_MAREQUEST) {
			bits &= ~IE_BITS_MAREQUEST;

			ipc_marequest_cancel(space, name);
		}

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		/*
		 *	The port lock is needed for ipc_right_dncancel;
		 *	otherwise, we wouldn't have to take the lock
		 *	until just before dropping the space lock.
		 */

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);

#if	MACH_IPC_COMPAT
		if (bits & IE_BITS_COMPAT) {
			assert(entry->ie_request != 0);
			dnrequest = ipc_right_dncancel(space, port,
						       name, entry);
			assert(dnrequest == IP_NULL);

			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
		} else
#endif	MACH_IPC_COMPAT
		if (bits & MACH_PORT_TYPE_SEND) {
			assert(IE_BITS_TYPE(bits) ==
					MACH_PORT_TYPE_SEND_RECEIVE);
			assert(IE_BITS_UREFS(bits) > 0);
			assert(IE_BITS_UREFS(bits) < MACH_PORT_UREFS_MAX);
			assert(port->ip_srights > 0);

			/*
			 *	The remaining send right turns into a
			 *	dead name.  Notice we don't decrement
			 *	ip_srights, generate a no-senders notif,
			 *	or use ipc_right_dncancel, because the
			 *	port is destroyed "first".
			 */

			bits &= ~IE_BITS_TYPE_MASK;
			bits |= MACH_PORT_TYPE_DEAD_NAME;

			if (entry->ie_request != 0) {
				entry->ie_request = 0;
				bits++;	/* increment urefs */
			}

			entry->ie_bits = bits;
			entry->ie_object = IO_NULL;
		} else {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_RECEIVE);
			assert(IE_BITS_UREFS(bits) == 0);

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
		}
		is_write_unlock(space);

		ipc_port_clear_receiver(port);
		ipc_port_destroy(port);	/* consumes ref, unlocks */

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    case MACH_PORT_RIGHT_SEND_ONCE: {
		ipc_port_t port, dnrequest;

		if ((bits & MACH_PORT_TYPE_SEND_ONCE) == 0)
			goto invalid_right;

		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND_ONCE);
		assert(IE_BITS_UREFS(bits) == 1);
		assert((bits & IE_BITS_MAREQUEST) == 0);

		if ((delta > 0) || (delta < -1))
			goto invalid_value;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			assert(!(entry->ie_bits & MACH_PORT_TYPE_SEND_ONCE));
			goto invalid_right;
		}
		/* port is locked and active */

		assert(port->ip_sorights > 0);

		if (delta == 0) {
			ip_unlock(port);
			goto success;
		}

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);
		ip_unlock(port);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);
		is_write_unlock(space);

		ipc_notify_send_once(port);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    case MACH_PORT_RIGHT_DEAD_NAME: {
		mach_port_urefs_t urefs;

		if (bits & MACH_PORT_TYPE_SEND_RIGHTS) {
			ipc_port_t port;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			if (!ipc_right_check(space, port, name, entry)) {
				/* port is locked and active */
				ip_unlock(port);
				goto invalid_right;
			}

#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
		} else if ((bits & MACH_PORT_TYPE_DEAD_NAME) == 0)
			goto invalid_right;

		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
		assert(IE_BITS_UREFS(bits) > 0);
		assert((bits & IE_BITS_MAREQUEST) == 0);
		assert(entry->ie_object == IO_NULL);
		assert(entry->ie_request == 0);

		urefs = IE_BITS_UREFS(bits);
		if (MACH_PORT_UREFS_UNDERFLOW(urefs, delta))
			goto invalid_value;
		if (MACH_PORT_UREFS_OVERFLOW(urefs, delta))
			goto urefs_overflow;

		if ((urefs + delta) == 0)
			ipc_entry_dealloc(space, name, entry);
		else
			entry->ie_bits = bits + delta;

		is_write_unlock(space);
		break;
	    }

	    case MACH_PORT_RIGHT_SEND: {
		mach_port_urefs_t urefs;
		ipc_port_t port;
		ipc_port_t dnrequest = IP_NULL;
		ipc_port_t nsrequest = IP_NULL;
		mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */

		if ((bits & MACH_PORT_TYPE_SEND) == 0)
			goto invalid_right;

		/* maximum urefs for send is MACH_PORT_UREFS_MAX-1 */

		urefs = IE_BITS_UREFS(bits);
		if (MACH_PORT_UREFS_UNDERFLOW(urefs, delta))
			goto invalid_value;
		if (MACH_PORT_UREFS_OVERFLOW(urefs+1, delta))
			goto urefs_overflow;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			assert((entry->ie_bits & MACH_PORT_TYPE_SEND) == 0);
			goto invalid_right;
		}
		/* port is locked and active */

		assert(port->ip_srights > 0);

		if ((urefs + delta) == 0) {
			if (--port->ip_srights == 0) {
				nsrequest = port->ip_nsrequest;
				if (nsrequest != IP_NULL) {
					port->ip_nsrequest = IP_NULL;
					mscount = port->ip_mscount;
				}
			}

			if (bits & MACH_PORT_TYPE_RECEIVE) {
				assert(port->ip_receiver_name == name);
				assert(port->ip_receiver == space);
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_SEND_RECEIVE);

				entry->ie_bits = bits &~ (IE_BITS_UREFS_MASK|
							  MACH_PORT_TYPE_SEND);
			} else {
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_SEND);

				dnrequest = ipc_right_dncancel_macro(
						space, port, name, entry);

				ipc_hash_delete(space, (ipc_object_t) port,
						name, entry);

				if (bits & IE_BITS_MAREQUEST)
					ipc_marequest_cancel(space, name);

				ip_release(port);
				entry->ie_object = IO_NULL;
				ipc_entry_dealloc(space, name, entry);
			}
		} else
			entry->ie_bits = bits + delta;

		ip_unlock(port); /* even if dropped a ref, port is active */
		is_write_unlock(space);

		if (nsrequest != IP_NULL)
			ipc_notify_no_senders(nsrequest, mscount);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_delta: strange right");
#else
		panic("ipc_right_delta: strange right");
#endif
	}

	return KERN_SUCCESS;

    success:
	is_write_unlock(space);
	return KERN_SUCCESS;

    invalid_right:
	is_write_unlock(space);
	return KERN_INVALID_RIGHT;

    invalid_value:
	is_write_unlock(space);
	return KERN_INVALID_VALUE;

    urefs_overflow:
	is_write_unlock(space);
	return KERN_UREFS_OVERFLOW;

#if	MACH_IPC_COMPAT
    invalid_name:
	is_write_unlock(space);
	return KERN_INVALID_NAME;
#endif	MACH_IPC_COMPAT
}

/*
 *	Routine:	ipc_right_info
 *	Purpose:
 *		Retrieves information about the right.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return
 *		if the call is unsuccessful.  The space must be active.
 *	Returns:
 *		KERN_SUCCESS		Retrieved info; space still locked.
 */

kern_return_t
ipc_right_info(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_entry_t		entry,
	mach_port_type_t	*typep,
	mach_port_urefs_t	*urefsp)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	ipc_port_request_index_t request;
	mach_port_type_t type;

	if (bits & MACH_PORT_TYPE_SEND_RIGHTS) {
		ipc_port_t port = (ipc_port_t) entry->ie_object;

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT) {
				is_write_unlock(space);
				return KERN_INVALID_NAME;
			}
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
		} else
			ip_unlock(port);
	}

	type = IE_BITS_TYPE(bits);
	request = entry->ie_request;

#if	MACH_IPC_COMPAT
	if (bits & IE_BITS_COMPAT)
		type |= MACH_PORT_TYPE_COMPAT;
	else
#endif	MACH_IPC_COMPAT
	if (request != 0)
		type |= MACH_PORT_TYPE_DNREQUEST;
	if (bits & IE_BITS_MAREQUEST)
		type |= MACH_PORT_TYPE_MAREQUEST;

	*typep = type;
	*urefsp = IE_BITS_UREFS(bits);
	return KERN_SUCCESS;
}

/*
 *	Routine:	ipc_right_copyin_check
 *	Purpose:
 *		Check if a subsequent ipc_right_copyin would succeed.
 *	Conditions:
 *		The space is locked (read or write) and active.
 */

boolean_t
ipc_right_copyin_check(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_entry_t		entry,
	mach_msg_type_name_t	msgt_name)
{
	ipc_entry_bits_t bits = entry->ie_bits;

	assert(space->is_active);

	switch (msgt_name) {
	    case MACH_MSG_TYPE_MAKE_SEND:
	    case MACH_MSG_TYPE_MAKE_SEND_ONCE:
	    case MACH_MSG_TYPE_MOVE_RECEIVE:
		if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
			return FALSE;

		break;

	    case MACH_MSG_TYPE_COPY_SEND:
	    case MACH_MSG_TYPE_MOVE_SEND:
	    case MACH_MSG_TYPE_MOVE_SEND_ONCE: {
		ipc_port_t port;
		boolean_t active;

		if (bits & MACH_PORT_TYPE_DEAD_NAME)
			break;

		if ((bits & MACH_PORT_TYPE_SEND_RIGHTS) == 0)
			return FALSE;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		active = ip_active(port);
		ip_unlock(port);

		if (!active) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				return FALSE;
#endif	MACH_IPC_COMPAT

			break;
		}

		if (msgt_name == MACH_MSG_TYPE_MOVE_SEND_ONCE) {
			if ((bits & MACH_PORT_TYPE_SEND_ONCE) == 0)
				return FALSE;
		} else {
			if ((bits & MACH_PORT_TYPE_SEND) == 0)
				return FALSE;
		}

		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_copyin_check: strange rights");
#else
		panic("ipc_right_copyin_check: strange rights");
#endif
	}

	return TRUE;
}

/*
 *	Routine:	ipc_right_copyin
 *	Purpose:
 *		Copyin a capability from a space.
 *		If successful, the caller gets a ref
 *		for the resulting object, unless it is IO_DEAD,
 *		and possibly a send-once right which should
 *		be used in a port-deleted notification.
 *
 *		If deadok is not TRUE, the copyin operation
 *		will fail instead of producing IO_DEAD.
 *
 *		The entry is never deallocated (except
 *		when KERN_INVALID_NAME), so the caller
 *		should deallocate the entry if its type
 *		is MACH_PORT_TYPE_NONE.
 *	Conditions:
 *		The space is write-locked and active.
 *	Returns:
 *		KERN_SUCCESS		Acquired an object, possibly IO_DEAD.
 *		KERN_INVALID_RIGHT	Name doesn't denote correct right.
 */

kern_return_t
ipc_right_copyin(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_entry_t		entry,
	mach_msg_type_name_t	msgt_name,
	boolean_t		deadok,
	ipc_object_t		*objectp,
	ipc_port_t		*sorightp)
{
	ipc_entry_bits_t bits = entry->ie_bits;

	assert(space->is_active);

	switch (msgt_name) {
	    case MACH_MSG_TYPE_MAKE_SEND: {
		ipc_port_t port;

		if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
			goto invalid_right;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);

		port->ip_mscount++;
		port->ip_srights++;
		ip_reference(port);
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*sorightp = IP_NULL;
		break;
	    }

	    case MACH_MSG_TYPE_MAKE_SEND_ONCE: {
		ipc_port_t port;

		if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
			goto invalid_right;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);

		port->ip_sorights++;
		ip_reference(port);
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*sorightp = IP_NULL;
		break;
	    }

	    case MACH_MSG_TYPE_MOVE_RECEIVE: {
		ipc_port_t port;
		ipc_port_t dnrequest = IP_NULL;

		if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
			goto invalid_right;

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);

		if (bits & MACH_PORT_TYPE_SEND) {
			assert(IE_BITS_TYPE(bits) ==
					MACH_PORT_TYPE_SEND_RECEIVE);
			assert(IE_BITS_UREFS(bits) > 0);
			assert(port->ip_srights > 0);

			ipc_hash_insert(space, (ipc_object_t) port,
					name, entry);

			ip_reference(port);
		} else {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_RECEIVE);
			assert(IE_BITS_UREFS(bits) == 0);

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			if (bits & IE_BITS_MAREQUEST)
				ipc_marequest_cancel(space, name);

			entry->ie_object = IO_NULL;
		}
		entry->ie_bits = bits &~ MACH_PORT_TYPE_RECEIVE;

		ipc_port_clear_receiver(port);

		port->ip_receiver_name = MACH_PORT_NULL;
		port->ip_destination = IP_NULL;
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*sorightp = dnrequest;
		break;
	    }

	    case MACH_MSG_TYPE_COPY_SEND: {
		ipc_port_t port;

		if (bits & MACH_PORT_TYPE_DEAD_NAME)
			goto copy_dead;

		/* allow for dead send-once rights */

		if ((bits & MACH_PORT_TYPE_SEND_RIGHTS) == 0)
			goto invalid_right;

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			goto copy_dead;
		}
		/* port is locked and active */

		if ((bits & MACH_PORT_TYPE_SEND) == 0) {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND_ONCE);
			assert(port->ip_sorights > 0);

			ip_unlock(port);
			goto invalid_right;
		}

		assert(port->ip_srights > 0);

		port->ip_srights++;
		ip_reference(port);
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*sorightp = IP_NULL;
		break;
	    }

	    case MACH_MSG_TYPE_MOVE_SEND: {
		ipc_port_t port;
		ipc_port_t dnrequest = IP_NULL;

		if (bits & MACH_PORT_TYPE_DEAD_NAME)
			goto move_dead;

		/* allow for dead send-once rights */

		if ((bits & MACH_PORT_TYPE_SEND_RIGHTS) == 0)
			goto invalid_right;

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			goto move_dead;
		}
		/* port is locked and active */

		if ((bits & MACH_PORT_TYPE_SEND) == 0) {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND_ONCE);
			assert(port->ip_sorights > 0);

			ip_unlock(port);
			goto invalid_right;
		}

		assert(port->ip_srights > 0);

		if (IE_BITS_UREFS(bits) == 1) {
			if (bits & MACH_PORT_TYPE_RECEIVE) {
				assert(port->ip_receiver_name == name);
				assert(port->ip_receiver == space);
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_SEND_RECEIVE);

				ip_reference(port);
			} else {
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_SEND);

				dnrequest = ipc_right_dncancel_macro(
						space, port, name, entry);

				ipc_hash_delete(space, (ipc_object_t) port,
						name, entry);

				if (bits & IE_BITS_MAREQUEST)
					ipc_marequest_cancel(space, name);

				entry->ie_object = IO_NULL;
			}
			entry->ie_bits = bits &~
				(IE_BITS_UREFS_MASK|MACH_PORT_TYPE_SEND);
		} else {
			port->ip_srights++;
			ip_reference(port);
			entry->ie_bits = bits-1; /* decrement urefs */
		}

		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*sorightp = dnrequest;
		break;
	    }

	    case MACH_MSG_TYPE_MOVE_SEND_ONCE: {
		ipc_port_t port;
		ipc_port_t dnrequest;

		if (bits & MACH_PORT_TYPE_DEAD_NAME)
			goto move_dead;

		/* allow for dead send rights */

		if ((bits & MACH_PORT_TYPE_SEND_RIGHTS) == 0)
			goto invalid_right;

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;
#endif	MACH_IPC_COMPAT

			bits = entry->ie_bits;
			goto move_dead;
		}
		/* port is locked and active */

		if ((bits & MACH_PORT_TYPE_SEND_ONCE) == 0) {
			assert(bits & MACH_PORT_TYPE_SEND);
			assert(port->ip_srights > 0);

			ip_unlock(port);
			goto invalid_right;
		}

		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND_ONCE);
		assert(IE_BITS_UREFS(bits) == 1);
		assert((bits & IE_BITS_MAREQUEST) == 0);
		assert(port->ip_sorights > 0);

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);
		ip_unlock(port);

		entry->ie_object = IO_NULL;
		entry->ie_bits = bits &~ MACH_PORT_TYPE_SEND_ONCE;

		*objectp = (ipc_object_t) port;
		*sorightp = dnrequest;
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_copyin: strange rights");
#else
		panic("ipc_right_copyin: strange rights");
#endif
	}

	return KERN_SUCCESS;

    copy_dead:
	assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
	assert(IE_BITS_UREFS(bits) > 0);
	assert((bits & IE_BITS_MAREQUEST) == 0);
	assert(entry->ie_request == 0);
	assert(entry->ie_object == 0);

	if (!deadok)
		goto invalid_right;

	*objectp = IO_DEAD;
	*sorightp = IP_NULL;
	return KERN_SUCCESS;

    move_dead:
	assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
	assert(IE_BITS_UREFS(bits) > 0);
	assert((bits & IE_BITS_MAREQUEST) == 0);
	assert(entry->ie_request == 0);
	assert(entry->ie_object == 0);

	if (!deadok)
		goto invalid_right;

	if (IE_BITS_UREFS(bits) == 1)
		entry->ie_bits = bits &~ MACH_PORT_TYPE_DEAD_NAME;
	else
		entry->ie_bits = bits-1; /* decrement urefs */

	*objectp = IO_DEAD;
	*sorightp = IP_NULL;
	return KERN_SUCCESS;

    invalid_right:
	return KERN_INVALID_RIGHT;

#if	MACH_IPC_COMPAT
    invalid_name:
	return KERN_INVALID_NAME;
#endif	MACH_IPC_COMPAT
}

/*
 *	Routine:	ipc_right_copyin_undo
 *	Purpose:
 *		Undoes the effects of an ipc_right_copyin
 *		of a send/send-once right that is dead.
 *		(Object is either IO_DEAD or a dead port.)
 *	Conditions:
 *		The space is write-locked and active.
 */

void
ipc_right_copyin_undo(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_entry_t		entry,
	mach_msg_type_name_t	msgt_name,
	ipc_object_t		object,
	ipc_port_t		soright)
{
	ipc_entry_bits_t bits = entry->ie_bits;

	assert(space->is_active);

	assert((msgt_name == MACH_MSG_TYPE_MOVE_SEND) ||
	       (msgt_name == MACH_MSG_TYPE_COPY_SEND) ||
	       (msgt_name == MACH_MSG_TYPE_MOVE_SEND_ONCE));

	if (soright != IP_NULL) {
		assert((msgt_name == MACH_MSG_TYPE_MOVE_SEND) ||
		       (msgt_name == MACH_MSG_TYPE_MOVE_SEND_ONCE));
		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE);
		assert(entry->ie_object == IO_NULL);
		assert(object != IO_DEAD);

		entry->ie_bits = ((bits &~ IE_BITS_RIGHT_MASK) |
				  MACH_PORT_TYPE_DEAD_NAME | 2);
	} else if (IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE) {
		assert((msgt_name == MACH_MSG_TYPE_MOVE_SEND) ||
		       (msgt_name == MACH_MSG_TYPE_MOVE_SEND_ONCE));
		assert(entry->ie_object == IO_NULL);

		entry->ie_bits = ((bits &~ IE_BITS_RIGHT_MASK) |
				  MACH_PORT_TYPE_DEAD_NAME | 1);
	} else if (IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME) {
		assert(entry->ie_object == IO_NULL);
		assert(object == IO_DEAD);
		assert(IE_BITS_UREFS(bits) > 0);

		if (msgt_name != MACH_MSG_TYPE_COPY_SEND) {
			assert(IE_BITS_UREFS(bits) < MACH_PORT_UREFS_MAX);

			entry->ie_bits = bits+1; /* increment urefs */
		}
	} else {
		assert((msgt_name == MACH_MSG_TYPE_MOVE_SEND) ||
		       (msgt_name == MACH_MSG_TYPE_COPY_SEND));
		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND);
		assert(object != IO_DEAD);
		assert(entry->ie_object == object);
		assert(IE_BITS_UREFS(bits) > 0);

		if (msgt_name != MACH_MSG_TYPE_COPY_SEND) {
			assert(IE_BITS_UREFS(bits) < MACH_PORT_UREFS_MAX-1);

			entry->ie_bits = bits+1; /* increment urefs */
		}

		/*
		 *	May as well convert the entry to a dead name.
		 *	(Or if it is a compat entry, destroy it.)
		 */

		(void) ipc_right_check(space, (ipc_port_t) object,
				       name, entry);
		/* object is dead so it is not locked */
	}

	/* release the reference acquired by copyin */

	if (object != IO_DEAD)
		ipc_object_release(object);
}

/*
 *	Routine:	ipc_right_copyin_two
 *	Purpose:
 *		Like ipc_right_copyin with MACH_MSG_TYPE_MOVE_SEND
 *		and deadok == FALSE, except that this moves two
 *		send rights at once.
 *	Conditions:
 *		The space is write-locked and active.
 *		The object is returned with two refs/send rights.
 *	Returns:
 *		KERN_SUCCESS		Acquired an object.
 *		KERN_INVALID_RIGHT	Name doesn't denote correct right.
 */

kern_return_t
ipc_right_copyin_two(
	ipc_space_t	space,
	mach_port_t	name,
	ipc_entry_t	entry,
	ipc_object_t	*objectp,
	ipc_port_t	*sorightp)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	mach_port_urefs_t urefs;
	ipc_port_t port;
	ipc_port_t dnrequest = IP_NULL;

	assert(space->is_active);

	if ((bits & MACH_PORT_TYPE_SEND) == 0)
		goto invalid_right;

	urefs = IE_BITS_UREFS(bits);
	if (urefs < 2)
		goto invalid_right;

	port = (ipc_port_t) entry->ie_object;
	assert(port != IP_NULL);

	if (ipc_right_check(space, port, name, entry)) {
#if	MACH_IPC_COMPAT
		if (bits & IE_BITS_COMPAT)
			goto invalid_name;
#endif	MACH_IPC_COMPAT

		goto invalid_right;
	}
	/* port is locked and active */

	assert(port->ip_srights > 0);

	if (urefs == 2) {
		if (bits & MACH_PORT_TYPE_RECEIVE) {
			assert(port->ip_receiver_name == name);
			assert(port->ip_receiver == space);
			assert(IE_BITS_TYPE(bits) ==
					MACH_PORT_TYPE_SEND_RECEIVE);

			port->ip_srights++;
			ip_reference(port);
			ip_reference(port);
		} else {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND);

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			ipc_hash_delete(space, (ipc_object_t) port,
					name, entry);

			if (bits & IE_BITS_MAREQUEST)
				ipc_marequest_cancel(space, name);

			port->ip_srights++;
			ip_reference(port);
			entry->ie_object = IO_NULL;
		}
		entry->ie_bits = bits &~
			(IE_BITS_UREFS_MASK|MACH_PORT_TYPE_SEND);
	} else {
		port->ip_srights += 2;
		ip_reference(port);
		ip_reference(port);
		entry->ie_bits = bits-2; /* decrement urefs */
	}
	ip_unlock(port);

	*objectp = (ipc_object_t) port;
	*sorightp = dnrequest;
	return KERN_SUCCESS;

    invalid_right:
	return KERN_INVALID_RIGHT;

#if	MACH_IPC_COMPAT
    invalid_name:
	return KERN_INVALID_NAME;
#endif	MACH_IPC_COMPAT
}

/*
 *	Routine:	ipc_right_copyout
 *	Purpose:
 *		Copyout a capability to a space.
 *		If successful, consumes a ref for the object.
 *
 *		Always succeeds when given a newly-allocated entry,
 *		because user-reference overflow isn't a possibility.
 *
 *		If copying out the object would cause the user-reference
 *		count in the entry to overflow, and overflow is TRUE,
 *		then instead the user-reference count is left pegged
 *		to its maximum value and the copyout succeeds anyway.
 *	Conditions:
 *		The space is write-locked and active.
 *		The object is locked and active.
 *		The object is unlocked; the space isn't.
 *	Returns:
 *		KERN_SUCCESS		Copied out capability.
 *		KERN_UREFS_OVERFLOW	User-refs would overflow;
 *			guaranteed not to happen with a fresh entry
 *			or if overflow=TRUE was specified.
 */

kern_return_t
ipc_right_copyout(
	ipc_space_t		space,
	mach_port_t		name,
	ipc_entry_t		entry,
	mach_msg_type_name_t	msgt_name,
	boolean_t		overflow,
	ipc_object_t		object)
{
	ipc_entry_bits_t bits = entry->ie_bits;
	ipc_port_t port;

	assert(IO_VALID(object));
	assert(io_otype(object) == IOT_PORT);
	assert(io_active(object));
	assert(entry->ie_object == object);

	port = (ipc_port_t) object;

	switch (msgt_name) {
	    case MACH_MSG_TYPE_PORT_SEND_ONCE:
		assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE);
		assert(port->ip_sorights > 0);

		/* transfer send-once right and ref to entry */
		ip_unlock(port);

		entry->ie_bits = bits | (MACH_PORT_TYPE_SEND_ONCE | 1);
		break;

	    case MACH_MSG_TYPE_PORT_SEND:
		assert(port->ip_srights > 0);

		if (bits & MACH_PORT_TYPE_SEND) {
			mach_port_urefs_t urefs = IE_BITS_UREFS(bits);

			assert(port->ip_srights > 1);
			assert(urefs > 0);
			assert(urefs < MACH_PORT_UREFS_MAX);

			if (urefs+1 == MACH_PORT_UREFS_MAX) {
				if (overflow) {
					/* leave urefs pegged to maximum */

					port->ip_srights--;
					ip_release(port);
					ip_unlock(port);
					return KERN_SUCCESS;
				}

				ip_unlock(port);
				return KERN_UREFS_OVERFLOW;
			}

			port->ip_srights--;
			ip_release(port);
			ip_unlock(port);
		} else if (bits & MACH_PORT_TYPE_RECEIVE) {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_RECEIVE);
			assert(IE_BITS_UREFS(bits) == 0);

			/* transfer send right to entry */
			ip_release(port);
			ip_unlock(port);
		} else {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE);
			assert(IE_BITS_UREFS(bits) == 0);

			/* transfer send right and ref to entry */
			ip_unlock(port);

			/* entry is locked holding ref, so can use port */

			ipc_hash_insert(space, (ipc_object_t) port,
					name, entry);
		}

		entry->ie_bits = (bits | MACH_PORT_TYPE_SEND) + 1;
		break;

	    case MACH_MSG_TYPE_PORT_RECEIVE: {
		ipc_port_t dest;

		assert(port->ip_mscount == 0);
		assert(port->ip_receiver_name == MACH_PORT_NULL);
		dest = port->ip_destination;

		port->ip_receiver_name = name;
		port->ip_receiver = space;

		assert((bits & MACH_PORT_TYPE_RECEIVE) == 0);

		if (bits & MACH_PORT_TYPE_SEND) {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_SEND);
			assert(IE_BITS_UREFS(bits) > 0);
			assert(port->ip_srights > 0);

			ip_release(port);
			ip_unlock(port);

			/* entry is locked holding ref, so can use port */

			ipc_hash_delete(space, (ipc_object_t) port,
					name, entry);
		} else {
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE);
			assert(IE_BITS_UREFS(bits) == 0);

			/* transfer ref to entry */
			ip_unlock(port);
		}

		entry->ie_bits = bits | MACH_PORT_TYPE_RECEIVE;

		if (dest != IP_NULL)
			ipc_port_release(dest);
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_copyout: strange rights");
#else
		panic("ipc_right_copyout: strange rights");
#endif
	}

	return KERN_SUCCESS;
}

#if 0
/*XXX same, but allows multiple duplicate send rights */
kern_return_t
ipc_right_copyout_multiname(space, name, entry, object)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
	ipc_object_t object;
{
	ipc_entry_bits_t bits = entry->ie_bits;
	ipc_port_t port;

	assert(IO_VALID(object));
	assert(io_otype(object) == IOT_PORT);
	assert(io_active(object));
	assert(entry->ie_object == object);

	port = (ipc_port_t) object;

	assert(port->ip_srights > 0);

	assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_NONE);
	assert(IE_BITS_UREFS(bits) == 0);

	/* transfer send right and ref to entry */
	ip_unlock(port);

	/* entry is locked holding ref, so can use port */

	entry->ie_bits = (bits | MACH_PORT_TYPE_SEND) + 1;

	return KERN_SUCCESS;
}
#endif

/*
 *	Routine:	ipc_right_rename
 *	Purpose:
 *		Transfer an entry from one name to another.
 *		The old entry is deallocated.
 *	Conditions:
 *		The space is write-locked and active.
 *		The new entry is unused.  Upon return,
 *		the space is unlocked.
 *	Returns:
 *		KERN_SUCCESS		Moved entry to new name.
 */

kern_return_t
ipc_right_rename(
	ipc_space_t	space,
	mach_port_t	oname,
	ipc_entry_t	oentry,
	mach_port_t	nname,
	ipc_entry_t	nentry)
{
	ipc_entry_bits_t bits = oentry->ie_bits;
	ipc_port_request_index_t request = oentry->ie_request;
	ipc_object_t object = oentry->ie_object;

	assert(space->is_active);
	assert(oname != nname);

	/*
	 *	If IE_BITS_COMPAT, we can't allow the entry to be renamed
	 *	if the port is dead.  (This would foil ipc_port_destroy.)
	 *	Instead we should fail because oentry shouldn't exist.
	 *	Note IE_BITS_COMPAT implies ie_request != 0.
	 */

	if (request != 0) {
		ipc_port_t port;

		assert(bits & MACH_PORT_TYPE_PORT_RIGHTS);
		port = (ipc_port_t) object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, oname, oentry)) {
#if	MACH_IPC_COMPAT
			if (bits & IE_BITS_COMPAT) {
				ipc_entry_dealloc(space, nname, nentry);
				is_write_unlock(space);
				return KERN_INVALID_NAME;
			}
#endif	MACH_IPC_COMPAT

			bits = oentry->ie_bits;
			assert(IE_BITS_TYPE(bits) == MACH_PORT_TYPE_DEAD_NAME);
			assert(oentry->ie_request == 0);
			request = 0;
			assert(oentry->ie_object == IO_NULL);
			object = IO_NULL;
		} else {
			/* port is locked and active */

			ipc_port_dnrename(port, request, oname, nname);
			ip_unlock(port);
			oentry->ie_request = 0;
		}
	}

	if (bits & IE_BITS_MAREQUEST) {
		assert(bits & MACH_PORT_TYPE_SEND_RECEIVE);

		ipc_marequest_rename(space, oname, nname);
	}

	/* initialize nentry before letting ipc_hash_insert see it */

	assert((nentry->ie_bits & IE_BITS_RIGHT_MASK) == 0);
	nentry->ie_bits |= bits & IE_BITS_RIGHT_MASK;
	nentry->ie_request = request;
	nentry->ie_object = object;

	switch (IE_BITS_TYPE(bits)) {
	    case MACH_PORT_TYPE_SEND: {
		ipc_port_t port;

		port = (ipc_port_t) object;
		assert(port != IP_NULL);

		ipc_hash_delete(space, (ipc_object_t) port, oname, oentry);
		ipc_hash_insert(space, (ipc_object_t) port, nname, nentry);
		break;
	    }

	    case MACH_PORT_TYPE_RECEIVE:
	    case MACH_PORT_TYPE_SEND_RECEIVE: {
		ipc_port_t port;

		port = (ipc_port_t) object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == oname);
		assert(port->ip_receiver == space);

		port->ip_receiver_name = nname;
		ip_unlock(port);
		break;
	    }

	    case MACH_PORT_TYPE_PORT_SET: {
		ipc_pset_t pset;

		pset = (ipc_pset_t) object;
		assert(pset != IPS_NULL);

		ips_lock(pset);
		assert(ips_active(pset));
		assert(pset->ips_local_name == oname);

		pset->ips_local_name = nname;
		ips_unlock(pset);
		break;
	    }

	    case MACH_PORT_TYPE_SEND_ONCE:
	    case MACH_PORT_TYPE_DEAD_NAME:
		break;

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_rename: strange rights");
#else
		panic("ipc_right_rename: strange rights");
#endif
	}

	assert(oentry->ie_request == 0);
	oentry->ie_object = IO_NULL;
	ipc_entry_dealloc(space, oname, oentry);
	is_write_unlock(space);

	return KERN_SUCCESS;
}

#if	MACH_IPC_COMPAT

/*
 *	Routine:	ipc_right_copyin_compat
 *	Purpose:
 *		Copyin a capability from a space.
 *		If successful, the caller gets a ref
 *		for the resulting object, which is always valid.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return.
 *		The space must be active.
 *	Returns:
 *		KERN_SUCCESS		Acquired a valid object.
 *		KERN_INVALID_RIGHT	Name doesn't denote correct right.
 *		KERN_INVALID_NAME	[MACH_IPC_COMPAT]
 *			Caller should pretend lookup of entry failed.
 */

kern_return_t
ipc_right_copyin_compat(space, name, entry, msgt_name, dealloc, objectp)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
	mach_msg_type_name_t msgt_name;
	boolean_t dealloc;
	ipc_object_t *objectp;
{
	ipc_entry_bits_t bits = entry->ie_bits;

	assert(space->is_active);

	switch (msgt_name) {
	    case MSG_TYPE_PORT:
		if (dealloc) {
			ipc_port_t port;
			ipc_port_t dnrequest;

			/*
			 *	Pulls a send right out of the space,
			 *	leaving the space with no rights.
			 *	Not allowed to destroy the port,
			 *	so the space can't have receive rights.
			 *	Doesn't operate on dead names.
			 */

			if (IE_BITS_TYPE(bits) != MACH_PORT_TYPE_SEND)
				goto invalid_right;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			if (ipc_right_check(space, port, name, entry)) {
				if (bits & IE_BITS_COMPAT)
					goto invalid_name;

				goto invalid_right;
			}
			/* port is locked and active */

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			assert(port->ip_srights > 0);
			ip_unlock(port);

			if (bits & IE_BITS_MAREQUEST)
				ipc_marequest_cancel(space, name);

			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
			is_write_unlock(space);

			if (dnrequest != IP_NULL)
				ipc_notify_port_deleted(dnrequest, name);

			*objectp = (ipc_object_t) port;
			break;
		} else {
			ipc_port_t port;

			/*
			 *	Pulls a send right out of the space,
			 *	making a send right if necessary.
			 *	Doesn't operate on dead names.
			 */

			if ((bits & MACH_PORT_TYPE_SEND_RECEIVE) == 0)
				goto invalid_right;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			if (ipc_right_check(space, port, name, entry)) {
				if (bits & IE_BITS_COMPAT)
					goto invalid_name;

				goto invalid_right;
			}
			/* port is locked and active */

			is_write_unlock(space);

			if ((bits & MACH_PORT_TYPE_SEND) == 0) {
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_RECEIVE);
				assert(IE_BITS_UREFS(bits) == 0);

				port->ip_mscount++;
			}

			port->ip_srights++;
			ip_reference(port);
			ip_unlock(port);

			*objectp = (ipc_object_t) port;
			break;
		}

	    case MSG_TYPE_PORT_ALL:
		if (dealloc) {
			ipc_port_t port;
			ipc_port_t dnrequest = IP_NULL;
			ipc_port_t nsrequest = IP_NULL;
			mach_port_mscount_t mscount = 0; /* '=0' to shut up lint */

			/*
			 *	Like MACH_MSG_TYPE_MOVE_RECEIVE, except that
			 *	the space is always left without rights,
			 *	so we kill send rights if necessary.
			 */

			if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
				goto invalid_right;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			ip_lock(port);
			assert(ip_active(port));
			assert(port->ip_receiver_name == name);
			assert(port->ip_receiver == space);

			dnrequest = ipc_right_dncancel_macro(space, port,
							     name, entry);

			if (bits & IE_BITS_MAREQUEST)
				ipc_marequest_cancel(space, name);

			entry->ie_object = IO_NULL;
			ipc_entry_dealloc(space, name, entry);
			is_write_unlock(space);

			if (bits & MACH_PORT_TYPE_SEND) {
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_SEND_RECEIVE);
				assert(IE_BITS_UREFS(bits) > 0);
				assert(port->ip_srights > 0);

				if (--port->ip_srights == 0) {
					nsrequest = port->ip_nsrequest;
					if (nsrequest != IP_NULL) {
						port->ip_nsrequest = IP_NULL;
						mscount = port->ip_mscount;
					}
				}
			}

			ipc_port_clear_receiver(port);

			port->ip_receiver_name = MACH_PORT_NULL;
			port->ip_destination = IP_NULL;
			ip_unlock(port);

			if (nsrequest != IP_NULL)
				ipc_notify_no_senders(nsrequest, mscount);

			if (dnrequest != IP_NULL)
				ipc_notify_port_deleted(dnrequest, name);

			*objectp = (ipc_object_t) port;
			break;
		} else {
			ipc_port_t port;

			/*
			 *	Like MACH_MSG_TYPE_MOVE_RECEIVE, except that
			 *	the space is always left with send rights,
			 *	so we make a send right if necessary.
			 */

			if ((bits & MACH_PORT_TYPE_RECEIVE) == 0)
				goto invalid_right;

			port = (ipc_port_t) entry->ie_object;
			assert(port != IP_NULL);

			ip_lock(port);
			assert(ip_active(port));
			assert(port->ip_receiver_name == name);
			assert(port->ip_receiver == space);

			if ((bits & MACH_PORT_TYPE_SEND) == 0) {
				assert(IE_BITS_TYPE(bits) ==
						MACH_PORT_TYPE_RECEIVE);
				assert(IE_BITS_UREFS(bits) == 0);

				/* ip_mscount will be cleared below */
				port->ip_srights++;
				bits |= MACH_PORT_TYPE_SEND | 1;
			}

			ipc_hash_insert(space, (ipc_object_t) port,
					name, entry);

			entry->ie_bits = bits &~ MACH_PORT_TYPE_RECEIVE;
			is_write_unlock(space);

			ipc_port_clear_receiver(port); /* clears ip_mscount */

			port->ip_receiver_name = MACH_PORT_NULL;
			port->ip_destination = IP_NULL;
			ip_reference(port);
			ip_unlock(port);

			*objectp = (ipc_object_t) port;
			break;
		}

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_copyin_compat: strange rights");
#else
		panic("ipc_right_copyin_compat: strange rights");
#endif
	}

	return KERN_SUCCESS;

    invalid_right:
	is_write_unlock(space);
	return KERN_INVALID_RIGHT;

    invalid_name:
	is_write_unlock(space);
	return KERN_INVALID_NAME;
}

/*
 *	Routine:	ipc_right_copyin_header
 *	Purpose:
 *		Copyin a capability from a space.
 *		If successful, the caller gets a ref
 *		for the resulting object, which is always valid.
 *		The type of the acquired capability is returned.
 *	Conditions:
 *		The space is write-locked, and is unlocked upon return.
 *		The space must be active.
 *	Returns:
 *		KERN_SUCCESS		Acquired a valid object.
 *		KERN_INVALID_RIGHT	Name doesn't denote correct right.
 *		KERN_INVALID_NAME	[MACH_IPC_COMPAT]
 *			Caller should pretend lookup of entry failed.
 */

kern_return_t
ipc_right_copyin_header(space, name, entry, objectp, msgt_namep)
	ipc_space_t space;
	mach_port_t name;
	ipc_entry_t entry;
	ipc_object_t *objectp;
	mach_msg_type_name_t *msgt_namep;
{
	ipc_entry_bits_t bits = entry->ie_bits;
	mach_port_type_t type = IE_BITS_TYPE(bits);

	assert(space->is_active);

	switch (type) {
	    case MACH_PORT_TYPE_PORT_SET:
	    case MACH_PORT_TYPE_DEAD_NAME:
		goto invalid_right;

	    case MACH_PORT_TYPE_RECEIVE: {
		ipc_port_t port;

		/*
		 *	Like MACH_MSG_TYPE_MAKE_SEND.
		 */

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		ip_lock(port);
		assert(ip_active(port));
		assert(port->ip_receiver_name == name);
		assert(port->ip_receiver == space);
		is_write_unlock(space);

		port->ip_mscount++;
		port->ip_srights++;
		ip_reference(port);
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*msgt_namep = MACH_MSG_TYPE_PORT_SEND;
		break;
	    }

	    case MACH_PORT_TYPE_SEND:
	    case MACH_PORT_TYPE_SEND_RECEIVE: {
		ipc_port_t port;

		/*
		 *	Like MACH_MSG_TYPE_COPY_SEND,
		 *	except that the port must be alive.
		 */

		assert(IE_BITS_UREFS(bits) > 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;

			goto invalid_right;
		}
		/* port is locked and active */

		assert(port->ip_srights > 0);
		is_write_unlock(space);

		port->ip_srights++;
		ip_reference(port);
		ip_unlock(port);

		*objectp = (ipc_object_t) port;
		*msgt_namep = MACH_MSG_TYPE_PORT_SEND;
		break;
	    }

	    case MACH_PORT_TYPE_SEND_ONCE: {
		ipc_port_t port;
		ipc_port_t dnrequest, notify;

		/*
		 *	Like MACH_MSG_TYPE_MOVE_SEND_ONCE,
		 *	except that the port must be alive
		 *	and a port-deleted notification is generated.
		 */

		assert(IE_BITS_UREFS(bits) == 1);
		assert((bits & IE_BITS_MAREQUEST) == 0);

		port = (ipc_port_t) entry->ie_object;
		assert(port != IP_NULL);

		if (ipc_right_check(space, port, name, entry)) {
			if (bits & IE_BITS_COMPAT)
				goto invalid_name;

			goto invalid_right;
		}
		/* port is locked and active */

		assert(port->ip_sorights > 0);

		dnrequest = ipc_right_dncancel_macro(space, port, name, entry);
		ip_unlock(port);

		entry->ie_object = IO_NULL;
		ipc_entry_dealloc(space, name, entry);

		notify = ipc_space_make_notify(space);
		is_write_unlock(space);

		if (dnrequest != IP_NULL)
			ipc_notify_port_deleted(dnrequest, name);

		if (IP_VALID(notify))
			ipc_notify_port_deleted_compat(notify, name);

		*objectp = (ipc_object_t) port;
		*msgt_namep = MACH_MSG_TYPE_PORT_SEND_ONCE;
		break;
	    }

	    default:
#if MACH_ASSERT
		assert(!"ipc_right_copyin_header: strange rights");
#else
		panic("ipc_right_copyin_header: strange rights");
#endif
	}

	return KERN_SUCCESS;

    invalid_right:
	is_write_unlock(space);
	return KERN_INVALID_RIGHT;

    invalid_name:
	is_write_unlock(space);
	return KERN_INVALID_NAME;
}

#endif	MACH_IPC_COMPAT
