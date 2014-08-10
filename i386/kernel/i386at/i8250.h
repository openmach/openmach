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

/*
 * Header file for i8250 chip
 */

/* port offsets from the base i/o address */

#define RDAT		0
#define RIE		1
#define RID		2
#define RFC		2
#define RLC		3
#define RMC		4
#define RLS		5
#define RMS		6
#define RDLSB		0
#define RDMSB		1

/* interrupt control register */

#define IERD		0x01	/* read int */
#define IETX		0x02	/* xmit int */
#define IELS		0x04	/* line status int */
#define IEMS		0x08	/* modem int */

/* interrupt status register */

#define IDIP		0x01	/* not interrupt pending */
#define IDMS		0x00	/* modem int */
#define IDTX		0x02	/* xmit int */
#define IDRD		0x04	/* read int */
#define IDLS		0x06	/* line status int */
#define IDMASK		0x0f	/* interrupt ID mask */

/* line control register */

#define LC5		0x00	/* word length 5 */
#define LC6		0x01	/* word length 6 */
#define LC7		0x02	/* word length 7 */
#define LC8		0x03	/* word length 8 */
#define LCSTB		0x04	/* 2 stop */
#define LCPEN		0x08	/* parity enable */
#define LCEPS		0x10	/* even parity select */
#define LCSP		0x20	/* stick parity */
#define LCBRK		0x40	/* send break */
#define LCDLAB		0x80	/* divisor latch access bit */
#define LCPAR		0x38	/* parity mask */

/* line status register */

#define LSDR		0x01	/* data ready */
#define LSOR		0x02	/* overrun error */
#define LSPE		0x04	/* parity error */
#define LSFE		0x08	/* framing error */
#define LSBI		0x10	/* break interrupt */
#define LSTHRE		0x20	/* xmit holding reg empty */
#define LSTSRE		0x40	/* xmit shift reg empty */

/* modem control register */

#define MCDTR		0x01	/* DTR */
#define MCRTS		0x02	/* RTS */
#define MCOUT1		0x04	/* OUT1 */
#define MCOUT2		0x08	/* OUT2 */
#define MCLOOP		0x10	/* loopback */

/* modem status register */

#define MSDCTS		0x01	/* delta CTS */
#define MSDDSR		0x02	/* delta DSR */
#define MSTERI		0x04	/* delta RE */
#define MSDRLSD 	0x08	/* delta CD */
#define MSCTS		0x10	/* CTS */
#define MSDSR		0x20	/* DSR */
#define MSRI		0x40	/* RE */
#define MSRLSD		0x80	/* CD */

/* divisor latch register settings for various baud rates */

#define BCNT1200	0x60
#define BCNT2400	0x30
#define BCNT4800	0x18
#define BCNT9600	0x0c
