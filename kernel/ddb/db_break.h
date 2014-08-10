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
#ifndef	_DDB_DB_BREAK_H_
#define	_DDB_DB_BREAK_H_

#include <machine/db_machdep.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <mach/boolean.h>

/*
 * thread list at the same breakpoint address
 */
struct db_thread_breakpoint {
	vm_offset_t tb_task_thd;		/* target task or thread */
	boolean_t tb_is_task;			/* task qualified */
	short	 tb_number;			/* breakpoint number */
	short	 tb_init_count;			/* skip count(initial value) */
	short	 tb_count;			/* current skip count */
	short	 tb_cond;			/* break condition */
	struct	 db_thread_breakpoint *tb_next;	/* next chain */
};

typedef struct db_thread_breakpoint *db_thread_breakpoint_t;

/*
 * Breakpoint.
 */

struct db_breakpoint {
	task_t	  task;			/* target task */
	db_addr_t address;		/* set here */
	db_thread_breakpoint_t threads; /* thread */
	int	flags;			/* flags: */
#define	BKPT_SINGLE_STEP	0x2	/* to simulate single step */
#define	BKPT_TEMP		0x4	/* temporary */
#define BKPT_USR_GLOBAL		0x8	/* global user space break point */
#define BKPT_SET_IN_MEM		0x10	/* break point is set in memory */
#define BKPT_1ST_SET		0x20	/* 1st time set of user global bkpt */
	vm_size_t	bkpt_inst;	/* saved instruction at bkpt */
	struct db_breakpoint *link;	/* link in in-use or free chain */
};

typedef struct db_breakpoint *db_breakpoint_t;

extern db_breakpoint_t	db_find_breakpoint( task_t task, db_addr_t addr);
extern boolean_t	db_find_breakpoint_here( task_t task, db_addr_t addr);
extern void		db_set_breakpoints();
extern void		db_clear_breakpoints();
extern db_thread_breakpoint_t	db_find_thread_breakpoint_here
					( task_t task, db_addr_t addr );
extern db_thread_breakpoint_t	db_find_breakpoint_number
					( int num, db_breakpoint_t *bkptp);

extern db_breakpoint_t	db_set_temp_breakpoint( task_t task, db_addr_t addr);
extern void		db_delete_temp_breakpoint
					( task_t task, db_breakpoint_t bkpt);

#endif	_DDB_DB_BREAK_H_
