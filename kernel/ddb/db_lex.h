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
/*
 * Lexical analyzer.
 */

#define	TOK_STRING_SIZE		64 
#define DB_LEX_LINE_SIZE	256

struct db_lex_context {
	int  l_char;		/* peek char */
	int  l_token;		/* peek token */
	char *l_ptr;		/* line pointer */
	char *l_eptr;		/* line end pointer */
};

extern int	db_read_line(/* char *rep_str */);
extern void	db_flush_line();
extern int	db_read_char();
extern void	db_unread_char(/* char c */);
extern int	db_read_token();
extern void	db_unread_token(/* int t */);
extern void	db_flush_lex();
extern void	db_switch_input(/* char *, int */);
extern void	db_save_lex_context(/* struct db_lex_context * */);
extern void	db_restore_lex_context(/* struct db_lex_context * */);
extern void	db_skip_to_eol();

extern db_expr_t db_tok_number;
extern char	db_tok_string[TOK_STRING_SIZE];
extern db_expr_t db_radix;

#define	tEOF		(-1)
#define	tEOL		1
#define	tNUMBER		2
#define	tIDENT		3
#define	tPLUS		4
#define	tMINUS		5
#define	tDOT		6
#define	tSTAR		7
#define	tSLASH		8
#define	tEQ		9
#define	tLPAREN		10
#define	tRPAREN		11
#define	tPCT		12
#define	tHASH		13
#define	tCOMMA		14
#define	tQUOTE		15
#define	tDOLLAR		16
#define	tEXCL		17
#define	tSHIFT_L	18
#define	tSHIFT_R	19
#define	tDOTDOT		20
#define tSEMI_COLON	21
#define tLOG_EQ		22
#define tLOG_NOT_EQ	23
#define tLESS		24
#define tLESS_EQ	25
#define tGREATER	26
#define tGREATER_EQ	27
#define tBIT_AND	28
#define tBIT_OR		29
#define tLOG_AND	30
#define tLOG_OR		31
#define tSTRING		32
#define tQUESTION	33
