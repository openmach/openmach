/*
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
/*
 *	File:	act.h
 *
 *	This defines the Act structure,
 *	which is the kernel representation of a user-space activation.
 *
 */

#ifndef	_KERN_ACT_H_
#define _KERN_ACT_H_

#ifdef MIGRATING_THREADS

#ifndef __dead /* XXX */
#define __dead
#endif

#include <mach_ipc_compat.h>
#include <mach/vm_param.h>
#include <mach/port.h>
#include <kern/lock.h>
#include <kern/refcount.h>
#include <kern/queue.h>

#include "act.h"/*XXX*/

struct task;
struct thread;
struct Act;


struct ReturnHandler {
	struct ReturnHandler *next;
	void (*handler)(struct ReturnHandler *rh, struct Act *act);
};
typedef struct ReturnHandler ReturnHandler;



struct Act {

	/*** Task linkage ***/

	/* Links for task's circular list of activations.
	   The activation is only on the task's activation list while active.
	   Must be first.  */
	queue_chain_t	task_links;

	/* Reference to the task this activation is in.
	   This is constant as long as the activation is allocated.  */
	struct task	*task;



	/*** Machine-dependent state ***/
	/* XXX should be first to allow maximum flexibility to MD code */
	MachineAct	mact;



	/*** Consistency ***/
	RefCount	ref_count;
	decl_simple_lock_data(,lock)



	/*** ipc_target-related stuff ***/

	/* ActPool this activation normally lives on, zero if none.
	   The activation and actpool hold references to each other as long as this is nonzero
	   (even when the activation isn't actually on the actpool's list).  */
	struct ipc_target	*ipt;

	/* Link on the ipt's list of activations.
	   The activation is only actually on the ipt's list (and hence this is valid)
	   when we're not in use (thread == 0) and not suspended (suspend_count == 0).  */
	struct Act	*ipt_next;



	/*** Thread linkage ***/

	/* Thread this activation is in, zero if not in use.
	   The thread holds a reference on the activation while this is nonzero. */
	struct thread	*thread;

	/* The rest in this section is only valid when thread is nonzero.  */

	/* Next higher and next lower activation on the thread's activation stack.
	   For a topmost activation or the null_act, higher is undefined.
	   The bottommost activation is always the null_act.  */
	struct Act	*higher, *lower;

	/* Alert bits pending at this activation;
	   some of them may have propagated from lower activations.  */
	unsigned	alerts;

	/* Mask of alert bits to be allowed to pass through from lower levels.  */
	unsigned	alert_mask;



	/*** Control information ***/

	/* Number of outstanding suspensions on this activation.  */
	int		suspend_count;

	/* This is normally true, but is set to false when the activation is terminated.  */
	int		active;

	/* Chain of return handlers to be called
	   before the thread is allowed to return to this invocation */
	ReturnHandler	*handlers;

	/* A special ReturnHandler attached to the above chain to handle suspension and such */
	ReturnHandler	special_handler;



	/* Special ports attached to this activation */
	struct ipc_port *self;			/* not a right, doesn't hold ref */
	struct ipc_port *self_port;		/* a send right */
	struct ipc_port *exception_port;	/* a send right */
	struct ipc_port *syscall_port;		/* a send right */
#if	MACH_IPC_COMPAT
	struct ipc_port *reply_port;		/* a send right */
	struct task	*reply_task;
#endif	MACH_IPC_COMPAT
};
typedef struct Act Act;
typedef struct Act *act_t;
typedef mach_port_t *act_array_t;

#define ACT_NULL ((Act*)0)


/* Exported to world */
kern_return_t	act_create(struct task *task, vm_offset_t user_stack, vm_offset_t user_rbuf, vm_size_t user_rbuf_size, struct Act **new_act);
kern_return_t	act_alert_mask(struct Act *act, unsigned alert_mask);
kern_return_t	act_alert(struct Act *act, unsigned alerts);
kern_return_t	act_abort(struct Act *act);
kern_return_t	act_abort_safely(struct Act *act);
kern_return_t	act_terminate(struct Act *act);
kern_return_t	act_suspend(struct Act *act);
kern_return_t	act_resume(struct Act *act);
kern_return_t	act_get_state(struct Act *act, int flavor,
			natural_t *state, natural_t *pcount);
kern_return_t	act_set_state(struct Act *act, int flavor,
			natural_t *state, natural_t count);

#define		act_lock(act)		simple_lock(&(act)->lock)
#define		act_unlock(act)		simple_unlock(&(act)->lock)

#define		act_reference(act)	refcount_take(&(act)->ref_count)
void		act_deallocate(struct Act *act);

/* Exported to startup.c */
void		act_init(void);

/* Exported to task.c */
kern_return_t	act_terminate_task_locked(struct Act *act);

/* Exported to thread.c */
extern Act	null_act;
kern_return_t	act_create_kernel(Act **out_act);

/* Exported to machine-dependent activation code */
void		act_execute_returnhandlers(void);



/* System-dependent functions */
kern_return_t	act_machine_create(struct task *task, Act *inc, vm_offset_t user_stack, vm_offset_t user_rbuf, vm_size_t user_rbuf_size);
void		act_machine_destroy(Act *inc);
kern_return_t	act_machine_set_state(Act *inc, int flavor, int *tstate, unsigned count);
kern_return_t	act_machine_get_state(Act *inc, int flavor, int *tstate, unsigned *count);



#endif /* MIGRATING_THREADS */
#endif _KERN_ACT_H_
