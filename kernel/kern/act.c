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
 *	Author:	Bryan Ford, University of Utah CSL
 */
/*
 *	File:	act.c
 *
 *	Activation management routines
 *
 */

#ifdef MIGRATING_THREADS

#include <mach_ipc_compat.h> /* XXX */
#include <mach/kern_return.h>
#include <mach/alert.h>
#include <kern/mach_param.h> /* XXX INCALL_... */
#include <kern/zalloc.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/act.h>
#include <kern/current.h>
#include "ipc_target.h"

static void special_handler(ReturnHandler *rh, struct Act *act);

#ifdef ACT_STATIC_KLUDGE
#undef ACT_STATIC_KLUDGE
#define ACT_STATIC_KLUDGE 300
#endif

#ifndef ACT_STATIC_KLUDGE
static zone_t act_zone;
#else
static Act *act_freelist;
static Act free_acts[ACT_STATIC_KLUDGE];
#endif

/* This is a rather special activation
   which resides at the top and bottom of every thread.
   When the last "real" activation on a thread is destroyed,
   the null_act on the bottom gets invoked, destroying the thread.
   At the top, the null_act acts as an "invalid" cached activation,
   which will always fail the cached-activation test on RPC paths.

   As you might expect, most of its members have no particular value.
   alerts is zero.  */
Act null_act;

void
global_act_init()
{
#ifndef ACT_STATIC_KLUDGE
	act_zone = zinit(
			sizeof(struct Act),
			ACT_MAX * sizeof(struct Act), /* XXX */
			ACT_CHUNK * sizeof(struct Act),
			0, "activations");
#else
	int i;

printf("activations: [%x-%x]\n", &free_acts[0], &free_acts[ACT_STATIC_KLUDGE]);
	act_freelist = &free_acts[0];
	free_acts[0].ipt_next = 0;
	for (i = 1; i < ACT_STATIC_KLUDGE; i++) {
		free_acts[i].ipt_next = act_freelist;
		act_freelist = &free_acts[i];
	}
	/* XXX simple_lock_init(&act_freelist->lock); */
#endif

#if 0
	simple_lock_init(&null_act.lock);
	refcount_init(&null_act.ref_count, 1);
#endif

	act_machine_init();
}

/* Create a new activation in a specific task.
   Locking: Task */
kern_return_t act_create(task_t task, vm_offset_t user_stack,
			 vm_offset_t user_rbuf, vm_size_t user_rbuf_size,
			 struct Act **new_act)
{
	Act *act;
	int rc;

#ifndef ACT_STATIC_KLUDGE
	act = (Act*)zalloc(act_zone);
	if (act == 0)
		return(KERN_RESOURCE_SHORTAGE);
#else
	/* XXX ipt_lock(act_freelist); */
	act = act_freelist;
	if (act == 0) panic("out of activations");
	act_freelist = act->ipt_next;
	/* XXX ipt_unlock(act_freelist); */
	act->ipt_next = 0;
#endif
	bzero(act, sizeof(*act)); /*XXX shouldn't be needed */

#ifdef DEBUG
	act->lower = act->higher = 0;
#endif

	/* Start with one reference for being active, another for the caller */
	simple_lock_init(&act->lock);
	refcount_init(&act->ref_count, 2);

	/* Latch onto the task.  */
	act->task = task;
	task_reference(task);

	/* Other simple setup */
	act->ipt = 0;
	act->thread = 0;
	act->suspend_count = 0;
	act->active = 1;
	act->handlers = 0;

	/* The special_handler will always be last on the returnhandlers list.  */
	act->special_handler.next = 0;
	act->special_handler.handler = special_handler;

	ipc_act_init(task, act);
	act_machine_create(task, act, user_stack, user_rbuf, user_rbuf_size);

	task_lock(task);

	/* Chain the act onto the task's list */
	act->task_links.next = task->acts.next;
	act->task_links.prev = &task->acts;
	task->acts.next->prev = &act->task_links;
	task->acts.next = &act->task_links;
	task->act_count++;

	task_unlock(task);

	*new_act = act;
	return KERN_SUCCESS;
}

/* This is called when an act's ref_count drops to zero.
   This can only happen when thread is zero (not in use),
   ipt is zero (not attached to any ipt),
   and active is false (terminated).  */
static void act_free(Act *inc)
{
	act_machine_destroy(inc);
	ipc_act_destroy(inc);

	/* Drop the task reference.  */
	task_deallocate(inc->task);

	/* Put the act back on the act zone */
#ifndef ACT_STATIC_KLUDGE
	zfree(act_zone, (vm_offset_t)inc);
#else
	/* XXX ipt_lock(act_freelist); */
	inc->ipt_next = act_freelist;
	act_freelist = inc;
	/* XXX ipt_unlock(act_freelist); */
#endif
}

void act_deallocate(Act *inc)
{
	refcount_drop(&inc->ref_count, act_free(inc));
}

/* Attach an act to the top of a thread ("push the stack").
   The thread must be either the current one or a brand-new one.
   Assumes the act is active but not in use.
   Assumes that if it is attached to an ipt (i.e. the ipt pointer is nonzero),
   the act has already been taken off the ipt's list.

   Already locked: cur_thread, act */
void act_attach(Act *act, thread_t thread, unsigned init_alert_mask)
{
	Act *lower;

	act->thread = thread;

	/* The thread holds a reference to the activation while using it.  */
	refcount_take(&act->ref_count);

	/* XXX detach any cached activations from above the target */

	/* Chain the act onto the thread's act stack.  */
	lower = thread->top_act;
	act->lower = lower;
	lower->higher = act;
	thread->top_act = act;

	act->alert_mask = init_alert_mask;
	act->alerts = lower->alerts & init_alert_mask;
}

/* Remove the current act from the top of the current thread ("pop the stack").
   Return it to the ipt it lives on, if any.
   Locking: Thread > Act(not on ipt) > ipc_target */
void act_detach(Act *cur_act)
{
	thread_t cur_thread = cur_act->thread;

	thread_lock(cur_thread);
	act_lock(cur_act);

	/* Unlink the act from the thread's act stack */
	cur_thread->top_act = cur_act->lower;
	cur_act->thread = 0;
#ifdef DEBUG
	cur_act->lower = cur_act->higher = 0;
#endif

	thread_unlock(cur_thread);

	/* Return it to the ipt's list */
	if (cur_act->ipt)
	{
		ipt_lock(cur_act->ipt);
		cur_act->ipt_next = cur_act->ipt->ipt_acts;
		cur_act->ipt->ipt_acts = cur_act;
		ipt_unlock(cur_act->ipt);
#if 0
	printf("  return to ipt %x\n", cur_act->ipt);
#endif
	}

	act_unlock(cur_act);

	/* Drop the act reference taken for being in use.  */
	refcount_drop(&cur_act->ref_count, act_free(cur_act));
}



/*** Activation control support routines ***/

/* This is called by system-dependent code
   when it detects that act->handlers is non-null
   while returning into user mode.
   Activations linked onto an ipt always have null act->handlers,
   so RPC entry paths need not check it.

   Locking: Act */
void act_execute_returnhandlers()
{
	Act *act = current_act();

#if 0
	printf("execute_returnhandlers\n");
#endif
	while (1) {
		ReturnHandler *rh;

		/* Grab the next returnhandler */
		act_lock(act);
		rh = act->handlers;
		if (!rh) {
			act_unlock(act);
			return;
		}
		act->handlers = rh->next;
		act_unlock(act);

		/* Execute it */
		(*rh->handler)(rh, act);
	}
}

/* Try to nudge an act into executing its returnhandler chain.
   Ensures that the activation will execute its returnhandlers
   before it next executes any of its user-level code.
   Also ensures that it is safe to break the thread's activation chain
   immediately above this activation,
   by rolling out of any outstanding two-way-optimized RPC.

   The target activation is not necessarily active
   or even in use by a thread.
   If it isn't, this routine does nothing.

   Already locked: Act */
static void act_nudge(struct Act *act)
{
	/* If it's suspended, wake it up.  */
	thread_wakeup(&act->suspend_count);

	/* Do a machine-dependent low-level nudge.
	   If we're on a multiprocessor,
	   this may mean sending an interprocessor interrupt.
	   In any case, it means rolling out of two-way-optimized RPC paths.  */
	act_machine_nudge(act);
}

/* Install the special returnhandler that handles suspension and termination,
   if it hasn't been installed already.

   Already locked: Act */
static void install_special_handler(struct Act *act)
{
	ReturnHandler **rh;

	/* The work handler must always be the last ReturnHandler on the list,
	   because it can do tricky things like detach the act.  */
	for (rh = &act->handlers; *rh; rh = &(*rh)->next);
	if (rh != &act->special_handler.next) {
		*rh = &act->special_handler;
	}

	/* Nudge the target activation,
	   to ensure that it will see the returnhandler we're adding.  */
	act_nudge(act);
}

/* Locking: Act */
static void special_handler(ReturnHandler *rh, struct Act *cur_act)
{
      retry:

	act_lock(cur_act);

	/* If someone has killed this invocation,
	   invoke the return path with a terminated exception.  */
	if (!cur_act->active) {
		act_unlock(cur_act);
		act_machine_return(KERN_TERMINATED);
		/* XXX should just set the activation's reentry_routine
		   and then return from special_handler().
		   The magic reentry_routine should just pop its own activation
		   and chain to the reentry_routine of the _lower_ activation.
		   If that lower activation is the null_act,
		   the thread will then be terminated.  */
	}

	/* If we're suspended, go to sleep and wait for someone to wake us up.  */
	if (cur_act->suspend_count) {
		act_unlock(cur_act);
		/* XXX mp unsafe */
		thread_wait((int)&cur_act->suspend_count, FALSE);

		act_lock(cur_act);

		/* If we're still (or again) suspended,
		   go to sleep again after executing any new returnhandlers that may have appeared.  */
		if (cur_act->suspend_count)
			install_special_handler(cur_act);
	}

	act_unlock(cur_act);
}

#if 0 /************************ OLD SEMI-OBSOLETE CODE *********************/
static __dead void act_throughcall_return(Act *act)
{
	/* Done - destroy the act and return */
	act_detach(act);
	act_terminate(act);
	act_deallocate(act);

	/* XXX */
	thread_terminate_self();
}

__dead void act_throughcall(task_t task, void (*infunc)())
{
	thread_t thread = current_thread();
	Act *act;
	ReturnHandler rh;
	int rc;

	rc = act_create(task, 0, 0, 0, &act);
	if (rc) return rc;

	act->return_routine = act_throughcall_return;

	thread_lock(thread);
	act_lock(act);

	act_attach(thread, act, 0);

	rh.handler = infunc;
	rh.next = act->handlers;
	act->handlers = &rh;

	act_unlock(act);
	thread_unlock(thread);

	/* Call through the act into the returnhandler list */
	act_machine_throughcall(act);
}


/* Grab an act from the specified pool, to pass to act_upcall.
   Returns with the act locked, since it's in an inconsistent state
   (not on its ipt but not on a thread either).
   Returns null if no acts are available on the ipt.

   Locking: ipc_target > Act(on ipt) */
Act *act_grab(struct ipc_target *ipt)
{
	Act *act;

	ipt_lock(ipt);

      retry:

	/* Pull an act off the ipt's list.  */
	act = ipt->acts;
	if (!act)
		goto none_avail;
	ipt->acts = act->ipt_next;

	act_lock(act);

	/* If it's been terminated, drop it and get another one.  */
	if (!act->active) {
#if 0
		printf("dropping terminated act %08x\n", act);
#endif
		/* XXX ipt_deallocate(ipt); */
		act->ipt = 0;
		act_unlock(act);
		act_deallocate(act);
		goto retry;
	}

none_avail:
	ipt_unlock(ipt);

	return act;
}

/* Try to make an upcall with an act on the specified ipt.
   If the ipt is empty, returns KERN_RESOURCE_SHORTAGE.  XXX???

   Locking: ipc_target > Act > Thread */
kern_return_t	act_upcall(struct Act *act, unsigned init_alert_mask,
			      vm_offset_t user_entrypoint, vm_offset_t user_data)
{
	thread_t cur_thread = current_thread();
	int rc;

	/* XXX locking */

	act_attach(cur_thread, act, init_alert_mask);

	/* Make the upcall into the destination task */
	rc = act_machine_upcall(act, user_entrypoint, user_data);

	/* Done - detach the act and return */
	act_detach(act);

	return rc;
}
#endif /************************ END OF OLD SEMI-OBSOLETE CODE *********************/




/*** Act service routines ***/

/* Lock this act and its current thread.
   We can only find the thread from the act
   and the thread must be locked before the act,
   requiring a little icky juggling.

   If the thread is not currently on any thread,
   returns with only the act locked.

   Note that this routine is not called on any performance-critical path.
   It is only for explicit act operations
   which don't happen often.

   Locking: Thread > Act */
static thread_t act_lock_thread(Act *act)
{
	thread_t thread;

      retry:

	/* Find the thread */
	act_lock(act);
	thread = act->thread;
	if (thread == 0)
	{
		act_unlock(act);
		return 0;
	}
	thread_reference(thread);
	act_unlock(act);

	/* Lock the thread and re-lock the act,
	   and make sure the thread didn't change.  */
	thread_lock(thread);
	act_lock(act);
	if (act->thread != thread)
	{
		act_unlock(act);
		thread_unlock(thread);
		thread_deallocate(thread);
		goto retry;
	}

	thread_deallocate(thread);

	return thread;
}

/* Already locked: act->task
   Locking: Task > Act */
kern_return_t act_terminate_task_locked(struct Act *act)
{
	act_lock(act);

	if (act->active)
	{
		/* Unlink the act from the task's act list,
		   so it doesn't appear in calls to task_acts and such.
		   The act still keeps its ref on the task, however,
		   until it loses all its own references and is freed.  */
		act->task_links.next->prev = act->task_links.prev;
		act->task_links.prev->next = act->task_links.next;
		act->task->act_count--;

		/* Remove it from any ipc_target.  XXX is this right?  */
		act_set_target(act, 0);

		/* This will allow no more control operations on this act.  */
		act->active = 0;

		/* When the special_handler gets executed,
		   it will see the terminated condition and exit immediately.  */
		install_special_handler(act);

		/* Drop the act reference taken for being active.
		   (There is still at least one reference left: the one we were passed.)  */
		act_deallocate(act);
	}

	act_unlock(act);

	return KERN_SUCCESS;
}

/* Locking: Task > Act */
kern_return_t act_terminate(struct Act *act)
{
	task_t task = act->task;
	kern_return_t rc;

	/* act->task never changes,
	   so we can read it before locking the act.  */
	task_lock(act->task);

	rc = act_terminate_task_locked(act);

	task_unlock(act->task);

	return rc;
}

/* If this Act is on a Thread and is not the topmost,
   yank it and everything below it off of the thread's stack
   and put it all on a new thread forked from the original one.
   May fail due to resource shortage, but can always be retried.

   Locking: Thread > Act */
kern_return_t act_yank(Act *act)
{
	thread_t thread = act_lock_thread(act);

#if 0
	printf("act_yank inc %08x thread %08x\n", act, thread);
#endif
	if (thread)
	{
		if (thread->top_act != act)
		{
			printf("detaching act %08x from thread %08x\n", act, thread);

			/* Nudge the activation into a clean point for detachment.  */
			act_nudge(act);

			/* Now detach the activation
			   and give the orphan its own flow of control.  */
			/*XXX*/
		}

		thread_unlock(thread);
	}
	act_unlock(act);

	/* Ask the thread to return as quickly as possible,
	   because its results are now useless.  */
	act_abort(act);

	return KERN_SUCCESS;
}

/* Assign an activation to a specific ipc_target.
   Fails if the activation is already assigned to another pool.
   If ipt == 0, we remove the from its ipt.

   Locking: Act(not on ipt) > ipc_target > Act(on ipt) */
kern_return_t act_set_target(Act *act, struct ipc_target *ipt)
{
	act_lock(act);

	if (ipt == 0)
	{
		Act **lact;

		ipt = act->ipt;
		if (ipt == 0)
			return;

		/* XXX This is a violation of the locking order.  */
		ipt_lock(ipt);
		for (lact = &ipt->ipt_acts; *lact; lact = &((*lact)->ipt_next))
			if (act == *lact)
			{
				*lact = act->ipt_next;
				break;
			}
		ipt_unlock(ipt);

		act->ipt = 0;
		/* XXX ipt_deallocate(ipt); */
		act_deallocate(act);
		return;
	}
	if (act->ipt != ipt)
	{
		if (act->ipt != 0)
		{
			act_unlock(act);
			return KERN_FAILURE; /*XXX*/
		}
		act->ipt = ipt;
		ipt->ipt_type |= IPT_TYPE_MIGRATE_RPC;

		/* They get references to each other.  */
		act_reference(act);
		ipt_reference(ipt);

		/* If it is available,
		   add it to the ipt's available-activation list.  */
		if ((act->thread == 0) && (act->suspend_count == 0))
		{
			ipt_lock(ipt);
			act->ipt_next = ipt->ipt_acts;
			act->ipt->ipt_acts = act;
			ipt_unlock(ipt);
		}
	}
	act_unlock(act);

	return KERN_SUCCESS;
}

/* Register an alert from this activation.
   Each set bit is propagated upward from (but not including) this activation,
   until the top of the chain is reached or the bit is masked.

   Locking: Thread > Act */
kern_return_t act_alert(struct Act *act, unsigned alerts)
{
	thread_t thread = act_lock_thread(act);

#if 0
	printf("act_alert %08x: %08x\n", act, alerts);
#endif
	if (thread)
	{
		struct Act *act_up = act;
		while ((alerts) && (act_up != thread->top_act))
		{
			act_up = act_up->higher;
			alerts &= act_up->alert_mask;
			act_up->alerts |= alerts;
		}

		/* XXX If we reach the top, and it is blocked in glue code, do something.  */

		thread_unlock(thread);
	}
	act_unlock(act);

	return KERN_SUCCESS;
}

/* Locking: Thread > Act */
kern_return_t act_abort(struct Act *act)
{
	return act_alert(act, ALERT_ABORT_STRONG);
}

/* Locking: Thread > Act */
kern_return_t act_abort_safely(struct Act *act)
{
	return act_alert(act, ALERT_ABORT_SAFE);
}

/* Locking: Thread > Act */
kern_return_t act_alert_mask(struct Act *act, unsigned alert_mask)
{
	panic("act_alert_mask\n");
	return KERN_SUCCESS;
}

/* Locking: Thread > Act */
kern_return_t act_suspend(struct Act *act)
{
	thread_t thread = act_lock_thread(act);
	kern_return_t rc = KERN_SUCCESS;

#if 0
	printf("act_suspend %08x\n", act);
#endif
	if (act->active)
	{
		if (act->suspend_count++ == 0)
		{
			/* XXX remove from ipt */
			install_special_handler(act);
			act_nudge(act);
		}
	}
	else
		rc = KERN_TERMINATED;

	if (thread)
		thread_unlock(thread);
	act_unlock(act);

	return rc;
}

/* Locking: Act */
kern_return_t act_resume(struct Act *act)
{
#if 0
	printf("act_resume %08x from %d\n", act, act->suspend_count);
#endif

	act_lock(act);
	if (!act->active)
	{
		act_unlock(act);
		return KERN_TERMINATED;
	}

	if (act->suspend_count > 0) {
		if (--act->suspend_count == 0) {
			thread_wakeup(&act->suspend_count);
			/* XXX return to ipt */
		}
	}

	act_unlock(act);

	return KERN_SUCCESS;
}

typedef struct GetSetState {
	struct ReturnHandler rh;
	int flavor;
	void *state;
	int *pcount;
	int result;
} GetSetState;

/* Locking: Thread */
kern_return_t get_set_state(struct Act *act, int flavor, void *state, int *pcount,
			    void (*handler)(ReturnHandler *rh, struct Act *act))
{
	GetSetState gss;

	/* Initialize a small parameter structure */
	gss.rh.handler = handler;
	gss.flavor = flavor;
	gss.state = state;
	gss.pcount = pcount;

	/* Add it to the act's return handler list */
	act_lock(act);
	gss.rh.next = act->handlers;
	act->handlers = &gss.rh;

	act_nudge(act);

	act_unlock(act);
	/* XXX mp unsafe */
	thread_wait((int)&gss, 0); /* XXX could be interruptible */

	return gss.result;
}

static void get_state_handler(ReturnHandler *rh, struct Act *act)
{
	GetSetState *gss = (GetSetState*)rh;

	gss->result = act_machine_get_state(act, gss->flavor, gss->state, gss->pcount);
	thread_wakeup((int)gss);
}

/* Locking: Thread */
kern_return_t act_get_state(struct Act *act, int flavor, natural_t *state, natural_t *pcount)
{
	return get_set_state(act, flavor, state, pcount, get_state_handler);
}

static void set_state_handler(ReturnHandler *rh, struct Act *act)
{
	GetSetState *gss = (GetSetState*)rh;

	gss->result = act_machine_set_state(act, gss->flavor, gss->state, *gss->pcount);
	thread_wakeup((int)gss);
}

/* Locking: Thread */
kern_return_t act_set_state(struct Act *act, int flavor, natural_t *state, natural_t count)
{
	return get_set_state(act, flavor, state, &count, set_state_handler);
}



/*** backward compatibility hacks ***/

#include <mach/thread_info.h>
#include <mach/thread_special_ports.h>
#include <ipc/ipc_port.h>

kern_return_t act_thread_info(Act *act, int flavor,
				 thread_info_t thread_info_out, unsigned *thread_info_count)
{
	return thread_info(act->thread, flavor, thread_info_out, thread_info_count);
}

kern_return_t
act_thread_assign(Act *act, processor_set_t new_pset)
{
	return thread_assign(act->thread, new_pset);
}

kern_return_t
act_thread_assign_default(Act *act)
{
	return thread_assign_default(act->thread);
}

kern_return_t
act_thread_get_assignment(Act *act, processor_set_t *pset)
{
	return thread_get_assignment(act->thread, pset);
}

kern_return_t
act_thread_priority(Act *act, int priority, boolean_t set_max)
{
	return thread_priority(act->thread, priority, set_max);
}

kern_return_t
act_thread_max_priority(Act *act, processor_set_t *pset, int max_priority)
{
	return thread_max_priority(act->thread, pset, max_priority);
}

kern_return_t
act_thread_policy(Act *act, int policy, int data)
{
	return thread_policy(act->thread, policy, data);
}

kern_return_t
act_thread_wire(struct host *host, Act *act, boolean_t wired)
{
	return thread_wire(host, act->thread, wired);
}

kern_return_t
act_thread_depress_abort(Act *act)
{
	return thread_depress_abort(act->thread);
}

/*
 *	Routine:	act_get_special_port [kernel call]
 *	Purpose:
 *		Clones a send right for one of the thread's
 *		special ports.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		KERN_SUCCESS		Extracted a send right.
 *		KERN_INVALID_ARGUMENT	The thread is null.
 *		KERN_FAILURE		The thread is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
act_get_special_port(Act *act, int which, ipc_port_t *portp)
{
	ipc_port_t *whichp;
	ipc_port_t port;

#if 0
	printf("act_get_special_port\n");
#endif
	if (act == 0)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case THREAD_REPLY_PORT:
		whichp = &act->reply_port;
		break;
#endif	MACH_IPC_COMPAT

	    case THREAD_KERNEL_PORT:
		whichp = &act->self_port;
		break;

	    case THREAD_EXCEPTION_PORT:
		whichp = &act->exception_port;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	thread_lock(act->thread);

	if (act->self_port == IP_NULL) {
		thread_unlock(act->thread);
		return KERN_FAILURE;
	}

	port = ipc_port_copy_send(*whichp);
	thread_unlock(act->thread);

	*portp = port;
	return KERN_SUCCESS;
}

/*
 *	Routine:	act_set_special_port [kernel call]
 *	Purpose:
 *		Changes one of the thread's special ports,
 *		setting it to the supplied send right.
 *	Conditions:
 *		Nothing locked.  If successful, consumes
 *		the supplied send right.
 *	Returns:
 *		KERN_SUCCESS		Changed the special port.
 *		KERN_INVALID_ARGUMENT	The thread is null.
 *		KERN_FAILURE		The thread is dead.
 *		KERN_INVALID_ARGUMENT	Invalid special port.
 */

kern_return_t
act_set_special_port(Act *act, int which, ipc_port_t port)
{
	ipc_port_t *whichp;
	ipc_port_t old;

#if 0
	printf("act_set_special_port\n");
#endif
	if (act == 0)
		return KERN_INVALID_ARGUMENT;

	switch (which) {
#if	MACH_IPC_COMPAT
	    case THREAD_REPLY_PORT:
		whichp = &act->reply_port;
		break;
#endif	MACH_IPC_COMPAT

	    case THREAD_KERNEL_PORT:
		whichp = &act->self_port;
		break;

	    case THREAD_EXCEPTION_PORT:
		whichp = &act->exception_port;
		break;

	    default:
		return KERN_INVALID_ARGUMENT;
	}

	thread_lock(act->thread);
	if (act->self_port == IP_NULL) {
		thread_unlock(act->thread);
		return KERN_FAILURE;
	}

	old = *whichp;
	*whichp = port;
	thread_unlock(act->thread);

	if (IP_VALID(old))
		ipc_port_release_send(old);
	return KERN_SUCCESS;
}

/*
 *	XXX lame, non-blocking ways to get/set state.
 *	Return thread's machine-dependent state.
 */
kern_return_t
act_get_state_immediate(act, flavor, old_state, old_state_count)
	register Act		*act;
	int			flavor;
	void			*old_state;	/* pointer to OUT array */
	unsigned int		*old_state_count;	/*IN/OUT*/
{
	kern_return_t		ret;

	act_lock(act);
	/* not the top activation, return current state */
	if (act->thread && act->thread->top_act != act) {
		ret = act_machine_get_state(act, flavor,
					    old_state, old_state_count);
		act_unlock(act);
		return ret;
	}
	act_unlock(act);

	/* not sure this makes sense */
	return act_get_state(act, flavor, old_state, old_state_count);
}

/*
 *	Change thread's machine-dependent state.
 */
kern_return_t
act_set_state_immediate(act, flavor, new_state, new_state_count)
	register Act		*act;
	int			flavor;
	void			*new_state;
	unsigned int		new_state_count;
{
	kern_return_t		ret;

	act_lock(act);
	/* not the top activation, set it now */
	if (act->thread && act->thread->top_act != act) {
		ret = act_machine_set_state(act, flavor,
					    new_state, new_state_count);
		act_unlock(act);
		return ret;
	}
	act_unlock(act);

	/* not sure this makes sense */
	return act_set_state(act, flavor, new_state, new_state_count);
}

void act_count()
{
	int i;
	Act *act;
	static int amin = ACT_STATIC_KLUDGE;

	i = 0;
	for (act = act_freelist; act; act = act->ipt_next)
		i++;
	if (i < amin)
		amin = i;
	printf("%d of %d activations in use, %d max\n",
	       ACT_STATIC_KLUDGE-i, ACT_STATIC_KLUDGE, ACT_STATIC_KLUDGE-amin);
}

dump_act(act)
	Act *act;
{
	act_count();
	kact_count();
	while (act) {
		printf("%08.8x: thread=%x, task=%x, hi=%x, lo=%x, ref=%x\n",
		       act, act->thread, act->task,
		       act->higher, act->lower, act->ref_count);
		printf("\talerts=%x, mask=%x, susp=%x, active=%x\n",
		       act->alerts, act->alert_mask,
		       act->suspend_count, act->active);
		machine_dump_act(&act->mact);
		if (act == act->lower)
			break;
		act = act->lower;
	}
}

#ifdef ACTWATCH
Act *
get_next_act(sp)
	int sp;
{
	static int i;
	Act *act;

	while (1) {
		if (i == ACT_STATIC_KLUDGE) {
			i = 0;
			return 0;
		}
		act = &free_acts[i];
		i++;
		if (act->mact.space == sp)
			return act;
	}
}
#endif

#endif /* MIGRATING_THREADS */
