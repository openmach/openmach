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
 *	File:	ipc/ipc_marequest.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to handle msg-accepted requests.
 */

#include <mach_ipc_compat.h>

#include <mach/message.h>
#include <mach/port.h>
#include <kern/lock.h>
#include <kern/mach_param.h>
#include <kern/kalloc.h>
#include <kern/zalloc.h>
#include <ipc/port.h>
#include <ipc/ipc_init.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_right.h>
#include <ipc/ipc_marequest.h>
#include <ipc/ipc_notify.h>

#include <mach_ipc_debug.h>
#if	MACH_IPC_DEBUG
#include <mach/kern_return.h>
#include <mach_debug/hash_info.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#endif


zone_t ipc_marequest_zone;
int ipc_marequest_max = IMAR_MAX;

#define	imar_alloc()		((ipc_marequest_t) zalloc(ipc_marequest_zone))
#define	imar_free(imar)		zfree(ipc_marequest_zone, (vm_offset_t) (imar))

typedef unsigned int ipc_marequest_index_t;

ipc_marequest_index_t ipc_marequest_size;
ipc_marequest_index_t ipc_marequest_mask;

#define	IMAR_HASH(space, name)						\
		((((ipc_marequest_index_t)((vm_offset_t)space) >> 4) +	\
		  MACH_PORT_INDEX(name) + MACH_PORT_NGEN(name)) &	\
		 ipc_marequest_mask)

typedef struct ipc_marequest_bucket {
	decl_simple_lock_data(, imarb_lock_data)
	ipc_marequest_t imarb_head;
} *ipc_marequest_bucket_t;

#define	IMARB_NULL	((ipc_marequest_bucket_t) 0)

#define	imarb_lock_init(imarb)	simple_lock_init(&(imarb)->imarb_lock_data)
#define	imarb_lock(imarb)	simple_lock(&(imarb)->imarb_lock_data)
#define	imarb_unlock(imarb)	simple_unlock(&(imarb)->imarb_lock_data)

ipc_marequest_bucket_t ipc_marequest_table;



/*
 *	Routine:	ipc_marequest_init
 *	Purpose:
 *		Initialize the msg-accepted request module.
 */

void
ipc_marequest_init()
{
	ipc_marequest_index_t i;

	/* if not configured, initialize ipc_marequest_size */

	if (ipc_marequest_size == 0) {
		ipc_marequest_size = ipc_marequest_max >> 8;
		if (ipc_marequest_size < 16)
			ipc_marequest_size = 16;
	}

	/* make sure it is a power of two */

	ipc_marequest_mask = ipc_marequest_size - 1;
	if ((ipc_marequest_size & ipc_marequest_mask) != 0) {
		unsigned int bit;

		/* round up to closest power of two */

		for (bit = 1;; bit <<= 1) {
			ipc_marequest_mask |= bit;
			ipc_marequest_size = ipc_marequest_mask + 1;

			if ((ipc_marequest_size & ipc_marequest_mask) == 0)
				break;
		}
	}

	/* allocate ipc_marequest_table */

	ipc_marequest_table = (ipc_marequest_bucket_t)
		kalloc((vm_size_t) (ipc_marequest_size *
				    sizeof(struct ipc_marequest_bucket)));
	assert(ipc_marequest_table != IMARB_NULL);

	/* and initialize it */

	for (i = 0; i < ipc_marequest_size; i++) {
		ipc_marequest_bucket_t bucket;

		bucket = &ipc_marequest_table[i];
		imarb_lock_init(bucket);
		bucket->imarb_head = IMAR_NULL;
	}

	ipc_marequest_zone =
		zinit(sizeof(struct ipc_marequest),
		      ipc_marequest_max * sizeof(struct ipc_marequest),
		      sizeof(struct ipc_marequest),
		      IPC_ZONE_TYPE, "ipc msg-accepted requests");
}

/*
 *	Routine:	ipc_marequest_create
 *	Purpose:
 *		Create a msg-accepted request, because
 *		a sender is forcing a message with MACH_SEND_NOTIFY.
 *
 *		The "notify" argument should name a receive right
 *		that is used to create the send-once notify port.
 *
 *		[MACH_IPC_COMPAT] If "notify" is MACH_PORT_NULL,
 *		then an old-style msg-accepted request is created.
 *	Conditions:
 *		Nothing locked; refs held for space and port.
 *	Returns:
 *		MACH_MSG_SUCCESS	Msg-accepted request created.
 *		MACH_SEND_INVALID_NOTIFY	The space is dead.
 *		MACH_SEND_INVALID_NOTIFY	The notify port is bad.
 *		MACH_SEND_NOTIFY_IN_PROGRESS
 *			This space has already forced a message to this port.
 *		MACH_SEND_NO_NOTIFY	Can't allocate a msg-accepted request.
 */

mach_msg_return_t
ipc_marequest_create(space, port, notify, marequestp)
	ipc_space_t space;
	ipc_port_t port;
	mach_port_t notify;
	ipc_marequest_t *marequestp;
{
	mach_port_t name;
	ipc_entry_t entry;
	ipc_port_t soright;
	ipc_marequest_t marequest;
	ipc_marequest_bucket_t bucket;

#if	!MACH_IPC_COMPAT
	assert(notify != MACH_PORT_NULL);
#endif	!MACH_IPC_COMPAT

	marequest = imar_alloc();
	if (marequest == IMAR_NULL)
		return MACH_SEND_NO_NOTIFY;

	/*
	 *	Delay creating the send-once right until
	 *	we know there will be no errors.  Otherwise,
	 *	we would have to worry about disposing of it
	 *	when it turned out it wasn't needed.
	 */

	is_write_lock(space);
	if (!space->is_active) {
		is_write_unlock(space);
		imar_free(marequest);
		return MACH_SEND_INVALID_NOTIFY;
	}

	if (ipc_right_reverse(space, (ipc_object_t) port, &name, &entry)) {
		ipc_entry_bits_t bits;

		/* port is locked and active */
		ip_unlock(port);
		bits = entry->ie_bits;

		assert(port == (ipc_port_t) entry->ie_object);
		assert(bits & MACH_PORT_TYPE_SEND_RECEIVE);

		if (bits & IE_BITS_MAREQUEST) {
			is_write_unlock(space);
			imar_free(marequest);
			return MACH_SEND_NOTIFY_IN_PROGRESS;
		}

#if	MACH_IPC_COMPAT
		if (notify == MACH_PORT_NULL)
			soright = IP_NULL;
		else
#endif	MACH_IPC_COMPAT
		if ((soright = ipc_port_lookup_notify(space, notify))
								== IP_NULL) {
			is_write_unlock(space);
			imar_free(marequest);
			return MACH_SEND_INVALID_NOTIFY;
		}

		entry->ie_bits = bits | IE_BITS_MAREQUEST;

		is_reference(space);
		marequest->imar_space = space;
		marequest->imar_name = name;
		marequest->imar_soright = soright;

		bucket = &ipc_marequest_table[IMAR_HASH(space, name)];
		imarb_lock(bucket);

		marequest->imar_next = bucket->imarb_head;
		bucket->imarb_head = marequest;

		imarb_unlock(bucket);
	} else {
#if	MACH_IPC_COMPAT
		if (notify == MACH_PORT_NULL)
			soright = IP_NULL;
		else
#endif	MACH_IPC_COMPAT
		if ((soright = ipc_port_lookup_notify(space, notify))
								== IP_NULL) {
			is_write_unlock(space);
			imar_free(marequest);
			return MACH_SEND_INVALID_NOTIFY;
		}

		is_reference(space);
		marequest->imar_space = space;
		marequest->imar_name = MACH_PORT_NULL;
		marequest->imar_soright = soright;
	}

	is_write_unlock(space);
	*marequestp = marequest;
	return MACH_MSG_SUCCESS;
}

/*
 *	Routine:	ipc_marequest_cancel
 *	Purpose:
 *		Cancel a msg-accepted request, because
 *		the space's entry is being destroyed.
 *	Conditions:
 *		The space is write-locked and active.
 */

void
ipc_marequest_cancel(space, name)
	ipc_space_t space;
	mach_port_t name;
{
	ipc_marequest_bucket_t bucket;
	ipc_marequest_t marequest, *last;

	assert(space->is_active);

	bucket = &ipc_marequest_table[IMAR_HASH(space, name)];
	imarb_lock(bucket);

	for (last = &bucket->imarb_head;
	     (marequest = *last) != IMAR_NULL;
	     last = &marequest->imar_next)
		if ((marequest->imar_space == space) &&
		    (marequest->imar_name == name))
			break;

	assert(marequest != IMAR_NULL);
	*last = marequest->imar_next;
	imarb_unlock(bucket);

	marequest->imar_name = MACH_PORT_NULL;
}

/*
 *	Routine:	ipc_marequest_rename
 *	Purpose:
 *		Rename a msg-accepted request, because the entry
 *		in the space is being renamed.
 *	Conditions:
 *		The space is write-locked and active.
 */

void
ipc_marequest_rename(space, old, new)
	ipc_space_t space;
	mach_port_t old, new;
{
	ipc_marequest_bucket_t bucket;
	ipc_marequest_t marequest, *last;

	assert(space->is_active);

	bucket = &ipc_marequest_table[IMAR_HASH(space, old)];
	imarb_lock(bucket);

	for (last = &bucket->imarb_head;
	     (marequest = *last) != IMAR_NULL;
	     last = &marequest->imar_next)
		if ((marequest->imar_space == space) &&
		    (marequest->imar_name == old))
			break;

	assert(marequest != IMAR_NULL);
	*last = marequest->imar_next;
	imarb_unlock(bucket);

	marequest->imar_name = new;

	bucket = &ipc_marequest_table[IMAR_HASH(space, new)];
	imarb_lock(bucket);

	marequest->imar_next = bucket->imarb_head;
	bucket->imarb_head = marequest;

	imarb_unlock(bucket);
}

/*
 *	Routine:	ipc_marequest_destroy
 *	Purpose:
 *		Destroy a msg-accepted request, because
 *		the kernel message is being received/destroyed.
 *	Conditions:
 *		Nothing locked.
 */

void
ipc_marequest_destroy(marequest)
	ipc_marequest_t marequest;
{
	ipc_space_t space = marequest->imar_space;
	mach_port_t name;
	ipc_port_t soright;
#if	MACH_IPC_COMPAT
	ipc_port_t sright = IP_NULL;
#endif	MACH_IPC_COMPAT

	is_write_lock(space);

	name = marequest->imar_name;
	soright = marequest->imar_soright;

	if (name != MACH_PORT_NULL) {
		ipc_marequest_bucket_t bucket;
		ipc_marequest_t this, *last;

		bucket = &ipc_marequest_table[IMAR_HASH(space, name)];
		imarb_lock(bucket);

		for (last = &bucket->imarb_head;
		     (this = *last) != IMAR_NULL;
		     last = &this->imar_next)
			if ((this->imar_space == space) &&
			    (this->imar_name == name))
				break;

		assert(this == marequest);
		*last = this->imar_next;
		imarb_unlock(bucket);

		if (space->is_active) {
			ipc_entry_t entry;

			entry = ipc_entry_lookup(space, name);
			assert(entry != IE_NULL);
			assert(entry->ie_bits & IE_BITS_MAREQUEST);
			assert(entry->ie_bits & MACH_PORT_TYPE_SEND_RECEIVE);

			entry->ie_bits &= ~IE_BITS_MAREQUEST;

#if	MACH_IPC_COMPAT
			if (soright == IP_NULL)
				sright = ipc_space_make_notify(space);
#endif	MACH_IPC_COMPAT
		} else
			name = MACH_PORT_NULL;
	}

	is_write_unlock(space);
	is_release(space);

	imar_free(marequest);

#if	MACH_IPC_COMPAT
	if (soright == IP_NULL) {
		if (IP_VALID(sright)) {
			assert(name != MACH_PORT_NULL);
			ipc_notify_msg_accepted_compat(sright, name);
		}

		return;
	}
	assert(sright == IP_NULL);
#endif	MACH_IPC_COMPAT

	assert(soright != IP_NULL);
	ipc_notify_msg_accepted(soright, name);
}

#if	MACH_IPC_DEBUG


/*
 *	Routine:	ipc_marequest_info
 *	Purpose:
 *		Return information about the marequest hash table.
 *		Fills the buffer with as much information as possible
 *		and returns the desired size of the buffer.
 *	Conditions:
 *		Nothing locked.  The caller should provide
 *		possibly-pageable memory.
 */

unsigned int
ipc_marequest_info(maxp, info, count)
	unsigned int *maxp;
	hash_info_bucket_t *info;
	unsigned int count;
{
	ipc_marequest_index_t i;

	if (ipc_marequest_size < count)
		count = ipc_marequest_size;

	for (i = 0; i < count; i++) {
		ipc_marequest_bucket_t bucket = &ipc_marequest_table[i];
		unsigned int bucket_count = 0;
		ipc_marequest_t marequest;

		imarb_lock(bucket);
		for (marequest = bucket->imarb_head;
		     marequest != IMAR_NULL;
		     marequest = marequest->imar_next)
			bucket_count++;
		imarb_unlock(bucket);

		/* don't touch pageable memory while holding locks */
		info[i].hib_count = bucket_count;
	}

	*maxp = ipc_marequest_max;
	return ipc_marequest_size;
}

#endif	MACH_IPC_DEBUG
