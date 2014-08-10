/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#include "mach_kdb.h"
#if MACH_KDB

#include <machine/db_machdep.h>
#include <ddb/db_task_thread.h>
#include <ddb/db_variables.h>



/*
 * Following constants are used to prevent infinite loop of task
 * or thread search due to the incorrect list.
 */
#define	DB_MAX_TASKID	0x10000		/* max # of tasks */
#define DB_MAX_THREADID	0x10000		/* max # of threads in a task */
#define DB_MAX_PSETS	0x10000		/* max # of processor sets */

task_t		db_default_task;	/* default target task */
thread_t	db_default_thread;	/* default target thread */

/*
 * search valid task queue, and return the queue position as the task id
 */
int
db_lookup_task(target_task)
	task_t target_task;
{
	register task_t task;
	register task_id;
	register processor_set_t pset;
	register npset = 0;

	task_id = 0;
	if (queue_first(&all_psets) == 0)
	    return(-1);
	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
	    if (npset++ >= DB_MAX_PSETS)
		return(-1);
	    if (queue_first(&pset->tasks) == 0)
		continue;
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		if (target_task == task)
		    return(task_id);
		if (task_id++ >= DB_MAX_TASKID)
		    return(-1);
	    }
	}
	return(-1);
}

/*
 * search thread queue of the task, and return the queue position
 */
int
db_lookup_task_thread(task, target_thread)
	task_t	 task;
	thread_t target_thread;
{
	register thread_t thread;
	register thread_id;

	thread_id = 0;
	if (queue_first(&task->thread_list) == 0)
	    return(-1);
	queue_iterate(&task->thread_list, thread, thread_t, thread_list) {
	    if (target_thread == thread)
		return(thread_id);
	    if (thread_id++ >= DB_MAX_THREADID)
		return(-1);
	}
	return(-1);
}

/*
 * search thread queue of every valid task, and return the queue position
 * as the thread id.
 */
int
db_lookup_thread(target_thread)
	thread_t target_thread;
{
	register thread_id;
	register task_t task;
	register processor_set_t pset;
	register ntask = 0;
	register npset = 0;

	if (queue_first(&all_psets) == 0)
	    return(-1);
	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
	    if (npset++ >= DB_MAX_PSETS)
		return(-1);
	    if (queue_first(&pset->tasks) == 0)
		continue;
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		if (ntask++ > DB_MAX_TASKID)
		    return(-1);
		if (task->thread_count == 0)
		    continue;
	        thread_id = db_lookup_task_thread(task, target_thread);
		if (thread_id >= 0)
		    return(thread_id);
	    }
	}
	return(-1);
}

/*
 * check the address is a valid thread address
 */
boolean_t
db_check_thread_address_valid(thread)
	thread_t thread;
{
	if (db_lookup_thread(thread) < 0) {
	    db_printf("Bad thread address 0x%x\n", thread);
	    db_flush_lex();
	    return(FALSE);
	} else
	    return(TRUE);
}

/*
 * convert task_id(queue postion) to task address
 */
task_t
db_lookup_task_id(task_id)
	register task_id;
{
	register task_t task;
	register processor_set_t pset;
	register npset = 0;

	if (task_id > DB_MAX_TASKID)
	    return(TASK_NULL);
	if (queue_first(&all_psets) == 0)
	    return(TASK_NULL);
	queue_iterate(&all_psets, pset, processor_set_t, all_psets) {
	    if (npset++ >= DB_MAX_PSETS)
		return(TASK_NULL);
	    if (queue_first(&pset->tasks) == 0)
		continue;
	    queue_iterate(&pset->tasks, task, task_t, pset_tasks) {
		if (task_id-- <= 0)
			return(task);
	    }
	}
	return(TASK_NULL);
}

/*
 * convert (task_id, thread_id) pair to thread address
 */
static thread_t
db_lookup_thread_id(task, thread_id)
	task_t	 task;
	register thread_id;
{
	register thread_t thread;

	
	if (thread_id > DB_MAX_THREADID)
	    return(THREAD_NULL);
	if (queue_first(&task->thread_list) == 0)
	    return(THREAD_NULL);
	queue_iterate(&task->thread_list, thread, thread_t, thread_list) {
	    if (thread_id-- <= 0)
		return(thread);
	}
	return(THREAD_NULL);
}

/*
 * get next parameter from a command line, and check it as a valid
 * thread address
 */
boolean_t
db_get_next_thread(threadp, position)
	thread_t	*threadp;
	int		position;
{
	db_expr_t	value;
	thread_t	thread;

	*threadp = THREAD_NULL;
	if (db_expression(&value)) {
	    thread = (thread_t) value;
	    if (!db_check_thread_address_valid(thread)) {
		db_flush_lex();
		return(FALSE);
	    }
	} else if (position <= 0) {
	    thread = db_default_thread;
	} else
	    return(FALSE);
	*threadp = thread;
	return(TRUE);
}

/*
 * check the default thread is still valid
 *	( it is called in entering DDB session )
 */
void
db_init_default_thread()
{
	if (db_lookup_thread(db_default_thread) < 0) {
	    db_default_thread = THREAD_NULL;
	    db_default_task = TASK_NULL;
	} else
	    db_default_task = db_default_thread->task;
}

/*
 * set or get default thread which is used when /t or :t option is specified
 * in the command line
 */
/* ARGSUSED */
int
db_set_default_thread(vp, valuep, flag)
	struct db_variable *vp;
	db_expr_t	*valuep;
	int		flag;
{
	thread_t	thread;

	if (flag != DB_VAR_SET) {
	    *valuep = (db_expr_t) db_default_thread;
	    return(0);
	}
	thread = (thread_t) *valuep;
	if (thread != THREAD_NULL && !db_check_thread_address_valid(thread))
	    db_error(0);
	    /* NOTREACHED */
	db_default_thread = thread;
	if (thread)
		db_default_task = thread->task;
	return(0);
}

/*
 * convert $taskXXX[.YYY] type DDB variable to task or thread address
 */
int
db_get_task_thread(vp, valuep, flag, ap)
	struct db_variable	*vp;
	db_expr_t		*valuep;
	int			flag;
	db_var_aux_param_t	ap;
{
	task_t	 task;
	thread_t thread;

	if (flag != DB_VAR_GET) {
	    db_error("Cannot set to $task variable\n");
	    /* NOTREACHED */
	}
	if ((task = db_lookup_task_id(ap->suffix[0])) == TASK_NULL) {
	    db_printf("no such task($task%d)\n", ap->suffix[0]);
	    db_error(0);
	    /* NOTREACHED */
	}
	if (ap->level <= 1) {
	    *valuep = (db_expr_t) task;
	    return(0);
	}
	if ((thread = db_lookup_thread_id(task, ap->suffix[1])) == THREAD_NULL){
	    db_printf("no such thread($task%d.%d)\n", 
					ap->suffix[0], ap->suffix[1]);
	    db_error(0);
	    /* NOTREACHED */
	}
	*valuep = (db_expr_t) thread;
	return(0);
}

#endif MACH_KDB
