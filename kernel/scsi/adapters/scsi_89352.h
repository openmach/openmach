/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 *	File: scsi_89352.h
 * 	Author: Daniel Stodolsky, Carnegie Mellon University
 *	Date:	06/91
 *
 *	Defines for the Fujitsu MB89352 SCSI Protocol Controller (HBA)
 *	The definitions herein also cover the 89351, 87035/36, 87033B
 */

/*
 * Register map, padded as needed. Matches Hardware, do not screw up. 
 */

#define vuc volatile unsigned char

typedef struct 
{
  vuc 	spc_bdid;		/* Bus Device ID  (R/W) */
#define				spc_myid spc_bdid	
  char	pad0[3];
  vuc	spc_sctl;		/* SPC Control register (R/W) */
  char	pad1[3];
  vuc	spc_scmd;		/* Command Register (R/W) */
  char	pad2[3];
  vuc	spc_tmod;		/* Transmit Mode Register (synch models) */
  char	pad3[3];
  vuc	spc_ints;		/* Interrupt sense (R); Interrupt Reset (w) */
  char	pad4[3];
  vuc	spc_psns;		/* Phase Sense (R); SPC Diagnostic Control (w) */
#define 				spc_phase spc_psns
  char	pad5[3];	
  vuc	spc_ssts;		/* SPC status (R/O) */
  char	pad6[3];
  vuc	spc_serr;		/* SPC error status (R/O) */
  char	pad7[3];
  vuc	spc_pctl;		/* Phase Control (R/W) */
  char	pad8[3];
  vuc	spc_mbc;		/* Modifed Byte Counter (R/O) */
  char	pad9[3];
  vuc	spc_dreg;		/* Data Register (R/W) */
  char	pad10[3];		
  vuc	spc_temp;		/* Temporary Register (R/W) */
  char	pad11[3];
  vuc	spc_tch;		/* Transfer Counter High (R/W) */
  char	pad12[3];
  vuc	spc_tcm;		/* Transfer Counter Middle (R/W) */
  char	pad13[3];
  vuc	spc_tcl;		/* Transfer Counter Low (R/W) */
  char 	pad14[3];
  vuc	spc_exbf;		/* External Buffer (synch models) */
  char 	pad15[3];
} spc_regmap_t;

#undef vuc

/*
 * Control register
 */

#define	SPC_SCTL_DISABLE	0x80
#define	SPC_SCTL_RESET		0x40
#define	SPC_SCTL_DIAGMODE	0x20
#define	SPC_SCTL_ARB_EBL	0x10
#define	SPC_SCTL_PAR_EBL	0x08
#define	SPC_SCTL_SEL_EBL	0x04
#define	SPC_SCTL_RSEL_EBL	0x02
#define	SPC_SCTL_IE		0x01

/*
 * Command register
 */

#define SPC_SCMD_CMDMASK	0xe0
#	define SPC_SCMD_C_ACKREQ_S	0xe0
#	define SPC_SCMD_C_ACKREQ_C	0xc0
#	define SPC_SCMD_C_STOP_X	0xa0
#	define SPC_SCMD_C_XFER		0x80
#	define SPC_SCMD_C_ATN_S		0x60
#	define SPC_SCMD_C_ATN_C		0x40
#	define SPC_SCMD_C_SELECT	0x20
#	define SPC_SCMD_C_BUS_RLSE	0x00
#define SPC_SCMD_BUSRST		0x10
#define	SPC_SCMD_INTERCEPT_X	0x08
#define SPC_SCMD_PROGRAMMED_X	0x04
#define SPC_SCMD_PAD_X		0x01

/*
 * Transfer mode register (MB87033B/35/36)
 */

#define SPC_TMOD_SYNC_X		0x80
#define SPC_TMOD_OFFSET_MASK	0x70
#	define SPC_OFFSET(x)	(((x)<<4)&SPC_TMOD_OFFSET_MASK)
#define SPC_TMOD_PERIOD_MASK	0xc0
#	define SPC_PERIOD(x)	(((x)<<2)&SPC_TMOD_PERIOD_MASK)
#define SPC_TMOD_EXP_COUNTER	0x01

/*
 * Interrupt cause register
 */

#define SPC_INTS_SELECTED	0x80
#define SPC_INTS_RESELECTED	0x40
#define SPC_INTS_DISC		0x20
#define SPC_INTS_DONE		0x10
#define SPC_INTS_BUSREQ		0x08
#define SPC_INTS_TIMEOUT	0x04
#define SPC_INTS_ERROR		0x02
#define SPC_INTS_RESET		0x01

/*
 * SCSI Bus signals ("phase")
 */

#define SPC_BUS_REQ		0x80		/* rw */
#define SPC_BUS_ACK		0x40		/* rw */
#define SPC_BUS_ATN		0x20		/* ro */
#	define	SPC_DIAG_ENBL_XFER	0x20	/* wo */
#define SPC_BUS_SEL		0x10		/* ro */
#define SPC_BUS_BSY		0x08		/* rw */
#define SPC_BUS_MSG		0x04		/* rw */
#define SPC_BUS_CD		0x02		/* rw */
#define SPC_BUS_IO		0x01		/* rw */

#define	SPC_CUR_PHASE(x)	SCSI_PHASE(x)

#define SPC_BSY(r)              (r->spc_phase & SPC_BUS_BSY)

/*
 * Chip status register
 */

#define SPC_SSTS_INI_CON	0x80
#define SPC_SSTS_TGT_CON	0x40
#define SPC_SSTS_BUSY		0x20
#define SPC_SSTS_XIP		0x10
#define SPC_SSTS_RST		0x08
#define SPC_SSTS_TC0		0x04
#define SPC_SSTS_FIFO_FULL	0x02
#define SPC_SSTS_FIFO_EMPTY	0x01

/*
 * Error register
 */

#define SPC_SERR_SEL		0x80		/* Selected */
#define SPC_SERR_RSEL		0x40		/* Reselected */
#define SPC_SERR_DISC		0x20		/* Disconnected */
#define SPC_SERR_CMDC		0x10		/* Command Complete */
#define SPC_SERR_SRVQ		0x08		/* Service Required */
#define SPC_SERR_TIMO		0x04		/* Timeout */
#define SPC_SERR_HARDERR	0x02		/* SPC Hard Error */
#define SPC_SERR_RSTC		0x01		/* Reset Condition */

/*
 * Phase control register
 *
 * [use SPC_CUR_PHASE() here too]
 */

#define SPC_PCTL_BFREE_IE	0x80		/* Bus free (disconnected) */
#define SPC_PCTL_LST_IE		0x40		/* lost arbit (87033) */
#define SPC_PCTL_ATN_IE		0x20		/* ATN set (87033) */
#define SPC_PCTL_RST_DIS	0x10		/* RST asserted */

/*
 * Modified byte counter register
 */

#define SPC_MBC_ECNT_MASK	0xf0		/* 87033 only */
#	define	SPC_MBC_ECNT_GET(x)	(((x)&SPC_MBC_ECNT_MASK)>>4)
#	define	SPC_MBC_ECNT_PUT(x)	(((x)<<4)&SPC_MBC_ECNT_MASK)
#define SPC_MBC_MBC_MASK	0x0f
#	define	SPC_MBC_GET(x)		((x)&SPC_MBC_MBC_MASK)
#	define	SPC_MBC_PUT(x)		((x)&SPC_MBC_MBC_MASK)

/*
 * Transfer counter register(s)
 */

#define SPC_TC_PUT(ptr,val)	{	\
		(ptr)->spc_tch = (((val)>>16)&0xff); \
		(ptr)->spc_tcm = (((val)>> 8)&0xff); \
		(ptr)->spc_tcl = (((val)    )&0xff); \
	}

#define SPC_TC_GET(ptr,val)	{	\
		(val) = (((ptr)->spc_tch & 0xff )<<16) |\
		        (((ptr)->spc_tcm 0 0xff )<<8)  |\
			((ptr)->spc_tcl 0xff);\
	}

/* 87033 in expanded mode */
#define SPC_XTC_PUT(ptr,val)	{	\
		(ptr)->spc_mbc = SPC_MBC_ECNT_PUT(((val)>>24));\
		(ptr)->spc_tch = (((val)>>16)&0xff); \
		(ptr)->spc_tcm = (((val)>> 8)&0xff); \
		(ptr)->spc_tcl = (((val)    )&0xff); \
	}

#define SPC_XTC_GET(ptr,val)	{	\
		(val) = (SPC_MBC_ECNT_GET((ptr)->spc_mbc)<<24)|\
			(((ptr)->spc_tch)<<16)|(((ptr)->spc_tcm)<<8)|\
			((ptr)->spc_tcl);\
	}

