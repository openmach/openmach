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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#include "mach_kdb.h"
#if MACH_KDB

#include <mach/boolean.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_task_thread.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <mach/vm_param.h>

#define db_thread_to_task(thread)	((thread)? thread->task: TASK_NULL)

char		db_examine_format[TOK_STRING_SIZE] = "x";
int		db_examine_count = 1;
db_addr_t	db_examine_prev_addr = 0;
thread_t	db_examine_thread = THREAD_NULL;

extern	db_addr_t db_disasm(db_addr_t pc, boolean_t altform, task_t task);
			/* instruction disassembler */

void db_examine();/*forwards*/
void db_strcpy();

/*
 * Examine (print) data.
 */
/*ARGSUSED*/
void
db_examine_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	thread_t	thread;
	boolean_t	db_option();

	if (modif[0] != '\0')
	    db_strcpy(db_examine_format, modif);

	if (count == -1)
	    count = 1;
	db_examine_count = count;
	if (db_option(modif, 't')) 
	  {
	    if (!db_get_next_thread(&thread, 0))
	      return;
	  }
	else 
	  if (db_option(modif,'u'))
	    thread = current_thread();
	  else
	    thread = THREAD_NULL;
	
	db_examine_thread = thread;
	db_examine((db_addr_t) addr, db_examine_format, count, 
					db_thread_to_task(thread));
}

/* ARGSUSED */
void
db_examine_forward(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_examine(db_next, db_examine_format, db_examine_count,
				db_thread_to_task(db_examine_thread));
}

/* ARGSUSED */
void
db_examine_backward(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char *		modif;
{

	db_examine(db_examine_prev_addr - (db_next - db_examine_prev_addr),
			 db_examine_format, db_examine_count,
				db_thread_to_task(db_examine_thread));
}

void
db_examine(addr, fmt, count, task)
	register
	db_addr_t	addr;
	char *		fmt;	/* format string */
	int		count;	/* repeat count */
	task_t		task;
{
	int		c;
	db_expr_t	value;
	int		size;	/* in bytes */
	int		width;
	char *		fp;

	db_examine_prev_addr = addr;
	while (--count >= 0) {
	    fp = fmt;
	    size = sizeof(int);
	    width = 4*size;
	    while ((c = *fp++) != 0) {
		switch (c) {
		    case 'b':
			size = sizeof(char);
			width = 4*size;
			break;
		    case 'h':
			size = sizeof(short);
			width = 4*size;
			break;
		    case 'l':
			size = sizeof(long);
			width = 4*size;
			break;
		    case 'a':	/* address */
		    case 'A':   /* function address */
			/* always forces a new line */
			if (db_print_position() != 0)
			    db_printf("\n");
			db_prev = addr;
			db_task_printsym(addr, 
					(c == 'a')?DB_STGY_ANY:DB_STGY_PROC,
					task);
			db_printf(":\t");
			break;
		    case 'm':
			db_next = db_xcdump(addr, size, count+1, task);
			return;
		    default:
			if (db_print_position() == 0) {
			    /* If we hit a new symbol, print it */
			    char *	name;
			    db_addr_t	off;

			    db_find_task_sym_and_offset(addr,&name,&off,task);
			    if (off == 0)
				db_printf("%s:\t", name);
			    else
				db_printf("\t\t");

			    db_prev = addr;
			}

			switch (c) {
			    case ',':   /* skip one unit w/o printing */
				addr += size;
				break;

			    case 'r':	/* signed, current radix */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*R", width, value);
				break;
			    case 'x':	/* unsigned hex */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*X", width, value);
				break;
			    case 'z':	/* signed hex */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*Z", width, value);
				break;
			    case 'd':	/* signed decimal */
				value = db_get_task_value(addr,size,TRUE,task);
				addr += size;
				db_printf("%-*D", width, value);
				break;
			    case 'U':	/* unsigned decimal */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*U", width, value);
				break;
			    case 'o':	/* unsigned octal */
				value = db_get_task_value(addr,size,FALSE,task);
				addr += size;
				db_printf("%-*O", width, value);
				break;
			    case 'c':	/* character */
				value = db_get_task_value(addr,1,FALSE,task);
				addr += 1;
				if (value >= ' ' && value <= '~')
				    db_printf("%c", value);
				else
				    db_printf("\\%03o", value);
				break;
			    case 's':	/* null-terminated string */
				for (;;) {
				    value = db_get_task_value(addr,1,FALSE,task);
				    addr += 1;
				    if (value == 0)
					break;
				    if (value >= ' ' && value <= '~')
					db_printf("%c", value);
				    else
					db_printf("\\%03o", value);
				}
				break;
			    case 'i':	/* instruction */
				addr = db_disasm(addr, FALSE, task);
				break;
			    case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, TRUE, task);
				break;
			    default:
				break;
			}
			if (db_print_position() != 0)
			    db_end_line();
			break;
		}
	    }
	}
	db_next = addr;
}

/*
 * Print value.
 */
char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd()
{
	db_expr_t	value;
	int		t;
	task_t		task = TASK_NULL;

	if ((t = db_read_token()) == tSLASH) {
	    if (db_read_token() != tIDENT) {
		db_printf("Bad modifier \"/%s\"\n", db_tok_string);
		db_error(0);
		/* NOTREACHED */
	    }
	    if (db_tok_string[0])
		db_print_format = db_tok_string[0];
	    if (db_option(db_tok_string, 't') && db_default_thread)
		task = db_default_thread->task;
	} else
	    db_unread_token(t);
	
	for ( ; ; ) {
	    t = db_read_token();
	    if (t == tSTRING) {
		db_printf("%s", db_tok_string);
		continue;
	    }
	    db_unread_token(t);
	    if (!db_expression(&value))
		break;
	    switch (db_print_format) {
	    case 'a':
		db_task_printsym((db_addr_t)value, DB_STGY_ANY, task);
		break;
	    case 'r':
		db_printf("%*r", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'x':
		db_printf("%*x", 2*sizeof(db_expr_t), value);
		break;
	    case 'z':
		db_printf("%*z", 2*sizeof(db_expr_t), value);
		break;
	    case 'd':
		db_printf("%*d", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'u':
		db_printf("%*u", 3+2*sizeof(db_expr_t), value);
		break;
	    case 'o':
		db_printf("%o", 4*sizeof(db_expr_t), value);
		break;
	    case 'c':
		value = value & 0xFF;
		if (value >= ' ' && value <= '~')
		    db_printf("%c", value);
		else
		    db_printf("\\%03o", value);
		break;
	    default:
		db_printf("Unknown format %c\n", db_print_format);
		db_print_format = 'x';
		db_error(0);
	    }
	}
}

void
db_print_loc_and_inst(loc, task)
	db_addr_t	loc;
	task_t		task;
{
	db_task_printsym(loc, DB_STGY_PROC, task);
	db_printf(":\t");
	(void) db_disasm(loc, TRUE, task);
}

void
db_strcpy(dst, src)
	register char *dst;
	register char *src;
{
	while (*dst++ = *src++)
	    ;
}

void db_search(); /*forward*/
/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count] [thread]
 */
void
db_search_cmd()
{
	int		t;
	db_addr_t	addr;
	int		size = 0;
	db_expr_t	value;
	db_expr_t	mask;
	db_addr_t	count;
	thread_t	thread;
	boolean_t	thread_flag = FALSE;
	register char	*p;

	t = db_read_token();
	if (t == tSLASH) {
	    t = db_read_token();
	    if (t != tIDENT) {
	      bad_modifier:
		db_printf("Bad modifier \"/%s\"\n", db_tok_string);
		db_flush_lex();
		return;
	    }

	    for (p = db_tok_string; *p; p++) {
		switch(*p) {
		case 'b':
		    size = sizeof(char);
		    break;
		case 'h':
		    size = sizeof(short);
		    break;
		case 'l':
		    size = sizeof(long);
		    break;
		case 't':
		    thread_flag = TRUE;
		    break;
		default:
		    goto bad_modifier;
		}
	    }
	} else {
	    db_unread_token(t);
	    size = sizeof(int);
	}

	if (!db_expression(&addr)) {
	    db_printf("Address missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&value)) {
	    db_printf("Value missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&mask))
	    mask = ~0;

	t = db_read_token();
	if (t == tCOMMA) {
	    if (!db_expression(&count)) {
		db_printf("Count missing\n");
		db_flush_lex();
		return;
	    }
	} else {
	    db_unread_token(t);
	    count = -1;		/* effectively forever */
	}
	if (thread_flag) {
	    if (!db_get_next_thread(&thread, 0))
		return;
	} else
	    thread = THREAD_NULL;

	db_search(addr, size, value, mask, count, db_thread_to_task(thread));
}

void
db_search(addr, size, value, mask, count, task)
	register
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	unsigned int	count;
	task_t		task;
{
	while (count-- != 0) {
		db_prev = addr;
		if ((db_get_task_value(addr,size,FALSE,task) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}

#define DB_XCDUMP_NC	16

int
db_xcdump(addr, size, count, task)
	db_addr_t	addr;
	int		size;
	int		count;
	task_t		task;
{
	register 	i, n;
	db_expr_t	value;
	int		bcount;
	db_addr_t	off;
	char		*name;
	char		data[DB_XCDUMP_NC];

	db_find_task_sym_and_offset(addr, &name, &off, task);
	for (n = count*size; n > 0; n -= bcount) {
	    db_prev = addr;
	    if (off == 0) {
		db_printf("%s:\n", name);
		off = -1;
	    }
	    db_printf("%0*X:%s", 2*sizeof(db_addr_t), addr,
	    		(size != 1)? " ": "");
	    bcount = ((n > DB_XCDUMP_NC)? DB_XCDUMP_NC: n);
	    if (trunc_page(addr) != trunc_page(addr+bcount-1)) {
		db_addr_t next_page_addr = trunc_page(addr+bcount-1);
		if (!DB_CHECK_ACCESS(next_page_addr, sizeof(int), task))
		    bcount = next_page_addr - addr;
	    }
	    db_read_bytes((char *)addr, bcount, data, task);
	    for (i = 0; i < bcount && off != 0; i += size) {
		if (i % 4 == 0)
			db_printf(" ");
		value = db_get_task_value(addr, size, FALSE, task);
		db_printf("%0*x ", size*2, value);
		addr += size;
		db_find_task_sym_and_offset(addr, &name, &off, task);
	    }
	    db_printf("%*s",
			((DB_XCDUMP_NC-i)/size)*(size*2+1)+(DB_XCDUMP_NC-i)/4,
			 "");
	    bcount = i;
	    db_printf("%s*", (size != 1)? " ": "");
	    for (i = 0; i < bcount; i++) {
		value = data[i];
		db_printf("%c", (value >= ' ' && value <= '~')? value: '.');
	    }
	    db_printf("*\n");
	}
	return(addr);
}

#endif MACH_KDB
