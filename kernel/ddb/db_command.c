/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * Command dispatcher.
 */
#include <cpus.h>
#include <norma_ipc.h>
#include <norma_vm.h>

#include <mach/boolean.h>
#include <kern/strings.h>
#include <machine/db_machdep.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_task_thread.h>

#include <machine/setjmp.h>
#include <kern/thread.h>
#include <ipc/ipc_pset.h> /* 4proto */
#include <ipc/ipc_port.h> /* 4proto */



/*
 * Exported global variables
 */
boolean_t	db_cmd_loop_done;
jmp_buf_t	*db_recover = 0;
db_addr_t	db_dot;
db_addr_t	db_last_addr;
db_addr_t	db_prev;
db_addr_t	db_next;

/*
 * if 'ed' style: 'dot' is set at start of last item printed,
 * and '+' points to next line.
 * Otherwise: 'dot' points to next item, '..' points to last.
 */
boolean_t	db_ed_style = TRUE;

/*
 * Results of command search.
 */
#define	CMD_UNIQUE	0
#define	CMD_FOUND	1
#define	CMD_NONE	2
#define	CMD_AMBIGUOUS	3
#define	CMD_HELP	4

/*
 * Search for command prefix.
 */
int
db_cmd_search(name, table, cmdp)
	char *		name;
	struct db_command	*table;
	struct db_command	**cmdp;	/* out */
{
	struct db_command	*cmd;
	int		result = CMD_NONE;

	for (cmd = table; cmd->name != 0; cmd++) {
	    register char *lp;
	    register char *rp;
	    register int  c;

	    lp = name;
	    rp = cmd->name;
	    while ((c = *lp) == *rp) {
		if (c == 0) {
		    /* complete match */
		    *cmdp = cmd;
		    return (CMD_UNIQUE);
		}
		lp++;
		rp++;
	    }
	    if (c == 0) {
		/* end of name, not end of command -
		   partial match */
		if (result == CMD_FOUND) {
		    result = CMD_AMBIGUOUS;
		    /* but keep looking for a full match -
		       this lets us match single letters */
		}
		else {
		    *cmdp = cmd;
		    result = CMD_FOUND;
		}
	    }
	}
	if (result == CMD_NONE) {
	    /* check for 'help' */
	    if (!strncmp(name, "help", strlen(name)))
		result = CMD_HELP;
	}
	return (result);
}

void
db_cmd_list(table)
	struct db_command *table;
{
	register struct db_command *cmd;

	for (cmd = table; cmd->name != 0; cmd++) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	}
}

void
db_command(last_cmdp, cmd_table)
	struct db_command	**last_cmdp;	/* IN_OUT */
	struct db_command	*cmd_table;
{
	struct db_command	*cmd;
	int		t;
	char		modif[TOK_STRING_SIZE];
	db_expr_t	addr, count;
	boolean_t	have_addr = FALSE;
	int		result;

	t = db_read_token();
	if (t == tEOL || t == tSEMI_COLON) {
	    /* empty line repeats last command, at 'next' */
	    cmd = *last_cmdp;
	    addr = (db_expr_t)db_next;
	    have_addr = FALSE;
	    count = 1;
	    modif[0]  = '\0';
	    if (t == tSEMI_COLON)
		db_unread_token(t);
	}
	else if (t == tEXCL) {
	    void db_fncall();
	    db_fncall();
	    return;
	}
	else if (t != tIDENT) {
	    db_printf("?\n");
	    db_flush_lex();
	    return;
	}
	else {
	    /*
	     * Search for command
	     */
	    while (cmd_table) {
		result = db_cmd_search(db_tok_string,
				       cmd_table,
				       &cmd);
		switch (result) {
		    case CMD_NONE:
			if (db_exec_macro(db_tok_string) == 0)
			    return;
			db_printf("No such command \"%s\"\n", db_tok_string);
			db_flush_lex();
			return;
		    case CMD_AMBIGUOUS:
			db_printf("Ambiguous\n");
			db_flush_lex();
			return;
		    case CMD_HELP:
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    default:
			break;
		}
		if ((cmd_table = cmd->more) != 0) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_cmd_list(cmd_table);
			db_flush_lex();
			return;
		    }
		}
	    }

	    if ((cmd->flag & CS_OWN) == 0) {
		/*
		 * Standard syntax:
		 * command [/modifier] [addr] [,count]
		 */
		t = db_read_token();
		if (t == tSLASH) {
		    t = db_read_token();
		    if (t != tIDENT) {
			db_printf("Bad modifier \"/%s\"\n", db_tok_string);
			db_flush_lex();
			return;
		    }
		    db_strcpy(modif, db_tok_string);
		}
		else {
		    db_unread_token(t);
		    modif[0] = '\0';
		}

		if (db_expression(&addr)) {
		    db_dot = (db_addr_t) addr;
		    db_last_addr = db_dot;
		    have_addr = TRUE;
		}
		else {
		    addr = (db_expr_t) db_dot;
		    have_addr = FALSE;
		}
		t = db_read_token();
		if (t == tCOMMA) {
		    if (!db_expression(&count)) {
			db_printf("Count missing after ','\n");
			db_flush_lex();
			return;
		    }
		}
		else {
		    db_unread_token(t);
		    count = -1;
		}
	    }
	}
	*last_cmdp = cmd;
	if (cmd != 0) {
	    /*
	     * Execute the command.
	     */
	    (*cmd->fcn)(addr, have_addr, count, modif);

	    if (cmd->flag & CS_SET_DOT) {
		/*
		 * If command changes dot, set dot to
		 * previous address displayed (if 'ed' style).
		 */
		if (db_ed_style) {
		    db_dot = db_prev;
		}
		else {
		    db_dot = db_next;
		}
	    }
	    else {
		/*
		 * If command does not change dot,
		 * set 'next' location to be the same.
		 */
		db_next = db_dot;
	    }
	}
}

void
db_command_list(last_cmdp, cmd_table)
	struct db_command	**last_cmdp;	/* IN_OUT */
	struct db_command	*cmd_table;
{
	void db_skip_to_eol();

	do {
	    db_command(last_cmdp, cmd_table);
	    db_skip_to_eol();
	} while (db_read_token() == tSEMI_COLON && db_cmd_loop_done == 0);
}

/*
 * 'show' commands
 */
extern void	db_listbreak_cmd();
extern void	db_listwatch_cmd();
extern void	db_show_regs(), db_show_one_thread(), db_show_one_task();
extern void	db_show_all_threads();
extern void	db_show_macro();
extern void	vm_map_print(), vm_object_print(), vm_page_print();
extern void	vm_map_copy_print();
extern void	ipc_port_print(), ipc_pset_print(), db_show_all_slocks();
extern void	ipc_kmsg_print(), ipc_msg_print();
extern void	db_show_port_id();
void		db_show_help();
#if	NORMA_IPC
extern void	netipc_packet_print(), netipc_pcs_print(), db_show_all_uids();
extern void	db_show_all_proxies(), db_show_all_principals();
extern void	db_show_all_uids_verbose();
#endif	NORMA_IPC
#if	NORMA_VM
extern void	xmm_obj_print(), xmm_reply_print();
#endif	NORMA_VM

struct db_command db_show_all_cmds[] = {
	{ "threads",	db_show_all_threads,	0,	0 },
	{ "slocks",	db_show_all_slocks,	0,	0 },
#if	NORMA_IPC
	{ "uids",	db_show_all_uids,	0,	0 },
	{ "proxies",	db_show_all_proxies,	0,	0 },
	{ "principals",	db_show_all_principals,	0,	0 },
	{ "vuids",	db_show_all_uids_verbose, 0,	0 },
#endif	NORMA_IPC
	{ (char *)0 }
};

struct db_command db_show_cmds[] = {
	{ "all",	0,			0,	db_show_all_cmds },
	{ "registers",	db_show_regs,		0,	0 },
	{ "breaks",	db_listbreak_cmd, 	0,	0 },
	{ "watches",	db_listwatch_cmd, 	0,	0 },
	{ "thread",	db_show_one_thread,	0,	0 },
	{ "task",	db_show_one_task,	0,	0 },
	{ "macro",	db_show_macro,		CS_OWN, 0 },
	{ "map",	vm_map_print,		0,	0 },
	{ "object",	vm_object_print,	0,	0 },
	{ "page",	vm_page_print,		0,	0 },
	{ "copy",	vm_map_copy_print,	0,	0 },
	{ "port",	ipc_port_print,		0,	0 },
	{ "pset",	ipc_pset_print,		0,	0 },
	{ "kmsg",	ipc_kmsg_print,		0,	0 },
	{ "msg",	ipc_msg_print,		0,	0 },
	{ "ipc_port",	db_show_port_id,	0,	0 },
#if	NORMA_IPC
	{ "packet",	netipc_packet_print,	0,	0 },
	{ "pcs",	netipc_pcs_print,	0,	0 },
#endif	NORMA_IPC
#if	NORMA_VM
	{ "xmm_obj",	xmm_obj_print,		0,	0 },
	{ "xmm_reply",	xmm_reply_print,	0,	0 },
#endif	NORMA_VM
	{ (char *)0, }
};

extern void	db_print_cmd(), db_examine_cmd(), db_set_cmd();
extern void	db_examine_forward(), db_examine_backward();
extern void	db_search_cmd();
extern void	db_write_cmd();
extern void	db_delete_cmd(), db_breakpoint_cmd();
extern void	db_deletewatch_cmd(), db_watchpoint_cmd();
extern void	db_single_step_cmd(), db_trace_until_call_cmd(),
		db_trace_until_matching_cmd(), db_continue_cmd();
extern void	db_stack_trace_cmd(), db_cond_cmd();
void		db_help_cmd();
void		db_def_macro_cmd(), db_del_macro_cmd();
void		db_fncall();
extern void	db_reset_cpu();

struct db_command db_command_table[] = {
#ifdef DB_MACHINE_COMMANDS
  /* this must be the first entry, if it exists */
	{ "machine",    0,                      0,     		0},
#endif
	{ "print",	db_print_cmd,		CS_OWN,		0 },
	{ "examine",	db_examine_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "x",		db_examine_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "xf",		db_examine_forward,	CS_SET_DOT,	0 },
	{ "xb",		db_examine_backward,	CS_SET_DOT,	0 },
	{ "search",	db_search_cmd,		CS_OWN|CS_SET_DOT, 0 },
	{ "set",	db_set_cmd,		CS_OWN,		0 },
	{ "write",	db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "w",		db_write_cmd,		CS_MORE|CS_SET_DOT, 0 },
	{ "delete",	db_delete_cmd,		CS_OWN,		0 },
	{ "d",		db_delete_cmd,		CS_OWN,		0 },
	{ "break",	db_breakpoint_cmd,	CS_MORE,	0 },
	{ "dwatch",	db_deletewatch_cmd,	CS_MORE,	0 },
	{ "watch",	db_watchpoint_cmd,	CS_MORE,	0 },
	{ "step",	db_single_step_cmd,	0,		0 },
	{ "s",		db_single_step_cmd,	0,		0 },
	{ "continue",	db_continue_cmd,	0,		0 },
	{ "c",		db_continue_cmd,	0,		0 },
	{ "until",	db_trace_until_call_cmd,0,		0 },
	{ "next",	db_trace_until_matching_cmd,0,		0 },
	{ "match",	db_trace_until_matching_cmd,0,		0 },
	{ "trace",	db_stack_trace_cmd,	0,		0 },
	{ "cond",	db_cond_cmd,		CS_OWN,	 	0 },
	{ "call",	db_fncall,		CS_OWN,		0 },
	{ "macro",	db_def_macro_cmd,	CS_OWN,	 	0 },
	{ "dmacro",	db_del_macro_cmd,	CS_OWN,		0 },
	{ "show",	0,			0,	db_show_cmds },
	{ "reset",	db_reset_cpu,		0,		0 },
	{ "reboot",	db_reset_cpu,		0,		0 },
	{ (char *)0, }
};

#ifdef DB_MACHINE_COMMANDS

/* this function should be called to install the machine dependent
   commands. It should be called before the debugger is enabled  */
void db_machine_commands_install(ptr)
struct db_command *ptr;
{
  db_command_table[0].more = ptr;
  return;
}

#endif


struct db_command	*db_last_command = 0;

void
db_help_cmd()
{
	struct db_command *cmd = db_command_table;

	while (cmd->name != 0) {
	    db_printf("%-12s", cmd->name);
	    db_end_line();
	    cmd++;
	}
}

int	(*ddb_display)();

void
db_command_loop()
{
	jmp_buf_t db_jmpbuf;
	jmp_buf_t *prev = db_recover;
	extern int db_output_line;
	extern int db_macro_level;
#if	NORMA_IPC
	extern int _node_self;	/* node_self() may not be callable yet */
#endif	NORMA_IPC

	/*
	 * Initialize 'prev' and 'next' to dot.
	 */
	db_prev = db_dot;
	db_next = db_dot;

	if (ddb_display)
		(*ddb_display)();

	db_cmd_loop_done = 0;
	while (!db_cmd_loop_done) {
	    (void) _setjmp(db_recover = &db_jmpbuf);
	    db_macro_level = 0;
	    if (db_print_position() != 0)
		db_printf("\n");
	    db_output_line = 0;
	    db_printf("db%s", (db_default_thread)? "t": "");
#if	NORMA_IPC
	    db_printf("%d", _node_self);
#endif
#if	NCPUS > 1
	    db_printf("{%d}", cpu_number());
#endif
	    db_printf("> ");

	    (void) db_read_line("!!");
	    db_command_list(&db_last_command, db_command_table);
	}

	db_recover = prev;
}

boolean_t
db_exec_cmd_nest(cmd, size)
	char *cmd;
	int  size;
{
	struct db_lex_context lex_context;

	db_cmd_loop_done = 0;
	if (cmd) {
	    db_save_lex_context(&lex_context);
	    db_switch_input(cmd, size /**OLD, &lex_context OLD**/);
	}
	db_command_list(&db_last_command, db_command_table);
	if (cmd)
	    db_restore_lex_context(&lex_context);
	return(db_cmd_loop_done == 0);
}

#ifdef __GNUC__
extern __volatile__ void _longjmp();
#endif

void db_error(s)
	char *s;
{
	extern int db_macro_level;

	db_macro_level = 0;
	if (db_recover) {
	    if (s)
		db_printf(s);
	    db_flush_lex();
	    _longjmp(db_recover, 1);
	}
	else
	{
	    if (s)
	        db_printf(s);
	    panic("db_error");
	}
}

/*
 * Call random function:
 * !expr(arg,arg,arg)
 */
void
db_fncall()
{
	db_expr_t	fn_addr;
#define	MAXARGS		11
	db_expr_t	args[MAXARGS];
	int		nargs = 0;
	db_expr_t	retval;
	db_expr_t	(*func)();
	int		t;

	if (!db_expression(&fn_addr)) {
	    db_printf("Bad function \"%s\"\n", db_tok_string);
	    db_flush_lex();
	    return;
	}
	func = (db_expr_t (*) ()) fn_addr;

	t = db_read_token();
	if (t == tLPAREN) {
	    if (db_expression(&args[0])) {
		nargs++;
		while ((t = db_read_token()) == tCOMMA) {
		    if (nargs == MAXARGS) {
			db_printf("Too many arguments\n");
			db_flush_lex();
			return;
		    }
		    if (!db_expression(&args[nargs])) {
			db_printf("Argument missing\n");
			db_flush_lex();
			return;
		    }
		    nargs++;
		}
		db_unread_token(t);
	    }
	    if (db_read_token() != tRPAREN) {
		db_printf("?\n");
		db_flush_lex();
		return;
	    }
	}
	while (nargs < MAXARGS) {
	    args[nargs++] = 0;
	}

	retval = (*func)(args[0], args[1], args[2], args[3], args[4],
			 args[5], args[6], args[7], args[8], args[9] );
	db_printf(" %#N\n", retval);
}

boolean_t
db_option(modif, option)
	char	*modif;
	int	option;
{
	register char *p;

	for (p = modif; *p; p++)
	    if (*p == option)
		return(TRUE);
	return(FALSE);
}

#endif MACH_KDB
