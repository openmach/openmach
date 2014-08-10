/* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
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
 * 	File:	mig_support.c
 *	Author:	Mary R. Thompson, Carnegie Mellon University
 *	Date:	July, 1987
 *
 * 	Routines to set and deallocate the mig reply port for the current thread.
 * 	Called from mig-generated interfaces.
 *
 */


#include <mach.h>
#include <mach/mig_support.h>
#include <mach/mach_traps.h>
#include <mach/cthreads.h>
#include "cthread_internals.h"

private boolean_t multithreaded = FALSE;
/* use a global reply port before becoming multi-threaded */
private mach_port_t mig_reply_port = MACH_PORT_NULL;

/*
 * Called by mach_init with 0 before cthread_init is
 * called and again with initial cproc at end of cthread_init.
 */
void
mig_init(register void *initial)
{
	if (initial == NO_CPROC) {
		/* called from mach_init before cthread_init,
		   possibly after a fork.  clear global reply port. */

		multithreaded = FALSE;
		mig_reply_port = MACH_PORT_NULL;
	} else {
		/* recycle global reply port as this cthread's reply port */

		multithreaded = TRUE;
		((cproc_t) initial)->reply_port = mig_reply_port;
		mig_reply_port = MACH_PORT_NULL;
	}
}

/*
 * Called by mig interface code whenever a reply port is needed.
 */
mach_port_t
mig_get_reply_port(void)
{
	register mach_port_t reply_port;

	if (multithreaded) {
		register cproc_t self;

		self = cproc_self();
		ASSERT(self != NO_CPROC);

		if ((reply_port = self->reply_port) == MACH_PORT_NULL)
			self->reply_port = reply_port = mach_reply_port();
	} else {
		if ((reply_port = mig_reply_port) == MACH_PORT_NULL)
			mig_reply_port = reply_port = mach_reply_port();
	}

	return reply_port;
}

/*
 * Called by mig interface code after a timeout on the reply port.
 * May also be called by user.
 */
void
mig_dealloc_reply_port(mach_port_t p)
{
	register mach_port_t reply_port;

	if (multithreaded) {
		register cproc_t self;

		self = cproc_self();
		ASSERT(self != NO_CPROC);

		reply_port = self->reply_port;
		self->reply_port = MACH_PORT_NULL;
	} else {
		reply_port = mig_reply_port;
		mig_reply_port = MACH_PORT_NULL;
	}

	(void) mach_port_mod_refs(mach_task_self(), reply_port,
				  MACH_PORT_RIGHT_RECEIVE, -1);
}

/*
 *  Called by mig interfaces when done with a port.
 *  Used to provide the same interface as needed when a custom
 *  allocator is used.
 */

/*ARGSUSED*/
void
mig_put_reply_port(mach_port_t port)
{
	/* Do nothing */
}
