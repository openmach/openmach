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
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#include "mach_kdb.h"
#if MACH_KDB


/*
 * Breakpoints.
 */
#include <mach/boolean.h>
#include <machine/db_machdep.h>
#include <ddb/db_lex.h>
#include <ddb/db_break.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_command.h>
#include <ddb/db_task_thread.h>

#define	NBREAKPOINTS	100
#define NTHREAD_LIST	(NBREAKPOINTS*3)

struct db_breakpoint	db_break_table[NBREAKPOINTS];
db_breakpoint_t		db_next_free_breakpoint = &db_break_table[0];
db_breakpoint_t		db_free_breakpoints = 0;
db_breakpoint_t		db_breakpoint_list = 0;

static struct db_thread_breakpoint	db_thread_break_list[NTHREAD_LIST];
static db_thread_breakpoint_t		db_free_thread_break_list = 0;
static boolean_t			db_thread_break_init = FALSE;
static int				db_breakpoint_number = 0;

db_breakpoint_t
db_breakpoint_alloc()
{
	register db_breakpoint_t	bkpt;

	if ((bkpt = db_free_breakpoints) != 0) {
	    db_free_breakpoints = bkpt->link;
	    return (bkpt);
	}
	if (db_next_free_breakpoint == &db_break_table[NBREAKPOINTS]) {
	    db_printf("All breakpoints used.\n");
	    return (0);
	}
	bkpt = db_next_free_breakpoint;
	db_next_free_breakpoint++;

	return (bkpt);
}

void
db_breakpoint_free(bkpt)
	register db_breakpoint_t	bkpt;
{
	bkpt->link = db_free_breakpoints;
	db_free_breakpoints = bkpt;
}

static int
db_add_thread_breakpoint(bkpt, task_thd, count, task_bpt)
	register db_breakpoint_t bkpt;
	vm_offset_t task_thd;
	boolean_t task_bpt;
{
	register db_thread_breakpoint_t tp;

	if (db_thread_break_init == FALSE) {
	    for (tp = db_thread_break_list; 
		tp < &db_thread_break_list[NTHREAD_LIST-1]; tp++)
		tp->tb_next = tp+1;
	    tp->tb_next = 0;
	    db_free_thread_break_list = db_thread_break_list;
	    db_thread_break_init = TRUE;
	}
	if (db_free_thread_break_list == 0)
	    return (-1);
	tp = db_free_thread_break_list;
	db_free_thread_break_list = tp->tb_next;
	tp->tb_is_task = task_bpt;
	tp->tb_task_thd = task_thd;
	tp->tb_count = count;
	tp->tb_init_count = count;
	tp->tb_cond = 0;
	tp->tb_number = ++db_breakpoint_number;
	tp->tb_next = bkpt->threads;
	bkpt->threads = tp;
	return(0);
}

static int
db_delete_thread_breakpoint(bkpt, task_thd)
	register db_breakpoint_t bkpt;
	vm_offset_t task_thd;
{
	register db_thread_breakpoint_t tp;
	register db_thread_breakpoint_t *tpp;
	void	 db_cond_free();

	if (task_thd == 0) {
	    /* delete all the thread-breakpoints */

	    for (tpp = &bkpt->threads; (tp = *tpp) != 0; tpp = &tp->tb_next)
		db_cond_free(tp);

	    *tpp = db_free_thread_break_list;
	    db_free_thread_break_list = bkpt->threads;
	    bkpt->threads = 0;
	    return 0;
	} else {
	    /* delete the specified thread-breakpoint */

	    for (tpp = &bkpt->threads; (tp = *tpp) != 0; tpp = &tp->tb_next)
		if (tp->tb_task_thd == task_thd) {
		    db_cond_free(tp);
		    *tpp = tp->tb_next;
		    tp->tb_next = db_free_thread_break_list;
		    db_free_thread_break_list = tp;
		    return 0;
		}

	    return -1;	/* not found */
	}
}

static db_thread_breakpoint_t
db_find_thread_breakpoint(bkpt, thread)
	db_breakpoint_t bkpt;
	thread_t thread;
{
	register db_thread_breakpoint_t tp;
	register task_t task = (thread == THREAD_NULL)? TASK_NULL: thread->task;

	for (tp = bkpt->threads; tp; tp = tp->tb_next) {
	    if (tp->tb_is_task) {
		if (tp->tb_task_thd == (vm_offset_t)task)
		    break;
		continue;
	    }
	    if (tp->tb_task_thd == (vm_offset_t)thread || tp->tb_task_thd == 0)
		break;
	}
	return(tp);
}

db_thread_breakpoint_t
db_find_thread_breakpoint_here(task, addr)
	task_t		task;
	db_addr_t	addr;
{
	db_breakpoint_t bkpt;

	bkpt = db_find_breakpoint(task, (db_addr_t)addr);
	if (bkpt == 0)
	    return(0);
	return(db_find_thread_breakpoint(bkpt, current_thread()));
}

db_thread_breakpoint_t
db_find_breakpoint_number(num, bkptp)
	int num;
	db_breakpoint_t *bkptp;
{
	register db_thread_breakpoint_t tp;
	register db_breakpoint_t bkpt;

	for (bkpt = db_breakpoint_list; bkpt != 0; bkpt = bkpt->link) {
	    for (tp = bkpt->threads; tp; tp = tp->tb_next) {
		if (tp->tb_number == num) {
		    if (bkptp)
			*bkptp = bkpt;
		    return(tp);
		}
	    }
	}
	return(0);
}

static void
db_force_delete_breakpoint(bkpt, task_thd, is_task)
	db_breakpoint_t	bkpt;
	vm_offset_t  task_thd;
	boolean_t is_task;
{
	db_printf("deleted a stale breakpoint at ");
	if (bkpt->task == TASK_NULL || db_lookup_task(bkpt->task) >= 0)
	   db_task_printsym(bkpt->address, DB_STGY_PROC, bkpt->task);
	else
	   db_printf("%#X", bkpt->address);
	if (bkpt->task)
	   db_printf(" in task %X", bkpt->task);
	if (task_thd)
	   db_printf(" for %s %X", (is_task)? "task": "thread", task_thd);
	db_printf("\n");
	db_delete_thread_breakpoint(bkpt, task_thd);
}

void
db_check_breakpoint_valid()
{
	register db_thread_breakpoint_t tbp, tbp_next;
	register db_breakpoint_t bkpt, *bkptp;

	bkptp = &db_breakpoint_list;
	for (bkpt = *bkptp; bkpt; bkpt = *bkptp) {
	    if (bkpt->task != TASK_NULL) {
		if (db_lookup_task(bkpt->task) < 0) {
		    db_force_delete_breakpoint(bkpt, 0, FALSE);
		    *bkptp = bkpt->link;
		    db_breakpoint_free(bkpt);
		    continue;
		}
	    } else {
		for (tbp = bkpt->threads; tbp; tbp = tbp_next) {
		    tbp_next = tbp->tb_next;
		    if (tbp->tb_task_thd == 0)
			continue;
		    if ((tbp->tb_is_task && 
			 db_lookup_task((task_t)(tbp->tb_task_thd)) < 0) ||
			(!tbp->tb_is_task && 
			 db_lookup_thread((thread_t)(tbp->tb_task_thd)) < 0)) {
			db_force_delete_breakpoint(bkpt, 
					tbp->tb_task_thd, tbp->tb_is_task);
		    }
		}
		if (bkpt->threads == 0) {
		    db_put_task_value(bkpt->address, BKPT_SIZE,
				 bkpt->bkpt_inst, bkpt->task);
		    *bkptp = bkpt->link;
		    db_breakpoint_free(bkpt);
		    continue;
		}
	    }
	    bkptp = &bkpt->link;
	}
}

void
db_set_breakpoint(task, addr, count, thread, task_bpt)
	task_t		task;
	db_addr_t	addr;
	int		count;
	thread_t	thread;
	boolean_t	task_bpt;
{
	register db_breakpoint_t bkpt;
	db_breakpoint_t alloc_bkpt = 0;
	vm_offset_t task_thd;

	bkpt = db_find_breakpoint(task, addr);
	if (bkpt) {
	    if (thread == THREAD_NULL
		|| db_find_thread_breakpoint(bkpt, thread)) {
		db_printf("Already set.\n");
		return;
	    }
	} else {
	    if (!DB_CHECK_ACCESS(addr, BKPT_SIZE, task)) {
		db_printf("Cannot set break point at %X\n", addr);
		return;
	    }
	    alloc_bkpt = bkpt = db_breakpoint_alloc();
	    if (bkpt == 0) {
		db_printf("Too many breakpoints.\n");
		return;
	    }
	    bkpt->task = task;
	    bkpt->flags = (task && thread == THREAD_NULL)?
				(BKPT_USR_GLOBAL|BKPT_1ST_SET): 0;
	    bkpt->address = addr;
	    bkpt->threads = 0;
	}
	if (db_breakpoint_list == 0)
	    db_breakpoint_number = 0;
	task_thd = (task_bpt)? (vm_offset_t)(thread->task): (vm_offset_t)thread;
	if (db_add_thread_breakpoint(bkpt, task_thd, count, task_bpt) < 0) {
	    if (alloc_bkpt)
		db_breakpoint_free(alloc_bkpt);
	    db_printf("Too many thread_breakpoints.\n");
	} else {
	    db_printf("set breakpoint #%d\n", db_breakpoint_number);
	    if (alloc_bkpt) {
		bkpt->link = db_breakpoint_list;
		db_breakpoint_list = bkpt;
	    }
	}
}

void
db_delete_breakpoint(task, addr, task_thd)
	task_t	task;
	db_addr_t	addr;
	vm_offset_t	task_thd;
{
	register db_breakpoint_t	bkpt;
	register db_breakpoint_t	*prev;

	for (prev = &db_breakpoint_list; (bkpt = *prev) != 0;
					     prev = &bkpt->link) {
	    if ((bkpt->task == task
		   || (task != TASK_NULL && (bkpt->flags & BKPT_USR_GLOBAL)))
		&& bkpt->address == addr)
		break;
	}
	if (bkpt && (bkpt->flags & BKPT_SET_IN_MEM)) {
	    db_printf("cannot delete it now.\n");
	    return;
	}
	if (bkpt == 0
	    || db_delete_thread_breakpoint(bkpt, task_thd) < 0) {
	    db_printf("Not set.\n");
	    return;
	}
	if (bkpt->threads == 0) {
	    *prev = bkpt->link;
	    db_breakpoint_free(bkpt);
	}
}

db_breakpoint_t
db_find_breakpoint(task, addr)
	task_t	task;
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;

	for (bkpt = db_breakpoint_list; bkpt != 0; bkpt = bkpt->link) {
	    if ((bkpt->task == task
		  || (task != TASK_NULL && (bkpt->flags & BKPT_USR_GLOBAL)))
		&& bkpt->address == addr)
		return (bkpt);
	}
	return (0);
}

boolean_t
db_find_breakpoint_here(task, addr)
	task_t		task;
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;

	for (bkpt = db_breakpoint_list; bkpt != 0; bkpt = bkpt->link) {
	    if ((bkpt->task == task
		   || (task != TASK_NULL && (bkpt->flags & BKPT_USR_GLOBAL)))
                && bkpt->address == addr)
		return(TRUE);
	    if ((bkpt->flags & BKPT_USR_GLOBAL) == 0 &&
		  DB_PHYS_EQ(task, (vm_offset_t)addr, bkpt->task, (vm_offset_t)bkpt->address))
		return (TRUE);
	}
	return(FALSE);
}

boolean_t	db_breakpoints_inserted = TRUE;

void
db_set_breakpoints()
{
	register db_breakpoint_t bkpt;
	register task_t	task;
	db_expr_t	inst;
	task_t		cur_task;

	cur_task = (current_thread())? current_thread()->task: TASK_NULL;
	if (!db_breakpoints_inserted) {
	    for (bkpt = db_breakpoint_list; bkpt != 0; bkpt = bkpt->link) {
		if (bkpt->flags & BKPT_SET_IN_MEM)
		    continue;
		task = bkpt->task;
		if (bkpt->flags & BKPT_USR_GLOBAL) {
		    if ((bkpt->flags & BKPT_1ST_SET) == 0) {
		        if (cur_task == TASK_NULL)
			    continue;
		        task = cur_task;
		    } else
			bkpt->flags &= ~BKPT_1ST_SET;
		}
		if (DB_CHECK_ACCESS(bkpt->address, BKPT_SIZE, task)) {
		    inst = db_get_task_value(bkpt->address, BKPT_SIZE, FALSE,
								task);
		    if (inst == BKPT_SET(inst))
			continue;
		    bkpt->bkpt_inst = inst;
		    db_put_task_value(bkpt->address,
				BKPT_SIZE,
				BKPT_SET(bkpt->bkpt_inst), task);
		    bkpt->flags |= BKPT_SET_IN_MEM;
		} else {
		    db_printf("Warning: cannot set breakpoint at %X ", 
				bkpt->address);
		    if (task)
			db_printf("in task %X\n", task);
		    else
			db_printf("in kernel space\n");
		}
	    }
	    db_breakpoints_inserted = TRUE;
	}
}

void
db_clear_breakpoints()
{
	register db_breakpoint_t bkpt, *bkptp;
	register task_t	task;
	task_t		cur_task;
	db_expr_t	inst;

	cur_task = (current_thread())? current_thread()->task: TASK_NULL;
	if (db_breakpoints_inserted) {
	    bkptp = &db_breakpoint_list;
	    for (bkpt = *bkptp; bkpt; bkpt = *bkptp) {
		task = bkpt->task;
		if (bkpt->flags & BKPT_USR_GLOBAL) {
		    if (cur_task == TASK_NULL) {
			bkptp = &bkpt->link;
			continue;
		    }
		    task = cur_task;
		}
		if ((bkpt->flags & BKPT_SET_IN_MEM)
		    && DB_CHECK_ACCESS(bkpt->address, BKPT_SIZE, task)) {
		    inst = db_get_task_value(bkpt->address, BKPT_SIZE, FALSE, 
								task);
		    if (inst != BKPT_SET(inst)) {
			if (bkpt->flags & BKPT_USR_GLOBAL) {
			    bkptp = &bkpt->link;
			    continue;
			}
			db_force_delete_breakpoint(bkpt, 0, FALSE);
			*bkptp = bkpt->link;
		        db_breakpoint_free(bkpt);
			continue;
		    }
		    db_put_task_value(bkpt->address, BKPT_SIZE,
				 bkpt->bkpt_inst, task);
		    bkpt->flags &= ~BKPT_SET_IN_MEM;
		}
		bkptp = &bkpt->link;
	    }
	    db_breakpoints_inserted = FALSE;
	}
}

/*
 * Set a temporary breakpoint.
 * The instruction is changed immediately,
 * so the breakpoint does not have to be on the breakpoint list.
 */
db_breakpoint_t
db_set_temp_breakpoint(task, addr)
	task_t		task;
	db_addr_t	addr;
{
	register db_breakpoint_t	bkpt;

	bkpt = db_breakpoint_alloc();
	if (bkpt == 0) {
	    db_printf("Too many breakpoints.\n");
	    return 0;
	}
	bkpt->task = task;
	bkpt->address = addr;
	bkpt->flags = BKPT_TEMP;
	bkpt->threads = 0;
	if (db_add_thread_breakpoint(bkpt, 0, 1, FALSE) < 0) {
	    if (bkpt)
		db_breakpoint_free(bkpt);
	    db_printf("Too many thread_breakpoints.\n");
	    return 0;
	}
	bkpt->bkpt_inst = db_get_task_value(bkpt->address, BKPT_SIZE, 
						FALSE, task);
	db_put_task_value(bkpt->address, BKPT_SIZE, 
				BKPT_SET(bkpt->bkpt_inst), task);
	return bkpt;
}

void
db_delete_temp_breakpoint(task, bkpt)
	task_t		task;
	db_breakpoint_t	bkpt;
{
	db_put_task_value(bkpt->address, BKPT_SIZE, bkpt->bkpt_inst, task);
	db_delete_thread_breakpoint(bkpt, 0);
	db_breakpoint_free(bkpt);
}

/*
 * List breakpoints.
 */
void
db_list_breakpoints()
{
	register db_breakpoint_t	bkpt;

	if (db_breakpoint_list == 0) {
	    db_printf("No breakpoints set\n");
	    return;
	}

	db_printf(" No  Space    Thread      Cnt  Address(Cond)\n");
	for (bkpt = db_breakpoint_list;
	     bkpt != 0;
	     bkpt = bkpt->link)
	{
	    register 	db_thread_breakpoint_t tp;
	    int		task_id;
	    int		thread_id;

	    if (bkpt->threads) {
		for (tp = bkpt->threads; tp; tp = tp->tb_next) {
		    db_printf("%3d  ", tp->tb_number);
		    if (bkpt->flags & BKPT_USR_GLOBAL)
			db_printf("user     ");
		    else if (bkpt->task == TASK_NULL)
			db_printf("kernel   ");
		    else if ((task_id = db_lookup_task(bkpt->task)) < 0)
			db_printf("%0*X ", 2*sizeof(vm_offset_t), bkpt->task);
		    else
			db_printf("task%-3d  ", task_id);
		    if (tp->tb_task_thd == 0) {
			db_printf("all         ");
		    } else {
			if (tp->tb_is_task) {
			    task_id = db_lookup_task((task_t)(tp->tb_task_thd));
			    if (task_id < 0)
				db_printf("%0*X    ", 2*sizeof(vm_offset_t),
					   tp->tb_task_thd);
			    else
				db_printf("task%03d     ", task_id);
			} else {
			    thread_t thd = (thread_t)(tp->tb_task_thd);
			    task_id = db_lookup_task(thd->task);
			    thread_id = db_lookup_task_thread(thd->task, thd);
			    if (task_id < 0 || thread_id < 0)
				db_printf("%0*X    ", 2*sizeof(vm_offset_t),
					   tp->tb_task_thd);
			    else	
				db_printf("task%03d.%-3d ", task_id, thread_id);
			}
		    }
	    	    db_printf("%3d  ", tp->tb_init_count);
		    db_task_printsym(bkpt->address, DB_STGY_PROC, bkpt->task);
		    if (tp->tb_cond > 0) {
			db_printf("(");
			db_cond_print(tp);
			db_printf(")");
		    }
		    db_printf("\n");
		}
	    } else {
		if (bkpt->task == TASK_NULL)
		    db_printf("  ?  kernel   ");
		else
		    db_printf("%*X ", 2*sizeof(vm_offset_t), bkpt->task);
		db_printf("(?)              ");
		db_task_printsym(bkpt->address, DB_STGY_PROC, bkpt->task);
		db_printf("\n");
	    }
	}
}

/* Delete breakpoint */
/*ARGSUSED*/
void
db_delete_cmd()
{
	register n;
	thread_t thread;
	vm_offset_t task_thd;
	boolean_t user_global = FALSE;
	boolean_t task_bpt = FALSE;
	boolean_t user_space = FALSE;
	boolean_t thd_bpt = FALSE;
	db_expr_t addr;
	int t;
	
	t = db_read_token();
	if (t == tSLASH) {
	    t = db_read_token();
	    if (t != tIDENT) {
		db_printf("Bad modifier \"%s\"\n", db_tok_string);
		db_error(0);
	    }
	    user_global = db_option(db_tok_string, 'U');
	    user_space = (user_global)? TRUE: db_option(db_tok_string, 'u');
	    task_bpt = db_option(db_tok_string, 'T');
	    thd_bpt = db_option(db_tok_string, 't');
	    if (task_bpt && user_global)
		db_error("Cannot specify both 'T' and 'U' option\n");
	    t = db_read_token();
	}
	if (t == tHASH) {
	    db_thread_breakpoint_t tbp;
	    db_breakpoint_t bkpt;

	    if (db_read_token() != tNUMBER) {
		db_printf("Bad break point number #%s\n", db_tok_string);
		db_error(0);
	    }
	    if ((tbp = db_find_breakpoint_number(db_tok_number, &bkpt)) == 0) {
	        db_printf("No such break point #%d\n", db_tok_number);
	        db_error(0);
	    }
	    db_delete_breakpoint(bkpt->task, bkpt->address, tbp->tb_task_thd);
	    return;
	}
	db_unread_token(t);
	if (!db_expression(&addr)) {
	    /*
	     *	We attempt to pick up the user_space indication from db_dot,
	     *	so that a plain "d" always works.
	     */
	    addr = (db_expr_t)db_dot;
	    if (!user_space && !DB_VALID_ADDRESS((vm_offset_t)addr, FALSE))
		user_space = TRUE;
	}
	if (!DB_VALID_ADDRESS((vm_offset_t) addr, user_space)) {
	    db_printf("Address %#X is not in %s space\n", addr, 
			(user_space)? "user": "kernel");
	    db_error(0);
	}
	if (thd_bpt || task_bpt) {
	    for (n = 0; db_get_next_thread(&thread, n); n++) {
		if (thread == THREAD_NULL)
		    db_error("No active thread\n");
		if (task_bpt) {
		    if (thread->task == TASK_NULL)
			db_error("No task\n");
		    task_thd = (vm_offset_t) (thread->task);
		} else
		    task_thd = (user_global)? 0: (vm_offset_t) thread;
		db_delete_breakpoint(db_target_space(thread, user_space),
					(db_addr_t)addr, task_thd);
	    }
	} else {
	    db_delete_breakpoint(db_target_space(THREAD_NULL, user_space),
					 (db_addr_t)addr, 0);
	}
}

/* Set breakpoint with skip count */
/*ARGSUSED*/
void
db_breakpoint_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	register n;
	thread_t thread;
	boolean_t user_global = db_option(modif, 'U');
	boolean_t task_bpt = db_option(modif, 'T');
	boolean_t user_space;

	if (count == -1)
	    count = 1;

	if (!task_bpt && db_option(modif,'t'))
	  task_bpt = TRUE;

	if (task_bpt && user_global)
	    db_error("Cannot specify both 'T' and 'U'\n");
	user_space = (user_global)? TRUE: db_option(modif, 'u');
	if (user_space && db_access_level < DB_ACCESS_CURRENT)
	    db_error("User space break point is not supported\n");
	if (!task_bpt && !DB_VALID_ADDRESS((vm_offset_t)addr, user_space)) {
	    /* if the user has explicitly specified user space,
	       do not insert a breakpoint into the kernel */
	    if (user_space)
	      db_error("Invalid user space address\n");
	    user_space = TRUE;
	    db_printf("%#X is in user space\n", addr);
	}
	if (db_option(modif, 't') || task_bpt) {
	    for (n = 0; db_get_next_thread(&thread, n); n++) {
		if (thread == THREAD_NULL)
		    db_error("No active thread\n");
		if (task_bpt && thread->task == TASK_NULL)
		    db_error("No task\n");
		if (db_access_level <= DB_ACCESS_CURRENT && user_space
			 && thread->task != db_current_task())
		    db_error("Cannot set break point in inactive user space\n");
		db_set_breakpoint(db_target_space(thread, user_space), 
					(db_addr_t)addr, count,
					(user_global)? THREAD_NULL: thread,
					task_bpt);
	    }
	} else {
	    db_set_breakpoint(db_target_space(THREAD_NULL, user_space),
				 (db_addr_t)addr,
				 count, THREAD_NULL, FALSE);
	}
}

/* list breakpoints */
void
db_listbreak_cmd()
{
	db_list_breakpoints();
}

#endif MACH_KDB
