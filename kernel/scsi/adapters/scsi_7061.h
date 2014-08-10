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
 *	File: scsi_7061.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Defines for the DEC DC7061 SII gate array (SCSI interface)
 */

/*
 * Register map
 */

typedef struct {
	volatile unsigned short	sii_sdb;	/* rw: Data bus and parity */
	volatile unsigned short	sii_sc1;	/* rw: scsi signals 1 */
	volatile unsigned short	sii_sc2;	/* rw: scsi signals 2 */
	volatile unsigned short	sii_csr;	/* rw: control and status */
	volatile unsigned short	sii_id;		/* rw: scsi bus ID */
	volatile unsigned short	sii_sel_csr;	/* rw: selection status */
	volatile unsigned short	sii_destat;	/* ro: selection detector status */
	volatile unsigned short	sii_dstmo;	/* unsupp: dssi timeout */
	volatile unsigned short	sii_data;	/* rw: data register */
	volatile unsigned short	sii_dma_ctrl;	/* rw: dma control reg */
	volatile unsigned short	sii_dma_len;	/* rw: length of transfer */
	volatile unsigned short	sii_dma_adr_low;/* rw: low address */
	volatile unsigned short	sii_dma_adr_hi;	/* rw: high address */
	volatile unsigned short	sii_dma_1st_byte;/* rw: initial byte */
	volatile unsigned short	sii_stlp;	/* unsupp: dssi short trgt list ptr */
	volatile unsigned short	sii_ltlp;	/* unsupp: dssi long " " " */
	volatile unsigned short	sii_ilp;	/* unsupp: dssi initiator list ptr */
	volatile unsigned short	sii_dssi_csr;	/* unsupp: dssi control */
	volatile unsigned short	sii_conn_csr;	/* rc: connection interrupt control */
	volatile unsigned short	sii_data_csr;	/* rc: data interrupt control */
	volatile unsigned short	sii_cmd;	/* rw: command register */
	volatile unsigned short	sii_diag_csr;	/* rw: disgnostic status */
} sii_regmap_t;

/*
 * Data bus register (diag)
 */

#define SII_SDB_DATA		0x00ff		/* data bits, assert high */
#define SII_SDB_PARITY		0x0100		/* parity bit */

/*
 * Control signals one (diag)
 */

#define SII_CS1_IO		0x0001		/* I/O bit */
#define SII_CS1_CD		0x0002		/* Control/Data bit */
#define SII_CS1_MSG		0x0004		/* Message bit */
#define SII_CS1_ATN		0x0008		/* Attention bit */
#define SII_CS1_REQ		0x0010		/* Request bit */
#define SII_CS1_ACK		0x0020		/* Acknowledge bit */
#define SII_CS1_RST		0x0040		/* Reset bit */
#define SII_CS1_SEL		0x0080		/* Selection bit */
#define SII_CS1_BSY		0x0100		/* Busy bit */

/*
 * Control signals two (diag)
 */

#define SII_CS2_SBE		0x0001		/* Bus enable */
#define SII_CS2_ARB		0x0002		/* arbitration enable */
#define SII_CS2_TGS		0x0004		/* Target role steer */
#define SII_CS2_IGS		0x0008		/* Initiator role steer */

/*
 * Control and status register
 */

#define SII_CSR_IE		0x0001		/* Interrupt enable */
#define SII_CSR_PCE		0x0002		/* Parity check enable */
#define SII_CSR_SLE		0x0004		/* Select enable */
#define SII_CSR_RSE		0x0008		/* Reselect enable */
#define SII_CSR_HPM		0x0010		/* Arbitration enable */

/*
 * SCSI bus ID register
 */

#define SII_ID_MASK		0x0007		/* The scsi ID */
#define SII_ID_IO		0x8000		/* ID pins are in/out */

/*
 * Selector control and status register
 */

#define SII_SEL_ID		0x0003		/* Destination ID */

/*
 * Selection detector status register
 */

#define SII_DET_ID		0x0003		/* Selector's ID */

/*
 * Data register (silo)
 */

#define SII_DATA_VAL		0x00ff		/* Lower byte */

/*
 * DMA control register
 */

#define SII_DMA_SYN_OFFSET	0x0003		/* 0 -> asynch */

/*
 * DMA counter
 */

#define SII_DMA_COUNT_MASK	0x1fff		/* in bytes */

/*
 * DMA address registers
 */

#define SII_DMA_LOW_MASK	0xffff		/* all bits */
#define SII_DMA_HIGH_MASK	0x0003		/* unused ones mbz */

/*
 * DMA initial byte
 */

#define SII_DMA_IBYTE		0x00ff		/* for odd address DMAs */

/*
 * Connection status register
 */

#define SII_CON_LST		0x0002		/* ro: lost arbitration */
#define SII_CON_SIP		0x0004		/* ro: selection InProgress */
#define SII_CON_SWA		0x0008		/* rc: selected with ATN */
#define SII_CON_TGT		0x0010		/* ro: target role */
#define SII_CON_DST		0x0020		/* ro: sii is destination */
#define SII_CON_CON		0x0040		/* ro: sii is connected */
#define SII_CON_SCH		0x0080		/* rci: state change */
#define SII_CON_LDN		0x0100		/* ??i: dssi list elm done */
#define SII_CON_BUF		0x0200		/* ??i: dssi buffer service */
#define SII_CON_TZ		0x0400		/* ??: dssi target zero */
#define SII_CON_OBC		0x0800		/* ??i: dssi outen bit clr */
#define SII_CON_BERR		0x1000		/* rci: bus error */
#define SII_CON_RST		0x2000		/* rci: RST asserted */
#define SII_CON_DI		0x4000		/* ro: data_csr intr */
#define SII_CON_CI		0x8000		/* ro: con_csr intr */

/*
 * Data transfer status register
 */

#define SII_DTR_IO		0x0001		/* ro: I/O asserted */
#define SII_DTR_CD		0x0002		/* ro: CD asserted */
#define SII_DTR_MSG		0x0004		/* ro: MSG asserted */
#define SII_DTR_ATN		0x0008		/* rc: ATN found asserted */
#define SII_DTR_MIS		0x0010		/* roi: phase mismatch */
#define SII_DTR_OBB		0x0100		/* ro: odd byte boundry */
#define SII_DTR_IPE		0x0200		/* ro: incoming parity err */
#define SII_DTR_IBF		0x0400		/* roi: input buffer full */
#define SII_DTR_TBE		0x0800		/* roi: xmt buffer empty */
#define SII_DTR_TCZ		0x1000		/* ro: xfer counter zero */
#define SII_DTR_DONE		0x2000		/* rci: xfer complete */
#define SII_DTR_DI		0x4000		/* ro: data_csr intr */
#define SII_DTR_CI		0x8000		/* ro: con_csr intr */

#define	SII_PHASE(dtr)		SCSI_PHASE(dtr)


/*
 * Command register
 *
 * Certain bits are only valid in certain roles:
 *	I - Initiator   D - Destination   T - Target
 * Bits 0-3  give the 'expected phase'
 * Bits 4-6  give the 'expected state'
 * Bits 7-11 are the 'command' proper
 */

#define SII_CMD_IO		0x0001		/* rw: (T) assert I/O */
#define SII_CMD_CD		0x0002		/* rw: (T) assert CD */
#define SII_CMD_MSG		0x0004		/* rw: (T) assert MSG */
#define SII_CMD_ATN		0x0008		/* rw: (I) assert ATN */

#define SII_CMD_TGT		0x0010		/* rw: (DIT) target */
#define SII_CMD_DST		0x0020		/* rw: (DIT) destination */
#define SII_CMD_CON		0x0040		/* rw: (DIT) connected */

#define SII_CMD_RESET		0x0080		/* rw: (DIT) reset */
#define SII_CMD_DIS		0x0100		/* rw: (DIT) disconnect */
#define SII_CMD_REQ		0x0200		/* rw: (T) request data */
#define SII_CMD_SEL		0x0400		/* rw: (D) select */
#define SII_CMD_XFER		0x0800		/* rw: (IT) xfer information */

#define SII_CMD_RSL		0x1000		/* rw: reselect target */
#define SII_CMD_RST		0x4000		/* zw: assert RST */
#define SII_CMD_DMA		0x8000		/* rw: command uses DMA */

/*
 * Diagnostic control register
 */

#define SII_DIAG_TEST		0x0001		/* rw: test mode */
#define SII_DIAG_DIA		0x0002		/* rw: ext loopback mode */
#define SII_DIAG_PORT_ENB	0x0004		/* rw: enable drivers */
#define SII_DIAG_LPB		0x0008		/* rw: loopback reg writes */
