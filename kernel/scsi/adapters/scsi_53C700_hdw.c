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
 *	File: scsi_53C700_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/91
 *
 *	Bottom layer of the SCSI driver: chip-dependent functions
 *
 *	This file contains the code that is specific to the NCR 53C700
 *	SCSI chip (Host Bus Adapter in SCSI parlance): probing, start
 *	operation, and interrupt routine.
 */


#include <siop.h>
#if	NSIOP > 0
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

#include <scsi/adapters/scsi_53C700.h>

#ifdef	PAD
typedef struct {
	volatile unsigned char	siop_scntl0;	/* rw: SCSI control reg 0 */
	PAD(pad0);
	volatile unsigned char	siop_scntl1;	/* rw: SCSI control reg 1 */
	PAD(pad1);
	volatile unsigned char	siop_sdid;	/* rw: SCSI Destination ID */
	PAD(pad2);
	volatile unsigned char	siop_sien;	/* rw: SCSI Interrupt Enable */
	PAD(pad3);
	volatile unsigned char	siop_scid;	/* rw: SCSI Chip ID reg */
	PAD(pad4);
	volatile unsigned char	siop_sxfer;	/* rw: SCSI Transfer reg */
	PAD(pad5);
	volatile unsigned char	siop_sodl;	/* rw: SCSI Output Data Latch */
	PAD(pad6);
	volatile unsigned char	siop_socl;	/* rw: SCSI Output Control Latch */
	PAD(pad7);
	volatile unsigned char	siop_sfbr;	/* ro: SCSI First Byte Received */
	PAD(pad8);
	volatile unsigned char	siop_sidl;	/* ro: SCSI Input Data Latch */
	PAD(pad9);
	volatile unsigned char	siop_sbdl;	/* ro: SCSI Bus Data Lines */
	PAD(pad10);
	volatile unsigned char	siop_sbcl;	/* ro: SCSI Bus Control Lines */
	PAD(pad11);
	volatile unsigned char	siop_dstat;	/* ro: DMA status */
	PAD(pad12);
	volatile unsigned char	siop_sstat0;	/* ro: SCSI status reg 0 */
	PAD(pad13);
	volatile unsigned char	siop_sstat1;	/* ro: SCSI status reg 1 */
	PAD(pad14);
	volatile unsigned char	siop_sstat2;	/* ro: SCSI status reg 2 */
	PAD(pad15);
	volatile unsigned char	siop_res1;
	PAD(pad16);
	volatile unsigned char	siop_res2;
	PAD(pad17);
	volatile unsigned char	siop_res3;
	PAD(pad18);
	volatile unsigned char	siop_res4;
	PAD(pad19);
	volatile unsigned char	siop_ctest0;	/* ro: Chip test register 0 */
	PAD(pad20);
	volatile unsigned char	siop_ctest1;	/* ro: Chip test register 1 */
	PAD(pad21);
	volatile unsigned char	siop_ctest2;	/* ro: Chip test register 2 */
	PAD(pad22);
	volatile unsigned char	siop_ctest3;	/* ro: Chip test register 3 */
	PAD(pad23);
	volatile unsigned char	siop_ctest4;	/* rw: Chip test register 4 */
	PAD(pad24);
	volatile unsigned char	siop_ctest5;	/* rw: Chip test register 5 */
	PAD(pad25);
	volatile unsigned char	siop_ctest6;	/* rw: Chip test register 6 */
	PAD(pad26);
	volatile unsigned char	siop_ctest7;	/* rw: Chip test register 7 */
	PAD(pad27);
	volatile unsigned char	siop_temp0;	/* rw: Temporary Stack reg */
	PAD(pad28);
	volatile unsigned char	siop_temp1;
	PAD(pad29);
	volatile unsigned char	siop_temp2;
	PAD(pad30);
	volatile unsigned char	siop_temp3;
	PAD(pad31);
	volatile unsigned char	siop_dfifo;	/* rw: DMA FIFO */
	PAD(pad32);
	volatile unsigned char	siop_istat;	/* rw: Interrupt Status reg */
	PAD(pad33);
	volatile unsigned char	siop_res5;
	PAD(pad34);
	volatile unsigned char	siop_res6;
	PAD(pad35);
	volatile unsigned char	siop_dbc0;	/* rw: DMA Byte Counter reg */
	PAD(pad36);
	volatile unsigned char	siop_dbc1;
	PAD(pad37);
	volatile unsigned char	siop_dbc2;
	PAD(pad38);
	volatile unsigned char	siop_dcmd;	/* rw: DMA Command Register */
	PAD(pad39);
	volatile unsigned char	siop_dnad0;	/* rw: DMA Next Address */
	PAD(pad40);
	volatile unsigned char	siop_dnad1;
	PAD(pad41);
	volatile unsigned char	siop_dnad2;
	PAD(pad42);
	volatile unsigned char	siop_dnad3;
	PAD(pad43);
	volatile unsigned char	siop_dsp0;	/* rw: DMA SCRIPTS Pointer reg */
	PAD(pad44);
	volatile unsigned char	siop_dsp1;
	PAD(pad45);
	volatile unsigned char	siop_dsp2;
	PAD(pad46);
	volatile unsigned char	siop_dsp3;
	PAD(pad47);
	volatile unsigned char	siop_dsps0;	/* rw: DMA SCRIPTS Pointer Save reg */
	PAD(pad48);
	volatile unsigned char	siop_dsps1;
	PAD(pad49);
	volatile unsigned char	siop_dsps2;
	PAD(pad50);
	volatile unsigned char	siop_dsps3;
	PAD(pad51);
	volatile unsigned char	siop_dmode;	/* rw: DMA Mode reg */
	PAD(pad52);
	volatile unsigned char	siop_res7;
	PAD(pad53);
	volatile unsigned char	siop_res8;
	PAD(pad54);
	volatile unsigned char	siop_res9;
	PAD(pad55);
	volatile unsigned char	siop_res10;
	PAD(pad56);
	volatile unsigned char	siop_dien;	/* rw: DMA Interrupt Enable */
	PAD(pad57);
	volatile unsigned char	siop_dwt;	/* rw: DMA Watchdog Timer */
	PAD(pad58);
	volatile unsigned char	siop_dcntl;	/* rw: DMA Control reg */
	PAD(pad59);
	volatile unsigned char	siop_res11;
	PAD(pad60);
	volatile unsigned char	siop_res12;
	PAD(pad61);
	volatile unsigned char	siop_res13;
	PAD(pad62);
	volatile unsigned char	siop_res14;
	PAD(pad63);
} siop_padded_regmap_t;
#else
typedef siop_regmap_t	siop_padded_regmap_t;
#endif

/*
 * Macros to make certain things a little more readable
 */

/* forward decls */

int	siop_reset_scsibus();
boolean_t siop_probe_target();

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) 53C700 interface
 */
struct siop_softc {
	watchdog_t	wd;
	siop_padded_regmap_t	*regs;		/* 53C700 registers */
	scsi_dma_ops_t	*dma_ops;	/* DMA operations and state */
	opaque_t	dma_state;

	script_t	script;
	int		(*error_handler)();
	int		in_count;	/* amnt we expect to receive */
	int		out_count;	/* amnt we are going to ship */

	volatile char	state;
#define	SIOP_STATE_BUSY		0x01	/* selecting or currently connected */
#define SIOP_STATE_TARGET	0x04	/* currently selected as target */
#define SIOP_STATE_COLLISION	0x08	/* lost selection attempt */
#define SIOP_STATE_DMA_IN	0x10	/* tgt --> initiator xfer */

	unsigned char	ntargets;	/* how many alive on this scsibus */
	unsigned char	done;

	scsi_softc_t	*sc;
	target_info_t	*active_target;

	target_info_t	*next_target;	/* trying to seize bus */
	queue_head_t	waiting_targets;/* other targets competing for bus */

} siop_softc_data[NSIOP];

typedef struct siop_softc *siop_softc_t;

siop_softc_t	siop_softc[NSIOP];

/*
 * Definition of the controller for the auto-configuration program.
 */

int	siop_probe(), scsi_slave(), scsi_attach(), siop_go(), siop_intr();

caddr_t	siop_std[NSIOP] = { 0 };
struct	bus_device *siop_dinfo[NSIOP*8];
struct	bus_ctlr *siop_minfo[NSIOP];
struct	bus_driver siop_driver = 
        { siop_probe, scsi_slave, scsi_attach, siop_go, siop_std, "rz", siop_dinfo,
	  "siop", siop_minfo, BUS_INTR_B4_PROBE};

/*
 * Scripts
 */
struct script
siop_script_data_in[] = {
},

siop_script_data_out[] = {
},

siop_script_cmd[] = {
},

/* Synchronous transfer neg(oti)ation */

siop_script_try_synch[] = {
},

/* Disconnect sequence */

siop_script_disconnect[] = {
};


#define	DEBUG
#ifdef	DEBUG

siop_state(base)
	vm_offset_t	base;
{
	siop_padded_regmap_t	*regs;
....
	return 0;
}
siop_target_state(tgt)
	target_info_t		*tgt;
{
	if (tgt == 0)
		tgt = siop_softc[0]->active_target;
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
		db_printf(": %x ", spt->condition);
		db_printsym(spt->action,1);
		db_printf(", ");
		db_printsym(tgt->transient_state.handler, 1);
		db_printf("\n");
	}

	return 0;
}

siop_all_targets(unit)
{
	int i;
	target_info_t	*tgt;
	for (i = 0; i < 8; i++) {
		tgt = siop_softc[unit]->sc->target[i];
		if (tgt)
			siop_target_state(tgt);
	}
}

siop_script_state(unit)
{
	script_t	spt = siop_softc[unit]->script;

	if (spt == 0) return 0;
	db_printsym(spt,1);
	db_printf(": %x ", spt->condition);
	db_printsym(spt->action,1);
	db_printf(", ");
	db_printsym(siop_softc[unit]->error_handler, 1);
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
int siop_logpt;
char siop_log[LOGSIZE];

#define MAXLOG_VALUE	0x24
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static LOG(e,f)
	char *f;
{
	siop_log[siop_logpt++] = (e);
	if (siop_logpt == LOGSIZE) siop_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

siop_print_log(skip)
	int skip;
{
	register int i, j;
	register unsigned char c;

	for (i = 0, j = siop_logpt; i < LOGSIZE; i++) {
		c = siop_log[j];
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

siop_print_stat()
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
siop_probe(reg, ui)
	char		*reg;
	struct bus_ctlr	*ui;
{
	int             unit = ui->unit;
	siop_softc_t     siop = &siop_softc_data[unit];
	int		target_id, i;
	scsi_softc_t	*sc;
	register siop_padded_regmap_t	*regs;
	int		s;
	boolean_t	did_banner = FALSE;
	char		*cmd_ptr;
	static char	*here = "siop_probe";

	/*
	 * We are only called if the chip is there,
	 * but make sure anyways..
	 */
	regs = (siop_padded_regmap_t *) (reg);
	if (check_memory(regs, 0))
		return 0;

#if	notyet
	/* Mappable version side */
	SIOP_probe(reg, ui);
#endif

	/*
	 * Initialize hw descriptor
	 */
	siop_softc[unit] = siop;
	siop->regs = regs;

	if ((siop->dma_ops = (scsi_dma_ops_t *)siop_std[unit]) == 0)
		/* use same as unit 0 if undefined */
		siop->dma_ops = (scsi_dma_ops_t *)siop_std[0];
	siop->dma_state = (*siop->dma_ops->init)(unit, reg);

	queue_init(&siop->waiting_targets);

	sc = scsi_master_alloc(unit, siop);
	siop->sc = sc;

	sc->go = siop_go;
	sc->probe = siop_probe_target;
	sc->watchdog = scsi_watchdog;
	siop->wd.reset = siop_reset_scsibus;

#ifdef	MACH_KERNEL
	sc->max_dma_data = -1;				/* unlimited */
#else
	sc->max_dma_data = scsi_per_target_virtual;
#endif

	/*
	 * Reset chip
	 */
	s = splbio();
	siop_reset(siop, TRUE);

	/*
	 * Our SCSI id on the bus.
	 */

	sc->initiator_id = my_scsi_id(unit);
	printf("%s%d: my SCSI id is %d", ui->name, unit, sc->initiator_id);

	/*
	 * For all possible targets, see if there is one and allocate
	 * a descriptor for it if it is there.
	 */
	for (target_id = 0; target_id < 8; target_id++) {

		register unsigned csr, dsr;
		scsi_status_byte_t	status;

		/* except of course ourselves */
		if (target_id == sc->initiator_id)
			continue;

		.....

		printf(",%s%d", did_banner++ ? " " : " target(s) at ",
				target_id);

		.....


		/*
		 * Found a target
		 */
		siop->ntargets++;
		{
			register target_info_t	*tgt;

			tgt = scsi_slave_alloc(unit, target_id, siop);

			tgt->cmd_ptr = ...
			tgt->dma_ptr = ...
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
siop_probe_target(sc, tgt, ior)
	scsi_softc_t		*sc;
	target_info_t		*tgt;
	io_req_t		ior;
{
	siop_softc_t     siop = siop_softc[sc->masterno];
	boolean_t	newlywed;

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		/* desc was allocated afresh */

		tgt->cmd_ptr = ...
		tgt->dma_ptr = ...
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_init(&tgt->fdma, scsi_per_target_virtual);
#endif	/*MACH_KERNEL*/

	}

	if (scsi_inquiry(sc, tgt, SCSI_INQ_STD_DATA) == SCSI_RET_DEVICE_DOWN)
		return FALSE;

	tgt->flags = TGT_ALIVE;
	return TRUE;
}


static siop_wait(preg, until)
	volatile unsigned char	*preg;
{
	int timeo = 1000000;
	while ((*preg & until) != until) {
		delay(1);
		if (!timeo--) {
			printf("siop_wait TIMEO with x%x\n", *preg);
			break;
		}
	}
	return *preg;
}


siop_reset(siop, quickly)
	siop_softc_t		siop;
	boolean_t		quickly;
{
	register siop_padded_regmap_t	*regs = siop->regs;

	....

	if (quickly)
		return;

	/*
	 * reset the scsi bus, the interrupt routine does the rest
	 * or you can call siop_bus_reset().
	 */
	....
	
}

/*
 *	Operational functions
 */

/*
 * Start a SCSI command on a target
 */
siop_go(sc, tgt, cmd_count, in_count, cmd_only)
	scsi_softc_t		*sc;
	target_info_t		*tgt;
	boolean_t		cmd_only;
{
	siop_softc_t		siop;
	register int		s;
	boolean_t		disconn;
	script_t		scp;
	boolean_t		(*handler)();

	LOG(1,"go");

	siop = (siop_softc_t)tgt->hw_state;

	....
}

siop_attempt_selection(siop)
	siop_softc_t	siop;
{
	target_info_t	*tgt;
	register int	out_count;
	siop_padded_regmap_t	*regs;
	register int	cmd;
	boolean_t	ok;
	scsi_ret_t	ret;

	regs = siop->regs;
	tgt = siop->next_target;

	LOG(4,"select");
	LOG(0x80+tgt->target_id,0);

	/*
	 * Init bus state variables and set registers.
	 */
	siop->active_target = tgt;

	/* reselection pending ? */
	......
}

/*
 * Interrupt routine
 *	Take interrupts from the chip
 *
 * Implementation:
 *	Move along the current command's script if
 *	all is well, invoke error handler if not.
 */
siop_intr(unit)
{
	register siop_softc_t	siop;
	register script_t	scp;
	register unsigned	csr, bs, cmd;
	register siop_padded_regmap_t	*regs;
	boolean_t		try_match;
#if	notyet
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface)
		return SIOP_intr(unit);
#endif

	LOG(5,"\n\tintr");

	siop = siop_softc[unit];
	regs = siop->regs;

	/* ack interrupt */
	....
}


siop_target_intr(siop)
	register siop_softc_t	siop;
{
	panic("SIOP: TARGET MODE !!!\n");
}

/*
 * All the many little things that the interrupt
 * routine might switch to
 */

#endif	/*NSIOP > 0*/

