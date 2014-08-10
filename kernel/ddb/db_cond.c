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
#include <machine/setjmp.h>

#include <ddb/db_lex.h>
#include <ddb/db_break.h>
#include <ddb/db_command.h>



#define DB_MAX_COND	10		/* maximum conditions to be set */

int   db_ncond_free = DB_MAX_COND;			/* free condition */
struct db_cond {
	int	c_size;					/* size of cond */
	char	c_cond_cmd[DB_LEX_LINE_SIZE];		/* cond & cmd */
} db_cond[DB_MAX_COND];

void
db_cond_free(bkpt)
	db_thread_breakpoint_t bkpt;
{
	if (bkpt->tb_cond > 0) {
	    db_cond[bkpt->tb_cond-1].c_size = 0;
	    db_ncond_free++;
	    bkpt->tb_cond = 0;
	}
}

boolean_t
db_cond_check(bkpt)
	db_thread_breakpoint_t bkpt;
{
	register  struct db_cond *cp;
	db_expr_t value;
	int	  t;
	jmp_buf_t db_jmpbuf;
	extern 	  jmp_buf_t *db_recover;

	if (bkpt->tb_cond <= 0)		/* no condition */
	    return(TRUE);
	db_dot = PC_REGS(DDB_REGS);
	db_prev = db_dot;
	db_next = db_dot;
	if (_setjmp(db_recover = &db_jmpbuf)) {
	    /*
	     * in case of error, return true to enter interactive mode
	     */
	    return(TRUE);
	}

	/*
	 * switch input, and evalutate condition
	 */
	cp = &db_cond[bkpt->tb_cond - 1];
	db_switch_input(cp->c_cond_cmd, cp->c_size);
	if (!db_expression(&value)) {
	    db_printf("error: condition evaluation error\n");
	    return(TRUE);
	}
	if (value == 0 || --(bkpt->tb_count) > 0)
	    return(FALSE);

	/*
	 * execute a command list if exist
	 */
	bkpt->tb_count = bkpt->tb_init_count;
	if ((t = db_read_token()) != tEOL) {
	    db_unread_token(t);
	    return(db_exec_cmd_nest(0, 0));
	}
	return(TRUE);
}

void
db_cond_print(bkpt)
	db_thread_breakpoint_t bkpt;
{
	register char *p, *ep;
	register struct db_cond *cp;

	if (bkpt->tb_cond <= 0)
	    return;
	cp = &db_cond[bkpt->tb_cond-1];
	p = cp->c_cond_cmd;
	ep = p + cp->c_size;
	while (p < ep) {
	    if (*p == '\n' || *p == 0)
		break;
	    db_putchar(*p++);
	}
}

void
db_cond_cmd()
{
	register  c;
	register  struct db_cond *cp;
	register  char *p;
	db_expr_t value;
	db_thread_breakpoint_t bkpt;

	if (db_read_token() != tHASH || db_read_token() != tNUMBER) {
	    db_printf("#<number> expected instead of \"%s\"\n", db_tok_string);
	    db_error(0);
	    return;
	}
	if ((bkpt = db_find_breakpoint_number(db_tok_number, 0)) == 0) {
	    db_printf("No such break point #%d\n", db_tok_number);
	    db_error(0);
	    return;
	}
	/*
	 * if the break point already has a condition, free it first
	 */
	if (bkpt->tb_cond > 0) {
	    cp = &db_cond[bkpt->tb_cond - 1];
	    db_cond_free(bkpt);
	} else {
	    if (db_ncond_free <= 0) {
		db_error("Too many conditions\n");
		return;
	    }
	    for (cp = db_cond; cp < &db_cond[DB_MAX_COND]; cp++)
		if (cp->c_size == 0)
		    break;
	    if (cp >= &db_cond[DB_MAX_COND])
		panic("bad db_cond_free");
	}
	for (c = db_read_char(); c == ' ' || c == '\t'; c = db_read_char());
	for (p = cp->c_cond_cmd; c >= 0; c = db_read_char())
	    *p++ = c;
	/*
	 * switch to saved data and call db_expression to check the condition.
	 * If no condition is supplied, db_expression will return false.
	 * In this case, clear previous condition of the break point.
         * If condition is supplied, set the condition to the permanent area.
	 * Note: db_expression will not return here, if the condition
	 *       expression is wrong.
	 */
	db_switch_input(cp->c_cond_cmd, p - cp->c_cond_cmd);
	if (!db_expression(&value)) {
	    /* since condition is already freed, do nothing */
	    db_flush_lex();
	    return;
	}
	db_flush_lex();
	db_ncond_free--;
	cp->c_size = p - cp->c_cond_cmd;
	bkpt->tb_cond = (cp - db_cond) + 1;
}

#endif MACH_KDB
