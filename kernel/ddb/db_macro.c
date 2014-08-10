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

#include <kern/thread.h>

#include <machine/db_machdep.h>
#include <ddb/db_lex.h>
#include <ddb/db_variables.h>
#include <ddb/db_command.h>



/*
 * debugger macro support
 */

#define DB_MACRO_LEVEL	5		/* max macro nesting */
#define DB_NARGS	10		/* max args */
#define DB_NUSER_MACRO	10		/* max user macros */

int		db_macro_free = DB_NUSER_MACRO;
struct db_user_macro {
	char	m_name[TOK_STRING_SIZE];
	char	m_lbuf[DB_LEX_LINE_SIZE];
	int	m_size;
} db_user_macro[DB_NUSER_MACRO];

int		db_macro_level = 0;
db_expr_t	db_macro_args[DB_MACRO_LEVEL][DB_NARGS];

static struct db_user_macro *
db_lookup_macro(name)
	char *name;
{
	register struct db_user_macro *mp;

	for (mp = db_user_macro; mp < &db_user_macro[DB_NUSER_MACRO]; mp++) {
	    if (mp->m_name[0] == 0)
		continue;
	    if (strcmp(mp->m_name, name) == 0)
		return(mp);
	}
	return(0);
}

void
db_def_macro_cmd()
{
	register char *p;
	register c;
	register struct db_user_macro *mp, *ep;

	if (db_read_token() != tIDENT) {
	    db_printf("Bad macro name \"%s\"\n", db_tok_string);
	    db_error(0);
	    /* NOTREACHED */
	}
	if ((mp = db_lookup_macro(db_tok_string)) == 0) {
	    if (db_macro_free <= 0)
		db_error("Too many macros\n");
		/* NOTREACHED */
	    ep = &db_user_macro[DB_NUSER_MACRO];
	    for (mp = db_user_macro; mp < ep && mp->m_name[0]; mp++);
	    if (mp >= ep)
		db_error("ddb: internal error(macro)\n");
		/* NOTREACHED */
	    db_macro_free--;
	    db_strcpy(mp->m_name, db_tok_string);
	}
	for (c = db_read_char(); c == ' ' || c == '\t'; c = db_read_char());
	for (p = mp->m_lbuf; c > 0; c = db_read_char())
	    *p++ = c;
	*p = 0;
	mp->m_size = p - mp->m_lbuf;
}

void
db_del_macro_cmd()
{
	register struct db_user_macro *mp;

	if (db_read_token() != tIDENT 
	    || (mp = db_lookup_macro(db_tok_string)) == 0) {
	    db_printf("No such macro \"%s\"\n", db_tok_string);
	    db_error(0);
	    /* NOTREACHED */
	} else {
	    mp->m_name[0] = 0;
	    db_macro_free++;
	}
}

void
db_show_macro()
{
	register struct db_user_macro *mp;
	int  t;
	char *name = 0;

	if ((t = db_read_token()) == tIDENT)
	    name = db_tok_string;
	else
	    db_unread_token(t);
	for (mp = db_user_macro; mp < &db_user_macro[DB_NUSER_MACRO]; mp++) {
	    if (mp->m_name[0] == 0)
		continue;
	    if (name && strcmp(mp->m_name, name))
		continue;
	    db_printf("%s: %s", mp->m_name, mp->m_lbuf);
	}
}
	
int
db_exec_macro(name)
	char *name;
{
	register struct db_user_macro *mp;
	register n;

	if ((mp = db_lookup_macro(name)) == 0)
	    return(-1);
	if (db_macro_level+1 >= DB_MACRO_LEVEL) {
	    db_macro_level = 0;
	    db_error("Too many macro nest\n");
	    /* NOTREACHED */
	}
	for (n = 0;
	     n < DB_NARGS && 
	     db_expression(&db_macro_args[db_macro_level+1][n]);
	     n++);
	while (n < DB_NARGS)
	    db_macro_args[db_macro_level+1][n++] = 0;
	db_macro_level++;
	db_exec_cmd_nest(mp->m_lbuf, mp->m_size);
	db_macro_level--;
	return(0);
}

int
/* ARGSUSED */
db_arg_variable(vp, valuep, flag, ap)
	struct db_variable	*vp;
	db_expr_t		*valuep;
	int			flag;
	db_var_aux_param_t	ap;
{
	if (ap->level != 1 || ap->suffix[0] < 1 || ap->suffix[0] > DB_NARGS) {
	    db_error("Bad $arg variable\n");
	    /* NOTREACHED */
	}
	if (flag == DB_VAR_GET)
	    *valuep = db_macro_args[db_macro_level][ap->suffix[0]-1];
	else
	    db_macro_args[db_macro_level][ap->suffix[0]-1] = *valuep;
	return(0);
}

#endif MACH_KDB
