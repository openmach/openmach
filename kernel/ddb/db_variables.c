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

#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_task_thread.h>

extern unsigned long	db_maxoff;

extern db_expr_t	db_radix;
extern db_expr_t	db_max_width;
extern db_expr_t	db_tab_stop_width;
extern db_expr_t	db_max_line;
extern int	db_set_default_thread();
extern int	db_get_task_thread();
extern int	db_arg_variable();

#define DB_NWORK	32		/* number of work variable */

db_expr_t	db_work[DB_NWORK];		/* work variable */

struct db_variable db_vars[] = {
	{ "radix",	&db_radix,		FCN_NULL },
	{ "maxoff",	(db_expr_t*)&db_maxoff,	FCN_NULL },
	{ "maxwidth",	&db_max_width,		FCN_NULL },
	{ "tabstops",	&db_tab_stop_width,	FCN_NULL },
	{ "lines",	&db_max_line,		FCN_NULL },
	{ "thread",	0,			db_set_default_thread	},
	{ "task",	0,			db_get_task_thread,
	  1,		2,			-1,	-1		},
	{ "work",	&db_work[0],		FCN_NULL,
	  1,		1,			0,	DB_NWORK-1	},
	{ "arg",	0,			db_arg_variable,
	  1,		1,			-1,	-1		},
};
struct db_variable *db_evars = db_vars + sizeof(db_vars)/sizeof(db_vars[0]);

char *
db_get_suffix(suffix, suffix_value)
	register char	*suffix;
	short		*suffix_value;
{
	register value;

	for (value = 0; *suffix && *suffix != '.' && *suffix != ':'; suffix++) {
	    if (*suffix < '0' || *suffix > '9')
		return(0);
	    value = value*10 + *suffix - '0';
	}
	*suffix_value = value;
	if (*suffix == '.')
	    suffix++;
	return(suffix);
}
	
static boolean_t
db_cmp_variable_name(vp, name, ap)
	struct db_variable		*vp;
	char				*name;
	register db_var_aux_param_t	ap;
{
	register char *var_np, *np;
	register level;
	
	for (np = name, var_np = vp->name; *var_np; ) {
	    if (*np++ != *var_np++)
		return(FALSE);
	}
	for (level = 0; *np && *np != ':' && level < vp->max_level; level++){
	    if ((np = db_get_suffix(np, &ap->suffix[level])) == 0)
		return(FALSE);
	}
	if ((*np && *np != ':') || level < vp->min_level
	    || (level > 0 && (ap->suffix[0] < vp->low 
		  	      || (vp->high >= 0 && ap->suffix[0] > vp->high))))
	    return(FALSE);
	db_strcpy(ap->modif, (*np)? np+1: "");
	ap->thread = (db_option(ap->modif, 't')?db_default_thread: THREAD_NULL);
	ap->level = level;
	return(TRUE);
}

int
db_find_variable(varp, ap)
	struct db_variable	**varp;
	db_var_aux_param_t	ap;
{
	int	t;
	struct db_variable *vp;

	t = db_read_token();
	if (t == tIDENT) {
	    for (vp = db_vars; vp < db_evars; vp++) {
		if (db_cmp_variable_name(vp, db_tok_string, ap)) {
		    *varp = vp;
		    return (1);
		}
	    }
	    for (vp = db_regs; vp < db_eregs; vp++) {
		if (db_cmp_variable_name(vp, db_tok_string, ap)) {
		    *varp = vp;
		    return (1);
		}
	    }
	}
	db_printf("Unknown variable \"$%s\"\n", db_tok_string);
	db_error(0);
	return (0);
}


void db_read_write_variable(); /* forward */

int
db_get_variable(valuep)
	db_expr_t	*valuep;
{
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	if (!db_find_variable(&vp, &aux_param))
	    return (0);

	db_read_write_variable(vp, valuep, DB_VAR_GET, &aux_param);

	return (1);
}

int
db_set_variable(value)
	db_expr_t	value;
{
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	if (!db_find_variable(&vp, &aux_param))
	    return (0);

	db_read_write_variable(vp, &value, DB_VAR_SET, &aux_param);

	return (1);
}

void
db_read_write_variable(vp, valuep, rw_flag, ap)
	struct db_variable	*vp;
	db_expr_t		*valuep;
	int 			rw_flag;
	db_var_aux_param_t	ap;
{
	int	(*func)() = vp->fcn;
	struct  db_var_aux_param aux_param;

	if (ap == 0) {
	    ap = &aux_param;
	    ap->modif = "";
	    ap->level = 0;
	    ap->thread = THREAD_NULL;
	}
	if (func == FCN_NULL) {
	    if (rw_flag == DB_VAR_SET)
	        vp->valuep[(ap->level)? (ap->suffix[0] - vp->low): 0] = *valuep;
	    else
	        *valuep = vp->valuep[(ap->level)? (ap->suffix[0] - vp->low): 0];
	} else
	    (*func)(vp, valuep, rw_flag, ap);
}

void
db_set_cmd()
{
	db_expr_t	value;
	int		t;
	struct db_variable *vp;
	struct db_var_aux_param aux_param;
	char		modif[TOK_STRING_SIZE];

	aux_param.modif = modif;
	t = db_read_token();
	if (t != tDOLLAR) {
	    db_error("Variable name should be prefixed with $\n");
	    return;
	}
	if (!db_find_variable(&vp, &aux_param)) {
	    db_error("Unknown variable\n");
	    return;
	}

	t = db_read_token();
	if (t != tEQ)
	    db_unread_token(t);

	if (!db_expression(&value)) {
	    db_error("No value\n");
	    return;
	}
	if ((t = db_read_token()) == tSEMI_COLON)
	    db_unread_token(t);
	else if (t != tEOL)
	    db_error("?\n");

	db_read_write_variable(vp, &value, DB_VAR_SET, &aux_param);
}

#endif MACH_KDB
