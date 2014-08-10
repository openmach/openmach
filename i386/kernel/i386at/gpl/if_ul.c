/* 
 * Copyright (c) 1994 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/*
 * Written 1993 by Donald Becker.
 * 
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.	 This software may be used and
 * distributed according to the terms of the GNU Public License,
 * incorporated herein by reference.
 *
 * The Author may be reached as becker@super.org or
 * C/O Supercomputing Research Ctr., 17100 Science Dr., Bowie MD 20715
 */

#include <wd.h>
#include <ul.h>
#if NUL > 0
/*
 * Driver for SMC Ultra ethernet adaptor.
 * Derived from the Linux driver by Donald Becker.
 *
 * Shantanu Goel (goel@cs.columbia.edu)
 */
#include <mach/sa/sys/types.h>
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
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/gpl/if_nsreg.h>

#define START_PG	0x00	/* first page of TX buffer */
#define ULTRA_CMDREG	0	/* offset of ASIC command register */
#define  ULTRA_RESET	0x80	/* board reset in ULTRA_CMDREG */
#define  ULTRA_MEMEN	0x40	/* enable shared memory */
#define ULTRA_NIC_OFF	16	/* NIC register offset */

#define ulunit(dev)	minor(dev)

/*
 * Autoconfiguration stuff.
 */
int	ulprobe();
void	ulattach();
int	ulstd[] = { 0x200, 0x220, 0x240, 0x280, 0x300, 0x340, 0x380, 0 };
struct	bus_device *ulinfo[NUL];
struct	bus_driver uldriver = {
	ulprobe, 0, ulattach, 0, ulstd, "ul", ulinfo, 0, 0, 0
};

/*
 * NS8390 state.
 */
struct	nssoftc ulnssoftc[NUL];

/*
 * Ultra state.
 */
struct ulsoftc {
	int	sc_mstart;	/* start of board's RAM */
	int	sc_mend;	/* end of board's RAM */
	int	sc_rmstart;	/* start of receive RAM */
	int	sc_rmend;	/* end of receive RAM */
} ulsoftc[NUL];

void	ulstart(int);
void	ul_reset(struct nssoftc *sc);
void	ul_input(struct nssoftc *sc, int, char *, int);
int	ul_output(struct nssoftc *sc, int, char *, int);

/*
 * Watchdog.
 */
int	ulwstart = 0;
void	ulwatch(void);

#define ULDEBUG
#ifdef ULDEBUG
int	uldebug = 0;
#define DEBUGF(stmt)	{ if (uldebug) stmt; }
#else
#define DEBUGF(stmt)
#endif

/*
 * Probe for the Ultra.
 * This looks like an 8013 with the station address PROM
 * at I/O ports <base>+8 to <base>+13, with a checksum following.
 */
int
ulprobe(xxx, ui)
	int xxx;
	struct bus_device *ui;
{
	int *port;

	if (ui->unit >= NUL) {
		printf("ul%d: not configured\n", ui->unit);
		return (0);
	}
	for (port = ulstd; *port; port++) {
		if (*port < 0)
			continue;
		/*
		 * Check chip ID nibble.
		 */
		if ((inb(*port + 7) & 0xf0) != 0x20)
			continue;
		if (ulprobe1(*port, ui)) {
			ui->address = *port;
			*port = -1;
#if NWD > 0
			/*
			 * XXX: The Western Digital/SMC driver can sometimes
			 * probe the Ultra incorrectly.  Remove the Ultra's
			 * port from it's list to avoid the problem.
			 */
			{
				int i;
				extern int wdstd[];

				for (i = 0; wdstd[i]; i++) {
					if (wdstd[i] == ui->address) {
						wdstd[i] = -1;
						break;
					}
				}
			}
#endif
			return (1);
		}
	}
	return (0);
}

int
ulprobe1(port, ui)
	int port;
	struct bus_device *ui;
{
	u_char num_pages, irqreg, addr, reg4;
	u_char irqmap[] = { 0, 9, 3, 5, 7, 10, 11, 15 };
	short num_pages_tbl[4] = { 0x20, 0x40, 0x80, 0xff };
	int i, irq, checksum = 0;
	int addr_tbl[4] = { 0x0c0000, 0x0e0000, 0xfc0000, 0xfe0000 };
	struct ulsoftc *ul = &ulsoftc[ui->unit];
	struct nssoftc *ns = &ulnssoftc[ui->unit];
	struct ifnet *ifp = &ns->sc_if;

	/*
	 * Select the station address register set.
	 */
	reg4 = inb(port + 4) & 0x7f;
	outb(port + 4, reg4);

	for (i = 0; i < 8; i++)
		checksum += inb(port + 8 + i);
	if ((checksum & 0xff) != 0xff)
		return (0);

	/*
	 * Use 2 transmit buffers.
	 */
	ns->sc_pingpong = 1;

	printf("ul%d: SMC Ultra at 0x%03x, ", ui->unit, port);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i == 0)
			printf("%02x", ns->sc_addr[i] = inb(port + 8 + i));
		else
			printf(":%02x", ns->sc_addr[i] = inb(port + 8 + i));
	}
	/*
	 * Switch from station address to alternate register set
	 * and read useful registers there.
	 */
	outb(port + 4, 0x80 | reg4);

	/*
	 * Enable FINE16 mode to avoid BIOS ROM width mismatches
	 * during reboot.
	 */
	outb(port + 0x0c, 0x80 | inb(port + 0x0c));
	irqreg = inb(port + 0x0d);
	addr = inb(port + 0x0b);

	/*
	 * Switch back to station address register set so the MSDOG
	 * driver can find the card after a warm boot.
	 */
	outb(port + 4, reg4);

	/*
	 * Determine IRQ.  The IRQ bits are split.
	 */
	irq = irqmap[((irqreg & 0x40) >> 4) + ((irqreg & 0x0c) >> 2)];
	if (irq == 0) {
		printf(", failed to detect IRQ line.\n");
		return (0);
	}
	ui->sysdep1 = irq;
	take_dev_irq(ui);
	printf(", irq %d", irq);

	/*
	 * Determine board's RAM location.
	 */
	ul->sc_mstart = ((addr & 0x0f) << 13) + addr_tbl[(addr >> 6) & 3];
	num_pages = num_pages_tbl[(addr >> 4) & 3];
	ul->sc_rmstart = ul->sc_mstart + TX_PAGES(ns) * 256;
	ul->sc_mend = ul->sc_rmend
		= ul->sc_mstart + (num_pages - START_PG) * 256;
	printf(", memory 0x%05x-0x%05x\n", ul->sc_mstart, ul->sc_mend);

	/*
	 * Initialize 8390 state.
	 */
	ns->sc_name = ui->name;
	ns->sc_unit = ui->unit;
	ns->sc_port = port + ULTRA_NIC_OFF;
	ns->sc_word16 = 1;
	ns->sc_txstrtpg = START_PG;
	ns->sc_rxstrtpg = START_PG + TX_PAGES(ns);
	ns->sc_stoppg = num_pages;
	ns->sc_reset = ul_reset;
	ns->sc_input = ul_input;
	ns->sc_output = ul_output;

	DEBUGF(printf("ul%d: txstrtpg %d rxstrtpg %d num_pages %d\n",
		      ui->unit, ns->sc_txstrtpg, ns->sc_rxstrtpg, num_pages));

	/*
	 * Initialize interface header.
	 */
	ifp->if_unit = ui->unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST;
	ifp->if_header_size = sizeof(struct ether_header);
	ifp->if_header_format = HDR_ETHERNET;
	ifp->if_address_size = ETHER_ADDR_LEN;
	ifp->if_address = ns->sc_addr;
	if_init_queues(ifp);

	return (1);
}

void
ulattach(ui)
	struct bus_device *ui;
{
	/*
	 * void
	 */
}

int
ulopen(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = ulunit(dev), s;
	struct bus_device *ui;

	if (unit >= NUL || (ui = ulinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	/*
	 * Start watchdog.
	 */
	if (!ulwstart) {
		ulwstart++;
		timeout(ulwatch, 0, hz);
	}
	ulnssoftc[unit].sc_if.if_flags |= IFF_UP;
	s = splimp();
	outb(ui->address, ULTRA_MEMEN);	/* enable memory, 16 bit mode */
	outb(ui->address + 5, 0x80);
	outb(ui->address + 6, 0x01);	/* enable interrupts and memory */
	nsinit(&ulnssoftc[unit]);
	splx(s);
	return (0);
}

int
uloutput(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	int unit = ulunit(dev);
	struct bus_device *ui;

	if (unit >= NUL || (ui = ulinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_write(&ulnssoftc[unit].sc_if, ulstart, ior));
}

int
ulsetinput(dev, receive_port, priority, filter, filter_count)
	dev_t dev;
	mach_port_t receive_port;
	int priority;
	filter_t *filter;
	unsigned filter_count;
{
	int unit = ulunit(dev);
	struct bus_device *ui;

	if (unit >= NUL || (ui = ulinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_set_filter(&ulnssoftc[unit].sc_if, receive_port,
			       priority, filter, filter_count));
}

int
ulgetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned *count;
{
	int unit = ulunit(dev);
	struct bus_device *ui;

	if (unit >= NUL || (ui = ulinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_getstat(&ulnssoftc[unit].sc_if, flavor, status, count));
}

int
ulsetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned count;
{
	int unit = ulunit(dev), oflags, s;
	struct bus_device *ui;
	struct ifnet *ifp;
	struct net_status *ns;

	if (unit >= NUL || (ui = ulinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	ifp = &ulnssoftc[unit].sc_if;

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
			nsinit(&ulnssoftc[unit]);
			splx(s);
		}
		break;

	default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

void
ulintr(unit)
	int unit;
{
	nsintr(&ulnssoftc[unit]);
}

void
ulstart(unit)
	int unit;
{
	nsstart(&ulnssoftc[unit]);
}

void
ul_reset(ns)
	struct nssoftc *ns;
{
	int port = ns->sc_port - ULTRA_NIC_OFF;	/* ASIC base address */

	outb(port, ULTRA_RESET);
	outb(0x80, 0);			/* I/O delay */
	outb(port, ULTRA_MEMEN);
}

void
ul_input(ns, count, buf, ring_offset)
	struct nssoftc *ns;
	int count;
	char *buf;
	int ring_offset;
{
	int xfer_start;
	struct ulsoftc *ul = &ulsoftc[ns->sc_unit];

	DEBUGF(printf("ul%d: ring_offset = %d\n", ns->sc_unit, ring_offset));

	xfer_start = ul->sc_mstart + ring_offset - (START_PG << 8);
	if (xfer_start + count > ul->sc_rmend) {
		int semi_count = ul->sc_rmend - xfer_start;

		/*
		 * Input move must be wrapped.
		 */
		bcopy((char *)phystokv(xfer_start), buf, semi_count);
		count -= semi_count;
		bcopy((char *)phystokv(ul->sc_rmstart), buf+semi_count, count);
	} else
		bcopy((char *)phystokv(xfer_start), buf, count);
}

int
ul_output(ns, count, buf, start_page)
	struct nssoftc *ns;
	int count;
	char *buf;
	int start_page;
{
	char *shmem;
	int i;
	struct ulsoftc *ul = &ulsoftc[ns->sc_unit];

	DEBUGF(printf("ul%d: start_page = %d\n", ns->sc_unit, start_page));

	shmem = (char *)phystokv(ul->sc_mstart + ((start_page-START_PG) << 8));
	bcopy(buf, shmem, count);
	while (count <  ETHERMIN + sizeof(struct ether_header)) {
		*(shmem + count) = 0;
		count++;
	}
	return (count);
}

/*
 * Watchdog.
 * Check for hung transmissions.
 */
void
ulwatch()
{
	int unit, s;
	struct nssoftc *ns;

	timeout(ulwatch, 0, hz);

	s = splimp();
	for (unit = 0; unit < NUL; unit++) {
		if (ulinfo[unit] == 0 || ulinfo[unit]->alive == 0)
			continue;
		ns = &ulnssoftc[unit];
		if (ns->sc_timer && --ns->sc_timer == 0) {
			printf("ul%d: transmission timeout\n", unit);
			nsinit(ns);
		}
	}
	splx(s);
}

#endif /* NUL > 0 */
