/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	sched.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	Header file for scheduler.
 *
 */

#ifndef	_KERN_SCHED_H_
#define _KERN_SCHED_H_

#include <cpus.h>
#include <mach_fixpri.h>
#include <simple_clock.h>
#include <stat_time.h>

#include <kern/queue.h>
#include <kern/lock.h>
#include <kern/macro_help.h>

#if	MACH_FIXPRI
#include <mach/policy.h>
#endif	MACH_FIXPRI

#if	STAT_TIME

/*
 *	Statistical timing uses microseconds as timer units.  18 bit shift
 *	yields priorities.  PRI_SHIFT_2 isn't needed.
 */
#define PRI_SHIFT	18

#else	STAT_TIME

/*
 *	Otherwise machine provides shift(s) based on time units it uses.
 */
#include <machine/sched_param.h>

#endif	STAT_TIME
#define NRQS	32			/* 32 run queues per cpu */

struct run_queue {
	queue_head_t		runq[NRQS];	/* one for each priority */
	decl_simple_lock_data(,	lock)		/* one lock for all queues */
	int			low;		/* low queue value */
	int			count;		/* count of threads runable */
};

typedef struct run_queue	*run_queue_t;
#define RUN_QUEUE_NULL	((run_queue_t) 0)

#if	MACH_FIXPRI
/*
 *	NOTE: For fixed priority threads, first_quantum indicates
 *	whether context switch at same priority is ok.  For timeshareing
 *	it indicates whether preempt is ok.
 */

#define csw_needed(thread, processor) ((thread)->state & TH_SUSP ||	\
	((processor)->runq.count > 0) ||				\
	((thread)->policy == POLICY_TIMESHARE &&			\
		(processor)->first_quantum == FALSE &&			\
		(processor)->processor_set->runq.count > 0 &&		\
		  (processor)->processor_set->runq.low <=		\
			(thread)->sched_pri) ||				\
	((thread)->policy == POLICY_FIXEDPRI &&				\
		(processor)->processor_set->runq.count > 0 &&		\
		 ((((processor)->first_quantum == FALSE) &&		\
		  ((processor)->processor_set->runq.low <=		\
			(thread)->sched_pri)) ||			\
		 ((processor)->processor_set->runq.low <		\
			(thread)->sched_pri))))

#else	MACH_FIXPRI
#define csw_needed(thread, processor) ((thread)->state & TH_SUSP ||	\
		((processor)->runq.count > 0) ||			\
		((processor)->first_quantum == FALSE &&			\
		 ((processor)->processor_set->runq.count > 0 &&		\
		  (processor)->processor_set->runq.low <=		\
			((thread)->sched_pri))))
#endif	MACH_FIXPRI

/*
 *	Scheduler routines.
 */

extern struct run_queue	*rem_runq();
extern struct thread	*choose_thread();
extern queue_head_t	action_queue;	/* assign/shutdown queue */
decl_simple_lock_data(extern,action_lock);

extern int		min_quantum;	/* defines max context switch rate */

/*
 *	Default base priorities for threads.
 */
#define BASEPRI_SYSTEM	6
#define BASEPRI_USER	12

/*
 *	Macro to check for invalid priorities.
 */

#define invalid_pri(pri) (((pri) < 0) || ((pri) >= NRQS))

/*
 *	Shift structures for holding update shifts.  Actual computation
 *	is  usage = (usage >> shift1) +/- (usage >> abs(shift2))  where the
 *	+/- is determined by the sign of shift 2.
 */
struct shift {
	int	shift1;
	int	shift2;
};

typedef	struct shift	*shift_t, shift_data_t;

/*
 *	sched_tick increments once a second.  Used to age priorities.
 */

extern unsigned	sched_tick;

#define SCHED_SCALE	128
#define SCHED_SHIFT	7

/*
 *	thread_timer_delta macro takes care of both thread timers.
 */

#define thread_timer_delta(thread)  				\
MACRO_BEGIN							\
	register unsigned	delta;				\
								\
	delta = 0;						\
	TIMER_DELTA((thread)->system_timer,			\
		(thread)->system_timer_save, delta);		\
	TIMER_DELTA((thread)->user_timer,			\
		(thread)->user_timer_save, delta);		\
	(thread)->cpu_delta += delta;				\
	(thread)->sched_delta += delta * 			\
			(thread)->processor_set->sched_load;	\
MACRO_END

#if	SIMPLE_CLOCK
/*
 *	sched_usec is an exponential average of number of microseconds
 *	in a second for clock drift compensation.
 */

extern int	sched_usec;
#endif	SIMPLE_CLOCK

#endif	_KERN_SCHED_H_
