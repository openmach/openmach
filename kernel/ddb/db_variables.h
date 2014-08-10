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

#ifndef	_DB_VARIABLES_H_
#define	_DB_VARIABLES_H_

#include <kern/thread.h>

/*
 * Debugger variables.
 */
struct db_variable {
	char	*name;		/* Name of variable */
	db_expr_t *valuep;	/* pointer to value of variable */
				/* function to call when reading/writing */
	int	(*fcn)(/* db_variable, db_expr_t, int, db_var_aux_param_t */);
	short	min_level;	/* number of minimum suffix levels */
	short	max_level;	/* number of maximum suffix levels */
	short	low;		/* low value of level 1 suffix */
	short	high;		/* high value of level 1 suffix */
#define DB_VAR_GET	0
#define DB_VAR_SET	1
};
#define	FCN_NULL	((int (*)())0)

#define DB_VAR_LEVEL	3	/* maximum number of suffix level */

#define db_read_variable(vp, valuep)	\
	db_read_write_variable(vp, valuep, DB_VAR_GET, 0)
#define db_write_variable(vp, valuep)	\
	db_read_write_variable(vp, valuep, DB_VAR_SET, 0)

/*
 * auxiliary parameters passed to a variable handler
 */
struct db_var_aux_param {
	char		*modif;			/* option strings */
	short		level;			/* number of levels */
	short		suffix[DB_VAR_LEVEL];	/* suffix */
	thread_t	thread;			/* target task */
};

typedef struct db_var_aux_param	*db_var_aux_param_t;
	

extern struct db_variable	db_vars[];	/* debugger variables */
extern struct db_variable	*db_evars;
extern struct db_variable	db_regs[];	/* machine registers */
extern struct db_variable	*db_eregs;

#endif	/* _DB_VARIABLES_H_ */
