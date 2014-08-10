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
#if NWD > 0
/*
 * Driver for SMC/Western Digital Ethernet adaptors.
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

#define WD_START_PG	0x00	/* first page of TX buffer */
#define WD03_STOP_PG	0x20	/* last page +1 of RX ring */
#define WD13_STOP_PG	0x40	/* last page +1 of RX ring */

#define WD_CMDREG	0	/* offset of ASIC command register */
#define  WD_RESET	0x80	/* board reset in WDTRA_CMDREG */
#define  WD_MEMEN	0x40	/* enable shared memory */
#define WD_CMDREG5	5	/* offset of 16-bit-only ASIC register 5 */
#define  ISA16		0x80	/* enable 16 bit access from the ISA bus */
#define  NIC16		0x40	/* enable 16 bit access from the 8390 */
#define WD_NIC_OFF	16	/* NIC register offset */

#define wdunit(dev)	minor(dev)

/*
 * Autoconfiguration stuff.
 */
int	wdprobe();
void	wdattach();
int	wdstd[] = { 0x300, 0x280, 0x380, 0x240, 0 };
struct	bus_device *wdinfo[NWD];
struct	bus_driver wddriver = {
	wdprobe, 0, wdattach, 0, wdstd, "wd", wdinfo, 0, 0, 0
};

/*
 * NS8390 state.
 */
struct	nssoftc wdnssoftc[NWD];

/*
 * Board state.
 */
struct wdsoftc {
	int	sc_mstart;	/* start of board's RAM */
	int	sc_mend;	/* end of board's RAM */
	int	sc_rmstart;	/* start of receive RAM */
	int	sc_rmend;	/* end of receive RAM */
	int	sc_reg0;	/* copy of register 0 of ASIC */
	int	sc_reg5;	/* copy of register 5 of ASIC */
} wdsoftc[NWD];

void	wdstart(int);
void	wd_reset(struct nssoftc *sc);
void	wd_input(struct nssoftc *sc, int, char *, int);
int	wd_output(struct nssoftc *sc, int, char *, int);

/*
 * Watchdog.
 */
int	wdwstart = 0;
void	wdwatch(void);

#define WDDEBUG
#ifdef WDDEBUG
int	wddebug = 0;
#define DEBUGF(stmt)	{ if (wddebug) stmt; }
#else
#define DEBUGF(stmt)
#endif

/*
 * Probe for the WD8003 and WD8013.
 * These cards have the station address PROM at I/O ports <base>+8
 * to <base>+13, with a checksum following.  A Soundblaster can have
 * the same checksum as a WD ethercard, so we have an extra exclusionary
 * check for it.
 */
int
wdprobe(xxx, ui)
	int xxx;
	struct bus_device *ui;
{
	int *port;

	if (ui->unit >= NWD) {
		printf("wd%d: not configured\n", ui->unit);
		return (0);
	}
	for (port = wdstd; *port; port++) {
		if (*port < 0)
			continue;
		if (inb(*port + 8) != 0xff
		    && inb(*port + 9) != 0xff
		    && wdprobe1(*port, ui)) {
			ui->address = *port;
			*port = -1;
			return (1);
		}
	}
	return (0);
}

int
wdprobe1(port, ui)
	int port;
	struct bus_device *ui;
{

	int i, irq = 0, checksum = 0, ancient = 0, word16 = 0;
	struct wdsoftc *wd = &wdsoftc[ui->unit];
	struct nssoftc *ns = &wdnssoftc[ui->unit];
	struct ifnet *ifp = &ns->sc_if;

	for (i = 0; i < 8; i++)
		checksum += inb(port + 8 + i);
	if ((checksum & 0xff) != 0xff)
		return (0);

	printf("wd%d: WD80x3 at 0x%03x, ", ui->unit, port);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i == 0)
			printf("%02x", ns->sc_addr[i] = inb(port + 8 + i));
		else
			printf(":%02x", ns->sc_addr[i] = inb(port + 8 + i));
	}
	/*
	 * Check for PureData.
	 */
	if (inb(port) == 'P' && inb(port + 1) == 'D') {
		u_char reg5 = inb(port + 5);

		switch (inb(port + 2)) {

		case 0x03:
		case 0x05:
			word16 = 0;
			break;

		case 0x0a:
			word16 = 1;
			break;

		default:
			word16 = 0;
			break;
		}
		wd->sc_mstart = ((reg5 & 0x1c) + 0xc0) << 12;
		irq = (reg5 & 0xe0) == 0xe0 ? 10 : (reg5 >> 5) + 1;
	} else {
		/*
		 * Check for 8 bit vs 16 bit card.
		 */
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			if (inb(port + i) != inb(port + 8 + i))
				break;
		if (i >= ETHER_ADDR_LEN) {
			ancient = 1;
			word16 = 0;
		} else {
			int tmp = inb(port + 1);

			/*
			 * Attempt to clear 16bit bit.
			 */
			outb(port + 1, tmp ^ 0x01);
			if (((inb(port + 1) & 0x01) == 0x01)	/* 16 bit */
			    && (tmp & 0x01) == 0x01) {	/* in 16 bit slot */
				int asic_reg5 = inb(port + WD_CMDREG5);

				/*
				 * Magic to set ASIC to word-wide mode.
				 */
				outb(port+WD_CMDREG5, NIC16|(asic_reg5&0x1f));
				outb(port + 1, tmp);
				word16 = 1;
			} else
				word16 = 0;
			outb(port + 1, tmp);
		}
		if (!ancient && (inb(port + 1) & 0x01) != (word16 & 0x01))
			printf("\nwd%d: bus width conflict, "
			       "%d (probe) != %d (reg report)", ui->unit,
			       word16 ? 16 : 8, (inb(port+1) & 0x01) ? 16 : 8);
	}
	/*
	 * Determine board's RAM location.
	 */
	if (wd->sc_mstart == 0) {
		int reg0 = inb(port);

		if (reg0 == 0xff || reg0 == 0)
			wd->sc_mstart = 0xd0000;
		else {
			int high_addr_bits = inb(port + WD_CMDREG5) & 0x1f;

			if (high_addr_bits == 0x1f || word16 == 0)
				high_addr_bits = 0x01;
			wd->sc_mstart = ((reg0&0x3f)<<13)+(high_addr_bits<<19);
		}
	}
	/*
	 * Determine irq.
	 */
	if (irq == 0) {
		int irqmap[] = { 9, 3, 5, 7, 10, 11, 15, 4 };
		int reg1 = inb(port + 1);
		int reg4 = inb(port + 4);

		/*
		 * For old card, irq must be supplied.
		 */
		if (ancient || reg1 == 0xff) {
			if (ui->sysdep1 == 0) {
				printf("\nwd%d: must specify IRQ for card\n",
				       ui->unit);
				return (0);
			}
			irq = ui->sysdep1;
		} else {
			DEBUGF({
				int i = ((reg4 >> 5) & 0x03) + (reg1 & 0x04);

				printf("\nwd%d: irq index %d\n", ui->unit, i);
				printf("wd%d:", ui->unit);
			})
			irq = irqmap[((reg4 >> 5) & 0x03) + (reg1 & 0x04)];
		}
	} else if (irq == 2)
		irq = 9;
	ui->sysdep1 = irq;
	take_dev_irq(ui);
	printf(", irq %d", irq);

	/*
	 * Initialize 8390 state.
	 */
	ns->sc_name = ui->name;
	ns->sc_unit = ui->unit;
	ns->sc_port = port + WD_NIC_OFF;
	ns->sc_reset = wd_reset;
	ns->sc_input = wd_input;
	ns->sc_output = wd_output;
	ns->sc_pingpong = 1;
	ns->sc_word16 = word16;
	ns->sc_txstrtpg = WD_START_PG;
	ns->sc_rxstrtpg = WD_START_PG + TX_PAGES(ns);
	ns->sc_stoppg = word16 ? WD13_STOP_PG : WD03_STOP_PG;

	wd->sc_rmstart = wd->sc_mstart + TX_PAGES(ns) * 256;
	wd->sc_mend = wd->sc_rmend
		= wd->sc_mstart + (ns->sc_stoppg - WD_START_PG) * 256;
	printf(", memory 0x%05x-0x%05x", wd->sc_mstart, wd->sc_mend);

	if (word16)
		printf(", 16 bit");
	printf("\n");

	DEBUGF(printf("wd%d: txstrtpg %d rxstrtpg %d num_pages %d\n",
		      ui->unit, ns->sc_txstrtpg, ns->sc_rxstrtpg,
		      (wd->sc_mend - wd->sc_mstart) / 256));

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
wdattach(ui)
	struct bus_device *ui;
{
	/*
	 * void
	 */
}

int
wdopen(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = wdunit(dev), s;
	struct bus_device *ui;
	struct wdsoftc *wd;
	struct nssoftc *ns;

	if (unit >= NWD || (ui = wdinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	/*
	 * Start watchdog.
	 */
	if (!wdwstart) {
		wdwstart++;
		timeout(wdwatch, 0, hz);
	}
	wd = &wdsoftc[unit];
	ns = &wdnssoftc[unit];
	ns->sc_if.if_flags |= IFF_UP;
	s = splimp();
	wd->sc_reg0 = ((wd->sc_mstart >> 13) & 0x3f) | WD_MEMEN;
	wd->sc_reg5 = ((wd->sc_mstart >> 19) & 0x1f) | NIC16;
	if (ns->sc_word16)
		outb(ui->address + WD_CMDREG5, wd->sc_reg5);
	outb(ui->address, wd->sc_reg0);
	nsinit(ns);
	splx(s);
	return (0);
}

int
wdoutput(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	int unit = wdunit(dev);
	struct bus_device *ui;

	if (unit >= NWD || (ui = wdinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_write(&wdnssoftc[unit].sc_if, wdstart, ior));
}

int
wdsetinput(dev, receive_port, priority, filter, filter_count)
	dev_t dev;
	mach_port_t receive_port;
	int priority;
	filter_t *filter;
	unsigned filter_count;
{
	int unit = wdunit(dev);
	struct bus_device *ui;

	if (unit >= NWD || (ui = wdinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_set_filter(&wdnssoftc[unit].sc_if, receive_port,
			       priority, filter, filter_count));
}

int
wdgetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned *count;
{
	int unit = wdunit(dev);
	struct bus_device *ui;

	if (unit >= NWD || (ui = wdinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	return (net_getstat(&wdnssoftc[unit].sc_if, flavor, status, count));
}

int
wdsetstat(dev, flavor, status, count)
	dev_t dev;
	int flavor;
	dev_status_t status;
	unsigned count;
{
	int unit = wdunit(dev), oflags, s;
	struct bus_device *ui;
	struct ifnet *ifp;
	struct net_status *ns;

	if (unit >= NWD || (ui = wdinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	ifp = &wdnssoftc[unit].sc_if;

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
			nsinit(&wdnssoftc[unit]);
			splx(s);
		}
		break;

	default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

void
wdintr(unit)
	int unit;
{
	nsintr(&wdnssoftc[unit]);
}

void
wdstart(unit)
	int unit;
{
	nsstart(&wdnssoftc[unit]);
}

void
wd_reset(ns)
	struct nssoftc *ns;
{
	int port = ns->sc_port - WD_NIC_OFF;	/* ASIC base address */
	struct wdsoftc *wd = &wdsoftc[ns->sc_unit];

	outb(port, WD_RESET);
	outb(0x80, 0);		/* I/O delay */
	/*
	 * Set up the ASIC registers, just in case something changed them.
	 */
	outb(port, ((wd->sc_mstart >> 13) & 0x3f) | WD_MEMEN);
	if (ns->sc_word16)
		outb(port + WD_CMDREG5, NIC16 | ((wd->sc_mstart>>19) & 0x1f));
}

void
wd_input(ns, count, buf, ring_offset)
	struct nssoftc *ns;
	int count;
	char *buf;
	int ring_offset;
{
	int port = ns->sc_port - WD_NIC_OFF;
	int xfer_start;
	struct wdsoftc *wd = &wdsoftc[ns->sc_unit];

	DEBUGF(printf("wd%d: ring_offset = %d\n", ns->sc_unit, ring_offset));

	xfer_start = wd->sc_mstart + ring_offset - (WD_START_PG << 8);

	/*
	 * The NIC driver calls us 3 times.  Once to read the NIC 4 byte
	 * header, next to read the Ethernet header and finally to read
	 * the actual data.  We enable 16 bit mode before the NIC header
	 * and disable it after the packet body.
	 */
	if (count == 4) {
		if (ns->sc_word16)
			outb(port + WD_CMDREG5, ISA16 | wd->sc_reg5);
		((int *)buf)[0] = ((int *)phystokv(xfer_start))[0];
		return;
	}
	if (count == sizeof(struct ether_header)) {
		xfer_start = (int)phystokv(xfer_start);
		((int *)buf)[0] = ((int *)xfer_start)[0];
		((int *)buf)[1] = ((int *)xfer_start)[1];
		((int *)buf)[2] = ((int *)xfer_start)[2];
		((short *)(buf + 12))[0] = ((short *)(xfer_start + 12))[0];
		return;
	}
	if (xfer_start + count > wd->sc_rmend) {
		int semi_count = wd->sc_rmend - xfer_start;

		/*
		 * Input move must be wrapped.
		 */
		bcopy((char *)phystokv(xfer_start), buf, semi_count);
		count -= semi_count;
		bcopy((char *)phystokv(wd->sc_rmstart),buf+semi_count,count);
	} else
		bcopy((char *)phystokv(xfer_start), buf, count);
	if (ns->sc_word16)
		outb(port + WD_CMDREG5, wd->sc_reg5);
}

int
wd_output(ns, count, buf, start_page)
	struct nssoftc *ns;
	int count;
	char *buf;
	int start_page;
{
	char *shmem;
	int i, port = ns->sc_port - WD_NIC_OFF;
	struct wdsoftc *wd = &wdsoftc[ns->sc_unit];

	DEBUGF(printf("wd%d: start_page = %d\n", ns->sc_unit, start_page));

	shmem = (char *)phystokv(wd->sc_mstart+((start_page-WD_START_PG)<<8));
	if (ns->sc_word16) {
		outb(port + WD_CMDREG5, ISA16 | wd->sc_reg5);
		bcopy(buf, shmem, count);
		outb(port + WD_CMDREG5, wd->sc_reg5);
	} else
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
wdwatch()
{
	int unit, s;
	struct nssoftc *ns;

	timeout(wdwatch, 0, hz);

	s = splimp();
	for (unit = 0; unit < NWD; unit++) {
		if (wdinfo[unit] == 0 || wdinfo[unit]->alive == 0)
			continue;
		ns = &wdnssoftc[unit];
		if (ns->sc_timer && --ns->sc_timer == 0) {
			printf("wd%d: transmission timeout\n", unit);
			nsinit(ns);
		}
	}
	splx(s);
}

#endif /* NWD > 0 */
