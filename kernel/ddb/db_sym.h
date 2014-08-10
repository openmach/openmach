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
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/90
 */

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <machine/db_machdep.h>

/*
 * This module can handle multiple symbol tables,
 * of multiple types, at the same time
 */
#define	SYMTAB_NAME_LEN	32

typedef struct {
	int		type;
#define	SYMTAB_AOUT	0
#define	SYMTAB_COFF	1
#define	SYMTAB_MACHDEP	2
	char		*start;		/* symtab location */
	char		*end;
	char		*private;	/* optional machdep pointer */
	char		*map_pointer;	/* symbols are for this map only,
					   if not null */
	char		name[SYMTAB_NAME_LEN];
					/* symtab name */
} db_symtab_t;

extern db_symtab_t	*db_last_symtab; /* where last symbol was found */

/*
 * Symbol representation is specific to the symtab style:
 * BSD compilers use dbx' nlist, other compilers might use
 * a different one
 */
typedef	char *		db_sym_t;	/* opaque handle on symbols */
#define	DB_SYM_NULL	((db_sym_t)0)

/*
 * Non-stripped symbol tables will have duplicates, for instance
 * the same string could match a parameter name, a local var, a
 * global var, etc.
 * We are most concerned with the following matches.
 */
typedef int		db_strategy_t;	/* search strategy */

#define	DB_STGY_ANY	0			/* anything goes */
#define DB_STGY_XTRN	1			/* only external symbols */
#define DB_STGY_PROC	2			/* only procedures */

extern boolean_t	db_qualify_ambiguous_names;
					/* if TRUE, check across symbol tables
					 * for multiple occurrences of a name.
					 * Might slow down quite a bit
					 * ..but the machine has nothing
					 * else to do, now does it ? */

/*
 * Functions exported by the symtable module
 */

/* extend the list of symbol tables */

extern boolean_t	db_add_symbol_table( 	int type,
						char * start,
						char * end,
						char *name,
						char *ref,
						char *map_pointer );

/* find symbol value given name */

extern int	db_value_of_name( char* name, db_expr_t* valuep);

/* find symbol given value */

extern db_sym_t	db_search_task_symbol(	db_addr_t val,
					db_strategy_t strategy,
					db_addr_t *offp,
					task_t task );

/* return name and value of symbol */

extern void	db_symbol_values( db_symtab_t *stab,
				  db_sym_t sym,
				  char** namep,
				  db_expr_t* valuep);

/* find name&value given approx val */

#define db_find_sym_and_offset(val,namep,offp)	\
	db_symbol_values(0, db_search_symbol(val,DB_STGY_ANY,offp),namep,0)

/* ditto, but no locals */
#define db_find_xtrn_sym_and_offset(val,namep,offp)	\
	db_symbol_values(0, db_search_symbol(val,DB_STGY_XTRN,offp),namep,0)

/* find name&value given approx val */

#define db_find_task_sym_and_offset(val,namep,offp,task)	\
	db_symbol_values(0, db_search_task_symbol(val,DB_STGY_ANY,offp,task),  \
			 namep, 0)

/* ditto, but no locals */
#define db_find_xtrn_task_sym_and_offset(val,namep,offp,task)	\
	db_symbol_values(0, db_search_task_symbol(val,DB_STGY_XTRN,offp,task), \
			 namep,0)

/* find symbol in current task */
#define db_search_symbol(val,strgy,offp)	\
	db_search_task_symbol(val,strgy,offp,0)

/* strcmp, modulo leading char */
extern boolean_t	db_eqname( char* src, char* dst, char c );

/* print closest symbol to a value */
extern void	db_task_printsym( db_expr_t off,
				  db_strategy_t strategy,
				  task_t task);

/* print closest symbol to a value */
extern void	db_printsym( db_expr_t off, db_strategy_t strategy);

/*
 * Symbol table switch, defines the interface
 * to symbol-table specific routines.
 * [NOTE: incomplete prototypes cuz broken compiler]
 */

extern struct db_sym_switch {

	boolean_t	(*init)(
/*				char *start,
				char *end,
				char *name,
				char *task_addr
*/				);

	db_sym_t	(*lookup)(
/*				db_symtab_t *stab,
				char *symstr
*/				);
	db_sym_t	(*search_symbol)(
/*				db_symtab_t *stab,
				db_addr_t off,
				db_strategy_t strategy,
				db_expr_t *diffp
*/				);

	boolean_t	(*line_at_pc)(
/*				db_symtab_t	*stab,
				db_sym_t	sym,
				char		**file,
				int		*line,
				db_expr_t	pc
*/				);

	void		(*symbol_values)(
/*				db_sym_t	sym,
				char		**namep,
				db_expr_t	*valuep
*/				);

} x_db[];

#ifndef	symtab_type
#define	symtab_type(s)		SYMTAB_AOUT
#endif

#define	X_db_sym_init(s,e,n,t)		x_db[symtab_type(s)].init(s,e,n,t)
#define	X_db_lookup(s,n)		x_db[(s)->type].lookup(s,n)
#define	X_db_search_symbol(s,o,t,d)	x_db[(s)->type].search_symbol(s,o,t,d)
#define	X_db_line_at_pc(s,p,f,l,a)	x_db[(s)->type].line_at_pc(s,p,f,l,a)
#define	X_db_symbol_values(s,p,n,v)	x_db[(s)->type].symbol_values(p,n,v)
