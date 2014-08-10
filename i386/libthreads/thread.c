/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 * i386/thread.c
 *
 */


#include <mach/cthreads.h>
#include "cthread_internals.h"
#include <mach.h>

/*
 * Set up the initial state of a MACH thread
 * so that it will invoke cthread_body(child)
 * when it is resumed.
 */
void
cproc_setup(register cproc_t child, thread_t thread, void (*routine)(cproc_t))
{
	register int *top = (int *) (child->stack_base + child->stack_size);
	struct i386_thread_state state;
	register struct i386_thread_state *ts = &state;
	kern_return_t r;
	unsigned int count;

	/*
	 * Set up i386 call frame and registers.
	 * Read registers first to get correct segment values.
	 */
	count = i386_THREAD_STATE_COUNT;
	MACH_CALL(thread_get_state(thread,i386_THREAD_STATE,(thread_state_t) &state,&count),r);

	ts->eip = (int) routine;
	*--top = (int) child;	/* argument to function */
	*--top = 0;		/* fake return address */
	ts->uesp = (int) top;	/* set stack pointer */
	ts->ebp = 0;		/* clear frame pointer */

	MACH_CALL(thread_set_state(thread,i386_THREAD_STATE,(thread_state_t) &state,i386_THREAD_STATE_COUNT),r);
}

#if	defined(cthread_sp)
#undef	cthread_sp
#endif

int
cthread_sp(void)
{
	int x;

	return (int) &x;
}

