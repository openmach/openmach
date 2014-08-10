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
 *
 *	This file contains routines to check whether an ast is needed.
 *
 *	ast_check() - check whether ast is needed for interrupt or context
 *	switch.  Usually called by clock interrupt handler.
 *
 */

#include <cpus.h>
#include <mach_fixpri.h>
#include <norma_ipc.h>

#include <kern/ast.h>
#include <kern/counters.h>
#include "cpu_number.h"
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/processor.h>

#include <machine/machspl.h>	/* for splsched */

#if	MACH_FIXPRI
#include <mach/policy.h>
#endif	MACH_FIXPRI


volatile ast_t need_ast[NCPUS];

void
ast_init()
{
#ifndef	MACHINE_AST
	register int i;

	for (i=0; i<NCPUS; i++)
		need_ast[i] = 0;
#endif	MACHINE_AST
}

void
ast_taken()
{
	register thread_t self = current_thread();
	register ast_t reasons;

	/*
	 *	Interrupts are still disabled.
	 *	We must clear need_ast and then enable interrupts.
	 */

	reasons = need_ast[cpu_number()];
	need_ast[cpu_number()] = AST_ZILCH;
	(void) spl0();

	/*
	 *	These actions must not block.
	 */

	if (reasons & AST_NETWORK)
		net_ast();

#if	NORMA_IPC
	if (reasons & AST_NETIPC)
		netipc_ast();
#endif	NORMA_IPC

	/*
	 *	Make darn sure that we don't call thread_halt_self
	 *	or thread_block from the idle thread.
	 */

	if (self != current_processor()->idle_thread) {
#ifndef MIGRATING_THREADS
		while (thread_should_halt(self))
			thread_halt_self();
#endif

		/*
		 *	One of the previous actions might well have
		 *	woken a high-priority thread, so we use
		 *	csw_needed in addition to AST_BLOCK.
		 */

		if ((reasons & AST_BLOCK) ||
		    csw_needed(self, current_processor())) {
			counter(c_ast_taken_block++);
			thread_block(thread_exception_return);
		}
	}
}

void
ast_check()
{
	register int		mycpu = cpu_number();
	register processor_t	myprocessor;
	register thread_t	thread = current_thread();
	register run_queue_t	rq;
	spl_t			s = splsched();

	/*
	 *	Check processor state for ast conditions.
	 */
	myprocessor = cpu_to_processor(mycpu);
	switch(myprocessor->state) {
	    case PROCESSOR_OFF_LINE:
	    case PROCESSOR_IDLE:
	    case PROCESSOR_DISPATCHING:
		/*
		 *	No ast.
		 */
	    	break;

#if	NCPUS > 1
	    case PROCESSOR_ASSIGN:
	    case PROCESSOR_SHUTDOWN:
	        /*
		 * 	Need ast to force action thread onto processor.
		 *
		 * XXX  Should check if action thread is already there.
		 */
		ast_on(mycpu, AST_BLOCK);
		break;
#endif	NCPUS > 1

	    case PROCESSOR_RUNNING:

		/*
		 *	Propagate thread ast to processor.  If we already
		 *	need an ast, don't look for more reasons.
		 */
		ast_propagate(thread, mycpu);
		if (ast_needed(mycpu))
			break;

		/*
		 *	Context switch check.  The csw_needed macro isn't
		 *	used here because the rq->low hint may be wrong,
		 *	and fixing it here avoids an extra ast.
		 *	First check the easy cases.
		 */
		if (thread->state & TH_SUSP || myprocessor->runq.count > 0) {
			ast_on(mycpu, AST_BLOCK);
			break;
		}

		/*
		 *	Update lazy evaluated runq->low if only timesharing.
		 */
#if	MACH_FIXPRI
		if (myprocessor->processor_set->policies & POLICY_FIXEDPRI) {
		    if (csw_needed(thread,myprocessor)) {
			ast_on(mycpu, AST_BLOCK);
			break;
		    }
		    else {
			/*
			 *	For fixed priority threads, set first_quantum
			 *	so entire new quantum is used.
			 */
			if (thread->policy == POLICY_FIXEDPRI)
			    myprocessor->first_quantum = TRUE;
		    }
		}
		else {
#endif	MACH_FIXPRI			
		rq = &(myprocessor->processor_set->runq);
		if (!(myprocessor->first_quantum) && (rq->count > 0)) {
		    register queue_t 	q;
		    /*
		     *	This is not the first quantum, and there may
		     *	be something in the processor_set runq.
		     *	Check whether low hint is accurate.
		     */
		    q = rq->runq + *(volatile int *)&rq->low;
		    if (queue_empty(q)) {
			register int i;

			/*
			 *	Need to recheck and possibly update hint.
			 */
			simple_lock(&rq->lock);
			q = rq->runq + rq->low;
			if (rq->count > 0) {
			    for (i = rq->low; i < NRQS; i++) {
				if(!(queue_empty(q)))
				    break;
				q++;
			    }
			    rq->low = i;
			}
			simple_unlock(&rq->lock);
		    }

		    if (rq->low <= thread->sched_pri) {
			ast_on(mycpu, AST_BLOCK);
			break;
		    }
		}
#if	MACH_FIXPRI
		}
#endif	MACH_FIXPRI
		break;

	    default:
	        panic("ast_check: Bad processor state (cpu %d processor %08x) state: %d",
			mycpu, myprocessor, myprocessor->state);
	}

	(void) splx(s);
}
