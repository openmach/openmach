/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
 * Copyright (c) 1993 University of Dublin
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and the following permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND THE UNIVERSITY OF DUBLIN ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND THE
 * UNIVERSITY OF DUBLIN DISCLAIM ANY LIABILITY OF ANY KIND FOR
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
 * Support for AHA-174x in enhanced mode. Dominic Herity (dherity@cs.tcd.ie) 
 * Will refer to "Adaptec AHA-1740A/1742A/1744 Technical Reference Manual"
 * page x-y as TRMx-y in comments below.
 */

#include <eaha.h>
#if	NEAHA > 0

#define db_printf printf

#include <cpus.h>
#include <platforms.h>
#include <aha.h>

#ifdef OSF
#include <eisa.h>
#else
#include <i386at/eisa.h>
#endif

#include <mach/std_types.h>
#include <sys/types.h>
#include <chips/busses.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#include <scsi/adapters/scsi_aha15.h>
#include <vm/vm_kern.h>

#ifdef	AT386
#define	MACHINE_PGBYTES		I386_PGBYTES
#define	MAPPABLE			0
#define	gimmeabreak()		asm("int3")


#include <i386/pio.h>		/* inlining of outb and inb */
#ifdef OSF
#include <machine/mp/mp.h>
#endif
#endif	/*AT386*/

#ifdef	CBUS			
#include <cbus/cbus.h>
#endif


#ifndef	MACHINE_PGBYTES		/* cross compile check */
#define	MACHINE_PGBYTES		0x1000
#define	MAPPABLE			1
#define	gimmeabreak()		Debugger("gimmeabreak");
#endif

int	eaha_probe(), scsi_slave(), eaha_go(), eaha_intr();
void	scsi_attach();

vm_offset_t	eaha_std[NEAHA] = { 0 };
struct	bus_device *eaha_dinfo[NEAHA*8];
struct	bus_ctlr *eaha_minfo[NEAHA];
struct	bus_driver eaha_driver = 
        { eaha_probe, scsi_slave, scsi_attach, eaha_go, eaha_std, "rz",
	  eaha_dinfo, "eahac", eaha_minfo, BUS_INTR_B4_PROBE};


#define TRACE
#ifdef TRACE

#define LOGSIZE 256
int eaha_logpt;
char eaha_log[LOGSIZE];

#define MAXLOG_VALUE	0x1e
struct {
	char *name;
	unsigned int count;
} logtbl[MAXLOG_VALUE];

static LOG(
	int e,
	char *f)
{
	eaha_log[eaha_logpt++] = (e);
	if (eaha_logpt == LOGSIZE) eaha_logpt = 0;
	if ((e) < MAXLOG_VALUE) {
		logtbl[(e)].name = (f);
		logtbl[(e)].count++;
	}
}

eaha_print_log(
	int skip)
{
	register int i, j;
	register unsigned char c;

	for (i = 0, j = eaha_logpt; i < LOGSIZE; i++) {
		c = eaha_log[j];
		if (++j == LOGSIZE) j = 0;
		if (skip-- > 0)
			continue;
		if (c < MAXLOG_VALUE)
			db_printf(" %s", logtbl[c].name);
		else
			db_printf("-%x", c & 0x7f);
	}
	return 0;
}

eaha_print_stat()
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

#ifdef DEBUG
#define ASSERT(x) { if (!(x)) gimmeabreak() ; }
#define MARK() gimmeabreak()
#else
#define ASSERT(x)
#define MARK()
#endif

/*
 *	Notes :
 *
 * do each host command TRM6-4
 * find targets in probe
 * disable SCSI writes
 * matching port with structs, eaha_go with port, eaha_intr with port
 *
 */

/* eaha registers. See TRM4-11..23. dph */

#define HID0(z)		((z)+0xC80)
#define HID1(z)		((z)+0xC81)
#define HID2(z)		((z)+0xC82)
#define HID3(z)		((z)+0xC83)
#define EBCTRL(z)	((z)+0xC84)
#define PORTADDR(z)	((z)+0xCC0)
#define BIOSADDR(z)	((z)+0xCC1)
#define INTDEF(z)	((z)+0xCC2)
#define SCSIDEF(z)	((z)+0xCC3)
#define MBOXOUT0(z)	((z)+0xCD0)
#define MBOXOUT1(z)	((z)+0xCD1)
#define MBOXOUT2(z)	((z)+0xCD2)
#define MBOXOUT3(z)	((z)+0xCD3)
#define MBOXIN0(z)	((z)+0xCD8)
#define MBOXIN1(z)	((z)+0xCD9)
#define MBOXIN2(z)	((z)+0xCDA)
#define MBOXIN3(z)	((z)+0xCDB)
#define ATTN(z)		((z)+0xCD4)
#define G2CNTRL(z)	((z)+0xCD5)
#define G2INTST(z)	((z)+0xCD6)
#define G2STAT(z)	((z)+0xCD7)
#define G2STAT2(z)	((z)+0xCDC)

/*
 * Enhanced mode data structures: ring, enhanced ccbs, a per target buffer
 */

#define SCSI_TARGETS 8	/* Allow for SCSI-2 */


/* Extended Command Control Block Format. See TRM6-3..12. */

typedef struct {
	unsigned short command ;
#		define EAHA_CMD_NOP 0
#		define EAHA_CMD_INIT_CMD 1
#		define EAHA_CMD_DIAG 5
#		define EAHA_CMD_INIT_SCSI 6
#		define EAHA_CMD_READ_SENS 8
#		define EAHA_CMD_DOWNLOAD 9
#		define EAHA_CMD_HOST_INQ 0x0a
#		define EAHA_CMD_TARG_CMD 0x10

	/*
	 * It appears to be customary to tackle the endian-ness of
	 * bit fields as follows, so I won't deviate. However, nothing in
	 * K&R implies that bit fields are implemented so that the fields
	 * of an unsigned char are allocated lsb first. Indeed, K&R _warns_
	 * _against_ using bit fields to describe storage allocation.
	 * This issue is separate from endian-ness. dph
	 * And this is exactly the reason macros are used.  If your compiler
	 * is weird just override the macros and we will all be happy. af
	 */
	BITFIELD_3(unsigned char,
		cne:1,
		xxx0:6,
		di:1) ;
	BITFIELD_7(unsigned char,
		xxx1:2,
		ses:1,
		xxx2:1,
		sg:1,
		xxx3:1,
		dsb:1,
		ars:1) ;
		
	BITFIELD_5(unsigned char,
		lun:3,
		tag:1,
		tt:2,
		nd:1,
		xxx4:1) ;
	BITFIELD_7(unsigned char,
		dat:1,
		dir:1,
		st:1,
		chk:1,
		xxx5:2,
		rec:1,
		nbr:1) ;

	unsigned short xxx6 ;

	vm_offset_t scather ; /* scatter/gather */
	unsigned scathlen ;
	vm_offset_t status ;
	vm_offset_t chain ;
	int xxx7 ;

	vm_offset_t sense_p ;
	unsigned char sense_len ;
	unsigned char cdb_len ;
	unsigned short checksum ;
	scsi_command_group_5 cdb ;
	unsigned char buffer[256] ; /* space for data returned. */

} eccb ;

#define NTARGETS (8)
#define NECCBS (NTARGETS+2) /* Targets + 2 to allow for temporaries. */
	/* Can be up to 64 (TRM6-2), but that entails lots of bss usage */

typedef struct {	/* Status Block Format. See TRM6-13..19. */
	BITFIELD_8(unsigned char,
		don:1,
		du:1,
		xxx0:1,
		qf:1,
		sc:1,
		dover:1,
		ch:1,
		inti:1) ;
	BITFIELD_8(unsigned char,
		asa:1, /* Error in TRM6-15..16 says both asa and sns */
		sns:1,	 /* bit 9. Bits 8 and 10 are not mentioned. */
		xxx1:1,
		ini:1,
		me:1,
		xxx2:1,
		eca:1,
		xxx3:1) ;

	unsigned char ha_status ;
#		define HA_STATUS_SUCCESS 0x00
#		define HA_STATUS_HOST_ABORTED 0x04
#		define HA_STATUS_ADP_ABORTED 0x05
#		define HA_STATUS_NO_FIRM 0x08
#		define HA_STATUS_NOT_TARGET 0x0a
#		define HA_STATUS_SEL_TIMEOUT 0x11
#		define HA_STATUS_OVRUN 0x12
#		define HA_STATUS_BUS_FREE 0x13
#		define HA_STATUS_PHASE_ERROR 0x14
#		define HA_STATUS_BAD_OPCODE 0x16
#		define HA_STATUS_INVALID_LINK 0x17
#		define HA_STATUS_BAD_CBLOCK 0x18
#		define HA_STATUS_DUP_CBLOCK 0x19
#		define HA_STATUS_BAD_SCATHER 0x1a
#		define HA_STATUS_RSENSE_FAIL 0x1b
#		define HA_STATUS_TAG_REJECT 0x1c
#		define HA_STATUS_HARD_ERROR 0x20
#		define HA_STATUS_TARGET_NOATTN 0x21
#		define HA_STATUS_HOST_RESET 0x22
#		define HA_STATUS_OTHER_RESET 0x23
#		define HA_STATUS_PROG_BAD_SUM 0x80

	scsi2_status_byte_t	target_status ;

	unsigned residue ;
	vm_offset_t residue_buffer ;
	unsigned short add_stat_len ;
	unsigned char sense_len ;
	char xxx4[9] ;
	unsigned char cdb[6] ;

} status_block ;

typedef struct {
	vm_offset_t ptr ;
	unsigned len ;
} scather_entry ;

#define SCATHER_ENTRIES 128	/* TRM 6-11 */

struct erccbx {
	target_info_t	*active_target;
	eccb		_eccb;
	status_block	status ;
	struct erccbx	*next ;
} ;

typedef struct erccbx erccb ;

/* forward decls */
int	eaha_reset_scsibus();
boolean_t eaha_probe_target();

/*
 * State descriptor for this layer.  There is one such structure
 * per (enabled) board
 */
typedef struct {
	watchdog_t	wd;
	decl_simple_lock_data(, aha_lock)
	int		port;		/* I/O port */

	int		has_sense_info [NTARGETS];
	int		sense_info_lun [NTARGETS];
			/* 1742 enhanced mode will hang if target has
			 * sense info and host doesn't request it (TRM6-34).
			 * This sometimes happens in the scsi driver.
			 * These flags indicate when a target has sense
			 * info to disgorge.
			 * If set, eaha_go reads and discards sense info 
			 * before running any command except request sense.
			 * dph
			 */

	scsi_softc_t	*sc;		/* HBA-indep info */

	erccb		_erccbs[NECCBS] ;	/* mailboxes */
	erccb		*toperccb ;

	/* This chicanery is for mapping back the phys address
	   of a CCB (which we get in an MBI) to its virtual */
	/* [we could use phystokv(), but it isn't standard] */
	vm_offset_t	I_hold_my_phys_address;

	char		host_inquiry_data[256] ; /* Check out ../scsi2.h */

} eaha_softc ;

eaha_softc eaha_softc_data[NEAHA];

typedef eaha_softc *eaha_softc_t;

eaha_softc_t	eaha_softc_pool[NEAHA];

int eaha_quiet ;

erccb *erccb_alloc(
	eaha_softc *eaha)
{
	erccb *e ;
	int x ;

	do {
		while (eaha->toperccb == 0) ;/* Shouldn't be often or long, */
						/* BUT should use a semaphore */
		x = splbio() ;
		e = eaha->toperccb ;
		if (e == 0)
			splx(x) ;
	} while (!e) ;
	eaha->toperccb = e->next ;
	splx(x) ;
	bzero(e,sizeof(*e)) ;
	e->_eccb.status = kvtophys((vm_offset_t)&e->status) ;
	return e ;
}

void erccb_free(
	eaha_softc *eaha,
	erccb *e)
{
	int x ;
	ASSERT ( e >= eaha->_erccbs && e < eaha->_erccbs+NECCBS) ;
	x = splbio() ;
	e->next = eaha->toperccb ;
	eaha->toperccb = e ;
	splx(x) ;
}

void eaha_mboxout(
	int port,
	vm_offset_t phys)
{
        outb(MBOXOUT0(port),phys) ;
        outb(MBOXOUT1(port),phys>>8) ;
        outb(MBOXOUT2(port),phys>>16) ;
        outb(MBOXOUT3(port),phys>>24) ;
}

void eaha_command(	/* start a command */
	int port,
	erccb *_erccb)
{
	int s ;
	vm_offset_t phys = kvtophys((vm_offset_t) &_erccb->_eccb) ;
	while ((inb(G2STAT(port)) & 0x04)==0); /*While MBO busy. TRM6-1 */
	s = splbio() ;
	eaha_mboxout(port,phys) ;
	while (inb(G2STAT(port)) & 1) ; 	/* While adapter busy. TRM6-2 */
	outb(ATTN(port),0x40 | _erccb->active_target->target_id) ; /* TRM6-20 */
			/* (Should use target id for intitiator command) */
	splx(s) ;
}

eaha_reset(
	eaha_softc_t	eaha,
	boolean_t	quick)
{
	/*
	 * Reset board and wait till done
	 */
	unsigned st ;
	int target_id ;
	int port = eaha->port ;

	/* Reset adapter, maybe with SCSIbus */
	eaha_mboxout(port, quick ? 0x00080080 : 0x00000080 ) ; /* TRM 6-43..45 */
	outb(ATTN(port), 0x10 | inb(SCSIDEF(port)) & 0x0f) ;
	outb(G2CNTRL(port),0x20) ;	/* TRM 4-22 */

	do {
		st = inb(G2INTST(port)) >> 4 ;
	} while (st == 0) ;
	/* TRM 4-22 implies that 1 should not be returned in G2INTST, but
	   in practise, it is. So this code takes 0 to mean non-completion. */

	for (target_id = 0 ; target_id < NTARGETS; target_id++)
		eaha->has_sense_info[target_id] = FALSE ;

}

void eaha_init(
	eaha_softc_t	eaha)
{
	/* Do nothing - I guess */
}

void eaha_bus_reset(
	eaha_softc_t     eaha)

{
	LOG(0x1d,"bus_reset");

	/*
	 * Clear bus descriptor
	 */
	eaha->wd.nactive = 0;
	eaha_reset(eaha, TRUE);
	eaha_init(eaha);

	printf("eaha: (%d) bus reset ", ++eaha->wd.reset_count);
	delay(scsi_delay_after_reset); /* some targets take long to reset */

	if (eaha->sc == 0)	/* sanity */
		return;

	scsi_bus_was_reset(eaha->sc);
}

#ifdef notdef
	/* functions added to complete 1742 support, but not used. Untested. */

	void eaha_download(port, data, len)
	int port ;
	char *data ;
	unsigned len ;
	{
		/* 1744 firmware download. Not implemented. TRM6-21 */
	}

	void eaha_initscsi(data, len)
	char *data ;
	unsigned len ;
	{
		/* initialize SCSI subsystem. Presume BIOS does it.
		   Not implemented. TRM6-23 */
	}

	void eaha_noop()
	{
		/* Not implemented. TRM6-27 */
	}

	erccb *eaha_host_adapter_inquiry(eaha)	/* Returns a promise */
	eaha_softc *eaha ;			/* TRM6-31..33 */
	{
		erccb *_erccb = erccb_alloc(eaha) ;
		_erccb->_eccb.scather = (vm_offset_t) kvtophys(eaha->host_inquiry_data) ;
		_erccb->_eccb.scathlen = sizeof(eaha->host_inquiry_data) ;
		_erccb->_eccb.ses = 1 ;
		_erccb->_eccb.command = EAHA_CMD_HOST_INQ ;
		eaha_command(eaha->port,_erccb->_eccb,0) ; /* Is scsi_id used */
		return _erccb ;
	}

	erccb *eaha_read_sense_info(eaha, target, lun) /* TRM 6-33..35 */
	eaha_softc *eaha ;
	unsigned target, lun ;
	{	/* Don't think we need this because its done in scsi_alldevs.c */
	#ifdef notdef
		erccb *_erccb = erccb_alloc(eaha) ;
		_erccb->_eccb.command = EAHA_CMD_READ_SENS ;
		_erccb->_eccb.lun = lun ;
		eaha_command(eaha->port,_erccb->_eccb, target) ;/*Wrong # args*/
		return _erccb ;
	#else
		return 0 ;
	#endif
	}

	void eaha_diagnostic(eaha)
	eaha_softc *eaha ;
	{
		/* Not implemented. TRM6-36..37 */
	}

	erccb *eaha_target_cmd(eaha, target, lun, data, len) /* TRM6-38..39 */
	eaha_softc *eaha ;
	unsigned target, lun ;
	char *data ;
	unsigned len ;
	{
		erccb *_erccb = erccb_alloc(eaha) ;
		_erccb->_eccb.command = EAHA_CMD_TARG_CMD ;
		_erccb->_eccb.lun = lun ;
		eaha_command(eaha->port,_erccb->_eccb,target);/*Wrong # args*/
		return _erccb ;
	}

	erccb *eaha_init_cmd(port) /* SHOULD RETURN TOKEN. i.e. ptr to eccb */
				 /* Need list of free eccbs */
	{ /* to be continued,. possibly. */
	}

#endif /* notdef */

target_info_t *
eaha_tgt_alloc(
	eaha_softc_t	eaha,
	int		id,
	target_info_t	*tgt)
{
	erccb		*_erccb;

	if (tgt == 0)
		tgt = scsi_slave_alloc(eaha - eaha_softc_data, id, eaha);

	_erccb = erccb_alloc(eaha) ; /* This is very dodgy */
	tgt->cmd_ptr =  (char *)& _erccb->_eccb.cdb ;
	tgt->dma_ptr = 0;
	return tgt;
}


struct {
    scsi_sense_data_t sns ;
    unsigned char extra
	[254-sizeof(scsi_sense_data_t)] ;
} eaha_xsns [NTARGETS] ;/*must be bss to be contiguous*/


/* Enhanced adapter probe routine */

eaha_probe(
	register int	port,
	struct bus_ctlr	*ui)
{
	int unit = ui->unit;
	eaha_softc_t eaha = &eaha_softc_data[unit] ;
	int target_id ;
	scsi_softc_t *sc ;
	int		s;
	boolean_t did_banner = FALSE ;
        struct aha_devs installed;
	unsigned char my_scsi_id, my_interrupt ;

	if (unit >= NEAHA)
		return(0);

	/* No interrupts yet */
	s = splbio();

	/*
	 * Detect prescence of 174x in enhanced mode. Ignore HID2 and HID3
	 * on the assumption that compatibility will be preserved. dph
	 */
	if (inb(HID0(port)) != 0x04 || inb(HID1(port)) != 0x90 ||
	    (inb(PORTADDR(port)) & 0x80) != 0x80) {
	  	splx(s);
		return 0 ;
	}

	/* Issue RESET in case this is a reboot */

	outb(EBCTRL(port),0x04) ; /* Disable board. TRM4-12 */
	outb(PORTADDR(port),0x80) ; /* Disable standard mode ports. TRM4-13. */
	my_interrupt = inb(INTDEF(port)) & 0x07 ;
	outb(INTDEF(port), my_interrupt | 0x00) ;
					/* Disable interrupts. TRM4-15 */
	my_scsi_id = inb(SCSIDEF(port)) & 0x0f ;
	outb(SCSIDEF(port), my_scsi_id | 0x10) ;
				/* Force SCSI reset on hard reset. TRM4-16 */
	outb(G2CNTRL(port),0xe0) ; /* Reset board, clear interrupt */
				    /* and set 'host ready'. */
	delay(10*10) ;		/* HRST must remain set for 10us. TRM4-22 */
			/* (I don't believe the delay loop is slow enough.) */
	outb(G2CNTRL(port),0x60);/*Un-reset board, set 'host ready'. TRM4-22*/

	printf("Adaptec 1740A/1742A/1744 enhanced mode\n");
	
	/* Get host inquiry data */

	eaha_softc_pool[unit] = eaha ;
	bzero(eaha,sizeof(*eaha)) ;
	eaha->port = port ;

	sc = scsi_master_alloc(unit, eaha) ;
	eaha->sc = sc ;
	sc->go = eaha_go ;
	sc->watchdog = scsi_watchdog ;
	sc->probe = eaha_probe_target ;
	eaha->wd.reset = eaha_reset_scsibus ;
	sc->max_dma_data = -1 ; /* Lets be optimistic */
	sc->initiator_id = my_scsi_id ;
	eaha_reset(eaha,TRUE) ;
	eaha->I_hold_my_phys_address =
		kvtophys((vm_offset_t)&eaha->I_hold_my_phys_address) ;
	{
		erccb *e ;	
		eaha->toperccb = eaha->_erccbs ;
		for (e=eaha->_erccbs; e < eaha->_erccbs+NECCBS; e++) {
			e->next = e+1 ;
			e->_eccb.status =
				kvtophys((vm_offset_t) &e->status) ;
		}
		eaha->_erccbs[NECCBS-1].next = 0 ;

	}

	ui->sysdep1 = my_interrupt + 9 ;
	take_ctlr_irq(ui) ;

	printf("%s%d: [port 0x%x intr ch %d] my SCSI id is %d",
		ui->name, unit, port, my_interrupt + 9, my_scsi_id) ;

	outb(INTDEF(port), my_interrupt | 0x10) ;
					/* Enable interrupts. TRM4-15 */
	outb(EBCTRL(port),0x01) ; /* Enable board. TRM4-12 */

	{	target_info_t *t = eaha_tgt_alloc(eaha, my_scsi_id, 0) ;
		/* Haven't enabled target mode a la standard mode, because */
		/* it doesn't seem to be necessary. */
		sccpu_new_initiator(t, t) ;
	}

	/* Find targets, incl. ourselves. */

	for (target_id=0; target_id < SCSI_TARGETS; target_id++)
		if (target_id != sc->initiator_id) {
		        scsi_cmd_test_unit_ready_t      *cmd;
			erccb *_erccb = erccb_alloc(eaha) ;
			unsigned attempts = 0 ;
#define MAX_ATTEMPTS	2
			target_info_t temp_targ ;

			temp_targ.ior = 0 ;
			temp_targ.hw_state = (char *) eaha ;
			temp_targ.cmd_ptr = (char *) &_erccb->_eccb.cdb ;
			temp_targ.target_id = target_id ;
			temp_targ.lun = 0 ;
			temp_targ.cur_cmd = SCSI_CMD_TEST_UNIT_READY;

			cmd = (scsi_cmd_test_unit_ready_t *) temp_targ.cmd_ptr;

			do {
				cmd->scsi_cmd_code = SCSI_CMD_TEST_UNIT_READY;
				cmd->scsi_cmd_lun_and_lba1 = 0; /*assume 1 lun?*/
				cmd->scsi_cmd_lba2 = 0;
				cmd->scsi_cmd_lba3 = 0;
				cmd->scsi_cmd_ss_flags = 0;
				cmd->scsi_cmd_ctrl_byte = 0;    /* not linked */

				eaha_go( &temp_targ,
					sizeof(scsi_cmd_test_unit_ready_t),0,0);
				/* ints disabled, so call isr yourself. */
				while (temp_targ.done == SCSI_RET_IN_PROGRESS)
					if (inb(G2STAT(eaha->port)) & 0x02) {
						eaha_quiet = 1 ;
						eaha_intr(unit) ;
						eaha_quiet = 0 ;
					}
                                if (temp_targ.done == SCSI_RET_NEED_SENSE) {
                                        /* MUST get sense info : TRM6-34 */
					if (eaha_retrieve_sense_info(
						eaha, temp_targ.target_id,
						temp_targ.lun) &&
						attempts == MAX_ATTEMPTS-1) {

						printf(
						"\nTarget %d Check Condition : "
							,temp_targ.target_id) ;
						scsi_print_sense_data(&eaha_xsns
							[temp_targ.target_id]);
						printf("\n") ;
					}
                                }
			} while (temp_targ.done != SCSI_RET_SUCCESS &&
				temp_targ.done != SCSI_RET_ABORTED &&
				++attempts < MAX_ATTEMPTS) ;

			/*
			 * Recognize target which is present, whether or not
			 * it is ready, e.g. drive with removable media.
			 */
			if (temp_targ.done == SCSI_RET_SUCCESS ||
				temp_targ.done == SCSI_RET_NEED_SENSE &&
				_erccb->status.target_status.bits != 0) { /* Eureka */
			    installed.tgt_luns[target_id]=1;/*Assume 1 lun?*/
			    printf(", %s%d",
				did_banner++ ? "" : "target(s) at ",
				target_id);

			erccb_free(eaha, _erccb) ;

			    /* Normally, only LUN 0 */
			    if (installed.tgt_luns[target_id] != 1)
			    	printf("(%x)", installed.tgt_luns[target_id]);
			    /*
			     * Found a target
			     */
			    (void) eaha_tgt_alloc(eaha, target_id, 0);
				/* Why discard ? */
			} else
			    installed.tgt_luns[target_id]=0;
		}

	printf(".\n") ;
	splx(s);
	return 1 ;
}

int eaha_retrieve_sense_info (
        eaha_softc_t            eaha,
	int			tid,
	int			lun)
{
	int result ;
	int s ;
	target_info_t dummy_target ;	/* Keeps eaha_command() happy. HACK */
	erccb *_erccb1 = erccb_alloc(eaha) ;

	_erccb1->active_target = &dummy_target ;
	dummy_target.target_id = tid ;
	_erccb1->_eccb.command =
		EAHA_CMD_READ_SENS ;
	_erccb1->_eccb.lun = lun ;
	_erccb1->_eccb.sense_p = kvtophys((vm_offset_t) &eaha_xsns [tid]);
	_erccb1->_eccb.sense_len = sizeof(eaha_xsns [tid]);
	_erccb1->_eccb.ses = 1 ;
	s = splbio() ;
	eaha_command(eaha->port,_erccb1) ;
	while ((inb(G2STAT(eaha->port)) & 0x02) == 0) ;
	outb(G2CNTRL(eaha->port),0x40);/* Clear int */
	splx(s) ;
	result = _erccb1->status.target_status.bits != 0 ;
	erccb_free(eaha,_erccb1) ;
	return result ;
}

/*
 * Start a SCSI command on a target (enhanced mode)
 */
eaha_go(
	target_info_t		*tgt,
	int			cmd_count,
	int			in_count,
	boolean_t		cmd_only)/*lint: unused*/
{
	eaha_softc_t		eaha;
	int			s;
	erccb			*_erccb;
	int			len;
	vm_offset_t		virt;
	int tid = tgt->target_id ;

#ifdef CBUS
	at386_io_lock_state();
#endif
	LOG(1,"go");

#ifdef CBUS
	at386_io_lock(MP_DEV_WAIT);
#endif
	eaha = (eaha_softc_t)tgt->hw_state;

	if(eaha->has_sense_info[tid]) {
		(void) eaha_retrieve_sense_info
			(eaha, tid, eaha->sense_info_lun[tid]) ;
		eaha->has_sense_info[tid] = FALSE ;
		if (tgt->cur_cmd == SCSI_CMD_REQUEST_SENSE) {
			bcopy(&eaha_xsns[tid],tgt->cmd_ptr,in_count) ;
			tgt->done = SCSI_RET_SUCCESS;
			tgt->transient_state.cmd_count = cmd_count;
			tgt->transient_state.out_count = 0;
			tgt->transient_state.in_count = in_count;
			/* Fake up interrupt */
			/* Highlights from eaha_initiator_intr(), */
			/* ignoring errors */
			if (tgt->ior)
				(*tgt->dev_ops->restart)( tgt, TRUE);
#ifdef CBUS
			at386_io_unlock();
#endif
			return ;
		}
	}

/* XXX delay the handling of the ccb till later */
	_erccb = (erccb *)
		((unsigned)tgt->cmd_ptr - (unsigned) &((erccb *) 0)->_eccb.cdb);
	/* Tell *rccb about target, eg. id ? */
	_erccb->active_target = tgt;

	/*
	 * We can do real DMA.
	 */
/*	tgt->transient_state.copy_count = 0;	unused */
/*	tgt->transient_state.dma_offset = 0;	unused */

	tgt->transient_state.cmd_count = cmd_count;

	if ((tgt->cur_cmd == SCSI_CMD_WRITE) ||
	    (tgt->cur_cmd == SCSI_CMD_LONG_WRITE)){
		io_req_t	ior = tgt->ior;
		register int	len = ior->io_count;

		tgt->transient_state.out_count = len;

		/* How do we avoid leaks here ?  Trust the board
		   will do zero-padding, for now.  XXX CHECKME */
#if 0
		if (len < tgt->block_size) {
			bzero(to + len, tgt->block_size - len);
			len = tgt->block_size;
			tgt->transient_state.out_count = len;
		}
#endif
	} else {
		tgt->transient_state.out_count = 0;
	}

	/* See above for in_count < block_size */
	tgt->transient_state.in_count = in_count;

	/*
	 * Setup CCB state
	 */
	tgt->done = SCSI_RET_IN_PROGRESS;

	switch (tgt->cur_cmd) {
	    case SCSI_CMD_READ:
	    case SCSI_CMD_LONG_READ:
		LOG(9,"readop");
		virt = (vm_offset_t)tgt->ior->io_data;
		len = tgt->transient_state.in_count;
		break;
	    case SCSI_CMD_WRITE:
	    case SCSI_CMD_LONG_WRITE:
		LOG(0x1a,"writeop");
		virt = (vm_offset_t)tgt->ior->io_data;
		len = tgt->transient_state.out_count;
		break;
	    case SCSI_CMD_INQUIRY:
	    case SCSI_CMD_REQUEST_SENSE:
	    case SCSI_CMD_MODE_SENSE:
	    case SCSI_CMD_RECEIVE_DIAG_RESULTS:
	    case SCSI_CMD_READ_CAPACITY:
	    case SCSI_CMD_READ_BLOCK_LIMITS:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		virt = (vm_offset_t)tgt->cmd_ptr;
		len = tgt->transient_state.in_count;
		break;
	    case SCSI_CMD_MODE_SELECT:
	    case SCSI_CMD_REASSIGN_BLOCKS:
	    case SCSI_CMD_FORMAT_UNIT:
		tgt->transient_state.cmd_count = sizeof(scsi_command_group_0);
		len =
		tgt->transient_state.out_count = cmd_count - sizeof(scsi_command_group_0);
		virt = (vm_offset_t)tgt->cmd_ptr+sizeof(scsi_command_group_0);
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		break;
	    default:
		LOG(0x1c,"cmdop");
		LOG(0x80+tgt->cur_cmd,0);
		virt = 0;
		len = 0;
	}

	eaha_prepare_rccb(tgt, _erccb, virt, len);	

	_erccb->_eccb.lun = tgt->lun;

	/*
	 * XXX here and everywhere, locks!
	 */
	s = splbio();

	simple_lock(&eaha->aha_lock);
	if (eaha->wd.nactive++ == 0)
		eaha->wd.watchdog_state = SCSI_WD_ACTIVE;
	simple_unlock(&eaha->aha_lock);

	LOG(3,"enqueue");

	eaha_command(eaha->port, _erccb) ;

	splx(s);
#ifdef CBUS
	at386_io_unlock();
#endif
}

eaha_prepare_rccb(
	target_info_t		*tgt,
	erccb			*_erccb,
	vm_offset_t		virt,
	vm_size_t		len)
{
	_erccb->_eccb.cdb_len = tgt->transient_state.cmd_count;

	_erccb->_eccb.command = EAHA_CMD_INIT_CMD;/* default common case */

	if (virt == 0) {
		/* no xfers */
		_erccb->_eccb.scather = 0 ;
		_erccb->_eccb.scathlen = 0 ;
                _erccb->_eccb.sg = 0 ;
	} else {
		/* messy xfer */
		scather_entry		*seglist;
		vm_size_t		l1, off;

		_erccb->_eccb.sg = 1 ;

		if (tgt->dma_ptr == 0)
			eaha_alloc_segment_list(tgt);
		seglist = (scather_entry *) tgt->dma_ptr;

		_erccb->_eccb.scather = kvtophys((vm_offset_t) seglist);

		l1 = MACHINE_PGBYTES - (virt & (MACHINE_PGBYTES - 1));
		if (l1 > len)
			l1 = len ;

		off = 1;/* now #pages */
		while (1) {
			seglist->ptr = kvtophys(virt) ;
			seglist->len = l1 ;
			seglist++;

			if (len <= l1)
				break ;
			len-= l1 ;
			virt += l1; off++;

			l1 = (len > MACHINE_PGBYTES) ? MACHINE_PGBYTES : len;
		}
		_erccb->_eccb.scathlen = off * sizeof(*seglist);
	}
}

/*
 * Allocate dynamically segment lists to
 * targets (for scatter/gather)
 */
vm_offset_t	eaha_seglist_next = 0, eaha_seglist_end = 0 ;
#define	EALLOC_SIZE	(SCATHER_ENTRIES * sizeof(scather_entry))

eaha_alloc_segment_list(
	target_info_t	*tgt)
{

/* XXX locking */
/* ? Can't spl() for unknown duration */
	if ((eaha_seglist_next + EALLOC_SIZE) > eaha_seglist_end) {
		(void)kmem_alloc_wired(kernel_map,&eaha_seglist_next,PAGE_SIZE);
		eaha_seglist_end = eaha_seglist_next + PAGE_SIZE;
	}
	tgt->dma_ptr = (char *)eaha_seglist_next;
	eaha_seglist_next += EALLOC_SIZE;
/* XXX locking */
}

/*
 *
 * shameless copy from above
 */
eaha_reset_scsibus(
	register eaha_softc_t	eaha)
{
	register target_info_t	*tgt;
	register 		port = eaha->port;
	register int		i;

	for (i = 0; i < NECCBS; i++) {
		tgt = eaha->_erccbs[i].active_target;
		if (/*scsi_debug &&*/ tgt)
			printf("Target %d was active, cmd x%x in x%x out x%x\n", 
				tgt->target_id, tgt->cur_cmd,
				tgt->transient_state.in_count,
				tgt->transient_state.out_count);
	}
	eaha_reset(eaha, FALSE);
	delay(35);
	/* no interrupt will come */
	eaha_bus_reset(eaha);
}

boolean_t
eaha_probe_target(
	target_info_t		*tgt,
	io_req_t		ior)
{
	eaha_softc_t    eaha = eaha_softc_pool[tgt->masterno];
	boolean_t	newlywed;

	newlywed = (tgt->cmd_ptr == 0);
	if (newlywed) {
		/* desc was allocated afresh */
		(void) eaha_tgt_alloc(eaha,tgt->target_id, tgt);
	}

	if (scsi_inquiry(tgt, SCSI_INQ_STD_DATA) == SCSI_RET_DEVICE_DOWN)
		return FALSE;

	tgt->flags = TGT_ALIVE;
	return TRUE;
}


/*
 * Interrupt routine (enhanced mode)
 *	Take interrupts from the board
 *
 * Implementation:
 *	TBD
 */
eaha_intr(
	int			unit)
{
	register eaha_softc_t	eaha;
	register		port;
	unsigned 		g2intst, g2stat, g2stat2 ;
	vm_offset_t			mbi ;
	erccb			*_erccb ;
	status_block		*status ;

#if	MAPPABLE
	extern boolean_t	rz_use_mapped_interface;

	if (rz_use_mapped_interface) {
                EAHA_intr(unit);
                return ;
        }
#endif	/*MAPPABLE*/

	eaha = eaha_softc_pool[unit];
	port = eaha->port;

	LOG(5,"\n\tintr");
gotintr:
	/* collect ephemeral information */
	
	g2intst = inb(G2INTST(port)) ;		/* See TRM4-22..23 */
	g2stat  = inb(G2STAT(port)) ;	/*lint:set,not used*/
	g2stat2 = inb(G2STAT2(port)) ; 	/*lint:set,not used*/
	mbi = (vm_offset_t) inb(MBOXIN0(port)) + (inb(MBOXIN1(port))<<8) +
		(inb(MBOXIN2(port))<<16) + (inb(MBOXIN3(port))<<24) ;

	/* we got an interrupt allright */
	if (eaha->wd.nactive)
		eaha->wd.watchdog_state = SCSI_WD_ACTIVE;

	outb(G2CNTRL(port),0x40) ;	/* Clear EISA interrupt */

	switch(g2intst>>4) {
		case 0x07 :	/* hardware error ? */
		case 0x0a :	/* immediate command complete - don't expect */
		case 0x0e :	/* ditto with failure */
		default :
			printf( "aha%d: Bogus status (x%x) in MBI\n",
				unit, mbi);
			gimmeabreak() ; /* Any of above is disaster */
			break; 

		case 0x0d :	/* Asynchronous event TRM6-41 */
			if ((g2intst & 0x0f) == (inb(SCSIDEF(eaha->port)) & 0x0f))
				eaha_reset_scsibus(eaha) ;
			else
				eaha_target_intr(eaha, mbi, g2intst & 0x0f);
			break;

		case 0x0c : 	/* ccb complete with error */
		case 0x01 :	/* ccb completed with success */
		case 0x05 :	/* ccb complete with success after retry */

			_erccb = (erccb *)
				( ((vm_offset_t)&eaha->I_hold_my_phys_address) +
				(mbi - eaha->I_hold_my_phys_address) -
				(vm_offset_t)&(((erccb *)0)->_eccb) ) ;
				/* That ain't necessary. As kernel (must be) */
				/* contiguous, only need delta to translate */

			status = &_erccb->status ;

#ifdef NOTDEF
			if (!eaha_quiet && (!status->don || status->qf ||
				status->sc || status->dover ||
				status->ini || status->me)) {
				printf("\nccb complete error G2INTST=%02X\n",
					g2intst) ;
				DUMP(*_erccb) ;
				gimmeabreak() ;
			}
#endif

			eaha_initiator_intr(eaha, _erccb);
			break;
	}

	/* See if more work ready */
	if (inb(G2STAT(port)) & 0x02) {
		LOG(7,"\n\tre-intr");
		goto gotintr;
	}
}

/*
 * The interrupt routine turns to one of these two
 * functions, depending on the incoming mbi's role
 */
eaha_target_intr(
	eaha_softc_t	eaha,
	unsigned int	mbi,
	unsigned int 	peer)
{
	target_info_t		*initiator;	/* this is the caller */
	target_info_t		*self;		/* this is us */
	int			len;

	self = eaha->sc->target[eaha->sc->initiator_id];

	initiator = eaha->sc->target[peer];

	/* ..but initiators are not required to answer to our inquiry */
	if (initiator == 0) {
		/* allocate */
		initiator = eaha_tgt_alloc(eaha, peer, 0);

		/* We do not know here wether the host was down when
		   we inquired, or it refused the connection.  Leave
		   the decision on how we will talk to it to higher
		   level code */
		LOG(0xC, "new_initiator");
		sccpu_new_initiator(self, initiator);
		/* Bug fix: was (aha->sc, self, initiator); dph */
	}

	/* The right thing to do would be build an ior
	   and call the self->dev_ops->strategy routine,
	   but we cannot allocate it at interrupt level.
	   Also note that we are now disconnected from the
	   initiator, no way to do anything else with it
	   but reconnect and do what it wants us to do */

	/* obviously, this needs both spl and MP protection */
	self->dev_info.cpu.req_pending = TRUE;
	self->dev_info.cpu.req_id = peer ;
	self->dev_info.cpu.req_lun = (mbi>>24) & 0x07 ;
	self->dev_info.cpu.req_cmd =
		(mbi & 0x80000000) ? SCSI_CMD_SEND: SCSI_CMD_RECEIVE;
	len = mbi & 0x00ffffff ;

	self->dev_info.cpu.req_len = len;

	LOG(0xB,"tgt-mode-restart");
	(*self->dev_ops->restart)( self, FALSE);

	/* The call above has either prepared the data,
	   placing an ior on self, or it handled it some
	   other way */
	if (self->ior == 0)
		return;	/* I guess we'll do it later */

	{
		erccb	*_erccb ;

		_erccb = erccb_alloc(eaha) ;
		_erccb->active_target = initiator;
		_erccb->_eccb.command = EAHA_CMD_TARG_CMD ;
		_erccb->_eccb.ses = 1 ;
		_erccb->_eccb.dir = (self->cur_cmd == SCSI_CMD_SEND) ? 1 : 0 ;

		eaha_prepare_rccb(initiator, _erccb,
			(vm_offset_t)self->ior->io_data, self->ior->io_count);
		_erccb->_eccb.lun = initiator->lun;

		simple_lock(&eaha->aha_lock);
		if (eaha->wd.nactive++ == 0)
			eaha->wd.watchdog_state = SCSI_WD_ACTIVE;
		simple_unlock(&eaha->aha_lock);
		
		eaha_command(eaha->port, _erccb);
	}
}

eaha_initiator_intr(
	eaha_softc_t	eaha,
	erccb		*_erccb)
{
	scsi2_status_byte_t	status;
	target_info_t		*tgt;

	tgt = _erccb->active_target;
	_erccb->active_target = 0;

	/* shortcut (sic!) */
	if (_erccb->status.ha_status == HA_STATUS_SUCCESS)
		goto allok;

	switch (_erccb->status.ha_status) {	/* TRM6-17 */
	    case HA_STATUS_SUCCESS :
allok:
		status = _erccb->status.target_status ;
		if (status.st.scsi_status_code != SCSI_ST_GOOD) {
			scsi_error(tgt, SCSI_ERR_STATUS, status.bits, 0);
			tgt->done = (status.st.scsi_status_code == SCSI_ST_BUSY) ?
				SCSI_RET_RETRY : SCSI_RET_NEED_SENSE;
		} else
			tgt->done = SCSI_RET_SUCCESS;
		break;

	    case HA_STATUS_SEL_TIMEOUT :
		if (tgt->flags & TGT_FULLY_PROBED)
			tgt->flags = 0; /* went offline */
		tgt->done = SCSI_RET_DEVICE_DOWN;
		break;

	    case HA_STATUS_OVRUN :
                /* BUT we don't know if this is an underrun.
                   It is ok if we get less data than we asked
                   for, in a number of cases.  Most boards do not
                   seem to generate this anyways, but some do.  */
                { register int cmd = tgt->cur_cmd;
                        switch (cmd) {
                        case SCSI_CMD_INQUIRY:
                        case SCSI_CMD_REQUEST_SENSE:
			case SCSI_CMD_RECEIVE_DIAG_RESULTS:
			case SCSI_CMD_MODE_SENSE:
				if (_erccb->status.du) /*Ignore underrun only*/
					break;
                        default:
                              printf("eaha: U/OVRUN on scsi command x%x\n",cmd);
                              gimmeabreak();
                        }
                }
                goto allok;
	    case HA_STATUS_BUS_FREE :
                printf("aha: bad disconnect\n");
		tgt->done = SCSI_RET_ABORTED;
		break;
	    case HA_STATUS_PHASE_ERROR :
		/* we'll get an interrupt soon */
                printf("aha: bad PHASE sequencing\n");
		tgt->done = SCSI_RET_ABORTED;
		break;
	    case HA_STATUS_BAD_OPCODE :
printf("aha: BADCCB\n");gimmeabreak();
		tgt->done = SCSI_RET_RETRY;
		break;

	    case HA_STATUS_HOST_ABORTED :
	    case HA_STATUS_ADP_ABORTED :
	    case HA_STATUS_NO_FIRM :
	    case HA_STATUS_NOT_TARGET :
	    case HA_STATUS_INVALID_LINK :	/* These aren't expected. */
	    case HA_STATUS_BAD_CBLOCK :
	    case HA_STATUS_DUP_CBLOCK :
	    case HA_STATUS_BAD_SCATHER :
	    case HA_STATUS_RSENSE_FAIL :
	    case HA_STATUS_TAG_REJECT :
	    case HA_STATUS_HARD_ERROR :
	    case HA_STATUS_TARGET_NOATTN :
	    case HA_STATUS_HOST_RESET :
	    case HA_STATUS_OTHER_RESET :
	    case HA_STATUS_PROG_BAD_SUM :
	    default :
		printf("aha: bad ha_status (x%x)\n", _erccb->status.ha_status);
		tgt->done = SCSI_RET_ABORTED;
		break;
	}

	eaha->has_sense_info [tgt->target_id] =
		(tgt->done == SCSI_RET_NEED_SENSE) ;
	if (eaha->has_sense_info [tgt->target_id])
		eaha->sense_info_lun [tgt->target_id] = tgt->lun ;

	LOG(8,"end");

	simple_lock(&eaha->aha_lock);
	if (eaha->wd.nactive-- == 1)
		eaha->wd.watchdog_state = SCSI_WD_INACTIVE;
	simple_unlock(&eaha->aha_lock);

	if (tgt->ior) {
		LOG(0xA,"ops->restart");
		(*tgt->dev_ops->restart)( tgt, TRUE);
	}

	return FALSE;/*lint: Always returns FALSE. ignored. */
}

#endif	/* NEAHA > 0 */
