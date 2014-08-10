/*
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
/*
 *	File:	shuttle.h
 *	Author:	Bryan Ford
 *
 *	This file contains definitions for shuttles,
 *	which handle microscheduling for individual threads.
 *
 */

#ifndef	_KERN_SHUTTLE_H_
#define _KERN_SHUTTLE_H_

#include <kern/lock.h>



struct Shuttle {
	/* XXX must be first in thread */
/*
 *	NOTE:	The runq field in the thread structure has an unusual
 *	locking protocol.  If its value is RUN_QUEUE_NULL, then it is
 *	locked by the thread_lock, but if its value is something else
 *	(i.e. a run_queue) then it is locked by that run_queue's lock.
 */
	queue_chain_t	links;		/* current run queue links */
	run_queue_t	runq;		/* run queue p is on SEE BELOW */

	/* Next pointer when on a queue */
	struct Shuttle *next;

	/* Micropriority level */
	int priority;

	/* General-purpose pointer field whose use depends on what the
	   thread happens to be doing */
	void		*message;

	int foobar[1];
};
typedef struct Shuttle Shuttle;



/* Exported functions */



/* Machine-dependent code must define the following functions */



#endif	_KERN_SHUTTLE_H_
