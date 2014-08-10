/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989 Carnegie Mellon University
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
 * sync.c
 *
 * Spin locks
 */

#include <mach/cthreads.h>
#include "cthread_internals.h"

/*
 * Spin locks.
 * Use test and test-and-set logic on all architectures.
 */

int cthread_spin_count=0;

void
spin_lock_solid(register spin_lock_t *p)
{
	while (spin_lock_locked(p) || !spin_try_lock(p)) {
#ifdef STATISTICS
		cthread_spin_count++;
#endif
#ifdef SPIN_RESCHED
		yield();
#endif
	}
}
