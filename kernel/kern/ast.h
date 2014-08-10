/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 *	kern/ast.h: Definitions for Asynchronous System Traps.
 */

#ifndef	_KERN_AST_H_
#define _KERN_AST_H_

/*
 *	A CPU takes an AST when it is about to return to user code.
 *	Instead of going back to user code, it calls ast_taken.
 *	Machine-dependent code is responsible for maintaining
 *	a set of reasons for an AST, and passing this set to ast_taken.
 */

#include <cpus.h>

#include "cpu_number.h"
#include <kern/macro_help.h>
#include <machine/ast.h>

/*
 *	Bits for reasons
 */

#define	AST_ZILCH	0x0
#define AST_HALT	0x1
#define AST_TERMINATE	0x2
#define AST_BLOCK	0x4
#define AST_NETWORK	0x8
#define AST_NETIPC	0x10

#define	AST_SCHEDULING	(AST_HALT|AST_TERMINATE|AST_BLOCK)

/*
 * Per-thread ASTs are reset at context-switch time.
 * machine/ast.h can define MACHINE_AST_PER_THREAD.
 */

#ifndef	MACHINE_AST_PER_THREAD
#define	MACHINE_AST_PER_THREAD	0
#endif

#define AST_PER_THREAD  (AST_HALT | AST_TERMINATE | MACHINE_AST_PER_THREAD)

typedef unsigned int ast_t;

extern volatile ast_t need_ast[NCPUS];

#ifdef	MACHINE_AST
/*
 *	machine/ast.h is responsible for defining aston and astoff.
 */
#else	MACHINE_AST

#define aston(mycpu)
#define astoff(mycpu)

#endif	MACHINE_AST

extern void ast_taken();

/*
 *	ast_needed, ast_on, ast_off, ast_context, and ast_propagate
 *	assume splsched.  mycpu is always cpu_number().  It is an
 *	argument in case cpu_number() is expensive.
 */

#define ast_needed(mycpu)		need_ast[mycpu]

#define ast_on(mycpu, reasons)						\
MACRO_BEGIN								\
	if ((need_ast[mycpu] |= (reasons)) != AST_ZILCH)		\
		{ aston(mycpu); }					\
MACRO_END

#define ast_off(mycpu, reasons)						\
MACRO_BEGIN								\
	if ((need_ast[mycpu] &= ~(reasons)) == AST_ZILCH)		\
		{ astoff(mycpu); } 					\
MACRO_END

#define ast_propagate(thread, mycpu)	ast_on((mycpu), (thread)->ast)

#define ast_context(thread, mycpu)					\
MACRO_BEGIN								\
	if ((need_ast[mycpu] =						\
	     (need_ast[mycpu] &~ AST_PER_THREAD) | (thread)->ast)	\
					!= AST_ZILCH)			\
		{ aston(mycpu);	}					\
	else								\
		{ astoff(mycpu); }					\
MACRO_END


#define	thread_ast_set(thread, reason)		(thread)->ast |= (reason)
#define thread_ast_clear(thread, reason)	(thread)->ast &= ~(reason)
#define thread_ast_clear_all(thread)		(thread)->ast = AST_ZILCH

/*
 *	NOTE: if thread is the current thread, thread_ast_set should
 *	be followed by ast_propagate().
 */

#endif	_KERN_AST_H_
