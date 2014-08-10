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

#include <mach/std_types.h>
#include <sys/time.h>
#include <kern/time_stamp.h>

/*
 *	ts.c - kern_timestamp system call.
 */
#ifdef	multimax
#include <mmax/timer.h>
#endif	multimax



kern_return_t
kern_timestamp(tsp)
struct	tsval	*tsp;
{
#ifdef	multimax
	struct	tsval	temp;
	temp.low_val = FRcounter;
	temp.high_val = 0;
#else	multimax
/*
	temp.low_val = 0;
	temp.high_val = ts_tick_count;
*/
	time_value_t temp;
	temp = time;
#endif	multimax

	if (copyout((char *)&temp,
		    (char *)tsp,
		    sizeof(struct tsval)) != KERN_SUCCESS)
	    return(KERN_INVALID_ADDRESS);
	return(KERN_SUCCESS);
}

/*
 *	Initialization procedure.
 */

void timestamp_init()
{
#ifdef	multimax
#else	multimax
	ts_tick_count = 0;
#endif	multimax
}
