/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	File: mapped_scsi.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	In-kernel side of the user-mapped SCSI driver.
 */

#include <asc.h>
#include <sii.h>
#define NRZ	(NASC+NSII)
#if	NRZ > 0
#include <platforms.h>

#include <machine/machspl.h>		/* spl definitions */

#include <device/device_types.h>
#include <device/io_req.h>
#include <chips/busses.h>

#include <vm/vm_kern.h>
#include <kern/eventcount.h>

#include <scsi/mapped_scsi.h>

#include <machine/machspl.h>

#ifdef	DECSTATION

#define	machine_btop	mips_btop

#define	kvctophys(v)	K0SEG_TO_PHYS((v))	/* kernel virtual cached */
#define	phystokvc(p)	PHYS_TO_K0SEG((p))	/* and back */
#define	kvutophys(v)	K1SEG_TO_PHYS((v))	/* kernel virtual uncached */
#define	phystokvu(p)	PHYS_TO_K1SEG((p))	/* and back */

#include <mips/mips_cpu.h>
#include <mips/PMAX/kn01.h>
#include <mips/PMAX/pmaz_aa.h>

#define SII_REG_PHYS(self)	kvutophys(self->registers.any)
#define	SII_RAM_PHYS(self)	(SII_REG_PHYS((self))+(KN01_SYS_SII_B_START-KN01_SYS_SII))
#define	SII_RAM_SIZE		(KN01_SYS_SII_B_END-KN01_SYS_SII_B_START)

#define ASC_REG_PHYS(self)	kvutophys(self->registers.any)
#define ASC_DMAR_PHYS(self)	(ASC_REG_PHYS((self))+ ASC_OFFSET_DMAR)
#define ASC_RAM_PHYS(self)	(ASC_REG_PHYS((self))+ ASC_OFFSET_RAM)

#define	PAD_7061(n)		short n
#define PAD_53C94(n)		char n[3]

#endif	/*DECSTATION*/

#ifdef	VAXSTATION
#define	machine_btop	vax_btop
#endif	/*VAXSTATION*/

#ifdef P40

#define	machine_btop	mips_btop

#define	kvctophys(v)	K0SEG_TO_PHYS((v))	/* kernel virtual cached */
#define	phystokvc(p)	PHYS_TO_K0SEG((p))	/* and back */
#define	kvutophys(v)	K1SEG_TO_PHYS((v))	/* kernel virtual uncached */
#define	phystokvu(p)	PHYS_TO_K1SEG((p))	/* and back */

#include <mips/mips_cpu.h>

#define ASC_RAM_SIZE		0
#define ASC_OFFSET_DMAR		0
#define ASC_OFFSET_RAM		0

#define ASC_REG_PHYS(self)	kvutophys(self->registers.any)
#define ASC_DMAR_PHYS(self)	(ASC_REG_PHYS((self))+ ASC_OFFSET_DMAR)
#define ASC_RAM_PHYS(self)	(ASC_REG_PHYS((self))+ ASC_OFFSET_RAM)
#endif  /* P40 */

/*
 * Phys defines for the various supported HBAs
 */

/* DEC7061	*/
#include <scsi/adapters/scsi_7061.h>

#ifdef	PAD_7061

typedef struct {
	volatile unsigned short	sii_sdb;	/* rw: Data bus and parity */
	PAD_7061(pad0);
	volatile unsigned short	sii_sc1;	/* rw: scsi signals 1 */
	PAD_7061(pad1);
	volatile unsigned short	sii_sc2;	/* rw: scsi signals 2 */
	PAD_7061(pad2);
	volatile unsigned short	sii_csr;	/* rw: control and status */
	PAD_7061(pad3);
	volatile unsigned short	sii_id;		/* rw: scsi bus ID */
	PAD_7061(pad4);
	volatile unsigned short	sii_sel_csr;	/* rw: selection status */
	PAD_7061(pad5);
	volatile unsigned short	sii_destat;	/* ro: selection detector status */
	PAD_7061(pad6);
	volatile unsigned short	sii_dstmo;	/* unsupp: dssi timeout */
	PAD_7061(pad7);
	volatile unsigned short	sii_data;	/* rw: data register */
	PAD_7061(pad8);
	volatile unsigned short	sii_dma_ctrl;	/* rw: dma control reg */
	PAD_7061(pad9);
	volatile unsigned short	sii_dma_len;	/* rw: length of transfer */
	PAD_7061(pad10);
	volatile unsigned short	sii_dma_adr_low;/* rw: low address */
	PAD_7061(pad11);
	volatile unsigned short	sii_dma_adr_hi;	/* rw: high address */
	PAD_7061(pad12);
	volatile unsigned short	sii_dma_1st_byte;/* rw: initial byte */
	PAD_7061(pad13);
	volatile unsigned short	sii_stlp;	/* unsupp: dssi short trgt list ptr */
	PAD_7061(pad14);
	volatile unsigned short	sii_ltlp;	/* unsupp: dssi long " " " */
	PAD_7061(pad15);
	volatile unsigned short	sii_ilp;	/* unsupp: dssi initiator list ptr */
	PAD_7061(pad16);
	volatile unsigned short	sii_dssi_csr;	/* unsupp: dssi control */
	PAD_7061(pad17);
	volatile unsigned short	sii_conn_csr;	/* rc: connection interrupt control */
	PAD_7061(pad18);
	volatile unsigned short	sii_data_csr;	/* rc: data interrupt control */
	PAD_7061(pad19);
	volatile unsigned short	sii_cmd;	/* rw: command register */
	PAD_7061(pad20);
	volatile unsigned short	sii_diag_csr;	/* rw: disgnostic status */
	PAD_7061(pad21);
} sii_padded_regmap_t;

#else	/*!PAD_7061*/

typedef sii_regmap_t	sii_padded_regmap_t;

#endif	/*!PAD_7061*/

/* NCR 53C94	*/
#include <scsi/adapters/scsi_53C94.h>

#ifdef	PAD_53C94
typedef struct {
	volatile unsigned char	asc_tc_lsb;	/* rw: Transfer Counter LSB */
	PAD_53C94(pad0);
	volatile unsigned char	asc_tc_msb;	/* rw: Transfer Counter MSB */
	PAD_53C94(pad1);
	volatile unsigned char	asc_fifo;	/* rw: FIFO top */
	PAD_53C94(pad2);
	volatile unsigned char	asc_cmd;	/* rw: Command */
	PAD_53C94(pad3);
	volatile unsigned char	asc_csr;	/* r:  Status */
/*#define		asc_dbus_id asc_csr	/* w: Destination Bus ID */
	PAD_53C94(pad4);
	volatile unsigned char	asc_intr;	/* r:  Interrupt */
/*#define		asc_sel_timo asc_intr	/* w: (re)select timeout */
	PAD_53C94(pad5);
	volatile unsigned char	asc_ss;		/* r:  Sequence Step */
/*#define		asc_syn_p asc_ss	/* w: synchronous period */
	PAD_53C94(pad6);
	volatile unsigned char	asc_flags;	/* r:  FIFO flags + seq step */
/*#define		asc_syn_o asc_flags	/* w: synchronous offset */
	PAD_53C94(pad7);
	volatile unsigned char	asc_cnfg1;	/* rw: Configuration 1 */
	PAD_53C94(pad8);
	volatile unsigned char	asc_ccf;	/* w:  Clock Conv. Factor */
	PAD_53C94(pad9);
	volatile unsigned char	asc_test;	/* w:  Test Mode */
	PAD_53C94(pad10);
	volatile unsigned char	asc_cnfg2;	/* rw: Configuration 2 */
	PAD_53C94(pad11);
	volatile unsigned char	asc_cnfg3;	/* rw: Configuration 3 */
	PAD_53C94(pad12);
	volatile unsigned char	asc_rfb;	/* w:  Reserve FIFO byte */
	PAD_53C94(pad13);
} asc_padded_regmap_t;

#else	/* !PAD_53C94 */

typedef asc_regmap_t	asc_padded_regmap_t;

#endif	/* !PAD_53C94 */

/*
 * Co-existency with in-kernel drivers
 */
boolean_t	rz_use_mapped_interface = FALSE;

/*
 * Status information for all HBAs
 */
/*static*/ struct RZ_status {
	union {
		unsigned long		any;
		asc_padded_regmap_t	*asc;
		sii_padded_regmap_t	*sii;
	} registers;
	int				(*stop)();
	vm_offset_t			(*mmap)();
	mapped_scsi_info_t		info;
	struct evc			eventcounter;
} RZ_statii[NRZ];

typedef struct RZ_status	*RZ_status_t;


/*
 * Probe routine for all HBAs
 */
RZ_probe(regbase, ui, hba)
	unsigned long			regbase;
	register struct bus_device 	*ui;
{
	int			unit = ui->unit;
	vm_offset_t		addr;
	mapped_scsi_info_t	info;
	struct RZ_status	*self;

	printf("[mappable] ");

	self = &RZ_statii[unit];

	self->registers.any = regbase;

	/*
	 * Grab a page to be mapped later to users 
	 */
	(void) kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE);	/* kseg2 */
	bzero(addr, PAGE_SIZE);
	addr = pmap_extract(pmap_kernel(), addr);	/* phys */
	info = (mapped_scsi_info_t) (phystokvc(addr));
	self->info = info;

	/*
	 * Set permanent info
	 */
	info->interrupt_count	=	0;
/*XXX*/	info->ram_size		=	ASC_RAM_SIZE;
	info->hba_type		=	hba;

	evc_init(&self->eventcounter);
	info->wait_event	=	self->eventcounter.ev_id;

	return 1;
}

/*
 * Device open procedure
 */
RZ_open(dev, flag, ior)
	io_req_t ior;
{
	int             	unit = dev;
	register RZ_status_t	self = &RZ_statii[unit];


	if (unit >= NRZ)
		return D_NO_SUCH_DEVICE;

	/*
	 * Silence interface, just in case 
	 */
	(*self->stop)(unit);

	/*
	 * Reset eventcounter
	 */
	evc_signal(&self->eventcounter);

	rz_use_mapped_interface = TRUE;

	/*
	 * Do not turn interrupts on.  The user can do it when ready
	 * to take them. 
	 */

	return 0;
}

/*
 * Device close procedure
 */
RZ_close(dev, flag)
{
	int             	unit = dev;
	register RZ_status_t	self = &RZ_statii[unit];

	if (unit >= NRZ)
		return D_NO_SUCH_DEVICE;

	/*
	 * Silence interface, in case user forgot
	 */
	(*self->stop)(unit);

	evc_signal(&self->eventcounter);

	rz_use_mapped_interface = FALSE;

	/* XXX	rz_kernel_mode(); XXX */

	return 0;
}


/*
 * Get status procedure.
 * We need to tell that we are mappable.
 */
io_return_t
RZ_get_status(dev, flavor, status, status_count)
	int		dev;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	return (D_SUCCESS);
}

/*
 * Should not refuse this either
 */
RZ_set_status(dev, flavor, status, status_count)
	int		dev;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	return (D_SUCCESS);
}

/*
 * Port death notification routine
 */
RZ_portdeath(dev, dead_port)
{
}

/*
 * Page mapping, switch off to HBA-specific for regs&ram
 */
vm_offset_t
RZ_mmap(dev, off, prot)
	int		dev;
{
	int             	unit = dev;
	register RZ_status_t	self = &RZ_statii[unit];
	vm_offset_t     	page;
	vm_offset_t     	addr;
	io_return_t		ret;

	if (off < SCSI_INFO_SIZE) {
		addr = kvctophys (self->info) + off;
		ret = D_SUCCESS;
	} else
		ret = (*self->mmap)(self, off, prot, &addr);

	if (ret != D_SUCCESS)
		return ret;

	page = machine_btop(addr);

	return (page);	
}


/*
 *---------------------------------------------------------------
 * 	The rest of the file contains HBA-specific routines
 *---------------------------------------------------------------
 */

#if	NASC > 0
/*
 * Routines for the NCR 53C94
 */
static
ASC_stop(unit)
{
	register RZ_status_t	   self = &RZ_statii[unit];
	register asc_padded_regmap_t *regs = self->registers.asc;
	int			   ack;

	ack = regs->asc_intr;	/* Just acknowledge pending interrupts */
}

ASC_probe(reg, ui)
	unsigned long			reg;
	register struct bus_device 	*ui;
{
	register RZ_status_t	self = &RZ_statii[ui->unit];
	static vm_offset_t	ASC_mmap();

	self->stop = ASC_stop;
	self->mmap = ASC_mmap;
	return RZ_probe(reg, ui, HBA_NCR_53c94);
}


ASC_intr(unit,spllevel)
	spl_t	spllevel;
{
	register RZ_status_t 	   self = &RZ_statii[unit];
	register asc_padded_regmap_t *regs = self->registers.asc;
	register        	   csr, intr, seq_step, cmd;

	/*
	 * Acknowledge interrupt request
	 *
	 * This clobbers some two other registers, therefore
	 * we read them beforehand.  It also clears the intr
	 * request bit, silencing the interface for now.
	 */
	csr = regs->asc_csr;

	/* drop spurious interrupts */
	if ((csr & ASC_CSR_INT) == 0)
		return;
	seq_step = regs->asc_ss;
	cmd = regs->asc_cmd;

	intr = regs->asc_intr;	/* ack */

	splx(spllevel);	/* drop priority */

	if (self->info) {
		self->info->interrupt_count++;	/* total interrupts */
		self->info->saved_regs.asc.csr = csr;
		self->info->saved_regs.asc.isr = intr;
		self->info->saved_regs.asc.seq = seq_step;
		self->info->saved_regs.asc.cmd = cmd;
	}

	/* Awake user thread */
	evc_signal(&self->eventcounter);
}

/*
 * Virtual->physical mapping routine for PMAZ-AA
 */
static vm_offset_t
ASC_mmap(self, off, prot, addr)
	RZ_status_t	self;
	vm_offset_t	off;
	vm_prot_t	prot;
	vm_offset_t	*addr;
{
	/*
	 * The offset (into the VM object) defines the following layout
	 *
	 *	off	size	what
	 *	0	1pg	mapping information (csr & #interrupts)
	 *	1pg	1pg	ASC registers
	 *	2pg	1pg	ASC dma
	 *	3pg	128k	ASC ram buffers
	 */

#define	ASC_END	(ASC_RAM_BASE+ASC_RAM_SIZE)

	if (off < ASC_DMAR_BASE)
		*addr = (vm_offset_t) ASC_REG_PHYS(self) + (off - SCSI_INFO_SIZE);
	else if (off < ASC_RAM_BASE)
		*addr = (vm_offset_t) ASC_DMAR_PHYS(self) + (off - ASC_REGS_BASE);
	else if (off < ASC_END)
		*addr = (vm_offset_t) ASC_RAM_PHYS(self) + (off - ASC_RAM_BASE);
	else
		return D_INVALID_SIZE;

	return D_SUCCESS;
}
#endif	NASC > 0

#if	NSII > 0
SII_stop(unit)
{
	register RZ_status_t	   self = &RZ_statii[unit];
	register sii_padded_regmap_t *regs = self->registers.sii;

	regs->sii_csr &= ~SII_CSR_IE;	/* disable interrupts */
					/* clear all wtc bits */
	regs->sii_conn_csr = regs->sii_conn_csr;
	regs->sii_data_csr = regs->sii_data_csr;
}

SII_probe(reg, ui)
	unsigned long			reg;
	register struct bus_device 	*ui;
{
	register RZ_status_t	self = &RZ_statii[ui->unit];
	static vm_offset_t	SII_mmap();

	self->stop = SII_stop;
	self->mmap = SII_mmap;
	return RZ_probe(reg, ui, HBA_DEC_7061);
}

SII_intr(unit,spllevel)
	spl_t	spllevel;
{
	register RZ_status_t 	   self = &RZ_statii[unit];
	register sii_padded_regmap_t *regs = self->registers.sii;
	register unsigned short	   conn, data;

	/*
	 * Disable interrupts, saving cause(s) first.
	 */
	conn = regs->sii_conn_csr;
	data = regs->sii_data_csr;

	/* drop spurious calls */
	if (((conn|data) & (SII_DTR_DI|SII_DTR_CI)) == 0)
		return;

	regs->sii_csr &= ~SII_CSR_IE;

	regs->sii_conn_csr = conn;
	regs->sii_data_csr = data;

	splx(spllevel);

	if (self->info) {
		self->info->interrupt_count++;	/* total interrupts */
		self->info->saved_regs.sii.sii_conn_csr = conn;
		self->info->saved_regs.sii.sii_data_csr = data;
	}

	/* Awake user thread */
	evc_signal(&self->eventcounter);
}

static vm_offset_t
SII_mmap(self, off, prot, addr)
	RZ_status_t	self;
	vm_offset_t	off;
	vm_prot_t	prot;
	vm_offset_t	*addr;
{
	/*
	 * The offset (into the VM object) defines the following layout
	 *
	 *	off	size	what
	 *	0	1pg	mapping information (csr & #interrupts)
	 *	1pg	1pg	SII registers
	 *	2pg	128k	SII ram buffer
	 */

#define	SII_END	(SII_RAM_BASE+SII_RAM_SIZE)

	if (off < SII_RAM_BASE)
		*addr = (vm_offset_t) SII_REG_PHYS(self) + (off - SCSI_INFO_SIZE);
	else if (off < SII_END)
		*addr = (vm_offset_t) SII_RAM_PHYS(self) + (off - SII_RAM_BASE);
	else
		return D_INVALID_SIZE;

	return D_SUCCESS;
}
#endif	NSII > 0

#endif	NRZ > 0
