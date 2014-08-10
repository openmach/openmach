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

#include <sys/types.h>
#include <i386/ipl.h>
#include <i386/pic.h>
#include <rc.h>


/* These interrupts are always present */
extern intnull(), fpintr(), hardclock(), kdintr();
extern prtnull();

int (*ivect[NINTR])() = {
	/* 00 */	hardclock,	/* always */
#if RCLINE < 0
	/* 01 */	kdintr,		/* kdintr, ... */
#else
	/* 01 */	intnull,	/* kdintr, ... */
#endif
	/* 02 */	intnull,
	/* 03 */	intnull,	/* lnpoll, comintr, ... */

	/* 04 */	intnull,	/* comintr, ... */
	/* 05 */	intnull,	/* comintr, wtintr, ... */
	/* 06 */	intnull,	/* fdintr, ... */
	/* 07 */	prtnull,	/* qdintr, ... */

	/* 08 */	intnull,
	/* 09 */	intnull,	/* ether */
	/* 10 */	intnull,
	/* 11 */	intnull,

	/* 12 */	intnull,
	/* 13 */	fpintr,		/* always */
	/* 14 */	intnull,	/* hdintr, ... */
	/* 15 */	intnull,	/* ??? */
};

int intpri[NINTR] = {
	/* 00 */   	0,	SPL6,	0,	0,
	/* 04 */	0,	0,	0,	0,
	/* 08 */	0,	0,	0,	0,
	/* 12 */	0,	SPL1,	0,	0,
};
