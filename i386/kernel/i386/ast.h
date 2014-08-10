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

#ifndef	_I386_AST_H_
#define	_I386_AST_H_

/*
 * Machine-dependent AST file for machines with no hardware AST support.
 *
 * For the I386, we define AST_I386_FP to handle delayed
 * floating-point exceptions.  The FPU may interrupt on errors
 * while the user is not running (in kernel or other thread running).
 */

#define	AST_I386_FP	0x80000000

#define MACHINE_AST_PER_THREAD		AST_I386_FP


/* Chain to the machine-independent header.  */
#include_next "ast.h"


#endif	/* _I386_AST_H_ */
