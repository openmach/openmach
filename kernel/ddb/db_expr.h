/*
 * (c) Copyright 1992, 1993, 1994, 1995 OPEN SOFTWARE FOUNDATION, INC. 
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF RI nmk19b2 5/2/95
 */

#ifndef	_DDB_DB_EXPR_H_
#define	_DDB_DB_EXPR_H_

#include <mach/boolean.h>
#include <machine/db_machdep.h>


/* Prototypes for functions exported by this module.
 */

int db_size_option(
	char		*modif,
	boolean_t	*u_option,
	boolean_t	*t_option);

int db_expression(db_expr_t *valuep);

#endif	/* !_DDB_DB_EXPR_H_ */
