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
 *	File: scsi_7061_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Bottom layer of the SCSI driver: chip-dependent functions
 *
 *	This file contains the code that is specific to the DEC DC7061
 *	SCSI chip (Host Bus Adapter in SCSI parlance): probing, start
 *	operation, and interrupt routine.
 */

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

#include <sii.h>
#if	NSII > 0

#include <platforms.h>

#ifdef	DECSTATION
#define	PAD(n)		short n
#endif

#include <machine/machspl.h>		/* spl definitions */
#include <mach/std_types.h>
#include <chips/busses.h>
#include <scsi/compat_30.h>
#include <machine/machspl.h>

#include <sys/syslog.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#define	isa_oddbb	hba_dep[0]
#define	oddbb		hba_dep[1]

#include <scsi/adapters/scsi_7061.h>

#ifdef	PAD

typedef struct {
	volatile unsigned short	sii_sdb;	/* rw: Data bus and parity */
	PAD(pad0);
	volatile unsigned short	sii_sc1;	/* rw: scsi signals 1 */
	PAD(pad1);
	volatile unsigned short	sii_sc2;	/* rw: scsi signals 2 */
	PAD(pad2);
	volatile unsigned short	sii_csr;	/* rw: control and status */
	PAD(pad3);
	volatile unsigned short	sii_id;		/* rw: scsi bus ID */
	PAD(pad4);
	volatile unsigned short	sii_sel_csr;	/* rw: selection status */
	PAD(pad5);
	volatile unsigned short	sii_destat;	/* ro: selection detector status */
	PAD(pad6);
	volatile unsigned short	sii_dstmo;	/* unsupp: dssi timeout */
	PAD(pad7);
	volatile unsigned short	sii_data;	/* rw: data register */
	PAD(pad8);
	volatile unsigned short	sii_dma_ctrl;	/* rw: dma control reg */
	PAD(pad9);
	volatile unsigned short	sii_dma_len;	/* rw: length of transfer */
	PAD(pad10);
	volatile unsigned short	sii_dma_adr_low;/* rw: low address */
	PAD(pad11);
	volatile unsigned short	sii_dma_adr_hi;	/* rw: high address */
	PAD(pad12);
	volatile unsigned short	sii_dma_1st_byte;/* rw: initial byte */
	PAD(pad13);
	volatile unsigned short	sii_stlp;	/* unsupp: dssi short trgt list ptr */
	PAD(pad14);
	volatile unsigned short	sii_ltlp;	/* unsupp: dssi long " " " */
	PAD(pad15);
	volatile unsigned short	sii_ilp;	/* unsupp: dssi initiator list ptr */
	PAD(pad16);
	volatile unsigned short	sii_dssi_csr;	/* unsupp: dssi control */
	PAD(pad17);
	volatile unsigned short	sii_conn_csr;	/* rc: connection interrupt control */
	PAD(pad18);
	volatile unsigned short	sii_data_csr;	/* rc: data interrupt control */
	PAD(pad19);
	volatile unsigned short	sii_cmd;	/* rw: command register */
	PAD(pad20);
	volatile unsigned short	sii_diag_csr;	/* rw: disgnostic status */
	PAD(pad21);
} sii_padded_regmap_t;

#else	/*!PAD*/

typedef sii_regmap_t	sii_padded_regmap_t;

#endif	/*!PAD*/


#undef	SII_CSR_SLE
#define SII_CSR_SLE	0	/* for now */

#ifdef	DECSTATION
#include <mips/PMAX/kn01.h>
#define	SII_OFFSET_RAM	(KN01_SYS_SII_B_START-KN01_SYS_SII)
#define	SII_RAM_SIZE	(KN01_SYS_SII_B_END-KN01_SYS_SII_B_START)
/* 16 bits in 32 bit envelopes */
#define	SII_DMADR_LO(ptr)	((((unsigned)ptr)>>1)&SII_DMA_LOW_MASK)
#define	SII_DMADR_HI(ptr)	((((unsigned)ptr)>>(16+1))&SII_DMA_HIGH_MASK)
#endif	/* DECSTATION */

#ifndef	SII_OFFSET_RAM		/* cross compile check */
#define	SII_OFFSET_RAM		0
#define	SII_RAM_SIZE		0x10000
#define	SII_DMADR_LO(ptr)	(((unsigned)ptr)>>16)
#define	SII_DMADR_HI(ptr)	(((unsigned)ptr)&0xffff)
#endif

/*
 * Statically partition the DMA buffer between targets.
 * This way we will eventually be able to attach/detach
 * drives on-fly.  And 18k/target is enough.
 */
#define PER_TGT_DMA_SIZE		((SII_RAM_SIZE/7) & ~(sizeof(int)-1))

/*
 * Round to 4k to make debug easier
 */
#define	PER_TGT_BUFF_SIZE		((PER_TGT_DMA_SIZE >> 12) << 12)

/*
 * Macros to make certain things a little more readable
 */
#define	SII_COMMAND(regs,cs,ds,cmd)				\
	{							\
		(regs)->sii_cmd = ((cs) & 0x70) |		\
				      ((ds) & 0x07) | (cmd);	\
		wbflush();					\
	}
#define	SII_ACK(regs,cs,ds,cmd)					\
	{							\
		SII_COMMAND(regs,cs,ds,cmd);			\
		(regs)->sii_conn_csr = (cs);			\
		(regs)->sii_data_csr = (ds);			\
	}

/*
 * Deal with bogus pmax dma buffer
 */

static char	decent_buffer[NSII*8][256];

/*
 * A script has a three parts: a pre-condition, an action, and
 * an optional command to the chip.  The first triggers error
 * handling if not satisfied and in our case it is formed by the
 * values of the sii_conn_csr and sii_data_csr register
 * bits.  The action part is just a function pointer, and the
 * command is what the 7061 should be told to do at the end
 * of the action processing.  This command is only issued and the
 * script proceeds if the action routine returns TRUE.
 * See sii_intr() for how and where this is all done.
 */

typedef struct script {
	int	condition;	/* expected state at interrupt */
	int	(*action)();	/* extra operations */
	int	command;	/* command to the chip */
} *script_t;

#define	SCRIPT_MATCH(cs,ds)	((cs)&0x70|SCSI_PHASE((ds)))

#define	SII_PHASE_DISC	0x4	/* sort of .. */

/* When no command is needed */
#define	SCRIPT_END	-1

/* forward decls of script actions */
boolean_t
	sii_script_true(),		/* when nothing needed */
	sii_identify(),			/* send identify msg */
	sii_dosynch(),			/* negotiate synch xfer */
	sii_dma_in(),			/* get data from target via dma */
	sii_dma_out(),			/* send data to target via dma */
	sii_get_status(),		/* get status from target */
	sii_end_transaction(),		/* all come to an end */
	sii_msg_in(),			/* get disconnect message(s) */
	sii_disconnected();		/* current target disconnected */
/* forward decls of error handlers */
boolean_t
	sii_err_generic(),		/* generic error handler */
	sii_err_disconn(),		/* when a target disconnects */
	sii_err_rdp(),			/* in reconn, handle rdp mgs */
	gimmeabreak();			/* drop into the debugger */

int	sii_reset_scsibus();
boolean_t sii_probe_target();
static sii_wait();

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) SCSI-7061 interface
 */
struct sii_softc {
	watchdog_t	wd;
	sii_padded_regmap_t	*regs;
	volatile char	*buff;
	script_t	script;
	int		(*error_handler)();
	int		in_count;	/* amnt we expect to receive */
	int		out_count;	/* amnt we are going to ship */

	volatile char	state;
#define	SII_STATE_BUSY		0x01	/* selecting or currently connected */
#define SII_STATE_TARGET	0x04	/* currently selected as target */
#define SII_STATE_COLLISION	0x08	/* lost selection attempt */
#define SII_STATE_DMA_IN	0x10	/* tgt --> initiator xfer */

	unsigned char	ntargets;	/* how many alive on this scsibus */
	unsigned char	done;
	unsigned char	cmd_count;

	scsi_softc_t	*sc;
	target_info_t	*active_target;

	target_info_t	*next_target;	/* trying to seize bus */
	queue_head_t	waiting_targets;/* other targets competing for bus */

} sii_softc_data[NSII];

typedef struct sii_softc *sii_softc_t;

sii_softc_t	sii_softc[NSII];

/*
 * Synch xfer parameters, and timing conversions
 */
int	sii_min_period = 63;	/* in 4 ns units */
int	sii_max_offset = 3;	/* pure number */

#define sii_to_scsi_period(a)	(a)
#define scsi_period_to_sii(p)	(((p) < sii_min_period) ? sii_min_period : (p))

/*
 * Definition of the controller for the auto-configuration program.
 */

int	sii_probe(), scsi_slave(), sii_go(), sii_intr();
extern void scsi_attach();

vm_offset_t	sii_std[NSII] = { 0 };
struct	bus_device *sii_dinfo[NSII*8];
struct	bus_ctlr *sii_minfo[NSII];
struct	bus_driver sii_driver = 
        { sii_probe, scsi_slave, scsi_attach, sii_go, sii_std, "rz", sii_dinfo,
	  "sii", sii_minfo, /*BUS_INTR_B4_PROBE?*/};

/*
 * Scripts
 */
struct script
sii_script_data_in[] = {
	{ SCSI_PHASE_CMD|SII_CON_CON, sii_script_true,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SCSI_PHASE_CMD},
	{ SCSI_PHASE_DATAI|SII_CON_CON, sii_dma_in,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SCSI_PHASE_DATAI},
	{ SCSI_PHASE_STATUS|SII_CON_CON, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON, sii_end_transaction, SCRIPT_END}
},

sii_script_data_out[] = {
	{ SCSI_PHASE_CMD|SII_CON_CON, sii_script_true,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SCSI_PHASE_CMD},
	{ SCSI_PHASE_DATAO|SII_CON_CON, sii_dma_out,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SCSI_PHASE_DATAO},
	{ SCSI_PHASE_STATUS|SII_CON_CON, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON, sii_end_transaction, SCRIPT_END}
},

sii_script_cmd[] = {
	{ SCSI_PHASE_CMD|SII_CON_CON, sii_script_true,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SCSI_PHASE_CMD},
	{ SCSI_PHASE_STATUS|SII_CON_CON, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON, sii_end_transaction, SCRIPT_END}
},

/* Same, after a disconnect */

sii_script_restart_data_in[] = {
	{ SCSI_PHASE_DATAI|SII_CON_CON|SII_CON_DST, sii_dma_in,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SII_CON_DST|SCSI_PHASE_DATAI},
	{ SCSI_PHASE_STATUS|SII_CON_CON|SII_CON_DST, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SII_CON_DST|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON|SII_CON_DST, sii_end_transaction, SCRIPT_END}
},

sii_script_restart_data_out[] = {
	{ SCSI_PHASE_DATAO|SII_CON_CON|SII_CON_DST, sii_dma_out,
		(SII_CMD_XFER|SII_CMD_DMA)|SII_CON_CON|SII_CON_DST|SCSI_PHASE_DATAO},
	{ SCSI_PHASE_STATUS|SII_CON_CON|SII_CON_DST, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SII_CON_DST|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON|SII_CON_DST, sii_end_transaction, SCRIPT_END}
},

sii_script_restart_cmd[] = {
	{ SCSI_PHASE_STATUS|SII_CON_CON|SII_CON_DST, sii_get_status,
		SII_CMD_XFER|SII_CON_CON|SII_CON_DST|SCSI_PHASE_STATUS},
	{ SCSI_PHASE_MSG_IN|SII_CON_CON|SII_CON_DST, sii_end_transaction, SCRIPT_END}
},

/* Synchronous transfer negotiation */

sii_script_try_synch[] = {
	{ SCSI_PHASE_MSG_OUT|SII_CON_CON, sii_dosynch, SCRIPT_END}
},

/* Disconnect sequence */

sii_script_disconnect[] = {
	{ SII_PHASE_DISC, sii_disconnected, SCRIPT_END}
};



#define	u_min(a,b)	(((a) < (b)) ? (a) : (b))


#define	DEBUG
#ifdef	DEBUG

sii_state(regs)
	sii_padded_regmap_t	*regs;
{
	unsigned	dmadr;

	if (regs == 0)
		regs = (sii_padded_regmap_t*) 0xba000000;

	dmadr = regs->sii_dma_adr_low | (regs->sii_dma_adr_hi << 16);
	db_printf("sc %x, dma %x @ x%X, cs %x, ds %x, cmd %x\n",
		(unsigned) regs->sii_sc1,
		(unsigned) regs->sii_dma_len, dmadr,
		(unsigned) regs->sii_conn_csr,
		(unsigned) regs->sii_data_csr,
		(unsigned) regs->sii_cmd);

}
sii_target_state(tgt)
	target_info_t		*tgt;
{
	if (tgt == 0)
		tgt = sii_softc[0]->active_target;
	if (tgt == 0)
		return 0;
	db_printf("@x%x: fl %X dma %X+%X cmd %x@%X id %X per %X off %X ior %X ret %X\n",
		tgt,
		tgt->flags, tgt->dma_ptr, tgt->transient_state.dma_offset, tgt->cur_cmd,
		tgt->cmd_ptr, tgt->target_id, tgt->sync_period, tgt->sync_offset,
		tgt->ior, tgt->done);
	if (tgt->flags & TGT_DISCONNECTED){
		script_t	spt;

		spt = tgt->transient_state.script;
		db_printf("disconnected at ");
		db_printsym(spt,1);
		db_printf(": %X %X ", spt->condition, spt->command);
		db_printsym(spt->action,1);
		db_printf(", ");
		db_printsym(tgt->transient_state.handler, 1);
		db_printf("\n");
	}

	return 0;
}

sii_all_targets(unit)
{
	int i;
	target_info_t	*tgt;
	for (i = 0; i < 8; i++) {
		tgt = sii_softc[unit]->sc->target[i];
		if (tgt)
			sii_target_state(tgt);
	}
}

sii_script_state(unit)
{
	script_t	spt = sii_softc[unit]->script;

	if (spt == 0) return 0;
	db_printsym(spt,1);
	db_printf(": %X %X ", spt->condition, spt->command);
	db_printsym(spt->action,1);
	db_printf(", ");
	db_printsym(sii_softc[unit]->error_handler, 1);
	return 0;

}

#define	PRINT(x)	if (scsi_debug) printf x

#define TRMAX 200
int tr[TRMAX+3];
int trpt, trpthi;
#define	TR(x)	tr[trpt++] = x
#define TRWRAP	trpthi = trpt; trpt = 0;
#define TRCHECK	if (trpt > TRMAX) {TRWRAP}

#define TRACE

#ifdef TRACE

#define LOGSIZE 256
int sii_logpt;
char sii_log[LOGSIZE];

#define MAXLOG_VALUE	0x25
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static LOG(e,f)
	char *f;
{
	sii_log[sii_logpt++] = (e);
	if (sii_logpt == LOGSIZE) sii_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

sii_print_log(skip)
	int skip;
{
	register int i, j;
	register unsigned char c;

	for (i = 0, j = sii_logpt; i < LOGSIZE; i++) {
		c = sii_log[j];
		if (++j == LOGSIZE) j = 0;
		if (skip-- > 0)
			continue;
		if (c < MAXLOG_VALUE)
			db_printf(" %s", logtbl[c].name);
		else
			db_printf("-x%x", c & 0x7f);
	}
	db_printf("\n");
	return 0;
}

sii_print_stat()
{
	register int i;
	register char *p;
	for (i = 0; i < MAXLOG_VALUE; i++) {
		if (p = logtbl[i].name)
			printf("%d %s\n", logtbl[i].count, p);
	}
}

#else	/* TRACE */
#define	LOG(e,f)
#endif	/* TRACE */

struct cnt {
	unsigned int	zeroes;
	unsigned int	usage;
	unsigned int	avg;
	unsigned int	min;
	unsigned int	max;
};

static bump(counter, value)
	register struct cnt	*counter;
	register unsigned int	value;
{
	register unsigned int	n;

	if (value == 0) {
		counter->zeroes++;
		return;
	}
	n = counter->usage + 1;
	counter->usage = n;
	if (n == 0) {
		printf("{Counter at x%x overflowed with avg x%x}",
			counter, counter->avg);
		return;
	} else
	if (n == 1)
		counter->min = 0xffffffff;

	counter->avg = ((counter->avg * (n - 1)) + value) / n;
	if (counter->min > value)
		counter->min = value;
	if (counter->max < value)
		counter->max = value;
}

struct cnt
	s_cnt;

#else	/* DEBUG */
#define	PRINT(x)
#define	LOG(e,f)
#define TR(x)
#define TRCHECK
#define TRWRAP
#endif	/* DEBUG */


/*
 *	Probe/Slave/Attach functions
 */

/*
 * Probe routine:
 *	Should find out (a) if the controller is
 *	present and (b) which/where slaves are present.
 *
 * Implementation:
 *	Send an identify msg to each possible target on the bus
 *	except of course ourselves.
 */
sii_probe(reg, ui)
	unsigned	reg;
	struct bus_ctlr	*ui;
{
	int             unit = ui->unit;
	sii_softc_t     sii = &sii_softc_data[unit];
	int		target_id, i;
	scsi_softc_t	*sc;
	register sii_padded_regmap_t	*regs;
	spl_t		s;
	boolean_t	did_banner = FALSE;
	char		*dma_ptr;
	static char	*here = "sii_probe";

	/*
	 * We are only called if the chip is there,
	 * but make sure anyways..
	 */
	if (check_memory(reg, 0))
		return 0;

#ifdef	MACH_KERNEL
	/* Mappable version side */
	SII_probe(reg, ui);
#endif	/*MACH_KERNEL*/

	/*
	 * Initialize hw descriptor
	 */
	sii_softc[unit] = sii;
	sii->regs = (sii_padded_regmap_t *) (reg);
	sii->buff = (volatile char*) (reg + SII_OFFSET_RAM);

	queue_init(&sii->waiting_targets);

	sc = scsi_master_alloc(unit, sii);
	sii->sc = sc;

	sc->go = sii_go;
	sc->watchdog = scsi_watchdog;
	sc->probe = sii_probe_target;

	sii->wd.reset = sii_reset_scsibus;

#ifdef	MACH_KERNEL
	sc->max_dma_data = -1;				/* unlimited */
#else
	sc->max_dma_data = scsi_per_target_virtual;
#endif

	regs = sii->regs;

	/*
	 * Clear out dma buffer
	 */
	blkclr(sii->buff, SII_RAM_SIZE);

	/*
	 * Reset chip, fully.
	 */
	s = splbio();
	sii_reset(regs, TRUE);

	/*
	 * Our SCSI id on the bus.
	 * The user can set this via the prom on pmaxen/3maxen.
	 * If this changes it is easy to fix: make a default that
	 * can be changed as boot arg.
	 */
#ifdef	unneeded
	regs->sii_id = (scsi_initiator_id[unit] & SII_ID_MASK)|SII_ID_IO;
#endif
	sc->initiator_id = regs->sii_id & SII_ID_MASK;
	printf("%s%d: my SCSI id is %d", ui->name, unit, sc->initiator_id);

	/*
	 * For all possible targets, see if there is one and allocate
	 * a descriptor for it if it is there.
	 */
	for (target_id = 0, dma_ptr = (char*)sii->buff;
	     target_id < 8;
	     target_id++, dma_ptr += (PER_TGT_DMA_SIZE*2)) {

		register unsigned csr, dsr;
		register scsi_status_byte_t	status;

		/* except of course ourselves */
		if (target_id == sc->initiator_id)
			continue;

		regs->sii_sel_csr = target_id;
		wbflush();

		/* select */
		regs->sii_cmd = SII_CMD_SEL;
		wbflush();

		/* wait for a selection timeout delay, and some more */
		delay(251000);

		dsr = regs->sii_data_csr;
		csr = regs->sii_conn_csr;
		if ((csr & SII_CON_CON) == 0) {

			regs->sii_conn_csr = csr;/*wtc bits*/

			/* abort sel in progress */
			if (csr & SII_CON_SIP) {
				regs->sii_cmd = SII_CMD_DIS;
				wbflush();
				csr = sii_wait(&regs->sii_conn_csr, SII_CON_SCH,1);
				regs->sii_conn_csr = 0xffff;/*wtc bits */
				regs->sii_data_csr = 0xffff;
				regs->sii_cmd = 0;
				wbflush();
			}
			continue;
		}

		printf(",%s%d", did_banner++ ? " " : " target(s) at ",
				target_id);

		/* should be command phase here */
		if (SCSI_PHASE(dsr) != SCSI_PHASE_CMD)
			panic(here);

		/* acknowledge state change */
		SII_ACK(regs,csr,dsr,0);

		/* build command in (bogus) dma area */
		{
			unsigned int	*p = (unsigned int*) dma_ptr;

			p[0] = SCSI_CMD_TEST_UNIT_READY | (0 << 8);
			p[1] = 0 | (0 << 8);
			p[2] = 0 | (0 << 8);
		}

		/* set up dma xfer parameters */
		regs->sii_dma_len = 6;
		regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
		regs->sii_dma_adr_hi  = SII_DMADR_HI(dma_ptr);
		wbflush();

		/* issue dma command */
		SII_COMMAND(regs,csr,dsr,SII_CMD_XFER|SII_CMD_DMA);

		/* wait till done */
		dsr = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
		regs->sii_cmd &= ~(SII_CMD_XFER|SII_CMD_DMA);
		regs->sii_data_csr = SII_DTR_DONE;/* clear */
		regs->sii_dma_len = 0;

		/* move on to status phase */
		dsr = sii_wait(&regs->sii_data_csr, SCSI_PHASE_STATUS,1);
		csr = regs->sii_conn_csr;
		SII_ACK(regs,csr,dsr,0);

		if (SCSI_PHASE(dsr) != SCSI_PHASE_STATUS)
			panic(here);

		/* get status byte */
		dsr = sii_wait(&regs->sii_data_csr, SII_DTR_IBF,1);
		csr = regs->sii_conn_csr;

		status.bits = regs->sii_data;
		if (status.st.scsi_status_code != SCSI_ST_GOOD)
			scsi_error( 0, SCSI_ERR_STATUS, status.bits, 0);

		/* get cmd_complete message */
		SII_ACK(regs,csr,dsr,0);
		SII_COMMAND(regs,csr,dsr,SII_CMD_XFER);
		dsr = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);

		dsr = sii_wait(&regs->sii_data_csr, SCSI_PHASE_MSG_IN,1);
		csr = regs->sii_conn_csr;


		SII_ACK(regs,csr,dsr,0);
		i = regs->sii_data;
		SII_COMMAND(regs,csr,dsr,SII_CMD_XFER);
		
		/* check disconnected, clear all intr bits */
		csr = sii_wait(&regs->sii_conn_csr, SII_CON_SCH,1);
		if (regs->sii_conn_csr & SII_CON_CON)
			panic(here);

		regs->sii_data_csr = 0xffff;
		regs->sii_conn_csr = 0xffff;
		regs->sii_cmd = 0;

		/*
		 * Found a target
		 */
		sii->ntargets++;
		{
			register target_info_t	*tgt;
			tgt = scsi_slave_alloc(sc->masterno, target_id, sii);

			tgt->dma_ptr = dma_ptr;
			tgt->cmd_ptr = decent_buffer[unit*8 + target_id];
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
			fdma_init(&tgt->fdma, scsi_per_target_virtual);
#endif	/*MACH_KERNEL*/
		}
	}
	printf(".\n");

	splx(s);
	return 1;
}

boolean_t
sii_probe_target(tgt, ior)
	target_info_t		*tgt;
	io_req_t		ior;
{
	sii_softc_t     sii = sii_softc[tgt->masterno];
	boolean_t	newlywed;
	int		sii_probe_timeout();

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		/* desc was allocated afresh */
		char *dma_ptr = (char*)sii->buff;

		dma_ptr += (PER_TGT_DMA_SIZE * tgt->target_id)*2;
		tgt->dma_ptr = dma_ptr;
		tgt->cmd_ptr = decent_buffer[tgt->masterno*8 + tgt->target_id];
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_init(&tgt->fdma, scsi_per_target_virtual);
#endif	/*MACH_KERNEL*/

	}

	/* Unfortunately, the SII chip does not have timeout support
	   for selection */
	timeout(sii_probe_timeout, tgt, hz);

	if (scsi_inquiry(tgt, SCSI_INQ_STD_DATA) == SCSI_RET_DEVICE_DOWN)
		return FALSE;

	untimeout(sii_probe_timeout, tgt);
	tgt->flags = TGT_ALIVE;
	return TRUE;
}

sii_probe_timeout(tgt)
	target_info_t		*tgt;
{
	sii_softc_t		sii = (sii_softc_t)tgt->hw_state;
	register sii_padded_regmap_t	*regs = sii->regs;
	int			cs, ds;
	spl_t			s;

	/* cancelled ? */
	if (tgt->done != SCSI_RET_IN_PROGRESS)
		return;

	s = splbio();

	/* Someone else might be using the bus (rare) */
	switch (regs->sii_conn_csr & (SII_CON_LST|SII_CON_SIP)) {
	case SII_CON_SIP:
		/* We really timed out */
		break;
	case SII_CON_SIP|SII_CON_LST:
		/* Someone else is (still) using the bus */
		sii->wd.watchdog_state = SCSI_WD_ACTIVE;
		/* fall-through */
	default:
		/* Did not get a chance to the bus yet */
		timeout(sii_probe_timeout, tgt, hz);
		goto ret;
	}
	regs->sii_cmd = SII_CMD_DIS;
	wbflush();
	regs->sii_csr |= SII_CSR_RSE;
	regs->sii_cmd = 0;
	wbflush();

	sii->done = SCSI_RET_DEVICE_DOWN;
	cs = regs->sii_conn_csr;
	ds = regs->sii_data_csr;
	if (!sii_end(sii, cs, ds))
		(void) sii_reconnect(sii, cs, ds);
ret:
	splx(s);
}


static sii_wait(preg, until, complain)
	volatile unsigned short	*preg;
{
	int timeo = 1000000;
	while ((*preg & until) != until) {
		delay(1);
		if (!timeo--) {
			if (complain) {
			gimmeabreak();
			printf("sii_wait TIMEO with x%x\n", *preg);
			}
			break;
		}
	}
#ifdef	DEBUG
	bump(&s_cnt, 1000000-timeo);
#endif
	return *preg;
}

sii_reset(regs, quickly)
	register sii_padded_regmap_t	*regs;
	boolean_t		quickly;
{
	int my_id;

	my_id = regs->sii_id & SII_ID_MASK;

	regs->sii_cmd = SII_CMD_RESET;
	wbflush();
	delay(30);

	/* clear them all random bitsy */
	regs->sii_conn_csr = SII_CON_SWA|SII_CON_SCH|SII_CON_BERR|SII_CON_RST;
	regs->sii_data_csr = SII_DTR_ATN|SII_DTR_DONE;

	regs->sii_id = my_id | SII_ID_IO;

	regs->sii_dma_ctrl = 0;	/* asynch */

	regs->sii_dma_len = 0;
	regs->sii_dma_adr_low = 0;
	regs->sii_dma_adr_hi = 0;

	regs->sii_csr = SII_CSR_IE|SII_CSR_PCE|SII_CSR_SLE|SII_CSR_HPM;
		/* later: SII_CSR_RSE */

	regs->sii_diag_csr = SII_DIAG_PORT_ENB;
	wbflush();

	if (quickly)
		return;

	/*
	 * reset the scsi bus, the interrupt routine does the rest
	 * or you can call sii_bus_reset().
	 */
	regs->sii_cmd = SII_CMD_RST;
	
}

/*
 *	Operational functions
 */

/*
 * Start a SCSI command on a target
 */
sii_go(tgt, cmd_count, in_count, cmd_only)
	target_info_t		*tgt;
	boolean_t		cmd_only;
{
	sii_softc_t		sii;
	register spl_t		s;
	boolean_t		disconn;
	script_t		scp;
	boolean_t		(*handler)();

	LOG(1,"go");

	sii = (sii_softc_t)tgt->hw_state;

	/*
	 * We cannot do real DMA.
	 */
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
	if (tgt->ior)
		fdma_map(&tgt->fdma, tgt->ior);
#endif	/*MACH_KERNEL*/

	copyout_gap16(tgt->cmd_ptr, tgt->dma_ptr, cmd_count);

	if ((tgt->cur_cmd == SCSI_CMD_WRITE) ||
	    (tgt->cur_cmd == SCSI_CMD_LONG_WRITE)){
		io_req_t	ior = tgt->ior;
		register int	len = ior->io_count;

		tgt->transient_state.out_count = len;

		if (len > PER_TGT_BUFF_SIZE)
			len = PER_TGT_BUFF_SIZE;
		copyout_gap16(	ior->io_data,
				tgt->dma_ptr + (cmd_count<<1),
				len);
		tgt->transient_state.copy_count = len;

		/* avoid leaks */
		if (len < tgt->block_size) {
			bzero_gap16(tgt->dma_ptr + ((cmd_count + len)<<1),
				    len - tgt->block_size);
			len = tgt->block_size;
			tgt->transient_state.copy_count = len;
		}

	} else {
		tgt->transient_state.out_count = 0;
		tgt->transient_state.copy_count = 0;
	}

	tgt->transient_state.cmd_count = cmd_count;
	tgt->transient_state.isa_oddbb = FALSE;

	disconn  = BGET(scsi_might_disconnect,tgt->masterno,tgt->target_id);
	disconn  = disconn && (sii->ntargets > 1);
	disconn |= BGET(scsi_should_disconnect,tgt->masterno,tgt->target_id);

	/*
	 * Setup target state
	 */
	tgt->done = SCSI_RET_IN_PROGRESS;

	handler = (disconn) ? sii_err_disconn : sii_err_generic;

	switch (tgt->cur_cmd) {
	    case SCSI_CMD_READ:
	    case SCSI_CMD_LONG_READ:
		LOG(0x13,"readop");
		scp = sii_script_data_in;
		break;
	    case SCSI_CMD_WRITE:
	    case SCSI_CMD_LONG_WRITE:
		LOG(0x14,"writeop");
		scp = sii_script_data_out;
		break;
	    case SCSI_CMD_INQUIRY:
		/* This is likely the first thing out:
		   do the synch neg if so */
		if (!cmd_only && ((tgt->flags&TGT_DID_SYNCH)==0)) {
			scp = sii_script_try_synch;
			tgt->flags |= TGT_TRY_SYNCH;
			break;
		}
	    case SCSI_CMD_REQUEST_SENSE:
	    case SCSI_CMD_MODE_SENSE:
	    case SCSI_CMD_RECEIVE_DIAG_RESULTS:
	    case SCSI_CMD_READ_CAPACITY:
	    case SCSI_CMD_READ_BLOCK_LIMITS:
	    case SCSI_CMD_READ_TOC:
	    case SCSI_CMD_READ_SUBCH:
	    case SCSI_CMD_READ_HEADER:
	    case 0xc4:	/* despised: SCSI_CMD_DEC_PLAYBACK_STATUS */
	    case 0xdd:	/* despised: SCSI_CMD_NEC_READ_SUBCH_Q */
	    case 0xde:	/* despised: SCSI_CMD_NEC_READ_TOC */
		scp = sii_script_data_in;
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    case SCSI_CMD_MODE_SELECT:
	    case SCSI_CMD_REASSIGN_BLOCKS:
	    case SCSI_CMD_FORMAT_UNIT:
	    case 0xc9: /* vendor-spec: SCSI_CMD_DEC_PLAYBACK_CONTROL */
		tgt->transient_state.cmd_count = sizeof_scsi_command(tgt->cur_cmd);
		tgt->transient_state.out_count =
			cmd_count - tgt->transient_state.cmd_count;
		scp = sii_script_data_out;
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    case SCSI_CMD_TEST_UNIT_READY:
		/*
		 * Do the synch negotiation here, unless done already
		 */
		if (tgt->flags & TGT_DID_SYNCH) {
			scp = sii_script_cmd;
		} else {
			scp = sii_script_try_synch;
			tgt->flags |= TGT_TRY_SYNCH;
		}
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    default:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		scp = sii_script_cmd;
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

	tgt->transient_state.dma_offset = 0;

	/*
	 * See if another target is currently selected on
	 * this SCSI bus, e.g. lock the sii structure.
	 * Note that it is the strategy routine's job
	 * to serialize ops on the same target as appropriate.
	 * XXX here and everywhere, locks!
	 */
	/*
	 * Protection viz reconnections makes it tricky.
	 */
/*	s = splbio();*/
	s = splhigh();

	if (sii->wd.nactive++ == 0)
		sii->wd.watchdog_state = SCSI_WD_ACTIVE;

	if (sii->state & SII_STATE_BUSY) {
		/*
		 * Queue up this target, note that this takes care
		 * of proper FIFO scheduling of the scsi-bus.
		 */
		LOG(3,"enqueue");
		LOG(0x80+tgt->target_id,0);
		enqueue_tail(&sii->waiting_targets, (queue_entry_t) tgt);
	} else {
		/*
		 * It is down to at most two contenders now,
		 * we will treat reconnections same as selections
		 * and let the scsi-bus arbitration process decide.
		 */
		sii->state |= SII_STATE_BUSY;
		sii->next_target = tgt;
		sii_attempt_selection(sii);
		/*
		 * Note that we might still lose arbitration..
		 */
	}
	splx(s);
}

sii_attempt_selection(sii)
	sii_softc_t	sii;
{
	target_info_t	*tgt;
	register int	out_count;
	sii_padded_regmap_t	*regs;
	register int	cmd;

	regs = sii->regs;
	tgt = sii->next_target;

	LOG(4,"select");
	LOG(0x80+tgt->target_id,0);

	/*
	 * Init bus state variables and set registers.
	 * [They are intermixed to avoid wbflush()es]
	 */
	sii->active_target = tgt;

	out_count = tgt->transient_state.cmd_count;

	/* set dma pointer and counter */
	regs->sii_dma_len     = out_count;
	regs->sii_dma_adr_low = SII_DMADR_LO(tgt->dma_ptr);
	regs->sii_dma_adr_hi  = SII_DMADR_HI(tgt->dma_ptr);

	sii->error_handler = tgt->transient_state.handler;

	regs->sii_sel_csr = tgt->target_id;

	sii->done = SCSI_RET_IN_PROGRESS;

	regs->sii_dma_ctrl = tgt->sync_offset;

	sii->cmd_count = out_count;

/*	if (regs->sii_conn_csr & (SII_CON_CON|SII_CON_DST))*/
	if (regs->sii_sc1 & (SII_CS1_BSY|SII_CS1_SEL))
		return;
	regs->sii_csr = SII_CSR_IE|SII_CSR_PCE|SII_CSR_SLE|SII_CSR_HPM;

	sii->script = tgt->transient_state.script;
	sii->in_count = 0;
	sii->out_count = 0;

	if (tgt->flags & TGT_DID_SYNCH) {
		if (tgt->transient_state.identify == 0xff)
			cmd =	SII_CMD_SEL;
		else {
			cmd =	SII_CMD_SEL | SII_CMD_ATN |
				SII_CMD_CON | SII_CMD_XFER | SCSI_PHASE_MSG_OUT;
			/* chain select and message out */
/*??*/			regs->sii_dma_1st_byte = tgt->transient_state.identify;
		}
	} else if (tgt->flags & TGT_TRY_SYNCH)
		cmd =	SII_CMD_SEL | SII_CMD_ATN;
	else
		cmd =	SII_CMD_SEL;

/*	if (regs->sii_conn_csr & (SII_CON_CON|SII_CON_DST)) { */
	if (regs->sii_sc1 & (SII_CS1_BSY|SII_CS1_SEL)) {
		/* let the reconnection attempt proceed */
		regs->sii_csr = SII_CSR_IE|SII_CSR_PCE|SII_CSR_SLE|
				SII_CSR_HPM|SII_CSR_RSE;
		sii->script = 0;
		LOG(0x8c,0);
	} else {
		regs->sii_cmd = cmd;
		wbflush();
	}
}

/*
 * Interrupt routine
 *	Take interrupts from the chip
 *
 * Implementation:
 *	Move along the current command's script if
 *	all is well, invoke error handler if not.
 */
boolean_t	sii_inside_sii_intr = FALSE;

sii_intr(unit,spllevel)
{
	register sii_softc_t	sii;
	register script_t	scp;
	register int		cs, ds;
	register sii_padded_regmap_t	*regs;
	boolean_t		try_match;
#ifdef	MACH_KERNEL
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface)
		return SII_intr(unit,spllevel);
#endif	/*MACH_KERNEL*/

	/* interrupt code is NOT reentrant */
	if (sii_inside_sii_intr) {
		LOG(0x22,"!!attempted to reenter sii_intr!!");
		return;
	}
	sii_inside_sii_intr = TRUE;

	LOG(5,"\n\tintr");

	sii = sii_softc[unit];

	/* collect status information */
	regs = sii->regs;
	cs = regs->sii_conn_csr;
	ds = regs->sii_data_csr;

TR(cs);
TR(ds);
TR(regs->sii_cmd);
TRCHECK;

	if (cs & SII_CON_RST){
		sii_bus_reset(sii);
		goto getout;
	}

	/* we got an interrupt allright */
	if (sii->active_target)
		sii->wd.watchdog_state = SCSI_WD_ACTIVE;

	/* rid of DONEs */
	if (ds & SII_DTR_DONE) {
		regs->sii_data_csr = SII_DTR_DONE;
		LOG(0x1e,"done");
		ds = regs->sii_data_csr;
		cs = regs->sii_conn_csr;
	}

	/* drop spurious calls, note that sometimes
	 * ds and cs get out-of-sync  */
	if (((cs & SII_CON_CI) | (ds & SII_DTR_DI)) == 0) {
		LOG(2,"SPURIOUS");
		goto getout;
	}

	/* clear interrupt flags */

	regs->sii_conn_csr = cs;
	regs->sii_data_csr = cs;

	/* drop priority */
	splx(spllevel);

	if ((sii->state & SII_STATE_TARGET) ||
	    (cs & SII_CON_TGT)) {
		sii_target_intr(sii,cs,ds);
		goto getout;
	}

	scp = sii->script;

	/* check who got the bus */
	if ((scp == 0) || (cs & SII_CON_LST)) {
		if (cs & SII_CON_DST) {
			sii_reconnect(sii, cs, ds);
			goto getout;
		}
		LOG(0x12,"no-script");
		goto getout;
	}

	if (SCRIPT_MATCH(cs,ds) != scp->condition) {
		if (try_match = (*sii->error_handler)(sii, cs, ds)) {
			cs = regs->sii_conn_csr;
			ds = regs->sii_data_csr;
		}
	} else
		try_match = TRUE;

	/* might have been side effected */
	scp = sii->script;

	if (try_match && (SCRIPT_MATCH(cs,ds) == scp->condition)) {
		/*
		 * Perform the appropriate operation,
		 * then proceed
		 */
		if ((*scp->action)(sii, cs, ds)) {
			/* might have been side effected */
			scp = sii->script;
			sii->script = scp + 1;
			regs->sii_cmd = scp->command;
			wbflush();
		}
	}
getout:
	sii_inside_sii_intr = FALSE;
}


sii_target_intr(sii)
	register sii_softc_t	sii;
{
	panic("SII: TARGET MODE !!!\n");
}

/*
 * All the many little things that the interrupt
 * routine might switch to
 */
boolean_t
sii_script_true(sii, cs, ds)
	register sii_softc_t	sii;

{
	SII_COMMAND(sii->regs,cs,ds,SII_CON_CON/*sanity*/);
	LOG(7,"nop");
	return TRUE;
}

boolean_t
sii_end_transaction( sii, cs, ds)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs = sii->regs;

	SII_COMMAND(sii->regs,cs,ds,0);

	LOG(0x1f,"end_t");

	regs->sii_csr &= ~SII_CSR_RSE;

	/* is the fifo really clean here ? */
	ds = sii_wait(&regs->sii_data_csr, SII_DTR_IBF,1);

	if (regs->sii_data != SCSI_COMMAND_COMPLETE)
		printf("{T%x}", regs->sii_data);

	regs->sii_cmd = SII_CMD_XFER | SII_CON_CON | SCSI_PHASE_MSG_IN |
			(cs & SII_CON_DST);
	wbflush();

	ds = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
	regs->sii_data_csr = SII_DTR_DONE;

	regs->sii_cmd = 0/*SII_PHASE_DISC*/;
	wbflush();

	cs = regs->sii_conn_csr;

	if ((cs & SII_CON_SCH) == 0)
		cs = sii_wait(&regs->sii_conn_csr, SII_CON_SCH,1);
	regs->sii_conn_csr = SII_CON_SCH;

	regs->sii_csr |= SII_CSR_RSE;

	cs = regs->sii_conn_csr;

	if (!sii_end(sii, cs, ds))
		(void) sii_reconnect(sii, cs, ds);
	return FALSE;
}

boolean_t
sii_end( sii, cs, ds)
	register sii_softc_t	sii;
{
	register target_info_t	*tgt;
	register io_req_t	ior;
	register sii_padded_regmap_t	*regs = sii->regs;

	LOG(6,"end");

	tgt = sii->active_target;

	if ((tgt->done = sii->done) == SCSI_RET_IN_PROGRESS)
		tgt->done = SCSI_RET_SUCCESS;

	sii->script = 0;

	if (sii->wd.nactive-- == 1)
		sii->wd.watchdog_state = SCSI_WD_INACTIVE;

	/* check reconnection not pending */
	cs = regs->sii_conn_csr;
	if ((cs & SII_CON_DST) == 0)
		sii_release_bus(sii);
	else {
		sii->active_target = 0;
/*		sii->state &= ~SII_STATE_BUSY; later */
	}

	if (ior = tgt->ior) {
		LOG(0xA,"ops->restart");
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_unmap(&tgt->fdma, ior);
#endif	/*MACH_KERNEL*/
		(*tgt->dev_ops->restart)(tgt, TRUE);
		if (cs & SII_CON_DST)
			sii->state &= ~SII_STATE_BUSY;
	}

	return ((cs & SII_CON_DST) == 0);
}

boolean_t
sii_release_bus(sii)
	register sii_softc_t	sii;
{
	boolean_t	ret = FALSE;

	LOG(9,"release");

	sii->script = 0;

	if (sii->state & SII_STATE_COLLISION) {

		LOG(0xB,"collided");
		sii->state &= ~SII_STATE_COLLISION;
		sii_attempt_selection(sii);

	} else if (queue_empty(&sii->waiting_targets)) {

		sii->state &= ~SII_STATE_BUSY;
		sii->active_target = 0;
		ret = TRUE;

	} else {

		LOG(0xC,"dequeue");
		sii->next_target = (target_info_t *)
				dequeue_head(&sii->waiting_targets);
		sii_attempt_selection(sii);
	}
	return ret;
}

boolean_t
sii_get_status( sii, cs, ds)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs = sii->regs;
	register scsi2_status_byte_t status;
	register target_info_t	*tgt;
	unsigned int		len;
	unsigned short		cmd;

	LOG(0xD,"get_status");
TRWRAP;

	sii->state &= ~SII_STATE_DMA_IN;

	tgt = sii->active_target;
	sii->error_handler = tgt->transient_state.handler;

	if (len = sii->in_count) {
		if ((tgt->cur_cmd != SCSI_CMD_READ) &&
		    (tgt->cur_cmd != SCSI_CMD_LONG_READ)){
			len -= regs->sii_dma_len;
			copyin_gap16(tgt->dma_ptr, tgt->cmd_ptr, len);
			if (len & 0x1) /* odd byte, left in silo */
				tgt->cmd_ptr[len - 1] = regs->sii_data;
		} else {
			if (regs->sii_dma_len) {
#if 0
				this is incorrect and besides..
				tgt->ior->io_residual = regs->sii_dma_len;
#endif
				len -= regs->sii_dma_len;
			}
			careful_copyin_gap16(	tgt, tgt->transient_state.dma_offset,
						len, ds & SII_DTR_OBB,
						regs->sii_dma_1st_byte);
		}
		sii->in_count = 0;
	}

	len = regs->sii_dma_len;
	regs->sii_dma_len = 0;/*later?*/

	/* if dma is still in progress we have to quiet it down */
	cmd = regs->sii_cmd;
	if (cmd & SII_CMD_DMA) {
		regs->sii_cmd = cmd & ~(SII_CMD_DMA|SII_CMD_XFER);
		wbflush();
		/* DONE might NOT pop up. Sigh. */
		delay(10);
		regs->sii_data_csr = regs->sii_data_csr;
	}

	regs->sii_cmd = SCSI_PHASE_STATUS|SII_CON_CON|(cs & SII_CON_DST);
	wbflush();

	ds = sii_wait(&regs->sii_data_csr, SII_DTR_IBF,1);
	status.bits = regs->sii_data;

	if (status.st.scsi_status_code != SCSI_ST_GOOD) {
		scsi_error(sii->active_target, SCSI_ERR_STATUS, status.bits, 0);
		sii->done = (status.st.scsi_status_code == SCSI_ST_BUSY) ?
			SCSI_RET_RETRY : SCSI_RET_NEED_SENSE;
	} else
		sii->done = SCSI_RET_SUCCESS;

	return TRUE;
}

boolean_t
sii_dma_in( sii, cs, ds)
	register sii_softc_t	sii;
{
	register target_info_t	*tgt;
	register sii_padded_regmap_t	*regs = sii->regs;
	char			*dma_ptr;
	register int		count;
	boolean_t		advance_script = TRUE;

	SII_COMMAND(regs,cs,ds,0);
	LOG(0xE,"dma_in");

	tgt = sii->active_target;
	sii->error_handler = tgt->transient_state.handler;
	sii->state |= SII_STATE_DMA_IN;

	if (sii->in_count == 0) {
		/*
		 * Got nothing yet: either just sent the command
		 * or just reconnected
		 */
		register int avail;

		if (tgt->transient_state.isa_oddbb) {
			regs->sii_dma_1st_byte = tgt->transient_state.oddbb;
			tgt->transient_state.isa_oddbb = FALSE;
		}

		count = tgt->transient_state.in_count;
		count = u_min(count, (SII_DMA_COUNT_MASK+1));
		avail = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
		count = u_min(count, avail);

		/* common case of 8k-or-less read ? */
		advance_script = (tgt->transient_state.in_count == count);

	} else {

		/*
		 * We received some data.
		 * Also, take care of bogus interrupts
		 */
		register int offset, xferred;
		unsigned char	obb = regs->sii_data;

		xferred = sii->in_count - regs->sii_dma_len;
		assert(xferred > 0);
		tgt->transient_state.in_count -= xferred;
		assert(tgt->transient_state.in_count > 0);
		offset = tgt->transient_state.dma_offset;
		tgt->transient_state.dma_offset += xferred;
		count = u_min(tgt->transient_state.in_count, (SII_DMA_COUNT_MASK+1));
		if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE) {
			tgt->transient_state.dma_offset = 0;
		} else {
			register int avail;
			avail = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
			count = u_min(count, avail);
		}

		/* get some more */
		dma_ptr = tgt->dma_ptr + (tgt->transient_state.dma_offset << 1);
		sii->in_count = count;
		regs->sii_dma_len = count;
		regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
		regs->sii_dma_adr_hi  = SII_DMADR_HI(dma_ptr);
		wbflush();
		regs->sii_cmd = sii->script->command;
		wbflush();

		/* copy what we got */
		careful_copyin_gap16( tgt, offset, xferred, ds & SII_DTR_OBB, obb);

		/* last chunk ? */
		if (count == tgt->transient_state.in_count) {
			sii->script++;
		}
		return FALSE;
	}
quickie:
	sii->in_count = count;
	dma_ptr = tgt->dma_ptr + (tgt->transient_state.dma_offset << 1);
	regs->sii_dma_len     = count;
	regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
	regs->sii_dma_adr_hi  = SII_DMADR_HI(dma_ptr);
	wbflush();

	if (!advance_script) {
		regs->sii_cmd = sii->script->command;
		wbflush();
	}
	return advance_script;
}

/* send data to target. Called in three different ways:
   (a) to start transfer (b) to restart a bigger-than-8k
   transfer (c) after reconnection
 */
boolean_t
sii_dma_out( sii, cs, ds)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs = sii->regs;
	register char		*dma_ptr;
	register target_info_t	*tgt;
	boolean_t		advance_script = TRUE;
	int			count = sii->out_count;

	SII_COMMAND(regs,cs,ds,0);
	LOG(0xF,"dma_out");

	tgt = sii->active_target;
	sii->error_handler = tgt->transient_state.handler;
	sii->state &= ~SII_STATE_DMA_IN;

	if (sii->out_count == 0) {
		/*
		 * Nothing committed: either just sent the
		 * command or reconnected
		 */
		register int remains;

		count = tgt->transient_state.out_count;
		count = u_min(count, (SII_DMA_COUNT_MASK+1));
		remains = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
		count = u_min(count, remains);

		/* common case of 8k-or-less write ? */
		advance_script = (tgt->transient_state.out_count == count);
	} else {
		/*
		 * We sent some data.
		 * Also, take care of bogus interrupts
		 */
		register int offset, xferred;

		xferred = sii->out_count - regs->sii_dma_len;
		assert(xferred > 0);
		tgt->transient_state.out_count -= xferred;
		assert(tgt->transient_state.out_count > 0);
		offset = tgt->transient_state.dma_offset;
		tgt->transient_state.dma_offset += xferred;
		count = u_min(tgt->transient_state.out_count, (SII_DMA_COUNT_MASK+1));
		if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE) {
			tgt->transient_state.dma_offset = 0;
		} else {
			register int remains;
			remains = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
			count = u_min(count, remains);
		}
		/* last chunk ? */
		if (tgt->transient_state.out_count == count)
			goto quickie;

		/* ship some more */
		dma_ptr = tgt->dma_ptr +
			((tgt->transient_state.cmd_count + tgt->transient_state.dma_offset) << 1);
		sii->out_count = count;
		regs->sii_dma_len = count;
		regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
		regs->sii_dma_adr_hi  = SII_DMADR_HI(dma_ptr);
		wbflush();
		regs->sii_cmd = sii->script->command;

		/* copy some more data */
		careful_copyout_gap16(tgt, offset, xferred);
		return FALSE;
	}

quickie:
	sii->out_count = count;
	dma_ptr = tgt->dma_ptr +
		((tgt->transient_state.cmd_count + tgt->transient_state.dma_offset) << 1);
	regs->sii_dma_len = count;
	regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
	regs->sii_dma_adr_hi  = SII_DMADR_HI(dma_ptr);
	wbflush();

	if (!advance_script) {
		regs->sii_cmd = sii->script->command;
		wbflush();
	}
	return advance_script;
}

/* disconnect-reconnect ops */

/* get the message in via dma */
boolean_t
sii_msg_in(sii, cs, ds)
	register sii_softc_t	sii;
	register unsigned char	cs, ds;
{
	register target_info_t	*tgt;
	char			*dma_ptr;
	register sii_padded_regmap_t	*regs = sii->regs;

	LOG(0x15,"msg_in");

	tgt = sii->active_target;

	dma_ptr = tgt->dma_ptr;
	/* We would clobber the data for READs */
	if (sii->state & SII_STATE_DMA_IN) {
		register int offset;
		offset = tgt->transient_state.cmd_count + tgt->transient_state.dma_offset;
		if (offset & 1) offset++;
		dma_ptr += (offset << 1);
	}

	regs->sii_dma_adr_low = SII_DMADR_LO(dma_ptr);
	regs->sii_dma_adr_hi = SII_DMADR_HI(dma_ptr);
	/* We only really expect two bytes */
	regs->sii_dma_len = sizeof(scsi_command_group_0);
	wbflush();

	return TRUE;
}

/* check the message is indeed a DISCONNECT */
boolean_t
sii_disconnect(sii, cs, ds)
	register sii_softc_t	sii;
	register unsigned char	cs, ds;

{
	register target_info_t	*tgt;
	register int		len;
	boolean_t		ok = FALSE;
	unsigned int		dmsg = 0;

	tgt = sii->active_target;

	len = sizeof(scsi_command_group_0) - sii->regs->sii_dma_len;
	PRINT(("{G%d}",len));

/*	if (len == 0) ok = FALSE; */
	if (len == 1) {
		dmsg = sii->regs->sii_dma_1st_byte;
		ok = (dmsg == SCSI_DISCONNECT);
	} else if (len == 2) {
		register char		*msgs;
		register unsigned int	offset;
		register sii_padded_regmap_t	*regs = sii->regs;

		/* wherever it was, take it from there */
		offset = regs->sii_dma_adr_low | ((regs->sii_dma_adr_hi&3)<<16);
		msgs = (char*)sii->buff + (offset << 1);
		dmsg = *((unsigned short *)msgs);

		/* A SDP message preceeds it in non-completed READs */
		ok = (((dmsg & 0xff) == SCSI_DISCONNECT) ||
		      (dmsg == ((SCSI_DISCONNECT<<8)|SCSI_SAVE_DATA_POINTER)));
	}
	if (!ok)
		printf("[tgt %d bad msg (%d): %x]", tgt->target_id, len, dmsg);

	return TRUE;
}

/* save all relevant data, free the BUS */
boolean_t
sii_disconnected(sii, cs, ds)
	register sii_softc_t	sii;
	register unsigned char	cs, ds;

{
	register target_info_t	*tgt;

	SII_COMMAND(sii->regs,cs,ds,0);

	sii->regs->sii_csr = SII_CSR_IE|SII_CSR_PCE|SII_CSR_SLE|
			SII_CSR_HPM|SII_CSR_RSE;

	LOG(0x16,"disconnected");

	sii_disconnect(sii,cs,ds);

	tgt = sii->active_target;
	tgt->flags |= TGT_DISCONNECTED;
	tgt->transient_state.handler = sii->error_handler;
	/* the rest has been saved in sii_err_disconn() */

	PRINT(("{D%d}", tgt->target_id));

	sii_release_bus(sii);

	return FALSE;
}

/* get reconnect message, restore BUS */
boolean_t
sii_reconnect(sii, cs, ds)
	register sii_softc_t	sii;
	register unsigned char	cs, ds;

{
	register target_info_t	*tgt;
	sii_padded_regmap_t		*regs;
	int			id;

	regs = sii->regs;
	regs->sii_conn_csr = SII_CON_SCH;
	regs->sii_cmd = SII_CON_CON|SII_CON_DST|SCSI_PHASE_MSG_IN;
	wbflush();

	LOG(0x17,"reconnect");

	/*
	 * See if this reconnection collided with a selection attempt
	 */
	if (sii->state & SII_STATE_BUSY)
		sii->state |= SII_STATE_COLLISION;

	sii->state |= SII_STATE_BUSY;

	cs = regs->sii_conn_csr;

	/* tk50s are slow */
	if ((cs & SII_CON_CON) == 0)
		cs = sii_wait(&regs->sii_conn_csr, SII_CON_CON,1);

	/* ?? */
	if (regs->sii_conn_csr & SII_CON_BERR)
		regs->sii_conn_csr = SII_CON_BERR;

	if ((ds & SII_DTR_IBF) == 0)
		ds = sii_wait(&regs->sii_data_csr, SII_DTR_IBF,1);

	if (regs->sii_data != SCSI_IDENTIFY)
		printf("{I%x %x}", regs->sii_data, regs->sii_dma_1st_byte);

	/* find tgt: id is in sii_destat */
	id = regs->sii_destat;

	tgt = sii->sc->target[id];
	if (id > 7 || tgt == 0) panic("sii_reconnect");

	PRINT(("{R%d}", id));
	if (sii->state & SII_STATE_COLLISION)
		PRINT(("[B %d-%d]", sii->active_target->target_id, id));

	LOG(0x80+id,0);

	sii->active_target = tgt;
	tgt->flags &= ~TGT_DISCONNECTED;

	/* synch params */
	regs->sii_dma_ctrl = tgt->sync_offset;
	regs->sii_dma_len  = 0;

	sii->script = tgt->transient_state.script;
	sii->error_handler = sii_err_rdp;
	sii->in_count = 0;
	sii->out_count = 0;

	regs->sii_cmd = SII_CMD_XFER|SII_CMD_CON|SII_CMD_DST|SCSI_PHASE_MSG_IN;
	wbflush();

	(void) sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
	regs->sii_data_csr = SII_DTR_DONE;

	return TRUE;
}


/* do the synch negotiation */
boolean_t
sii_dosynch( sii, cs, ds)
	register sii_softc_t	sii;
{
	/*
	 * Phase is MSG_OUT here, cmd has not been xferred
	 */
	int			*p, len;
	unsigned short		dmalo, dmahi, dmalen;
	register target_info_t	*tgt;
	register sii_padded_regmap_t	*regs = sii->regs;
	unsigned char		off;

	regs->sii_cmd = SCSI_PHASE_MSG_OUT|SII_CMD_ATN|SII_CON_CON;
	wbflush();

	LOG(0x11,"dosync");

	tgt = sii->active_target;

	tgt->flags |= TGT_DID_SYNCH;	/* only one chance */
	tgt->flags &= ~TGT_TRY_SYNCH;

	p = (int*) (tgt->dma_ptr + (((regs->sii_dma_len<<1) + 2) & ~3));
	p[0] = SCSI_IDENTIFY | (SCSI_EXTENDED_MESSAGE<<8);
	p[1] = 3 | (SCSI_SYNC_XFER_REQUEST<<8);
	if (BGET(scsi_no_synchronous_xfer,tgt->masterno,tgt->target_id))
		off = 0;
	else
		off = sii_max_offset;
	/* but we'll ship "off" manually */
	p[2] =  sii_to_scsi_period(sii_min_period) |(off << 8);

	dmalen = regs->sii_dma_len;
	dmalo  = regs->sii_dma_adr_low;
	dmahi  = regs->sii_dma_adr_hi;
	regs->sii_dma_len = sizeof(scsi_synch_xfer_req_t) /* + 1 */;
	regs->sii_dma_adr_low = SII_DMADR_LO(p);
	regs->sii_dma_adr_hi = SII_DMADR_HI(p);
	wbflush();

	regs->sii_cmd = SII_CMD_DMA|SII_CMD_XFER|SII_CMD_ATN|
			    SII_CON_CON|SCSI_PHASE_MSG_OUT;
	wbflush();

	/* wait for either DONE or MIS */
	ds = sii_wait(&regs->sii_data_csr, SII_DTR_DI,1);

	/* TK50s do not like xtended messages */
	/* and some others just ignore the standard */
	if (SCSI_PHASE(ds) != SCSI_PHASE_MSG_OUT) {
		/* disentangle FIFO */
		regs->sii_cmd = SII_CON_CON|SCSI_PHASE_MSG_OUT;
		ds = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
		if (SCSI_PHASE(ds) == SCSI_PHASE_MSG_IN)
			goto msgin;
		goto got_answer;			
	}

	/* ack and stop dma */
	regs->sii_cmd = SII_CON_CON|SCSI_PHASE_MSG_OUT|SII_CMD_ATN;
	wbflush();
	ds = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
	regs->sii_data_csr = SII_DTR_DONE;
	wbflush();

	/* last byte of message */
	regs->sii_data = off;
	wbflush();
	regs->sii_cmd = SII_CMD_XFER|SII_CON_CON|SCSI_PHASE_MSG_OUT;
	wbflush();

	/* Race here: who will interrupt first, the DMA
	   controller or the status watching machine ? */
	delay(1000);
	regs->sii_cmd = SII_CON_CON|SCSI_PHASE_MSG_OUT;
	wbflush();

	ds = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
	regs->sii_data_csr = SII_DTR_DONE;

	/* The standard sez there nothing else the target can do but.. */
	ds = sii_wait(&regs->sii_data_csr, SCSI_PHASE_MSG_IN,0);

	/* Of course, what are standards for ? */
	if (SCSI_PHASE(ds) == SCSI_PHASE_CMD)
		goto cmdp;
msgin:
	/* ack */
	regs->sii_cmd = SII_CON_CON|SCSI_PHASE_MSG_IN;
	wbflush();

	/* set up dma to receive answer */
	regs->sii_dma_adr_low  = SII_DMADR_LO(p);
	regs->sii_dma_adr_hi = SII_DMADR_HI(p);
	regs->sii_dma_len = sizeof(scsi_synch_xfer_req_t);
	wbflush();
	regs->sii_cmd = SII_CMD_DMA|SII_CMD_XFER|SII_CON_CON|SCSI_PHASE_MSG_IN;
	wbflush();

	/* wait for the answer, and look at it */
	ds = sii_wait(&regs->sii_data_csr, SII_DTR_MIS,1);

	regs->sii_cmd = SII_CON_CON|SCSI_PHASE_MSG_IN;
	wbflush();
	ds = sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);

got_answer:
	/* do not cancel the phase mismatch */
	regs->sii_data_csr = SII_DTR_DONE;

	if (regs->sii_dma_len || ((p[0] & 0xff) != SCSI_EXTENDED_MESSAGE)) {
		/* did not like it */
		printf(" did not like SYNCH xfer ");
	} else {
		/* will do synch */
		tgt->sync_period = scsi_period_to_sii((p[1]>>8)&0xff);
		tgt->sync_offset = regs->sii_data;	/* odd xfer, in silo */
		/* sanity */
		if (tgt->sync_offset > sii_max_offset)
			tgt->sync_offset = sii_max_offset;
		regs->sii_dma_ctrl = tgt->sync_offset;
	}

cmdp:
	/* phase should be command now */
	regs->sii_dma_len = dmalen;
	regs->sii_dma_adr_low = dmalo;
	regs->sii_dma_adr_hi = dmahi;
	wbflush();

	/* continue with simple command script */
	sii->error_handler = sii_err_generic;

	sii->script = (tgt->cur_cmd == SCSI_CMD_INQUIRY) ?
			sii_script_data_in : sii_script_cmd;
	if (SCSI_PHASE(ds) == SCSI_PHASE_CMD )
		return TRUE;

	sii->script++;
	if (SCSI_PHASE(ds) == SCSI_PHASE_STATUS )
		return TRUE;

	sii->script++; /* msgin? */
	sii->script++;
	if (SCSI_PHASE(ds) == SII_PHASE_DISC)
		return TRUE;

gimmeabreak(); 
	panic("sii_dosynch");
	return FALSE;
}

/*
 * The bus was reset
 */
sii_bus_reset(sii)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs = sii->regs;

	LOG(0x21,"bus_reset");

	/*
	 * Clear interrupt bits
	 */
	regs->sii_conn_csr = 0xffff;
	regs->sii_data_csr = 0xffff;

	/*
	 * Clear bus descriptor
	 */
	sii->script = 0;
	sii->error_handler = 0;
	sii->active_target = 0;
	sii->next_target = 0;
	sii->state = 0;
	queue_init(&sii->waiting_targets);
	sii->wd.nactive = 0;
	sii_reset(regs, TRUE);

	log(LOG_KERN, "sii: (%d) bus reset ", ++sii->wd.reset_count);
	delay(scsi_delay_after_reset); /* some targets take long to reset */

	if (sii->sc == 0)	/* sanity */
		return;

	sii_inside_sii_intr = FALSE;

	scsi_bus_was_reset(sii->sc);
}

/*
 * Error handlers
 */

/*
 * Generic, default handler
 */
boolean_t
sii_err_generic(sii, cs, ds)
	register sii_softc_t	sii;
{
	register int		cond = sii->script->condition;

	LOG(0x10,"err_generic");

	/*
	 * Note to DEC hardware people.
	 * Dropping the notion of interrupting on
	 * DMA completions (or at least make it optional)
	 * would save TWO interrupts out of the SEVEN that
	 * are currently requested for a non-disconnecting
	 * READ or WRITE operation.
	 */
	if (ds & SII_DTR_DONE)
		return TRUE;

	/* this is a band-aid */
	if ((SCSI_PHASE(cond) == SII_PHASE_DISC) &&
	    (cs & SII_CON_SCH)) {
		ds &= ~7;
		ds |= SII_PHASE_DISC;
		(void) (*sii->script->action)(sii,cs,ds);
		return FALSE;
	}

	/* TK50s are slow to connect, forgive em */
	if ((SCSI_PHASE(ds) == SCSI_PHASE_MSG_OUT) ||
	    (SCSI_PHASE(cond) == SCSI_PHASE_MSG_OUT))
		return TRUE;
	if ((SCSI_PHASE(cond) == SCSI_PHASE_CMD) &&
	    ((SCSI_PHASE(ds) == 0) || (SCSI_PHASE(ds) == 4) || (SCSI_PHASE(ds) == 5)))
		return TRUE;

	/* transition to status ? */
	if (SCSI_PHASE(ds) == SCSI_PHASE_STATUS)
		return sii_err_to_status(sii, cs, ds);

	return sii_err_phase_mismatch(sii,cs,ds);
}

/*
 * Handle generic errors that are reported as
 * an unexpected change to STATUS phase
 */
sii_err_to_status(sii, cs, ds)
	register sii_softc_t	sii;
{
	script_t		scp = sii->script;

	LOG(0x20,"err_tostatus");
	while (SCSI_PHASE(scp->condition) != SCSI_PHASE_STATUS)
		scp++;
	sii->script = scp;
#if 0
	/*
	 * Normally, we would already be able to say the command
	 * is in error, e.g. the tape had a filemark or something.
	 * But in case we do disconnected mode WRITEs, it is quite
	 * common that the following happens:
	 *	dma_out -> disconnect -> reconnect
	 * and our script might expect at this point that the dma
	 * had to be restarted (it didn't know it was completed
	 * because the tape record is shorter than we asked for).
	 * And in any event.. it is both correct and cleaner to
	 * declare error iff the STATUS byte says so.
	 */
	sii->done = SCSI_RET_NEED_SENSE;
#endif
	return TRUE;
}

/*
 * Watch for a disconnection
 */
boolean_t
sii_err_disconn(sii, cs, ds)
	register sii_softc_t	sii;
	register unsigned	cs, ds;
{
	register sii_padded_regmap_t	*regs;
	register target_info_t	*tgt;
	int			count;
	int			from;
	unsigned char		obb;
	int			delayed_copy = 0;

	LOG(0x18,"err_disconn");

	if (SCSI_PHASE(ds) != SCSI_PHASE_MSG_IN)
		return sii_err_generic(sii, cs, ds);

	regs = sii->regs;

	if ((regs->sii_cmd & (SII_CMD_DMA|SII_CMD_XFER)) ==
				(SII_CMD_DMA|SII_CMD_XFER)) {
		/* stop dma and wait */
		regs->sii_cmd &= ~(SII_CMD_DMA|SII_CMD_XFER);
		(void) sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
/* later:	regs->sii_data_csr = SII_DTR_DONE; */
	}

	SII_COMMAND(regs,cs,ds,0);

	tgt = sii->active_target;
	switch (SCSI_PHASE(sii->script->condition)) {
	case SCSI_PHASE_DATAO:
		LOG(0x1b,"+DATAO");
		if (sii->out_count) {
			register int xferred, offset;

			xferred = sii->out_count - regs->sii_dma_len;
			tgt->transient_state.out_count -= xferred;
			assert(tgt->transient_state.out_count > 0);
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			delayed_copy = 1;
			from = offset;
			count = xferred;

		}
		tgt->transient_state.script = sii_script_restart_data_out;
		break;

	case SCSI_PHASE_DATAI:
		LOG(0x19,"+DATAI");
		if (sii->in_count) {
			register int offset, xferred;

			obb = regs->sii_dma_1st_byte;

			xferred = sii->in_count - regs->sii_dma_len;
			assert(xferred > 0);
			if (ds & SII_DTR_OBB) {
				tgt->transient_state.isa_oddbb = TRUE;
				tgt->transient_state.oddbb = obb;
			}
			tgt->transient_state.in_count -= xferred;
			assert(tgt->transient_state.in_count > 0);
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			/* copy what we got */

			delayed_copy = 2;
			from = offset;
			count = xferred;

		}
		tgt->transient_state.script = sii_script_restart_data_in;
		break;

	case SCSI_PHASE_STATUS:
		/* will have to restart dma */
		if (count = regs->sii_dma_len) {
			(void) sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
			regs->sii_data_csr = SII_DTR_DONE;
		}
		if (sii->state & SII_STATE_DMA_IN) {
			register int offset, xferred;

			obb = regs->sii_dma_1st_byte;

			LOG(0x1a,"+STATUS+R");

			xferred = sii->in_count - count;
			assert(xferred > 0);
			if (ds & SII_DTR_OBB) {
				tgt->transient_state.isa_oddbb = TRUE;
				tgt->transient_state.oddbb = obb;
			}
			tgt->transient_state.in_count -= xferred;
/*			assert(tgt->transient_state.in_count > 0);*/
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			/* copy what we got */

			delayed_copy = 2;
			from = offset;
			count = xferred;

			tgt->transient_state.script = sii_script_restart_data_in;
			if (tgt->transient_state.in_count == 0)
				tgt->transient_state.script++;

		} else {

			LOG(0x1d,"+STATUS+W");

			if ((count == 0) && (tgt->transient_state.out_count == sii->out_count)) {
				/* all done */
				tgt->transient_state.script = &sii_script_restart_data_out[1];
				tgt->transient_state.out_count = 0;
			} else {
				register int xferred, offset;

				/* how much we xferred */
				xferred = sii->out_count - count;

				tgt->transient_state.out_count -= xferred;
				assert(tgt->transient_state.out_count > 0);
				offset = tgt->transient_state.dma_offset;
				tgt->transient_state.dma_offset += xferred;
				if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE)
					tgt->transient_state.dma_offset = 0;

				delayed_copy = 1;
				from = offset;
				count = xferred;

				tgt->transient_state.script = sii_script_restart_data_out;
			}
			sii->out_count = 0;
		}
		break;
	case SII_PHASE_DISC: /* sometimes disconnects and phase remains */
		return sii_err_generic(sii, cs, ds);
	default:
		gimmeabreak();
	}
	regs->sii_csr &= ~SII_CSR_RSE;
	sii_msg_in(sii,cs,ds);
	sii->script = sii_script_disconnect;
	regs->sii_cmd = SII_CMD_DMA|SII_CMD_XFER|SCSI_PHASE_MSG_IN|
			SII_CON_CON|(regs->sii_conn_csr & SII_CON_DST);
	wbflush();
	if (delayed_copy == 2)
		careful_copyin_gap16( tgt, from, count, ds & SII_DTR_OBB, obb);
	else if (delayed_copy == 1)
		careful_copyout_gap16( tgt, from, count);

	return FALSE;
}

/*
 * Suppose someone reads the specs as they read the Bible.
 * They would send these unnecessary restore-pointer msgs
 * in reconnect phases.  If this was a SCSI-2 modify-pointer
 * I could understand, but.  Oh well.
 */
sii_err_rdp(sii, cs, ds)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs;
	register target_info_t	*tgt;

	LOG(0x24,"err_drp");
	
	/* One chance */
	sii->error_handler = sii->active_target->transient_state.handler;

	if (SCSI_PHASE(ds) != SCSI_PHASE_MSG_IN)
		return sii_err_generic(sii, cs, ds);

	regs = sii->regs;

	if ((ds & SII_DTR_IBF) == 0)
		ds = sii_wait(&regs->sii_data_csr, SII_DTR_IBF,1);

	if (regs->sii_data != SCSI_RESTORE_POINTERS)
		return sii_err_disconn(sii, cs, ds);

	regs->sii_cmd = SII_CMD_XFER|SII_CMD_CON|SII_CMD_DST|SCSI_PHASE_MSG_IN;
	wbflush();

	(void) sii_wait(&regs->sii_data_csr, SII_DTR_DONE,1);
	regs->sii_data_csr = SII_DTR_DONE;

	return FALSE;
}

/*
 * Handle strange, yet unexplained interrupts and error
 * situations which eventually I will be old and wise
 * enough to deal with properly with preventive care.
 */
sii_err_phase_mismatch(sii, cs, ds)
	register sii_softc_t	sii;
{
	register sii_padded_regmap_t	*regs = sii->regs;
	register int		match;

	LOG(0x23,"err_mismatch");

	match = SCSI_PHASE(sii->script->condition);

	/* dmain interrupted */
	if ((match == SCSI_PHASE_STATUS) && (SCSI_PHASE(ds) == SCSI_PHASE_DATAI)) {
		register int xferred;
		register char *p;

		if (regs->sii_dma_len <= 1) {
/*if (scsi_debug)*/
printf("[DMAINZERO %x %x %x]", cs, ds, regs->sii_dma_len);
			if (regs->sii_dma_len == 0) {
				regs->sii_dma_len = sii->in_count;
				wbflush();
				regs->sii_cmd = sii->script[-1].command;
			}
			return FALSE;
		}

		/* This happens when you do not "init" the prom
		   and the fifo is screwed up */
		xferred = sii->in_count - regs->sii_dma_len;
		p = (char*)( regs->sii_dma_adr_low | ((regs->sii_dma_adr_hi&3)<<16) );
		p += xferred;
if (scsi_debug)
printf("[DMAIN %x %x %x]", cs, ds, xferred);
		/* odd bytes are not xferred */
		if (((unsigned)p) & 0x1){
			register short	*oddb;
			oddb = (short*)(sii->buff) + ((unsigned)p-1);/*shifts*/
			*oddb = regs->sii_dma_1st_byte;
		}
		regs->sii_dma_adr_low  = ((unsigned)p);
		regs->sii_dma_adr_hi = ((unsigned)p) << 16;
		wbflush();
		regs->sii_cmd = sii->script[-1].command;
		wbflush();
		return FALSE;
	} else
	/* dmaout interrupted */
	if ((match == SCSI_PHASE_STATUS) && (SCSI_PHASE(ds) == SCSI_PHASE_DATAO)) {
		register int xferred;
		register char *p;

		if (regs->sii_dma_len <= 1) {
/*if (scsi_debug)*/
printf("[DMAOUTZERO %x %x %x]", cs, ds, regs->sii_dma_len);
gimmeabreak();
			if (regs->sii_dma_len == 0) {
				regs->sii_dma_len = sii->out_count;
				wbflush();
				regs->sii_cmd = sii->script[-1].command;
			}
			return FALSE;
		}

		xferred = sii->out_count - regs->sii_dma_len;
/*if (scsi_debug)*/
printf("[DMAOUT %x %x %x %x]", cs, ds, regs->sii_dma_len, sii->out_count);
		sii->out_count -= xferred;
		p = (char*)( regs->sii_dma_adr_low | ((regs->sii_dma_adr_hi&3)<<16) );
		p += xferred;
		regs->sii_dma_adr_low  = ((unsigned)p);
		regs->sii_dma_adr_hi = ((unsigned)p) << 16;
		wbflush();
		regs->sii_cmd = sii->script[-1].command;
		wbflush();
		return FALSE;
	}
#if 1 /* ?? */
	/* stuck in cmd phase */
	else if ((SCSI_PHASE(ds) == SCSI_PHASE_CMD) &&
		 ((match == SCSI_PHASE_DATAI) || (match == SCSI_PHASE_DATAO))) {
/*if (scsi_debug)*/
printf("[CMD %x %x %x %x]",  cs, ds, sii->cmd_count, regs->sii_dma_len);
		if (regs->sii_dma_len != 0) {
			/* ouch, this hurts */
			register int xferred;
			register char *p;

			xferred = sii->cmd_count - regs->sii_dma_len;
			sii->cmd_count -= xferred;
			p = (char*)( regs->sii_dma_adr_low | ((regs->sii_dma_adr_hi&3)<<16) );
			p += xferred;
			regs->sii_dma_adr_low  = ((unsigned)p);
			regs->sii_dma_adr_hi = ((unsigned)p) << 16;
			wbflush();
			regs->sii_cmd = 0x8842;
			wbflush();
			return FALSE;;

		}
		SII_ACK(regs,cs,ds,0/*match*/);
		wbflush();
		return FALSE;;
	}
#endif
	else {
		printf("{D%x %x}", cs, ds);
/*	if (scsi_debug)*/ gimmeabreak();
	}
	return FALSE;
}

/*
 * Watchdog
 *
 * There are two ways in which I have seen the chip
 * get stuck: a target never reconnected, or the
 * selection deadlocked.  Both cases involved a tk50,
 * but elsewhere it showed up with hitachi disks too.
 */
sii_reset_scsibus(sii)
	register sii_softc_t	sii;
{
	register target_info_t	*tgt = sii->active_target;
	register sii_padded_regmap_t	*regs = sii->regs;

	/* see if SIP still --> device down or non-existant */
	if ((regs->sii_conn_csr & (SII_CON_LST|SII_CON_SIP)) == SII_CON_SIP){
		if (tgt) {
			log(LOG_KERN, "Target %d went offline\n",
				tgt->target_id);
			tgt->flags = 0;
			return sii_probe_timeout(tgt);
		}
		/* else fall through */
	}

	if (tgt)
		log(LOG_KERN, "Target %d was active, cmd x%x in x%x out x%x Sin x%x Sou x%x dmalen x%x\n", 
			tgt->target_id, tgt->cur_cmd,
			tgt->transient_state.in_count, tgt->transient_state.out_count,
			sii->in_count, sii->out_count,
			sii->regs->sii_dma_len);

	sii->regs->sii_cmd = SII_CMD_RST;
	delay(25);
}

/*
 * Copy routines that avoid odd pointers
 */
boolean_t nocopyin = FALSE;
careful_copyin_gap16(tgt, offset, len, isaobb, obb)
	register target_info_t	*tgt;
	unsigned char obb;
{
	register char	*from, *to;
	register int	count;

	count = tgt->transient_state.copy_count;

	from = tgt->dma_ptr + (offset << 1);
	to = tgt->ior->io_data + count;
	tgt->transient_state.copy_count = count + len;
	if (count & 1) {
		from -= (1 << 1);
		to -= 1;
		len += 1;
	}
if (nocopyin) return;/*timing*/
	copyin_gap16( from, to, len);
	/* check for last, poor little odd byte */
	if (isaobb) {
		to += len;
		to[-1] = obb;
	}
}

careful_copyout_gap16( tgt, offset, len)
	register target_info_t	*tgt;
{
	register char	*from, *to;
	register int	count, olen;
	unsigned char	c;
	char		*p;

	count = tgt->ior->io_count - tgt->transient_state.copy_count;
	if (count > 0) {

		len = u_min(count, len);
		offset += tgt->transient_state.cmd_count;

		count = tgt->transient_state.copy_count;
		tgt->transient_state.copy_count = count + len;

		from = tgt->ior->io_data + count;
		to = tgt->dma_ptr + (offset << 1);

		/* the scsi buffer acts weirdo at times */
		if ((olen=len) & 1) {
			p = tgt->dma_ptr + ((offset + olen - 1)<<1);
			c = (*(unsigned short*)p) >> 8;/*!MSF*/
		}

		if (count & 1) {
			from -= 1;
			to -= (1 << 1);
			len += 1;
		}

		count = copyout_gap16(from, to, len);

		/* the scsi buffer acts weirdo at times */
		if (olen & 1) {
			unsigned char cv;
			cv = (*(unsigned short*)p) >> 8;/*!MSF*/
			if (c != cv) {
				/*
				 * Scott Fahlman would say
				 * "Use a big plier!"
				 */
				unsigned short s;
				volatile unsigned short *pp;
				pp = (volatile unsigned short*)p;
				s = (c << 8) | (from[len-1] & 0xff);
				do {
					*pp = s;
				} while (*pp != s);
			}
		}
	}
}

#endif	NSII > 0

