/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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

#ifndef	_I386_TIMER_H_
#define _I386_TIMER_H_

/*
 *	Machine dependent timer definitions.
 */

#include <platforms.h>

#ifdef	SYMMETRY

/*
 *	TIMER_MAX is not used on the Sequent because a 32-bit rollover
 *	timer does not need to be adjusted for maximum value.
 */

/*
 *	TIMER_RATE is the rate of the timer in ticks per second.
 *	It is used to calculate percent cpu usage.
 */

#define TIMER_RATE	1000000

/*
 *	TIMER_HIGH_UNIT is the unit for high_bits in terms of low_bits.
 *	Setting it to TIMER_RATE makes the high unit seconds.
 */

#define TIMER_HIGH_UNIT	TIMER_RATE

/*
 *	TIMER_ADJUST is used to adjust the value of a timer after
 *	it has been copied into a time_value_t.  No adjustment is needed
 *	on Sequent because high_bits is in seconds.
 */

/*
 *	MACHINE_TIMER_ROUTINES should defined if the timer routines are
 *	implemented in machine-dependent code (e.g. assembly language).
 */
#define	MACHINE_TIMER_ROUTINES

#endif

#endif	/* _I386_TIMER_H_ */
