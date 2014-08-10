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

#include <mach_fixpri.h>
#include <cpus.h>

#include <mach/boolean.h>
#include <mach/thread_switch.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/counters.h>
#include <kern/ipc_kobject.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/ipc_sched.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/time_out.h>
#include <machine/machspl.h>	/* for splsched */

#if	MACH_FIXPRI
#include <mach/policy.h>
#endif	MACH_FIXPRI



/*
 *	swtch and swtch_pri both attempt to context switch (logic in
 *	thread_block no-ops the context switch if nothing would happen).
 *	A boolean is returned that indicates whether there is anything
 *	else runnable.
 *
 *	This boolean can be used by a thread waiting on a
 *	lock or condition:  If FALSE is returned, the thread is justified
 *	in becoming a resource hog by continuing to spin because there's
 *	nothing else useful that the processor could do.  If TRUE is
 *	returned, the thread should make one more check on the
 *	lock and then be a good citizen and really suspend.
 */

extern void thread_depress_priority();
extern kern_return_t thread_depress_abort();

#ifdef CONTINUATIONS
void swtch_continue()
{
	register processor_t	myprocessor;

	myprocessor = current_processor();
	thread_syscall_return(myprocessor->runq.count > 0 ||
			      myprocessor->processor_set->runq.count > 0);
	/*NOTREACHED*/
}
#else	/* not CONTINUATIONS */
#define swtch_continue 0
#endif	/* not CONTINUATIONS */

boolean_t swtch()
{
	register processor_t	myprocessor;

#if	NCPUS > 1
	myprocessor = current_processor();
	if (myprocessor->runq.count == 0 &&
	    myprocessor->processor_set->runq.count == 0)
		return(FALSE);
#endif	NCPUS > 1

	counter(c_swtch_block++);
	thread_block(swtch_continue);
	myprocessor = current_processor();
	return(myprocessor->runq.count > 0 ||
	       myprocessor->processor_set->runq.count > 0);
}

#ifdef CONTINUATIONS
void swtch_pri_continue()
{
	register thread_t	thread = current_thread();
	register processor_t	myprocessor;

	if (thread->depress_priority >= 0)
		(void) thread_depress_abort(thread);
	myprocessor = current_processor();
	thread_syscall_return(myprocessor->runq.count > 0 ||
			      myprocessor->processor_set->runq.count > 0);
	/*NOTREACHED*/
}
#else	/* not CONTINUATIONS */
#define swtch_pri_continue 0
#endif	/* not CONTINUATIONS */

boolean_t  swtch_pri(pri)
	int pri;
{
	register thread_t	thread = current_thread();
	register processor_t	myprocessor;

#ifdef	lint
	pri++;
#endif	lint

#if	NCPUS > 1
	myprocessor = current_processor();
	if (myprocessor->runq.count == 0 &&
	    myprocessor->processor_set->runq.count == 0)
		return(FALSE);
#endif	NCPUS > 1

	/*
	 *	XXX need to think about depression duration.
	 *	XXX currently using min quantum.
	 */
	thread_depress_priority(thread, min_quantum);

	counter(c_swtch_pri_block++);
	thread_block(swtch_pri_continue);

	if (thread->depress_priority >= 0)
		(void) thread_depress_abort(thread);
	myprocessor = current_processor();
	return(myprocessor->runq.count > 0 ||
	       myprocessor->processor_set->runq.count > 0);
}

extern int hz;

#ifdef CONTINUATIONS
void thread_switch_continue()
{
	register thread_t	cur_thread = current_thread();

	/*
	 *  Restore depressed priority			 
	 */
	if (cur_thread->depress_priority >= 0)
		(void) thread_depress_abort(cur_thread);
	thread_syscall_return(KERN_SUCCESS);
	/*NOTREACHED*/
}
#else	/* not CONTINUATIONS */
#define thread_switch_continue 0
#endif	/* not CONTINUATIONS */

/*
 *	thread_switch:
 *
 *	Context switch.  User may supply thread hint.
 *
 *	Fixed priority threads that call this get what they asked for
 *	even if that violates priority order.
 */
kern_return_t thread_switch(thread_name, option, option_time)
mach_port_t thread_name;
int option;
mach_msg_timeout_t option_time;
{
    register thread_t		cur_thread = current_thread();
    register processor_t	myprocessor;
    ipc_port_t			port;

    /*
     *	Process option.
     */
    switch (option) {
	case SWITCH_OPTION_NONE:
	    /*
	     *	Nothing to do.
	     */
	    break;

	case SWITCH_OPTION_DEPRESS:
	    /*
	     *	Depress priority for given time.
	     */
	    thread_depress_priority(cur_thread, option_time);
	    break;

	case SWITCH_OPTION_WAIT:
	    thread_will_wait_with_timeout(cur_thread, option_time);
	    break;

	default:
	    return(KERN_INVALID_ARGUMENT);
    }
    
#ifndef MIGRATING_THREADS /* XXX thread_run defunct */
    /*
     *	Check and act on thread hint if appropriate.
     */
    if ((thread_name != 0) &&
	(ipc_port_translate_send(cur_thread->task->itk_space,
				 thread_name, &port) == KERN_SUCCESS)) {
	    /* port is locked, but it might not be active */

	    /*
	     *	Get corresponding thread.
	     */
	    if (ip_active(port) && (ip_kotype(port) == IKOT_THREAD)) {
		register thread_t thread;
		register spl_t s;

		thread = (thread_t) port->ip_kobject;
		/*
		 *	Check if the thread is in the right pset. Then
		 *	pull it off its run queue.  If it
		 *	doesn't come, then it's not eligible.
		 */
		s = splsched();
		thread_lock(thread);
		if ((thread->processor_set == cur_thread->processor_set)
		    && (rem_runq(thread) != RUN_QUEUE_NULL)) {
			/*
			 *	Hah, got it!!
			 */
			thread_unlock(thread);
			(void) splx(s);
			ip_unlock(port);
			/* XXX thread might disappear on us now? */
#if	MACH_FIXPRI
			if (thread->policy == POLICY_FIXEDPRI) {
			    myprocessor = current_processor();
			    myprocessor->quantum = thread->sched_data;
			    myprocessor->first_quantum = TRUE;
			}
#endif	MACH_FIXPRI
			counter(c_thread_switch_handoff++);
			thread_run(thread_switch_continue, thread);
			/*
			 *  Restore depressed priority			 
			 */
			if (cur_thread->depress_priority >= 0)
				(void) thread_depress_abort(cur_thread);

			return(KERN_SUCCESS);
		}
		thread_unlock(thread);
		(void) splx(s);
	    }
	    ip_unlock(port);
    }
#endif /* not MIGRATING_THREADS */

    /*
     *	No handoff hint supplied, or hint was wrong.  Call thread_block() in
     *	hopes of running something else.  If nothing else is runnable,
     *	thread_block will detect this.  WARNING: thread_switch with no
     *	option will not do anything useful if the thread calling it is the
     *	highest priority thread (can easily happen with a collection
     *	of timesharing threads).
     */
#if	NCPUS > 1
    myprocessor = current_processor();
    if (myprocessor->processor_set->runq.count > 0 ||
	myprocessor->runq.count > 0)
#endif	NCPUS > 1
    {
	counter(c_thread_switch_block++);
	thread_block(thread_switch_continue);
    }

    /*
     *  Restore depressed priority			 
     */
    if (cur_thread->depress_priority >= 0)
	(void) thread_depress_abort(cur_thread);
    return(KERN_SUCCESS);
}

/*
 *	thread_depress_priority
 *
 *	Depress thread's priority to lowest possible for specified period.
 *	Intended for use when thread wants a lock but doesn't know which
 *	other thread is holding it.  As with thread_switch, fixed
 *	priority threads get exactly what they asked for.  Users access
 *	this by the SWITCH_OPTION_DEPRESS option to thread_switch.  A Time
 *      of zero will result in no timeout being scheduled.
 */
void
thread_depress_priority(thread, depress_time)
register thread_t thread;
mach_msg_timeout_t depress_time;
{
    unsigned int ticks;
    spl_t	s;

    /* convert from milliseconds to ticks */
    ticks = convert_ipc_timeout_to_ticks(depress_time);

    s = splsched();
    thread_lock(thread);

    /*
     *	If thread is already depressed, override previous depression.
     */
    reset_timeout_check(&thread->depress_timer);

    /*
     *	Save current priority, then set priority and
     *	sched_pri to their lowest possible values.
     */
    thread->depress_priority = thread->priority;
    thread->priority = 31;
    thread->sched_pri = 31;
    if (ticks != 0)
	set_timeout(&thread->depress_timer, ticks);

    thread_unlock(thread);
    (void) splx(s);
}	

/*
 *	thread_depress_timeout:
 *
 *	Timeout routine for priority depression.
 */
void
thread_depress_timeout(thread)
register thread_t thread;
{
    spl_t	s;

    s = splsched();
    thread_lock(thread);

    /*
     *	If we lose a race with thread_depress_abort,
     *	then depress_priority might be -1.
     */

    if (thread->depress_priority >= 0) {
	thread->priority = thread->depress_priority;
	thread->depress_priority = -1;
	compute_priority(thread, FALSE);
    }

    thread_unlock(thread);
    (void) splx(s);
}

/*
 *	thread_depress_abort:
 *
 *	Prematurely abort priority depression if there is one.
 */
kern_return_t
thread_depress_abort(thread)
register thread_t	thread;
{
    spl_t	s;

    if (thread == THREAD_NULL)
	return(KERN_INVALID_ARGUMENT);

    s = splsched();
    thread_lock(thread);

    /*
     *	Only restore priority if thread is depressed.
     */
    if (thread->depress_priority >= 0) {
	reset_timeout_check(&thread->depress_timer);
	thread->priority = thread->depress_priority;
	thread->depress_priority = -1;
	compute_priority(thread, FALSE);
    }

    thread_unlock(thread);
    (void) splx(s);
    return(KERN_SUCCESS);
}
