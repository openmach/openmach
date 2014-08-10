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
 *	File: serial_defs.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	7/91
 *
 *	Generic console driver for serial-line based consoles.
 */


/*
 * Common defs
 */

extern int	(*console_probe)(), (*console_param)(), (*console_start)(),
		(*console_putc)(), (*console_getc)(),
		(*console_pollc)(), (*console_mctl)(), (*console_softCAR)();
extern		cngetc(), cnmaygetc(), cnputc(), rcputc();

extern struct tty	*console_tty[];
extern int rcline, cnline;
extern int	console;

/* Simple one-char-at-a-time scheme */
extern		cons_simple_tint(), cons_simple_rint();

#define	CONS_ERR_PARITY		0x1000
#define	CONS_ERR_BREAK		0x2000
#define	CONS_ERR_OVERRUN	0x4000
