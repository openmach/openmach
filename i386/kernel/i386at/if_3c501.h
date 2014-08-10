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
 *	File:	if_3c501.h
 *	Author: Philippe Bernadat
 *	Date:	1989
 * 	Copyright (c) 1989 OSF Research Institute 
 *
 * 	3COM Etherlink 3C501 Mach Ethernet drvier
 */
/*
  Copyright 1990 by Open Software Foundation,
Cambridge, MA.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appears in all copies and
that both the copyright notice and this permission notice appear in
supporting documentation, and that the name of OSF or Open Software
Foundation not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

  OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* The various IE command registers */

#define	EDLC_ADDR(base)	(base)		/* EDLC station address, 6 bytes*/
#define	EDLC_RCV(base)	((base)+0x6)	/* EDLC receive cmd. & stat.	*/
#define	EDLC_XMT(base)	((base)+0x7)	/* EDLC transmit cmd. & stat.	*/
#define	IE_GP(base)	((base)+0x8)	/* General Purpose pointer	*/
#define	IE_RP(base)	((base)+0xa)	/* Receive buffer pointer	*/
#define	IE_SAPROM(base)	((base)+0xc)	/* station addr prom window	*/
#define	IE_CSR(base)	((base)+0xe)	/* IE command and status	*/
#define	IE_BFR(base)	((base)+0xf)	/* 1 byte window on packet buffer*/

/*  CSR Status Register (read)
 *
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * | XMTBSY |  RIDE  |   DMA  |  EDMA  |      BUFCTL     |        | RCVBSY |
 * |________|________|________|________|________|________|________|________|
 *
 */

/*  CSR Command Register (write)
 * 
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * | RESET  |  RIDE  |   DMA  |        |      BUFCTL     |        |  IRE   |
 * |________|________|________|________|________|________|________|________|
 * 
 */

#define	IE_XMTBSY	0x80	/* Transmitter busy (ro)		*/
#define	IE_RESET	0x80	/* reset the controller (wo)		*/
#define	IE_RIDE		0x40	/* request interrupt/DMA enable (rw)	*/
#define	IE_DMA		0x20	/* DMA request (rw)			*/
#define	IE_EDMA		0x10	/* DMA done (ro)			*/
#define	IE_BUFCTL	0x0c	/* mask for buffer control field (rw)	*/
#define	IE_RCVBSY	0x01	/* receive in progress (ro)		*/
#define	IE_IRE		0x01	/* Interrupt request enable		*/

/* BUFCTL values */

#define	IE_LOOP		0x0c	/* 2 bit field in bits 2,3, loopback	*/
#define	IE_RCVEDLC	0x08	/* gives buffer to receiver		*/
#define	IE_XMTEDLC	0x04	/* gives buffer to transmit		*/
#define	IE_SYSBFR	0x00	/* gives buffer to processor		*/

/*  XMTCSR Transmit Status Register (read)
 * 
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * |        |        |        |        |  IDLE  |  16    |   JAM  |  UNDER |
 * |________|________|________|________|________|________|________|________|
 * 
 */

/*  XMTCSR Transmit Command Register (write) enables interrupts when written
 * 
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * |        |        |        |        |        |        |        |        |
 * |________|________|________|________|________|________|________|________|
 * 
 */

#define	EDLC_IDLE	0x08	/* transmit idle			*/
#define	EDLC_16		0x04	/* packet experienced 16 collisions	*/
#define	EDLC_JAM	0x02	/* packet experienced a collision	*/
#define	EDLC_UNDER	0x01	/* data underflow			*/

/*  RCVCSR Receive Status Register (read) 
 * 
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * |  STALE |        |  GOOD  |  ANY   | SHORT  | DRIBBLE|  FCS   |  OVER  |
 * |________|________|________|________|________|________|________|________|
 *
 */

/*  RCVCSR Receive Command Register (write) enables interrupt when written
 * 
 *  _______________________________________________________________________ 
 * |        |        |        |        |        |        |        |        |
 * | ADDR MATCH MODE |  GOOD  |  ANY   | SHORT  | DRIBBLE|  FCS   |  OVER  |
 * |________|________|________|________|________|________|________|________|
 *
 */

#define	EDLC_STALE	0x80	/* receive CSR status previously read 	*/
#define	EDLC_GOOD	0x20	/* well formed packets only 		*/
#define	EDLC_ANY	0x10	/* any packet, even those with errors 	*/
#define	EDLC_SHORT	0x08	/* short frame 				*/
#define	EDLC_DRIBBLE	0x04	/* dribble error 			*/
#define	EDLC_FCS	0x02	/* CRC error 				*/
#define	EDLC_OVER	0x01	/* data overflow 			*/

/* Address Match Mode */

#define	EDLC_NONE	0x00	/* match mode in bits 5-6, write only 	*/
#define	EDLC_ALL	0x40	/* promiscuous receive, write only 	*/
#define	EDLC_BROAD	0x80	/* station address plus broadcast 	*/
#define	EDLC_MULTI	0xc0	/* station address plus multicast 	*/
 
/* Packet Buffer size */

#define BFRSIZ		2048

#define NAT3C501	1
#define ETHER_ADD_SIZE	6	/* size of a MAC address */

#ifndef TRUE
#define TRUE		1
#endif	TRUE
#define	HZ		100

#define	DSF_LOCK	1
#define DSF_RUNNING	2

#define MOD_ENAL 1
#define MOD_PROM 2
