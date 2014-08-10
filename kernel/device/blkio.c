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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	7/89
 *
 * 	Block IO driven from generic kernel IO interface.
 */
#include <mach/kern_return.h>

#include <device/param.h>
#include <device/device_types.h>
#include <device/io_req.h>
#include <device/ds_routines.h>



io_return_t block_io(strat, max_count, ior)
	void			(*strat)();
	void			(*max_count)();
	register io_req_t	ior;
{
	register kern_return_t	rc;
	boolean_t		wait = FALSE;

	/*
	 * Make sure the size is not too large by letting max_count
	 * change io_count.  If we are doing a write, then io_alloc_size
	 * preserves the original io_count.
	 */
	(*max_count)(ior);

	/*
	 * If reading, allocate memory.  If writing, wire
	 * down the incoming memory.
	 */
	if (ior->io_op & IO_READ)
	    rc = device_read_alloc(ior, (vm_size_t)ior->io_count);
	else
	    rc = device_write_get(ior, &wait);

	if (rc != KERN_SUCCESS)
	    return (rc);

	/*
	 * Queue the operation for the device.
	 */
	(*strat)(ior);

	/*
	 * The io is now queued.  Wait for it if needed.
	 */
	if (wait) {
		iowait(ior);
		return(D_SUCCESS);
	}

	return (D_IO_QUEUED);
}

/*
 * 'standard' max_count routine.  VM continuations mean that this
 * code can cope with arbitrarily-sized write operations (they won't be
 * atomic, but any caller that cares will do the op synchronously).
 */
#define MAX_PHYS        (256 * 1024)

void minphys(ior)
	register io_req_t	ior;
{
	if ((ior->io_op & (IO_WRITE | IO_READ | IO_OPEN)) == IO_WRITE)
	    return;

        if (ior->io_count > MAX_PHYS)
            ior->io_count = MAX_PHYS;
}

/*
 * Dummy routine placed in device switch entries to indicate that
 * block device may be mapped.
 */
vm_offset_t block_io_mmap()
{
	return (0);
}

/*
 * Disk sort routine.
 *
 * We order the disk request chain so that the disk head will sweep
 * back and forth across the disk.  The chain is divided into two
 * pieces, with requests ordered in opposite directions.  Assume that
 * the first part of the chain holds increasing cylinder numbers.
 * If a new request has a higher cylinder number than the head of
 * the chain, the disk head has not yet reached it; the new request
 * can go in the first part of the chain.  If the new request has
 * a lower cylinder number, the disk head has already passed it and
 * must catch it on the way back; so the new request goes in the
 * second (descending) part of the chain.
 * When all of the requests in the ascending portion are filled,
 * the descending chain becomes the first chain, and requests above
 * the first now go in the second part of the chain (ascending).
 */

#define	io_cylinder	io_residual
				/* Disk drivers put cylinder here */
#define	h_head		io_next
#define	h_tail		io_prev
				/* IORs are chained here */

void disksort(head, ior)
	io_req_t	head;	/* (sort of) */
	io_req_t	ior;
{
	register int		cylinder = ior->io_cylinder;
	register io_req_t	next, prev;

	next = head->h_head;
	if (next == 0) {
	    head->h_head = ior;
	    head->h_tail = ior;
	    ior->io_next = 0;
	    return;
	}

	do {
	    prev = next;
	    next = prev->io_next;
	} while (next != 0 && prev->io_cylinder == next->io_cylinder);

	if (next == 0) {
	    prev->io_next = ior;
	    head->h_tail = ior;
	    ior->io_next = 0;
	    return;
	}

	if (prev->io_cylinder < next->io_cylinder) {
	    /*
	     * Ascending list first.
	     */
	    if (prev->io_cylinder <= cylinder) {
		/*
		 * Insert in ascending list.
		 */
		while (next != 0 &&
			next->io_cylinder <= cylinder &&
			prev->io_cylinder <= next->io_cylinder)
		{
		    prev = next;
		    next = prev->io_next;
		}
	    }
	    else {
		/*
		 * Insert in descending list
		 */
		do {
		    prev = next;
		    next = prev->io_next;
		} while (next != 0 &&
			prev->io_cylinder <= next->io_cylinder);

		while (next != 0 &&
			next->io_cylinder >= cylinder)
		{
		    prev = next;
		    next = prev->io_next;
		}
	    }
	}
	else {
	    /*
	     * Descending first.
	     */
	    if (prev->io_cylinder >= cylinder) {
		/*
		 * Insert in descending list.
		 */
		while (next != 0 &&
			next->io_cylinder >= cylinder &&
			prev->io_cylinder >= next->io_cylinder)
		{
		    prev = next;
		    next = prev->io_next;
		}
	    }
	    else {
		/*
		 * Insert in ascending list
		 */
		do {
		    prev = next;
		    next = prev->io_next;
		} while (next != 0 &&
			prev->io_cylinder >= next->io_cylinder);
		while (next != 0 &&
			next->io_cylinder <= cylinder)
		{
		    prev = next;
		    next = prev->io_next;
		}
	    }
	}
	/*
	 * Insert between prev and next.
	 */
	prev->io_next = ior;
	ior->io_next = next;
	if (next == 0) {
	    /* At tail of list. */
	    head->h_tail = ior;
	}
}

