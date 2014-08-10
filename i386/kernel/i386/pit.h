/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <platforms.h>
#if	defined(MB1) || defined(MB2) || EXL > 0 || iPSC386
/* Definitions for 8254 Programmable Interrupt Timer ports on 386/20 */
#define PITCTR0_PORT	0xD0		/* counter 0 port */	
#define PITCTR1_PORT	0xD2		/* counter 1 port */	
#define PITCTR2_PORT	0xD4		/* counter 2 port */	
#define PITCTL_PORT	0xD6		/* PIT control port */
#else	/* defined(AT386) || defined(PS2) */
/* Definitions for 8254 Programmable Interrupt Timer ports on AT 386 */
#define PITCTR0_PORT	0x40		/* counter 0 port */	
#define PITCTR1_PORT	0x41		/* counter 1 port */	
#define PITCTR2_PORT	0x42		/* counter 2 port */	
#define PITCTL_PORT	0x43		/* PIT control port */
#define PITAUX_PORT	0x61		/* PIT auxiliary port */
/* bits used in auxiliary control port for timer 2 */
#define PITAUX_GATE2	0x01		/* aux port, PIT gate 2 input */
#define PITAUX_OUT2	0x02		/* aux port, PIT clock out 2 enable */
#endif	/* defined(AT386) || defined(PS2) */

/* Following are used for Timer 0 */
#define PIT_C0          0x00            /* select counter 0 */
#define PIT_LOADMODE	0x30		/* load least significant byte followed
					 * by most significant byte */
#define PIT_NDIVMODE	0x04		/*divide by N counter */
#define PIT_SQUAREMODE	0x06		/* square-wave mode */

/* Used for Timer 1. Used for delay calculations in countdown mode */
#define PIT_C1          0x40            /* select counter 1 */
#define PIT_READMODE	0x30		/* read or load least significant byte
					 * followed by most significant byte */
#define PIT_RATEMODE	0x06		/* square-wave mode for USART */

/*
 * Clock speed for the timer in hz divided by the constant HZ
 * (defined in param.h)
 */
#if	AT386 || PS2
#define CLKNUM		1193167
#endif	/* AT386 || PS2 */
#if	defined(MB1)
#define CLKNUM		12300
#endif
#if	defined(MB2) || EXL > 0
#define CLKNUM		12500
#endif
#if	iPSC386
#define CLKNUM          1000000
#endif	iPSC386

#if	EXL
/* added micro-timer support.   --- csy */
typedef struct time_latch {
		time_t	ticks;          /* time in HZ since boot */
		time_t	uticks;         /* time in 1.25 MHZ */
/* don't need these two for now.   --- csy */
/*		time_t  secs;           /* seconds since boot */
/*		time_t  epochsecs;      /* seconds since epoch */
	} time_latch;
/* a couple in-line assembly codes for efficiency. */
asm  int   intr_disable()
{
     pushfl
     cli
}

asm  int   intr_restore()
{
     popfl
}

#endif	EXL
