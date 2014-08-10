/* 
 * Mach Operating System
 * Copyright (c) 1992,1991 Carnegie Mellon University
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
 *	File: scsi_89352_hdw.c
 * 	Author: Daniel Stodolsky, Carnegie Mellon University
 *	Date:	06/91
 *
 *	Bottom layer of the SCSI driver: chip-dependent functions
 *
 *	This file contains the code that is specific to the Fujitsu MB89352
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

/*
 *
 *
 * Known Headaches/Features with this chip.
 *
 * (1) After the interrupt raised by select, the phase sense (psns) 
 *     and SPC status (ssts) registers do not display the correct values
 *     until the REQ line (via psns) is high. (danner@cs.cmu.edu 6/11/91) 
 *
 * (2) After a data in phase, the command complete interrupt may be raised
 *     before the psns, ssts, and transfer counter registers settle. The reset
 *     acknowledge or request command should not be issued until they settle. 
 *     (danner@cs.cmu.edu 6/14/91)  
 * 
 * (3) In general, an interrupt can be raised before the psns and ssts have 
 *     meaningful values. One should wait for the psns to show the REQ bit (0x80)
 *     set before expecting meaningful values, with the exception of (2) above. 
 *     Currently this is handled by spc_err_generic ("Late REQ"). (This problem
 *     is really a refinement of (1)). (danner@cs.cmu.edu 6/14/91)  
 *
 * (4) When issuing a multibyte command after a select with attention,
 *     The chip will automatically drop ATN before sending the last byte of the
 *     message, in accordance with the ANSI SCSI standard. This requires, of course,
 *     the transfer counter be an accurate representation of the amount of data to be
 *     transfered. (danner@cs.cmu.edu 6/14/91)  
 *
 */

#if 0

#include <platforms.h>

#include <scsi.h>

#if	NSCSI > 0

#include <mach/std_types.h>
#include <sys/types.h>
#include <chips/busses.h>
#include <scsi/compat_30.h>
#include <sys/syslog.h>
#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>
#include <scsi/adapters/scsi_89352.h>

#include <machine/db_machdep.h> /*4proto*/
#include <ddb/db_sym.h> /*4proto*/

#ifdef	LUNA88K
#include <luna88k/board.h>
#define	SPC_DEFAULT_ADDRESS	(caddr_t) SCSI_ADDR
#endif

#ifndef	SPC_DEFAULT_ADDRESS	/* cross compile check */
#define	SPC_DEFAULT_ADDRESS	(caddr_t) 0
#endif


/* external/forward declarations */
int spc_probe(), spc_slave(), spc_attach(), scsi_go();
void spc_reset(), spc_attempt_selection(), spc_target_intr(), spc_bus_reset();
/*
 * Statically allocated command & temp buffers
 * This way we can attach/detach drives on-fly
 */
#define	PER_TGT_BUFF_DATA	256

static char	spc_buffer[NSCSI * 8 * PER_TGT_BUFF_DATA];

/*
 * Macros to make certain things a little more readable
 */

/*
  wait for the desired phase to appear, but make sure the REQ bit set in the psns
  (otherwise the values tend to float/be garbage.
*/

#define SPC_WAIT_PHASE(p)  while(((regs->spc_psns & (SPC_BUS_REQ|SCSI_PHASE_MASK)))  \
				 != (SPC_BUS_REQ|(p)))

/*
  wait until a phase different than p appears in the psns. Since it is only valid
  when the REQ bit is set, don't test unless REQ bit is set. So spin until
  REQ is high or the phase is not p.
*/

#define SPC_WAIT_PHASE_VANISH(p) while(1) { int _psns_ = regs->spc_psns; \
    if ((_psns_ & SPC_BUS_REQ) && (_psns_ & SCSI_PHASE_MASK)!=p) break; }
					


/* ?? */
/* #define	SPC_ACK(ptr,phase)	(ptr)->spc_pctl = (phase) */

/*
 * A script has a two parts: a pre-condition and an action.
 * The first triggers error handling if not satisfied and in
 * our case it is formed by the current bus phase and connected
 * condition as per bus status bits.  The action part is just a
 * function pointer, invoked in a standard way.  The script
 * pointer is advanced only if the action routine returns TRUE.
 * See spc_intr() for how and where this is all done.
 */

typedef struct script {
	char	condition;	/* expected state at interrupt */
	int	(*action)();	/* action routine */
} *script_t;

#define	SCRIPT_MATCH(psns) (SPC_CUR_PHASE((psns))|((psns) & SPC_BUS_BSY))

/* ?? */
#define	SPC_PHASE_DISC	0x0	/* sort of .. */

/* The active script is in the state expected right after the issue of a select */

#define SCRIPT_SELECT(scp) (scp->action == spc_issue_command || \
                            scp->action == spc_issue_ident_and_command)

/* forward decls of script actions */
boolean_t
	spc_dosynch(),			/* negotiate synch xfer */
	spc_xfer_in(),			/* get data from target via dma */
	spc_xfer_out(),			/* send data to target via dma */
	spc_get_status(),		/* get status from target */
	spc_end_transaction(),		/* all come to an end */
	spc_msg_in(),			/* get disconnect message(s) */
        spc_issue_command(),            /* spit on the bus */
        spc_issue_ident_and_command(),  /* spit on the bus (with ATN) */
	spc_disconnected();		/* current target disconnected */
/* forward decls of error handlers */
boolean_t
	spc_err_generic(),		/* generic error handler */
	spc_err_disconn();		/* when a target disconnects */
void 	gimmeabreak();			/* drop into the debugger */

void	spc_reset_scsibus();
boolean_t spc_probe_target();

scsi_ret_t	spc_select_target();

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) 89352 chip
 */
struct spc_softc {
	watchdog_t	wd;
	spc_regmap_t	*regs;		/* 5380 registers */
	char		*buff;		/* scratch buffer memory */
	char		*data_ptr;	/* orig/dest memory */
	script_t	script;
	int		(*error_handler)();
	int		in_count;	/* amnt we expect to receive */
	int		out_count;	/* amnt we are going to ship */

	volatile char	state;
#define	SPC_STATE_BUSY		0x01	/* selecting or currently connected */
#define SPC_STATE_TARGET	0x04	/* currently selected as target */
#define SPC_STATE_COLLISION	0x08	/* lost selection attempt */
#define SPC_STATE_DMA_IN	0x10	/* tgt --> initiator xfer */

	unsigned char	ntargets;	/* how many alive on this scsibus */
	unsigned char	done;
	unsigned char	xxxx;

	scsi_softc_t	*sc;
	target_info_t	*active_target;

	target_info_t	*next_target;	/* trying to seize bus */
	queue_head_t	waiting_targets;/* other targets competing for bus */
	decl_simple_lock_data(,chiplock) /* Interlock */
} spc_softc_data[NSCSI];

typedef struct spc_softc *spc_softc_t;

spc_softc_t	spc_softc[NSCSI];

/*
 * Definition of the controller for the auto-configuration program.
 */

int	spc_probe(), scsi_slave(), spc_go();
void	spc_intr();
void    scsi_attach();

vm_offset_t spc_std[NSCSI] = { SPC_DEFAULT_ADDRESS };

struct	bus_device *spc_dinfo[NSCSI*8];
struct	bus_ctlr *spc_minfo[NSCSI];
struct	bus_driver spc_driver = 
        { spc_probe, scsi_slave, scsi_attach, spc_go, spc_std, "rz", spc_dinfo,
	  "spc", spc_minfo, BUS_INTR_B4_PROBE};

/*
 * Scripts
 */

struct script
spc_script_data_in[] = {
        { SCSI_PHASE_CMD|SPC_BUS_BSY, spc_issue_command},
	{ SCSI_PHASE_DATAI|SPC_BUS_BSY, spc_xfer_in},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},

spc_script_late_data_in[] = {
        { SCSI_PHASE_MSG_OUT|SPC_BUS_BSY, spc_issue_ident_and_command},
	{ SCSI_PHASE_DATAI|SPC_BUS_BSY, spc_xfer_in},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},

spc_script_data_out[] = {
        { SCSI_PHASE_CMD|SPC_BUS_BSY, spc_issue_command},
	{ SCSI_PHASE_DATAO|SPC_BUS_BSY, spc_xfer_out},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},


spc_script_late_data_out[] = {
        { SCSI_PHASE_MSG_OUT|SPC_BUS_BSY, spc_issue_ident_and_command},
	{ SCSI_PHASE_DATAO|SPC_BUS_BSY, spc_xfer_out},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},


spc_script_cmd[] = {
        { SCSI_PHASE_CMD|SPC_BUS_BSY, spc_issue_command},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},

spc_script_late_cmd[] = {
        { SCSI_PHASE_MSG_OUT|SPC_BUS_BSY, spc_issue_ident_and_command},
	{ SCSI_PHASE_STATUS|SPC_BUS_BSY, spc_get_status},
	{ SCSI_PHASE_MSG_IN|SPC_BUS_BSY, spc_end_transaction}
},

/* Synchronous transfer neg(oti)ation */

spc_script_try_synch[] = {
	{ SCSI_PHASE_MSG_OUT|SPC_BUS_BSY, spc_dosynch}
},

/* Disconnect sequence */

spc_script_disconnect[] = {
	{ SPC_PHASE_DISC, spc_disconnected}
};



#define	u_min(a,b)	(((a) < (b)) ? (a) : (b))


#define	DEBUG
#ifdef	DEBUG

int spc_state(base)
	vm_offset_t	base;
{
	register spc_regmap_t	*regs;

	if (base == 0)
		base = (vm_offset_t) SPC_DEFAULT_ADDRESS;

	regs = (spc_regmap_t*) (base);

	db_printf("spc_bdid (bus device #):          %x\n",regs->spc_bdid);
	db_printf("spc_sctl (spc internal control):  %x\n",regs->spc_sctl);
	db_printf("spc_scmd (scp command):           %x\n",regs->spc_scmd);
	db_printf("spc_ints (spc interrupt):         %x\n",regs->spc_ints);
	db_printf("spc_psns (scsi bus phase):        %x\n",regs->spc_psns);
	db_printf("spc_ssts (spc internal status):   %x\n",regs->spc_ssts);
	db_printf("spc_serr (spc internal err stat): %x\n",regs->spc_serr);
	db_printf("spc_pctl (scsi transfer phase):   %x\n",regs->spc_pctl);
	db_printf("spc_mbc  (spc transfer data ct):  %x\n",regs->spc_mbc);
/*	db_printf("spc_dreg (spc transfer data r/w): %x\n",regs->spc_dreg);*/
	db_printf("spc_temp (scsi data bus control): %x\n",regs->spc_temp);
	db_printf("spc_tch  (transfer byte ct (MSB): %x\n",regs->spc_tch);
	db_printf("spc_tcm  (transfer byte ct (2nd): %x\n",regs->spc_tcm);
	db_printf("spc_tcl  (transfer byte ct (LSB): %x\n",regs->spc_tcl);
	
	return 0;
}

int spc_target_state(tgt)
	target_info_t		*tgt;
{
	if (tgt == 0)
		tgt = spc_softc[0]->active_target;
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
		db_printsym((db_expr_t)spt,1);
		db_printf(": %x ", spt->condition);
		db_printsym((db_expr_t)spt->action,1);
		db_printf(", ");
		db_printsym((db_expr_t)tgt->transient_state.handler, 1);
		db_printf("\n");
	}

	return 0;
}

void spc_all_targets(unit)
int unit;
{
	int i;
	target_info_t	*tgt;
	for (i = 0; i < 8; i++) {
		tgt = spc_softc[unit]->sc->target[i];
		if (tgt)
			spc_target_state(tgt);
	}
}

int spc_script_state(unit)
int unit;
{
	script_t	spt = spc_softc[unit]->script;

	if (spt == 0) return 0;
	db_printsym((db_expr_t)spt,1);
	db_printf(": %x ", spt->condition);
	db_printsym((db_expr_t)spt->action,1);
	db_printf(", ");
	db_printsym((db_expr_t)spc_softc[unit]->error_handler, 1);
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
int spc_logpt;
int spc_log[LOGSIZE];

#define MAXLOG_VALUE	0x30
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static void LOG(e,f)
	int e;
	char *f;
{
	spc_log[spc_logpt++] = (e);
	if (spc_logpt == LOGSIZE) spc_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

int spc_print_log(skip)
	int skip;
{
	register int i, j;
	register unsigned int c;

	for (i = 0, j = spc_logpt; i < LOGSIZE; i++) {
		c = spc_log[j];
		if (++j == LOGSIZE) j = 0;
		if (skip-- > 0)
			continue;
		if (c < MAXLOG_VALUE)
			db_printf(" %s", logtbl[c].name);
		else
			db_printf("-0x%x", c - 0x80);
	}
	db_printf("\n");
	return 0;
}

void spc_print_stat()
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
int spc_probe(reg, ui)
	char		*reg;
	struct bus_ctlr	*ui;
{
  	int 		tmp;
	int             unit = ui->unit;
	spc_softc_t     spc = &spc_softc_data[unit];
	int		target_id, i;
	scsi_softc_t	*sc;
	register spc_regmap_t	*regs;
	int		s;
	boolean_t	did_banner = FALSE;
	char		*cmd_ptr;

	/*
	 * We are only called if the chip is there,
	 * but make sure anyways..
	 */
	regs = (spc_regmap_t *) (reg);
	if (check_memory((unsigned)regs, 0))
		return 0;

#if	notyet
	/* Mappable version side */
	SPC_probe(reg, ui);
#endif

	/*
	 * Initialize hw descriptor
	 */
	spc_softc[unit] = spc;
	spc->regs = regs;
	spc->buff = spc_buffer;

	queue_init(&spc->waiting_targets);

	simple_lock_init(&spc->chiplock);

	sc = scsi_master_alloc(unit, (char*)spc);
	spc->sc = sc;

	sc->go = spc_go;
	sc->probe = spc_probe_target;
	sc->watchdog = scsi_watchdog;
	spc->wd.reset = spc_reset_scsibus;

#ifdef	MACH_KERNEL
	sc->max_dma_data = -1;				/* unlimited */
#else
	sc->max_dma_data = scsi_per_target_virtual;
#endif

	scsi_might_disconnect[unit] = 0;	/* XXX for now */

	/*
	 * Reset chip
	 */
	s = splbio();
	spc_reset(regs, TRUE);
	tmp = regs->spc_ints = regs->spc_ints;

	/*
	 * Our SCSI id on the bus.
	 */

	sc->initiator_id = bdid_to_id(regs->spc_bdid);
	printf("%s%d: my SCSI id is %d", ui->name, unit, sc->initiator_id);

	/*
	 * For all possible targets, see if there is one and allocate
	 * a descriptor for it if it is there.
	 */
	cmd_ptr = spc_buffer;
	for (target_id = 0; target_id < 8; target_id++, cmd_ptr += PER_TGT_BUFF_DATA) {

		register unsigned csr, ints;
		scsi_status_byte_t	status;

		/* except of course ourselves */
		if (target_id == sc->initiator_id)
			continue;

		if (spc_select_target( regs, sc->initiator_id, target_id, FALSE) 
		    == SCSI_RET_DEVICE_DOWN) {
		        tmp = regs->spc_ints = regs->spc_ints;
			continue;
		}

		printf(",%s%d", did_banner++ ? " " : " target(s) at ",
				target_id);

		/* should be command phase here: we selected wo ATN! */
		SPC_WAIT_PHASE(SCSI_PHASE_CMD);

		SPC_ACK(regs,SCSI_PHASE_CMD);

		/* build command in buffer */
		{
			unsigned char	*p = (unsigned char*) cmd_ptr;

			p[0] = SCSI_CMD_TEST_UNIT_READY;
			p[1] = 
			p[2] = 
			p[3] = 
			p[4] = 
			p[5] = 0;
		}

		spc_data_out(regs, SCSI_PHASE_CMD, 6, cmd_ptr);

		SPC_WAIT_PHASE(SCSI_PHASE_STATUS);

		/* should have recieved a Command Complete Interrupt */
		while (!(regs->spc_ints))
		  delay(1);
		ints = regs->spc_ints;
		if (ints != (SPC_INTS_DONE))
		  gimmeabreak(); 
		regs->spc_ints = ints;
		
		SPC_ACK(regs,SCSI_PHASE_STATUS);

		csr = spc_data_in(regs, SCSI_PHASE_STATUS, 1, &status.bits);
		LOG(0x25,"din_count");
		LOG(0x80+csr,0);

		if (status.st.scsi_status_code != SCSI_ST_GOOD)
			scsi_error( 0, SCSI_ERR_STATUS, status.bits, 0);

		/* expect command complete interupt */
		while (!(regs->spc_ints & SPC_INTS_DONE))
		  delay(1);

		/*  clear all intr bits */
		tmp = regs->spc_ints;
		LOG(0x26,"ints");
		LOG(0x80+tmp,0);
		regs->spc_ints = SPC_INTS_DONE;

		/* get cmd_complete message */
		SPC_WAIT_PHASE(SCSI_PHASE_MSG_IN);

		SPC_ACK(regs,SCSI_PHASE_MSG_IN);

		csr = spc_data_in(regs,SCSI_PHASE_MSG_IN, 1,(unsigned char*)&i);
		LOG(0x25,"din_count");
		LOG(0x80+csr,0);
		
		while (!(regs->spc_ints & SPC_INTS_DONE))
		  delay(1);

		/*  clear all done intr */
		tmp = regs->spc_ints;
		LOG(0x26,"ints");
		LOG(0x80+tmp,0);
		regs->spc_ints = SPC_INTS_DONE;

		SPC_ACK(regs,SPC_PHASE_DISC);

		/* release the bus */
		regs->spc_pctl = ~SPC_PCTL_BFREE_IE & SPC_PHASE_DISC;
		/* regs->spc_scmd = 0; only in TARGET mode */

		/* wait for disconnected interrupt */
		while (!(regs->spc_ints & SPC_INTS_DISC))
		  delay(1);

		tmp = regs->spc_ints;
		LOG(0x26,"ints");
		LOG(0x80+tmp,0);
		regs->spc_ints = tmp;
	        LOG(0x29,"Probed\n");

		/*
		 * Found a target
		 */
		spc->ntargets++;
		{
			register target_info_t	*tgt;

			tgt = scsi_slave_alloc(unit, target_id, (char*)spc);

			/* "virtual" address for our use */
			tgt->cmd_ptr = cmd_ptr;
			/* "physical" address for dma engine (??) */
			tgt->dma_ptr = 0;
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
spc_probe_target(tgt, ior)
	target_info_t		*tgt;
	io_req_t		ior;
{
	boolean_t	newlywed;

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		/* desc was allocated afresh */

		/* "virtual" address for our use */
		tgt->cmd_ptr = &spc_buffer[PER_TGT_BUFF_DATA*tgt->target_id +
					   (tgt->masterno*8*PER_TGT_BUFF_DATA) ];
		/* "physical" address for dma engine */
		tgt->dma_ptr = 0;
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

int bdid_to_id(bdid)
	register int	bdid;
{
	register int	i;
	for (i = 0; i < 8; i++)
		if (bdid == (1 << i)) break;
	return i;
}

scsi_ret_t
spc_select_target(regs, myid, id, with_atn)
	register spc_regmap_t	*regs;
	unsigned 		myid, id;
	boolean_t		with_atn;
{
	scsi_ret_t		ret = SCSI_RET_RETRY;
	int mask;

	if ((regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL)) 
#ifdef MIPS
	    && (regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL)) 
	    && (regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL))
#endif
	    )
		return ret;

	/* setup for for select: 

#if 0
	 (1) Toggle the Enable transfer bit (turning on the chips
	     SCSI bus drivers).
#endif
         (2) Enable arbitration, parity, reselect display, but 
             disable interrupt generation to the CPU (we are polling).
	 (3) Disable the bus free interrupt and set I/O direction 
	 (4) If doing a select with attention, write the Set attention command.
	     Then delay 1 microsecond to avoid command races.

	 (5) Temp register gets 1<<target | 1<<initiator ids
	 (6) Timeout clocked into transfer registers 
	 (7) Drive select (and optionally attention) onto the bus
	 (8) Wait 1/4 second for timeout.
	 */
	
#if 0
	regs->spc_psns = SPC_DIAG_ENBL_XFER;          /* (1) */
#endif
	
	regs->spc_sctl = SPC_SCTL_ARB_EBL|
	                 SPC_SCTL_PAR_EBL|
			 SPC_SCTL_RSEL_EBL;          /* (2) */


 
	mask = ~SPC_PCTL_BFREE_IE & regs->spc_pctl;  
	mask &= ~1; /* set I/O direction to be out */

	regs->spc_pctl = mask;                        /* (3) */

	if (with_atn)
	  {
	    regs->spc_scmd = SPC_SCMD_C_ATN_S;        /* (4) */
	    delay(1);
	  }

	regs->spc_temp = (1<<myid) | (1<<id);         /* (5) */

	SPC_TC_PUT(regs,0xfa004);                     /* (6) */

	regs->spc_scmd = (SPC_SCMD_C_SELECT | SPC_SCMD_PROGRAMMED_X); /* (7) */

	{
	  int count = 2500;

	  /* wait for an interrupt */
	  while ((regs->spc_ints)==0)
	    {
	      if (--count > 0)
		delay(100);
	      else
		{
		  goto nodev;
		}
	    }

	  count = regs->spc_ints;   
	  if (count & SPC_INTS_TIMEOUT)
	    {
	      /* sanity check. The ssts should have the busy bit set */
	      if (regs->spc_ssts & SPC_SSTS_BUSY)
		goto nodev;
	      else
		panic("spc_select_target: timeout");
	    }

	  /* otherwise, we should have received a 
	     command complete interrupt  */

	  if (count & ~SPC_INTS_DONE)
	    panic("spc_select_target");
	  
	}  /* (8) */
	
	/* we got a response - now connected; bus is in COMMAND phase */
	
	regs->spc_ints = regs->spc_ints;
	/* regs->spc_scmd = 0; target only */
	return SCSI_RET_SUCCESS;
nodev:
	SPC_TC_PUT(regs,0);    /* play it safe */
	regs->spc_ints = regs->spc_ints;
	/* regs->spc_scmd = 0; target only */
	ret = SCSI_RET_DEVICE_DOWN;
	return ret;
}

int spc_data_out(regs, phase, count, data)
	int phase, count;
	register spc_regmap_t	*regs;
	unsigned char		*data;
{
	/* This is the one that sends data out. returns how many
	bytes it did NOT xfer: */

	if (SPC_CUR_PHASE(regs->spc_phase) != phase)
		return count;

	/* check that the fifo is empty. If not, cry */
	if (!(regs->spc_ssts & SPC_SSTS_FIFO_EMPTY))
	  panic("spc_data_out: junk in fifo\n");

	SPC_TC_PUT(regs,count);
	regs->spc_scmd = SPC_SCMD_C_XFER | SPC_SCMD_PROGRAMMED_X;

	/* wait for the SPC to start processing the command */
	while ((regs->spc_ssts & (SPC_SSTS_INI_CON|SPC_SSTS_TGT_CON|SPC_SSTS_BUSY|SPC_SSTS_XIP))
	       != (SPC_SSTS_INI_CON|SPC_SSTS_BUSY|SPC_SSTS_XIP))
	  delay(1);
	
	/* shovel out the data */
	
	while (count)
	  {
	    /* check if interrupt is pending */
	    int ints = regs->spc_ints;
	    int ssts;

	    if (ints)      /* something has gone wrong */
	      break;

	    ssts = regs->spc_ssts;
	    if (ssts & SPC_SSTS_FIFO_FULL) /* full fifo - can't write */
	      delay(1);
	    else
	      { /* spit out a byte */
		regs->spc_dreg = *data;
		data++;
		count--;
	      }
	  }
		

	if (count != 0)
	  {
	    /* need some sort of fifo cleanup if failed */
	    gimmeabreak(); /* Bytes stranded in the fifo */
	  }

	return count;
}

int spc_data_in(regs, phase, count, data)
	int phase, count;
	register spc_regmap_t	*regs;
	unsigned char		*data;
{
	if (SPC_CUR_PHASE(regs->spc_phase) != phase)
		return count;

	SPC_TC_PUT(regs,count);
	regs->spc_scmd = SPC_SCMD_C_XFER | SPC_SCMD_PROGRAMMED_X;

	/* The Fujistu code sample suggests waiting for the top nibble of the SSTS to
	   become 0xb (ssts & 0xf0) = 0xb. This state, however is transient. If the
	   message is short (say , 1 byte), it can get sucked into the fifo before
	   we ever get to look at the state. So instead, we are going to wait for
	   the fifo to become nonempty. 
	   */

	while ((regs->spc_ssts & SPC_SSTS_FIFO_EMPTY))
	  delay(1);

	while (count)
	  {
	    int ints = regs->spc_ints;
	    int ssts;

	    /* If there is an interrupt pending besides command complete or
	       phase mismatch, give up */

	    if (ints & ~(SPC_INTS_DONE|SPC_INTS_BUSREQ))
	      break;

	    /* see if there is any data in the fifo */
	    ssts = regs->spc_ssts;
	    if ((ssts & SPC_SSTS_FIFO_EMPTY) == 0) 
	      {
		*data = regs->spc_dreg;
		data++;
		count--;
		continue;
	      }

	    /* if empty, check if phase has changed */
	    if (SPC_CUR_PHASE(regs->spc_phase) != phase)
	      break;

	  }

	if ((count==0) && (phase == SCSI_PHASE_MSG_IN))
	  {
	    while (!(regs->spc_ints & SPC_INTS_DONE))
	      delay(1);
	    
	    /*   
	      So the command complete interrupt has arrived. Now check that the
	      other two conditions we expect - The psns to be in ack|busy|message_in phase
	      and ssts to indicate connected|xfer in progress|busy|xfer counter 0|empty fifo
	      are true. 
	      */
	    while (1)
	      {
		register int psns = regs->spc_psns;
		register int ssts = regs->spc_ssts;
		register int sscon = ssts & (SPC_SSTS_INI_CON | SPC_SSTS_TGT_CON);
		register int ssncon = ssts & ~(SPC_SSTS_INI_CON | SPC_SSTS_TGT_CON);

		if (psns == (SPC_BUS_ACK | SPC_BUS_BSY | SCSI_PHASE_MSG_IN) &&
		    ssncon == (SPC_SSTS_BUSY | SPC_SSTS_XIP | SPC_SSTS_TC0 | SPC_SSTS_FIFO_EMPTY) &&
		    sscon)
		  break;
	      }

	    regs->spc_scmd = SPC_SCMD_C_ACKREQ_C;
	  }

	return count;
}

void spc_reset(regs, quickly)
	register spc_regmap_t	*regs;
	boolean_t		quickly;
{
	register char		myid;

	/* save our id across reset */
	myid = bdid_to_id(regs->spc_bdid);

	/* wait for Reset In signal to go low */
	while (regs->spc_ssts & SPC_SSTS_RST)
	  delay(1);
	    
	/* reset chip */
	regs->spc_sctl = SPC_SCTL_RESET;
	delay(25);

	regs->spc_myid = myid;
	regs->spc_sctl = SPC_SCTL_ARB_EBL|SPC_SCTL_PAR_EBL|SPC_SCTL_SEL_EBL|
			 SPC_SCTL_RSEL_EBL|SPC_SCTL_IE;
	regs->spc_scmd = SPC_SCMD_C_BUS_RLSE;
	/* regs->spc_tmod = 0; - SANDRO ? */
	regs->spc_ints = 0xff;/* clear off any pending */
#if  0
	regs->spc_pctl = SPC_PCTL_LST_IE; /* useful only on 87033 */
#else
	regs->spc_pctl = 0;
#endif
	regs->spc_mbc = 0;
	SPC_TC_PUT(regs,0);

	if (quickly)
		return;

	/*
	 * reset the scsi bus, the interrupt routine does the rest
	 * or you can call spc_bus_reset().
	 */
	regs->spc_scmd = SPC_SCMD_BUSRST|SPC_SCMD_C_STOP_X;/*?*/
}

/*
 *	Operational functions
 */

/*
 * Start a SCSI command on a target
 */
spc_go(tgt, cmd_count, in_count, cmd_only)
	int cmd_count, in_count;
	target_info_t		*tgt;
	boolean_t		cmd_only;
{
	spc_softc_t		spc;
	register int		s;
	boolean_t		disconn;
	script_t		scp;
	boolean_t		(*handler)();
	int                     late; 

	LOG(1,"\n\tgo");

	spc = (spc_softc_t)tgt->hw_state;

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
		tgt->transient_state.copy_count = 0;

		if (len < tgt->block_size) {
		gimmeabreak();
 
		/* avoid leaks */
#if 0
you`ll have to special case this 
#endif
			tgt->transient_state.out_count = tgt->block_size;
		}
	} else {
		tgt->transient_state.out_count = 0;
		tgt->transient_state.copy_count = 0;
	}

	tgt->transient_state.cmd_count = cmd_count;

	disconn =
	  BGET(scsi_might_disconnect,(unsigned)tgt->masterno, tgt->target_id);
	disconn  = disconn && (spc->ntargets > 1);
	disconn |=
	  BGET(scsi_should_disconnect,(unsigned)tgt->masterno, tgt->target_id);

	/*
	 * Setup target state
	 */
	tgt->done = SCSI_RET_IN_PROGRESS;

	handler = (disconn) ? spc_err_disconn : spc_err_generic;

        /* determine wether or not to use the late forms of the scripts */
        late = cmd_only ? FALSE : (tgt->flags & TGT_DID_SYNCH);

	switch (tgt->cur_cmd) {
	    case SCSI_CMD_READ:
	    case SCSI_CMD_LONG_READ:
		LOG(0x13,"readop");
		scp = late ? spc_script_late_data_in : spc_script_data_in;
		break;
	    case SCSI_CMD_WRITE:
	    case SCSI_CMD_LONG_WRITE:
		LOG(0x14,"writeop");
		scp = late ? spc_script_late_data_out : spc_script_data_out;
		break;  
	    case SCSI_CMD_INQUIRY:
		/* This is likely the first thing out:
		   do the synch neg if so */
		if (!cmd_only && ((tgt->flags&TGT_DID_SYNCH)==0)) {
			scp = spc_script_try_synch;
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
	    case 0xc6:	/* despised: SCSI_CMD_TOSHIBA_READ_SUBCH_Q */
	    case 0xc7:	/* despised: SCSI_CMD_TOSHIBA_READ_TOC_ENTRY */
	    case 0xdd:	/* despised: SCSI_CMD_NEC_READ_SUBCH_Q */
	    case 0xde:	/* despised: SCSI_CMD_NEC_READ_TOC */
		scp = late ? spc_script_late_data_in : spc_script_data_in;
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
		scp = late ? spc_script_late_data_out : spc_script_data_out;
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    case SCSI_CMD_TEST_UNIT_READY:
		/*
		 * Do the synch negotiation here, unless prohibited
		 * or done already
		 */
		if (tgt->flags & TGT_DID_SYNCH) {
			scp = late ? spc_script_late_cmd : spc_script_cmd;
		} else {
			scp = spc_script_try_synch;
			tgt->flags |= TGT_TRY_SYNCH;
			cmd_only = FALSE;
		}
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    default:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		scp = late ? spc_script_late_cmd : spc_script_cmd;
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
	 * this SCSI bus, e.g. lock the spc structure.
	 * Note that it is the strategy routine's job
	 * to serialize ops on the same target as appropriate.
	 */
#if 0
locking code here
#endif
	s = splbio();

	if (spc->wd.nactive++ == 0)
		spc->wd.watchdog_state = SCSI_WD_ACTIVE;

	if (spc->state & SPC_STATE_BUSY) {
		/*
		 * Queue up this target, note that this takes care
		 * of proper FIFO scheduling of the scsi-bus.
		 */
		LOG(3,"enqueue");
		enqueue_tail(&spc->waiting_targets, (queue_entry_t) tgt);
	} else {
		/*
		 * It is down to at most two contenders now,
		 * we will treat reconnections same as selections
		 * and let the scsi-bus arbitration process decide.
		 */
		spc->state |= SPC_STATE_BUSY;
		spc->next_target = tgt;
		spc_attempt_selection(spc);
		/*
		 * Note that we might still lose arbitration..
		 */
	}
	splx(s);
}

void spc_attempt_selection(spc)
	spc_softc_t	spc;
{
	target_info_t	*tgt;
	spc_regmap_t	*regs;
	register int	cmd;
	int atn=0;

	/* This is about your select code */

	regs = spc->regs;
	tgt = spc->next_target;

	LOG(4,"select");
	LOG(0x80+tgt->target_id,0);

	/*
	 * Init bus state variables and set registers.
	 */
	spc->active_target = tgt;

	/* reselection pending ? */
	if ((regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL)) 
#ifdef MIPS
	    && (regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL)) 
	    && (regs->spc_phase & (SPC_BUS_BSY|SPC_BUS_SEL))
#endif
	    )
	    return;

	spc->script = tgt->transient_state.script;
	spc->error_handler = tgt->transient_state.handler;
	spc->done = SCSI_RET_IN_PROGRESS;

	spc->in_count = 0;
	spc->out_count = 0;

	cmd =	SPC_SCMD_C_SELECT | SPC_SCMD_PROGRAMMED_X;
	if (tgt->flags & TGT_DID_SYNCH) 
	  {
	    if (tgt->transient_state.identify != 0xff)
	      atn = 1;
	  } 
	else 
	  if (tgt->flags & TGT_TRY_SYNCH)
	    atn = 1;

#if  0
	regs->spc_psns = SPC_DIAG_ENBL_XFER;         
#endif
	
	regs->spc_sctl = SPC_SCTL_ARB_EBL  | SPC_SCTL_PAR_EBL |
			 SPC_SCTL_RSEL_EBL | SPC_SCTL_IE;                

  
	{ int mask;
	mask = ~SPC_PCTL_BFREE_IE & regs->spc_pctl;  
	regs->spc_pctl = mask;                        
	}

	regs->spc_temp = (1<<(spc->sc->initiator_id)) | (1<<(tgt->target_id));

	SPC_TC_PUT(regs,0xfa004);                     

	if (atn)
	  {
	    regs->spc_scmd = SPC_SCMD_C_ATN_S;
	    /* delay 1us to avoid races */
	    delay(1);
	  }

	regs->spc_scmd = cmd;
	return;
}

/*
 * Interrupt routine
 *	Take interrupts from the chip
 *
 * Implementation:
 *	Move along the current command's script if
 *	all is well, invoke error handler if not.
 */
void spc_intr(unit)
int unit;
{
	register spc_softc_t	spc;
	register script_t	scp;
	register unsigned	ints, psns, ssts; 
	register spc_regmap_t	*regs;
	boolean_t		try_match;
#if	notyet
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface)
	{
		SPC_intr(unit);
		return;
	}
#endif

	spc = spc_softc[unit];
	regs = spc->regs;

	/* read the interrupt status register */
	ints = regs->spc_ints;

	LOG(5,"\n\tintr");
	LOG(0x80+ints,0);

TR(ints);
TRCHECK;

	if (ints & SPC_INTS_RESET) 
	  {
	    /* does its own interrupt reset when ready */
	    spc_bus_reset(spc); 
	    return;
	  }

	/* we got an interrupt allright */
	if (spc->active_target)
		spc->wd.watchdog_state = SCSI_WD_ACTIVE;

	
	if (ints == 0) 
	  { /* no obvious cause */
	    LOG(2,"SPURIOUS");
	    gimmeabreak();
	    return;
	  }


	/* reset the interrupt */
	regs->spc_ints = ints;

	/* go get the phase, and status. We can't trust the
	   phase until REQ is asserted in the psns. Only do 
	   this is we received a command complete or service
	   required interrupt. Otherwise, just read them once 
	   and trust. */


	
	if (ints & (SPC_INTS_DONE|SPC_INTS_BUSREQ))
	  while(1)
	    {
	      psns = regs->spc_psns;
	      if (psns & SPC_BUS_REQ)
		break;
	      delay(1); /* don't hog the bus */
	    }
	else
	  psns = regs->spc_psns;

	ssts = regs->spc_psns;

TR(psns);
TR(ssts);
TRCHECK;

	if ((spc->state & SPC_STATE_TARGET) ||
	    (ints & SPC_INTS_SELECTED))
	  spc_target_intr(spc /**, ints, psns, ssts **/);
	
	scp = spc->script;

	if ((scp == 0) || (ints & SPC_INTS_RESELECTED))
	  {
	    gimmeabreak();
	    spc_reconnect(spc, ints, psns, ssts);
	    return;
	  }

	if (SCRIPT_MATCH(psns) != scp->condition) {
		if (try_match = (*spc->error_handler)(spc, ints, psns, ssts)) {
		  psns = regs->spc_psns;
		  ssts = regs->spc_ssts;
		}
	} else
		try_match = TRUE;
	

	/* might have been side effected */
	scp = spc->script;

	if (try_match && (SCRIPT_MATCH(psns) == scp->condition)) {
		/*
		 * Perform the appropriate operation,
		 * then proceed
		 */
		if ((*scp->action)(spc, ints, psns, ssts)) {
			/* might have been side effected */
			scp = spc->script;
			spc->script = scp + 1;
		}
	}
}

void spc_target_intr(spc)
	register spc_softc_t	spc;
{
	panic("SPC: TARGET MODE !!!\n");
}

/*
 * All the many little things that the interrupt
 * routine might switch to
 */
boolean_t
spc_issue_command(spc, ints, psns, ssts)
     spc_softc_t	spc;
     int                ints, psns, ssts;
{
	register spc_regmap_t	*regs = spc->regs;

	LOG(0x12, "cmd_issue");
	/* we have just done a select; 
	   Bus is in CMD phase; 
	   need to phase match */
	SPC_ACK(regs, SCSI_PHASE_CMD);
  
	return spc_data_out(regs, SCSI_PHASE_CMD, 
		     spc->active_target->transient_state.cmd_count,
		     spc->active_target->cmd_ptr) ? FALSE : TRUE;
}

boolean_t
spc_issue_ident_and_command(spc, ints, psns, ssts)
     spc_softc_t	spc;
     int                ints, psns, ssts;
{
	register spc_regmap_t	*regs = spc->regs;

	LOG(0x22, "ident_and_cmd");
	/* we have just done a select with atn Bus is in MSG_OUT phase; 
	   need to phase match */
	SPC_ACK(regs, SCSI_PHASE_MSG_OUT);
  
	spc_data_out(regs, SCSI_PHASE_MSG_OUT, 1, 
		     &spc->active_target->transient_state.identify);

	/* wait to go to command phase */
	SPC_WAIT_PHASE(SCSI_PHASE_CMD);

	/* ack */
	SPC_ACK(regs, SCSI_PHASE_CMD);

	/* should be a command complete intr pending. Eat it */
	if (regs->spc_ints != SPC_INTS_DONE)
	  gimmeabreak();
	regs->spc_ints = SPC_INTS_DONE;

	/* spit */
	return spc_data_out(regs, SCSI_PHASE_CMD, 
		     spc->active_target->transient_state.cmd_count,
		     spc->active_target->cmd_ptr) ? FALSE : TRUE;
}


boolean_t
spc_end_transaction( spc, ints, psns, serr)
	register spc_softc_t	spc;
	int ints, psns, serr;
{
	register spc_regmap_t	*regs = spc->regs;
	char			cmc;
	int			tmp;

	LOG(0x1f,"end_t");

	SPC_ACK(regs,SCSI_PHASE_MSG_IN /*,1*/);

	spc_data_in(regs, SCSI_PHASE_MSG_IN, 1, &cmc);

	if (cmc != SCSI_COMMAND_COMPLETE)
		printf("{T%x}", cmc);

	while (regs->spc_ints != (SPC_INTS_DONE|SPC_INTS_DISC));

	SPC_ACK(regs,SPC_PHASE_DISC);

  	/* going to disconnect */
	regs->spc_pctl = ~SPC_PCTL_BFREE_IE & SPC_PHASE_DISC;
	/* regs->spc_scmd = 0; */

	/*  clear all intr bits? */
	tmp = regs->spc_ints;
	regs->spc_ints = tmp;


	if (!spc_end(spc, ints, psns, serr))
		(void) spc_reconnect(spc, ints, psns, serr);
	return FALSE;
}

boolean_t
spc_end( spc, ints, psns, serr)
	register spc_softc_t	spc;
	int ints, psns, serr;
{
	register target_info_t	*tgt;
	register io_req_t	ior;
	register spc_regmap_t	*regs = spc->regs;
	int csr;

	LOG(6,"end");

	tgt = spc->active_target;

	if ((tgt->done = spc->done) == SCSI_RET_IN_PROGRESS)
		tgt->done = SCSI_RET_SUCCESS;

	spc->script = 0;

	if (spc->wd.nactive-- == 1)
		spc->wd.watchdog_state = SCSI_WD_INACTIVE;

	/* check reconnection not pending */
	csr = SPC_INTS_RESELECTED & regs->spc_ints;
	if (!csr)
	  spc_release_bus(spc);
	else 
	  {
	    spc->active_target = 0;
	    /*		spc->state &= ~SPC_STATE_BUSY; later */
	  }
	if (ior = tgt->ior) {
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
		fdma_unmap(&tgt->fdma, ior);
#endif	/*MACH_KERNEL*/
		LOG(0xA,"ops->restart");
		(*tgt->dev_ops->restart)( tgt, TRUE);
		if (csr)
		  spc->state &= ~SPC_STATE_BUSY;
	}

	/* return not reselected */
	return  (csr & SPC_INTS_RESELECTED) ? 0 : 1;
}

boolean_t
spc_release_bus(spc)
	register spc_softc_t	spc;
{
	boolean_t	ret = FALSE;

	LOG(9,"release");

	spc->script = 0;

	if (spc->state & SPC_STATE_COLLISION) {

		LOG(0xB,"collided");
		spc->state &= ~SPC_STATE_COLLISION;
		spc_attempt_selection(spc);

	} else if (queue_empty(&spc->waiting_targets)) {

		spc->state &= ~SPC_STATE_BUSY;
		spc->active_target = 0;
		ret = TRUE;

	} else {

		LOG(0xC,"dequeue");
		spc->next_target = (target_info_t *)
				dequeue_head(&spc->waiting_targets);
		spc_attempt_selection(spc);
	}
	return ret;
}

boolean_t
spc_get_status( spc, ints, psns, serr)
	register spc_softc_t	spc;
	int ints, psns, serr;
{
	register spc_regmap_t	*regs = spc->regs;
	scsi2_status_byte_t	status;
	register target_info_t	*tgt;

	LOG(0xD,"get_status");
TRWRAP;

	spc->state &= ~SPC_STATE_DMA_IN;

	tgt = spc->active_target;

	SPC_ACK(regs,SCSI_PHASE_STATUS /*,1*/);

	spc_data_in(regs, SCSI_PHASE_STATUS, 1, &status.bits);

	if (status.st.scsi_status_code != SCSI_ST_GOOD) {
		scsi_error(spc->active_target, SCSI_ERR_STATUS, status.bits, 0);
		spc->done = (status.st.scsi_status_code == SCSI_ST_BUSY) ?
			SCSI_RET_RETRY : SCSI_RET_NEED_SENSE;
	} else
		spc->done = SCSI_RET_SUCCESS;

	return TRUE;
}

boolean_t
spc_xfer_in( spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register target_info_t	*tgt;
	register spc_regmap_t	*regs = spc->regs;
	register int		count;
	boolean_t		advance_script = TRUE;

	LOG(0xE,"xfer_in");

	tgt = spc->active_target;
	spc->state |= SPC_STATE_DMA_IN;

	count = tgt->transient_state.in_count;

	SPC_ACK(regs, SCSI_PHASE_DATAI);

	if ((tgt->cur_cmd != SCSI_CMD_READ) &&
	    (tgt->cur_cmd != SCSI_CMD_LONG_READ))
	  spc_data_in(regs, SCSI_PHASE_DATAI, count, tgt->cmd_ptr);
	else
	  {
	    spc_data_in(regs, SCSI_PHASE_DATAI, count, tgt->ior->io_data);
          }

	return advance_script;
}

boolean_t
spc_xfer_out( spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register spc_regmap_t	*regs = spc->regs;
	register target_info_t	*tgt;
	boolean_t		advance_script = TRUE;
	int			count = spc->out_count;

	LOG(0xF,"xfer_out");

	tgt = spc->active_target;
	spc->state &= ~SPC_STATE_DMA_IN;

	count = tgt->transient_state.out_count;

	SPC_ACK(regs, SCSI_PHASE_DATAO);

	if ((tgt->cur_cmd != SCSI_CMD_WRITE) &&
	    (tgt->cur_cmd != SCSI_CMD_LONG_WRITE))
	  spc_data_out(regs, SCSI_PHASE_DATAO, count, 
		       tgt->cmd_ptr + tgt->transient_state.cmd_count);
	else
	  spc_data_out(regs, SCSI_PHASE_DATAO, count, tgt->ior->io_data);

	return advance_script;
}

/* disconnect-reconnect ops */

/* get the message in via dma ?? */
boolean_t
spc_msg_in(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register target_info_t	*tgt;

	LOG(0x15,"msg_in");
	gimmeabreak();

	tgt = spc->active_target;

#if 0
You can do this by hand, just leave an interrupt pending at the end
#endif

	/* We only really expect two bytes */
#if 0
	SPC_PUT(dmar,sizeof(scsi_command_group_0));
	....
#endif
	return TRUE;
}

/* check the message is indeed a DISCONNECT */
boolean_t
spc_disconnect(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register int		len = 0;
	boolean_t		ok = FALSE;
	register char		*msgs = 0;


/*	SPC_TC_GET(dmar,len); */
	len = sizeof(scsi_command_group_0) - len;

/*	msgs = tgt->cmd_ptr; */ /* I think */

	if ((len == 0) || (len > 2) || msgs == 0)
		ok = FALSE;
	else {
		/* A SDP message preceeds it in non-completed READs */
		ok = ((msgs[0] == SCSI_DISCONNECT) ||	/* completed op */
		      ((msgs[0] == SCSI_SAVE_DATA_POINTER) && /* incomplete */
		       (msgs[1] == SCSI_DISCONNECT)));
	}
	if (!ok)
		printf("[tgt %d bad msg (%d): %x]",
			spc->active_target->target_id, len, *msgs);

	return TRUE;
}

/* save all relevant data, free the BUS */
boolean_t
spc_disconnected(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register target_info_t	*tgt;

/*	make sure reselects will work */

	LOG(0x16,"disconnected");

	spc_disconnect(spc,ints, psns, ssts);

	tgt = spc->active_target;
	tgt->flags |= TGT_DISCONNECTED;
	tgt->transient_state.handler = spc->error_handler;
	/* the rest has been saved in spc_err_disconn() */

	PRINT(("{D%d}", tgt->target_id));

	spc_release_bus(spc);

	return FALSE;
}

/* get reconnect message, restore BUS */
boolean_t
spc_reconnect(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{

	LOG(0x17,"reconnect");

	if (spc->wd.nactive == 0) {
		LOG(2,"SPURIOUS");
		return FALSE;
	}

#if 0
This is the 5380 code, for reference:
	spc_regmap_t *regs = spc->regs;
	register target_info_t	*tgt;
	register int		id;
	int			msg;


	id = regs->spc_data;/*parity?*/
	/* xxx check our id is in there */

	id &= ~(1 << spc->sc->initiator_id);
	{
		register int i;
		for (i = 0; i < 8; i++)
			if (id & (1 << i)) break;
if (i == 8) {printf("{P%x}", id);return;}
		id = i;
	}
	regs->spc_icmd = SPC_ICMD_BSY;
	while (regs->spc_bus_csr & SPC_BUS_SEL)
		;
	regs->spc_icmd = 0;
	delay_1p2_us();
	while ( ((regs->spc_bus_csr & SPC_BUS_BSY) == 0) &&
		((regs->spc_bus_csr & SPC_BUS_BSY) == 0) &&
		((regs->spc_bus_csr & SPC_BUS_BSY) == 0))
		;

	/* Now should wait for correct phase: REQ signals it */
	while (	((regs->spc_bus_csr & SPC_BUS_REQ) == 0) &&
		((regs->spc_bus_csr & SPC_BUS_REQ) == 0) &&
		((regs->spc_bus_csr & SPC_BUS_REQ) == 0))
		;

	regs->spc_mode |= SPC_MODE_MONBSY;

	/*
	 * See if this reconnection collided with a selection attempt
	 */
	if (spc->state & SPC_STATE_BUSY)
		spc->state |= SPC_STATE_COLLISION;

	spc->state |= SPC_STATE_BUSY;

	/* Get identify msg */
	bs = regs->spc_phase;
if (SPC_CUR_PHASE(bs) != SCSI_PHASE_MSG_IN) gimmeabreak();
	SPC_ACK(regs,SCSI_PHASE_MSG_IN /*,1*/);
	msg = 0;
	spc_data_in(regs, SCSI_PHASE_MSG_IN, 1, &msg);
	regs->spc_mode = SPC_MODE_PAR_CHK|SPC_MODE_DMA|SPC_MODE_MONBSY;

	if (msg != SCSI_IDENTIFY)
		printf("{I%x %x}", id, msg);

	tgt = spc->sc->target[id];
	if (id > 7 || tgt == 0) panic("spc_reconnect");

	PRINT(("{R%d}", id));
	if (spc->state & SPC_STATE_COLLISION)
		PRINT(("[B %d-%d]", spc->active_target->target_id, id));

	LOG(0x80+id,0);

	spc->active_target = tgt;
	tgt->flags &= ~TGT_DISCONNECTED;

	spc->script = tgt->transient_state.script;
	spc->error_handler = tgt->transient_state.handler;
	spc->in_count = 0;
	spc->out_count = 0;

	/* Should get a phase mismatch when tgt changes phase */
#endif
	return TRUE;
}



/* do the synch negotiation */
boolean_t
spc_dosynch( spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	/*
	 * Phase is MSG_OUT here, cmd has not been xferred
	 */
	int			len;
	register target_info_t	*tgt;
	register spc_regmap_t	*regs = spc->regs;
	unsigned char		off;
	unsigned char		p[6];

	LOG(0x11,"dosync");

	/* ATN still asserted */
	SPC_ACK(regs,SCSI_PHASE_MSG_OUT);
	
	tgt = spc->active_target;

	tgt->flags |= TGT_DID_SYNCH;	/* only one chance */
	tgt->flags &= ~TGT_TRY_SYNCH;

	/*p = some scratch buffer, on the stack  */

	p[0] = SCSI_IDENTIFY;
	p[1] = SCSI_EXTENDED_MESSAGE;
	p[2] = 3;
	p[3] = SCSI_SYNC_XFER_REQUEST;
	/* We cannot run synchronous */
#define spc_to_scsi_period(x)	0x7
#define scsi_period_to_spc(x)	(x)
	off = 0;
	p[4] = spc_to_scsi_period(spc_min_period);
	p[5] = off;

	/* The transfer is started with ATN still set. The 
	   chip will automagically drop ATN before it transfers the 
	   last byte. Pretty neat. */
	spc_data_out(regs, SCSI_PHASE_MSG_OUT,
			sizeof(scsi_synch_xfer_req_t)+1, p);

	/* wait for phase change to status phase */
	SPC_WAIT_PHASE_VANISH(SCSI_PHASE_MSG_OUT);


	psns = regs->spc_phase;

	/* The standard sez there nothing else the target can do but.. */
	if (SPC_CUR_PHASE(psns) != SCSI_PHASE_MSG_IN)
		panic("spc_dosync");/* XXX put offline */

  /*
    msgin:
   */
	/* ack */
	SPC_ACK(regs,SCSI_PHASE_MSG_IN);

	/* clear any pending interrupts */
	regs->spc_ints = regs->spc_ints;

	/* get answer */
	len = sizeof(scsi_synch_xfer_req_t);
	len = spc_data_in(regs, SCSI_PHASE_MSG_IN, len, p);

	/* do not cancel the phase mismatch interrupt ! */

	/* look at the answer and see if we like it */
	if (len || (p[0] != SCSI_EXTENDED_MESSAGE)) {
		/* did not like it at all */
		printf(" did not like SYNCH xfer ");
	} else {
		/* will NOT do synch */
		printf(" but we cannot do SYNCH xfer ");
		tgt->sync_period = scsi_period_to_spc(p[3]);
		tgt->sync_offset = p[4];
		/* sanity */
		if (tgt->sync_offset != 0)
			printf(" ?OFFSET %x? ", tgt->sync_offset);
	}

	/* wait for phase change */
	SPC_WAIT_PHASE_VANISH(SCSI_PHASE_MSG_IN);

	psns = regs->spc_phase;

	/* phase should be command now */
	/* continue with simple command script */
	spc->error_handler = spc_err_generic;
	spc->script = spc_script_cmd;

/* Make sure you get out right here, esp the script pointer and/or pending intr */

	if (SPC_CUR_PHASE(psns) == SCSI_PHASE_CMD )
		return FALSE;

	if (SPC_CUR_PHASE(psns) == SCSI_PHASE_STATUS )  /* jump to get_status */
		return TRUE;	/* intr is pending */ 

	spc->script++;
	if (SPC_CUR_PHASE(psns) == SCSI_PHASE_MSG_IN )
		return TRUE;

	if ((psns & SPC_BUS_BSY) == 0)	/* uhu? disconnected */
	  return TRUE; 
	    
	gimmeabreak();
	return FALSE;
}

/*
 * The bus was reset
 */
void spc_bus_reset(spc)
	register spc_softc_t	spc;
{
	register spc_regmap_t	*regs = spc->regs;

	LOG(0x21,"bus_reset");

	/*
	 * Clear bus descriptor
	 */
	spc->script = 0;
	spc->error_handler = 0;
	spc->active_target = 0;
	spc->next_target = 0;
	spc->state = 0;
	queue_init(&spc->waiting_targets);
	spc->wd.nactive = 0;
	spc_reset(regs, TRUE);

	printf("spc%d: (%d) bus reset ", spc->sc->masterno, ++spc->wd.reset_count);
	delay(scsi_delay_after_reset); /* some targets take long to reset */

	if (spc->sc == 0)	/* sanity */
		return;

	scsi_bus_was_reset(spc->sc);
}

/*
 * Error handlers
 */

/*
 * Generic, default handler
 */
boolean_t
spc_err_generic(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	register spc_regmap_t  *regs = spc->regs; 
	LOG(0x10,"err_generic");

	if (ints & SPC_INTS_TIMEOUT) /* we timed out */
	  if ((regs->spc_scmd & SPC_SCMD_CMDMASK) == SPC_SCMD_C_SELECT)
	    {
		/* Powered off ? */
		if (spc->active_target->flags & TGT_FULLY_PROBED)
		  {
		    spc->active_target->flags = 0;
		    LOG(0x1e,"Device Down");
		  }
		spc->done = SCSI_RET_DEVICE_DOWN;
		spc_end(spc, ints, psns, ssts);
		return FALSE; /* don't retry - just report missing device */
	    }
	  else
	    { /* timed out - but not on a select. What is going on? */
	      gimmeabreak();
	    }

	if (SPC_CUR_PHASE(psns) == SCSI_PHASE_STATUS)
		return spc_err_to_status(spc, ints, psns, ssts);
	gimmeabreak();
	return FALSE;
}

/*
 * Handle generic errors that are reported as
 * an unexpected change to STATUS phase
 */
boolean_t
spc_err_to_status(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
	script_t		scp = spc->script;

	LOG(0x20,"err_tostatus");
	while (SCSI_PHASE(scp->condition) != SCSI_PHASE_STATUS)
		scp++;
	spc->script = scp;
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
	spc->done = SCSI_RET_NEED_SENSE;
#endif
	return TRUE;
}

/*
 * Watch for a disconnection
 */
boolean_t
spc_err_disconn(spc, ints, psns, ssts)
	register spc_softc_t	spc;
	int ints, psns, ssts;
{
#if 1
/*
 * THIS ROUTINE CAN'T POSSIBLY WORK...
 * FOR EXAMPLE, THE VARIABLE 'xferred' IS NEVER INITIALIZED.
 */
	return FALSE;
#else
	register spc_regmap_t	*regs;
	register target_info_t	*tgt;
	int			xferred;

	LOG(0x18,"err_disconn");

	if (SPC_CUR_PHASE(ints) != SCSI_PHASE_MSG_IN)
		return spc_err_generic(spc, ints, psns, ssts);

	regs = spc->regs;

	tgt = spc->active_target;

	switch (SCSI_PHASE(spc->script->condition)) {
	case SCSI_PHASE_DATAO:
		LOG(0x1b,"+DATAO");
/*updatecounters:*/
			tgt->transient_state.out_count -= xferred;
			assert(tgt->transient_state.out_count > 0);
			tgt->transient_state.dma_offset += xferred;

		tgt->transient_state.script = spc_script_data_out;
		break;

	case SCSI_PHASE_DATAI:
		LOG(0x19,"+DATAI");

/*update counters: */
			assert(xferred > 0);
			tgt->transient_state.in_count -= xferred;
			assert(tgt->transient_state.in_count > 0);
			tgt->transient_state.dma_offset += xferred;

		tgt->transient_state.script = spc_script_data_in;
		break;

	case SCSI_PHASE_STATUS:

		if (spc->state & SPC_STATE_DMA_IN) {

			LOG(0x1a,"+STATUS+R");

/*same as above.. */
			assert(xferred > 0);
			tgt->transient_state.in_count -= xferred;
/*			assert(tgt->transient_state.in_count > 0);*/
			tgt->transient_state.dma_offset += xferred;

			tgt->transient_state.script = spc_script_data_in;
			if (tgt->transient_state.in_count == 0)
				tgt->transient_state.script++;

		} else {

			LOG(0x1d,"+STATUS+W");

			if ((tgt->transient_state.out_count == spc->out_count)) {
				/* all done */
				tgt->transient_state.script = &spc_script_data_out[1];
				tgt->transient_state.out_count = 0;
			} else {

/*.. */
				tgt->transient_state.out_count -= xferred;
				assert(tgt->transient_state.out_count > 0);
				tgt->transient_state.dma_offset += xferred;

				tgt->transient_state.script = spc_script_data_out;
			}
			spc->out_count = 0;
		}
		break;
	default:
		gimmeabreak();
	}
	/* spc->xxx = 0; */

/*	SPC_ACK(regs,SCSI_PHASE_MSG_IN); later */
	(void) spc_msg_in(spc, ints, psns, ssts);

	spc->script = spc_script_disconnect;

	return FALSE;
#endif
}

/*
 * Watchdog
 *
 */
void spc_reset_scsibus(spc)
        register spc_softc_t    spc;
{
        register target_info_t  *tgt = spc->active_target;
        if (tgt) {
		int cnt = 0;
		/* SPC_TC_GET(spc->dmar,cnt); */
                log(	LOG_KERN,
			"Target %d was active, cmd x%x in x%x out x%x Sin x%x Sou x%x dmalen x%x\n",
                        tgt->target_id, tgt->cur_cmd,
                        tgt->transient_state.in_count, tgt->transient_state.out_count,
                        spc->in_count, spc->out_count, cnt);
	}
#if 0
	spc->regs->..... 
#endif
        delay(25);
}

int SPC_ACK(regs, phase)
register spc_regmap_t *regs;
unsigned phase;
{
  /* we want to switch into the specified phase -

     The calling routine should already dismissed
     any pending interrupts (spc_ints) 
  */

  regs->spc_psns = 0;
  regs->spc_pctl = phase | SPC_PCTL_BFREE_IE;
  return 0;
}
#endif	/*NSCSI > 0*/

#endif 0
