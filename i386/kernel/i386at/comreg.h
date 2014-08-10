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
 *	Olivetti serial port driver v1.0
 *	Copyright Ing. C. Olivetti & C. S.p.A. 1988, 1989
 *	All rights reserved.
 *
 */ 
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#define TXRX(addr)	(addr + 0)
#define BAUD_LSB(addr)	(addr + 0)
#define BAUD_MSB(addr)	(addr + 1)
#define INTR_ENAB(addr)	(addr + 1)
#define INTR_ID(addr)	(addr + 2)
#define FIFO_CTL(addr)	(addr + 2)
#define LINE_CTL(addr)	(addr + 3)
#define MODEM_CTL(addr)	(addr + 4)
#define LINE_STAT(addr)	(addr + 5)
#define MODEM_STAT(addr)(addr + 6)
#define SCR(addr)	(addr + 7)

#define MODi 0
#define TRAi 2
#define RECi 4
#define LINi 6
#define CTIi 0xc
#define MASKi 0xf

/* line control register */
#define		iWLS0	0x01		/*word length select bit 0 */	
#define		iWLS1	0x02		/*word length select bit 2 */	
#define		iSTB	0x04		/* number of stop bits */
#define		iPEN	0x08		/* parity enable */
#define		iEPS	0x10		/* even parity select */
#define		iSP	0x20		/* stick parity */
#define		iSETBREAK 0x40		/* break key */
#define		iDLAB	0x80		/* divisor latch access bit */
#define		i5BITS	0x00		/* 5 bits per char */
#define		i6BITS	0x01		/* 6 bits per char */
#define		i7BITS	0x02		/* 7 bits per char */
#define		i8BITS	0x03		/* 8 bits per char */

/* line status register */
#define		iDR	0x01		/* data ready */
#define		iOR	0x02		/* overrun error */
#define		iPE	0x04		/* parity error */
#define		iFE	0x08		/* framing error */
#define		iBRKINTR 0x10		/* a break has arrived */
#define		iTHRE	0x20		/* tx hold reg is now empty */
#define		iTSRE	0x40		/* tx shift reg is now empty */

/* interrupt id regisger */
#define		iMODEM_INTR	0x01
#define		iTX_INTR	0x02
#define		iRX_INTR	0x04
#define		iERROR_INTR	0x08

/* interrupt enable register */
#define		iRX_ENAB	0x01
#define		iTX_ENAB	0x02
#define		iERROR_ENAB	0x04
#define		iMODEM_ENAB	0x08

/* modem control register */
#define		iDTR		0x01	/* data terminal ready */
#define		iRTS		0x02	/* request to send */
#define		iOUT1		0x04	/* COM aux line -not used */
#define		iOUT2		0x08	/* turns intr to 386 on/off */	
#define		iLOOP		0x10	/* loopback for diagnostics */

/* modem status register */
#define		iDCTS		0x01	/* delta clear to send */
#define		iDDSR		0x02	/* delta data set ready */
#define		iTERI		0x04	/* trail edge ring indicator */
#define		iDRLSD		0x08	/* delta rx line sig detect */
#define		iCTS		0x10	/* clear to send */
#define		iDSR		0x20	/* data set ready */
#define		iRI		0x40	/* ring indicator */
#define		iRLSD		0x80	/* rx line sig detect */

/* fifo control register (only in 16550) */
#define		iFIFOENA	0x01	/* Enable fifos */
#define		iCLRRCVRFIFO	0x02	/* Clear receive fifo */
#define		iCLRXMITFIFO	0x04	/* Clear transmit fifo */
#define		iDMAMODE	0x08	/* DMA transfer enable */
#define		iFIFO1CH	0x00	/* Receive fifo trigger level 1 char */
#define		iFIFO4CH	0x40	/* Receive fifo trigger level 4 chars*/
#define		iFIFO8CH	0x80	/* Receive fifo trigger level 8 chars*/
#define		iFIFO14CH	0xc0	/* Receive fifo trigger level 14 chars*/
