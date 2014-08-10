/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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
 *	File: if_se_mapped.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/90
 *
 *	In-kernel side of the user-mapped ethernet driver.
 */

#include <ln.h>
#if     NLN > 0
#include <platforms.h>

#include <mach/machine/vm_types.h>
#include <machine/machspl.h>		/* spl definitions */
#include <chips/lance.h>
#include <chips/busses.h>

#include <device/device_types.h>
#include <device/errno.h>
#include <device/io_req.h>
#include <device/net_status.h>
#include <device/net_io.h>
#include <device/if_hdr.h>
#include <device/if_ether.h>

#include <vm/vm_kern.h>
#include <kern/eventcount.h>

#include <machine/machspl.h>

#ifdef	DECSTATION

#define	machine_btop	mips_btop

#define	kvctophys(v)	K0SEG_TO_PHYS((v))	/* kernel virtual cached */
#define	phystokvc(p)	PHYS_TO_K0SEG((p))	/* and back */
#define	kvutophys(v)	K1SEG_TO_PHYS((v))	/* kernel virtual uncached */
#define	phystokvu(p)	PHYS_TO_K1SEG((p))	/* and back */
	/* remap from k2 to k0 */
#define kvirt(v)        ((phystokvc(pmap_extract(pmap_kernel(),v))))

#include <mips/mips_cpu.h>
/*
 * Wired addresses and sizes
 */
#define SE0_REG_EMRG	(se_reg_t)(0xb8000000)

#define	REGBASE(unit)	(((u_int)SE_statii[unit].registers) - se_sw->regspace)

#define SE_REG_PHYS(unit)	kvutophys(REGBASE(unit)+se_sw->regspace)
#define SE_REG_SIZE		PAGE_SIZE

#define SE_BUF_PHYS(unit)	kvutophys(REGBASE(unit)+se_sw->bufspace)
#define SE_BUF_SIZE		(128*1024)

#define SE_ADR_PHYS(unit)	kvutophys(REGBASE(unit)+se_sw->romspace)
#define SE_ADR_SIZE		PAGE_SIZE
#endif	/*DECSTATION*/

#ifdef	VAXSTATION
#define	machine_btop	vax_btop
#endif	/*VAXSTATION*/

#ifdef LUNA88K
#  define machine_btop    	m88k_btop
#  define kvirt(v)		(v)
#  define kvctophys(v)    	pmap_extract(pmap_kernel(),(v))
#  define SE0_REG_EMRG    	((se_reg_t)0xF1000000U)
#  define REGBASE(unit)	(((u_int)SE_statii[unit].registers) - se_sw->regspace)
#  define SE_REG_PHYS(unit)	(REGBASE(unit) + se_sw->regspace)
#  define SE_REG_SIZE		PAGE_SIZE
#  define SE_BUF_PHYS(unit)	(REGBASE(unit) + se_sw->bufspace)
#  define SE_BUF_SIZE		(64*1024)
#  define SE_ADR_PHYS(unit)	kvctophys(REGBASE(unit) + se_sw->romspace)
#  define SE_ADR_SIZE		PAGE_SIZE
#  define wbflush()	/*empty*/
#endif /*LUNA88K*/

/*
 * Autoconf info
 */

static  vm_offset_t SEstd[NLN] = { 0 };
static	struct bus_device *SEinfo[NLN];
	void SE_attach();
	int SE_probe();

struct bus_driver SEdriver =
       { SE_probe, 0, SE_attach, 0, SEstd, "SE", SEinfo, };

/*
 * Externally visible functions
 */
int		SE_probe();		/* Kernel */
void		SE_intr(), SE_portdeath();
						/* User */
int		SE_open(), SE_close();
vm_offset_t	SE_mmap();


/* forward declarations */

static void SE_stop(unsigned int unit);

/*
 * Status information for all interfaces
 */
/*static*/ struct SE_status {
	se_reg_t			 registers;
	mapped_ether_info_t		 info;
	struct evc			 eventcounter;
} SE_statii[NLN];


/*
 * Probe the Lance to see (if) that it's there
 */
int
SE_probe(regbase, ui)
	vm_offset_t regbase;
	register struct bus_device *ui;
{
	int			unit = ui->unit;
	se_reg_t		regs;
	vm_offset_t     	addr;
	mapped_ether_info_t	info;
	struct SE_status	*self;


	if (unit >= NLN)
		return 0;

	self = &SE_statii[unit];

	printf("[mappable] ");

	regs = (se_reg_t) (regbase + se_sw->regspace);
	self->registers = regs;

	/*
	 * Reset the interface 
	 */
	SE_stop(unit);

	/*
	 * Grab a page to be mapped later to users 
	 */
	(void) kmem_alloc_wired(kernel_map, &addr, PAGE_SIZE);	
	/* 
	  on the decstation, kmem_alloc_wired returns virtual addresses
	  in the k2 seg. Since this page is going to get mapped in
	  user space, we need to transform it to a better understood
	  virtual address. The kvirt function does this.
  	*/
	bzero(addr, PAGE_SIZE);
	info = (mapped_ether_info_t) kvirt(addr);
	self->info = info;

	/*
	 * Set permanent info
	 */
	info->rom_stride = se_sw->romstride;
	info->ram_stride = se_sw->ramstride;
	info->buffer_size = se_sw->ramsize;
	info->buffer_physaddr = se_sw->ln_bufspace;

	/*
	 * Synch setup
	 */
	evc_init(&self->eventcounter);
	info->wait_event = self->eventcounter.ev_id;

	return 1;
}

void
SE_attach(ui)
	register struct bus_device *ui;
{
}
 

/*
 * Shut off the lance
 */
static void SE_stop(unsigned int unit)
{
	register se_reg_t	regs = SE_statii[unit].registers;

	if (regs == 0)
	    	/* Stray interrupt */
 		regs = SE0_REG_EMRG;

	regs[2] = CSR0_SELECT;	/* XXXX rap XXXX */
	wbflush();
	regs[0] = LN_CSR0_STOP;
	wbflush();
}


/*
 * Ethernet interface interrupt routine
 */
void SE_intr(unit,spllevel)
	int	unit;
	spl_t	spllevel;
{
	register struct SE_status *self = &SE_statii[unit];
	register se_reg_t	   regs = self->registers;
	register		   csr;

	if (regs == 0) {	/* stray */
		SE_stop(unit);
		return;
	}

	/* Acknowledge interrupt request, drop spurious intr */
	csr = regs[0];
	if ((csr & LN_CSR0_INTR) == 0)
		return;
	regs[0] = csr & LN_CSR0_WTC;		/* silence it */

	splx(spllevel);	/* drop priority now */

	/* Pass csr state up to user thread */
	if (self->info) {
		self->info->interrupt_count++;	/* total interrupts */
		self->info->saved_csr0 = csr;
	}

	/* Awake user thread */
	evc_signal(&self->eventcounter);
}
 

extern boolean_t se_use_mapped_interface[NLN];

/*
 * Device open procedure
 */
int SE_open(dev, flag, ior)
	io_req_t ior;
{
	int             unit = dev;
	register struct SE_status *self = &SE_statii[unit];

	if (unit >= NLN)
		return EINVAL;

	/*
	 * Silence interface, just in case 
	 */
	SE_stop(unit);

	/*
	 * Reset eventcounter
	 */
	evc_signal(&self->eventcounter);

	se_use_mapped_interface[unit] = 1;

	/*
	 * Do not turn Ether interrupts on.  The user can do it when ready
	 * to take them. 
	 */

	return 0;
}

/*
 * Device close procedure
 */
int SE_close(dev, flag)
{
	int             unit = dev;
	register struct SE_status *self = &SE_statii[unit];

	if (unit >= NLN)
		return EINVAL;

	/*
	 * Silence interface, in case user forgot
	 */
	SE_stop(unit);
	evc_signal(&self->eventcounter);

	se_normal(unit);

	return 0;
}


/*
 * Get status procedure.
 * We need to tell that we are mappable.
 */
io_return_t
SE_get_status(ifp, flavor, status, status_count)
/*	struct ifnet	*ifp; not really..*/
	int		flavor;
	dev_status_t	status;		/* pointer to OUT array */
	unsigned int	*status_count;		/* OUT */
{
	switch (flavor) {
	    case NET_STATUS:
	    {
		register struct net_status *ns = (struct net_status *)status;

		ns->min_packet_size = sizeof(struct ether_header);
		ns->max_packet_size = sizeof(struct ether_header) + ETHERMTU;
		ns->header_format   = HDR_ETHERNET;
		ns->header_size	    = sizeof(struct ether_header);
		ns->address_size    = 6;
		ns->flags	    = IFF_BROADCAST;
		ns->mapped_size	    = SE_BUF_SIZE + (3 * PAGE_SIZE);

		*status_count = NET_STATUS_COUNT;
		break;
	    }
/*	    case NET_ADDRESS:	find it yourself */
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

/*
 * Should not refuse this either
 */
int SE_set_status(dev, flavor, status, status_count)
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
void SE_portdeath(dev, dead_port)
{
}


/*
 * Virtual->physical mapping routine.
 */
vm_offset_t
SE_mmap(dev, off, prot)
	int		dev;
	vm_offset_t	off;
	vm_prot_t	prot;
{
	vm_offset_t     page;
	vm_offset_t     addr;
	int             unit = dev;

	/*
	 * The offset (into the VM object) defines the following layout
	 *
	 *	off	size	what
	 *	0	1pg	mapping information (csr & #interrupts)
	 *	1pg	1pg	lance registers
	 *	2pg	1pg	lance station address (ROM)
	 *	3pg	128k	lance buffers 
	 */
#define S0	PAGE_SIZE
#define S1	(S0+SE_REG_SIZE)
#define S2	(S1+SE_ADR_SIZE)
#define S3	(S2+SE_BUF_SIZE)

	if (off < S0) {
		addr = kvctophys (SE_statii[unit].info);
	} else if (off < S1) {
		addr = (vm_offset_t) SE_REG_PHYS(unit);
		off -= S0;
	} else if (off < S2) {
		addr = (vm_offset_t) SE_ADR_PHYS(unit);
		off -= S1;
	} else if (off < S3) {
		addr = (vm_offset_t) SE_BUF_PHYS(unit);
		off -= S2;
	} else
		return (EINVAL);

	page = machine_btop(addr + off);
	return (page);
}

#endif	NLN > 0
