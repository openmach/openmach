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
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include "mach_kdb.h"
#if MACH_KDB

/*
 * Trap entry point to kernel debugger.
 */
#include <mach/boolean.h>
#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#include <ddb/db_access.h>
#include <ddb/db_break.h>
#include <ddb/db_task_thread.h>



extern jmp_buf_t *db_recover;

extern void		db_restart_at_pc();
extern boolean_t	db_stop_at_pc();

extern int		db_inst_count;
extern int		db_load_count;
extern int		db_store_count;

void
db_task_trap(type, code, user_space)
	int	  type, code;
	boolean_t user_space;
{
	jmp_buf_t db_jmpbuf;
	jmp_buf_t *prev;
	boolean_t	bkpt;
	boolean_t	watchpt;
	void		db_init_default_thread();
	void		db_check_breakpoint_valid();
	task_t		task_space;

	task_space = db_target_space(current_thread(), user_space);
	bkpt = IS_BREAKPOINT_TRAP(type, code);
	watchpt = IS_WATCHPOINT_TRAP(type, code);

	db_init_default_thread();
	db_check_breakpoint_valid();
	if (db_stop_at_pc(&bkpt, task_space)) {
	    if (db_inst_count) {
		db_printf("After %d instructions (%d loads, %d stores),\n",
			  db_inst_count, db_load_count, db_store_count);
	    }
	    if (bkpt)
		db_printf("Breakpoint at  ");
	    else if (watchpt)
		db_printf("Watchpoint at  ");
	    else
		db_printf("Stopped at  ");
	    db_dot = PC_REGS(DDB_REGS);

	    prev = db_recover;
	    if (_setjmp(db_recover = &db_jmpbuf) == 0)
		db_print_loc_and_inst(db_dot, task_space);
	    else
		db_printf("Trouble printing location %#X.\n", db_dot);
	    db_recover = prev;

	    db_command_loop();
	}

	db_restart_at_pc(watchpt, task_space);
}

void
db_trap(type, code)
	int	type, code;
{
	db_task_trap(type, code, !DB_VALID_KERN_ADDR(PC_REGS(DDB_REGS)));
}

#endif MACH_KDB
