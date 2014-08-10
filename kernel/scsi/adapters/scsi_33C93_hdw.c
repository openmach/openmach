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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS AS-IS
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
 *	File: scsi_33C93_hdw.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/91
 *
 *	Bottom layer of the SCSI driver: chip-dependent functions
 *
 *	This file contains the code that is specific to the WD/AMD 33C93
 *	SCSI chip (Host Bus Adapter in SCSI parlance): probing, start
 *	operation, and interrupt routine.
 */

#if 0
DISCLAIMER: THIS DOES NOT EVEN COMPILE YET, it went in by mistake.
Code that probably makes some sense is from here to "TILL HERE"

/*
 * This layer works based on small simple 'scripts' that are installed
 * at the start of the command and drive the chip to completion.
 * The idea comes from the specs of the NCR 53C700 'script' processor.
 *
 * There are various reasons for this, mainly
 * - Performance: identify the common (successful) path, and follow it;
 *   at interrupt time no code is needed to find the current status
 * - Code size: it should be easy to compact common operations
 * - Adaptability: the code skeleton should adapt to different chips without
 *   terrible complications.
 * - Error handling: and it is easy to modify the actions performed
 *   by the scripts to cope with strange but well identified sequences
 *
 */

#include <sbic.h>
#if	NSBIC > 0
#include <platforms.h>

#ifdef	IRIS
#define	PAD(n)		char n[3]	/* or whatever */
#define	SBIC_MUX_ADDRESSING		/* comment out if wrong */
#define	SBIC_CLOCK_FREQUENCY	20	/* FIXME FIXME FIXME */
#define	SBIC_MACHINE_DMA_MODE	SBIC_CTL_DMA	/* FIXME FIXME FIXME */

#define	SBIC_SET_RST_ADDR	/*SCSI_INIT_ADDR*/
#define	SBIC_CLR_RST_ADDR	/*SCSI_RDY_ADDR*/
#define	SBIC_MACHINE_RESET_SCSIBUS(regs,per)				\
	{	int temp;						\
		temp = *(volatile unsigned int *)SBIC_SET_RST_ADDR;	\
		delay(per);						\
		temp = *(volatile unsigned int *)SBIC_CLR_RST_ADDR;	\
	}

#endif

#include <machine/machspl.h>		/* spl definitions */
#include <mach/std_types.h>
#include <sys/types.h>
#include <chips/busses.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>

#include <scsi/adapters/scsi_33C93.h>
#include <scsi/scsi_defs.h>
#include <scsi/adapters/scsi_dma.h>

/*
 * Spell out all combinations of padded/nopadded and mux/nomux
 */
#ifdef	PAD
typedef struct {

	volatile unsigned char	sbic_myid;	/* rw: My SCSI id */
/*#define		sbic_cdbsize sbic_myid	/* w : size of CDB */
	PAD(pad0)
	volatile unsigned char	sbic_control;	/* rw: Control register */
	PAD(pad1)
	volatile unsigned char	sbic_timeo;	/* rw: Timeout period */
	PAD(pad2)
	volatile unsigned char	sbic_cdb1;	/* rw: CDB, 1st byte */
	PAD(pad3)
	volatile unsigned char	sbic_cdb2;	/* rw: CDB, 2nd byte */
	PAD(pad4)
	volatile unsigned char	sbic_cdb3;	/* rw: CDB, 3rd byte */
	PAD(pad5)
	volatile unsigned char	sbic_cdb4;	/* rw: CDB, 4th byte */
	PAD(pad6)
	volatile unsigned char	sbic_cdb5;	/* rw: CDB, 5th byte */
	PAD(pad7)
	volatile unsigned char	sbic_cdb6;	/* rw: CDB, 6th byte */
	PAD(pad8)
	volatile unsigned char	sbic_cdb7;	/* rw: CDB, 7th byte */
	PAD(pad9)
	volatile unsigned char	sbic_cdb8;	/* rw: CDB, 8th byte */
	PAD(pad10)
	volatile unsigned char	sbic_cdb9;	/* rw: CDB, 9th byte */
	PAD(pad11)
	volatile unsigned char	sbic_cdb10;	/* rw: CDB, 10th byte */
	PAD(pad12)
	volatile unsigned char	sbic_cdb11;	/* rw: CDB, 11th byte */
	PAD(pad13)
	volatile unsigned char	sbic_cdb12;	/* rw: CDB, 12th byte */
	PAD(pad14)
	volatile unsigned char	sbic_tlun;	/* rw: Target LUN */
	PAD(pad15)
	volatile unsigned char	sbic_cmd_phase;	/* rw: Command phase */
	PAD(pad16)
	volatile unsigned char	sbic_syn;	/* rw: Synch xfer params */
	PAD(pad17)
	volatile unsigned char	sbic_count_hi;	/* rw: Xfer count, hi */
	PAD(pad18)
	volatile unsigned char	sbic_count_med;	/* rw: Xfer count, med */
	PAD(pad19)
	volatile unsigned char	sbic_count_lo;	/* rw: Xfer count, lo */
	PAD(pad20)
	volatile unsigned char	sbic_selid;	/* rw: Target ID (select) */
	PAD(pad21)
	volatile unsigned char	sbic_rselid;	/* rw: Target ID (reselect) */
	PAD(pad22)
	volatile unsigned char	sbic_csr;	/* r : Status register */
	PAD(pad23)
	volatile unsigned char	sbic_cmd;	/* rw: Command register */
	PAD(pad24)
	volatile unsigned char	sbic_data;	/* rw: FIFO top */
	PAD(pad25)
	char u0;				/* unused, padding */
	PAD(pad26)
	char u1;				/* unused, padding */
	PAD(pad27)
	char u2;				/* unused, padding */
	PAD(pad28)
	char u3;				/* unused, padding */
	PAD(pad29)
	char u4;				/* unused, padding */
	PAD(pad30)
	volatile unsigned char	sbic_asr;	/* r : Aux Status Register */
	PAD(pad31)

} sbic_padded_mux_regmap_t;

typedef struct {
	volatile unsigned char	sbic_asr;	/* r : Aux Status Register */
/*#define		sbic_address sbic_asr	/* w : desired register no */
	PAD(pad0);
	volatile unsigned char	sbic_value;	/* rw: register value */
	PAD(pad1);
} sbic_padded_ind_regmap_t;

#else	/* !PAD */

typedef sbic_mux_regmap_t	sbic_padded_mux_regmap_t;
typedef sbic_ind_regmap_t	sbic_padded_ind_regmap_t;

#endif	/* !PAD */

/*
 * Could have used some non-ANSIsm in the following :-))
 */
#ifdef	SBIC_MUX_ADDRESSING

typedef sbic_padded_mux_regmap_t	sbic_padded_regmap_t;

#define	SET_SBIC_myid(regs,val)		(regs)->sbic_myid = (val)
#define	GET_SBIC_myid(regs,val)		(val) = (regs)->sbic_myid
#define	SET_SBIC_cdbsize(regs,val)	(regs)->sbic_cdbsize = (val)
#define	GET_SBIC_cdbsize(regs,val)	(val) = (regs)->sbic_cdbsize
#define	SET_SBIC_control(regs,val)	(regs)->sbic_control = (val)
#define	GET_SBIC_control(regs,val)	(val) = (regs)->sbic_control
#define	SET_SBIC_timeo(regs,val)	(regs)->sbic_timeo = (val)
#define	GET_SBIC_timeo(regs,val)	(val) = (regs)->sbic_timeo
#define	SET_SBIC_cdb1(regs,val)		(regs)->sbic_cdb1 = (val)
#define	GET_SBIC_cdb1(regs,val)		(val) = (regs)->sbic_cdb1
#define	SET_SBIC_cdb2(regs,val)		(regs)->sbic_cdb2 = (val)
#define	GET_SBIC_cdb2(regs,val)		(val) = (regs)->sbic_cdb2
#define	SET_SBIC_cdb3(regs,val)		(regs)->sbic_cdb3 = (val)
#define	GET_SBIC_cdb3(regs,val)		(val) = (regs)->sbic_cdb3
#define	SET_SBIC_cdb4(regs,val)		(regs)->sbic_cdb4 = (val)
#define	GET_SBIC_cdb4(regs,val)		(val) = (regs)->sbic_cdb4
#define	SET_SBIC_cdb5(regs,val)		(regs)->sbic_cdb5 = (val)
#define	GET_SBIC_cdb5(regs,val)		(val) = (regs)->sbic_cdb5
#define	SET_SBIC_cdb6(regs,val)		(regs)->sbic_cdb6 = (val)
#define	GET_SBIC_cdb6(regs,val)		(val) = (regs)->sbic_cdb6
#define	SET_SBIC_cdb7(regs,val)		(regs)->sbic_cdb7 = (val)
#define	GET_SBIC_cdb7(regs,val)		(val) = (regs)->sbic_cdb7
#define	SET_SBIC_cdb8(regs,val)		(regs)->sbic_cdb8 = (val)
#define	GET_SBIC_cdb8(regs,val)		(val) = (regs)->sbic_cdb8
#define	SET_SBIC_cdb9(regs,val)		(regs)->sbic_cdb9 = (val)
#define	GET_SBIC_cdb9(regs,val)		(val) = (regs)->sbic_cdb9
#define	SET_SBIC_cdb10(regs,val)	(regs)->sbic_cdb10 = (val)
#define	GET_SBIC_cdb10(regs,val)	(val) = (regs)->sbic_cdb10
#define	SET_SBIC_cdb11(regs,val)	(regs)->sbic_cdb11 = (val)
#define	GET_SBIC_cdb11(regs,val)	(val) = (regs)->sbic_cdb11
#define	SET_SBIC_cdb12(regs,val)	(regs)->sbic_cdb12 = (val)
#define	GET_SBIC_cdb12(regs,val)	(val) = (regs)->sbic_cdb12
#define	SET_SBIC_tlun(regs,val)		(regs)->sbic_tlun = (val)
#define	GET_SBIC_tlun(regs,val)		(val) = (regs)->sbic_tlun
#define	SET_SBIC_cmd_phase(regs,val)	(regs)->sbic_cmd_phase = (val)
#define	GET_SBIC_cmd_phase(regs,val)	(val) = (regs)->sbic_cmd_phase
#define	SET_SBIC_syn(regs,val)		(regs)->sbic_syn = (val)
#define	GET_SBIC_syn(regs,val)		(val) = (regs)->sbic_syn
#define	SET_SBIC_count_hi(regs,val)	(regs)->sbic_count_hi = (val)
#define	GET_SBIC_count_hi(regs,val)	(val) = (regs)->sbic_count_hi
#define	SET_SBIC_count_med(regs,val)	(regs)->sbic_count_med = (val)
#define	GET_SBIC_count_med(regs,val)	(val) = (regs)->sbic_count_med
#define	SET_SBIC_count_lo(regs,val)	(regs)->sbic_count_lo = (val)
#define	GET_SBIC_count_lo(regs,val)	(val) = (regs)->sbic_count_lo
#define	SET_SBIC_selid(regs,val)	(regs)->sbic_selid = (val)
#define	GET_SBIC_selid(regs,val)	(val) = (regs)->sbic_selid
#define	SET_SBIC_rselid(regs,val)	(regs)->sbic_rselid = (val)
#define	GET_SBIC_rselid(regs,val)	(val) = (regs)->sbic_rselid
#define	SET_SBIC_csr(regs,val)		(regs)->sbic_csr = (val)
#define	GET_SBIC_csr(regs,val)		(val) = (regs)->sbic_csr
#define	SET_SBIC_cmd(regs,val)		(regs)->sbic_cmd = (val)
#define	GET_SBIC_cmd(regs,val)		(val) = (regs)->sbic_cmd
#define	SET_SBIC_data(regs,val)		(regs)->sbic_data = (val)
#define	GET_SBIC_data(regs,val)		(val) = (regs)->sbic_data

#define	SBIC_TC_SET(regs,val)	{				\
		(regs)->sbic_count_hi = ((val)>>16));		\
		(regs)->sbic_count_med = (val)>>8;		\
		(regs)->sbic_count_lo = (val);			\
	}
#define	SBIC_TC_GET(regs,val)	{				\
		(val) = ((regs)->sbic_count_hi << 16) |		\
			((regs)->sbic_count_med << 8) |		\
			((regs)->sbic_count_lo);		\
	}

#define	SBIC_LOAD_COMMAND(regs,cmd,cmdsize)	{		\
		register char *ptr = (char*)(cmd);		\
		(regs)->cis_cdb1 = *ptr++;			\
		(regs)->cis_cdb2 = *ptr++;			\
		(regs)->cis_cdb3 = *ptr++;			\
		(regs)->cis_cdb4 = *ptr++;			\
		(regs)->cis_cdb5 = *ptr++;			\
		(regs)->cis_cdb6 = *ptr++;			\
		if (cmdsize > 6) {				\
			(regs)->cis_cdb7 = *ptr++;		\
			(regs)->cis_cdb8 = *ptr++;		\
			(regs)->cis_cdb9 = *ptr++;		\
			(regs)->cis_cdb10 = *ptr++;		\
		}						\
		if (cmdsize > 10) {				\
			(regs)->cis_cdb11 = *ptr++;		\
			(regs)->cis_cdb12 = *ptr;		\
		}						\
	}

#else	/*SBIC_MUX_ADDRESSING*/

typedef sbic_padded_ind_regmap_t	sbic_padded_regmap_t;

#define	SET_SBIC_myid(regs,val)		sbic_write_reg(regs,SBIC_myid,val)
#define	GET_SBIC_myid(regs,val)		sbic_read_reg(regs,SBIC_myid,val)
#define	SET_SBIC_cdbsize(regs,val)	sbic_write_reg(regs,SBIC_cdbsize,val)
#define	GET_SBIC_cdbsize(regs,val)	sbic_read_reg(regs,SBIC_cdbsize,val)
#define	SET_SBIC_control(regs,val)	sbic_write_reg(regs,SBIC_control,val)
#define	GET_SBIC_control(regs,val)	sbic_read_reg(regs,SBIC_control,val)
#define	SET_SBIC_timeo(regs,val)	sbic_write_reg(regs,SBIC_timeo,val)
#define	GET_SBIC_timeo(regs,val)	sbic_read_reg(regs,SBIC_timeo,val)
#define	SET_SBIC_cdb1(regs,val)		sbic_write_reg(regs,SBIC_cdb1,val)
#define	GET_SBIC_cdb1(regs,val)		sbic_read_reg(regs,SBIC_cdb1,val)
#define	SET_SBIC_cdb2(regs,val)		sbic_write_reg(regs,SBIC_cdb2,val)
#define	GET_SBIC_cdb2(regs,val)		sbic_read_reg(regs,SBIC_cdb2,val)
#define	SET_SBIC_cdb3(regs,val)		sbic_write_reg(regs,SBIC_cdb3,val)
#define	GET_SBIC_cdb3(regs,val)		sbic_read_reg(regs,SBIC_cdb3,val)
#define	SET_SBIC_cdb4(regs,val)		sbic_write_reg(regs,SBIC_cdb4,val)
#define	GET_SBIC_cdb4(regs,val)		sbic_read_reg(regs,SBIC_cdb4,val)
#define	SET_SBIC_cdb5(regs,val)		sbic_write_reg(regs,SBIC_cdb5,val)
#define	GET_SBIC_cdb5(regs,val)		sbic_read_reg(regs,SBIC_cdb5,val)
#define	SET_SBIC_cdb6(regs,val)		sbic_write_reg(regs,SBIC_cdb6,val)
#define	GET_SBIC_cdb6(regs,val)		sbic_read_reg(regs,SBIC_cdb6,val)
#define	SET_SBIC_cdb7(regs,val)		sbic_write_reg(regs,SBIC_cdb7,val)
#define	GET_SBIC_cdb7(regs,val)		sbic_read_reg(regs,SBIC_cdb7,val)
#define	SET_SBIC_cdb8(regs,val)		sbic_write_reg(regs,SBIC_cdb8,val)
#define	GET_SBIC_cdb8(regs,val)		sbic_read_reg(regs,SBIC_cdb8,val)
#define	SET_SBIC_cdb9(regs,val)		sbic_write_reg(regs,SBIC_cdb9,val)
#define	GET_SBIC_cdb9(regs,val)		sbic_read_reg(regs,SBIC_cdb9,val)
#define	SET_SBIC_cdb10(regs,val)	sbic_write_reg(regs,SBIC_cdb10,val)
#define	GET_SBIC_cdb10(regs,val)	sbic_read_reg(regs,SBIC_cdb10,val)
#define	SET_SBIC_cdb11(regs,val)	sbic_write_reg(regs,SBIC_cdb11,val)
#define	GET_SBIC_cdb11(regs,val)	sbic_read_reg(regs,SBIC_cdb11,val)
#define	SET_SBIC_cdb12(regs,val)	sbic_write_reg(regs,SBIC_cdb12,val)
#define	GET_SBIC_cdb12(regs,val)	sbic_read_reg(regs,SBIC_cdb12,val)
#define	SET_SBIC_tlun(regs,val)		sbic_write_reg(regs,SBIC_tlun,val)
#define	GET_SBIC_tlun(regs,val)		sbic_read_reg(regs,SBIC_tlun,val)
#define	SET_SBIC_cmd_phase(regs,val)	sbic_write_reg(regs,SBIC_cmd_phase,val)
#define	GET_SBIC_cmd_phase(regs,val)	sbic_read_reg(regs,SBIC_cmd_phase,val)
#define	SET_SBIC_syn(regs,val)		sbic_write_reg(regs,SBIC_syn,val)
#define	GET_SBIC_syn(regs,val)		sbic_read_reg(regs,SBIC_syn,val)
#define	SET_SBIC_count_hi(regs,val)	sbic_write_reg(regs,SBIC_count_hi,val)
#define	GET_SBIC_count_hi(regs,val)	sbic_read_reg(regs,SBIC_count_hi,val)
#define	SET_SBIC_count_med(regs,val)	sbic_write_reg(regs,SBIC_count_med,val)
#define	GET_SBIC_count_med(regs,val)	sbic_read_reg(regs,SBIC_count_med,val)
#define	SET_SBIC_count_lo(regs,val)	sbic_write_reg(regs,SBIC_count_lo,val)
#define	GET_SBIC_count_lo(regs,val)	sbic_read_reg(regs,SBIC_count_lo,val)
#define	SET_SBIC_selid(regs,val)	sbic_write_reg(regs,SBIC_selid,val)
#define	GET_SBIC_selid(regs,val)	sbic_read_reg(regs,SBIC_selid,val)
#define	SET_SBIC_rselid(regs,val)	sbic_write_reg(regs,SBIC_rselid,val)
#define	GET_SBIC_rselid(regs,val)	sbic_read_reg(regs,SBIC_rselid,val)
#define	SET_SBIC_csr(regs,val)		sbic_write_reg(regs,SBIC_csr,val)
#define	GET_SBIC_csr(regs,val)		sbic_read_reg(regs,SBIC_csr,val)
#define	SET_SBIC_cmd(regs,val)		sbic_write_reg(regs,SBIC_cmd,val)
#define	GET_SBIC_cmd(regs,val)		sbic_read_reg(regs,SBIC_cmd,val)
#define	SET_SBIC_data(regs,val)		sbic_write_reg(regs,SBIC_data,val)
#define	GET_SBIC_data(regs,val)		sbic_read_reg(regs,SBIC_data,val)

#define	SBIC_TC_SET(regs,val)	{				\
		sbic_write_reg(regs,SBIC_count_hi,((val)>>16));	\
		(regs)->sbic_value = (val)>>8;	wbflush();	\
		(regs)->sbic_value = (val);			\
	}
#define	SBIC_TC_GET(regs,val)	{				\
		sbic_read_reg(regs,SBIC_count_hi,(val));	\
		(val) = ((val)<<8) | (regs)->sbic_value;	\
		(val) = ((val)<<8) | (regs)->sbic_value;	\
	}

#define	SBIC_LOAD_COMMAND(regs,cmd,cmdsize)	{
		register int n=cmdsize-1;			\
		register char *ptr = (char*)(cmd);		\
		sbic_write_reg(regs,SBIC_cdb1,*ptr++);		\
		while (n-- > 0) (regs)->sbic_value = *ptr++;	\
	}

#endif	/*SBIC_MUX_ADDRESSING*/

#define	GET_SBIC_asr(regs,val)		(val) = (regs)->sbic_asr


/*
 * If all goes well (cross fingers) the typical read/write operation
 * should complete in just one interrupt.  Therefore our scripts
 * have only two parts: a pre-condition and an action. The first
 * triggers error handling if not satisfied and in our case it is a match
 * of ....
 * The action part is just a function pointer, invoked in a standard way.
 * The script proceeds only if the action routine returns TRUE.
 * See sbic_intr() for how and where this is all done.
 */

typedef struct script {
	struct {			/* expected state at interrupt: */
		unsigned char	csr;	/* interrupt cause */
		unsigned char	pha;	/* command phase */
	} condition;
/*	unsigned char	unused[2];	/* unused padding */
	boolean_t	(*action)();	/* extra operations */
} *script_t;

/* Matching on the condition value */
#define	ANY				0xff
#define	SCRIPT_MATCH(csr,pha,cond)	\
		(((cond).csr == (csr)) && \
		 (((cond).pha == (pha)) || ((cond).pha==ANY)))


/* forward decls of script actions */
boolean_t
	sbic_end(),			/* all come to an end */
	sbic_get_status(),		/* get status from target */
	sbic_dma_in(),			/* get data from target via dma */
	sbic_dma_in_r(),			/* get data from target via dma (restartable)*/
	sbic_dma_out(),			/* send data to target via dma */
	sbic_dma_out_r(),		/* send data to target via dma (restartable) */
	sbic_dosynch(),			/* negotiate synch xfer */
	sbic_msg_in(),			/* receive the disconenct message */
	sbic_disconnected(),		/* target has disconnected */
	sbic_reconnect();		/* target reconnected */

/* forward decls of error handlers */
boolean_t
	sbic_err_generic(),		/* generic handler */
	sbic_err_disconn(),		/* target disconnects amidst */
	gimmeabreak();			/* drop into the debugger */

int	sbic_reset_scsibus();
boolean_t sbic_probe_target();
static	sbic_wait();

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) SCSI-33c93 interface
 */
struct sbic_softc {
	watchdog_t	wd;
	sbic_padded_regmap_t	*regs;	/* 33c93 registers */

	scsi_dma_ops_t	*dma_ops;	/* DMA operations and state */
	opaque_t	dma_state;

	script_t	script;		/* what should happen next */
	boolean_t	(*error_handler)();/* what if something is wrong */
	int		in_count;	/* amnt we expect to receive */
	int		out_count;	/* amnt we are going to ship */

	volatile char	state;
#define	SBIC_STATE_BUSY		0x01	/* selecting or currently connected */
#define SBIC_STATE_TARGET	0x04	/* currently selected as target */
#define SBIC_STATE_COLLISION	0x08	/* lost selection attempt */
#define SBIC_STATE_DMA_IN	0x10	/* tgt --> initiator xfer */
#define	SBIC_STATE_AM_MODE	0x20	/* 33c93A with advanced mode (AM) */

	unsigned char	ntargets;	/* how many alive on this scsibus */
	unsigned char	done;
	unsigned char	unused;

	scsi_softc_t	*sc;		/* HBA-indep info */
	target_info_t	*active_target;	/* the current one */

	target_info_t	*next_target;	/* trying to seize bus */
	queue_head_t	waiting_targets;/* other targets competing for bus */

} sbic_softc_data[NSBIC];

typedef struct sbic_softc *sbic_softc_t;

sbic_softc_t	sbic_softc[NSBIC];

/*
 * Synch xfer parameters, and timing conversions
 */
int	sbic_min_period = SBIC_SYN_MIN_PERIOD;	/* in cycles = f(ICLK,FSn) */
int	sbic_max_offset = SBIC_SYN_MAX_OFFSET;	/* pure number */

int sbic_to_scsi_period(regs,a)
{
	unsigned int fs;

	/* cycle = DIV / (2*CLK) */
	/* DIV = FS+2 */
	/* best we can do is 200ns at 20Mhz, 2 cycles */

	GET_SBIC_myid(regs,fs);
	fs = (fs >>6) + 2;				/* DIV */
	fs = (fs * 1000) / (SBIC_CLOCK_FREQUENCY<<1);	/* Cycle, in ns */
	if (a < 2) a = 8;				/* map to Cycles */
	return ((fs*a)>>2);				/* in 4 ns units */
}

int scsi_period_to_sbic(regs,p)
{
	register unsigned int fs;
	
	/* Just the inverse of the above */

	GET_SBIC_myid(regs,fs);
	fs = (fs >>6) + 2;				/* DIV */
	fs = (fs * 1000) / (SBIC_CLOCK_FREQUENCY<<1);	/* Cycle, in ns */

	ret = p << 2;					/* in ns units */
	ret = ret / fs;					/* in Cycles */
	if (ret < sbic_min_period)
		return sbic_min_period;
	/* verify rounding */
	if (sbic_to_scsi_period(regs,ret) < p)
		ret++;
	return (ret >= 8) ? 0 : ret;
}

#define	u_min(a,b)	(((a) < (b)) ? (a) : (b))

/*
 * Definition of the controller for the auto-configuration program.
 */

int	sbic_probe(), scsi_slave(), scsi_attach(), sbic_go(), sbic_intr();

caddr_t	sbic_std[NSBIC] = { 0 };
struct	bus_device *sbic_dinfo[NSBIC*8];
struct	bus_ctlr *sbic_minfo[NSBIC];
struct	bus_driver sbic_driver = 
        { sbic_probe, scsi_slave, scsi_attach, sbic_go, sbic_std, "rz", sbic_dinfo,
	  "sbic", sbic_minfo, BUS_INTR_B4_PROBE};


sbic_set_dmaops(unit, dmaops)
	unsigned int	unit;
	scsi_dma_ops_t	*dmaops;
{
	if (unit < NSBIC)
		sbic_std[unit] = (caddr_t)dmaops;
}

/*
 * Scripts
 */
struct script
sbic_script_any_cmd[] = {	/* started with SEL & XFER */
	{{SBIC_CSR_S_XFERRED, 0x60}, sbic_get_status},
},

sbic_script_try_synch[] = {	/* started with SEL */
	{{SBIC_CSR_INITIATOR, ANY}, sbic_dosynch},
	{{SBIC_CSR_S_XFERRED, 0x60}, sbic_get_status},
};


#define DEBUG
#ifdef	DEBUG

#define	PRINT(x)	if (scsi_debug) printf x

sbic_state(regs, overrule)
	sbic_padded_regmap_t	*regs;
{
	register unsigned char asr,tmp;

	if (regs == 0) {
		if (sbic_softc[0])
			regs = sbic_softc[0]->regs;
		else
			regs = (sbic_padded_regmap_t*)0xXXXXXXXX;
	}

	GET_SBIC_asr(regs,asr);

	if ((asr & SBIC_ASR_BSY) && !overrule)
		db_printf("-BUSY- ");
	else {
		unsigned char	tlun,pha,selid,rselid;
		unsigned int	cnt;
		GET_SBIC_tlun(regs,tlun);
		GET_SBIC_cmd_phase(regs,pha);
		GET_SBIC_selid(regs,selid);
		GET_SBIC_rselid(regs,rselid);
		SBIC_TC_GET(regs,cnt);
		db_printf("tc %x tlun %x sel %x rsel %x pha %x ",
			  cnt, tlun, selid, rselid, pha);
	}

	if (asr & SBIC_ASR_INT)
		db_printf("-INT- ");
	else {
		GET_SBIC_csr(regs,tmp);
		db_printf("csr %x ", tmp);
	}

	if (asr & SBIC_ASR_CIP)
		db_printf("-CIP-\n");
	else {
		GET_SBIC_cmd(regs,tmp);
		db_printf("cmd %x\n", tmp);
	}
	return 0;
}

sbic_target_state(tgt)
	target_info_t	*tgt;
{
	if (tgt == 0)
		tgt = sbic_softc[0]->active_target;
	if (tgt == 0)
		return 0;
	db_printf("@x%x: fl %x dma %X+%x cmd %x@%X id %x per %x off %x ior %X ret %X\n",
		tgt,
		tgt->flags, tgt->dma_ptr, tgt->transient_state.dma_offset, tgt->cur_cmd,
		tgt->cmd_ptr, tgt->target_id, tgt->sync_period, tgt->sync_offset,
		tgt->ior, tgt->done);
	if (tgt->flags & TGT_DISCONNECTED){
		script_t	spt;

		spt = tgt->transient_state.script;
		db_printf("disconnected at ");
		db_printsym(spt,1);
		db_printf(": %x %x ", spt->condition.csr, spt->condition.pha);
		db_printsym(spt->action,1);
		db_printf(", ");
		db_printsym(tgt->transient_state.handler, 1);
		db_printf("\n");
	}

	return 0;
}

sbic_all_targets(unit)
{
	int i;
	target_info_t	*tgt;
	for (i = 0; i < 8; i++) {
		tgt = sbic_softc[unit]->sc->target[i];
		if (tgt)
			sbic_target_state(tgt);
	}
}

sbic_script_state(unit)
{
	script_t	spt = sbic_softc[unit]->script;

	if (spt == 0) return 0;
	db_printsym(spt,1);
	db_printf(": %x %x ", spt->condition.csr, spt->condition.pha);
	db_printsym(spt->action,1);
	db_printf(", ");
	db_printsym(sbic_softc[unit]->error_handler, 1);
	return 0;
}

#define TRMAX 200
int tr[TRMAX+3];
int trpt, trpthi;
#define	TR(x)	tr[trpt++] = x
#define TRWRAP	trpthi = trpt; trpt = 0;
#define TRCHECK	if (trpt > TRMAX) {TRWRAP}

#define TRACE

#ifdef TRACE

#define LOGSIZE 256
int sbic_logpt;
char sbic_log[LOGSIZE];

#define MAXLOG_VALUE	0x1e
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static LOG(e,f)
	char *f;
{
	sbic_log[sbic_logpt++] = (e);
	if (sbic_logpt == LOGSIZE) sbic_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

sbic_print_log(skip)
	int skip;
{
	register int i, j;
	register unsigned char c;

	for (i = 0, j = sbic_logpt; i < LOGSIZE; i++) {
		c = sbic_log[j];
		if (++j == LOGSIZE) j = 0;
		if (skip-- > 0)
			continue;
		if (c < MAXLOG_VALUE)
			db_printf(" %s", logtbl[c].name);
		else
			db_printf("-x%x", c & 0x7f);
	}
}

sbic_print_stat()
{
	register int i;
	register char *p;
	for (i = 0; i < MAXLOG_VALUE; i++) {
		if (p = logtbl[i].name)
			printf("%d %s\n", logtbl[i].count, p);
	}
}

#else	/*TRACE*/
#define	LOG(e,f)
#define LOGSIZE
#endif	/*TRACE*/

#else	/*DEBUG*/
#define PRINT(x)
#define	LOG(e,f)
#define LOGSIZE

#endif	/*DEBUG*/


/*
 *	Probe/Slave/Attach functions
 */

/*
 * Probe routine:
 *	Should find out (a) if the controller is
 *	present and (b) which/where slaves are present.
 *
 * Implementation:
 *	Send a test-unit-ready to each possible target on the bus
 *	except of course ourselves.
 */
sbic_probe(reg, ui)
	unsigned	reg;
	struct bus_ctlr	*ui;
{
	int             unit = ui->unit;
	sbic_softc_t    sbic = &sbic_softc_data[unit];
	int		target_id;
	scsi_softc_t	*sc;
	register sbic_padded_regmap_t	*regs;
	spl_t		s;
	boolean_t	did_banner = FALSE;

	/*
	 * We are only called if the right board is there,
	 * but make sure anyways..
	 */
	if (check_memory(reg, 0))
		return 0;

#if	MAPPABLE
	/* Mappable version side */
	SBIC_probe(reg, ui);
#endif	/*MAPPABLE*/

	/*
	 * Initialize hw descriptor, cache some pointers
	 */
	sbic_softc[unit] = sbic;
	sbic->regs = (sbic_padded_regmap_t *) (reg);

	if ((sbic->dma_ops = (scsi_dma_ops_t *)sbic_std[unit]) == 0)
		/* use same as unit 0 if undefined */
		sbic->dma_ops = (scsi_dma_ops_t *)sbic_std[0];

	sbic->dma_state = (*sbic->dma_ops->init)(unit, reg);

	queue_init(&sbic->waiting_targets);

	sc = scsi_master_alloc(unit, sbic);
	sbic->sc = sc;

	sc->go = sbic_go;
	sc->watchdog = scsi_watchdog;
	sc->probe = sbic_probe_target;
	sbic->wd.reset = sbic_reset_scsibus;

#ifdef	MACH_KERNEL
	sc->max_dma_data = -1;
#else
	sc->max_dma_data = scsi_per_target_virtual;
#endif

	regs = sbic->regs;

	/*
	 * Reset chip, fully.  Note that interrupts are already enabled.
	 */
	s = splbio();
	if (sbic_reset(regs, TRUE))
		sbic->state |= SBIC_STATE_AM_MODE;

	/*
	 * Our SCSI id on the bus.
	 * The user can probably set this via the prom.
	 * If not, it is easy to fix: make a default that
	 * can be changed as boot arg.  Otherwise we keep
	 * what the prom used.
	 */
#ifdef	unneeded
	SET_SBIC_myid(regs, (scsi_initiator_id[unit] & 0x7));
	sbic_reset(regs, TRUE);
#endif
	GET_SBIC_myid(regs,sc->initiator_id);
	sc->initiator_id &= 0x7;
	printf("%s%d: SCSI id %d", ui->name, unit, sc->initiator_id);

	/*
	 * For all possible targets, see if there is one and allocate
	 * a descriptor for it if it is there.
	 */
	for (target_id = 0; target_id < 8; target_id++) {
		register unsigned char	asr, csr, pha;
		register scsi_status_byte_t	status;

		/* except of course ourselves */
		if (target_id == sc->initiator_id)
			continue;

		SBIC_TC_SET(regs,0);
		SET_SBIC_selid(regs,target_id);
		SET_SBIC_timo(regs,SBIC_TIMEOUT(250,SBIC_CLOCK_FREQUENCY));

		/*
		 * See if the unit is ready.
		 * XXX SHOULD inquiry LUN 0 instead !!!
		 */
		{
			scsi_command_test_unit_ready_y	c;
			bzero(&c, sizeof(c));
			c.scsi_cmd_code = SCSI_CMD_TEST_UNIT_READY;
			SBIC_LOAD_COMMAND(regs,&c,sizeof(c));
		}

		/* select and send it */
		SET_SBIC_cmd(regs,SBIC_CMD_SEL_XFER);

		/* wait for the chip to complete, or timeout */
		asr = sbic_wait(regs, SBIC_ASR_INT);
		GET_SBIC_csr(regs,csr);

		/*
		 * Check if the select timed out
		 */
		GET_SBIC_cmd_phase(regs,pha);
		if ((SBIC_CPH(pha) == 0) && (csr & SBIC_CSR_CMD_ERR)) {
			/* noone out there */
#if notsure
			SET_SBIC_cmd(regs,SBIC_CMD_DISC);
			asr = sbic_wait(regs, SBIC_ASR_INT);
			GET_SBIC_csr(regs,csr);
#endif
			continue;
		}

		printf(",%s%d", did_banner++ ? " " : " target(s) at ",
				target_id);

		if (SBIC_CPH(pha) < 0x60)
			/* XXX recover by hand XXX */
			panic(" target acts weirdo");

		GET_SBIC_tlun(regs,status.bits);

		if (status.st.scsi_status_code != SCSI_ST_GOOD)
			scsi_error( 0, SCSI_ERR_STATUS, status.bits, 0);

		/*
		 * Found a target
		 */
		sbic->ntargets++;
		{
			register target_info_t	*tgt;
			tgt = scsi_slave_alloc(sc->masterno, target_id, sbic);

			(*sbic->dma_ops->new_target)(sbic->dma_state, tgt);
		}
	}

	printf(".\n");

	splx(s);
	return 1;
}

boolean_t
sbic_probe_target(tgt, ior)
	target_info_t		*tgt;
	io_req_t		ior;
{
	sbic_softc_t     sbic = sbic_softc[tgt->masterno];
	boolean_t	newlywed;

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		(*sbic->dma_ops->new_target)(sbic->dma_state, tgt);
	}

	if (scsi_inquiry(tgt, SCSI_INQ_STD_DATA) == SCSI_RET_DEVICE_DOWN)
		return FALSE;

	tgt->flags = TGT_ALIVE;
	return TRUE;
}

static sbic_wait(regs, until)
	sbic_padded_regmap_t	*regs;
	char			until;
{
	register unsigned char	val;
	int			timeo = 1000000;

	GET_SBIC_asr(regs,val);
	while ((val & until) == 0) {
		if (!timeo--) {
			printf("sbic_wait TIMEO with x%x\n", regs->sbic_csr);
			break;
		}
		delay(1);
		GET_SBIC_asr(regs,val);
	}
	return val;
}

boolean_t
sbic_reset(regs, quick)
	sbic_padded_regmap_t	*regs;
{
	char	my_id, csr;

	/* preserve our ID for now */
	GET_SBIC_myid(regs,my_id);
	my_id &= SBIC_ID_MASK;

	if (SBIC_CLOCK_FREQUENCY < 11)
		my_id |= SBIC_ID_FS_8_10;
	else if (SBIC_CLOCK_FREQUENCY < 16)
		my_id |= SBIC_ID_FS_12_15;
	else if (SBIC_CLOCK_FREQUENCY < 21)
		my_id |= SBIC_ID_FS_16_20;

	my_id |= SBIC_ID_EAF|SBIC_ID_EHP;

	SET_SBIC_myid(regs,myid);
	wbflush();

	/*
	 * Reset chip and wait till done
	 */
	SET_SBIC_cmd(regs,SBIC_CMD_RESET);
	delay(25);

	(void) sbic_wait(regs, SBIC_ASR_INT);
	GET_SBIC_csr(regs,csr);	/* clears interrupt also */

	/*
	 * Set up various chip parameters
	 */
	SET_SBIC_control(regs,	SBIC_CTL_HHP|SBIC_CTL_EDI|SBIC_CTL_HSP|
				SBIC_MACHINE_DMA_MODE);
				/* will do IDI on the fly */
	SET_SBIC_rselid(regs,	SBIC_RID_ER|SBIC_RID_ES|SBIC_RID_DSP);
	SET_SBIC_syn(regs,SBIC_SYN(0,sbic_min_period));	/* asynch for now */

	/* anything else was zeroed by reset */

	if (quick)
		return (csr & SBIC_CSR_RESET_AM);

	/*
	 * reset the scsi bus, the interrupt routine does the rest
	 * or you can call sbic_bus_reset().
	 */
	/*
	 * Now HOW do I do this ?  I just want to drive the SCSI "RST"
	 * signal true for about 25 usecs;  But the chip has no notion
	 * of such a signal at all.  The spec suggest that the chip's
	 * reset pin be connected to the RST signal, which makes this
	 * operation a machdep one.
	 */
	SBIC_MACHINE_RESET_SCSIBUS(regs, 30);

	return (csr & SBIC_CSR_RESET_AM);
}

/*
 *	Operational functions
 */

/*
 * Start a SCSI command on a target
 */
sbic_go(tgt, cmd_count, in_count, cmd_only)
	target_info_t		*tgt;
	boolean_t		cmd_only;
{
	sbic_softc_t	sbic;
	register spl_t	s;
	boolean_t	disconn;
	script_t	scp;
	boolean_t	(*handler)();

	LOG(1,"go");

	sbic = (sbic_softc_t)tgt->hw_state;

	tgt->transient_state.cmd_count = cmd_count; /* keep it here */

	(*sbic->dma_ops->map)(sbic->dma_state, tgt);

	disconn  = BGET(scsi_might_disconnect,tgt->masterno,tgt->target_id);
	disconn  = disconn && (sbic->ntargets > 1);
	disconn |= BGET(scsi_should_disconnect,tgt->masterno,tgt->target_id);

	/*
	 * Setup target state
	 */
	tgt->done = SCSI_RET_IN_PROGRESS;

	handler = (disconn) ? sbic_err_disconn : sbic_err_generic;
	scp = sbic_script_any_cmd;

	switch (tgt->cur_cmd) {
	    case SCSI_CMD_READ:
	    case SCSI_CMD_LONG_READ:
		LOG(2,"readop");
		break;
	    case SCSI_CMD_WRITE:
	    case SCSI_CMD_LONG_WRITE:
		LOG(0x1a,"writeop");
		break;
	    case SCSI_CMD_INQUIRY:
		/* This is likely the first thing out:
		   do the synch neg if so */
		if (!cmd_only && ((tgt->flags&TGT_DID_SYNCH)==0)) {
			scp = sbic_script_try_synch;
 			tgt->flags |= TGT_TRY_SYNCH;
			break;
		}
	    case SCSI_CMD_MODE_SELECT:
	    case SCSI_CMD_REASSIGN_BLOCKS:
	    case SCSI_CMD_FORMAT_UNIT:
		tgt->transient_state.cmd_count = sizeof(scsi_command_group_0);
		tgt->transient_state.out_count = cmd_count - sizeof(scsi_command_group_0);
		/* fall through */
	    case SCSI_CMD_REQUEST_SENSE:
	    case SCSI_CMD_MODE_SENSE:
	    case SCSI_CMD_RECEIVE_DIAG_RESULTS:
	    case SCSI_CMD_READ_CAPACITY:
	    case SCSI_CMD_READ_BLOCK_LIMITS:
	    case SCSI_CMD_READ_TOC:
	    case SCSI_CMD_READ_SUBCH:
	    case SCSI_CMD_READ_HEADER:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    case SCSI_CMD_TEST_UNIT_READY:
		/*
		 * Do the synch negotiation here, unless prohibited
		 * or done already
		 */
		if ( ! (tgt->flags & TGT_DID_SYNCH)) {
			scp = sbic_script_try_synch;
			tgt->flags |= TGT_TRY_SYNCH;
			cmd_only = FALSE;
		}
		/* fall through */
	    default:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	}

	tgt->transient_state.script = scp;
	tgt->transient_state.handler = handler;
	tgt->transient_state.identify = (cmd_only) ? 0xff :
		(disconn ? SCSI_IDENTIFY|SCSI_IFY_ENABLE_DISCONNECT :
			   SCSI_IDENTIFY);

	if (in_count)
		tgt->transient_state.in_count =
			(in_count < tgt->block_size) ? tgt->block_size : in_count;
	else
		tgt->transient_state.in_count = 0;

	/*
	 * See if another target is currently selected on
	 * this SCSI bus, e.g. lock the sbic structure.
	 * Note that it is the strategy routine's job
	 * to serialize ops on the same target as appropriate.
	 * XXX here and everywhere, locks!
	 */
	/*
	 * Protection viz reconnections makes it tricky.
	 */
	s = splbio();

	if (sbic->wd.nactive++ == 0)
		sbic->wd.watchdog_state = SCSI_WD_ACTIVE;

	if (sbic->state & SBIC_STATE_BUSY) {
		/*
		 * Queue up this target, note that this takes care
		 * of proper FIFO scheduling of the scsi-bus.
		 */
		LOG(3,"enqueue");
		enqueue_tail(&sbic->waiting_targets, (queue_entry_t) tgt);
	} else {
		/*
		 * It is down to at most two contenders now,
		 * we will treat reconnections same as selections
		 * and let the scsi-bus arbitration process decide.
		 */
		sbic->state |= SBIC_STATE_BUSY;
		sbic->next_target = tgt;
		sbic_attempt_selection(sbic);
		/*
		 * Note that we might still lose arbitration..
		 */
	}
	splx(s);
}

sbic_attempt_selection(sbic)
	sbic_softc_t	sbic;
{
	target_info_t	*tgt;
	sbic_padded_regmap_t	*regs;
	register unsigned char	val;
	register int	out_count;

	regs = sbic->regs;
	tgt = sbic->next_target;

	LOG(4,"select");
	LOG(0x80+tgt->target_id,0);

	/*
	 * We own the bus now.. unless we lose arbitration
	 */
	sbic->active_target = tgt;

	/* Try to avoid reselect collisions */
	GET_SBIC_asr(regs,val);
	if (val & SBIC_ASR_INT)
		return;

	/*
	 * Init bus state variables
	 */
	sbic->script = tgt->transient_state.script;
	sbic->error_handler = tgt->transient_state.handler;
	sbic->done = SCSI_RET_IN_PROGRESS;

	sbic->out_count = 0;
	sbic->in_count = 0;

	/* Define how the identify msg should be built */
	GET_SBIC_rselid(regs, val);
	val &= ~(SBIC_RID_MASK|SBIC_RID_ER);
	/* the enable reselection bit is used to build the identify msg */
	if (tgt->transient_state.identify != 0xff)
		val |= (tgt->transient_state.identify & SCSI_IFY_ENABLE_DISCONNECT) << 1;
	SET_SBIC_rselid(regs, val);
	SET_SBIC_tlun(regs, tgt->lun);

	/*
	 * Start the chip going
	 */
	out_count = (*sbic->dma_ops->start_cmd)(sbic->dma_state, tgt);
	SBIC_TC_PUT(regs, out_count);

	val = tgt->target_id;
	if (tgt->transient_state.in_count)
		val |= SBIC_SID_FROM_SCSI;
	SET_SBIC_selid(regs, val);

	SET_SBIC_timo(regs,SBIC_TIMEOUT(250,SBIC_CLOCK_FREQUENCY));

	SET_SBIC_syn(regs,SBIC_SYN(tgt->sync_offset,tgt->sync_period));

	/* ugly little help for compiler */
#define	command	out_count
	if (tgt->flags & TGT_DID_SYNCH) {
		command = (tgt->transient_state.identify == 0xff) ?
				SBIC_CMD_SEL_XFER :
				SBIC_CMD_SEL_ATN_XFER; /*preferred*/
	} else if (tgt->flags & TGT_TRY_SYNCH)
		command = SBIC_CMD_SEL_ATN;
	else
		command = SBIC_CMD_SEL_XFER;

	/* load 10 bytes anyways, the chip knows how much to use */
	SBIC_LOAD_COMMAND(regs, tgt->cmd_ptr, 10);

	/* Try to avoid reselect collisions */
	GET_SBIC_asr(regs,val);
	if (val & SBIC_ASR_INT)
		return;

	SET_SBIC_cmd_phase(regs, 0);	/* not a resume */
	SET_SBIC_cmd(regs, command);
#undef	command
}

/*
 * Interrupt routine
 *	Take interrupts from the chip
 *
 * Implementation:
 *	Move along the current command's script if
 *	all is well, invoke error handler if not.
 */
sbic_intr(unit, spllevel)
	spl_t	spllevel;
{
	register sbic_softc_t	sbic;
	register script_t	scp;
	register int		asr, csr, pha;
	register sbic_padded_regmap_t	*regs;
#if	MAPPABLE
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface)
		return SBIC_intr(unit,spllevel);
#endif	/*MAPPABLE*/

	sbic = sbic_softc[unit];
	regs = sbic->regs;

	LOG(5,"\n\tintr");

	/* drop spurious interrupts */
	GET_SBIC_asr(regs, asr);
	if ((asr & SBIC_ASR_INT) == 0)
		return;

	/* collect ephemeral information */
	GET_SBIC_cmd_phase(regs, pha);
	GET_SBIC_csr(regs, csr);

TR(csr);TR(asr);TR(pha);TRCHECK

	/* XXX verify this is indeed the case for a SCSI RST asserted */
	if ((csr & SBIC_CSR_CAUSE) == SBIC_CSR_RESET)
		return sbic_bus_reset(sbic);

	/* we got an interrupt allright */
	if (sbic->active_target)
		sbic->wd.watchdog_state = SCSI_WD_ACTIVE;

	splx(spllevel);	/* drop priority */

	if ((sbic->state & SBIC_STATE_TARGET) ||
	    (csr == SBIC_CSR_RSLT_AM) || (csr == SBIC_CSR_RSLT_NOAM) ||
	    (csr == SBIC_CSR_SLT) || (csr == SBIC_CSR_SLT_ATN))
		return sbic_target_intr(sbic);

	/*
	 * In attempt_selection() we gave the select command even if
	 * the chip might have been reconnected already.
	 */
	if ((csr == SBIC_CSR_RSLT_NI) || (csr == SBIC_CSR_RSLT_IFY))
		return sbic_reconnect(sbic, csr, pha);

	/*
	 * Check for parity errors
	 */
	if (asr & SBIC_ASR_PE) {
		char	*msg;
printf("{PE %x,%x}", asr, pha);

		msg = "SCSI bus parity error";
		/* all we can do is to throw a reset on the bus */
		printf( "sbic%d: %s%s", sbic - sbic_softc_data, msg,
			", attempting recovery.\n");
		sbic_reset(regs, FALSE);
		return;
	}

	if ((scp = sbic->script) == 0)	/* sanity */
		return;

	LOG(6,"match");
	if (SCRIPT_MATCH(csr,pha,scp->condition)) {
		/*
		 * Perform the appropriate operation,
		 * then proceed
		 */
		if ((*scp->action)(sbic, csr, pha)) {
			sbic->script = scp + 1;
		}
	} else
		return (*sbic->error_handler)(sbic, csr, pha);
}

sbic_target_intr()
{
	panic("SBIC: TARGET MODE !!!\n");
}

/*
 * Routines that the interrupt code might switch to
 */

boolean_t
sbic_end(sbic, csr, pha)
	register sbic_softc_t	sbic;
{
	register target_info_t	*tgt;
	register io_req_t	ior;

	LOG(8,"end");

	tgt = sbic->active_target;
	if ((tgt->done = sbic->done) == SCSI_RET_IN_PROGRESS)
		tgt->done = SCSI_RET_SUCCESS;

	sbic->script = 0;

	if (sbic->wd.nactive-- == 1)
		sbic->wd.watchdog_state = SCSI_WD_INACTIVE;

	sbic_release_bus(sbic);

	if (ior = tgt->ior) {
		(*sbic->dma_ops->end_cmd)(sbic->dma_state, tgt, ior);
		LOG(0xA,"ops->restart");
		(*tgt->dev_ops->restart)( tgt, TRUE);
	}

	return FALSE;
}

boolean_t
sbic_release_bus(sbic)
	register sbic_softc_t	sbic;
{
	boolean_t	ret = TRUE;

	LOG(9,"release");
	if (sbic->state & SBIC_STATE_COLLISION) {

		LOG(0xB,"collided");
		sbic->state &= ~SBIC_STATE_COLLISION;
		sbic_attempt_selection(sbic);

	} else if (queue_empty(&sbic->waiting_targets)) {

		sbic->state &= ~SBIC_STATE_BUSY;
		sbic->active_target = 0;
		sbic->script = 0;
		ret = FALSE;

	} else {

		LOG(0xC,"dequeue");
		sbic->next_target = (target_info_t *)
				dequeue_head(&sbic->waiting_targets);
		sbic_attempt_selection(sbic);
	}
	return ret;
}

boolean_t
sbic_get_status(sbic, csr, pha)
	register sbic_softc_t	sbic;
{
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register scsi2_status_byte_t status;
	int			len;
	io_req_t		ior;
	register target_info_t	*tgt = sbic->active_target;

	LOG(0xD,"get_status");
TRWRAP

	sbic->state &= ~SBIC_STATE_DMA_IN;

	/*
	 * Get the status byte
	 */
	GET_SBIC_tlun(regs, status.bits);

	if (status.st.scsi_status_code != SCSI_ST_GOOD) {
		scsi_error(sbic->active_target, SCSI_ERR_STATUS, status.bits, 0);
		sbic->done = (status.st.scsi_status_code == SCSI_ST_BUSY) ?
			SCSI_RET_RETRY : SCSI_RET_NEED_SENSE;
	} else
		sbic->done = SCSI_RET_SUCCESS;

	/* Tell DMA engine we are done */
	(*sbic->dma_ops->end_xfer)(sbic->dma_state, tgt, tgt->transient_state.in_count);

	return sbic_end(sbic, csr, pha);

}

#if 0

boolean_t
sbic_dma_in(sbic, csr, ir)
	register sbic_softc_t	sbic;
{
	register target_info_t	*tgt;
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register int		count;
	unsigned char		ff = regs->sbic_flags;

	LOG(0xE,"dma_in");
	tgt = sbic->active_target;

	sbic->state |= SBIC_STATE_DMA_IN;

	count = (*sbic->dma_ops->start_datain)(sbic->dma_state, tgt);
	SBIC_TC_PUT(regs, count);

	if ((sbic->in_count = count) == tgt->transient_state.in_count)
		return TRUE;
	regs->sbic_cmd = sbic->script->command;
	sbic->script = sbic_script_restart_data_in;
	return FALSE;
}

sbic_dma_in_r(sbic, csr, ir)
	register sbic_softc_t	sbic;
{
	register target_info_t	*tgt;
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register int		count;
	boolean_t		advance_script = TRUE;


	LOG(0xE,"dma_in");
	tgt = sbic->active_target;

	sbic->state |= SBIC_STATE_DMA_IN;

	if (sbic->in_count == 0) {
		/*
		 * Got nothing yet, we just reconnected.
		 */
		register int avail;

		/*
		 * Rather than using the messy RFB bit in cnfg2
		 * (which only works for synch xfer anyways)
		 * we just bump up the dma offset.  We might
		 * endup with one more interrupt at the end,
		 * so what.
		 * This is done in sbic_err_disconn(), this
		 * way dma (of msg bytes too) is always aligned
		 */

		count = (*sbic->dma_ops->restart_datain_1)
				(sbic->dma_state, tgt);

		/* common case of 8k-or-less read ? */
		advance_script = (tgt->transient_state.in_count == count);

	} else {

		/*
		 * We received some data.
		 */
		register int offset, xferred;

		/*
		 * Problem: sometimes we get a 'spurious' interrupt
		 * right after a reconnect.  It goes like disconnect,
		 * reconnect, dma_in_r, here but DMA is still rolling.
		 * Since there is no good reason we got here to begin with
		 * we just check for the case and dismiss it: we should
		 * get another interrupt when the TC goes to zero or the
		 * target disconnects.
		 */
		SBIC_TC_GET(regs,xferred);
		if (xferred != 0)
			return FALSE;

		xferred = sbic->in_count - xferred;
		assert(xferred > 0);

		tgt->transient_state.in_count -= xferred;
		assert(tgt->transient_state.in_count > 0);

		count = (*sbic->dma_ops->restart_datain_2)
				(sbic->dma_state, tgt, xferred);

		sbic->in_count = count;
		SBIC_TC_PUT(regs, count);
		regs->sbic_cmd = sbic->script->command;

		(*sbic->dma_ops->restart_datain_3)
			(sbic->dma_state, tgt);

		/* last chunk ? */
		if (count == tgt->transient_state.in_count)
			sbic->script++;

		return FALSE;
	}

	sbic->in_count = count;
	SBIC_TC_PUT(regs, count);

	if (!advance_script) {
		regs->sbic_cmd = sbic->script->command;
	}
	return advance_script;
}


/* send data to target.  Only called to start the xfer */

boolean_t
sbic_dma_out(sbic, csr, ir)
	register sbic_softc_t	sbic;
{
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register int		reload_count;
	register target_info_t	*tgt;
	int			command;

	LOG(0xF,"dma_out");

	SBIC_TC_GET(regs, reload_count);
	sbic->extra_count = regs->sbic_flags & SBIC_FLAGS_FIFO_CNT;
	reload_count += sbic->extra_count;
	SBIC_TC_PUT(regs, reload_count);
	sbic->state &= ~SBIC_STATE_DMA_IN;

	tgt = sbic->active_target;

	command = sbic->script->command;

	if ((sbic->out_count = reload_count) >=
	    tgt->transient_state.out_count)
		sbic->script++;
	else
		sbic->script = sbic_script_restart_data_out;

	if ((*sbic->dma_ops->start_dataout)
	      (sbic->dma_state, tgt, &regs->sbic_cmd, command)) {
			regs->sbic_cmd = command;
	}

	return FALSE;
}

/* send data to target. Called in two different ways:
   (a) to restart a big transfer and 
   (b) after reconnection
 */
boolean_t
sbic_dma_out_r(sbic, csr, ir)
	register sbic_softc_t	sbic;
{
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register target_info_t	*tgt;
	boolean_t		advance_script = TRUE;
	int			count;


	LOG(0xF,"dma_out");

	tgt = sbic->active_target;
	sbic->state &= ~SBIC_STATE_DMA_IN;

	if (sbic->out_count == 0) {
		/*
		 * Nothing committed: we just got reconnected
		 */
		count = (*sbic->dma_ops->restart_dataout_1)
				(sbic->dma_state, tgt);

		/* is this the last chunk ? */
		advance_script = (tgt->transient_state.out_count == count);
	} else {
		/*
		 * We sent some data.
		 */
		register int offset, xferred;

		SBIC_TC_GET(regs,count);

		/* see comment above */
		if (count) {
			return FALSE;
		}

		count += (regs->sbic_flags  & SBIC_FLAGS_FIFO_CNT);
		count -= sbic->extra_count;
		xferred = sbic->out_count - count;
		assert(xferred > 0);

		tgt->transient_state.out_count -= xferred;
		assert(tgt->transient_state.out_count > 0);

		count = (*sbic->dma_ops->restart_dataout_2)
				(sbic->dma_state, tgt, xferred);

		/* last chunk ? */
		if (tgt->transient_state.out_count == count)
			goto quickie;

		sbic->out_count = count;

		sbic->extra_count = (*sbic->dma_ops->restart_dataout_3)
					(sbic->dma_state, tgt, &regs->sbic_fifo);
		SBIC_TC_PUT(regs, count);
		regs->sbic_cmd = sbic->script->command;

		(*sbic->dma_ops->restart_dataout_4)(sbic->dma_state, tgt);

		return FALSE;
	}

quickie:
	sbic->extra_count = (*sbic->dma_ops->restart_dataout_3)
				(sbic->dma_state, tgt, &regs->sbic_fifo);

	sbic->out_count = count;

	SBIC_TC_PUT(regs, count);

	if (!advance_script) {
		regs->sbic_cmd = sbic->script->command;
	}
	return advance_script;
}
#endif	/*0*/

boolean_t
sbic_dosynch(sbic, csr, pha)
	register sbic_softc_t	sbic;
	register unsigned char	csr, pha;
{
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register unsigned char	c;
	int i, per, offs;
	register target_info_t	*tgt;

	/*
	 * Try synch negotiation
	 * Phase is MSG_OUT here.
	 */
	tgt = sbic->active_target;

#if 0
	regs->sbic_cmd = SBIC_CMD_FLUSH;
	delay(2);

	per = sbic_min_period;
	if (BGET(scsi_no_synchronous_xfer,sbic->sc->masterno,tgt->target_id))
		offs = 0;
	else
		offs = sbic_max_offset;

	tgt->flags |= TGT_DID_SYNCH;	/* only one chance */
	tgt->flags &= ~TGT_TRY_SYNCH;

	regs->sbic_fifo = SCSI_EXTENDED_MESSAGE;
	regs->sbic_fifo = 3;
	regs->sbic_fifo = SCSI_SYNC_XFER_REQUEST;
	regs->sbic_fifo = sbic_to_scsi_period(regs,sbic_min_period);
	regs->sbic_fifo = offs;
	regs->sbic_cmd = SBIC_CMD_XFER_INFO;
	csr = sbic_wait(regs, SBIC_CSR_INT);
	ir = regs->sbic_intr;

	if (SCSI_PHASE(csr) != SCSI_PHASE_MSG_IN)
		gimmeabreak();

	regs->sbic_cmd = SBIC_CMD_XFER_INFO;
	csr = sbic_wait(regs, SBIC_CSR_INT);
	ir = regs->sbic_intr;

	while ((regs->sbic_flags & SBIC_FLAGS_FIFO_CNT) > 0)
		c = regs->sbic_fifo;	/* see what it says */

	if (c == SCSI_MESSAGE_REJECT) {
		printf(" did not like SYNCH xfer ");

		/* Tk50s get in trouble with ATN, sigh. */
		regs->sbic_cmd = SBIC_CMD_CLR_ATN;

		goto cmd;
	}

	/*
	 * Receive the rest of the message
	 */
	regs->sbic_cmd = SBIC_CMD_MSG_ACPT;
	sbic_wait(regs, SBIC_CSR_INT);
	ir = regs->sbic_intr;

	if (c != SCSI_EXTENDED_MESSAGE)
		gimmeabreak();

	regs->sbic_cmd = SBIC_CMD_XFER_INFO;
	sbic_wait(regs, SBIC_CSR_INT);
	c = regs->sbic_intr;
	if (regs->sbic_fifo != 3)
		panic("sbic_dosynch");

	for (i = 0; i < 3; i++) {
		regs->sbic_cmd = SBIC_CMD_MSG_ACPT;
		sbic_wait(regs, SBIC_CSR_INT);
		c = regs->sbic_intr;

		regs->sbic_cmd = SBIC_CMD_XFER_INFO;
		sbic_wait(regs, SBIC_CSR_INT);
		c = regs->sbic_intr;/*ack*/
		c = regs->sbic_fifo;

		if (i == 1) tgt->sync_period = scsi_period_to_sbic(regs,c);
		if (i == 2) tgt->sync_offset = c;
	}

cmd:
	regs->sbic_cmd = SBIC_CMD_MSG_ACPT;
	csr = sbic_wait(regs, SBIC_CSR_INT);
	c = regs->sbic_intr;

	/* phase should normally be command here */
	if (SCSI_PHASE(csr) == SCSI_PHASE_CMD) {
		/* test unit ready or what ? */
		regs->sbic_fifo = 0;
		regs->sbic_fifo = 0;
		regs->sbic_fifo = 0;
		regs->sbic_fifo = 0;
		regs->sbic_fifo = 0;
		regs->sbic_fifo = 0;
		SBIC_TC_PUT(regs,0xff);
		regs->sbic_cmd = SBIC_CMD_XFER_PAD; /*0x98*/
		csr = sbic_wait(regs, SBIC_CSR_INT);
		ir = regs->sbic_intr;/*ack*/
	}

status:
	if (SCSI_PHASE(csr) != SCSI_PHASE_STATUS)
		gimmeabreak();

#endif
	return TRUE;
}

/*
 * The bus was reset
 */
sbic_bus_reset(sbic)
	register sbic_softc_t	sbic;
{
	register sbic_padded_regmap_t	*regs = sbic->regs;

	LOG(0x1d,"bus_reset");

	/*
	 * Clear bus descriptor
	 */
	sbic->script = 0;
	sbic->error_handler = 0;
	sbic->active_target = 0;
	sbic->next_target = 0;
	sbic->state &= SBIC_STATE_AM_MODE;	/* save this one bit only */
	queue_init(&sbic->waiting_targets);
	sbic->wd.nactive = 0;
	(void) sbic_reset(regs, TRUE);

	printf("sbic: (%d) bus reset ", ++sbic->wd.reset_count);
	delay(scsi_delay_after_reset); /* some targets take long to reset */

	if (sbic->sc == 0)	/* sanity */
		return;

	scsi_bus_was_reset(sbic->sc);
}

/*
 * Disconnect/reconnect mode ops
 */

/* save all relevant data, free the BUS */
boolean_t
sbic_disconnected(sbic, csr, pha)
	register sbic_softc_t	sbic;
	register unsigned char	csr, pha;

{
	register target_info_t	*tgt;

	LOG(0x11,"disconnected");

	tgt = sbic->active_target;
	tgt->flags |= TGT_DISCONNECTED;
	tgt->transient_state.handler = sbic->error_handler;
	/* anything else was saved in sbic_err_disconn() */

	PRINT(("{D%d}", tgt->target_id));

	sbic_release_bus(sbic);

	return FALSE;
}

/* See who reconnected, restore BUS */
boolean_t
sbic_reconnect(sbic, csr, ir)
	register sbic_softc_t	sbic;
	register unsigned char	csr, ir;

{
	register target_info_t	*tgt;
	sbic_padded_regmap_t	*regs;
	int			id, pha;

	LOG(0x12,"reconnect");
	/*
	 * See if this reconnection collided with a selection attempt
	 */
	if (sbic->state & SBIC_STATE_BUSY)
		sbic->state |= SBIC_STATE_COLLISION;

	sbic->state |= SBIC_STATE_BUSY;

	/* find tgt */
	regs = sbic->regs;
	GET_SBIC_rselid(regs,id);

	id &= 0x7;

	if ((sbic->state & SBIC_STATE_AM) == 0) {
		/* Must pick the identify */
		pha = 0x44;
	} else
		pha = 0x45;

	tgt = sbic->sc->target[id];
	if (id > 7 || tgt == 0) panic("sbic_reconnect");

	/* synch things*/
	SET_SBIC_syn(regs,SBIC_SYN(tgt->sync_offset,tgt->sync_period));

	PRINT(("{R%d}", id));
	if (sbic->state & SBIC_STATE_COLLISION)
		PRINT(("[B %d-%d]", sbic->active_target->target_id, id));

	LOG(0x80+id,0);

	sbic->active_target = tgt;
	tgt->flags &= ~TGT_DISCONNECTED;

	sbic->script = tgt->transient_state.script;
	sbic->error_handler = tgt->transient_state.handler;
	sbic->in_count = 0;
	sbic->out_count = 0;

set counter and setup dma, then

	/* Resume the command now */
	SET_SBIC_cmd_phase(regs, pha);
	SET_SBIC_cmd(regs, SBIC_CMD_SEL_XFER);

	return FALSE;
}

TILL HERE

/*
 * Error handlers
 */

/*
 * Fall-back error handler.
 */
sbic_err_generic(sbic, csr, ir)
	register sbic_softc_t	sbic;
{
	LOG(0x13,"err_generic");

	/* handle non-existant or powered off devices here */
	if ((ir == SBIC_INT_DISC) &&
	    (sbic_isa_select(sbic->cmd_was)) &&
	    (SBIC_SS(sbic->ss_was) == 0)) {
		/* Powered off ? */
		if (sbic->active_target->flags & TGT_FULLY_PROBED)
			sbic->active_target->flags = 0;
		sbic->done = SCSI_RET_DEVICE_DOWN;
		sbic_end(sbic, csr, ir);
		return;
	}

	switch (SCSI_PHASE(csr)) {
	case SCSI_PHASE_STATUS:
		if (sbic->script[-1].condition == SCSI_PHASE_STATUS) {
			/* some are just slow to get out.. */
		} else
			sbic_err_to_status(sbic, csr, ir);
		return;
		break;
	case SCSI_PHASE_DATAI:
		if (sbic->script->condition == SCSI_PHASE_STATUS) {
/*			printf("{P}");*/
			return;
		}
		break;
	case SCSI_PHASE_DATAO:
		if (sbic->script->condition == SCSI_PHASE_STATUS) {
			/*
			 * See comment above. Actually seen on hitachis.
			 */
/*			printf("{P}");*/
			return;
		}
	}
	gimmeabreak();
}

/*
 * Handle disconnections as exceptions
 */
sbic_err_disconn(sbic, csr, ir)
	register sbic_softc_t	sbic;
	register unsigned char	csr, ir;
{
	register sbic_padded_regmap_t	*regs;
	register target_info_t	*tgt;
	int			count;
	boolean_t		callback = FALSE;

	LOG(0x16,"err_disconn");

	if (SCSI_PHASE(csr) != SCSI_PHASE_MSG_IN)
		return sbic_err_generic(sbic, csr, ir);

	regs = sbic->regs;
	tgt = sbic->active_target;

	switch (sbic->script->condition) {
	case SCSI_PHASE_DATAO:
		LOG(0x1b,"+DATAO");
		if (sbic->out_count) {
			register int xferred, offset;

			SBIC_TC_GET(regs,xferred); /* temporary misnomer */
			xferred += regs->sbic_flags & SBIC_FLAGS_FIFO_CNT;
			xferred -= sbic->extra_count;
			xferred = sbic->out_count - xferred; /* ok now */
			tgt->transient_state.out_count -= xferred;
			assert(tgt->transient_state.out_count > 0);

			callback = (*sbic->dma_ops->disconn_1)
					(sbic->dma_state, tgt, xferred);

		} else {

			callback = (*sbic->dma_ops->disconn_2)
					(sbic->dma_state, tgt);

		}
		sbic->extra_count = 0;
		tgt->transient_state.script = sbic_script_restart_data_out;
		break;


	case SCSI_PHASE_DATAI:
		LOG(0x17,"+DATAI");
		if (sbic->in_count) {
			register int offset, xferred;

			SBIC_TC_GET(regs,count);
			xferred = sbic->in_count - count;
			assert(xferred > 0);

if (regs->sbic_flags & 0xf)
printf("{Xf %x,%x,%x}", xferred, sbic->in_count, regs->sbic_flags & SBIC_FLAGS_FIFO_CNT);
			tgt->transient_state.in_count -= xferred;
			assert(tgt->transient_state.in_count > 0);

			callback = (*sbic->dma_ops->disconn_3)
					(sbic->dma_state, tgt, xferred);

			tgt->transient_state.script = sbic_script_restart_data_in;
			if (tgt->transient_state.in_count == 0)
				tgt->transient_state.script++;

		}
		tgt->transient_state.script = sbic->script;
		break;

	case SCSI_PHASE_STATUS:
		/* will have to restart dma */
		SBIC_TC_GET(regs,count);
		if (sbic->state & SBIC_STATE_DMA_IN) {
			register int offset, xferred;

			LOG(0x1a,"+STATUS+R");

			xferred = sbic->in_count - count;
			assert(xferred > 0);

if (regs->sbic_flags & 0xf)
printf("{Xf %x,%x,%x}", xferred, sbic->in_count, regs->sbic_flags & SBIC_FLAGS_FIFO_CNT);
			tgt->transient_state.in_count -= xferred;
/*			assert(tgt->transient_state.in_count > 0);*/

			callback = (*sbic->dma_ops->disconn_4)
					(sbic->dma_state, tgt, xferred);

			tgt->transient_state.script = sbic_script_restart_data_in;
			if (tgt->transient_state.in_count == 0)
				tgt->transient_state.script++;

		} else {

			/* add what's left in the fifo */
			count += (regs->sbic_flags & SBIC_FLAGS_FIFO_CNT);
			/* take back the extra we might have added */
			count -= sbic->extra_count;
			/* ..and drop that idea */
			sbic->extra_count = 0;

			LOG(0x19,"+STATUS+W");


			if ((count == 0) && (tgt->transient_state.out_count == sbic->out_count)) {
				/* all done */
				tgt->transient_state.script = sbic->script;
				tgt->transient_state.out_count = 0;
			} else {
				register int xferred, offset;

				/* how much we xferred */
				xferred = sbic->out_count - count;

				tgt->transient_state.out_count -= xferred;
				assert(tgt->transient_state.out_count > 0);

				callback = (*sbic->dma_ops->disconn_5)
						(sbic->dma_state,tgt,xferred);

				tgt->transient_state.script = sbic_script_restart_data_out;
			}
			sbic->out_count = 0;
		}
		break;
	default:
		gimmeabreak();
		return;
	}
	sbic_msg_in(sbic,csr,ir);
	sbic->script = sbic_script_disconnect;
	regs->sbic_cmd = SBIC_CMD_XFER_INFO|SBIC_CMD_DMA;
	if (callback)
		(*sbic->dma_ops->disconn_callback)(sbic->dma_state, tgt);
}

/*
 * Watchdog
 *
 * We know that some (name withdrawn) disks get
 * stuck in the middle of dma phases...
 */
sbic_reset_scsibus(sbic)
	register sbic_softc_t	sbic;
{
	register target_info_t	*tgt = sbic->active_target;
	register sbic_padded_regmap_t	*regs = sbic->regs;
	register int		ir;

	if (scsi_debug && tgt) {
		int dmalen;
		SBIC_TC_GET(sbic->regs,dmalen);
		printf("Target %d was active, cmd x%x in x%x out x%x Sin x%x Sou x%x dmalen x%x\n", 
			tgt->target_id, tgt->cur_cmd,
			tgt->transient_state.in_count, tgt->transient_state.out_count,
			sbic->in_count, sbic->out_count,
			dmalen);
	}
	ir = regs->sbic_intr;
	if ((ir & SBIC_INT_RESEL) && (SCSI_PHASE(regs->sbic_csr) == SCSI_PHASE_MSG_IN)) {
		/* getting it out of the woods is a bit tricky */
		spl_t	s = splbio();

		(void) sbic_reconnect(sbic, regs->sbic_csr, ir);
		sbic_wait(regs, SBIC_CSR_INT);
		ir = regs->sbic_intr;
		regs->sbic_cmd = SBIC_CMD_MSG_ACPT;
		splx(s);
	} else {
		regs->sbic_cmd = SBIC_CMD_BUS_RESET;
		delay(35);
	}
}

#endif	NSBIC > 0

#endif 0
