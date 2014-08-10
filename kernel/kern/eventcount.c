/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
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
 *	File:	eventcount.c
 *	Author:	Alessandro Forin
 *	Date:	10/91
 *
 *	Eventcounters, for user-level drivers synchronization
 *
 */


#include <cpus.h>

#include <mach/machine.h>
#include <kern/ast.h>
#include "cpu_number.h"
#include <kern/lock.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>

#include <machine/machspl.h>	/* For def'n of splsched() */

#include <kern/eventcount.h>


#if  NCPUS <= 1
void simpler_thread_setrun(
	thread_t	th,
	boolean_t	may_preempt); /* forward */
#endif

#define	MAX_EVCS	10		/* xxx for now */
evc_t	all_eventcounters[MAX_EVCS];

/*
 * Initialization
 */
void
evc_init(evc_t	ev)
{
	int i;

	bzero((char*)ev, sizeof(*ev));

	/* keep track of who is who */
	for (i = 0; i < MAX_EVCS; i++)
		if (all_eventcounters[i] == 0) break;
	if (i == MAX_EVCS) {
		printf("Too many eventcounters\n");
		return;
	}

	all_eventcounters[i] = ev;
	ev->ev_id = i;
	ev->sanity = ev;
	ev->waiting_thread = THREAD_NULL;
	simple_lock_init(&ev->lock);
}

/*
 * Finalization
 */
void
evc_destroy(evc_t	ev)
{
	evc_signal(ev);
	ev->sanity = 0;
	if (all_eventcounters[ev->ev_id] == ev)
		all_eventcounters[ev->ev_id] = 0;
	ev->ev_id = -1;
}

/*
 * Thread termination.
 * HORRIBLE. This stuff needs to be fixed.
 */
void evc_notify_abort(thread_t thread)
{
    int i;
    evc_t ev;
    int s = splsched();
    for (i = 0; i < MAX_EVCS; i++)  {
	ev = all_eventcounters[i];
	if (ev)  {
	    simple_lock(&ev->lock);
	    if (ev->waiting_thread == thread)  
	      {
		ev->waiting_thread = 0;
		/* Removal of a waiting thread has to bump the count by one */
		ev->count++;
	      }
	    simple_unlock(&ev->lock);
	}
    }
    splx(s);
}

#ifdef CONTINUATIONS
/*
 * Just so that we return success, and give
 * up the stack while blocked
 */
static void
evc_continue(void)
{
	thread_syscall_return(KERN_SUCCESS);
	/* NOTREACHED */
}
#else /* not CONTINUATIONS */
#define evc_continue 0
#endif /* not CONTINUATIONS */

/*
 * User-trappable
 */
kern_return_t evc_wait(natural_t ev_id)
{
	spl_t		s;
	kern_return_t	ret;
	evc_t		ev;

	if ((ev_id >= MAX_EVCS) ||
	    ((ev = all_eventcounters[ev_id]) == 0) ||
	    (ev->ev_id != ev_id) || (ev->sanity != ev))
		return KERN_INVALID_ARGUMENT;

	s = splsched();
	simple_lock(&ev->lock);
		/*
		 * The values assumed by the "count" field are
		 * as follows:
		 *	0	At initialization time, and with no
		 *		waiting thread means no events pending;
		 *		with waiting thread means the event
		 *		was signalled and the thread not yet resumed
		 *	-1	no events, there must be a waiting thread
		 *	N>0	no waiting thread means N pending,
		 *		with waiting thread N-1 pending.
		 *	
		 */
		if (ev->count > 0) {
			ev->count--;
			ret = KERN_SUCCESS;
		} else {
			if (ev->waiting_thread == THREAD_NULL) {
				ev->count--;
				ev->waiting_thread = current_thread();
				assert_wait((event_t) 0, TRUE);	/* ifnot race */
				simple_unlock(&ev->lock);
				thread_block(evc_continue);
				return KERN_SUCCESS;
			}
			ret = KERN_NO_SPACE; /* XX */
		}
	simple_unlock(&ev->lock);
	splx(s);
	return ret;
}

/*
 * User-trappable
 */
kern_return_t evc_wait_clear(natural_t ev_id)
{
	spl_t		s;
	kern_return_t	ret;
	evc_t		ev;

	if ((ev_id >= MAX_EVCS) ||
	    ((ev = all_eventcounters[ev_id]) == 0) ||
	    (ev->ev_id != ev_id) || (ev->sanity != ev))
		return KERN_INVALID_ARGUMENT;

	s = splsched();
	simple_lock(&ev->lock);

		/*
		 * The values assumed by the "count" field are
		 * as follows:
		 *	0	At initialization time, and with no
		 *		waiting thread means no events pending;
		 *		with waiting thread means the event
		 *		was signalled and the thread not yet resumed
		 *	-1	no events, there must be a waiting thread
		 *	N>0	no waiting thread means N pending,
		 *		with waiting thread N-1 pending.
		 *	
		 */
	        /*
		 *  Note that we always clear count before blocking.
		 */
		if (ev->waiting_thread == THREAD_NULL) {
			ev->count = -1;
			ev->waiting_thread = current_thread();
			assert_wait((event_t) 0, TRUE);	/* ifnot race */
			simple_unlock(&ev->lock);
			thread_block(evc_continue);
			/* NOTREACHED */
		}

	simple_unlock(&ev->lock);
	splx(s);
	ret = KERN_NO_SPACE; /* XX */
}

/*
 * Called exclusively from interrupt context
 */
void
evc_signal(evc_t ev)
{
    register volatile thread_t thread;
    register int state;
    spl_t    s;
    if (ev->sanity != ev)
      return;

    s = splsched();
    simple_lock(&ev->lock);
    ev->count++;
    if (thread = ev->waiting_thread, thread != THREAD_NULL)
    {
	ev->waiting_thread = 0;

#if (NCPUS > 1)
      retry:
	while((thread->state & TH_RUN) || thread->lock.lock_data)
		;
#endif
	thread_lock(thread);

	/* make thread runnable on this processor */
	/* taken from clear_wait */
	switch ((state = thread->state) & TH_SCHED_STATE)
	{
	    case  TH_WAIT | TH_SUSP | TH_UNINT:
	    case  TH_WAIT           | TH_UNINT:
	    case  TH_WAIT:
		/*
		 *	Sleeping and not suspendable - put
		 *	on run queue.
		 */
		thread->state = (state &~ TH_WAIT) | TH_RUN;
		thread_unlock(thread);
#if NCPUS > 1
		thread_setrun(thread, TRUE);
#else
		simpler_thread_setrun(thread, TRUE);
#endif
		break;

	    case TH_RUN | TH_WAIT:
#if (NCPUS > 1)
		/*
		 * Legal on MP: between assert_wait()
		 * and thread_block(), in evc_wait() above.
		 *
	 	 * Mmm. Maybe don't need now that the while(..) check is
		 * done before the thread lock is grabbed.....
		 */
		thread_unlock(thread);
		goto retry;
#else
		/*FALLTHROUGH*/
#endif
	    case          TH_WAIT | TH_SUSP:
	    case TH_RUN | TH_WAIT | TH_SUSP:
	    case TH_RUN | TH_WAIT           | TH_UNINT:
	    case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:

		/*
		 *	Either already running, or suspended.
		 *      Just clear the wait.
		 */
		thread->state = state &~ TH_WAIT;
		thread_unlock(thread);
		break;

	    default:
		/*
		 *	Not waiting.
		 */
		panic("evc_signal.3");
		thread_unlock(thread);
		break;
	}
    }

    simple_unlock(&ev->lock);
    splx(s);
}

#if	NCPUS <= 1
/*
 * The scheduler is too messy for my old little brain
 */
void
simpler_thread_setrun(
	thread_t	th,
	boolean_t	may_preempt)
{
	register struct run_queue	*rq;
	register			whichq;

	/*
	 *	XXX should replace queue with a boolean in this case.
	 */
	if (default_pset.idle_count > 0) {
		processor_t	processor;

		processor = (processor_t) queue_first(&default_pset.idle_queue);
		queue_remove(&default_pset.idle_queue, processor,
		processor_t, processor_queue);
		default_pset.idle_count--;
		processor->next_thread = th;
		processor->state = PROCESSOR_DISPATCHING;
		return;
	}
	rq = &(master_processor->runq);
	ast_on(cpu_number(), AST_BLOCK);

	whichq = (th)->sched_pri;
	simple_lock(&(rq)->lock);	/* lock the run queue */
	enqueue_head(&(rq)->runq[whichq], (queue_entry_t) (th));

	if (whichq < (rq)->low || (rq)->count == 0)
		 (rq)->low = whichq;	/* minimize */
	(rq)->count++;
#ifdef MIGRATING_THREADS
	(th)->shuttle.runq = (rq);
#else
	(th)->runq = (rq);
#endif
	simple_unlock(&(rq)->lock);

	/*
	 *	Turn off first_quantum to allow context switch.
	 */
	current_processor()->first_quantum = FALSE;
}
#endif	/* NCPUS > 1 */

