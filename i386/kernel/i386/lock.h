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
 * Machine-dependent simple locks for the i386.
 */
#ifndef	_I386_LOCK_H_
#define	_I386_LOCK_H_

#if NCPUS > 1

/*
 *	All of the locking routines are built from calls on
 *	a locked-exchange operation.  Values of the lock are
 *	0 for unlocked, 1 for locked.
 */

#ifdef	__GNUC__

/*
 *	The code here depends on the GNU C compiler.
 */

#define	_simple_lock_xchg_(lock, new_val) \
    ({	register int _old_val_; \
	asm volatile("xchgl %0, %2" \
		    : "=r" (_old_val_) \
		    : "0" (new_val), "m" (*(lock)) \
		    ); \
	_old_val_; \
    })

#define	simple_lock_init(l) \
	((l)->lock_data = 0)

#define	simple_lock(l) \
    ({ \
	while(_simple_lock_xchg_(l, 1)) \
	    while (*(volatile int *)&(l)->lock_data) \
		continue; \
	0; \
    })

#define	simple_unlock(l) \
	(_simple_lock_xchg_(l, 0))

#define	simple_lock_try(l) \
	(!_simple_lock_xchg_(l, 1))

/*
 *	General bit-lock routines.
 */
#define	bit_lock(bit, l) \
    ({ \
	asm volatile("	jmp	1f	\n\
		    0:	btl	%0, %1	\n\
			jb	0b	\n\
		    1:	lock		\n\
			btsl	%0, %1	\n\
			jb	0b" \
		    : \
		    : "r" (bit), "m" (*(volatile int *)(l))); \
	0; \
    })

#define	bit_unlock(bit, l) \
    ({ \
	asm volatile("	lock		\n\
			btrl	%0, %1" \
		    : \
		    : "r" (bit), "m" (*(volatile int *)(l))); \
	0; \
    })

/*
 *	Set or clear individual bits in a long word.
 *	The locked access is needed only to lock access
 *	to the word, not to individual bits.
 */
#define	i_bit_set(bit, l) \
    ({ \
	asm volatile("	lock		\n\
			btsl	%0, %1" \
		    : \
		    : "r" (bit), "m" (*(l)) ); \
	0; \
    })

#define	i_bit_clear(bit, l) \
    ({ \
	asm volatile("	lock		\n\
			btrl	%0, %1" \
		    : \
		    : "r" (bit), "m" (*(l)) ); \
	0; \
    })

#endif	/* __GNUC__ */

extern void simple_lock_pause();

#endif NCPUS > 1


#include_next "lock.h"


#endif	/* _I386_LOCK_H_ */
