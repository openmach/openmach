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

#ifndef	_KERN_TIME_OUT_H_
#define	_KERN_TIME_OUT_H_

/*
 * Mach time-out and time-of-day facility.
 */

#include <mach/boolean.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/zalloc.h>

/*
 *	Timers in kernel:
 */
extern unsigned long elapsed_ticks; /* number of ticks elapsed since bootup */
extern int	hz;		/* number of ticks per second */
extern int	tick;		/* number of usec per tick */

/*
 *	Time-out element.
 */
struct timer_elt {
	queue_chain_t	chain;		/* chain in order of expiration */
	int		(*fcn)();	/* function to call */
	char *		param;		/* with this parameter */
	unsigned long	ticks;		/* expiration time, in ticks */
	int		set;		/* unset | set | allocated */
};
#define	TELT_UNSET	0		/* timer not set */
#define	TELT_SET	1		/* timer set */
#define	TELT_ALLOC	2		/* timer allocated from pool */

typedef	struct timer_elt	timer_elt_data_t;
typedef	struct timer_elt	*timer_elt_t;

/* for 'private' timer elements */
extern void		set_timeout();
extern boolean_t	reset_timeout();

/* for public timer elements */
extern void		timeout();
extern boolean_t	untimeout();

#define	set_timeout_setup(telt,fcn,param,interval)	\
	((telt)->fcn = (fcn),				\
	 (telt)->param = (param),			\
	 (telt)->private = TRUE,			\
	 set_timeout((telt), (interval)))

#define	reset_timeout_check(t) \
	MACRO_BEGIN \
	if ((t)->set) \
	    reset_timeout((t)); \
	MACRO_END

#endif	_KERN_TIME_OUT_H_
