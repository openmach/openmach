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
 *	File: fdi_82077_hdw.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Defines for the Intel 82077 Floppy Disk Controller chip.
 *	Includes defines for 8272A and 82072.
 */

#ifndef	_FDC_82077_H_
#define	_FDC_82077_H_

/*
 * Chips we claim to understand, and their modes
 */
#define	fdc_8272a	0
#define	fdc_82072	1
#define	fdc_82077aa	2

#define	at_mode		0
#define	ps2_mode	1
#define	mod30_mode	2

#define	DRIVES_PER_FDC	4

/*
 * Register maps
 */
typedef struct {
	volatile unsigned char	fd_sra;		/* r:  status register A */
	volatile unsigned char	fd_srb;		/* r:  status register B */
	volatile unsigned char	fd_dor;		/* rw: digital output reg */
	volatile unsigned char	fd_tdr;		/* rw: tape drive register */
	volatile unsigned char	fd_msr;		/* r:  main status register */
#define				fd_dsr	fd_msr	/* w:  data rate select reg */
	volatile unsigned char	fd_data;	/* rw: fifo */
	volatile unsigned char	fd_xxx;		/* --reserved-- */
	volatile unsigned char	fd_dir;		/* r:  digital input reg */
#define				fd_ccr	fd_dir	/* w:  config control reg */
} fd_regmap_t;

typedef struct {
	volatile unsigned char	fd_msr;		/* r:  main status register */
	volatile unsigned char	fd_data;	/* rw: data register */
} fd_8272a_regmap_t;

typedef fd_8272a_regmap_t	fd_82072_regmap_t;
/*#define			fd_dsr	fd_msr	/* w:  data rate select reg */

/*
 * Status register A (82077AA only)
 *
 * Only available in PS/2 (ps2) and Model 30 (m30) modes,
 * not available in PC/AT (at) mode.
 * Some signals have inverted polarity (~) on mod30
 */

#define	FD_SRA_INT		0x80	/* interrupt  */
#define	FD_SRA_DRV2		0x40	/* 2nd drive installed (ps2) */
#define	FD_SRA_DRQ		0x40	/* dma request (mod30) */
#define	FD_SRA_STEP		0x20	/* step pulse (~mod30) */
#define	FD_SRA_TRK0		0x10	/* Track 0 (~mod30) */
#define	FD_SRA_HDSEL		0x08	/* select head 1 (~mod30) */
#define	FD_SRA_INDX		0x04	/* Index hole (~mod30) */
#define	FD_SRA_WP		0x02	/* write protect (~mod30) */
#define	FD_SRA_DIR		0x01	/* step dir, 1->center (~mod30) */

/*
 * Status register B (82077AA only)
 * Not available in at mode.
 */

#define	FD_SRB_DRV2		0x80	/* 2nd drive installed (mod30) */
					/* wired 1 on ps2 */

#define	FD_SRB_DS1		0x40	/* drive select 1 (mod30) */
					/* wired 1 on ps2 */

#define	FD_SRB_DS0		0x20	/* drive select 0 */
#define	FD_SRB_WRDATA		0x10	/* out data (toggle or ~trigger) */
#define	FD_SRB_RDDATA		0x08	/*  in data (toggle or ~trigger) */
#define	FD_SRB_WE		0x04	/* write enable (~mod30) */
#define	FD_SRB_MOT_1		0x02	/* motor enable drive 1 (ps2) */
#define	FD_SRB_DS3		0x02	/* drive select 3 (mod30) */
#define	FD_SRB_MOT_0		0x01	/* motor enable drive 0 (ps2) */
#define	FD_SRB_DS2		0x01	/* drive select 2 (mod30) */

/*
 * Digital output register (82077AA only)
 */

#define	FD_DOR_MOT_3		0x80	/* motor enable drive 3 */
#define	FD_DOR_MOT_2		0x40
#define	FD_DOR_MOT_1		0x20
#define	FD_DOR_MOT_0		0x10
#define	FD_DOR_DMA_GATE		0x08	/* enable dma (mod30,at) */
#define	FD_DOR_ENABLE		0x04	/* chip reset (inverted) */
#define	FD_DOR_DRIVE_0		0x00	/* select drive no 0 */
#define	FD_DOR_DRIVE_1		0x01
#define	FD_DOR_DRIVE_2		0x02
#define	FD_DOR_DRIVE_3		0x03

/*
 * Tape drive register (82077AA only)
 */

#define	FD_TDR_TAPE_1		0x01	/* unit 1 is a tape */
#define	FD_TDR_TAPE_2		0x02
#define	FD_TDR_TAPE_3		0x03
#define	FD_TDR_xxx		0xfc

/*
 * Data-rate select register (82077AA and 82072)
 */

#define	FD_DSR_RESET		0x80	/* self-clearing reset */
#define	FD_DSR_POWER_DOWN	0x40	/* stop clocks and oscill */
#define	FD_DSR_zero		0x20	/* wired zero on 82077AA */
#define	FD_DSR_EPLL		0x20	/* enable PLL on 82072 */

#define	FD_DSR_PRECOMP_MASK	0x1c	/* precompensation value */
#	define	FD_DSR_PRECOMP_SHIFT	2

#	define	FD_DSR_PRECOMP_DEFAULT	0	/* 41.67@1Mbps else 125ns */
#	define	FD_DSR_PRECOMP_41_67	1
#	define	FD_DSR_PRECOMP_83_34	2
#	define	FD_DSR_PRECOMP_125_00	3
#	define	FD_DSR_PRECOMP_166_67	4
#	define	FD_DSR_PRECOMP_208_33	5
#	define	FD_DSR_PRECOMP_250_00	6
#	define	FD_DSR_PRECOMP_DISABLE	7	/* 0.00ns */

#define	FD_DSR_DATA_RATE_MASK	0x03
#define	FD_DSR_SD_250		0x00	/* fm modulation, 250Kbps bit clock */
#define	FD_DSR_SD_150		0x01
#define	FD_DSR_SD_125		0x02

#define	FD_DSR_DD_500		0x00	/* mfm modulation, 500Kbps */
#define	FD_DSR_DD_300		0x01
#define	FD_DSR_DD_250		0x02
#define	FD_DSR_DD_1000		0x03	/* illegal for 82077 */

/*
 * Main status register (all chips)
 */

#define FD_MSR_RQM		0x80	/* request from master (allowed) */
#define FD_MSR_DIO		0x40	/* data in/out, 1->master read */
#define FD_MSR_NON_DMA		0x20	/* dma disabled */
#define FD_MSR_CMD_BSY		0x10	/* command in progress */
#define FD_MSR_DRV_3_BSY	0x08	/* drive busy seeking */
#define FD_MSR_DRV_2_BSY	0x04
#define FD_MSR_DRV_1_BSY	0x02
#define FD_MSR_DRV_0_BSY	0x01

/*
 * FIFO (82077AA and 82072)
 *
 * Service delay is
 *		Threshold * 8
 *	delay = -------------  -  1.5 usecs
 *		  Data-rate
 */

#define	FD_FIFO_DEEP		16

/*
 * Digital input register (82077AA only)
 */

#define FD_DIR_DSK_CHG		0x80	/* disk was changed (~mod30) */

#define FD_DIR_ones		0x78	/* wired ones for ps2 */
#define FD_DIR_zeroes		0x70	/* wired zeroes for mod30 */
#define FD_DIR_undef		0x7f	/* undefined for at */

#define FD_DIR_DR_MASK_PS2	0x06	/* current data rate (ps2) */
#	define	FD_DIR_DR_SHIFT_PS2	1
#define FD_DIR_LOW_DENS		0x01	/* zero for 500/1M dr (ps2) */

#define FD_DIR_DMA_GATE		0x08	/* same as DOR (mod30) */
#define FD_DIR_NOPREC		0x04	/* same as CCR (mod30) */
#define FD_DIR_DR_MASK_M30	0x03	/* current data rate (mod30) */
#	define	FD_DIR_DR_SHIFT_M30	0

/*
 * Configuration control register (82077AA only)
 */

#define	FD_CCR_DATA_RATE_MASK	0x03	/* see DSR for values */
#define	FD_CCR_NOPREC		0x04	/* "has no function" (mod30) */


/*
 * Programming
 *
 * Legend for command bytes, when applicable
 *
 *	hds	bit 2 of byte 1, head select (1 -> head 1)
 *	ds	bits 0-1 of byte 1, drive select
 *	c	cylinder number (max 76 for 8272A, else 255)
 *	h	head number
 *	r	sector number
 *	n	number of bytes in sector
 *	eot	end-of-track, e.g. final sector number
 *	gpl	gap length
 *	dtl	data length (for partial sectors)
 *	st0-3	status byte
 *	srt	step rate time
 *	hut	head unload time
 *	hlt	head load time
 *	nd	disable DMA
 *	mot	do not turn motor on before checking drive status
 *	pcn	present cylinder number
 *	ncn	new cylinder number
 *	rcn	relative cylinder number (new=present+rcn)
 *	sc	sectors/cylinder
 *	d	filler byte
 *	?	undefined
 *	hsda	high-speed disk adjust (doubles motor on/off delays)
 *	moff	motor off timer, one disk revolution increments
 *	mon	motor on timer, ditto
 *	eis	enable implied seeks
 *	dfifo	disable fifo
 *	poll	disable poll
 *	fifthr	fifo threshold (1 to 16 bytes)
 *	pretrk	precomp starts on this trackno (0-255)
 *	wgate	change timings of WE signal, in perpendicular mode
 *	gap	change gap2 length, in perpendicular mode
 *	ec	in verify, qualify terminating conditions (sc viz eot)
 */

/* First byte of command, qualifiers */
#define	FD_CMD_MT		0x80	/* Multi-track */
#define	FD_CMD_MFM		0x40	/* Double density */
#define	FD_CMD_SK		0x20	/* skip deleted data address mark */
#define	FD_CMD_DIR		0x40	/* relative seek direction (up) */

/* command codes and description */

/*
 * Read an entire track.
 *	Qualifiers:	MFM, SK (8272A only)
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_READ_TRACK		0x02

/*
 * Specify timers
 *	Qualifiers:
 *	Bytes total:	3	code,srt+hut,hlt+nd
 *	Result total:
 */
#define	FD_CMD_SPECIFY			0x03

/*
 * Sense status of drive
 *	Qualifiers:
 *	Bytes total:	2	code,hds+ds +mot(82072 only)
 *	Result total:	1	st3
 */
#define	FD_CMD_SENSE_DRIVE_STATUS	0x04
#	define FD_CMD_SDS_NO_MOT		0x80

/*
 * Write
 *	Qualifiers:	MT, MFM
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_WRITE_DATA		0x05

/*
 * Read
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_READ_DATA		0x06

/*
 * Seek to track 0
 *	Qualifiers:
 *	Bytes total:	2	code,ds
 *	Result total:
 */
#define	FD_CMD_RECALIBRATE		0x07

/*
 * Sense interrupt status
 *	Qualifiers:
 *	Bytes total:	1	code
 *	Result total:	2	st0,pcn
 */
#define	FD_CMD_SENSE_INT_STATUS		0x08

/*
 * Write data and mark deleted
 *	Qualifiers:	MT, MFM
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_WRITE_DELETED_DATA	0x09

/*
 * Read current head position
 *	Qualifiers:	MFM
 *	Bytes total:	2	code,hds+ds
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_READ_ID			0x0a

/*
 * Set value of MOT pin, unconditionally
 *	Qualifiers:	see
 *	Bytes total:	1	code+..
 *	Result total:	none	returns to command phase
 */
#define	FD_CMD_MOTOR_ON_OFF		0x0b	/* 82072 only */

#	define	FD_CMD_MOT_ON			0x80
#	define	FD_CMD_MOT_DS			0x60
#	define	FD_CMD_MOT_DS_SHIFT		5

/*
 * Read data despite deleted address mark
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_READ_DELETED_DATA	0x0c

/*
 * Media initialization
 *	Qualifiers:	MFM
 *	Bytes total:	6	code,hds+ds,n,sc,gpl,d
 *	Data:		4*sc/2	c,h,r,n
 *	Result total:	7	st0,st1,st2,?,?,?,?
 */
#define	FD_CMD_FORMAT_TRACK		0x0d

/*
 * Dump internal register status
 *	Qualifiers:
 *	Bytes total:	1	code
 *	Result total:	10	pcn0,pcn1,pcn2,pcn3,srt+hut,hlt+nd,
 *				sc/eot,hsda+moff+mon,
 *				eis+dfifo+poll+fifothr, pretrk
 *	Notes:			82077AA does not provide for hsda+moff+mon
 */
#define	FD_CMD_DUMPREG			0x0e	/* not 8272a */

/*
 * Move head
 *	Qualifiers:
 *	Bytes total:	3	code,hds+ds,ncn
 *	Result total:
 */
#define	FD_CMD_SEEK			0x0f

/*
 * 
 *	Qualifiers:
 *	Bytes total:	1	code
 *	Result total:	1	version
 */
#define	FD_CMD_VERSION			0x10	/* 82077AA only */
#	define	FD_VERSION_82077AA		0x90

/*
 * Scan disk data
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_SCAN_EQUAL		0x11	/* 8272A only */

/*
 * Specify timers
 *	Qualifiers:
 *	Bytes total:	2	code,wgate+gap
 *	Result total:
 */
#define	FD_CMD_PERPENDICULAR_MODE	0x12	/* 82077AA only */

/*
 * Set configuration parameters
 *	Qualifiers:
 *	Bytes total:	4	code,hsda+moff+mon,eis+dfifo+poll+fifothr,
 *				pretrk
 *	Result total:
 *	Notes:			82077AA does not provide for hsda+moff+mon
 */
#define	FD_CMD_CONFIGURE		0x13	/* not 8272a */

/*
 * Verify CRC of disk data
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,ec+hds+ds,c,h,r,n,eot,gpl,dtl/sc
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_VERIFY			0x16	/* 82077AA only */

/*
 * Scan disk data (disk less-or-equal memory)
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_SCAN_LOW_OR_EQUAL	0x19	/* 8272A only */

/*
 * Scan disk data (disk greater-or-equal memory)
 *	Qualifiers:	MT, MFM, SK
 *	Bytes total:	9	code,hds+ds,c,h,r,n,eot,gpl,dtl
 *	Result total:	7	st0,st1,st2,c,h,r,n
 */
#define	FD_CMD_SCAN_HIGH_OR_EQUAL	0x1d	/* 8272A only */

/*
 * Specify timers
 *	Qualifiers:	DIR
 *	Bytes total:	3	code,hds+ds,rcn
 *	Result total:
 */
#define	FD_CMD_RELATIVE_SEEK		0x8f	/* not 8272a */

/*
 * Any invalid command code
 *	Qualifiers:
 *	Bytes total:	1	code
 *	Result total:	1	st0 (st0 == 0x80)
 */
#define	FD_CMD_INVALID			0xff


/*
 * Results and statii
 *
 * The typical command returns three status bytes,
 * followed by four drive status bytes.
 */

/*
 * Status register 0
 */
#define	FD_ST0_IC_MASK		0xc0	/* interrupt completion code */

#	define	FD_ST0_IC_OK		0x00	/* terminated ok */
#	define	FD_ST0_IC_AT		0x40	/* exec phase ended sour */
#	define	FD_ST0_IC_BAD_CMD	0x80	/* didnt grok */
#	define	FD_ST0_IC_AT_POLL	0xc0	/* polling got in the way */

#define	FD_ST0_SE		0x20	/* (implied) seek ended */
#define	FD_ST0_EC		0x10	/* equipment check */
#define	FD_ST0_NR		0x08	/* not ready (raz for 82077aa) */
#define	FD_ST0_H		0x04	/* currently selected head */
#define	FD_ST0_DS		0x03	/* currently selected drive */

/*
 * Status register 1
 */

#define	FD_ST1_EN		0x80	/* end of cylinder (TC not set?) */
#define	FD_ST1_zero		0x48
#define	FD_ST1_DE		0x20	/* data error, bad CRC */
#define	FD_ST1_OR		0x10	/* overrun/underrun
#define	FD_ST1_ND		0x04	/* no data, sector not found */
#define	FD_ST1_NW		0x02	/* write protect signal */
#define	FD_ST1_MA		0x01	/* missing address mark */

/*
 * Status register 2
 */

#define	FD_ST2_zero		0x80
#define	FD_ST2_CM		0x40	/* control mark, improper read */
#define	FD_ST2_DD		0x20	/* the CRC error was for data  */
#define	FD_ST2_WC		0x10	/* wrong cylinder */
#define	FD_ST2_SH		0x08	/* scan hit (8272a only) */
#define	FD_ST2_SN		0x04	/* scan not met (8272a only) */
#define	FD_ST2_BC		0x02	/* bad cylinder, has 0xff mark */
#define	FD_ST2_MD		0x01	/* missing data mark */

/*
 * Status register 3
 * (sense drive status)
 */

#define	FD_ST3_FT		0x80	/* fault pin (0 if not 8272a) */
#define	FD_ST3_WP		0x40	/* write protect pin */
#define	FD_ST3_RDY		0x20	/* ready pin (1 on 82077aa) */
#define	FD_ST3_T0		0x10	/* track0 pin */
#define	FD_ST3_TS		0x08	/* two-sided pin (1 if not 8272a) */
#define	FD_ST3_HD		0x04	/* hdsel pin */
#define	FD_ST3_DS		0x03	/* drive select pins (1&0) */


#endif	/* _FDC_82077_H_ */
