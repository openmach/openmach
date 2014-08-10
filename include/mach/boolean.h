/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	mach/boolean.h
 *
 *	Boolean data type.
 *
 */

#ifndef	_MACH_BOOLEAN_H_
#define	_MACH_BOOLEAN_H_

/*
 *	Pick up "boolean_t" type definition
 */

#ifndef	ASSEMBLER
#include <mach/machine/boolean.h>
#endif	/* ASSEMBLER */

#endif	/* _MACH_BOOLEAN_H_ */

/*
 *	Define TRUE and FALSE, only if they haven't been before,
 *	and not if they're explicitly refused.  Note that we're
 *	outside the BOOLEAN_H_ conditional, to avoid ordering
 *	problems.
 */

#if	!defined(NOBOOL)

#ifndef	TRUE
#define TRUE	((boolean_t) 1)
#endif	/* TRUE */

#ifndef	FALSE
#define FALSE	((boolean_t) 0)
#endif	/* FALSE */

#endif	/* !defined(NOBOOL) */
