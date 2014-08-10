/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	sched_prim.h
 *	Author:	David Golub
 *
 *	Scheduling primitive definitions file
 *
 */

#ifndef	_KERN_SCHED_PRIM_H_
#define _KERN_SCHED_PRIM_H_

#include <mach/boolean.h>
#include <mach/message.h>	/* for mach_msg_timeout_t */
#include <kern/lock.h>
#include <kern/kern_types.h>	/* for thread_t */

/*
 *	Possible results of assert_wait - returned in
 *	current_thread()->wait_result.
 */
#define THREAD_AWAKENED		0		/* normal wakeup */
#define THREAD_TIMED_OUT	1		/* timeout expired */
#define THREAD_INTERRUPTED	2		/* interrupted by clear_wait */
#define THREAD_RESTART		3		/* restart operation entirely */

typedef	void	*event_t;			/* wait event */

typedef	void	(*continuation_t)(void);	/* continuation */

/*
 *	Exported interface to sched_prim.c.
 */

extern void	sched_init(void);

extern void	assert_wait(
	event_t		event,
	boolean_t	interruptible);
extern void	clear_wait(
	thread_t	thread,
	int		result,
	boolean_t	interrupt_only);
extern void	thread_sleep(
	event_t		event,
	simple_lock_t	lock,
	boolean_t	interruptible);
extern void	thread_wakeup();		/* for function pointers */
extern void	thread_wakeup_prim(
	event_t		event,
	boolean_t	one_thread,
	int		result);
extern boolean_t thread_invoke(
	thread_t	old_thread,
	continuation_t	continuation,
	thread_t	new_thread);
extern void	thread_block(
	continuation_t	continuation);
extern void	thread_run(
	continuation_t	continuation,
	thread_t	new_thread);
extern void	thread_set_timeout(
	int		t);
extern void	thread_setrun(
	thread_t	thread,
	boolean_t	may_preempt);
extern void	thread_dispatch(
	thread_t	thread);
extern void	thread_continue(
	thread_t	old_thread);
extern void	thread_go(
	thread_t	thread);
extern void	thread_will_wait(
	thread_t	thread);
extern void	thread_will_wait_with_timeout(
	thread_t	thread,
	mach_msg_timeout_t msecs);
extern boolean_t thread_handoff(
	thread_t	old_thread,
	continuation_t	continuation,
	thread_t	new_thread);
extern void	recompute_priorities();

/*
 *	Routines defined as macros
 */

#define thread_wakeup(x)						\
		thread_wakeup_prim((x), FALSE, THREAD_AWAKENED)
#define thread_wakeup_with_result(x, z)					\
		thread_wakeup_prim((x), FALSE, (z))
#define thread_wakeup_one(x)						\
		thread_wakeup_prim((x), TRUE, THREAD_AWAKENED)

/*
 *	Machine-dependent code must define these functions.
 */

extern void	thread_bootstrap_return(void);
extern void	thread_exception_return(void);
#ifdef	__GNUC__
extern void 	__volatile__ thread_syscall_return(kern_return_t);
#else
extern void	thread_syscall_return(kern_return_t);
#endif
extern thread_t	switch_context(
	thread_t	old_thread,
	continuation_t	continuation,
	thread_t	new_thread);
extern void	stack_handoff(
	thread_t	old_thread,
	thread_t	new_thread);

/*
 *	These functions are either defined in kern/thread.c
 *	via machine-dependent stack_attach and stack_detach functions,
 *	or are defined directly by machine-dependent code.
 */

extern void	stack_alloc(
	thread_t	thread,
	void		(*resume)(thread_t));
extern boolean_t stack_alloc_try(
	thread_t	thread,
	void		(*resume)(thread_t));
extern void	stack_free(
	thread_t	thread);

/*
 *	Convert a timeout in milliseconds (mach_msg_timeout_t)
 *	to a timeout in ticks (for use by set_timeout).
 *	This conversion rounds UP so that small timeouts
 *	at least wait for one tick instead of not waiting at all.
 */

#define convert_ipc_timeout_to_ticks(millis)	\
		(((millis) * hz + 999) / 1000)

#endif	/* _KERN_SCHED_PRIM_H_ */
