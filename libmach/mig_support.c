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
 *  Abstract:
 *	Routines to set and deallocate the mig reply port.
 *	They are called from mig generated interfaces.
 *
 */

#include <mach.h>
#include <mach/mig_support.h>

static mach_port_t	mig_reply_port = MACH_PORT_NULL;

/*****************************************************
 *  Called by mach_init. This is necessary after
 *  a fork to get rid of bogus port number.
 ****************************************************/

void mig_init(void *first)
{
	if (first == (void *) MACH_PORT_NULL)
		mig_reply_port = MACH_PORT_NULL;
}

/********************************************************
 *  Called by mig interfaces whenever they  need a reply port.
 *  Used to provide the same interface as multi-threaded tasks need.
 ********************************************************/

mach_port_t
mig_get_reply_port(void)
{
	if (mig_reply_port == MACH_PORT_NULL)
		mig_reply_port = mach_reply_port();

	return mig_reply_port;
}

/*************************************************************
 *  Called by mig interfaces after a timeout on the port.
 *  Could be called by user.
 ***********************************************************/

void
mig_dealloc_reply_port(mach_port_t p)
{
	mach_port_t port;

	port = mig_reply_port;
	mig_reply_port = MACH_PORT_NULL;

	(void) mach_port_mod_refs(mach_task_self(), port,
				  MACH_PORT_RIGHT_RECEIVE, -1);
}

/*************************************************************
 *  Called by mig interfaces when done with a port.
 *  Used to provide the same interface as needed when a custom
 *  allocator is used.
 ***********************************************************/

/*ARGSUSED*/
void
mig_put_reply_port(mach_port_t port)
{
	/* Do nothing */
}
