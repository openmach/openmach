/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 *	Author: David B. Golub,  Carnegie Mellon University
 *	Date:	7/90
 */

#include "mach_kdb.h"
#if MACH_KDB

#include <mach/boolean.h>
#include <kern/task.h>
#include <kern/thread.h>

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>



/*
 * Write to file.
 */
/*ARGSUSED*/
void
db_write_cmd(address, have_addr, count, modif)
	db_expr_t	address;
	boolean_t	have_addr;
	db_expr_t	count;
	char *		modif;
{
	register db_addr_t	addr;
	register db_expr_t	old_value;
	db_expr_t	new_value;
	register int	size;
	boolean_t	wrote_one = FALSE;
	boolean_t	t_opt, u_opt;
	thread_t	thread;
	task_t		task;

	addr = (db_addr_t) address;

	size = db_size_option(modif, &u_opt, &t_opt);
	if (t_opt) 
	  {
	    if (!db_get_next_thread(&thread, 0))
	      return;
	    task = thread->task;
	  }
	else
	  task = db_current_task();
	
	/* if user space is not explicitly specified, 
	   look in the kernel */
	if (!u_opt)
	  task = TASK_NULL;

	if (!DB_VALID_ADDRESS(addr, u_opt)) {
	  db_printf("Bad address %#*X\n", 2*sizeof(vm_offset_t), addr);
	  return;
	}

	while (db_expression(&new_value)) {
	    old_value = db_get_task_value(addr, size, FALSE, task);
	    db_task_printsym(addr, DB_STGY_ANY, task);
	    db_printf("\t\t%#*N\t=\t%#*N\n",
	    	2*sizeof(db_expr_t), old_value,
		2*sizeof(db_expr_t), new_value);
	    db_put_task_value(addr, size, new_value, task);
	    addr += size;

	    wrote_one = TRUE;
	}

	if (!wrote_one)
	    db_error("Nothing written.\n");

	db_next = addr;
	db_prev = addr - size;
}

#endif MACH_KDB
