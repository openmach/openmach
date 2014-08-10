/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990 Carnegie Mellon University
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
#ifndef _MACHINE_CTHREADS_H_
#define _MACHINE_CTHREADS_H_

typedef volatile int spin_lock_t;
#define SPIN_LOCK_INITIALIZER	0
#define spin_lock_init(s)	(*(s) = 0)
#define spin_lock_locked(s)	(*(s) != 0)

#ifdef	__GNUC__

#define	spin_unlock(p) \
	({  register int _u__ ; \
	    __asm__ volatile("xorl %0, %0; \n\
			  xchgl %0, %1" \
			: "=&r" (_u__), "=m" (*(p)) ); \
	    0; })

#define	spin_try_lock(p)\
	(!({  boolean_t _r__; \
	    __asm__ volatile("movl $1, %0; \n\
			  xchgl %0, %1" \
			: "=&r" (_r__), "=m" (*(p)) ); \
	    _r__; }))

#define	cthread_sp() \
	({  int	_sp__; \
	    __asm__("movl %%esp, %0" \
	      :	"=g" (_sp__) ); \
	    _sp__; })

#endif	/* __GNUC__ */

#endif _MACHINE_CTHREADS_H_
