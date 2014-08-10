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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *	File: scsi_5380_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	4/91
 *
 *	Bottom layer of the SCSI driver: chip-dependent functions
 *
 *	This file contains the code that is specific to the NCR 5380
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

#include <sci.h>
#if	NSCI > 0
#include <platforms.h>

#include <mach/std_types.h>
#include <sys/types.h>
#include <chips/busses.h>
#include <scsi/compat_30.h>
#include <machine/machspl.h>

#include <sys/syslog.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#ifdef	VAXSTATION
#define	PAD(n)		char n[3]
#endif

#include <scsi/adapters/scsi_5380.h>

#ifdef	PAD
typedef struct {
	volatile unsigned char	sci_data;	/* r:  Current data */
/*#define		sci_odata sci_data	/* w:  Out data */
	PAD(pad0);

	volatile unsigned char	sci_icmd;	/* rw: Initiator command */
	PAD(pad1);

	volatile unsigned char	sci_mode;	/* rw: Mode */
	PAD(pad2);

	volatile unsigned char	sci_tcmd;	/* rw: Target command */
	PAD(pad3);

	volatile unsigned char	sci_bus_csr;	/* r:  Bus Status */
/*#define		sci_sel_enb sci_bus_csr	/* w:  Select enable */
	PAD(pad4);

	volatile unsigned char	sci_csr;	/* r:  Status */
/*#define		sci_dma_send sci_csr	/* w:  Start dma send data */
	PAD(pad5);

	volatile unsigned char	sci_idata;	/* r:  Input data */
/*#define		sci_trecv sci_idata	/* w:  Start dma receive, target */
	PAD(pad6);

	volatile unsigned char	sci_iack;	/* r:  Interrupt Acknowledge  */
/*#define		sci_irecv sci_iack	/* w:  Start dma receive, initiator */
	PAD(pad7);

} sci_padded_regmap_t;
#else
typedef sci_regmap_t	sci_padded_regmap_t;
#endif

#ifdef	VAXSTATION
#define check_memory(addr,dow)  ((dow) ? wbadaddr(addr,4) : badaddr(addr,4))

/* vax3100 */
#include <chips/vs42x_rb.h>
#define	STC_5380_A	VAX3100_STC_5380_A
#define	STC_5380_B	VAX3100_STC_5380_B
#define STC_DMAREG_OFF	VAX3100_STC_DMAREG_OFF

static int mem;	/* mem++ seems to take approx 0.34 usecs */
#define delay_1p2_us()	{mem++;mem++;mem++;mem++;}
#define	my_scsi_id(ctlr)	(ka3100_scsi_id((ctlr)))
#endif	/* VAXSTATION */


#ifndef	STC_5380_A	/* cross compile check */
typedef struct {
	int	sci_dma_dir, sci_dma_adr;
} *sci_dmaregs_t;
#define	STC_DMAREG_OFF	0
#define	SCI_DMA_DIR_WRITE	0
#define	SCI_DMA_DIR_READ	1
#define	STC_5380_A	0
#define	STC_5380_B	0x100
#define	SCI_RAM_SIZE	0x10000
#endif

/*
 * The 5380 can't tell you the scsi ID it uses, so
 * unless there is another way use the defaults
 */
#ifndef	my_scsi_id
#define	my_scsi_id(ctlr)	(scsi_initiator_id[(ctlr)])
#endif

/*
 * Statically partition the DMA buffer between targets.
 * This way we will eventually be able to attach/detach
 * drives on-fly.  And 18k/target is enough.
 */
#define PER_TGT_DMA_SIZE		((SCI_RAM_SIZE/7) & ~(sizeof(int)-1))

/*
 * Round to 4k to make debug easier
 */
#define	PER_TGT_BUFF_SIZE		((PER_TGT_DMA_SIZE >> 12) << 12)
#define PER_TGT_BURST_SIZE		(PER_TGT_BUFF_SIZE>>1)

/*
 * Macros to make certain things a little more readable
 */

#define	SCI_ACK(ptr,phase)	(ptr)->sci_tcmd = (phase)
#define	SCI_CLR_INTR(regs)	{register int temp = regs->sci_iack;}


/*
 * A script has a two parts: a pre-condition and an action.
 * The first triggers error handling if not satisfied and in
 * our case it is formed by the current bus phase and connected
 * condition as per bus status bits.  The action part is just a
 * function pointer, invoked in a standard way.  The script
 * pointer is advanced only if the action routine returns TRUE.
 * See sci_intr() for how and where this is all done.
 */

typedef struct script {
	int	condition;	/* expected state at interrupt */
	int	(*action)();	/* action routine */
} *script_t;

#define	SCRIPT_MATCH(cs,bs)	(((bs)&SCI_BUS_BSY)|SCI_CUR_PHASE((bs)))

#define	SCI_PHASE_DISC	0x0	/* sort of .. */


/* forward decls of script actions */
boolean_t
	sci_dosynch(),			/* negotiate synch xfer */
	sci_dma_in(),			/* get data from target via dma */
	sci_dma_out(),			/* send data to target via dma */
	sci_get_status(),		/* get status from target */
	sci_end_transaction(),		/* all come to an end */
	sci_msg_in(),			/* get disconnect message(s) */
	sci_disconnected();		/* current target disconnected */
/* forward decls of error handlers */
boolean_t
	sci_err_generic(),		/* generic error handler */
	sci_err_disconn(),		/* when a target disconnects */
	gimmeabreak();			/* drop into the debugger */

int	sci_reset_scsibus();
boolean_t sci_probe_target();

scsi_ret_t	sci_select_target();

#ifdef	VAXSTATION
/*
 * This should be somewhere else, and it was a
 * mistake to share this buffer across SCSIs.
 */
struct dmabuffer {
	volatile char	*base;
	char		*sbrk;
} dmab[1];

volatile char *
sci_buffer_base(unit)
{
	return dmab[unit].base;
}

sci_buffer_init(dmar, ram)
	sci_dmaregs_t	dmar;
	volatile char	*ram;
{
	dmar->sci_dma_rammode = SCI_RAM_EXPMODE;
	dmab[0].base = dmab[0].sbrk = (char *) ram;
	blkclr((char *) ram, SCI_RAM_SIZE);
}
char *
sci_buffer_sbrk(size)
{
	char	*ret = dmab[0].sbrk;

	dmab[0].sbrk += size;
	if ((dmab[0].sbrk - dmab[0].base) > SCI_RAM_SIZE)
		panic("scialloc");
	return ret;
}

#endif	/* VAXSTATION */

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) 5380 interface
 */
struct sci_softc {
	watchdog_t	wd;
	sci_padded_regmap_t	*regs;		/* 5380 registers */
	sci_dmaregs_t	dmar;		/* DMA controller registers */
	volatile char	*buff;		/* DMA buffer memory (I/O space) */
	script_t	script;
	int		(*error_handler)();
	int		in_count;	/* amnt we expect to receive */
	int		out_count;	/* amnt we are going to ship */

	volatile char	state;
#define	SCI_STATE_BUSY		0x01	/* selecting or currently connected */
#define SCI_STATE_TARGET	0x04	/* currently selected as target */
#define SCI_STATE_COLLISION	0x08	/* lost selection attempt */
#define SCI_STATE_DMA_IN	0x10	/* tgt --> initiator xfer */

	unsigned char	ntargets;	/* how many alive on this scsibus */
	unsigned char	done;
	unsigned char	extra_byte;

	scsi_softc_t	*sc;
	target_info_t	*active_target;

	target_info_t	*next_target;	/* trying to seize bus */
	queue_head_t	waiting_targets;/* other targets competing for bus */

} sci_softc_data[NSCI];

typedef struct sci_softc *sci_softc_t;

sci_softc_t	sci_softc[NSCI];

/*
 * Definition of the controller for the auto-configuration program.
 */

int	sci_probe(), scsi_slave(),  sci_go(), sci_intr();
void	scsi_attach();

vm_offset_t	sci_std[NSCI] = { 0 };
struct	bus_device *sci_dinfo[NSCI*8];
struct	bus_ctlr *sci_minfo[NSCI];
struct	bus_driver sci_driver = 
        { sci_probe, scsi_slave, scsi_attach, sci_go, sci_std, "rz", sci_dinfo,
	  "sci", sci_minfo, BUS_INTR_B4_PROBE};

/*
 * Scripts
 */
struct script
sci_script_data_in[] = {
	{ SCSI_PHASE_DATAI|SCI_BUS_BSY, sci_dma_in},
	{ SCSI_PHASE_STATUS|SCI_BUS_BSY, sci_get_status},
	{ SCSI_PHASE_MSG_IN|SCI_BUS_BSY, sci_end_transaction}
},

sci_script_data_out[] = {
	{ SCSI_PHASE_DATAO|SCI_BUS_BSY, sci_dma_out},
	{ SCSI_PHASE_STATUS|SCI_BUS_BSY, sci_get_status},
	{ SCSI_PHASE_MSG_IN|SCI_BUS_BSY, sci_end_transaction}
},

sci_script_cmd[] = {
	{ SCSI_PHASE_STATUS|SCI_BUS_BSY, sci_get_status},
	{ SCSI_PHASE_MSG_IN|SCI_BUS_BSY, sci_end_transaction}
},

/* Synchronous transfer neg(oti)ation */

sci_script_try_synch[] = {
	{ SCSI_PHASE_MSG_OUT|SCI_BUS_BSY, sci_dosynch}
},

/* Disconnect sequence */

sci_script_disconnect[] = {
	{ SCI_PHASE_DISC, sci_disconnected}
};



#define	u_min(a,b)	(((a) < (b)) ? (a) : (b))


#define	DEBUG
#ifdef	DEBUG

sci_state(base)
	vm_offset_t	base;
{
	sci_padded_regmap_t	*regs;
	sci_dmaregs_t	dmar;
	extern char 	*sci;
	unsigned	dmadr;
	int		cnt, i;
	
	if (base == 0)
		base = (vm_offset_t)sci;

	for (i = 0; i < 2; i++) {
		regs = (sci_padded_regmap_t*) (base +
				(i ? STC_5380_B : STC_5380_A));
		dmar = (sci_dmaregs_t) ((char*)regs + STC_DMAREG_OFF);
		SCI_DMADR_GET(dmar,dmadr);
		SCI_TC_GET(dmar,cnt);

		db_printf("scsi%d: ph %x (sb %x), mode %x, tph %x, csr %x, cmd %x, ",
			i,
			(unsigned) SCI_CUR_PHASE(regs->sci_bus_csr),
			(unsigned) regs->sci_bus_csr,
			(unsigned) regs->sci_mode,
			(unsigned) regs->sci_tcmd,
			(unsigned) regs->sci_csr,
			(unsigned) regs->sci_icmd);
		db_printf("dma%c %x @ %x\n", 
			(dmar->sci_dma_dir) ? 'I' : 'O', cnt, dmadr);
	}
	return 0;
}
sci_target_state(tgt)
	target_info_t		*tgt;
{
	if (tgt == 0)
		tgt = sci_softc[0]->active_target;
	if (tgt == 0)
		return 0;
	db_printf("fl %x dma %x+%x cmd %x id %x per %x off %x ior %x ret %x\n",
		tgt->flags, tgt->dma_ptr, tgt->transient_state.dma_offset,
		tgt->cmd_ptr, tgt->target_id, tgt->sync_period, tgt->sync_offset,
		tgt->ior, tgt->done);
	if (tgt->flags & TGT_DISCONNECTED){
		script_t	spt;

		spt = tgt->transient_state.script;
		db_printf("disconnected at ");
		db_printsym(spt,1);
		db_printf(": %x ", spt->condition);
		db_printsym(spt->action,1);
		db_printf(", ");
		db_printsym(tgt->transient_state.handler, 1);
		db_printf("\n");
	}

	return 0;
}

sci_all_targets(unit)
{
	int i;
	target_info_t	*tgt;
	for (i = 0; i < 8; i++) {
		tgt = sci_softc[unit]->sc->target[i];
		if (tgt)
			sci_target_state(tgt);
	}
}

sci_script_state(unit)
{
	script_t	spt = sci_softc[unit]->script;

	if (spt == 0) return 0;
	db_printsym(spt,1);
	db_printf(": %x ", spt->condition);
	db_printsym(spt->action,1);
	db_printf(", ");
	db_printsym(sci_softc[unit]->error_handler, 1);
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
int sci_logpt;
char sci_log[LOGSIZE];

#define MAXLOG_VALUE	0x24
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static LOG(e,f)
	char *f;
{
	sci_log[sci_logpt++] = (e);
	if (sci_logpt == LOGSIZE) sci_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

sci_print_log(skip)
	int skip;
{
	register int i, j;
	register unsigned char c;

	for (i = 0, j = sci_logpt; i < LOGSIZE; i++) {
		c = sci_log[j];
		if (++j == LOGSIZE) j = 0;
		if (skip-- > 0)
			continue;
		if (c < MAXLOG_VALUE)
			db_printf(" %s", logtbl[c].name);
		else
			db_printf("-%d", c & 0x7f);
	}
	db_printf("\n");
	return 0;
}

sci_print_stat()
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
sci_probe(reg, ui)
	char		*reg;
	struct bus_ctlr	*ui;
{
	int             unit = ui->unit;
	sci_softc_t     sci = &sci_softc_data[unit];
	int		target_id, i;
	scsi_softc_t	*sc;
	register sci_padded_regmap_t	*regs;
	spl_t		s;
	boolean_t	did_banner = FALSE;
	char		*cmd_ptr;
	static char	*here = "sci_probe";

	/*
	 * We are only called if the chip is there,
	 * but make sure anyways..
	 */
	regs = (sci_padded_regmap_t *) (reg);
	if (check_memory(regs, 0))
		return 0;

#if	notyet
	/* Mappable version side */
	SCI_probe(reg, ui);
#endif

	/*
	 * Initialize hw descriptor
	 */
	sci_softc[unit] = sci;
	sci->regs = regs;
	sci->dmar = (sci_dmaregs_t)(reg + STC_DMAREG_OFF);
	sci->buff = sci_buffer_base(0);

	queue_init(&sci->waiting_targets);

	sc = scsi_master_alloc(unit, sci);
	sci->sc = sc;

	sc->go = sci_go;
	sc->probe = sci_probe_target;
	sc->watchdog = scsi_watchdog;
	sci->wd.reset = sci_reset_scsibus;

#ifdef	MACH_KERNEL
	sc->max_dma_data = -1;				/* unlimited */
#else
	sc->max_dma_data = scsi_per_target_virtual;
#endif

	scsi_might_disconnect[unit] = 0;	/* still true */

	/*
	 * Reset chip
	 */
	s = splbio();
	sci_reset(sci, TRUE);
	SCI_CLR_INTR(regs);

	/*
	 * Our SCSI id on the bus.
	 */

	sc->initiator_id = my_scsi_id(unit);
	printf("%s%d: my SCSI id is %d", ui->name, unit, sc->initiator_id);

	/*
	 * For all possible targets, see if there is one and allocate
	 * a descriptor for it if it is there.
	 */
	cmd_ptr = sci_buffer_sbrk(0);
	for (target_id = 0; target_id < 8; target_id++) {

		register unsigned csr, dsr;
		scsi_status_byte_t	status;

		/* except of course ourselves */
		if (target_id == sc->initiator_id)
			continue;

		if (sci_select_target( regs, sc->initiator_id, target_id, FALSE) == SCSI_RET_DEVICE_DOWN) {
			SCI_CLR_INTR(regs);
			continue;
		}

		printf(",%s%d", did_banner++ ? " " : " target(s) at ",
				target_id);

		/* should be command phase here: we selected wo ATN! */
		while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_CMD)
			;

		SCI_ACK(regs,SCSI_PHASE_CMD);

		/* build command in dma area */
		{
			unsigned char	*p = (unsigned char*) cmd_ptr;

			p[0] = SCSI_CMD_TEST_UNIT_READY;
			p[1] = 
			p[2] = 
			p[3] = 
			p[4] = 
			p[5] = 0;
		}

		sci_data_out(regs, SCSI_PHASE_CMD, 6, cmd_ptr);

		while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_STATUS)
			;

		SCI_ACK(regs,SCSI_PHASE_STATUS);

		sci_data_in(regs, SCSI_PHASE_STATUS, 1, &status.bits);

		if (status.st.scsi_status_code != SCSI_ST_GOOD)
			scsi_error( 0, SCSI_ERR_STATUS, status.bits, 0);

		/* get cmd_complete message */
		while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_MSG_IN)
			;

		SCI_ACK(regs,SCSI_PHASE_MSG_IN);

		sci_data_in(regs, SCSI_PHASE_MSG_IN, 1, &i);

		/* check disconnected, clear all intr bits */
		while (regs->sci_bus_csr & SCI_BUS_BSY)
			;
		SCI_ACK(regs,SCI_PHASE_DISC);

		SCI_CLR_INTR(regs);

		/* ... */

		/*
		 * Found a target
		 */
		sci->ntargets++;
		{
			register target_info_t	*tgt;

			tgt = scsi_slave_alloc(unit, target_id, sci);

			/* "virtual" address for our use */
			tgt->cmd_ptr = sci_buffer_sbrk(PER_TGT_DMA_SIZE);
			/* "physical" address for dma engine */
			tgt->dma_ptr = (char*)(tgt->cmd_ptr - sci->buff);
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
sci_probe_target(tgt, ior)
	target_info_t		*tgt;
	io_req_t		ior;
{
	sci_softc_t     sci = sci_softc[tgt->masterno];
	boolean_t	newlywed;

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		/* desc was allocated afresh */

		/* "virtual" address for our use */
		tgt->cmd_ptr = sci_buffer_sbrk(PER_TGT_DMA_SIZE);
		/* "physical" address for dma engine */
		tgt->dma_ptr = (char*)(tgt->cmd_ptr - sci->buff);
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_init(&tgt->fdma, scsi_per_target_virtual);
#endif	/*MACH_KERNEL*/

	}

	if (scsi_inquiry(tgt, SCSI_INQ_STD_DATA) == SCSI_RET_DEVICE_DOWN)
		return FALSE;

	tgt->flags = TGT_ALIVE;
	return TRUE;
}


static sci_wait(preg, until)
	volatile unsigned char	*preg;
{
	int timeo = 1000000;
	/* read it over to avoid bus glitches */
	while ( ((*preg & until) != until) ||
		((*preg & until) != until) ||
		((*preg & until) != until)) {
		delay(1);
		if (!timeo--) {
			printf("sci_wait TIMEO with x%x\n", *preg);
			break;
		}
	}
	return *preg;
}

scsi_ret_t
sci_select_target(regs, myid, id, with_atn)
	register sci_padded_regmap_t	*regs;
	unsigned char		myid, id;
	boolean_t		with_atn;
{
	register unsigned char	bid, icmd;
	scsi_ret_t		ret = SCSI_RET_RETRY;

	if ((regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)))
		return ret;

	/* for our purposes.. */
	myid = 1 << myid;
	id = 1 << id;

	regs->sci_sel_enb = myid;	/* if not there already */

	regs->sci_odata = myid;
	regs->sci_mode |= SCI_MODE_ARB;
	/* AIP might not set if BSY went true after we checked */
	for (bid = 0; bid < 20; bid++)	/* 20usec circa */
		if (regs->sci_icmd & SCI_ICMD_AIP)
			break;
	if ((regs->sci_icmd & SCI_ICMD_AIP) == 0) {
		goto lost;
	}

	delay(2);	/* 2.2us arb delay */

	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto lost;
	}

	regs->sci_mode &= ~SCI_MODE_PAR_CHK;
	bid = regs->sci_data;

	if ((bid & ~myid) > myid) {
		goto lost;
	}
	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto lost;
	}

	/* Won arbitration, enter selection phase now */	
	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
	icmd |= (with_atn ? (SCI_ICMD_SEL|SCI_ICMD_ATN) : SCI_ICMD_SEL);
	regs->sci_icmd = icmd;

	if (regs->sci_icmd & SCI_ICMD_LST) {
		goto nosel;
	}

	/* XXX a target that violates specs might still drive the bus XXX */
	/* XXX should put our id out, and after the delay check nothi XXX */
	/* XXX ng else is out there.				      XXX */

	delay_1p2_us();

	regs->sci_sel_enb = 0;

	regs->sci_odata = myid | id;

	icmd |= SCI_ICMD_BSY|SCI_ICMD_DATA;
	regs->sci_icmd = icmd;

	regs->sci_mode &= ~SCI_MODE_ARB;	/* 2 deskew delays, too */
	
	icmd &= ~SCI_ICMD_BSY;
	regs->sci_icmd = icmd;

	/* bus settle delay, 400ns */
	delay(0); /* too much ? */

	regs->sci_mode |= SCI_MODE_PAR_CHK;

	{
		register int timeo  = 2500;/* 250 msecs in 100 usecs chunks */
		while ((regs->sci_bus_csr & SCI_BUS_BSY) == 0)
			if (--timeo > 0)
				delay(100);
			else {
				goto nodev;
			}
	}

	icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_SEL);
	regs->sci_icmd = icmd;
/*	regs->sci_sel_enb = myid;*/	/* looks like we should NOT have it */
	return SCSI_RET_SUCCESS;
nodev:
	ret = SCSI_RET_DEVICE_DOWN;
	regs->sci_sel_enb = myid;
nosel:
	icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_SEL|SCI_ICMD_ATN);
	regs->sci_icmd = icmd;
lost:
	bid = regs->sci_mode;
	bid &= ~SCI_MODE_ARB;
	bid |= SCI_MODE_PAR_CHK;
	regs->sci_mode = bid;

	return ret;
}

sci_data_out(regs, phase, count, data)
	register sci_padded_regmap_t	*regs;
	unsigned char		*data;
{
	register unsigned char	icmd;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
loop:
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return count;

	while (	((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0))
		;
	icmd |= SCI_ICMD_DATA;
	regs->sci_icmd = icmd;
	regs->sci_odata = *data++;
	icmd |= SCI_ICMD_ACK;
	regs->sci_icmd = icmd;

	icmd &= ~(SCI_ICMD_DATA|SCI_ICMD_ACK);
	while ( (regs->sci_bus_csr & SCI_BUS_REQ) &&
		(regs->sci_bus_csr & SCI_BUS_REQ) &&
		(regs->sci_bus_csr & SCI_BUS_REQ))
		;
	regs->sci_icmd = icmd;
	if (--count > 0)
		goto loop;
	return 0;
}

sci_data_in(regs, phase, count, data)
	register sci_padded_regmap_t	*regs;
	unsigned char		*data;
{
	register unsigned char	icmd;

	/* ..checks.. */

	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
loop:
	if (SCI_CUR_PHASE(regs->sci_bus_csr) != phase)
		return count;

	while ( ((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0))
		;
	*data++ = regs->sci_data;
	icmd |= SCI_ICMD_ACK;
	regs->sci_icmd = icmd;

	icmd &= ~SCI_ICMD_ACK;
	while ( (regs->sci_bus_csr & SCI_BUS_REQ) &&
		(regs->sci_bus_csr & SCI_BUS_REQ) &&
		(regs->sci_bus_csr & SCI_BUS_REQ))
		;
	regs->sci_icmd = icmd;
	if (--count > 0)
		goto loop;
	return 0;

}

sci_reset(sci, quickly)
	sci_softc_t		sci;
	boolean_t		quickly;
{
	register sci_padded_regmap_t	*regs = sci->regs;
	register sci_dmaregs_t	dma = sci->dmar;
	int dummy;

	regs->sci_icmd = SCI_ICMD_TEST;	/* don't drive outputs */
	regs->sci_icmd = SCI_ICMD_TEST|SCI_ICMD_RST;
	delay(25);
	regs->sci_icmd = 0;

	regs->sci_mode = SCI_MODE_PAR_CHK|SCI_MODE_PERR_IE;
	regs->sci_tcmd = SCI_PHASE_DISC; /* make sure we do not miss transition */
	regs->sci_sel_enb = 0;

	/* idle the dma controller */
	dma->sci_dma_adr = 0;
	dma->sci_dma_dir = SCI_DMA_DIR_WRITE;
	SCI_TC_PUT(dma,0);

	/* clear interrupt (two might be queued?) */
	SCI_CLR_INTR(regs);
	SCI_CLR_INTR(regs);

	if (quickly)
		return;

	/*
	 * reset the scsi bus, the interrupt routine does the rest
	 * or you can call sci_bus_reset().
	 */
	regs->sci_icmd = SCI_ICMD_RST;
	
}

/*
 *	Operational functions
 */

/*
 * Start a SCSI command on a target
 */
sci_go(tgt, cmd_count, in_count, cmd_only)
	target_info_t		*tgt;
	boolean_t		cmd_only;
{
	sci_softc_t		sci;
	register spl_t		s;
	boolean_t		disconn;
	script_t		scp;
	boolean_t		(*handler)();

	LOG(1,"go");

	sci = (sci_softc_t)tgt->hw_state;

	/*
	 * We cannot do real DMA.
	 */
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
	if (tgt->ior)
		fdma_map(&tgt->fdma, tgt->ior);
#endif	/*MACH_KERNEL*/

	if ((tgt->cur_cmd == SCSI_CMD_WRITE) ||
	    (tgt->cur_cmd == SCSI_CMD_LONG_WRITE)){
		io_req_t	ior = tgt->ior;
		register int	len = ior->io_count;

		tgt->transient_state.out_count = len;

		if (len > PER_TGT_BUFF_SIZE)
			len = PER_TGT_BUFF_SIZE;
		bcopy(	ior->io_data,
			tgt->cmd_ptr + cmd_count,
			len);
		tgt->transient_state.copy_count = len;

		/* avoid leaks */
		if (len < tgt->block_size) {
			bzero(	tgt->cmd_ptr + cmd_count + len,
				tgt->block_size - len);
			tgt->transient_state.out_count = tgt->block_size;
		}
	} else {
		tgt->transient_state.out_count = 0;
		tgt->transient_state.copy_count = 0;
	}

	tgt->transient_state.cmd_count = cmd_count;

	disconn  = BGET(scsi_might_disconnect,tgt->masterno,tgt->target_id);
	disconn  = disconn && (sci->ntargets > 1);
	disconn |= BGET(scsi_should_disconnect,tgt->masterno,tgt->target_id);

	/*
	 * Setup target state
	 */
	tgt->done = SCSI_RET_IN_PROGRESS;

	handler = (disconn) ? sci_err_disconn : sci_err_generic;

	switch (tgt->cur_cmd) {
	    case SCSI_CMD_READ:
	    case SCSI_CMD_LONG_READ:
		LOG(0x13,"readop");
		scp = sci_script_data_in;
		break;
	    case SCSI_CMD_WRITE:
	    case SCSI_CMD_LONG_WRITE:
		LOG(0x14,"writeop");
		scp = sci_script_data_out;
		break;
	    case SCSI_CMD_INQUIRY:
		/* This is likely the first thing out:
		   do the synch neg if so */
		if (!cmd_only && ((tgt->flags&TGT_DID_SYNCH)==0)) {
			scp = sci_script_try_synch;
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
		scp = sci_script_data_in;
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
		scp = sci_script_data_out;
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    case SCSI_CMD_TEST_UNIT_READY:
		/*
		 * Do the synch negotiation here, unless prohibited
		 * or done already
		 */
		if (tgt->flags & TGT_DID_SYNCH) {
			scp = sci_script_cmd;
		} else {
			scp = sci_script_try_synch;
			tgt->flags |= TGT_TRY_SYNCH;
			cmd_only = FALSE;
		}
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    default:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		scp = sci_script_cmd;
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
	 * this SCSI bus, e.g. lock the sci structure.
	 * Note that it is the strategy routine's job
	 * to serialize ops on the same target as appropriate.
	 * XXX here and everywhere, locks!
	 */
	/*
	 * Protection viz reconnections makes it tricky.
	 */
/*	s = splbio();*/
	s = splhigh();

	if (sci->wd.nactive++ == 0)
		sci->wd.watchdog_state = SCSI_WD_ACTIVE;

	if (sci->state & SCI_STATE_BUSY) {
		/*
		 * Queue up this target, note that this takes care
		 * of proper FIFO scheduling of the scsi-bus.
		 */
		LOG(3,"enqueue");
		enqueue_tail(&sci->waiting_targets, (queue_entry_t) tgt);
	} else {
		/*
		 * It is down to at most two contenders now,
		 * we will treat reconnections same as selections
		 * and let the scsi-bus arbitration process decide.
		 */
		sci->state |= SCI_STATE_BUSY;
		sci->next_target = tgt;
		sci_attempt_selection(sci);
		/*
		 * Note that we might still lose arbitration..
		 */
	}
	splx(s);
}

sci_attempt_selection(sci)
	sci_softc_t	sci;
{
	target_info_t	*tgt;
	register int	out_count;
	sci_padded_regmap_t	*regs;
	sci_dmaregs_t	dmar;
	register int	cmd;
	boolean_t	ok;
	scsi_ret_t	ret;

	regs = sci->regs;
	dmar = sci->dmar;
	tgt = sci->next_target;

	LOG(4,"select");
	LOG(0x80+tgt->target_id,0);

	/*
	 * Init bus state variables and set registers.
	 */
	sci->active_target = tgt;

	/* reselection pending ? */
	if ((regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)) &&
	    (regs->sci_bus_csr & (SCI_BUS_BSY|SCI_BUS_SEL)))
		return;

	sci->script = tgt->transient_state.script;
	sci->error_handler = tgt->transient_state.handler;
	sci->done = SCSI_RET_IN_PROGRESS;

	sci->in_count = 0;
	sci->out_count = 0;
	sci->extra_byte = 0;

	/*
	 * This is a bit involved, but the bottom line is we want to
	 * know after we selected with or w/o ATN if the selection
	 * went well (ret) and if it is (ok) to send the command.
	 */
	ok = TRUE;
	if (tgt->flags & TGT_DID_SYNCH) {
		if (tgt->transient_state.identify == 0xff) {
			/* Select w/o ATN */
			ret = sci_select_target(regs, sci->sc->initiator_id,
						tgt->target_id, FALSE);
		} else {
			/* Select with ATN */
			ret = sci_select_target(regs, sci->sc->initiator_id,
						tgt->target_id, TRUE);
			if (ret == SCSI_RET_SUCCESS) {
				register unsigned char	icmd;

				while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_MSG_OUT)
					;
				icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
				icmd &= ~SCI_ICMD_ATN;
				regs->sci_icmd = icmd;
				SCI_ACK(regs,SCSI_PHASE_MSG_OUT);
				ok = (sci_data_out(regs, SCSI_PHASE_MSG_OUT,
				     1, &tgt->transient_state.identify) == 0);
			}
		}
	} else if (tgt->flags & TGT_TRY_SYNCH) {
		/* Select with ATN, do the synch xfer neg */
		ret = sci_select_target(regs, sci->sc->initiator_id,
					tgt->target_id, TRUE);
		if (ret == SCSI_RET_SUCCESS) {
			while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_MSG_OUT)
				;
			ok = sci_dosynch( sci, regs->sci_csr, regs->sci_bus_csr);
		}
	} else {
		ret = sci_select_target(regs, sci->sc->initiator_id,
					tgt->target_id, FALSE);
	}

	if (ret == SCSI_RET_DEVICE_DOWN) {
		sci->done = ret;
		sci_end(sci, regs->sci_csr, regs->sci_bus_csr);
		return;
	}
	if ((ret != SCSI_RET_SUCCESS) || !ok)
		return;

/* time this out or do it via dma !! */
	while (SCI_CUR_PHASE(regs->sci_bus_csr) != SCSI_PHASE_CMD)
		;

	/* set dma pointer and counter to xfer command */
 	out_count = tgt->transient_state.cmd_count;
#if 0
	SCI_ACK(regs,SCSI_PHASE_CMD);
	sci_data_out(regs,SCSI_PHASE_CMD,out_count,tgt->cmd_ptr);
	regs->sci_mode = SCI_MODE_PAR_CHK|SCI_MODE_DMA|SCI_MODE_MONBSY;
#else
	SCI_DMADR_PUT(dmar,tgt->dma_ptr);
	delay_1p2_us();
	SCI_TC_PUT(dmar,out_count);
	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;
	SCI_ACK(regs,SCSI_PHASE_CMD);
	SCI_CLR_INTR(regs);
	regs->sci_mode = SCI_MODE_PAR_CHK|SCI_MODE_DMA|SCI_MODE_MONBSY;
	regs->sci_icmd = SCI_ICMD_DATA;
	regs->sci_dma_send = 1;
#endif
}

/*
 * Interrupt routine
 *	Take interrupts from the chip
 *
 * Implementation:
 *	Move along the current command's script if
 *	all is well, invoke error handler if not.
 */
sci_intr(unit)
{
	register sci_softc_t	sci;
	register script_t	scp;
	register unsigned	csr, bs, cmd;
	register sci_padded_regmap_t	*regs;
	boolean_t		try_match;
#if	notyet
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface)
		return SCI_intr(unit);
#endif

	LOG(5,"\n\tintr");

	sci = sci_softc[unit];
	regs = sci->regs;

	/* ack interrupt */
	csr = regs->sci_csr;
	bs = regs->sci_bus_csr;
	cmd = regs->sci_icmd;
TR(regs->sci_mode);
	SCI_CLR_INTR(regs);

TR(csr);
TR(bs);
TR(cmd);
TRCHECK;

	if (cmd & SCI_ICMD_RST){
		sci_bus_reset(sci);
		return;
	}

	/* we got an interrupt allright */
	if (sci->active_target)
		sci->wd.watchdog_state = SCSI_WD_ACTIVE;

	/* drop spurious calls */
	if ((csr & SCI_CSR_INT) == 0) {
		LOG(2,"SPURIOUS");
		return;
	}

	/* Note: reselect has I/O asserted, select has not */
	if ((sci->state & SCI_STATE_TARGET) ||
	    ((bs & (SCI_BUS_BSY|SCI_BUS_SEL|SCI_BUS_IO)) == SCI_BUS_SEL)) {
		sci_target_intr(sci,csr,bs);
		return;
	}

	scp = sci->script;

	/* Race: disconnecting, we get the disconnected notification
	   (csr sez BSY dropped) at the same time a reselect is active */
	if ((csr & SCI_CSR_DISC) &&
	    scp && (scp->condition == SCI_PHASE_DISC)) {
		(void) (*scp->action)(sci, csr, bs);
		/* takes care of calling reconnect if necessary */
		return;
	}

	/* check who got the bus */
	if ((scp == 0) || (cmd & SCI_ICMD_LST) ||
	    ((bs & (SCI_BUS_BSY|SCI_BUS_SEL|SCI_BUS_IO)) == (SCI_BUS_SEL|SCI_BUS_IO))) {
		sci_reconnect(sci, csr, bs);
		return;
	}

	if (SCRIPT_MATCH(csr,bs) != scp->condition) {
		if (try_match = (*sci->error_handler)(sci, csr, bs)) {
			csr = regs->sci_csr;
			bs = regs->sci_bus_csr;
		}
	} else
		try_match = TRUE;

	/* might have been side effected */
	scp = sci->script;

	if (try_match && (SCRIPT_MATCH(csr,bs) == scp->condition)) {
		/*
		 * Perform the appropriate operation,
		 * then proceed
		 */
		if ((*scp->action)(sci, csr, bs)) {
			/* might have been side effected */
			scp = sci->script;
			sci->script = scp + 1;
		}
	}
}


sci_target_intr(sci)
	register sci_softc_t	sci;
{
	panic("SCI: TARGET MODE !!!\n");
}

/*
 * All the many little things that the interrupt
 * routine might switch to
 */
boolean_t
sci_end_transaction( sci, csr, bs)
	register sci_softc_t	sci;
{
	register sci_padded_regmap_t	*regs = sci->regs;
	char			cmc;

	LOG(0x1f,"end_t");

	/* Stop dma, no interrupt on disconnect */
	regs->sci_icmd = 0;
	regs->sci_mode &= ~(SCI_MODE_DMA|SCI_MODE_MONBSY|SCI_MODE_DMA_IE);
/*	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;/* make sure we steal not */

	SCI_ACK(regs,SCSI_PHASE_MSG_IN);

	regs->sci_sel_enb = (1 << sci->sc->initiator_id);

	sci_data_in(regs, SCSI_PHASE_MSG_IN, 1, &cmc);

	if (cmc != SCSI_COMMAND_COMPLETE)
		printf("{T%x}", cmc);

	/* check disconnected, clear all intr bits */
	while (regs->sci_bus_csr & SCI_BUS_BSY)
			;
	SCI_CLR_INTR(regs);
	SCI_ACK(regs,SCI_PHASE_DISC);

	if (!sci_end(sci, csr, bs)) {
		SCI_CLR_INTR(regs);
		(void) sci_reconnect(sci, csr, bs);
	}
	return FALSE;
}

boolean_t
sci_end( sci, csr, bs)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	register io_req_t	ior;
	register sci_padded_regmap_t	*regs = sci->regs;
	boolean_t		reconn_pending;

	LOG(6,"end");

	tgt = sci->active_target;

	if ((tgt->done = sci->done) == SCSI_RET_IN_PROGRESS)
		tgt->done = SCSI_RET_SUCCESS;

	sci->script = 0;

	if (sci->wd.nactive-- == 1)
		sci->wd.watchdog_state = SCSI_WD_INACTIVE;

	/* check reconnection not pending */
	bs = regs->sci_bus_csr;
	reconn_pending = ((bs & (SCI_BUS_BSY|SCI_BUS_SEL|SCI_BUS_IO)) == (SCI_BUS_SEL|SCI_BUS_IO));
	if (!reconn_pending) {
		sci_release_bus(sci);
	} else {
		sci->active_target = 0;
/*		sci->state &= ~SCI_STATE_BUSY; later */
	}

	if (ior = tgt->ior) {
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_unmap(&tgt->fdma, ior);
#endif	/*MACH_KERNEL*/
		LOG(0xA,"ops->restart");
		(*tgt->dev_ops->restart)( tgt, TRUE);
		if (reconn_pending)
			sci->state &= ~SCI_STATE_BUSY;
	}

	return (!reconn_pending);
}

boolean_t
sci_release_bus(sci)
	register sci_softc_t	sci;
{
	boolean_t	ret = FALSE;

	LOG(9,"release");

	sci->script = 0;

	if (sci->state & SCI_STATE_COLLISION) {

		LOG(0xB,"collided");
		sci->state &= ~SCI_STATE_COLLISION;
		sci_attempt_selection(sci);

	} else if (queue_empty(&sci->waiting_targets)) {

		sci->state &= ~SCI_STATE_BUSY;
		sci->active_target = 0;
		ret = TRUE;

	} else {

		LOG(0xC,"dequeue");
		sci->next_target = (target_info_t *)
				dequeue_head(&sci->waiting_targets);
		sci_attempt_selection(sci);
	}
	return ret;
}

boolean_t
sci_get_status( sci, csr, bs)
	register sci_softc_t	sci;
{
	register sci_padded_regmap_t	*regs = sci->regs;
	register sci_dmaregs_t	dmar = sci->dmar;
	scsi2_status_byte_t	status;
	register target_info_t	*tgt;
	unsigned int		len, mode;

	LOG(0xD,"get_status");
TRWRAP;

	/* Stop dma */
	regs->sci_icmd = 0;
	mode = regs->sci_mode;
	regs->sci_mode = (mode & ~(SCI_MODE_DMA|SCI_MODE_DMA_IE));
	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;/* make sure we steal not */

	sci->state &= ~SCI_STATE_DMA_IN;

	tgt = sci->active_target;

	if (len = sci->in_count) {
		register int	count;
		SCI_TC_GET(dmar,count);
		if ((tgt->cur_cmd != SCSI_CMD_READ) &&
		    (tgt->cur_cmd != SCSI_CMD_LONG_READ)){
			len -= count;
		} else {
			if (count) {
#if 0
				this is incorrect and besides..
				tgt->ior->io_residual = count;
#endif
				len -= count;
			}
			sci_copyin(	tgt, tgt->transient_state.dma_offset,
						len, 0, 0);
		}
	}

	/* to get the phase mismatch intr */
	regs->sci_mode = mode;

	SCI_ACK(regs,SCSI_PHASE_STATUS);

	sci_data_in(regs, SCSI_PHASE_STATUS, 1, &status.bits);

	SCI_TC_PUT(dmar,0);

	if (status.st.scsi_status_code != SCSI_ST_GOOD) {
		scsi_error(sci->active_target, SCSI_ERR_STATUS, status.bits, 0);
		sci->done = (status.st.scsi_status_code == SCSI_ST_BUSY) ?
			SCSI_RET_RETRY : SCSI_RET_NEED_SENSE;
	} else
		sci->done = SCSI_RET_SUCCESS;

	return TRUE;
}

boolean_t
sci_dma_in( sci, csr, bs)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	register sci_padded_regmap_t	*regs = sci->regs;
	register sci_dmaregs_t	dmar = sci->dmar;
	char			*dma_ptr;
	register int		count;
	boolean_t		advance_script = TRUE;

	LOG(0xE,"dma_in");

	/*
	 * Problem: the 5380 pipelines xfers between the scsibus and
	 * itself and between itself and the DMA engine --> halting ?
	 * In the dmaout direction all is ok, except that (see NCR notes)
	 * the EOP interrupt is generated before the pipe is empty.
	 * In the dmain direction (here) the interrupt comes when
	 * one too many bytes have been xferred on chip!
	 *
	 * More specifically, IF we asked for count blindly and we had
	 * more than count bytes coming (double buffering) we would endup
	 * actually xferring count+1 from the scsibus, but only count
	 * to memory [hopefully the last byte sits in the sci_datai reg].
	 * This could be helped, except most times count is an exact multiple
	 * of the sector size which is where disks disconnect....
	 *
	 * INSTEAD, we recognize here that we expect more than count bytes
	 * coming and set the DMA count to count-1 but keep sci->in_count
	 * above to count.  This will be wrong if the target disconnects
	 * amidst, but we can cure it.
	 *
	 * The places where this has an effect are marked by "EXTRA_BYTE"
	 */

	tgt = sci->active_target;
	sci->state |= SCI_STATE_DMA_IN;

	/* ought to stop dma to start another */
	regs->sci_mode &= ~ (SCI_MODE_DMA|SCI_MODE_DMA_IE);
	regs->sci_icmd = 0;

	if (sci->in_count == 0) {
		/*
		 * Got nothing yet: either just sent the command
		 * or just reconnected
		 */
		register int avail;

		count = tgt->transient_state.in_count;
		count = u_min(count, (PER_TGT_BURST_SIZE));
		avail = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
		count = u_min(count, avail);

		/* common case of 8k-or-less read ? */
		advance_script = (tgt->transient_state.in_count == count);

	} else {

		/*
		 * We received some data.
		 */
		register int offset, xferred, eb;
		unsigned char	extrab = regs->sci_idata; /* EXTRA_BYTE */

		SCI_TC_GET(dmar,xferred);
		assert(xferred == 0);
if (scsi_debug) {
printf("{B %x %x %x (%x)}",
		sci->in_count, xferred, sci->extra_byte, extrab);
}
		/* ++EXTRA_BYTE */
		xferred = sci->in_count - xferred;
		eb = sci->extra_byte;
		/* --EXTRA_BYTE */
		assert(xferred > 0);
		tgt->transient_state.in_count -= xferred;
		assert(tgt->transient_state.in_count > 0);
		offset = tgt->transient_state.dma_offset;
		tgt->transient_state.dma_offset += xferred;
		count = u_min(tgt->transient_state.in_count, (PER_TGT_BURST_SIZE));
		if (tgt->transient_state.dma_offset == PER_TGT_BUFF_SIZE) {
			tgt->transient_state.dma_offset = 0;
		} else {
			register int avail;
			avail = PER_TGT_BUFF_SIZE - tgt->transient_state.dma_offset;
			count = u_min(count, avail);
		}
		advance_script = (tgt->transient_state.in_count == count);

		/* get some more */
		dma_ptr = tgt->dma_ptr + tgt->transient_state.dma_offset;
		sci->in_count = count;
		/* ++EXTRA_BYTE */
		if (!advance_script) {
			sci->extra_byte = 1;	/* that's the cure.. */
			count--;
		} else
			sci->extra_byte = 0;
		/* --EXTRA_BYTE */
		SCI_TC_PUT(dmar,count);
/*		regs->sci_icmd = 0;*/
		SCI_DMADR_PUT(dmar,dma_ptr);
		delay_1p2_us();
		SCI_ACK(regs,SCSI_PHASE_DATAI);
		SCI_CLR_INTR(regs);
		regs->sci_mode |= (advance_script ? SCI_MODE_DMA
				   : (SCI_MODE_DMA|SCI_MODE_DMA_IE));
		dmar->sci_dma_dir = SCI_DMA_DIR_READ;
		regs->sci_irecv = 1;

		/* copy what we got */
		sci_copyin( tgt, offset, xferred, eb, extrab);

		/* last chunk ? */
		return advance_script;
	}

	sci->in_count = count;
	dma_ptr = tgt->dma_ptr + tgt->transient_state.dma_offset;

	/* ++EXTRA_BYTE */
	if (!advance_script) {
		sci->extra_byte = 1;	/* that's the cure.. */
		count--;
	} else
		sci->extra_byte = 0;
	/* --EXTRA_BYTE */

	SCI_TC_PUT(dmar,count);
/*	regs->sci_icmd = 0;*/
	SCI_DMADR_PUT(dmar,dma_ptr);
	delay_1p2_us();
	SCI_ACK(regs,SCSI_PHASE_DATAI);
	SCI_CLR_INTR(regs);
	regs->sci_mode |= (advance_script ? SCI_MODE_DMA
			   : (SCI_MODE_DMA|SCI_MODE_DMA_IE));
	dmar->sci_dma_dir = SCI_DMA_DIR_READ;
	regs->sci_irecv = 1;

	return advance_script;
}

/* send data to target. Called in three different ways:
   (a) to start transfer (b) to restart a bigger-than-8k
   transfer (c) after reconnection
 */
int sci_delay = 1;

boolean_t
sci_dma_out( sci, csr, bs)
	register sci_softc_t	sci;
{
	register sci_padded_regmap_t	*regs = sci->regs;
	register sci_dmaregs_t	dmar = sci->dmar;
	register char		*dma_ptr;
	register target_info_t	*tgt;
	boolean_t		advance_script = TRUE;
	int			count = sci->out_count;
	spl_t			s;
	register int		tmp;

	LOG(0xF,"dma_out");

	tgt = sci->active_target;
	sci->state &= ~SCI_STATE_DMA_IN;

	if (sci->out_count == 0) {
		/*
		 * Nothing committed: either just sent the
		 * command or reconnected
		 */
		register int remains;

		/* ought to stop dma to start another */
		regs->sci_mode &= ~ (SCI_MODE_DMA|SCI_MODE_DMA_IE);
		dmar->sci_dma_dir = SCI_DMA_DIR_READ;/*hold it */

		regs->sci_icmd = SCI_ICMD_DATA;

		SCI_ACK(regs,SCSI_PHASE_DATAO);

		count = tgt->transient_state.out_count;
		count = u_min(count, (PER_TGT_BURST_SIZE));
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

if (sci_delay & 1) delay(1000);
		/* ought to stop dma to start another */
		regs->sci_mode &= ~ (SCI_MODE_DMA|SCI_MODE_DMA_IE);
		dmar->sci_dma_dir = SCI_DMA_DIR_READ;/*hold it */
/*		regs->sci_icmd = SCI_ICMD_DATA; */

		SCI_TC_GET(dmar,xferred);
if (xferred) printf("{A %x}", xferred);
		xferred = sci->out_count - xferred;
		assert(xferred > 0);
		tgt->transient_state.out_count -= xferred;
		assert(tgt->transient_state.out_count > 0);
		offset = tgt->transient_state.dma_offset;
		tgt->transient_state.dma_offset += xferred;
		count = u_min(tgt->transient_state.out_count, (PER_TGT_BURST_SIZE));
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
			tgt->transient_state.cmd_count + tgt->transient_state.dma_offset;
		sci->out_count = count;
		/*
		 *  Mistery: sometimes the first byte
		 *  of an 8k chunk is missing from the tape, it must
		 *  be that somehow touching the 5380 registers
		 *  after the dma engine is ready screws up: false DRQ?
		 */
s = splhigh();
		SCI_TC_PUT(dmar,count);
/*		SCI_CLR_INTR(regs);*/
		regs->sci_mode = SCI_MODE_PAR_CHK | SCI_MODE_DMA |
				 SCI_MODE_MONBSY | SCI_MODE_DMA_IE;
/*		regs->sci_icmd = SCI_ICMD_DATA;*/
		dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;
		SCI_DMADR_PUT(dmar,dma_ptr);
		delay_1p2_us();

		regs->sci_dma_send = 1;
splx(s);
		/* copy some more data */
		sci_copyout(tgt, offset, xferred);
		return FALSE;
	}

quickie:
	sci->out_count = count;
	dma_ptr = tgt->dma_ptr +
		tgt->transient_state.cmd_count + tgt->transient_state.dma_offset;
	tmp = (advance_script ?
		SCI_MODE_PAR_CHK|SCI_MODE_DMA|SCI_MODE_MONBSY:
		SCI_MODE_PAR_CHK|SCI_MODE_DMA|SCI_MODE_MONBSY|SCI_MODE_DMA_IE);
s = splhigh();
	SCI_TC_PUT(dmar,count);
/*	SCI_CLR_INTR(regs);*/
	regs->sci_mode = tmp;
/*	regs->sci_icmd = SCI_ICMD_DATA;*/
	SCI_DMADR_PUT(dmar,dma_ptr);
	delay_1p2_us();
	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;
	regs->sci_dma_send = 1;
splx(s);

	return advance_script;
}

/* disconnect-reconnect ops */

/* get the message in via dma */
boolean_t
sci_msg_in(sci, csr, bs)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	char			*dma_ptr;
	register sci_padded_regmap_t	*regs = sci->regs;
	register sci_dmaregs_t	dmar = sci->dmar;

	LOG(0x15,"msg_in");

	tgt = sci->active_target;

	dma_ptr = tgt->dma_ptr;
	/* We would clobber the data for READs */
	if (sci->state & SCI_STATE_DMA_IN) {
		register int offset;
		offset = tgt->transient_state.cmd_count + tgt->transient_state.dma_offset;
		dma_ptr += offset;
	}

	/* ought to stop dma to start another */
	regs->sci_mode &= ~ (SCI_MODE_DMA|SCI_MODE_DMA_IE);
	regs->sci_icmd = 0;

	/* We only really expect two bytes */
	SCI_TC_PUT(dmar,sizeof(scsi_command_group_0));
/*	regs->sci_icmd = 0*/
	SCI_DMADR_PUT(dmar,dma_ptr);
	delay_1p2_us();
	SCI_ACK(regs,SCSI_PHASE_MSG_IN);
	SCI_CLR_INTR(regs);
	regs->sci_mode |= SCI_MODE_DMA;
	dmar->sci_dma_dir = SCI_DMA_DIR_READ;
	regs->sci_irecv = 1;

	return TRUE;
}

/* check the message is indeed a DISCONNECT */
boolean_t
sci_disconnect(sci, csr, bs)
	register sci_softc_t	sci;
{
	register int		len;
	boolean_t		ok = FALSE;
	register sci_dmaregs_t	dmar = sci->dmar;
	register char		*msgs;
	unsigned int		offset;


	SCI_TC_GET(dmar,len);
	len = sizeof(scsi_command_group_0) - len;
	PRINT(("{G%d}",len));

	/* wherever it was, take it from there */
	SCI_DMADR_GET(dmar,offset);
	msgs = (char*)sci->buff + offset - len;

	if ((len == 0) || (len > 2))
		ok = FALSE;
	else {
		/* A SDP message preceeds it in non-completed READs */
		ok = ((msgs[0] == SCSI_DISCONNECT) ||	/* completed op */
		      ((msgs[0] == SCSI_SAVE_DATA_POINTER) && /* incomplete */
		       (msgs[1] == SCSI_DISCONNECT)));
	}
	if (!ok)
		printf("[tgt %d bad msg (%d): %x]",
			sci->active_target->target_id, len, *msgs);

	return TRUE;
}

/* save all relevant data, free the BUS */
boolean_t
sci_disconnected(sci, csr, bs)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	sci_padded_regmap_t		*regs = sci->regs;

	regs->sci_mode &= ~(SCI_MODE_MONBSY|SCI_MODE_DMA);
	SCI_CLR_INTR(regs);/*retriggered by MONBSY cuz intr routine did CLR */
	SCI_ACK(regs,SCI_PHASE_DISC);

	LOG(0x16,"disconnected");

	sci_disconnect(sci,csr,bs);

	tgt = sci->active_target;
	tgt->flags |= TGT_DISCONNECTED;
	tgt->transient_state.handler = sci->error_handler;
	/* the rest has been saved in sci_err_disconn() */

	PRINT(("{D%d}", tgt->target_id));

	sci_release_bus(sci);

	return FALSE;
}

/* get reconnect message, restore BUS */
boolean_t
sci_reconnect(sci, csr, bs)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	sci_padded_regmap_t		*regs;
	register int		id;
	int			msg;

	LOG(0x17,"reconnect");

	if (sci->wd.nactive == 0) {
		LOG(2,"SPURIOUS");
		return FALSE;
	}

	regs = sci->regs;

	regs->sci_mode &= ~SCI_MODE_PAR_CHK;
	id = regs->sci_data;/*parity!*/
	regs->sci_mode |= SCI_MODE_PAR_CHK;

	/* xxx check our id is in there */

	id &= ~(1 << sci->sc->initiator_id);
	{
		register int i;
		for (i = 0; i < 8; i++)
			if (id & (1 << i)) break;
if (i == 8) {printf("{P%x}", id);return;}
		id = i;
	}
	regs->sci_icmd = SCI_ICMD_BSY;
	while (regs->sci_bus_csr & SCI_BUS_SEL)
		;
	regs->sci_icmd = 0;
	delay_1p2_us();
	while ( ((regs->sci_bus_csr & SCI_BUS_BSY) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_BSY) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_BSY) == 0))
		;

	regs->sci_mode |= SCI_MODE_MONBSY;

	/* Now should wait for correct phase: REQ signals it */
	while (	((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0) &&
		((regs->sci_bus_csr & SCI_BUS_REQ) == 0))
		;

	/*
	 * See if this reconnection collided with a selection attempt
	 */
	if (sci->state & SCI_STATE_BUSY)
		sci->state |= SCI_STATE_COLLISION;

	sci->state |= SCI_STATE_BUSY;

	/* Get identify msg */
	bs = regs->sci_bus_csr;
if (SCI_CUR_PHASE(bs) != SCSI_PHASE_MSG_IN) gimmeabreak();
	SCI_ACK(regs,SCSI_PHASE_MSG_IN);
	msg = 0;
	sci_data_in(regs, SCSI_PHASE_MSG_IN, 1, &msg);
	regs->sci_mode = SCI_MODE_PAR_CHK|SCI_MODE_DMA|SCI_MODE_MONBSY;
	regs->sci_sel_enb = 0;

	if (msg != SCSI_IDENTIFY)
		printf("{I%x %x}", id, msg);

	tgt = sci->sc->target[id];
	if (id > 7 || tgt == 0) panic("sci_reconnect");

	PRINT(("{R%d}", id));
	if (sci->state & SCI_STATE_COLLISION)
		PRINT(("[B %d-%d]", sci->active_target->target_id, id));

	LOG(0x80+id,0);

	sci->active_target = tgt;
	tgt->flags &= ~TGT_DISCONNECTED;

	sci->script = tgt->transient_state.script;
	sci->error_handler = tgt->transient_state.handler;
	sci->in_count = 0;
	sci->out_count = 0;

	/* Should get a phase mismatch when tgt changes phase */

	return TRUE;
}



/* do the synch negotiation */
boolean_t
sci_dosynch( sci, csr, bs)
	register sci_softc_t	sci;
{
	/*
	 * Phase is MSG_OUT here, cmd has not been xferred
	 */
	int			len;
	register target_info_t	*tgt;
	register sci_padded_regmap_t	*regs = sci->regs;
	unsigned char		off, icmd;
	register unsigned char	*p;

	regs->sci_mode |= SCI_MODE_MONBSY;

	LOG(0x11,"dosync");

	/* ATN still asserted */
	SCI_ACK(regs,SCSI_PHASE_MSG_OUT);
	
	tgt = sci->active_target;

	tgt->flags |= TGT_DID_SYNCH;	/* only one chance */
	tgt->flags &= ~TGT_TRY_SYNCH;

	p = (unsigned char *)tgt->cmd_ptr + tgt->transient_state.cmd_count +
		tgt->transient_state.dma_offset;
	p[0] = SCSI_IDENTIFY;
	p[1] = SCSI_EXTENDED_MESSAGE;
	p[2] = 3;
	p[3] = SCSI_SYNC_XFER_REQUEST;
	/* We cannot run synchronous */
#define sci_to_scsi_period(x)	0xff
#define scsi_period_to_sci(x)	(x)
	off = 0;
	p[4] = sci_to_scsi_period(sci_min_period);
	p[5] = off;

	/* xfer all but last byte with ATN set */
	sci_data_out(regs, SCSI_PHASE_MSG_OUT,
			sizeof(scsi_synch_xfer_req_t), p);
	icmd = regs->sci_icmd & ~(SCI_ICMD_DIFF|SCI_ICMD_TEST);
	icmd &= ~SCI_ICMD_ATN;
	regs->sci_icmd = icmd;
	sci_data_out(regs, SCSI_PHASE_MSG_OUT, 1,
			&p[sizeof(scsi_synch_xfer_req_t)]);

	/* wait for phase change */
	while (regs->sci_csr & SCI_CSR_PHASE_MATCH)
		;
	bs = regs->sci_bus_csr;

	/* The standard sez there nothing else the target can do but.. */
	if (SCI_CUR_PHASE(bs) != SCSI_PHASE_MSG_IN)
		panic("sci_dosync");/* XXX put offline */

msgin:
	/* ack */
	SCI_ACK(regs,SCSI_PHASE_MSG_IN);

	/* get answer */
	len = sizeof(scsi_synch_xfer_req_t);
	len = sci_data_in(regs, SCSI_PHASE_MSG_IN, len, p);

	/* do not cancel the phase mismatch interrupt ! */

	/* look at the answer and see if we like it */
	if (len || (p[0] != SCSI_EXTENDED_MESSAGE)) {
		/* did not like it at all */
		printf(" did not like SYNCH xfer ");
	} else {
		/* will NOT do synch */
		printf(" but we cannot do SYNCH xfer ");
		tgt->sync_period = scsi_period_to_sci(p[3]);
		tgt->sync_offset = p[4];
		/* sanity */
		if (tgt->sync_offset != 0)
			printf(" ?OFFSET %x? ", tgt->sync_offset);
	}

	/* wait for phase change */
	while (regs->sci_csr & SCI_CSR_PHASE_MATCH)
		;
	bs = regs->sci_bus_csr;

	/* phase should be command now */
	/* continue with simple command script */
	sci->error_handler = sci_err_generic;
	sci->script = sci_script_cmd;

	if (SCI_CUR_PHASE(bs) == SCSI_PHASE_CMD )
		return TRUE;

/*	sci->script++;*/
	if (SCI_CUR_PHASE(bs) == SCSI_PHASE_STATUS )
		return TRUE;	/* intr is pending */

	sci->script++;
	if (SCI_CUR_PHASE(bs) == SCSI_PHASE_MSG_IN )
		return TRUE;

	if ((bs & SCI_BUS_BSY) == 0)	/* uhu? disconnected */
		return sci_end_transaction(sci, regs->sci_csr, regs->sci_bus_csr);

	panic("sci_dosynch");
	return FALSE;
}

/*
 * The bus was reset
 */
sci_bus_reset(sci)
	register sci_softc_t	sci;
{
	register target_info_t	*tgt;
	register sci_padded_regmap_t	*regs = sci->regs;
	int			i;

	LOG(0x21,"bus_reset");

	/*
	 * Clear bus descriptor
	 */
	sci->script = 0;
	sci->error_handler = 0;
	sci->active_target = 0;
	sci->next_target = 0;
	sci->state = 0;
	queue_init(&sci->waiting_targets);
	sci->wd.nactive = 0;
	sci_reset(sci, TRUE);

	printf("sci%d: (%d) bus reset ", sci->sc->masterno, ++sci->wd.reset_count);
	delay(scsi_delay_after_reset); /* some targets take long to reset */

	if (sci->sc == 0)	/* sanity */
		return;

	scsi_bus_was_reset(sci->sc);
}

/*
 * Error handlers
 */

/*
 * Generic, default handler
 */
boolean_t
sci_err_generic(sci, csr, bs)
	register sci_softc_t	sci;
{
	register int		cond = sci->script->condition;

	LOG(0x10,"err_generic");

	if (SCI_CUR_PHASE(bs) == SCSI_PHASE_STATUS)
		return sci_err_to_status(sci, csr, bs);
	gimmeabreak();
	return FALSE;
}

/*
 * Handle generic errors that are reported as
 * an unexpected change to STATUS phase
 */
sci_err_to_status(sci, csr, bs)
	register sci_softc_t	sci;
{
	script_t		scp = sci->script;

	LOG(0x20,"err_tostatus");
	while (SCSI_PHASE(scp->condition) != SCSI_PHASE_STATUS)
		scp++;
	sci->script = scp;
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
	sci->done = SCSI_RET_NEED_SENSE;
#endif
	return TRUE;
}

/*
 * Watch for a disconnection
 */
boolean_t
sci_err_disconn(sci, csr, bs)
	register sci_softc_t	sci;
{
	register sci_padded_regmap_t	*regs;
	register sci_dmaregs_t	dmar = sci->dmar;
	register target_info_t	*tgt;
	int			count;

	LOG(0x18,"err_disconn");

	if (SCI_CUR_PHASE(bs) != SCSI_PHASE_MSG_IN)
		return sci_err_generic(sci, csr, bs);

	regs = sci->regs;

	tgt = sci->active_target;

	switch (SCSI_PHASE(sci->script->condition)) {
	case SCSI_PHASE_DATAO:
		LOG(0x1b,"+DATAO");

if (sci_delay & 1) delay(1000);
	/* Stop dma */
	regs->sci_icmd = 0;
	regs->sci_mode &= ~(SCI_MODE_DMA|SCI_MODE_DMA_IE);
	dmar->sci_dma_dir = SCI_DMA_DIR_READ;/* make sure we steal not */

		if (sci->out_count) {
			register int xferred, offset;

			SCI_TC_GET(dmar,xferred);
if (scsi_debug)
printf("{O %x %x}", xferred, sci->out_count);
			/* 5380 prefetches */
			xferred = sci->out_count - xferred - 1;
/*			assert(xferred > 0);*/
			tgt->transient_state.out_count -= xferred;
			assert(tgt->transient_state.out_count > 0);
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset >= PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			sci_copyout( tgt, offset, xferred);

		}
		tgt->transient_state.script = sci_script_data_out;
		break;

	case SCSI_PHASE_DATAI:
		LOG(0x19,"+DATAI");

	/* Stop dma */
	regs->sci_icmd = 0;
	regs->sci_mode &= ~(SCI_MODE_DMA|SCI_MODE_DMA_IE);
	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;/* make sure we steal not */

		if (sci->in_count) {
			register int offset, xferred;
/*			unsigned char	extrab = regs->sci_idata;*/

			SCI_TC_GET(dmar,xferred);
			/* ++EXTRA_BYTE */
if (scsi_debug)
printf("{A %x %x %x}", xferred, sci->in_count, sci->extra_byte);
			xferred = sci->in_count - xferred - sci->extra_byte;
			/* ++EXTRA_BYTE */
			assert(xferred > 0);
			tgt->transient_state.in_count -= xferred;
			assert(tgt->transient_state.in_count > 0);
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset >= PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			/* copy what we got */
			sci_copyin( tgt, offset, xferred, 0, 0/*extrab*/);
		}
		tgt->transient_state.script = sci_script_data_in;
		break;

	case SCSI_PHASE_STATUS:
		/* will have to restart dma */
		SCI_TC_GET(dmar,count);
		if (sci->state & SCI_STATE_DMA_IN) {
			register int offset, xferred;
/*			unsigned char	extrab = regs->sci_idata;*/

			LOG(0x1a,"+STATUS+R");


	/* Stop dma */
	regs->sci_icmd = 0;
	regs->sci_mode &= ~(SCI_MODE_DMA|SCI_MODE_DMA_IE);
	dmar->sci_dma_dir = SCI_DMA_DIR_WRITE;/* make sure we steal not */

			/* ++EXTRA_BYTE */
if (scsi_debug)
printf("{A %x %x %x}", count, sci->in_count, sci->extra_byte);
			xferred = sci->in_count - count - sci->extra_byte;
			/* ++EXTRA_BYTE */
			assert(xferred > 0);
			tgt->transient_state.in_count -= xferred;
/*			assert(tgt->transient_state.in_count > 0);*/
			offset = tgt->transient_state.dma_offset;
			tgt->transient_state.dma_offset += xferred;
			if (tgt->transient_state.dma_offset >= PER_TGT_BUFF_SIZE)
				tgt->transient_state.dma_offset = 0;

			/* copy what we got */
			sci_copyin( tgt, offset, xferred, 0, 0/*/extrab*/);

			tgt->transient_state.script = sci_script_data_in;
			if (tgt->transient_state.in_count == 0)
				tgt->transient_state.script++;

		} else {

			LOG(0x1d,"+STATUS+W");

if (sci_delay & 1) delay(1000);
	/* Stop dma */
	regs->sci_icmd = 0;
	regs->sci_mode &= ~(SCI_MODE_DMA|SCI_MODE_DMA_IE);
	dmar->sci_dma_dir = SCI_DMA_DIR_READ;/* make sure we steal not */

if (scsi_debug)
printf("{O %x %x}", count, sci->out_count);
			if ((count == 0) && (tgt->transient_state.out_count == sci->out_count)) {
				/* all done */
				tgt->transient_state.script = &sci_script_data_out[1];
				tgt->transient_state.out_count = 0;
			} else {
				register int xferred, offset;

				/* how much we xferred */
				xferred = sci->out_count - count - 1;/*prefetch*/

				tgt->transient_state.out_count -= xferred;
				assert(tgt->transient_state.out_count > 0);
				offset = tgt->transient_state.dma_offset;
				tgt->transient_state.dma_offset += xferred;
				if (tgt->transient_state.dma_offset >= PER_TGT_BUFF_SIZE)
					tgt->transient_state.dma_offset = 0;

				sci_copyout( tgt, offset, xferred);

				tgt->transient_state.script = sci_script_data_out;
			}
			sci->out_count = 0;
		}
		break;
	default:
		gimmeabreak();
	}
	sci->extra_byte = 0;

/*	SCI_ACK(regs,SCSI_PHASE_MSG_IN); later */
	(void) sci_msg_in(sci,csr,bs);

	regs->sci_sel_enb = (1 << sci->sc->initiator_id);

	sci->script = sci_script_disconnect;

	return FALSE;
}

/*
 * Watchdog
 *
 */
sci_reset_scsibus(sci)
        register sci_softc_t    sci;
{
        register target_info_t  *tgt = sci->active_target;
        if (tgt) {
		int cnt;
		SCI_TC_GET(sci->dmar,cnt);
                log(	LOG_KERN,
			"Target %d was active, cmd x%x in x%x out x%x Sin x%x Sou x%x dmalen x%x\n",
                        tgt->target_id, tgt->cur_cmd,
                        tgt->transient_state.in_count, tgt->transient_state.out_count,
                        sci->in_count, sci->out_count, cnt);
	}
        sci->regs->sci_icmd = SCI_ICMD_RST;
        delay(25);
}

/*
 * Copy routines
 */
/*static*/
sci_copyin(tgt, offset, len, isaobb, obb)
	register target_info_t	*tgt;
	unsigned char obb;
{
	register char	*from, *to;
	register int	count;

	count = tgt->transient_state.copy_count;

	from = tgt->cmd_ptr + offset;
	to = tgt->ior->io_data + count;
	tgt->transient_state.copy_count = count + len;

	bcopy( from, to, len);
	/* check for last, poor little odd byte */
	if (isaobb) {
		to += len;
		to[-1] = obb;
	}
}

/*static*/
sci_copyout( tgt, offset, len)
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
		to = tgt->cmd_ptr + offset;

		bcopy(from, to, len);

	}
}

#endif	/*NSCI > 0*/

