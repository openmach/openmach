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
 *	File: dz_7085.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Defines for the DEC 7085 Serial Line Controller Chip
 */

#define	NDZ_LINE		4

/*
 * What's hanging off those 4 lines
 */

#define DZ_LINE_KEYBOARD	0
#define DZ_LINE_MOUSE		1
#define DZ_LINE_MODEM		2
#define DZ_LINE_PRINTER		3

/*
 * Register layout, ignoring padding
 */
typedef struct {
	volatile unsigned short	dz_csr;		/* Control and Status */
	volatile unsigned short	dz_rbuf;	/* Rcv buffer (RONLY) */
	volatile unsigned short	dz_tcr;		/* Xmt control (R/W)*/
	volatile unsigned short	dz_tbuf;	/* Xmt buffer (WONLY)*/
#	define 			dz_lpr dz_rbuf	/* Line parameters (WONLY)*/
#	define 			dz_msr dz_tbuf	/* Modem status (RONLY)*/
} dz_regmap_t;

/*
 * CSR bits
 */

#define DZ_CSR_MBZ		0x3c07	/* Must be zero */
#define DZ_CSR_MAINT		0x0008	/* rw: Maintenance mode */
#define DZ_CSR_CLR		0x0010	/* rw: Master clear (init) */
#define DZ_CSR_MSE		0x0020	/* rw: Master scan enable */
#define DZ_CSR_RIE		0x0040	/* rw: Rcv Interrupt Enable */
#define DZ_CSR_RDONE		0x0080	/* ro: Rcv done (silo avail) */
#define DZ_CSR_TLINE		0x0300	/* ro: Lineno ready for xmt */
#define DZ_CSR_TIE		0x4000	/* rw: Xmt Interrupt Enable */
#define DZ_CSR_TRDY		0x8000	/* ro: Xmt ready */

/*
 * Receiver buffer (top of silo).  Read-only.
 */

#define DZ_SILO_DEEP		64

#define DZ_RBUF_CHAR		0x00ff	/* Received character */
#define DZ_RBUF_RLINE		0x0300	/* Line it came from */
#define DZ_RBUF_XXXX		0x0c00	/* Reads as zero */
#define DZ_RBUF_PERR		0x1000	/* Parity error */
#define DZ_RBUF_FERR		0x2000	/* Framing error (break) */
#define DZ_RBUF_OERR		0x4000	/* Silo overrun */
#define DZ_RBUF_VALID		0x8000	/* Info is valid */

/*
 * Line parameters register.  Write-only.
 */

#define DZ_LPAR_LINE		0x0003	/* Bin encoded line no */
#define DZ_LPAR_MBZ		0xe004	/* Must be zero */
#define DZ_LPAR_CLEN		0x0018	/* Character length: */
#	define DZ_LPAR_5BITS	0x0000	/* 5 bits per char */
#	define DZ_LPAR_6BITS	0x0008	/* 6 bits per char */
#	define DZ_LPAR_7BITS	0x0010	/* 7 bits per char */
#	define DZ_LPAR_8BITS	0x0018	/* 8 bits per char */
#define DZ_LPAR_STOP		0x0020	/* stop bits: off->1, on->2 */
#define DZ_LPAR_PAR_ENB		0x0040	/* generate/detect parity */
#define DZ_LPAR_ODD_PAR		0x0080	/* generate/detect ODD parity */
#define DZ_LPAR_SPEED		0x0f00	/* Speed code: */
#	define DZ_LPAR_50	0x0000	/* 50 baud */
#	define DZ_LPAR_75	0x0100	/* 75 baud */
#	define DZ_LPAR_110	0x0200	/* 110 baud */
#	define DZ_LPAR_134_5	0x0300	/* 134.5 baud */
#	define DZ_LPAR_150	0x0400	/* 150 baud */
#	define DZ_LPAR_300	0x0500	/* 300 baud */
#	define DZ_LPAR_600	0x0600	/* 600 baud */
#	define DZ_LPAR_1200	0x0700	/* 1200 baud */
#	define DZ_LPAR_1800	0x0800	/* 1800 baud */
#	define DZ_LPAR_2000	0x0900	/* 2000 baud */
#	define DZ_LPAR_2400	0x0a00	/* 2400 baud */
#	define DZ_LPAR_3600	0x0b00	/* 3600 baud */
#	define DZ_LPAR_4800	0x0c00	/* 4800 baud */
#	define DZ_LPAR_7200	0x0d00	/* 7200 baud */
#	define DZ_LPAR_9600	0x0e00	/* 9600 baud */
#	define DZ_LPAR_MAX_SPEED	0x0f00	/* 19200/38400 baud */
#define DZ_LPAR_ENABLE		0x1000	/* Enable receiver */

/*
 * Xmt control register
 */

#define DZ_TCR_LNENB		0x000f	/* rw: Xmt line enable */
#define DZ_TCR_MBZ		0xf0f0	/* Must be zero */
#define DZ_TCR_DTR3		0x0100	/* rw: DTR on printer line */
#define DZ_TCR_RTS3		0x0200	/* rw: RTS on printer line */
#define DZ_TCR_DTR2		0x0400	/* rw: DTR on modem line */
#define DZ_TCR_RTS2		0x0800	/* rw: RTS on modem line */

/*
 * Modem status register. Read-only.
 */

#define DZ_MSR_CTS3		0x0001	/* Clear To Send,  printer line */
#define DZ_MSR_DSR3		0x0002	/* Data Set Ready, printer line */
#define DZ_MSR_CD3		0x0004	/* Carrier Detect, printer line */
#define DZ_MSR_RI3		0x0008	/* Ring Indicator, printer line */
#define DZ_MSR_XXXX		0xf0f0	/* Reads as zero */
#define DZ_MSR_CTS2		0x0100	/* Clear To Send,  modem line */
#define DZ_MSR_DSR2		0x0200	/* Data Set Ready, modem line */
#define DZ_MSR_CD2		0x0400	/* Carrier Detect, modem line */
#define DZ_MSR_RI2		0x0800	/* Ring Indicator, modem line */


/*
 * Xmt buffer
 */

#define DZ_TBUF_CHAR		0x00ff	/* Xmt character */
#define DZ_TBUF_BREAK_0		0x0100	/* set line 0 to space */
#define DZ_TBUF_BREAK_1		0x0200	/* set line 1 to space */
#define DZ_TBUF_BREAK_2		0x0400	/* set line 2 to space */
#define DZ_TBUF_BREAK_3		0x0800	/* set line 3 to space */
#define DZ_TBUF_MBZ		0xf000	/* Must be zero */
