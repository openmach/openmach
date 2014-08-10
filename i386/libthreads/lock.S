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

#include <mach/machine/asm.h>

/*
 * boolean_t spin_try_lock(int *m)
 */
ENTRY(spin_try_lock)
	movl	4(%esp),%ecx		/* point at mutex */
	movl	$1,%eax			/* set locked value in acc */
	xchg	%eax,(%ecx)		/* swap with mutex */
					/* xchg with memory is automatically */
					/* locked */
	xorl	$1,%eax			/* 1 (locked) => FALSE */
					/* 0 (locked) => TRUE */
	ret

/*
 * void spin_unlock(int *m)
 */
ENTRY(spin_unlock)
	movl	4(%esp),%ecx		/* point at mutex */
	xorl	%eax,%eax		/* set unlocked value in acc */
	xchg	%eax,(%ecx)		/* swap with mutex */
					/* xchg with memory is automatically */
					/* locked */
	ret
