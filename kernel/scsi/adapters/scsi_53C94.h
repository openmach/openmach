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
 *	File: scsi_53C94.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Defines for the NCR 53C94 ASC (SCSI interface)
 * 	Some gotcha came from the "86C01/53C94 DMA lab work" written
 * 	by Ken Stewart (NCR MED Logic Products Applications Engineer)
 * 	courtesy of NCR.  Thanks Ken !
 */

/*
 * Register map
 */

typedef struct {
	volatile unsigned char	asc_tc_lsb;	/* rw: Transfer Counter LSB */
	volatile unsigned char	asc_tc_msb;	/* rw: Transfer Counter MSB */
	volatile unsigned char	asc_fifo;	/* rw: FIFO top */
	volatile unsigned char	asc_cmd;	/* rw: Command */
	volatile unsigned char	asc_csr;	/* r:  Status */
#define			asc_dbus_id asc_csr	/* w: Destination Bus ID */
	volatile unsigned char	asc_intr;	/* r:  Interrupt */
#define			asc_sel_timo asc_intr	/* w: (re)select timeout */
	volatile unsigned char	asc_ss;		/* r:  Sequence Step */
#define			asc_syn_p asc_ss	/* w: synchronous period */
	volatile unsigned char	asc_flags;	/* r:  FIFO flags + seq step */
#define			asc_syn_o asc_flags	/* w: synchronous offset */
	volatile unsigned char	asc_cnfg1;	/* rw: Configuration 1 */
	volatile unsigned char	asc_ccf;	/* w:  Clock Conv. Factor */
	volatile unsigned char	asc_test;	/* w:  Test Mode */
	volatile unsigned char	asc_cnfg2;	/* rw: Configuration 2 */
	volatile unsigned char	asc_cnfg3;	/* rw: Configuration 3 */
	volatile unsigned char	asc_rfb;	/* w:  Reserve FIFO byte */
} asc_regmap_t;


/*
 * Transfer Count: access macros
 * That a NOP is required after loading the dma counter
 * I learned on the NCR test code. Sic.
 */

#define	ASC_TC_MAX	0x10000

#define ASC_TC_GET(ptr,val)				\
	val = ((ptr)->asc_tc_lsb&0xff)|(((ptr)->asc_tc_msb&0xff)<<8)
#define ASC_TC_PUT(ptr,val)				\
	(ptr)->asc_tc_lsb=(val);			\
	(ptr)->asc_tc_msb=(val)>>8; mb();		\
	(ptr)->asc_cmd = ASC_CMD_NOP|ASC_CMD_DMA;

/*
 * FIFO register
 */

#define ASC_FIFO_DEEP		16


/*
 * Command register (command codes)
 */

#define ASC_CMD_DMA		0x80
					/* Miscellaneous */
#define ASC_CMD_NOP		0x00
#define ASC_CMD_FLUSH		0x01
#define ASC_CMD_RESET		0x02
#define ASC_CMD_BUS_RESET	0x03
					/* Initiator state */
#define ASC_CMD_XFER_INFO	0x10
#define ASC_CMD_I_COMPLETE	0x11
#define ASC_CMD_MSG_ACPT	0x12
#define ASC_CMD_XFER_PAD	0x18
#define ASC_CMD_SET_ATN		0x1a
#define ASC_CMD_CLR_ATN		0x1b
					/* Target state */
#define ASC_CMD_SND_MSG		0x20
#define ASC_CMD_SND_STATUS	0x21
#define ASC_CMD_SND_DATA	0x22
#define ASC_CMD_DISC_SEQ	0x23
#define ASC_CMD_TERM		0x24
#define ASC_CMD_T_COMPLETE	0x25
#define ASC_CMD_DISC		0x27
#define ASC_CMD_RCV_MSG		0x28
#define ASC_CMD_RCV_CDB		0x29
#define ASC_CMD_RCV_DATA	0x2a
#define ASC_CMD_RCV_CMD		0x2b
#define ASC_CMD_ABRT_DMA	0x04
					/* Disconnected state */
#define ASC_CMD_RESELECT	0x40
#define ASC_CMD_SEL		0x41
#define ASC_CMD_SEL_ATN		0x42
#define ASC_CMD_SEL_ATN_STOP	0x43
#define ASC_CMD_ENABLE_SEL	0x44
#define ASC_CMD_DISABLE_SEL	0x45
#define ASC_CMD_SEL_ATN3	0x46

/* this is approximate (no ATN3) but good enough */
#define	asc_isa_select(cmd)	(((cmd)&0x7c)==0x40)

/*
 * Status register, and phase encoding
 */

#define ASC_CSR_INT		0x80
#define ASC_CSR_GE		0x40
#define ASC_CSR_PE		0x20
#define ASC_CSR_TC		0x10
#define ASC_CSR_VGC		0x08
#define ASC_CSR_MSG		0x04
#define ASC_CSR_CD		0x02
#define ASC_CSR_IO		0x01

#define	ASC_PHASE(csr)		SCSI_PHASE(csr)

/*
 * Destination Bus ID
 */

#define ASC_DEST_ID_MASK	0x07


/*
 * Interrupt register
 */

#define ASC_INT_RESET		0x80
#define ASC_INT_ILL		0x40
#define ASC_INT_DISC		0x20
#define ASC_INT_BS		0x10
#define ASC_INT_FC		0x08
#define ASC_INT_RESEL		0x04
#define ASC_INT_SEL_ATN		0x02
#define ASC_INT_SEL		0x01


/*
 * Timeout register:
 *
 *	val = (timeout * CLK_freq) / (8192 * CCF);
 */

#define	asc_timeout_250(clk,ccf)	((31*clk)/ccf)

/*
 * Sequence Step register
 */

#define ASC_SS_XXXX		0xf0
#define ASC_SS_SOM		0x80
#define ASC_SS_MASK		0x07
#define	ASC_SS(ss)		((ss)&ASC_SS_MASK)

/*
 * Synchronous Transfer Period
 */

#define ASC_STP_MASK		0x1f
#define ASC_STP_MIN		0x05		/* 5 clk per byte */
#define ASC_STP_MAX		0x04		/* after ovfl, 35 clk/byte */

/*
 * FIFO flags
 */

#define ASC_FLAGS_SEQ_STEP	0xe0
#define ASC_FLAGS_FIFO_CNT	0x1f

/*
 * Synchronous offset
 */

#define ASC_SYNO_MASK		0x0f		/* 0 -> asyn */

/*
 * Configuration 1
 */

#define ASC_CNFG1_SLOW		0x80
#define ASC_CNFG1_SRD		0x40
#define ASC_CNFG1_P_TEST	0x20
#define ASC_CNFG1_P_CHECK	0x10
#define ASC_CNFG1_TEST		0x08
#define ASC_CNFG1_MY_BUS_ID	0x07

/*
 * CCF register
 */

#define ASC_CCF_10MHz		0x2
#define ASC_CCF_15MHz		0x3
#define ASC_CCF_20MHz		0x4
#define ASC_CCF_25MHz		0x5

#define	mhz_to_ccf(x)		(((x-1)/5)+1)	/* see specs for limits */

/*
 * Test register
 */

#define ASC_TEST_XXXX		0xf8
#define ASC_TEST_HI_Z		0x04
#define ASC_TEST_I		0x02
#define ASC_TEST_T		0x01

/*
 * Configuration 2
 */

#define ASC_CNFG2_RFB		0x80
#define ASC_CNFG2_EPL		0x40
#define ASC_CNFG2_EBC		0x20
#define ASC_CNFG2_DREQ_HIZ	0x10
#define ASC_CNFG2_SCSI2		0x08
#define ASC_CNFG2_BPA		0x04
#define ASC_CNFG2_RPE		0x02
#define ASC_CNFG2_DPE		0x01

/*
 * Configuration 3
 */

#define ASC_CNFG3_XXXX		0xf8
#define ASC_CNFG3_SRB		0x04
#define ASC_CNFG3_ALT_DMA	0x02
#define ASC_CNFG3_T8		0x01

