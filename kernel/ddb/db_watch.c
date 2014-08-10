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
 * 	Author: Richard P. Draves, Carnegie Mellon University
 *	Date:	10/90
 */

#include "mach_kdb.h"
#if MACH_KDB

#include <mach/boolean.h>
#include <mach/vm_param.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/vm_param.h>
#include <vm/vm_map.h>

#include <machine/db_machdep.h>
#include <ddb/db_lex.h>
#include <ddb/db_watch.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>



/*
 * Watchpoints.
 */

boolean_t	db_watchpoints_inserted = TRUE;

#define	NWATCHPOINTS	100
struct db_watchpoint	db_watch_table[NWATCHPOINTS];
db_watchpoint_t		db_next_free_watchpoint = &db_watch_table[0];
db_watchpoint_t		db_free_watchpoints = 0;
db_watchpoint_t		db_watchpoint_list = 0;

extern vm_map_t		kernel_map;

db_watchpoint_t
db_watchpoint_alloc()
{
	register db_watchpoint_t	watch;

	if ((watch = db_free_watchpoints) != 0) {
	    db_free_watchpoints = watch->link;
	    return (watch);
	}
	if (db_next_free_watchpoint == &db_watch_table[NWATCHPOINTS]) {
	    db_printf("All watchpoints used.\n");
	    return (0);
	}
	watch = db_next_free_watchpoint;
	db_next_free_watchpoint++;

	return (watch);
}

void
db_watchpoint_free(watch)
	register db_watchpoint_t	watch;
{
	watch->link = db_free_watchpoints;
	db_free_watchpoints = watch;
}

void
db_set_watchpoint(task, addr, size)
	task_t		task;
	db_addr_t	addr;
	vm_size_t	size;
{
	register db_watchpoint_t	watch;

	/*
	 *	Should we do anything fancy with overlapping regions?
	 */

	for (watch = db_watchpoint_list; watch != 0; watch = watch->link) {
	    if (watch->task == task &&
		(watch->loaddr == addr) &&
		(watch->hiaddr == addr+size)) {
		db_printf("Already set.\n");
		return;
	    }
	}

	watch = db_watchpoint_alloc();
	if (watch == 0) {
	    db_printf("Too many watchpoints.\n");
	    return;
	}

	watch->task = task;
	watch->loaddr = addr;
	watch->hiaddr = addr+size;

	watch->link = db_watchpoint_list;
	db_watchpoint_list = watch;

	db_watchpoints_inserted = FALSE;
}

void
db_delete_watchpoint(task, addr)
	task_t		task;
	db_addr_t	addr;
{
	register db_watchpoint_t	watch;
	register db_watchpoint_t	*prev;

	for (prev = &db_watchpoint_list; (watch = *prev) != 0;
	     prev = &watch->link) {
	    if (watch->task == task &&
		(watch->loaddr <= addr) &&
		(addr < watch->hiaddr)) {
		*prev = watch->link;
		db_watchpoint_free(watch);
		return;
	    }
	}

	db_printf("Not set.\n");
}

void
db_list_watchpoints()
{
	register db_watchpoint_t watch;
	int	 task_id;

	if (db_watchpoint_list == 0) {
	    db_printf("No watchpoints set\n");
	    return;
	}

	db_printf("Space      Address  Size\n");
	for (watch = db_watchpoint_list; watch != 0; watch = watch->link)  {
	    if (watch->task == TASK_NULL)
		db_printf("kernel  ");
	    else {
		task_id = db_lookup_task(watch->task);
		if (task_id < 0)
		    db_printf("%*X", 2*sizeof(vm_offset_t), watch->task);
		else
		    db_printf("task%-3d ", task_id);
	    }
	    db_printf("  %*X  %X\n", 2*sizeof(vm_offset_t), watch->loaddr,
		      watch->hiaddr - watch->loaddr);
	}
}

static int
db_get_task(modif, taskp, addr)
	char		*modif;
	task_t		*taskp;
	db_addr_t	addr;
{
	task_t		task = TASK_NULL;
	db_expr_t	value;
	boolean_t	user_space;

	user_space = db_option(modif, 'T');
	if (user_space) {
	    if (db_expression(&value)) {
		task = (task_t)value;
		if (db_lookup_task(task) < 0) {
		    db_printf("bad task address %X\n", task);
		    return(-1);
		}
	    } else {
		task = db_default_task;
		if (task == TASK_NULL) {
		    if ((task = db_current_task()) == TASK_NULL) {
			db_printf("no task\n");
			return(-1);
		    }
		}
	    }
	}
	if (!DB_VALID_ADDRESS(addr, user_space)) {
	    db_printf("Address %#X is not in %s space\n", addr, 
			(user_space)? "user": "kernel");
	    return(-1);
	}
	*taskp = task;
	return(0);
}

/* Delete watchpoint */
/*ARGSUSED*/
void
db_deletewatch_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	task_t		task;

	if (db_get_task(modif, &task, addr) < 0)
	    return;
	db_delete_watchpoint(task, addr);
}

/* Set watchpoint */
/*ARGSUSED*/
void
db_watchpoint_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	vm_size_t	size;
	db_expr_t	value;
	task_t		task;
	boolean_t	db_option();

	if (db_get_task(modif, &task, addr) < 0)
	    return;
	if (db_expression(&value))
	    size = (vm_size_t) value;
	else
	    size = sizeof(int);
	db_set_watchpoint(task, addr, size);
}

/* list watchpoints */
void
db_listwatch_cmd()
{
	db_list_watchpoints();
}

void
db_set_watchpoints()
{
	register db_watchpoint_t	watch;
	vm_map_t			map;

	if (!db_watchpoints_inserted) {
	    for (watch = db_watchpoint_list; watch != 0; watch = watch->link) {
		map = (watch->task)? watch->task->map: kernel_map;
		pmap_protect(map->pmap,
			     trunc_page(watch->loaddr),
			     round_page(watch->hiaddr),
			     VM_PROT_READ);
	    }
	    db_watchpoints_inserted = TRUE;
	}
}

void
db_clear_watchpoints()
{
	db_watchpoints_inserted = FALSE;
}

boolean_t
db_find_watchpoint(map, addr, regs)
	vm_map_t	map;
	db_addr_t	addr;
	db_regs_t	*regs;
{
	register db_watchpoint_t watch;
	db_watchpoint_t found = 0;
	register task_t	task_space;

	task_space = (map == kernel_map)? TASK_NULL: db_current_task();
	for (watch = db_watchpoint_list; watch != 0; watch = watch->link) {
	    if (watch->task == task_space) {
		if ((watch->loaddr <= addr) && (addr < watch->hiaddr))
		    return (TRUE);
		else if ((trunc_page(watch->loaddr) <= addr) &&
			 (addr < round_page(watch->hiaddr)))
		    found = watch;
	    }
	}

	/*
	 *	We didn't hit exactly on a watchpoint, but we are
	 *	in a protected region.  We want to single-step
	 *	and then re-protect.
	 */

	if (found) {
	    db_watchpoints_inserted = FALSE;
	    db_single_step(regs, task_space);
	}

	return (FALSE);
}

#endif MACH_KDB
