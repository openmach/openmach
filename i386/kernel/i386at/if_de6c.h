/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 * HISTORY
 * $Log: if_de6c.h,v $
 * Revision 1.2  1994/11/08  20:47:25  baford
 * merged in CMU's MK83-MK83a diffs
 *
 * Revision 2.2  93/11/17  18:32:40  dbg
 * 	Moved source into kernel/i386at/DLINK/if_de6c.c, since we
 * 	can't release it but don't want to lose it.
 * 	[93/11/17            dbg]
 * 
 * 	Removed local declaration of HZ.
 * 	[93/01/29            dbg]
 * 
 * 	Created.
 * 	[92/08/13            rvb]
 * 
 */

/* PC/FTP Packet Driver source, conforming to version 1.09 of the spec
 *  Portions (C) Copyright 1990 D-Link, Inc.
 *
 *  Permission is granted to any individual or institution to use, copy,
 *  modify, or redistribute this software and its documentation provided
 *  this notice and the copyright notices are retained.  This software may
 *  not be distributed for profit, either in original form or in derivative
 *  works.  D-Link, inc. makes no representations about the suitability
 *  of this software for any purpose.  D-LINK GIVES NO WARRANTY,
 *  EITHER EXPRESS OR IMPLIED, FOR THE PROGRAM AND/OR DOCUMENTATION
 *  PROVIDED, INCLUDING, WITHOUT LIMITATION, WARRANTY OF MERCHANTABILITY
 *  AND WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE.
 */


#define	DATA(port)	(port + 0)
#define	STAT(port)	(port + 1)
#define	CMD(port)	(port + 2)

/*  DE-600's DATA port Command */
#define	WRITE	0x00	/* write memory */
#define	READ	0x01	/* read  memory */
#define	STATUS	0x02	/* read  status register */
#define	COMMAND	0x03	/* write command register */
#define	    RX_NONE	0x00	/*  M1=0, M0=0 (bit 1,0) */
#define	    RX_ALL	0x01	/*  M1=0, M0=1 */
#define	    RX_BP	0x02	/*  M1=1, M0=0 */
#define	    RX_MBP	0x03	/*  M1=1, M0=1 */
#define	    TXEN	0x04	/*  bit 2 */
#define	    RXEN	0x08	/*  bit 3 */
#define	    LOOPBACK	0x0c	/*  RXEN=1, TXEN=1 */
#define	    IRQINV	0x40	/*  bit 6   -- IRQ inverse */
#define	    RESET	0x80	/*  set bit 7 high */
#define	    STOP_RESET	0x00	/*  set bit 7 low */
#define	NUL_CMD	0x04	/* null command */
#define	RX_LEN	0x05	/* read  Rx packet length */
#define	TX_ADR	0x06	/* write Tx address */
#define	RW_ADR	0x07	/* write memory address */

/*  DE-600's CMD port Command */
/* #define	CMD(port)	(port + 2) */
#define	SLT_NIC		0x04  /* select Network Interface Card */
#define	SLT_PRN		0x1c  /* select Printer */
#define	NML_PRN		0xec  /* normal Printer situation */
#define	IRQEN		0x10  /* enable IRQ line */

/*  DE-600's STAT port bits 7-4 */
/* #define	STAT(port)	(port + 1) */
#define	RXBUSY		0x80
#define	GOOD		0x40
#define	RESET_FLAG	0x20
#define	T16		0x10
#define	TXBUSY		0x08

#define	STROBE		0x08
#define	EADDR		0x2000	/* HA13=0 => Mem, HA13=1 => Node Num */
#define	BFRSIZ		0x0800	/* number of bytes in a buffer */
#define	BFRS		4

#define	DSF_LOCK	1
#define DSF_RUNNING	2

#define MOD_ENAL 1
#define MOD_PROM 2

