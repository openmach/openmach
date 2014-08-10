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
 *	File:	thread.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for threads.
 *
 */

#ifndef	_KERN_THREAD_H_
#define _KERN_THREAD_H_

#include <mach_ipc_compat.h>
#include <hw_footprint.h>
#include <mach_fixpri.h>
#include <mach_host.h>
#include <net_atm.h>

#include <mach/boolean.h>
#include <mach/thread_info.h>
#include <mach/thread_status.h>
#include <mach/machine/vm_types.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/vm_prot.h>
#include <kern/ast.h>
#include <kern/cpu_number.h>
#include <kern/queue.h>
#include <kern/pc_sample.h>
#include <kern/processor.h>
#include <kern/sched_prim.h>	/* event_t, continuation_t */
#include <kern/time_out.h>
#include <kern/timer.h>
#include <kern/lock.h>
#include <kern/sched.h>
#include <kern/task.h>		/* for current_space(), current_map() */
#include <machine/thread.h>
#include <ipc/ipc_kmsg_queue.h>

struct thread {
	/* Run queues */
	queue_chain_t	links;		/* current run queue links */
	run_queue_t	runq;		/* run queue p is on SEE BELOW */
/*
 *	NOTE:	The runq field in the thread structure has an unusual
 *	locking protocol.  If its value is RUN_QUEUE_NULL, then it is
 *	locked by the thread_lock, but if its value is something else
 *	(i.e. a run_queue) then it is locked by that run_queue's lock.
 */

	/* Task information */
	task_t		task;		/* Task to which I belong */
	queue_chain_t	thread_list;	/* list of threads in task */

	/* Thread bookkeeping */
	queue_chain_t	pset_threads;	/* list of all threads in proc set*/

	/* Self-preservation */
	decl_simple_lock_data(,lock)
	int		ref_count;	/* number of references to me */

	/* Hardware state */
	pcb_t		pcb;		/* hardware pcb & machine state */
	vm_offset_t	kernel_stack;	/* accurate only if the thread is
					   not swapped and not executing */
	vm_offset_t	stack_privilege;/* reserved kernel stack */

	/* Swapping information */
	void		(*swap_func)();	/* start here after swapin */

	/* Blocking information */
	event_t		wait_event;	/* event we are waiting on */
	int		suspend_count;	/* internal use only */
	kern_return_t	wait_result;	/* outcome of wait -
					   may be examined by this thread
					   WITHOUT locking */
	boolean_t	wake_active;	/* someone is waiting for this
					   thread to become suspended */
	int		state;		/* Thread state: */
/*
 *	Thread states [bits or'ed]
 */
#define TH_WAIT			0x01	/* thread is queued for waiting */
#define TH_SUSP			0x02	/* thread has been asked to stop */
#define TH_RUN			0x04	/* thread is running or on runq */
#define TH_UNINT		0x08	/* thread is waiting uninteruptibly */
#define	TH_HALTED		0x10	/* thread is halted at clean point ? */

#define TH_IDLE			0x80	/* thread is an idle thread */

#define	TH_SCHED_STATE	(TH_WAIT|TH_SUSP|TH_RUN|TH_UNINT)

#define	TH_SWAPPED		0x0100	/* thread has no kernel stack */
#define	TH_SW_COMING_IN		0x0200	/* thread is waiting for kernel stack */

#define	TH_SWAP_STATE	(TH_SWAPPED | TH_SW_COMING_IN)

	/* Scheduling information */
	int		priority;	/* thread's priority */
	int		max_priority;	/* maximum priority */
	int		sched_pri;	/* scheduled (computed) priority */
#if	MACH_FIXPRI
	int		sched_data;	/* for use by policy */
	int		policy;		/* scheduling policy */
#endif	/* MACH_FIXPRI */
	int		depress_priority; /* depressed from this priority */
	unsigned int	cpu_usage;	/* exp. decaying cpu usage [%cpu] */
	unsigned int	sched_usage;	/* load-weighted cpu usage [sched] */
	unsigned int	sched_stamp;	/* last time priority was updated */

	/* VM global variables */

	vm_offset_t	recover;	/* page fault recovery (copyin/out) */
	boolean_t	vm_privilege;	/* Can use reserved memory? */

	/* User-visible scheduling state */
	int		user_stop_count;	/* outstanding stops */

	/* IPC data structures */
	struct thread *ith_next, *ith_prev;
	mach_msg_return_t ith_state;
	union {
		mach_msg_size_t msize;		/* max size for recvd msg */
		struct ipc_kmsg *kmsg;		/* received message */
	} data;
	mach_port_seqno_t ith_seqno;		/* seqno of recvd message */

	/* This queue is used only when destroying messages:
	   it prevents nasty recursion problems when destroying one message
	   causes other messages to be destroyed.
	   This queue should always be empty under normal circumstances.
	   See ipc_kmsg_destroy() for more details.  */
	struct ipc_kmsg_queue ith_messages; 

	decl_simple_lock_data(, ith_lock_data)
	struct ipc_port *ith_self;	/* not a right, doesn't hold ref */
	struct ipc_port *ith_sself;	/* a send right */
	struct ipc_port *ith_exception;	/* a send right */
#if	MACH_IPC_COMPAT
	struct ipc_port *ith_reply;	/* a send right */
#endif	/* MACH_IPC_COMPAT */

	mach_port_t ith_mig_reply;	/* reply port for mig */
	struct ipc_port *ith_rpc_reply;	/* reply port for kernel RPCs */

	/* State saved when thread's stack is discarded */
	union {
		struct {
			mach_msg_header_t *msg;
			mach_msg_option_t option;
			mach_msg_size_t rcv_size;
			mach_msg_timeout_t timeout;
			mach_port_t notify;
			struct ipc_object *object;
			struct ipc_mqueue *mqueue;
		} receive;
		struct {
			struct ipc_port *port;
			int exc;
			int code;
			int subcode;
		} exception;
		void *other;		/* catch-all for other state */
	} saved;

	/* Timing data structures */
	timer_data_t	user_timer;	/* user mode timer */
	timer_data_t	system_timer;	/* system mode timer */
	timer_save_data_t user_timer_save;  /* saved user timer value */
	timer_save_data_t system_timer_save;  /* saved sys timer val. */
	unsigned int	cpu_delta;	/* cpu usage since last update */
	unsigned int	sched_delta;	/* weighted cpu usage since update */

	/* Time-outs */
	timer_elt_data_t timer;		/* timer for thread */
	timer_elt_data_t depress_timer;	/* timer for priority depression */

	/* Ast/Halt data structures */
	boolean_t	active;		/* how alive is the thread */
	int		ast;    	/* ast's needed.  See ast.h */

	/* Processor data structures */
	processor_set_t	processor_set;	/* assigned processor set */
	processor_t	bound_processor;	/* bound to processor ?*/

	sample_control_t pc_sample;

#if	MACH_HOST
	boolean_t	may_assign;	/* may assignment change? */
	boolean_t	assign_active;	/* someone waiting for may_assign */
#endif	/* MACH_HOST */

#if	NCPUS > 1
	processor_t	last_processor; /* processor this last ran on */
#endif	/* NCPUS > 1 */

#if	NET_ATM
	nw_ep_owned_t   nw_ep_waited;
#endif	/* NET_ATM */
};

/* typedef of thread_t is in kern/kern_types.h */
typedef struct thread_shuttle	*thread_shuttle_t;
#define THREAD_NULL		((thread_t) 0)
#define THREAD_SHUTTLE_NULL	((thread_shuttle_t)0)

#define	ith_msize	data.msize
#define	ith_kmsg	data.kmsg
#define	ith_wait_result	wait_result

#define	ith_msg		saved.receive.msg
#define	ith_option	saved.receive.option
#define ith_rcv_size	saved.receive.rcv_size
#define ith_timeout	saved.receive.timeout
#define ith_notify	saved.receive.notify
#define ith_object	saved.receive.object
#define ith_mqueue	saved.receive.mqueue

#define	ith_port	saved.exception.port
#define ith_exc		saved.exception.exc
#define ith_exc_code	saved.exception.code
#define ith_exc_subcode	saved.exception.subcode

#define ith_other	saved.other

#ifndef	_KERN_KERN_TYPES_H_
typedef struct thread *thread_t;

#define THREAD_NULL	((thread_t) 0)

typedef	mach_port_t *thread_array_t;
#endif	/* _KERN_KERN_TYPES_H_ */


extern thread_t		active_threads[NCPUS];	/* active threads */
extern vm_offset_t	active_stacks[NCPUS];	/* active kernel stacks */

#ifdef KERNEL
/*
 *	User routines
 */

extern kern_return_t	thread_create(
	task_t		parent_task,
	thread_t	*child_thread);
extern kern_return_t	thread_terminate(
	thread_t	thread);
extern kern_return_t	thread_suspend(
	thread_t	thread);
extern kern_return_t	thread_resume(
	thread_t	thread);
extern kern_return_t	thread_abort(
	thread_t	thread);
extern kern_return_t	thread_get_state(
	thread_t	thread,
	int		flavor,
	thread_state_t	old_state,
	natural_t	*old_state_count);
extern kern_return_t	thread_set_state(
	thread_t	thread,
	int		flavor,
	thread_state_t	new_state,
	natural_t	new_state_count);
extern kern_return_t	thread_get_special_port(
	thread_t	thread,
	int		which,
	struct ipc_port	**portp);
extern kern_return_t	thread_set_special_port(
	thread_t	thread,
	int		which,
	struct ipc_port	*port);
extern kern_return_t	thread_info(
	thread_t	thread,
	int		flavor,
	thread_info_t	thread_info_out,
	natural_t	*thread_info_count);
extern kern_return_t	thread_assign(
	thread_t	thread,
	processor_set_t	new_pset);
extern kern_return_t	thread_assign_default(
	thread_t	thread);
#endif

/*
 *	Kernel-only routines
 */

extern void		thread_init(void);
extern void		thread_reference(thread_t);
extern void		thread_deallocate(thread_t);
extern void		thread_hold(thread_t);
extern kern_return_t	thread_dowait(
	thread_t	thread,
	boolean_t	must_halt);
extern void		thread_release(thread_t);
extern kern_return_t	thread_halt(
	thread_t	thread,
	boolean_t	must_halt);
extern void		thread_halt_self(void);
extern void		thread_force_terminate(thread_t);
extern void		thread_set_own_priority(
	int		priority);
extern thread_t		kernel_thread(
	task_t		task,
	void		(*start)(void),
	void *		arg);

extern void		reaper_thread(void);

#if	MACH_HOST
extern void		thread_freeze(
	thread_t	thread);
extern void		thread_doassign(
	thread_t	thread,
	processor_set_t	new_pset,
	boolean_t	release_freeze);
extern void		thread_unfreeze(
	thread_t	thread);
#endif	/* MACH_HOST */

/*
 *	Macro-defined routines
 */

#define thread_pcb(th)		((th)->pcb)

#define thread_lock(th)		simple_lock(&(th)->lock)
#define thread_unlock(th)	simple_unlock(&(th)->lock)

#define thread_should_halt(thread)	\
		((thread)->ast & (AST_HALT|AST_TERMINATE))

/*
 *	Machine specific implementations of the current thread macro
 *	designate this by defining CURRENT_THREAD.
 */
#ifndef	CURRENT_THREAD
#define current_thread()	(active_threads[cpu_number()])
#endif	/* CURRENT_THREAD */

#define	current_stack()		(active_stacks[cpu_number()])

#define	current_task()		(current_thread()->task)
#define	current_space()		(current_task()->itk_space)
#define	current_map()		(current_task()->map)

#endif	/* _KERN_THREAD_H_ */
