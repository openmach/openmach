/* 
 * Mach Operating System
 * Copyright (c) 1993-1987 Carnegie Mellon University
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
 *	File:	sched_prim.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1986
 *
 *	Scheduling primitives
 *
 */

#include <cpus.h>
#include <simple_clock.h>
#include <mach_fixpri.h>
#include <mach_host.h>
#include <hw_footprint.h>
#include <fast_tas.h>
#include <power_save.h>

#include <mach/machine.h>
#include <kern/ast.h>
#include <kern/counters.h>
#include <kern/cpu_number.h>
#include <kern/lock.h>
#include <kern/macro_help.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/syscall_subr.h>
#include <kern/thread.h>
#include <kern/thread_swap.h>
#include <kern/time_out.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <machine/machspl.h>	/* For def'n of splsched() */

#if	MACH_FIXPRI
#include <mach/policy.h>
#endif	/* MACH_FIXPRI */


extern int hz;

int		min_quantum;	/* defines max context switch rate */

unsigned	sched_tick;

#if	SIMPLE_CLOCK
int		sched_usec;
#endif	/* SIMPLE_CLOCK */

thread_t	sched_thread_id;

void recompute_priorities(void);	/* forward */
void update_priority(thread_t);
void set_pri(thread_t, int, boolean_t);
void do_thread_scan(void);

thread_t	choose_pset_thread();

timer_elt_data_t recompute_priorities_timer;

#if	DEBUG
void checkrq(run_queue_t, char *);
void thread_check(thread_t, run_queue_t);
#endif

/*
 *	State machine
 *
 * states are combinations of:
 *  R	running
 *  W	waiting (or on wait queue)
 *  S	suspended (or will suspend)
 *  N	non-interruptible
 *
 * init	action 
 *	assert_wait thread_block    clear_wait	suspend	resume
 *
 * R	RW,  RWN    R;   setrun	    -		RS	-
 * RS	RWS, RWNS   S;  wake_active -		-	R
 * RN	RWN	    RN;  setrun	    -		RNS	-
 * RNS	RWNS	    RNS; setrun	    -		-	RN
 *
 * RW		    W		    R		RWS	-
 * RWN		    WN		    RN		RWNS	-
 * RWS		    WS; wake_active RS		-	RW
 * RWNS		    WNS		    RNS		-	RWN
 *
 * W				    R;   setrun	WS	-
 * WN				    RN;  setrun	WNS	-
 * WNS				    RNS; setrun	-	WN
 *
 * S				    -		-	R
 * WS				    S		-	W
 *
 */

/*
 *	Waiting protocols and implementation:
 *
 *	Each thread may be waiting for exactly one event; this event
 *	is set using assert_wait().  That thread may be awakened either
 *	by performing a thread_wakeup_prim() on its event,
 *	or by directly waking that thread up with clear_wait().
 *
 *	The implementation of wait events uses a hash table.  Each
 *	bucket is queue of threads having the same hash function
 *	value; the chain for the queue (linked list) is the run queue
 *	field.  [It is not possible to be waiting and runnable at the
 *	same time.]
 *
 *	Locks on both the thread and on the hash buckets govern the
 *	wait event field and the queue chain field.  Because wakeup
 *	operations only have the event as an argument, the event hash
 *	bucket must be locked before any thread.
 *
 *	Scheduling operations may also occur at interrupt level; therefore,
 *	interrupts below splsched() must be prevented when holding
 *	thread or hash bucket locks.
 *
 *	The wait event hash table declarations are as follows:
 */

#define NUMQUEUES	59

queue_head_t		wait_queue[NUMQUEUES];
decl_simple_lock_data(,	wait_lock[NUMQUEUES])

/* NOTE: we want a small positive integer out of this */
#define wait_hash(event) \
	((((int)(event) < 0) ? ~(int)(event) : (int)(event)) % NUMQUEUES)

void wait_queue_init(void)
{
	register int i;

	for (i = 0; i < NUMQUEUES; i++) {
		queue_init(&wait_queue[i]);
		simple_lock_init(&wait_lock[i]);
	}
}

void sched_init(void)
{
	recompute_priorities_timer.fcn = (int (*)())recompute_priorities;
	recompute_priorities_timer.param = (char *)0;

	min_quantum = hz / 10;		/* context switch 10 times/second */
	wait_queue_init();
	pset_sys_bootstrap();		/* initialize processer mgmt. */
	queue_init(&action_queue);
	simple_lock_init(&action_lock);
	sched_tick = 0;
#if	SIMPLE_CLOCK
	sched_usec = 0;
#endif	/* SIMPLE_CLOCK */
	ast_init();
}

/*
 *	Thread timeout routine, called when timer expires.
 *	Called at splsoftclock.
 */
void thread_timeout(
	thread_t thread)
{
	assert(thread->timer.set == TELT_UNSET);

	clear_wait(thread, THREAD_TIMED_OUT, FALSE);
}

/*
 *	thread_set_timeout:
 *
 *	Set a timer for the current thread, if the thread
 *	is ready to wait.  Must be called between assert_wait()
 *	and thread_block().
 */
 
void thread_set_timeout(
	int	t)	/* timeout interval in ticks */
{
	register thread_t	thread = current_thread();
	register spl_t s;

	s = splsched();
	thread_lock(thread);
	if ((thread->state & TH_WAIT) != 0) {
		set_timeout(&thread->timer, t);
	}
	thread_unlock(thread);
	splx(s);
}

/*
 * Set up thread timeout element when thread is created.
 */
void thread_timeout_setup(
	register thread_t	thread)
{
	thread->timer.fcn = (int (*)())thread_timeout;
	thread->timer.param = (char *)thread;
	thread->depress_timer.fcn = (int (*)())thread_depress_timeout;
	thread->depress_timer.param = (char *)thread;
}

/*
 *	assert_wait:
 *
 *	Assert that the current thread is about to go to
 *	sleep until the specified event occurs.
 */
void assert_wait(
	event_t		event,
	boolean_t	interruptible)
{
	register queue_t	q;
	register int		index;
	register thread_t	thread;
#if	MACH_SLOCKS
	register simple_lock_t	lock;
#endif	/* MACH_SLOCKS */
	spl_t			s;

	thread = current_thread();
	if (thread->wait_event != 0) {
		panic("assert_wait: already asserted event %#x\n",
		      thread->wait_event);
	}
 	s = splsched();
	if (event != 0) {
		index = wait_hash(event);
		q = &wait_queue[index];
#if	MACH_SLOCKS
		lock = &wait_lock[index];
#endif	/* MACH_SLOCKS */
		simple_lock(lock);
		thread_lock(thread);
		enqueue_tail(q, (queue_entry_t) thread);
		thread->wait_event = event;
		if (interruptible)
			thread->state |= TH_WAIT;
		else
			thread->state |= TH_WAIT | TH_UNINT;
		thread_unlock(thread);
		simple_unlock(lock);
	}
	else {
		thread_lock(thread);
		if (interruptible)
			thread->state |= TH_WAIT;
		else
			thread->state |= TH_WAIT | TH_UNINT;
		thread_unlock(thread);
	}
	splx(s);
}

/*
 *	clear_wait:
 *
 *	Clear the wait condition for the specified thread.  Start the thread
 *	executing if that is appropriate.
 *
 *	parameters:
 *	  thread		thread to awaken
 *	  result		Wakeup result the thread should see
 *	  interrupt_only	Don't wake up the thread if it isn't
 *				interruptible.
 */
void clear_wait(
	register thread_t	thread,
	int			result,
	boolean_t		interrupt_only)
{
	register int		index;
	register queue_t	q;
#if	MACH_SLOCKS
	register simple_lock_t	lock;
#endif	/* MACH_SLOCKS */
	register event_t	event;
	spl_t			s;

	s = splsched();
	thread_lock(thread);
	if (interrupt_only && (thread->state & TH_UNINT)) {
		/*
		 *	can`t interrupt thread
		 */
		thread_unlock(thread);
		splx(s);
		return;
	}

	event = thread->wait_event;
	if (event != 0) {
		thread_unlock(thread);
		index = wait_hash(event);
		q = &wait_queue[index];
#if	MACH_SLOCKS
		lock = &wait_lock[index];
#endif	/* MACH_SLOCKS */
		simple_lock(lock);
		/*
		 *	If the thread is still waiting on that event,
		 *	then remove it from the list.  If it is waiting
		 *	on a different event, or no event at all, then
		 *	someone else did our job for us.
		 */
		thread_lock(thread);
		if (thread->wait_event == event) {
			remqueue(q, (queue_entry_t)thread);
			thread->wait_event = 0;
			event = 0;		/* cause to run below */
		}
		simple_unlock(lock);
	}
	if (event == 0) {
		register int	state = thread->state;

		reset_timeout_check(&thread->timer);

		switch (state & TH_SCHED_STATE) {
		    case	  TH_WAIT | TH_SUSP | TH_UNINT:
		    case	  TH_WAIT	    | TH_UNINT:
		    case	  TH_WAIT:
			/*
			 *	Sleeping and not suspendable - put
			 *	on run queue.
			 */
			thread->state = (state &~ TH_WAIT) | TH_RUN;
			thread->wait_result = result;
			thread_setrun(thread, TRUE);
			break;

		    case	  TH_WAIT | TH_SUSP:
		    case TH_RUN | TH_WAIT:
		    case TH_RUN | TH_WAIT | TH_SUSP:
		    case TH_RUN | TH_WAIT	    | TH_UNINT:
		    case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:
			/*
			 *	Either already running, or suspended.
			 */
			thread->state = state &~ TH_WAIT;
			thread->wait_result = result;
			break;

		    default:
			/*
			 *	Not waiting.
			 */
			break;
		}
	}
	thread_unlock(thread);
	splx(s);
}

/*
 *	thread_wakeup_prim:
 *
 *	Common routine for thread_wakeup, thread_wakeup_with_result,
 *	and thread_wakeup_one.
 *
 */
void thread_wakeup_prim(
	event_t		event,
	boolean_t	one_thread,
	int		result)
{
	register queue_t	q;
	register int		index;
	register thread_t	thread, next_th;
#if	MACH_SLOCKS
	register simple_lock_t	lock;
#endif  /* MACH_SLOCKS */
	spl_t			s;
	register int		state;

	index = wait_hash(event);
	q = &wait_queue[index];
	s = splsched();
#if	MACH_SLOCKS
	lock = &wait_lock[index];
#endif	/* MACH_SLOCKS */
	simple_lock(lock);
	thread = (thread_t) queue_first(q);
	while (!queue_end(q, (queue_entry_t)thread)) {
		next_th = (thread_t) queue_next((queue_t) thread);

		if (thread->wait_event == event) {
			thread_lock(thread);
			remqueue(q, (queue_entry_t) thread);
			thread->wait_event = 0;
			reset_timeout_check(&thread->timer);

			state = thread->state;
			switch (state & TH_SCHED_STATE) {

			    case	  TH_WAIT | TH_SUSP | TH_UNINT:
			    case	  TH_WAIT	    | TH_UNINT:
			    case	  TH_WAIT:
				/*
				 *	Sleeping and not suspendable - put
				 *	on run queue.
				 */
				thread->state = (state &~ TH_WAIT) | TH_RUN;
				thread->wait_result = result;
				thread_setrun(thread, TRUE);
				break;

			    case	  TH_WAIT | TH_SUSP:
			    case TH_RUN | TH_WAIT:
			    case TH_RUN | TH_WAIT | TH_SUSP:
			    case TH_RUN | TH_WAIT	    | TH_UNINT:
			    case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:
				/*
				 *	Either already running, or suspended.
				 */
				thread->state = state &~ TH_WAIT;
				thread->wait_result = result;
				break;

			    default:
				panic("thread_wakeup");
				break;
			}
			thread_unlock(thread);
			if (one_thread)
				break;
		}
		thread = next_th;
	}
	simple_unlock(lock);
	splx(s);
}

/*
 *	thread_sleep:
 *
 *	Cause the current thread to wait until the specified event
 *	occurs.  The specified lock is unlocked before releasing
 *	the cpu.  (This is a convenient way to sleep without manually
 *	calling assert_wait).
 */
void thread_sleep(
	event_t		event,
	simple_lock_t	lock,
	boolean_t	interruptible)
{
	assert_wait(event, interruptible);	/* assert event */
	simple_unlock(lock);			/* release the lock */
	thread_block((void (*)()) 0);		/* block ourselves */
}

/*
 *	thread_bind:
 *
 *	Force a thread to execute on the specified processor.
 *	If the thread is currently executing, it may wait until its
 *	time slice is up before switching onto the specified processor.
 *
 *	A processor of PROCESSOR_NULL causes the thread to be unbound.
 *	xxx - DO NOT export this to users.
 */
void thread_bind(
	register thread_t	thread,
	processor_t		processor)
{
	spl_t		s;

	s = splsched();
	thread_lock(thread);
	thread->bound_processor = processor;
	thread_unlock(thread);
	(void) splx(s);
}

/*
 *	Select a thread for this processor (the current processor) to run.
 *	May select the current thread.
 *	Assumes splsched.
 */

thread_t thread_select(
	register processor_t myprocessor)
{
	register thread_t thread;

	myprocessor->first_quantum = TRUE;
	/*
	 *	Check for obvious simple case; local runq is
	 *	empty and global runq has entry at hint.
	 */
	if (myprocessor->runq.count > 0) {
		thread = choose_thread(myprocessor);
		myprocessor->quantum = min_quantum;
	}
	else {
		register processor_set_t pset;

#if	MACH_HOST
		pset = myprocessor->processor_set;
#else	/* MACH_HOST */
		pset = &default_pset;
#endif	/* MACH_HOST */
		simple_lock(&pset->runq.lock);
#if	DEBUG
		checkrq(&pset->runq, "thread_select");
#endif	/* DEBUG */
		if (pset->runq.count == 0) {
			/*
			 *	Nothing else runnable.  Return if this
			 *	thread is still runnable on this processor.
			 *	Check for priority update if required.
			 */
			thread = current_thread();
			if ((thread->state == TH_RUN) &&
#if	MACH_HOST
			    (thread->processor_set == pset) &&
#endif	/* MACH_HOST */
			    ((thread->bound_processor == PROCESSOR_NULL) ||
			     (thread->bound_processor == myprocessor))) {

				simple_unlock(&pset->runq.lock);
				thread_lock(thread);
				if (thread->sched_stamp != sched_tick)
				    update_priority(thread);
				thread_unlock(thread);
			}
			else {
				thread = choose_pset_thread(myprocessor, pset);
			}
		}
		else {
			register queue_t	q;

			/*
			 *	If there is a thread at hint, grab it,
			 *	else call choose_pset_thread.
			 */
			q = pset->runq.runq + pset->runq.low;

			if (queue_empty(q)) {
				pset->runq.low++;
				thread = choose_pset_thread(myprocessor, pset);
			}
			else {
				thread = (thread_t) dequeue_head(q);
				thread->runq = RUN_QUEUE_NULL;
				pset->runq.count--;
#if	MACH_FIXPRI
				/*
				 *	Cannot lazy evaluate pset->runq.low for
				 *	fixed priority policy
				 */
				if ((pset->runq.count > 0) &&
				    (pset->policies & POLICY_FIXEDPRI)) {
					    while (queue_empty(q)) {
						pset->runq.low++;
						q++;
					    }
				}
#endif	/* MACH_FIXPRI */
#if	DEBUG
				checkrq(&pset->runq, "thread_select: after");
#endif	/* DEBUG */
				simple_unlock(&pset->runq.lock);
			}
		}

#if	MACH_FIXPRI
		if (thread->policy == POLICY_TIMESHARE) {
#endif	/* MACH_FIXPRI */
			myprocessor->quantum = pset->set_quantum;
#if	MACH_FIXPRI
		}
		else {
			/*
			 *	POLICY_FIXEDPRI
			 */
			myprocessor->quantum = thread->sched_data;
		}
#endif	/* MACH_FIXPRI */
	}

	return thread;
}

/*
 *	Stop running the current thread and start running the new thread.
 *	If continuation is non-zero, and the current thread is blocked,
 *	then it will resume by executing continuation on a new stack.
 *	Returns TRUE if the hand-off succeeds.
 *	Assumes splsched.
 */

boolean_t thread_invoke(
	register thread_t old_thread,
	continuation_t	  continuation,
	register thread_t new_thread)
{
	/*
	 *	Check for invoking the same thread.
	 */
	if (old_thread == new_thread) {
	    /*
	     *	Mark thread interruptible.
	     *	Run continuation if there is one.
	     */
	    thread_lock(new_thread);
	    new_thread->state &= ~TH_UNINT;
	    thread_unlock(new_thread);

	    if (continuation != (void (*)()) 0) {
		(void) spl0();
		call_continuation(continuation);
		/*NOTREACHED*/
	    }
	    return TRUE;
	}

	/*
	 *	Check for stack-handoff.
	 */
	thread_lock(new_thread);
	if ((old_thread->stack_privilege != current_stack()) &&
	    (continuation != (void (*)()) 0))
	{
	    switch (new_thread->state & TH_SWAP_STATE) {
		case TH_SWAPPED:

		    new_thread->state &= ~(TH_SWAPPED | TH_UNINT);
		    thread_unlock(new_thread);

#if	NCPUS > 1
		    new_thread->last_processor = current_processor();
#endif	/* NCPUS > 1 */

		    /*
		     *	Set up ast context of new thread and
		     *	switch to its timer.
		     */
		    ast_context(new_thread, cpu_number());
		    timer_switch(&new_thread->system_timer);

		    stack_handoff(old_thread, new_thread);

		    /*
		     *	We can dispatch the old thread now.
		     *	This is like thread_dispatch, except
		     *	that the old thread is left swapped
		     *	*without* freeing its stack.
		     *	This path is also much more frequent
		     *	than actual calls to thread_dispatch.
		     */

		    thread_lock(old_thread);
		    old_thread->swap_func = continuation;

		    switch (old_thread->state) {
			case TH_RUN	      | TH_SUSP:
			case TH_RUN	      | TH_SUSP | TH_HALTED:
			case TH_RUN | TH_WAIT | TH_SUSP:
			    /*
			     *	Suspend the thread
			     */
			    old_thread->state = (old_thread->state & ~TH_RUN)
						| TH_SWAPPED;
			    if (old_thread->wake_active) {
				old_thread->wake_active = FALSE;
				thread_unlock(old_thread);
				thread_wakeup((event_t)&old_thread->wake_active);

				goto after_old_thread;
			    }
			    break;

			case TH_RUN	      | TH_SUSP | TH_UNINT:
			case TH_RUN			| TH_UNINT:
			case TH_RUN:
			    /*
			     *	We can`t suspend the thread yet,
			     *	or it`s still running.
			     *	Put back on a run queue.
			     */
			    old_thread->state |= TH_SWAPPED;
			    thread_setrun(old_thread, FALSE);
			    break;

			case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:
			case TH_RUN | TH_WAIT		| TH_UNINT:
			case TH_RUN | TH_WAIT:
			    /*
			     *	Waiting, and not suspendable.
			     */
			    old_thread->state = (old_thread->state & ~TH_RUN)
						| TH_SWAPPED;
			    break;

			case TH_RUN | TH_IDLE:
			    /*
			     *	Drop idle thread -- it is already in
			     *	idle_thread_array.
			     */
			    old_thread->state = TH_RUN | TH_IDLE | TH_SWAPPED;
			    break;

			default:
			    panic("thread_invoke");
		    }
		    thread_unlock(old_thread);
		after_old_thread:

		    /*
		     *	call_continuation calls the continuation
		     *	after resetting the current stack pointer
		     *	to recover stack space.  If we called
		     *	the continuation directly, we would risk
		     *	running out of stack.
		     */

		    counter_always(c_thread_invoke_hits++);
		    (void) spl0();
		    call_continuation(new_thread->swap_func);
		    /*NOTREACHED*/
		    return TRUE; /* help for the compiler */

		case TH_SW_COMING_IN:
		    /*
		     *	Waiting for a stack
		     */
		    thread_swapin(new_thread);
		    thread_unlock(new_thread);
		    counter_always(c_thread_invoke_misses++);
		    return FALSE;

		case 0:
		    /*
		     *	Already has a stack - can`t handoff.
		     */
		    break;
	    }
	}

	else {
	    /*
	     *	Check that the thread is swapped-in.
	     */
	    if (new_thread->state & TH_SWAPPED) {
		if ((new_thread->state & TH_SW_COMING_IN) ||
		    !stack_alloc_try(new_thread, thread_continue))
		{
		    thread_swapin(new_thread);
		    thread_unlock(new_thread);
		    counter_always(c_thread_invoke_misses++);
		    return FALSE;
		}
	    }
	}

	new_thread->state &= ~(TH_SWAPPED | TH_UNINT);
	thread_unlock(new_thread);

	/*
	 *	Thread is now interruptible.
	 */
#if	NCPUS > 1
	new_thread->last_processor = current_processor();
#endif	/* NCPUS > 1 */

	/*
	 *	Set up ast context of new thread and switch to its timer.
	 */
	ast_context(new_thread, cpu_number());
	timer_switch(&new_thread->system_timer);

	/*
	 *	switch_context is machine-dependent.  It does the
	 *	machine-dependent components of a context-switch, like
	 *	changing address spaces.  It updates active_threads.
	 *	It returns only if a continuation is not supplied.
	 */
	counter_always(c_thread_invoke_csw++);
	old_thread = switch_context(old_thread, continuation, new_thread);

	/*
	 *	We're back.  Now old_thread is the thread that resumed
	 *	us, and we have to dispatch it.
	 */
	thread_dispatch(old_thread);

	return TRUE;
}

/*
 *	thread_continue:
 *
 *	Called when the current thread is given a new stack.
 *	Called at splsched.
 */
void thread_continue(
	register thread_t old_thread)
{
	register continuation_t	continuation = current_thread()->swap_func;

	/*
	 *	We must dispatch the old thread and then
	 *	call the current thread's continuation.
	 *	There might not be an old thread, if we are
	 *	the first thread to run on this processor.
	 */

	if (old_thread != THREAD_NULL)
		thread_dispatch(old_thread);
	(void) spl0();
	(*continuation)();
	/*NOTREACHED*/
}


/*
 *	thread_block:
 *
 *	Block the current thread.  If the thread is runnable
 *	then someone must have woken it up between its request
 *	to sleep and now.  In this case, it goes back on a
 *	run queue.
 *
 *	If a continuation is specified, then thread_block will
 *	attempt to discard the thread's kernel stack.  When the
 *	thread resumes, it will execute the continuation function
 *	on a new kernel stack.
 */

void thread_block(
	continuation_t	continuation)
{
	register thread_t thread = current_thread();
	register processor_t myprocessor = cpu_to_processor(cpu_number());
	register thread_t new_thread;
	spl_t s;

	check_simple_locks();

	s = splsched();

#if	FAST_TAS
	{
		extern void recover_ras();

		if (csw_needed(thread, myprocessor))
			recover_ras(thread);
	}
#endif	/* FAST_TAS */
	
	ast_off(cpu_number(), AST_BLOCK);

	do
		new_thread = thread_select(myprocessor);
	while (!thread_invoke(thread, continuation, new_thread));

	splx(s);
}

/*
 *	thread_run:
 *
 *	Switch directly from the current thread to a specified
 *	thread.  Both the current and new threads must be
 *	runnable.
 *
 *	If a continuation is specified, then thread_block will
 *	attempt to discard the current thread's kernel stack.  When the
 *	thread resumes, it will execute the continuation function
 *	on a new kernel stack.
 */
void thread_run(
	continuation_t		continuation,
	register thread_t	new_thread)
{
	register thread_t thread = current_thread();
	register processor_t myprocessor = cpu_to_processor(cpu_number());
	spl_t s;

	check_simple_locks();

	s = splsched();

	while (!thread_invoke(thread, continuation, new_thread))
		new_thread = thread_select(myprocessor);

	splx(s);
}

/*
 *	Dispatches a running thread that is not	on a runq.
 *	Called at splsched.
 */

void thread_dispatch(
	register thread_t	thread)
{
	/*
	 *	If we are discarding the thread's stack, we must do it
	 *	before the thread has a chance to run.
	 */

	thread_lock(thread);

	if (thread->swap_func != (void (*)()) 0) {
		assert((thread->state & TH_SWAP_STATE) == 0);
		thread->state |= TH_SWAPPED;
		stack_free(thread);
	}

	switch (thread->state &~ TH_SWAP_STATE) {
	    case TH_RUN		  | TH_SUSP:
	    case TH_RUN		  | TH_SUSP | TH_HALTED:
	    case TH_RUN | TH_WAIT | TH_SUSP:
		/*
		 *	Suspend the thread
		 */
		thread->state &= ~TH_RUN;
		if (thread->wake_active) {
		    thread->wake_active = FALSE;
		    thread_unlock(thread);
		    thread_wakeup((event_t)&thread->wake_active);
		    return;
		}
		break;

	    case TH_RUN		  | TH_SUSP | TH_UNINT:
	    case TH_RUN			    | TH_UNINT:
	    case TH_RUN:
		/*
		 *	No reason to stop.  Put back on a run queue.
		 */
		thread_setrun(thread, FALSE);
		break;

	    case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:
	    case TH_RUN | TH_WAIT	    | TH_UNINT:
	    case TH_RUN | TH_WAIT:
		/*
		 *	Waiting, and not suspended.
		 */
		thread->state &= ~TH_RUN;
		break;

	    case TH_RUN | TH_IDLE:
		/*
		 *	Drop idle thread -- it is already in
		 *	idle_thread_array.
		 */
		break;

	    default:
		panic("thread_dispatch");
	}
	thread_unlock(thread);
}


/*
 *	Define shifts for simulating (5/8)**n
 */

shift_data_t	wait_shift[32] = {
	{1,1},{1,3},{1,-3},{2,-7},{3,5},{3,-5},{4,-8},{5,7},
	{5,-7},{6,-10},{7,10},{7,-9},{8,-11},{9,12},{9,-11},{10,-13},
	{11,14},{11,-13},{12,-15},{13,17},{13,-15},{14,-17},{15,19},{16,18},
	{16,-19},{17,22},{18,20},{18,-20},{19,26},{20,22},{20,-22},{21,-27}};

/*
 *	do_priority_computation:
 *
 *	Calculate new priority for thread based on its base priority plus
 *	accumulated usage.  PRI_SHIFT and PRI_SHIFT_2 convert from
 *	usage to priorities.  SCHED_SHIFT converts for the scaling
 *	of the sched_usage field by SCHED_SCALE.  This scaling comes
 *	from the multiplication by sched_load (thread_timer_delta)
 *	in sched.h.  sched_load is calculated as a scaled overload
 *	factor in compute_mach_factor (mach_factor.c).
 */

#ifdef	PRI_SHIFT_2
#if	PRI_SHIFT_2 > 0
#define do_priority_computation(th, pri)				\
	MACRO_BEGIN							\
	(pri) = (th)->priority	/* start with base priority */		\
	    + ((th)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT))		\
	    + ((th)->sched_usage >> (PRI_SHIFT_2 + SCHED_SHIFT));	\
	if ((pri) > 31) (pri) = 31;					\
	MACRO_END
#else	/* PRI_SHIFT_2 */
#define do_priority_computation(th, pri)				\
	MACRO_BEGIN							\
	(pri) = (th)->priority	/* start with base priority */		\
	    + ((th)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT))		\
	    - ((th)->sched_usage >> (SCHED_SHIFT - PRI_SHIFT_2));	\
	if ((pri) > 31) (pri) = 31;					\
	MACRO_END
#endif	/* PRI_SHIFT_2 */
#else	/* defined(PRI_SHIFT_2) */
#define do_priority_computation(th, pri)				\
	MACRO_BEGIN							\
	(pri) = (th)->priority	/* start with base priority */		\
	    + ((th)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT));		\
	if ((pri) > 31) (pri) = 31;					\
	MACRO_END
#endif	/* defined(PRI_SHIFT_2) */

/*
 *	compute_priority:
 *
 *	Compute the effective priority of the specified thread.
 *	The effective priority computation is as follows:
 *
 *	Take the base priority for this thread and add
 *	to it an increment derived from its cpu_usage.
 *
 *	The thread *must* be locked by the caller. 
 */

void compute_priority(
	register thread_t	thread,
	boolean_t		resched)
{
	register int	pri;

#if	MACH_FIXPRI
	if (thread->policy == POLICY_TIMESHARE) {
#endif	/* MACH_FIXPRI */
	    do_priority_computation(thread, pri);
	    if (thread->depress_priority < 0)
		set_pri(thread, pri, resched);
	    else
		thread->depress_priority = pri;
#if	MACH_FIXPRI
	}
	else {
	    set_pri(thread, thread->priority, resched);
	}
#endif	/* MACH_FIXPRI */
}

/*
 *	compute_my_priority:
 *
 *	Version of compute priority for current thread or thread
 *	being manipulated by scheduler (going on or off a runq).
 *	Only used for priority updates.  Policy or priority changes
 *	must call compute_priority above.  Caller must have thread
 *	locked and know it is timesharing and not depressed.
 */

void compute_my_priority(
	register thread_t	thread)
{
	register int temp_pri;

	do_priority_computation(thread,temp_pri);
	thread->sched_pri = temp_pri;
}

/*
 *	recompute_priorities:
 *
 *	Update the priorities of all threads periodically.
 */
void recompute_priorities(void)
{
#if	SIMPLE_CLOCK
	int	new_usec;
#endif	/* SIMPLE_CLOCK */

	sched_tick++;		/* age usage one more time */
	set_timeout(&recompute_priorities_timer, hz);
#if	SIMPLE_CLOCK
	/*
	 *	Compensate for clock drift.  sched_usec is an
	 *	exponential average of the number of microseconds in
	 *	a second.  It decays in the same fashion as cpu_usage.
	 */
	new_usec = sched_usec_elapsed();
	sched_usec = (5*sched_usec + 3*new_usec)/8;
#endif	/* SIMPLE_CLOCK */
	/*
	 *	Wakeup scheduler thread.
	 */
	if (sched_thread_id != THREAD_NULL) {
		clear_wait(sched_thread_id, THREAD_AWAKENED, FALSE);
	}
}

/*
 *	update_priority
 *
 *	Cause the priority computation of a thread that has been 
 *	sleeping or suspended to "catch up" with the system.  Thread
 *	*MUST* be locked by caller.  If thread is running, then this
 *	can only be called by the thread on itself.
 */
void update_priority(
	register thread_t	thread)
{
	register unsigned int	ticks;
	register shift_t	shiftp;
	register int		temp_pri;

	ticks = sched_tick - thread->sched_stamp;

	assert(ticks != 0);

	/*
	 *	If asleep for more than 30 seconds forget all
	 *	cpu_usage, else catch up on missed aging.
	 *	5/8 ** n is approximated by the two shifts
	 *	in the wait_shift array.
	 */
	thread->sched_stamp += ticks;
	thread_timer_delta(thread);
	if (ticks >  30) {
		thread->cpu_usage = 0;
		thread->sched_usage = 0;
	}
	else {
		thread->cpu_usage += thread->cpu_delta;
		thread->sched_usage += thread->sched_delta;
		shiftp = &wait_shift[ticks];
		if (shiftp->shift2 > 0) {
		    thread->cpu_usage =
			(thread->cpu_usage >> shiftp->shift1) +
			(thread->cpu_usage >> shiftp->shift2);
		    thread->sched_usage =
			(thread->sched_usage >> shiftp->shift1) +
			(thread->sched_usage >> shiftp->shift2);
		}
		else {
		    thread->cpu_usage =
			(thread->cpu_usage >> shiftp->shift1) -
			(thread->cpu_usage >> -(shiftp->shift2));
		    thread->sched_usage =
			(thread->sched_usage >> shiftp->shift1) -
			(thread->sched_usage >> -(shiftp->shift2));
		}
	}
	thread->cpu_delta = 0;
	thread->sched_delta = 0;
	/*
	 *	Recompute priority if appropriate.
	 */
	if (
#if	MACH_FIXPRI
	    (thread->policy == POLICY_TIMESHARE) &&
#endif	/* MACH_FIXPRI */
	    (thread->depress_priority < 0)) {
		do_priority_computation(thread, temp_pri);
		thread->sched_pri = temp_pri;
	}
}

/*
 *	run_queue_enqueue macro for thread_setrun().
 */
#if	DEBUG
#define run_queue_enqueue(rq, th)					\
	MACRO_BEGIN							\
	    register unsigned int	whichq;				\
									\
	    whichq = (th)->sched_pri;					\
	    if (whichq >= NRQS) {					\
	printf("thread_setrun: pri too high (%d)\n", (th)->sched_pri);  \
		whichq = NRQS - 1;					\
	    }								\
									\
	    simple_lock(&(rq)->lock);	/* lock the run queue */	\
	    checkrq((rq), "thread_setrun: before adding thread");	\
	    enqueue_tail(&(rq)->runq[whichq], (queue_entry_t) (th));	\
									\
	    if (whichq < (rq)->low || (rq)->count == 0) 		\
		 (rq)->low = whichq;	/* minimize */			\
									\
	    (rq)->count++;						\
	    (th)->runq = (rq);						\
	    thread_check((th), (rq));					\
	    checkrq((rq), "thread_setrun: after adding thread");	\
	    simple_unlock(&(rq)->lock);					\
	MACRO_END
#else	/* DEBUG */
#define run_queue_enqueue(rq, th)					\
	MACRO_BEGIN							\
	    register unsigned int	whichq;				\
									\
	    whichq = (th)->sched_pri;					\
	    if (whichq >= NRQS) {					\
	printf("thread_setrun: pri too high (%d)\n", (th)->sched_pri);  \
		whichq = NRQS - 1;					\
	    }								\
									\
	    simple_lock(&(rq)->lock);	/* lock the run queue */	\
	    enqueue_tail(&(rq)->runq[whichq], (queue_entry_t) (th));	\
									\
	    if (whichq < (rq)->low || (rq)->count == 0) 		\
		 (rq)->low = whichq;	/* minimize */			\
									\
	    (rq)->count++;						\
	    (th)->runq = (rq);						\
	    simple_unlock(&(rq)->lock);					\
	MACRO_END
#endif	/* DEBUG */
/*
 *	thread_setrun:
 *
 *	Make thread runnable; dispatch directly onto an idle processor
 *	if possible.  Else put on appropriate run queue (processor
 *	if bound, else processor set.  Caller must have lock on thread.
 *	This is always called at splsched.
 */

void thread_setrun(
	register thread_t	th,
	boolean_t		may_preempt)
{
	register processor_t	processor;
	register run_queue_t	rq;
#if	NCPUS > 1
	register processor_set_t	pset;
#endif	/* NCPUS > 1 */

	/*
	 *	Update priority if needed.
	 */
	if (th->sched_stamp != sched_tick) {
		update_priority(th);
	}

	assert(th->runq == RUN_QUEUE_NULL);

#if	NCPUS > 1
	/*
	 *	Try to dispatch the thread directly onto an idle processor.
	 */
	if ((processor = th->bound_processor) == PROCESSOR_NULL) {
	    /*
	     *	Not bound, any processor in the processor set is ok.
	     */
	    pset = th->processor_set;
#if	HW_FOOTPRINT
	    /*
	     *	But first check the last processor it ran on.
	     */
	    processor = th->last_processor;
	    if (processor->state == PROCESSOR_IDLE) {
		    simple_lock(&processor->lock);
		    simple_lock(&pset->idle_lock);
		    if ((processor->state == PROCESSOR_IDLE)
#if	MACH_HOST
			&& (processor->processor_set == pset)
#endif	/* MACH_HOST */
			) {
			    queue_remove(&pset->idle_queue, processor,
			        processor_t, processor_queue);
			    pset->idle_count--;
			    processor->next_thread = th;
			    processor->state = PROCESSOR_DISPATCHING;
			    simple_unlock(&pset->idle_lock);
			    simple_unlock(&processor->lock);
		            return;
		    }
		    simple_unlock(&pset->idle_lock);
		    simple_unlock(&processor->lock);
	    }
#endif	/* HW_FOOTPRINT */

	    if (pset->idle_count > 0) {
		simple_lock(&pset->idle_lock);
		if (pset->idle_count > 0) {
		    processor = (processor_t) queue_first(&pset->idle_queue);
		    queue_remove(&(pset->idle_queue), processor, processor_t,
				processor_queue);
		    pset->idle_count--;
		    processor->next_thread = th;
		    processor->state = PROCESSOR_DISPATCHING;
		    simple_unlock(&pset->idle_lock);
		    return;
		}
		simple_unlock(&pset->idle_lock);
	    }
	    rq = &(pset->runq);
	    run_queue_enqueue(rq,th);
	    /*
	     * Preempt check
	     */
	    if (may_preempt &&
#if	MACH_HOST
		(pset == current_processor()->processor_set) &&
#endif	/* MACH_HOST */
		(current_thread()->sched_pri > th->sched_pri)) {
			/*
			 *	Turn off first_quantum to allow csw.
			 */
			current_processor()->first_quantum = FALSE;
			ast_on(cpu_number(), AST_BLOCK);
	    }
	}
	else {
	    /*
	     *	Bound, can only run on bound processor.  Have to lock
	     *  processor here because it may not be the current one.
	     */
	    if (processor->state == PROCESSOR_IDLE) {
		simple_lock(&processor->lock);
		pset = processor->processor_set;
		simple_lock(&pset->idle_lock);
		if (processor->state == PROCESSOR_IDLE) {
		    queue_remove(&pset->idle_queue, processor,
			processor_t, processor_queue);
		    pset->idle_count--;
		    processor->next_thread = th;
		    processor->state = PROCESSOR_DISPATCHING;
		    simple_unlock(&pset->idle_lock);
		    simple_unlock(&processor->lock);
		    return;
		}
		simple_unlock(&pset->idle_lock);
		simple_unlock(&processor->lock);
	    }
	    rq = &(processor->runq);
	    run_queue_enqueue(rq,th);

	    /*
	     *	Cause ast on processor if processor is on line.
	     *
	     *	XXX Don't do this remotely to master because this will
	     *	XXX send an interprocessor interrupt, and that's too
	     *  XXX expensive for all the unparallelized U*x code.
	     */
	    if (processor == current_processor()) {
		ast_on(cpu_number(), AST_BLOCK);
	    }
	    else if ((processor != master_processor) &&
	    	     (processor->state != PROCESSOR_OFF_LINE)) {
			cause_ast_check(processor);
	    }
	}
#else	/* NCPUS > 1 */
	/*
	 *	XXX should replace queue with a boolean in this case.
	 */
	if (default_pset.idle_count > 0) {
	    processor = (processor_t) queue_first(&default_pset.idle_queue);
	    queue_remove(&default_pset.idle_queue, processor,
		processor_t, processor_queue);
	    default_pset.idle_count--;
	    processor->next_thread = th;
	    processor->state = PROCESSOR_DISPATCHING;
	    return;
	}
	if (th->bound_processor == PROCESSOR_NULL) {
	    	rq = &(default_pset.runq);
	}
	else {
		rq = &(master_processor->runq);
		ast_on(cpu_number(), AST_BLOCK);
	}
	run_queue_enqueue(rq,th);

	/*
	 * Preempt check
	 */
	if (may_preempt && (current_thread()->sched_pri > th->sched_pri)) {
		/*
		 *	Turn off first_quantum to allow context switch.
		 */
		current_processor()->first_quantum = FALSE;
		ast_on(cpu_number(), AST_BLOCK);
	}
#endif	/* NCPUS > 1 */
}

/*
 *	set_pri:
 *
 *	Set the priority of the specified thread to the specified
 *	priority.  This may cause the thread to change queues.
 *
 *	The thread *must* be locked by the caller.
 */

void set_pri(
	thread_t	th,
	int		pri,
	boolean_t	resched)
{
	register struct run_queue	*rq;

	rq = rem_runq(th);
	th->sched_pri = pri;
	if (rq != RUN_QUEUE_NULL) {
	    if (resched)
		thread_setrun(th, TRUE);
	    else
		run_queue_enqueue(rq, th);
	}
}

/*
 *	rem_runq:
 *
 *	Remove a thread from its run queue.
 *	The run queue that the process was on is returned
 *	(or RUN_QUEUE_NULL if not on a run queue).  Thread *must* be locked
 *	before calling this routine.  Unusual locking protocol on runq
 *	field in thread structure makes this code interesting; see thread.h.
 */

struct run_queue *rem_runq(
	thread_t		th)
{
	register struct run_queue	*rq;

	rq = th->runq;
	/*
	 *	If rq is RUN_QUEUE_NULL, the thread will stay out of the
	 *	run_queues because the caller locked the thread.  Otherwise
	 *	the thread is on a runq, but could leave.
	 */
	if (rq != RUN_QUEUE_NULL) {
		simple_lock(&rq->lock);
#if	DEBUG
		checkrq(rq, "rem_runq: at entry");
#endif	/* DEBUG */
		if (rq == th->runq) {
			/*
			 *	Thread is in a runq and we have a lock on
			 *	that runq.
			 */
#if	DEBUG
			checkrq(rq, "rem_runq: before removing thread");
			thread_check(th, rq);
#endif	/* DEBUG */
			remqueue(&rq->runq[0], (queue_entry_t) th);
			rq->count--;
#if	DEBUG
			checkrq(rq, "rem_runq: after removing thread");
#endif	/* DEBUG */
			th->runq = RUN_QUEUE_NULL;
			simple_unlock(&rq->lock);
		}
		else {
			/*
			 *	The thread left the runq before we could
			 * 	lock the runq.  It is not on a runq now, and
			 *	can't move again because this routine's
			 *	caller locked the thread.
			 */
			simple_unlock(&rq->lock);
			rq = RUN_QUEUE_NULL;
		}
	}

	return rq;
}


/*
 *	choose_thread:
 *
 *	Choose a thread to execute.  The thread chosen is removed
 *	from its run queue.  Note that this requires only that the runq
 *	lock be held.
 *
 *	Strategy:
 *		Check processor runq first; if anything found, run it.
 *		Else check pset runq; if nothing found, return idle thread.
 *
 *	Second line of strategy is implemented by choose_pset_thread.
 *	This is only called on processor startup and when thread_block
 *	thinks there's something in the processor runq.
 */

thread_t choose_thread(
	processor_t myprocessor)
{
	thread_t th;
	register queue_t q;
	register run_queue_t runq;
	register int i;
	register processor_set_t pset;

	runq = &myprocessor->runq;

	simple_lock(&runq->lock);
	if (runq->count > 0) {
	    q = runq->runq + runq->low;
	    for (i = runq->low; i < NRQS ; i++, q++) {
		if (!queue_empty(q)) {
		    th = (thread_t) dequeue_head(q);
		    th->runq = RUN_QUEUE_NULL;
		    runq->count--;
		    runq->low = i;
		    simple_unlock(&runq->lock);
		    return th;
		}
	    }
	    panic("choose_thread");
	    /*NOTREACHED*/
	}
	simple_unlock(&runq->lock);

	pset = myprocessor->processor_set;

	simple_lock(&pset->runq.lock);
	return choose_pset_thread(myprocessor,pset);
}

/*
 *	choose_pset_thread:  choose a thread from processor_set runq or
 *		set processor idle and choose its idle thread.
 *
 *	Caller must be at splsched and have a lock on the runq.  This
 *	lock is released by this routine.  myprocessor is always the current
 *	processor, and pset must be its processor set.
 *	This routine chooses and removes a thread from the runq if there
 *	is one (and returns it), else it sets the processor idle and
 *	returns its idle thread.
 */

thread_t choose_pset_thread(
	register processor_t	myprocessor,
	processor_set_t		pset)
{
	register run_queue_t runq;
	register thread_t th;
	register queue_t q;
	register int i;

	runq = &pset->runq;

	if (runq->count > 0) {
	    q = runq->runq + runq->low;
	    for (i = runq->low; i < NRQS ; i++, q++) {
		if (!queue_empty(q)) {
		    th = (thread_t) dequeue_head(q);
		    th->runq = RUN_QUEUE_NULL;
		    runq->count--;
		    /*
		     *	For POLICY_FIXEDPRI, runq->low must be
		     *	accurate!
		     */
#if	MACH_FIXPRI
		    if ((runq->count > 0) &&
			(pset->policies & POLICY_FIXEDPRI)) {
			    while (queue_empty(q)) {
				q++;
				i++;
			    }
		    }
#endif	/* MACH_FIXPRI */
		    runq->low = i;
#if	DEBUG
		    checkrq(runq, "choose_pset_thread");
#endif	/* DEBUG */
		    simple_unlock(&runq->lock);
		    return th;
		}
	    }
	    panic("choose_pset_thread");
	    /*NOTREACHED*/
	}
	simple_unlock(&runq->lock);

	/*
	 *	Nothing is runnable, so set this processor idle if it
	 *	was running.  If it was in an assignment or shutdown,
	 *	leave it alone.  Return its idle thread.
	 */
	simple_lock(&pset->idle_lock);
	if (myprocessor->state == PROCESSOR_RUNNING) {
	    myprocessor->state = PROCESSOR_IDLE;
	    /*
	     *	XXX Until it goes away, put master on end of queue, others
	     *	XXX on front so master gets used last.
	     */
	    if (myprocessor == master_processor) {
		queue_enter(&(pset->idle_queue), myprocessor,
			processor_t, processor_queue);
	    }
	    else {
		queue_enter_first(&(pset->idle_queue), myprocessor,
			processor_t, processor_queue);
	    }

	    pset->idle_count++;
	}
	simple_unlock(&pset->idle_lock);

	return myprocessor->idle_thread;
}

/*
 *	no_dispatch_count counts number of times processors go non-idle
 *	without being dispatched.  This should be very rare.
 */
int	no_dispatch_count = 0;

/*
 *	This is the idle thread, which just looks for other threads
 *	to execute.
 */

void idle_thread_continue(void)
{
	register processor_t myprocessor;
	register volatile thread_t *threadp;
	register volatile int *gcount;
	register volatile int *lcount;
	register thread_t new_thread;
	register int state;
	int mycpu;
	spl_t s;

	mycpu = cpu_number();
	myprocessor = current_processor();
	threadp = (volatile thread_t *) &myprocessor->next_thread;
	lcount = (volatile int *) &myprocessor->runq.count;

	while (TRUE) {
#ifdef	MARK_CPU_IDLE
		MARK_CPU_IDLE(mycpu);
#endif	/* MARK_CPU_IDLE */

#if	MACH_HOST
		gcount = (volatile int *)
				&myprocessor->processor_set->runq.count;
#else	/* MACH_HOST */
		gcount = (volatile int *) &default_pset.runq.count;
#endif	/* MACH_HOST */

/*
 *	This cpu will be dispatched (by thread_setrun) by setting next_thread
 *	to the value of the thread to run next.  Also check runq counts.
 */
		while ((*threadp == (volatile thread_t)THREAD_NULL) &&
		       (*gcount == 0) && (*lcount == 0)) {

			/* check for ASTs while we wait */

			if (need_ast[mycpu] &~ AST_SCHEDULING) {
				(void) splsched();
				/* don't allow scheduling ASTs */
				need_ast[mycpu] &= ~AST_SCHEDULING;
				ast_taken();
				/* back at spl0 */
			}
			
			/*
			 * machine_idle is a machine dependent function,
			 * to conserve power.
			 */
#if	POWER_SAVE
			machine_idle(mycpu);
#endif /* POWER_SAVE */
		}

#ifdef	MARK_CPU_ACTIVE
		MARK_CPU_ACTIVE(mycpu);
#endif	/* MARK_CPU_ACTIVE */

		s = splsched();

		/*
		 *	This is not a switch statement to avoid the
		 *	bounds checking code in the common case.
		 */
retry:
		state = myprocessor->state;
		if (state == PROCESSOR_DISPATCHING) {
			/*
			 *	Commmon case -- cpu dispatched.
			 */
			new_thread = (thread_t) *threadp;
			*threadp = (volatile thread_t) THREAD_NULL;
			myprocessor->state = PROCESSOR_RUNNING;
			/*
			 *	set up quantum for new thread.
			 */
#if	MACH_FIXPRI
			if (new_thread->policy == POLICY_TIMESHARE) {
#endif	/* MACH_FIXPRI */
				/*
				 *  Just use set quantum.  No point in
				 *  checking for shorter local runq quantum;
				 *  csw_needed will handle correctly.
				 */
#if	MACH_HOST
				myprocessor->quantum = new_thread->
					processor_set->set_quantum;
#else	/* MACH_HOST */
				myprocessor->quantum =
					default_pset.set_quantum;
#endif	/* MACH_HOST */

#if	MACH_FIXPRI
			}
			else {
				/*
				 *	POLICY_FIXEDPRI
				 */
				myprocessor->quantum = new_thread->sched_data;
			}
#endif	/* MACH_FIXPRI */
			myprocessor->first_quantum = TRUE;
			counter(c_idle_thread_handoff++);
			thread_run(idle_thread_continue, new_thread);
		}
		else if (state == PROCESSOR_IDLE) {
			register processor_set_t pset;

			pset = myprocessor->processor_set;
			simple_lock(&pset->idle_lock);
			if (myprocessor->state != PROCESSOR_IDLE) {
				/*
				 *	Something happened, try again.
				 */
				simple_unlock(&pset->idle_lock);
				goto retry;
			}
			/*
			 *	Processor was not dispatched (Rare).
			 *	Set it running again.
			 */
			no_dispatch_count++;
			pset->idle_count--;
			queue_remove(&pset->idle_queue, myprocessor,
				processor_t, processor_queue);
			myprocessor->state = PROCESSOR_RUNNING;
			simple_unlock(&pset->idle_lock);
			counter(c_idle_thread_block++);
			thread_block(idle_thread_continue);
		}
		else if ((state == PROCESSOR_ASSIGN) ||
			 (state == PROCESSOR_SHUTDOWN)) {
			/*
			 *	Changing processor sets, or going off-line.
			 *	Release next_thread if there is one.  Actual
			 *	thread to run is on a runq.
			 */
			if ((new_thread = (thread_t)*threadp)!= THREAD_NULL) {
				*threadp = (volatile thread_t) THREAD_NULL;
				thread_setrun(new_thread, FALSE);
			}

			counter(c_idle_thread_block++);
			thread_block(idle_thread_continue);
		}
		else {
			printf(" Bad processor state %d (Cpu %d)\n",
				cpu_state(mycpu), mycpu);
			panic("idle_thread");
		}

		(void) splx(s);
	}
}

void idle_thread(void)
{
	register thread_t self = current_thread();
	spl_t s;

	stack_privilege(self);

	s = splsched();
	self->priority = 31;
	self->sched_pri = 31;

	/*
	 *	Set the idle flag to indicate that this is an idle thread,
	 *	enter ourselves in the idle array, and thread_block() to get
	 *	out of the run queues (and set the processor idle when we
	 *	run next time).
	 */
	thread_lock(self);
	self->state |= TH_IDLE;
	thread_unlock(self);
	current_processor()->idle_thread = self;
	(void) splx(s);

	counter(c_idle_thread_block++);
	thread_block(idle_thread_continue);
	idle_thread_continue();
	/*NOTREACHED*/
}

/*
 *	sched_thread: scheduler thread.
 *
 *	This thread handles periodic calculations in the scheduler that
 *	we don't want to do at interrupt level.  This allows us to
 *	avoid blocking.
 */
void sched_thread_continue(void)
{
    while (TRUE) {
	(void) compute_mach_factor();

	/*
	 *	Check for stuck threads.  This can't be done off of
	 *	the callout queue because it requires operations that
	 *	can't be used from interrupt level.
	 */
	if (sched_tick & 1)
	    	do_thread_scan();

	assert_wait((event_t) 0, FALSE);
	counter(c_sched_thread_block++);
	thread_block(sched_thread_continue);
    }
}

void sched_thread(void)
{
    sched_thread_id = current_thread();

    /*
     *	Sleep on event 0, recompute_priorities() will awaken
     *	us by calling clear_wait().
     */
    assert_wait((event_t) 0, FALSE);
    counter(c_sched_thread_block++);
    thread_block(sched_thread_continue);
    sched_thread_continue();
    /*NOTREACHED*/
}

#define	MAX_STUCK_THREADS	16

/*
 *	do_thread_scan: scan for stuck threads.  A thread is stuck if
 *	it is runnable but its priority is so low that it has not
 *	run for several seconds.  Its priority should be higher, but
 *	won't be until it runs and calls update_priority.  The scanner
 *	finds these threads and does the updates.
 *
 *	Scanner runs in two passes.  Pass one squirrels likely
 *	thread ids away in an array, and removes them from the run queue.
 *	Pass two does the priority updates.  This is necessary because
 *	the run queue lock is required for the candidate scan, but
 *	cannot be held during updates [set_pri will deadlock].
 *
 *	Array length should be enough so that restart isn't necessary,
 *	but restart logic is included.  Does not scan processor runqs.
 *
 */

boolean_t do_thread_scan_debug = FALSE;

thread_t		stuck_threads[MAX_STUCK_THREADS];
int			stuck_count = 0;

/*
 *	do_runq_scan is the guts of pass 1.  It scans a runq for
 *	stuck threads.  A boolean is returned indicating whether
 *	it ran out of space.
 */

boolean_t
do_runq_scan(
	run_queue_t	runq)
{
	register spl_t		s;
	register queue_t	q;
	register thread_t	thread;
	register int		count;

	s = splsched();
	simple_lock(&runq->lock);
	if((count = runq->count) > 0) {
	    q = runq->runq + runq->low;
	    while (count > 0) {
		thread = (thread_t) queue_first(q);
		while (!queue_end(q, (queue_entry_t) thread)) {
		    /*
		     *	Get the next thread now, since we may
		     *	remove this thread from the run queue.
		     */
		    thread_t next = (thread_t) queue_next(&thread->links);

		    if ((thread->state & TH_SCHED_STATE) == TH_RUN &&
			sched_tick - thread->sched_stamp > 1) {
			    /*
			     *	Stuck, save its id for later.
			     */
			    if (stuck_count == MAX_STUCK_THREADS) {
				/*
				 *	!@#$% No more room.
				 */
				simple_unlock(&runq->lock);
				splx(s);
				return TRUE;
			    }
			    /*
			     *	We can`t take the thread_lock here,
			     *	since we already have the runq lock.
			     *	So we can`t grab a reference to the
			     *	thread.  However, a thread that is
			     *	in RUN state cannot be deallocated
			     *	until it stops running.  If it isn`t
			     *	on the runq, then thread_halt cannot
			     *	see it.  So we remove the thread
			     *	from the runq to make it safe.
			     */
			    remqueue(q, (queue_entry_t) thread);
			    runq->count--;
			    thread->runq = RUN_QUEUE_NULL;

			    stuck_threads[stuck_count++] = thread;
if (do_thread_scan_debug)
    printf("do_runq_scan: adding thread %#x\n", thread);
		    }
		    count--;
		    thread = next;
		}
		q++;
	    }
	}
	simple_unlock(&runq->lock);
	splx(s);

	return FALSE;
}

void do_thread_scan(void)
{
	register spl_t		s;
	register boolean_t	restart_needed = 0;
	register thread_t	thread;
#if	MACH_HOST
	register processor_set_t	pset;
#endif	/* MACH_HOST */

	do {
#if	MACH_HOST
	    simple_lock(&all_psets_lock);
	    queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
		if (restart_needed = do_runq_scan(&pset->runq))
			break;
	    }
	    simple_unlock(&all_psets_lock);
#else	/* MACH_HOST */
	    restart_needed = do_runq_scan(&default_pset.runq);
#endif	/* MACH_HOST */
	    if (!restart_needed)
	    	restart_needed = do_runq_scan(&master_processor->runq);

	    /*
	     *	Ok, we now have a collection of candidates -- fix them.
	     */

	    while (stuck_count > 0) {
		thread = stuck_threads[--stuck_count];
		stuck_threads[stuck_count] = THREAD_NULL;
		s = splsched();
		thread_lock(thread);
		if ((thread->state & TH_SCHED_STATE) == TH_RUN) {
			/*
			 *	Do the priority update.  Call
			 *	thread_setrun because thread is
			 *	off the run queues.
			 */
			update_priority(thread);
			thread_setrun(thread, TRUE);
		}
		thread_unlock(thread);
		splx(s);
	    }
	} while (restart_needed);
}
		
#if	DEBUG
void checkrq(
	run_queue_t	rq,
	char		*msg)
{
	register queue_t	q1;
	register int		i, j;
	register queue_entry_t	e;
	register int		low;

	low = -1;
	j = 0;
	q1 = rq->runq;
	for (i = 0; i < NRQS; i++) {
	    if (q1->next == q1) {
		if (q1->prev != q1)
		    panic("checkrq: empty at %s", msg);
	    }
	    else {
		if (low == -1)
		    low = i;
		
		for (e = q1->next; e != q1; e = e->next) {
		    j++;
		    if (e->next->prev != e)
			panic("checkrq-2 at %s", msg);
		    if (e->prev->next != e)
			panic("checkrq-3 at %s", msg);
		}
	    }
	    q1++;
	}
	if (j != rq->count)
	    panic("checkrq: count wrong at %s", msg);
	if (rq->count != 0 && low < rq->low)
	    panic("checkrq: low wrong at %s", msg);
}

void thread_check(
	register thread_t	th,
	register run_queue_t	rq)
{
	register unsigned int 	whichq;

	whichq = th->sched_pri;
	if (whichq >= NRQS) {
		printf("thread_check: priority too high\n");
		whichq = NRQS-1;
	}
	if ((th->links.next == &rq->runq[whichq]) &&
		(rq->runq[whichq].prev != (queue_entry_t)th))
			panic("thread_check");
}
#endif	/* DEBUG */
