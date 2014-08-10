/*
        Written 1994 by Donald Becker.

        This driver is for the Hewlett Packard PC LAN (27***) plus ethercards.
        These cards are sold under several model numbers, usually 2724*.

        This software may be used and distributed according to the terms
        of the GNU Public License, incorporated herein by reference.

        The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O

        Center of Excellence in Space Data and Information Sciences
                Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

        As is often the case, a great deal of credit is owed to Russ Nelson.
        The Crynwr packet driver was my primary source of HP-specific
        programming information.
*/

/*
 * Ported to mach by Stephen Clawson, sclawson@cs.utah.edu
 * University of Utah CSL.
 *
 * Derived from the Linux driver by Donald Becker.
 * 
 * Also uses code Shantanu Goel adapted from Donald Becker
 * for ns8930 support.
 *
 */

#include <hpp.h>
#if NHPP > 0

#include <sys/types.h>
#include "vm_param.h"
#include <kern/time_out.h>
#include <device/device_types.h>
#include <device/errno.h>
#include <device/io_req.h>
#include <device/if_hdr.h>
#include <device/if_ether.h>
#include <device/net_status.h>
#include <device/net_io.h>
#include <chips/busses.h>
#include <i386/ipl.h>
#include <i386/pio.h>
#include <i386at/gpl/if_nsreg.h>


/*
 * XXX - This is some gross glue garbage.  The io instructions really
 * should be integrated into pio.h...
 */
#define IO_DELAY __asm__ __volatile__("outb %al,$0x80")
#define outb_p(p, v)    { outb(p, v); IO_DELAY; }
#define inb_p(p)        ({ unsigned char _v; _v = inb(p); IO_DELAY; _v; })


static __inline void
insw(u_short port, void *addr, int cnt)
{
        __asm __volatile("cld\n\trepne\n\tinsw" :
                         : "d" (port), "D" (addr), "c" (cnt) : "%edi", "%ecx");
}

static __inline void
outsw(u_short port, void *addr, int cnt)
{
        __asm __volatile("cld\n\trepne\n\toutsw" :
                         : "d" (port), "S" (addr), "c" (cnt) : "%esi", "%ecx");
}


/*
   The HP EtherTwist chip implementation is a fairly routine DP8390
   implementation.  It allows both shared memory and programmed-I/O buffer
   access, using a custom interface for both.  The programmed-I/O mode is
   entirely implemented in the HP EtherTwist chip, bypassing the problem
   ridden built-in 8390 facilities used on NE2000 designs.  The shared
   memory mode is likewise special, with an offset register used to make
   packets appear at the shared memory base.  Both modes use a base and bounds
   page register to hide the Rx ring buffer wrap -- a packet that spans the
   end of physical buffer memory appears continuous to the driver. (c.f. the
   3c503 and Cabletron E2100)

   A special note: the internal buffer of the board is only 8 bits wide.
   This lays several nasty traps for the unaware:
   - the 8390 must be programmed for byte-wide operations
   - all I/O and memory operations must work on whole words (the access
     latches are serially preloaded and have no byte-swapping ability).

   This board is laid out in I/O space much like the earlier HP boards:
   the first 16 locations are for the board registers, and the second 16 are
   for the 8390.  The board is easy to identify, with both a dedicated 16 bit
   ID register and a constant 0x530* value in the upper bits of the paging
   register.
*/

#define HP_ID			0x00	/* ID register, always 0x4850. */
#define HP_PAGING		0x02	/* Registers visible @ 8-f, see PageName. */ 
#define HPP_OPTION		0x04	/* Bitmapped options, see HP_Option.*/
#define HPP_OUT_ADDR		0x08	/* I/O output location in Perf_Page.*/
#define HPP_IN_ADDR		0x0A	/* I/O input location in Perf_Page.*/
#define HP_DATAPORT		0x0c	/* I/O data transfer in Perf_Page.*/
#define HPP_NIC_OFFSET		0x10	/* Offset to the 8390 registers.*/
#define HP_IO_EXTENT		32

#define HP_START_PG		0x00	/* First page of TX buffer */
#define HP_STOP_PG		0x80	/* Last page +1 of RX ring */
/*#define HP_STOP_PG		0x1f

/* The register set selected in HP_PAGING. */
enum PageName {
	Perf_Page 	= 0,		/* Normal operation. */
	MAC_Page 	= 1,		/* The ethernet address (+checksum). */
	HW_Page 	= 2,		/* EEPROM-loaded hw parameters. */
	LAN_Page 	= 4,		/* Transciever type, testing, etc. */
	ID_Page 	= 6 }; 

/* The bit definitions for the HPP_OPTION register. */
enum HP_Option {
	NICReset 	= 1,  		/* Active low, really UNreset. */
	ChipReset 	= 2, 
	EnableIRQ 	= 4, 
	FakeIntr 	= 8, 
	BootROMEnb 	= 0x10, 
	IOEnb 		= 0x20,
	MemEnable 	= 0x40, 
	ZeroWait 	= 0x80, 
	MemDisable 	= 0x1000, };


void hpp_reset_8390(struct nssoftc *ns);

void hpp_mem_block_input(struct nssoftc *ns, int, char *, int);
int hpp_mem_block_output(struct nssoftc *ns, int, char *, int);
void hpp_io_block_input(struct nssoftc *ns, int, char *, int);
int hpp_io_block_output(struct nssoftc *ns, int,char *, int);


/*
 * Watchdog timer.
 */
int	hppwstart = 0;
void	hppwatch(void);


/* 
 * Autoconfig structures.
 */
int hpp_std[] = { 0x200, 0x240, 0x280, 0x2C0, 0x300, 0x320, 0x340, 0 };
struct 	bus_device *hpp_info[NHPP];
int 	hpp_probe();
void 	hpp_attach();
struct 	bus_driver hppdriver = {
	hpp_probe, 0, hpp_attach, 0, hpp_std, "hpp", hpp_info, 0, 0, 0
};


/* 
 * ns8390 state.
 */
struct	nssoftc hppnssoftc[NHPP];


/*
 * hpp state.
 */
struct hppsoftc {
	unsigned long	rmem_start;	/* shmem "recv" start */
	unsigned long	rmem_end;	/* shmem "recv" end */
	unsigned long	mem_start;	/* shared mem start */
	unsigned long	mem_end;	/* shared mem end */
} hppsoftc[NHPP];


/* 
 * Probe a list of addresses for the card.
 *
 */
int hpp_probe(port, dev)
	int 	port;
	struct 	bus_device *dev;
{
	int unit = dev->unit;
	char *str = "hp-plus ethernet board %d out of range.\n";
	caddr_t base = (caddr_t) (dev ? dev->address : 0);
	int i;

	if ((unit < 0) || (unit >= NHPP)) {
		printf(str, unit);
		return(0);
	}

	/* Check a single specified location. */
	if (base > (caddr_t) 0x1ff)   
		return hpp_probe1(dev, base);
	else if (base != 0)				/* Don't probe at all. */
		return 0;

	for (i = 0; hpp_std[i]; i++) {
		int ioaddr = hpp_std[i];

		if ( ioaddr > 0 && hpp_probe1(dev, ioaddr) ) {
			dev->address = ioaddr;
			hpp_std[i] = -1; 		/* Mark address used */
			return(1);
		}
	}

	return 0;
}



/* 
 * Do the interesting part of the probe at a single address. 
 *
 */
int hpp_probe1(dev, ioaddr)
	struct bus_device *dev;
	int ioaddr;
{
	int i;
	u_char checksum = 0;
	int mem_start;
	
	struct hppsoftc *hpp = &hppsoftc[dev->unit];
	struct nssoftc	*ns  = &hppnssoftc[dev->unit];
	struct ifnet *ifp = &ns->sc_if;

	/* Check for the HP+ signature, 50 48 0x 53. */
	if (inw(ioaddr + HP_ID) != 0x4850
		|| (inw(ioaddr + HP_PAGING) & 0xfff0) != 0x5300)
		return 0;


	printf("%s%d: HP PClan plus at %#3x,", dev->name, dev->unit, ioaddr); 
	/* Retrieve and checksum the station address. */
	outw(ioaddr + HP_PAGING, MAC_Page);

	printf("MAC_Page = %d, ioaddr = %x\n", MAC_Page, ioaddr);

	for(i = 0; i < ETHER_ADDR_LEN; i++) {
		u_char inval = inb(ioaddr + 8 + i);
 		ns->sc_addr[i] = inval;  
		checksum += inval;
		printf(" %2.2x", inval);
	}
	checksum += inb(ioaddr + 14);

	if (checksum != 0xff) {
		printf(" bad checksum %2.2x.\n", checksum);
		return 0;
	} else {
		/* Point at the Software Configuration Flags. */
		outw(ioaddr + HP_PAGING, ID_Page);
		printf(" ID %4.4x", inw(ioaddr + 12));
	}


	/* Read the IRQ line. */
	outw(ioaddr + HP_PAGING, HW_Page);
	{
		int irq = inb(ioaddr + 13) & 0x0f;
		int option = inw(ioaddr + HPP_OPTION);

		dev->sysdep1 = irq;
		take_dev_irq(dev);

		if (option & MemEnable) {
			mem_start = inw(ioaddr + 9) << 8;
			printf(", IRQ %d, memory address %#x.\n", irq, mem_start);
		} else {
			mem_start = 0;
			printf(", IRQ %d, programmed-I/O mode.\n", irq);
		}
	}

	/* Set the wrap registers for string I/O reads.   */
	outw( ioaddr + 14, (HP_START_PG + TX_2X_PAGES) | ((HP_STOP_PG - 1) << 8));

	/* Set the base address to point to the NIC, not the "real" base! */
	ns->sc_port = ioaddr + HPP_NIC_OFFSET;

	ns->sc_name = dev->name;
	ns->sc_unit = dev->unit;
	ns->sc_pingpong = 0;		/* turn off pingpong mode */
	ns->sc_word16 = 0;		/* Agggghhhhh! Debug time: 2 days! */
	ns->sc_txstrtpg = HP_START_PG;
	ns->sc_rxstrtpg = HP_START_PG + TX_2X_PAGES;
	ns->sc_stoppg = HP_STOP_PG;
	

	ns->sc_reset = hpp_reset_8390;
	ns->sc_input = hpp_io_block_input;
	ns->sc_output = hpp_io_block_output;

	/* Check if the memory_enable flag is set in the option register. */
	if (mem_start) {
		ns->sc_input = hpp_mem_block_input;
		ns->sc_output = hpp_mem_block_output;
		hpp->mem_start = mem_start;
		hpp->rmem_start = hpp->mem_start + TX_2X_PAGES * 256;
		hpp->mem_end = hpp->rmem_end
			= hpp->mem_start + (HP_STOP_PG - HP_START_PG) * 256;
	}

	outw(ioaddr + HP_PAGING, Perf_Page);

	/* Leave the 8390 and HP chip reset. */
	outw( ioaddr + HPP_OPTION, inw(ioaddr + HPP_OPTION) & ~EnableIRQ );

	/*
	 * Initialize interface header.
	 */
	ifp->if_unit = dev->unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST;
	ifp->if_header_size = sizeof(struct ether_header);
	ifp->if_header_format = HDR_ETHERNET;
	ifp->if_address_size = ETHER_ADDR_LEN;
	ifp->if_address = ns->sc_addr;
	if_init_queues(ifp);

	return (1);
}

/*  
 * XXX
 *
 * this routine really should do the invasive part of the setup.
 */
void
hpp_attach(dev)
	struct bus_device *dev;
{
	/* NULL */
}



int
hppopen(dev, flag)
	dev_t	dev;
	int	flag;
{
	int s, unit = minor(dev);
	struct bus_device 	*bd;
	struct hppsoftc 	*hpp;
	struct nssoftc		*ns = &hppnssoftc[unit];

	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;
	int option_reg;

	if (unit < 0 || unit >= NHPP ||
	    (bd = hpp_info[unit]) == 0 || !(bd->alive))
		return ENXIO;

	/*
	 * Start watchdog.
	 */
	if (!hppwstart) {
		hppwstart++;
		timeout(hppwatch, 0, hz);
	}
	hpp = &hppsoftc[unit];
	ns->sc_if.if_flags |= IFF_UP;

	s = splimp();

	/* Reset the 8390 and HP chip. */
	option_reg = inw(ioaddr + HPP_OPTION);
	outw( ioaddr + HPP_OPTION, option_reg & ~(NICReset + ChipReset) );
	IO_DELAY; IO_DELAY;

	/* Unreset the board and enable interrupts. */
	outw( ioaddr + HPP_OPTION, option_reg | (EnableIRQ + NICReset + ChipReset));

	/* Set the wrap registers for programmed-I/O operation.   */
	outw( ioaddr + HP_PAGING, HW_Page );
	outw( ioaddr + 14, (HP_START_PG + TX_2X_PAGES) | ((HP_STOP_PG - 1) << 8) );

	/* Select the operational page. */
	outw( ioaddr + HP_PAGING, Perf_Page );
	nsinit(ns);

	splx(s);

	return (0);
}

/* 
 * needs to be called at splimp()?
 *
 */
void
hpp_reset_8390(ns)
	struct nssoftc *ns;
{
	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;
	int option_reg = inw(ioaddr + HPP_OPTION);

	outw( ioaddr + HPP_OPTION, option_reg & ~(NICReset + ChipReset) );
	/* Pause a few cycles for the hardware reset to take place. */
	IO_DELAY;
	IO_DELAY;
	ns->sc_txing = 0;
	outw( ioaddr + HPP_OPTION, option_reg | (EnableIRQ + NICReset + ChipReset) );

	/*  
	 * XXX - I'm not sure there needs to be this many IO_DELAY's...
	 */
	IO_DELAY; IO_DELAY;
	IO_DELAY; IO_DELAY;

	if ((inb_p(ioaddr + HPP_NIC_OFFSET + EN0_ISR) & ENISR_RESET) == 0)
		printf("%s: hp_reset_8390() did not complete.\n", ns->sc_name);

	return;
}


/* 
 * Block input and output, similar to the Crynwr packet driver.
 * Note that transfer with the EtherTwist+ must be on word boundaries. 
 */
void
hpp_io_block_input(ns, count, buf, ring_offset)
	struct nssoftc *ns;
	int count;
	char *buf;
	int ring_offset;
{
	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;

	outw(ioaddr + HPP_IN_ADDR, ring_offset);

	insw(ioaddr + HP_DATAPORT, buf, count >> 1 );

	if (count & 0x01)
		  buf[count-1] = (char) inw(ioaddr + HP_DATAPORT);

}

void
hpp_mem_block_input(ns, count, buf, ring_offset)
	struct nssoftc *ns;
	int count;
	char *buf;
	int ring_offset;
{
	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;
	int option_reg = inw(ioaddr + HPP_OPTION);
	char *mem_start = (char *)phystokv(hppsoftc[ns->sc_unit].mem_start);
	
	outw(ioaddr + HPP_IN_ADDR, ring_offset);
	outw(ioaddr + HPP_OPTION, option_reg & ~(MemDisable + BootROMEnb));

	/* copy as much as we can straight through */
	bcopy16(mem_start, buf, count & ~1);

	/* Now we copy that last byte. */
	if (count & 0x01) {
	  	u_short savebyte[2];

	  	bcopy16(mem_start + (count & ~1), savebyte, 2);
	 	buf[count-1] = savebyte[0];
	}

	outw(ioaddr + HPP_OPTION, option_reg);
}


/*
 * output data into NIC buffers.
 *
 * NOTE: All transfers must be on word boundaries.
 */
int
hpp_io_block_output(ns, count, buf, start_page)
	struct nssoftc *ns;
	int count;
	char *buf;
	int start_page;
{
	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;

	outw(ioaddr + HPP_OUT_ADDR, start_page << 8) ;

	if (count > 1) {
		outsw(ioaddr + HP_DATAPORT, buf, count >> 1);
	}

	if ( (count & 1) == 1 ) {
	  	u_char savebyte[2];

		savebyte[1] = 0;
		savebyte[0] = buf[count - 1];
		outw(ioaddr + HP_DATAPORT, *(u_short *)savebyte); 
	}

	if (count < (ETHERMIN + sizeof( struct ether_header )))
		count = ETHERMIN + sizeof( struct ether_header );


	return (count) ;
}


/* XXX
 *
 * I take great pains to not try and bcopy past the end of the buffer,
 * does this matter?  Are the io request buffers the exact byte size?
 */
int
hpp_mem_block_output(ns, count, buf, start_page )
	struct nssoftc *ns;
	int count;
	char *buf;
	int start_page;
{
	int ioaddr = ns->sc_port - HPP_NIC_OFFSET;
	int option_reg = inw(ioaddr + HPP_OPTION);
	struct hppsoftc *hpp = &hppsoftc[ns->sc_unit];
	char *shmem;

	outw(ioaddr + HPP_OUT_ADDR, start_page << 8);
	outw(ioaddr + HPP_OPTION, option_reg & ~(MemDisable + BootROMEnb));

	shmem = (char *)phystokv(hpp->mem_start);
	bcopy16(buf, shmem, count & ~1);

	if ( (count & 1) == 1 ) {
	  	u_char savebyte[2];
		
		savebyte[1] = 0;
		savebyte[0] = buf[count - 1];
		bcopy16(savebyte, shmem + (count & ~1), 2);
	}

	while (count < ETHERMIN + sizeof(struct ether_header)) {
		*(shmem + count) = 0;
		count++;
	}

	outw(ioaddr + HPP_OPTION, option_reg);

	return count;
}


int
hppintr(unit)
	int unit;
{
	nsintr(&hppnssoftc[unit]);
	
	return(0);
}

void
hppstart(unit)
	int unit;
{
	nsstart(&hppnssoftc[unit]);
}

int hppoutput();

int
hppoutput(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	int unit = minor(dev);
	struct bus_device *ui;

	if (unit >= NHPP || (ui = hpp_info[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_write(&hppnssoftc[unit].sc_if, hppstart, ior));
}


int
hppsetinput(dev, receive_port, priority, filter, filter_count)
	dev_t dev;
	mach_port_t receive_port;
	int priority;
	filter_t *filter;
	unsigned filter_count;
{
	int unit = minor(dev);
	struct bus_device *ui;

	if (unit >= NHPP || (ui = hpp_info[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_set_filter(&hppnssoftc[unit].sc_if, receive_port,
			       priority, filter, filter_count));
}


int
hppgetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned *count;
{
	int unit = minor(dev);
	struct bus_device *ui;

	if (unit >= NHPP || (ui = hpp_info[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_getstat(&hppnssoftc[unit].sc_if, flavor, status, count));
}


int
hppsetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned count;
{
	int unit = minor(dev), oflags, s;
	struct bus_device *ui;
	struct ifnet *ifp;
	struct net_status *ns;

	if (unit >= NHPP || (ui = hpp_info[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	ifp = &hppnssoftc[unit].sc_if;

	switch (flavor) {

	case NET_STATUS:
		if (count < NET_STATUS_COUNT)
			return (D_INVALID_SIZE);
		ns = (struct net_status *)status;
		oflags = ifp->if_flags & (IFF_ALLMULTI|IFF_PROMISC);
		ifp->if_flags &= ~(IFF_ALLMULTI|IFF_PROMISC);
		ifp->if_flags |= ns->flags & (IFF_ALLMULTI|IFF_PROMISC);
		if ((ifp->if_flags & (IFF_ALLMULTI|IFF_PROMISC)) != oflags) {
			s = splimp();
			nsinit(&hppnssoftc[unit]);
			splx(s);
		}
		break;

	default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

/*
 * Watchdog.
 * Check for hung transmissions.
 */
void
hppwatch()
{
	int unit, s;
	struct nssoftc *ns;

	timeout(hppwatch, 0, hz);

	s = splimp();
	for (unit = 0; unit < NHPP; unit++) {
		if (hpp_info[unit] == 0 || hpp_info[unit]->alive == 0)
			continue;
		ns = &hppnssoftc[unit];
		if (ns->sc_timer && --ns->sc_timer == 0) {
			printf("hpp%d: transmission timeout\n", unit);
			(*ns->sc_reset)(ns);
			nsinit(ns);
		}
	}
	splx(s);
}


#endif /* NHPP > 0 */


