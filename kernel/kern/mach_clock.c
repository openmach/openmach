/*
 * Mach Operating System
 * Copyright (c) 1994-1988 Carnegie Mellon University.
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
 *	File:	clock_prim.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1986
 *
 *	Clock primitives.
 */
#include <cpus.h>
#include <mach_pcsample.h>
#include <stat_time.h>

#include <mach/boolean.h>
#include <mach/machine.h>
#include <mach/time_value.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <kern/counters.h>
#include "cpu_number.h"
#include <kern/host.h>
#include <kern/lock.h>
#include <kern/mach_param.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/time_out.h>
#include <kern/time_stamp.h>
#include <vm/vm_kern.h>
#include <sys/time.h>
#include <machine/mach_param.h>	/* HZ */
#include <machine/machspl.h>

#if MACH_PCSAMPLE
#include <kern/pc_sample.h>
#endif


void softclock();		/* forward */

int		hz = HZ;		/* number of ticks per second */
int		tick = (1000000 / HZ);	/* number of usec per tick */
time_value_t	time = { 0, 0 };	/* time since bootup (uncorrected) */
unsigned long	elapsed_ticks = 0;	/* ticks elapsed since bootup */

int		timedelta = 0;
int		tickdelta = 0;

#if	HZ > 500
int		tickadj = 1;		/* can adjust HZ usecs per second */
#else
int		tickadj = 500 / HZ;	/* can adjust 100 usecs per second */
#endif
int		bigadj = 1000000;	/* adjust 10*tickadj if adjustment
					   > bigadj */

/*
 *	This update protocol, with a check value, allows
 *		do {
 *			secs = mtime->seconds;
 *			usecs = mtime->microseconds;
 *		} while (secs != mtime->check_seconds);
 *	to read the time correctly.  (On a multiprocessor this assumes
 *	that processors see each other's writes in the correct order.
 *	We may have to insert fence operations.)
 */

mapped_time_value_t *mtime = 0;

#define update_mapped_time(time)				\
MACRO_BEGIN							\
	if (mtime != 0) {					\
		mtime->check_seconds = (time)->seconds;		\
		mtime->microseconds = (time)->microseconds;	\
		mtime->seconds = (time)->seconds;		\
	}							\
MACRO_END

decl_simple_lock_data(,	timer_lock)	/* lock for ... */
timer_elt_data_t	timer_head;	/* ordered list of timeouts */
					/* (doubles as end-of-list) */

/*
 *	Handle clock interrupts.
 *
 *	The clock interrupt is assumed to be called at a (more or less)
 *	constant rate.  The rate must be identical on all CPUS (XXX - fix).
 *
 *	Usec is the number of microseconds that have elapsed since the
 *	last clock tick.  It may be constant or computed, depending on
 *	the accuracy of the hardware clock.
 *
 */
void clock_interrupt(usec, usermode, basepri)
	register int	usec;		/* microseconds per tick */
	boolean_t	usermode;	/* executing user code */
	boolean_t	basepri;	/* at base priority */
{
	register int		my_cpu = cpu_number();
	register thread_t	thread = current_thread();

	counter(c_clock_ticks++);
	counter(c_threads_total += c_threads_current);
	counter(c_stacks_total += c_stacks_current);

#if	STAT_TIME
	/*
	 *	Increment the thread time, if using
	 *	statistical timing.
	 */
	if (usermode) {
	    timer_bump(&thread->user_timer, usec);
	}
	else {
	    timer_bump(&thread->system_timer, usec);
	}
#endif	STAT_TIME

	/*
	 *	Increment the CPU time statistics.
	 */
	{
	    extern void	thread_quantum_update(); /* in priority.c */
	    register int	state;

	    if (usermode)
		state = CPU_STATE_USER;
	    else if (!cpu_idle(my_cpu))
		state = CPU_STATE_SYSTEM;
	    else
		state = CPU_STATE_IDLE;

	    machine_slot[my_cpu].cpu_ticks[state]++;

	    /*
	     *	Adjust the thread's priority and check for
	     *	quantum expiration.
	     */

	    thread_quantum_update(my_cpu, thread, 1, state);
	}

#if 	MACH_SAMPLE
	/*
	 * Take a sample of pc for the user if required.
	 * This had better be MP safe.  It might be interesting
	 * to keep track of cpu in the sample.
	 */
	if (usermode) {
		take_pc_sample_macro(thread, SAMPLED_PC_PERIODIC);
	}
#endif /* MACH_PCSAMPLE */

	/*
	 *	Time-of-day and time-out list are updated only
	 *	on the master CPU.
	 */
	if (my_cpu == master_cpu) {

	    register spl_t s;
	    register timer_elt_t	telt;
	    boolean_t	needsoft = FALSE;

#if	TS_FORMAT == 1
	    /*
	     *	Increment the tick count for the timestamping routine.
	     */
	    ts_tick_count++;
#endif	TS_FORMAT == 1

	    /*
	     *	Update the tick count since bootup, and handle
	     *	timeouts.
	     */

	    s = splsched();
	    simple_lock(&timer_lock);

	    elapsed_ticks++;

	    telt = (timer_elt_t)queue_first(&timer_head.chain);
	    if (telt->ticks <= elapsed_ticks)
		needsoft = TRUE;
	    simple_unlock(&timer_lock);
	    splx(s);

	    /*
	     *	Increment the time-of-day clock.
	     */
	    if (timedelta == 0) {
		time_value_add_usec(&time, usec);
	    }
	    else {
		register int	delta;

		if (timedelta < 0) {
		    delta = usec - tickdelta;
		    timedelta += tickdelta;
		}
		else {
		    delta = usec + tickdelta;
		    timedelta -= tickdelta;
		}
		time_value_add_usec(&time, delta);
	    }
	    update_mapped_time(&time);

	    /*
	     *	Schedule soft-interupt for timeout if needed
	     */
	    if (needsoft) {
		if (basepri) {
		    (void) splsoftclock();
		    softclock();
		}
		else {
		    setsoftclock();
		}
	    }
	}
}

/*
 *	There is a nasty race between softclock and reset_timeout.
 *	For example, scheduling code looks at timer_set and calls
 *	reset_timeout, thinking the timer is set.  However, softclock
 *	has already removed the timer but hasn't called thread_timeout
 *	yet.
 *
 *	Interim solution:  We initialize timers after pulling
 *	them out of the queue, so a race with reset_timeout won't
 *	hurt.  The timeout functions (eg, thread_timeout,
 *	thread_depress_timeout) check timer_set/depress_priority
 *	to see if the timer has been cancelled and if so do nothing.
 *
 *	This still isn't correct.  For example, softclock pulls a
 *	timer off the queue, then thread_go resets timer_set (but
 *	reset_timeout does nothing), then thread_set_timeout puts the
 *	timer back on the queue and sets timer_set, then
 *	thread_timeout finally runs and clears timer_set, then
 *	thread_set_timeout tries to put the timer on the queue again
 *	and corrupts it.
 */

void softclock()
{
	/*
	 *	Handle timeouts.
	 */
	spl_t	s;
	register timer_elt_t	telt;
	register int	(*fcn)();
	register char	*param;

	while (TRUE) {
	    s = splsched();
	    simple_lock(&timer_lock);
	    telt = (timer_elt_t) queue_first(&timer_head.chain);
	    if (telt->ticks > elapsed_ticks) {
		simple_unlock(&timer_lock);
		splx(s);
		break;
	    }
	    fcn = telt->fcn;
	    param = telt->param;

	    remqueue(&timer_head.chain, (queue_entry_t)telt);
	    telt->set = TELT_UNSET;
	    simple_unlock(&timer_lock);
	    splx(s);

	    assert(fcn != 0);
	    (*fcn)(param);
	}
}

/*
 *	Set timeout.
 *
 *	Parameters:
 *		telt	 timer element.  Function and param are already set.
 *		interval time-out interval, in hz.
 */
void set_timeout(telt, interval)
	register timer_elt_t	telt;	/* already loaded */
	register unsigned int	interval;
{
	spl_t			s;
	register timer_elt_t	next;

	s = splsched();
	simple_lock(&timer_lock);

	interval += elapsed_ticks;

	for (next = (timer_elt_t)queue_first(&timer_head.chain);
	     ;
	     next = (timer_elt_t)queue_next((queue_entry_t)next)) {

	    if (next->ticks > interval)
		break;
	}
	telt->ticks = interval;
	/*
	 * Insert new timer element before 'next'
	 * (after 'next'->prev)
	 */
	insque((queue_entry_t) telt, ((queue_entry_t)next)->prev);
	telt->set = TELT_SET;
	simple_unlock(&timer_lock);
	splx(s);
}

boolean_t reset_timeout(telt)
	register timer_elt_t	telt;
{
	spl_t	s;

	s = splsched();
	simple_lock(&timer_lock);
	if (telt->set) {
	    remqueue(&timer_head.chain, (queue_entry_t)telt);
	    telt->set = TELT_UNSET;
	    simple_unlock(&timer_lock);
	    splx(s);
	    return TRUE;
	}
	else {
	    simple_unlock(&timer_lock);
	    splx(s);
	    return FALSE;
	}
}

void init_timeout()
{
	simple_lock_init(&timer_lock);
	queue_init(&timer_head.chain);
	timer_head.ticks = ~0;	/* MAXUINT - sentinel */

	elapsed_ticks = 0;
}

/*
 * Read the time.
 */
kern_return_t
host_get_time(host, current_time)
	host_t		host;
	time_value_t	*current_time;	/* OUT */
{
	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

	do {
		current_time->seconds = mtime->seconds;
		current_time->microseconds = mtime->microseconds;
	} while (current_time->seconds != mtime->check_seconds);

	return (KERN_SUCCESS);
}

/*
 * Set the time.  Only available to privileged users.
 */
kern_return_t
host_set_time(host, new_time)
	host_t		host;
	time_value_t	new_time;
{
	spl_t	s;

	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

#if	NCPUS > 1
	/*
	 * Switch to the master CPU to synchronize correctly.
	 */
	thread_bind(current_thread(), master_processor);
	if (current_processor() != master_processor)
	    thread_block((void (*)) 0);
#endif	NCPUS > 1

	s = splhigh();
	time = new_time;
	update_mapped_time(&time);
	resettodr();
	splx(s);

#if	NCPUS > 1
	/*
	 * Switch off the master CPU.
	 */
	thread_bind(current_thread(), PROCESSOR_NULL);
#endif	NCPUS > 1

	return (KERN_SUCCESS);
}

/*
 * Adjust the time gradually.
 */
kern_return_t
host_adjust_time(host, new_adjustment, old_adjustment)
	host_t		host;
	time_value_t	new_adjustment;
	time_value_t	*old_adjustment;	/* OUT */
{
	time_value_t	oadj;
	unsigned int	ndelta;
	spl_t		s;

	if (host == HOST_NULL)
		return (KERN_INVALID_HOST);

	ndelta = new_adjustment.seconds * 1000000
		+ new_adjustment.microseconds;

#if	NCPUS > 1
	thread_bind(current_thread(), master_processor);
	if (current_processor() != master_processor)
	    thread_block((void (*)) 0);
#endif	NCPUS > 1

	s = splclock();

	oadj.seconds = timedelta / 1000000;
	oadj.microseconds = timedelta % 1000000;

	if (timedelta == 0) {
	    if (ndelta > bigadj)
		tickdelta = 10 * tickadj;
	    else
		tickdelta = tickadj;
	}
	if (ndelta % tickdelta)
	    ndelta = ndelta / tickdelta * tickdelta;

	timedelta = ndelta;

	splx(s);
#if	NCPUS > 1
	thread_bind(current_thread(), PROCESSOR_NULL);
#endif	NCPUS > 1

	*old_adjustment = oadj;

	return (KERN_SUCCESS);
}

void mapable_time_init()
{
	if (kmem_alloc_wired(kernel_map, (vm_offset_t *) &mtime, PAGE_SIZE)
						!= KERN_SUCCESS)
		panic("mapable_time_init");
	bzero((char *)mtime, PAGE_SIZE);
	update_mapped_time(&time);
}

int timeopen()
{
	return(0);
}
int timeclose()
{
	return(0);
}

/*
 *	Compatibility for device drivers.
 *	New code should use set_timeout/reset_timeout and private timers.
 *	These code can't use a zone to allocate timers, because
 *	it can be called from interrupt handlers.
 */

#define NTIMERS		20

timer_elt_data_t timeout_timers[NTIMERS];

/*
 *	Set timeout.
 *
 *	fcn:		function to call
 *	param:		parameter to pass to function
 *	interval:	timeout interval, in hz.
 */
void timeout(fcn, param, interval)
	int	(*fcn)(/* char * param */);
	char *	param;
	int	interval;
{
	spl_t	s;
	register timer_elt_t elt;

	s = splsched();
	simple_lock(&timer_lock);
	for (elt = &timeout_timers[0]; elt < &timeout_timers[NTIMERS]; elt++)
	    if (elt->set == TELT_UNSET)
		break;
	if (elt == &timeout_timers[NTIMERS])
	    panic("timeout");
	elt->fcn = fcn;
	elt->param = param;
	elt->set = TELT_ALLOC;
	simple_unlock(&timer_lock);
	splx(s);

	set_timeout(elt, (unsigned int)interval);
}

/*
 * Returns a boolean indicating whether the timeout element was found
 * and removed.
 */
boolean_t untimeout(fcn, param)
	register int	(*fcn)();
	register char *	param;
{
	spl_t	s;
	register timer_elt_t elt;

	s = splsched();
	simple_lock(&timer_lock);
	queue_iterate(&timer_head.chain, elt, timer_elt_t, chain) {

	    if ((fcn == elt->fcn) && (param == elt->param)) {
		/*
		 *	Found it.
		 */
		remqueue(&timer_head.chain, (queue_entry_t)elt);
		elt->set = TELT_UNSET;

		simple_unlock(&timer_lock);
		splx(s);
		return (TRUE);
	    }
	}
	simple_unlock(&timer_lock);
	splx(s);
	return (FALSE);
}
