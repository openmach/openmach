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
/* **********************************************************************
 File:         kd_queue.c
 Description:  Event queue code for keyboard/display (and mouse) driver.

 $ Header: $

 Copyright Ing. C. Olivetti & C. S.p.A. 1989.
 All rights reserved.
********************************************************************** */
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#include <i386at/kd_queue.h>

/*
 * Notice that when adding an entry to the queue, the caller provides 
 * its own storage, which is copied into the queue.  However, when 
 * removing an entry from the queue, the caller is given a pointer to a 
 * queue element.  This means that the caller must either process the 
 * element or copy it into its own storage before unlocking the queue.
 *
 * These routines should be called only at a protected SPL.
 */

#define q_next(index)	(((index)+1) % KDQSIZE)

boolean_t
kdq_empty(q)
	kd_event_queue *q;
{
	return(q->firstfree == q->firstout);
}

boolean_t
kdq_full(q)
	kd_event_queue *q;
{
	return(q_next(q->firstfree) == q->firstout);
}

void
kdq_put(q, ev)
	kd_event_queue *q;
	kd_event *ev;
{
	kd_event *qp = q->events + q->firstfree;

	qp->type = ev->type;
	qp->time = ev->time;
	qp->value = ev->value;
	q->firstfree = q_next(q->firstfree);
}

kd_event *
kdq_get(q)
	kd_event_queue *q;
{
	kd_event *result = q->events + q->firstout;

	q->firstout = q_next(q->firstout);
	return(result);
}

void
kdq_reset(q)
	kd_event_queue *q;
{
	q->firstout = q->firstfree = 0;
}
