/*
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989 Carnegie Mellon University
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
 *	Parallel port network driver v1.1
 *	All rights reserved.
 */ 

/*
 Subject: parallel network interface

 The printer network driver has the following hardware requirements for the
 interconnection cable:

 Connections:
 Side1		Side2		Function	Side1	 / Side2
 Pin 5		Pin 10	Interrupt strobe: send status (w)/send status (r)
 Pin 2		Pin 15	Data bits	:	write	 / read
 Pin 3		Pin 13	Data bits	:	write	 / read
 Pin 4		Pin 12	Data bits	:	write	 / read
 Pin 6		Pin 11	Data bits	:	write	 / read
 Pin 10		Pin 5
 Pin 11		Pin 6
 Pin 12		Pin 4
 Pin 13		Pin 3
 Pin 15		Pin 2
 Pins 18-25	Pins 18-25 (ground interconnections)

 The cable is "symmetric" in that either side can be plugged into either of the
 computers.  

 The hardware requirements are as follows:
 Port 0x378 must be writable with the following specifications:
 	Bit 4 -> pin 6
	Bit 3 -> pin 5
	Bit 2 -> pin 4
	Bit 1 -> pin 3
	Bit 0 -> pin 2
 Port 0x379 must be readable with the following specifications:
	Bit 7 <- pin 11
	Bit 6 <- pin 10
	Bit 5 <- pin 12
	Bit 4 <- pin 13
	Bit 3 <- pin 15
 Port 0x37a must be readable and writable with the following specifications:
	Bit 4 -> interrupt enable
 So Port 0x378 connects to  Port 0x379 as
	Bit 3 -> pin 5	:  pin 10 -> Bit 6	0x08 -> 0x40

 	Bit 4 -> pin 6	:  pin 11 -> Bit 7	0x08<<1 -> ~ 0x80
	Bit 2 -> pin 4	:  pin 12 -> Bit 5	0x07 -> 0x38
	Bit 1 -> pin 3	:  pin 13 -> Bit 4	0x07 -> 0x38
	Bit 0 -> pin 2	:  pin 15 -> Bit 3	0x07 -> 0x38
 [note: bit 0 is considered the least significant bit, pins on the connector
 are numbered starting with 1, -> represents sending data out on the bus, <-
 represents reading data from the bus]

 Pins 1,7,8,9, and 16 are currently unused, and may be allowed to "float".

 The data is sent in 4 bit "nybbles", with the highest 4 bits being sent first.

 To bring up the interface, all that should be required is 
 	ifconfig par0 <your ip address> <connected machine's ip address> up
 and to bring down the interface
 	ifconfig par0 down
 You may get a warning message (such as printer out of paper) once you down
 the interface, as the port is monitored for both printer and network activity
 depending on whether par0 is up or down, and when you down the interface the
 printer driver will then read whatever is on the port (which will be the last
 message from the other computer).
 */

#include <par.h>
#if NPAR > 0

#include	<kern/time_out.h>
#include	<device/device_types.h>
#include	<device/errno.h>
#include	<device/io_req.h>
#include	<device/if_hdr.h>
#include	<device/if_ether.h>
#include	<device/net_status.h>
#include	<device/net_io.h>

#include <i386/ipl.h>
#include <i386/pio.h>
#include <chips/busses.h>
#include <i386at/if_par.h>


int parintr();
int parioctl();
int parattach();
int paroutput();

int (*oldvect)();
int oldunit;

extern struct bus_device *lprinfo[];

int par_watch = 0;

struct par_softc {
	struct ifnet ds_if;
	u_char	ds_addr[6];		/* Ethernet hardware address */
	u_char	address[6];
	char	sc_buf[PARMTU+sizeof(struct ifnet *)];
} par_softc[NPAR];

void parintoff(unit)
int unit;
{
struct bus_device *lpdev = lprinfo[unit];

	outb(INTR(lpdev->address), 0x07);
	par_softc[unit].ds_if.if_flags &= ~IFF_RUNNING;
	ivect[lpdev->sysdep1] = oldvect;
	iunit[lpdev->sysdep1] = oldunit;
}

void parinit(unit)
int unit;
{
struct bus_device *lpdev = lprinfo[unit];

	if (ivect[lpdev->sysdep1] != parintr) {
		oldvect = ivect[lpdev->sysdep1];
		oldunit = iunit[lpdev->sysdep1];
		ivect[lpdev->sysdep1] = parintr;
		iunit[lpdev->sysdep1] = unit;
	}
	outb(INTR(lpdev->address),0x11);
	par_softc[unit].ds_if.if_flags |= IFF_RUNNING;
	*(struct ifnet **)par_softc[unit].sc_buf = &par_softc[unit].ds_if;
}

struct ether_header	par_eh;

int parattach(dev)
struct bus_device *dev;
{
	u_char		unit = (u_char)dev->unit;
	struct ifnet	*ifp;
	struct par_softc*sp;

	if ((unit < 0) || (unit >= NPAR))
		return(0);
	printf("\n  par%d: at lpr%d, port = %x, spl = %d, pic = %d. ",
		unit, unit, dev->address, dev->sysdep, dev->sysdep1);

	sp = &par_softc[unit];
	ifp = &(sp->ds_if);

	*(sp->ds_addr)      = *(sp->address)     = 0x11;
	*(sp->ds_addr + 1)  = *(sp->address + 1) = 0x22;
	*(sp->ds_addr + 2)  = *(sp->address + 2) = 0x33;
	*(sp->ds_addr + 3)  = *(sp->address + 3) = 0x44;
	*(sp->ds_addr + 4)  = *(sp->address + 4) = 0x55;
	*(sp->ds_addr + 5)  = *(sp->address + 5) = 0x66;

	par_eh.ether_dhost[5] = par_eh.ether_shost[0] = 0x11;
	par_eh.ether_dhost[4] = par_eh.ether_shost[1] = 0x22;
	par_eh.ether_dhost[3] = par_eh.ether_shost[2] = 0x33;
	par_eh.ether_dhost[2] = par_eh.ether_shost[3] = 0x44;
	par_eh.ether_dhost[1] = par_eh.ether_shost[4] = 0x55;
	par_eh.ether_dhost[0] = par_eh.ether_shost[5] = 0x66;
	par_eh.ether_type = htons(0x0800);

	printf("ethernet id [%x:%x:%x:%x:%x:%x]",
		sp->address[0],sp->address[1],sp->address[2],
		sp->address[3],sp->address[4],sp->address[5]);

	ifp->if_unit = unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_header_size = sizeof(struct ether_header);
	ifp->if_header_format = HDR_ETHERNET;
	ifp->if_address_size = 6;
	ifp->if_address = (char *)&par_softc[unit].address[0];
	if_init_queues(ifp);
	return(0);
}

int parstart();	/* forward */

/*ARGSUSED*/
paropen(dev, flag)
	dev_t	dev;
	int	flag;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NPAR)
	    return (ENXIO);

	par_softc[unit].ds_if.if_flags |= IFF_UP;
	parinit(unit);
	return(0);
}

paroutput(dev, ior)
	dev_t		dev;
	io_req_t	ior;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NPAR)
	    return (ENXIO);
	return (net_write(&par_softc[unit].ds_if, parstart, ior));
}

parsetinput(dev, receive_port, priority, filter, filter_count)
	dev_t		dev;
	mach_port_t	receive_port;
	int		priority;
	filter_t	filter[];
	unsigned int	filter_count;
{
	register int unit = minor(dev);

	if (unit < 0 || unit >= NPAR)
	    return (ENXIO);

	return (net_set_filter(&par_softc[unit].ds_if,
			receive_port, priority,
			filter, filter_count));
}

int parstart(unit)
{
	struct ifnet	*ifp = &(par_softc[unit].ds_if);
	u_short		addr = lprinfo[unit]->address;
	struct sockaddr	*dst;
	int		len, i;
	spl_t		s;
	u_char		*mcp, c;
	io_req_t	m;

	if (!(ifp->if_flags & IFF_RUNNING)) {
#ifdef	WHY
		m_free(m);
		parintoff(unit);
		return(ENETDOWN);
#else	WHY
		parintoff(unit);
		return(-1);
#endif	WHY
	}
	s = SPLNET();

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0) {
		splx(s);
		return 0;
	}
	len = m->io_count;
	if (par_watch)
		printf("O%d\n",len);
	len -= 14 /* XXX */;
	mcp = (u_char *)m->io_data + 14 /* XXX */;
	while (len--) {
		c=*mcp++;
		outb(OUTPUT(addr),((c&0x80)>>3) | ((c&0x70)>>4) | 0x08);
		i=MAXSPIN;
		while (!(inb(INPUT(addr))&0x40) && --i);
		outb(OUTPUT(addr),((c&0x08)<<1) | (c&0x07));
		i=MAXSPIN;
		while ((inb(INPUT(addr))&0x40) && --i);
	}
	outb(OUTPUT(addr),(((c&0x08)<<1) | (c&0x07))^0x17);
	iodone(m);
	splx(s);
	return (0);
}

/*ARGSUSED*/
pargetstat(dev, flavor, status, count)
	dev_t		dev;
	int		flavor;
	dev_status_t	status;		/* pointer to OUT array */
	unsigned int	*count;		/* out */
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NPAR)
	    return (ENXIO);


	switch (flavor) {
	    case NET_DSTADDR:
	    	return (D_SUCCESS);
		break;
	}

	return (net_getstat(&par_softc[unit].ds_if,
			    flavor,
			    status,
			    count));
}

parsetstat(dev, flavor, status, count)
	dev_t		dev;
	int		flavor;
	dev_status_t	status;
	unsigned int	count;
{
	register int	unit = minor(dev);
	register struct par_softc *sp;

	if (unit < 0 || unit >= NPAR)
	    return (ENXIO);

	sp = &par_softc[unit];

	switch (flavor) {
	    case NET_STATUS:
	    {
		/*
		 * All we can change are flags, and not many of those.
		 */
		register struct net_status *ns = (struct net_status *)status;
		int	mode = 0;

		if (count < NET_STATUS_COUNT)
		    return (D_INVALID_SIZE);

#if	0
		/* ha ha ha */
		if (ns->flags & IFF_ALLMULTI)
		    mode |= MOD_ENAL;
		if (ns->flags & IFF_PROMISC)
		    mode |= MOD_PROM;
		/*
		 * Force a complete reset if the receive mode changes
		 * so that these take effect immediately.
		 */
		if (sp->mode != mode) {
		    sp->mode = mode;
		    if (sp->flags & DSF_RUNNING) {
			sp->flags &= ~(DSF_LOCK | DSF_RUNNING);
			parinit(unit);
		    }
		}
#endif
		break;
	    }
	    case NET_ADDRESS:
	    {
		register union ether_cvt {
		    char	addr[6];
		    int		lwd[2];
		} *ec = (union ether_cvt *)status;

		if (count < sizeof(*ec)/sizeof(int))
		    return (D_INVALID_SIZE);

		ec->lwd[0] = ntohl(ec->lwd[0]);
		ec->lwd[1] = ntohl(ec->lwd[1]);
/*   		at3c501seteh(sp->base, ec->addr);*/
		break;
	    }

	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

int parintr(unit)
int unit;
{
	register struct par_softc *sp = &par_softc[unit];
	u_short		addr = lprinfo[unit]->address;
	char		*trav = sp->sc_buf;
	short		len = 0;
	u_char 		c, c2;
	int		i;
	ipc_kmsg_t		new_kmsg;
	struct ether_header	*ehp;
	struct packet_header	*pkt;
	struct ifnet *ifp = &(sp->ds_if);

	do {
		c2=inb(INPUT(addr));
		outb(OUTPUT(addr),0x08);
		i=MAXSPIN;
		while(((c=inb(INPUT(addr)))&0x40) && --i);

		c = inb(INPUT(addr));
		outb(OUTPUT(addr),0x00);
		if (!i)
			break;

		if (++len > ETHERMTU) {
			trav = sp->sc_buf;
			len = 0;
			continue;
		}
		*trav++ = ((~c2)&0x80) | ((c2&0x38)<<1) | (((~c)&0x80)>>4) | ((c&0x38)>>3);
		i=MAXSPIN;
		while (!((c2=inb(INPUT(addr)))&0x40) && --i)
			if (((c2^0xb8)&0xf8) == (c&0xf8))
		goto end;
	} while (i);
end:
	if (len < 20)		/* line noise ? */
		return;
	if (par_watch)
		printf("I%d\n",len);

	new_kmsg = net_kmsg_get();
	if (new_kmsg == IKM_NULL) {
		/*
	   	 * Drop the packet.
		*/
		sp->ds_if.if_rcvdrops++;    
		return;
	}
	ehp = (struct ether_header *) (&net_kmsg(new_kmsg)->header[0]);
	pkt = (struct packet_header *) (&net_kmsg(new_kmsg)->packet[0]);
	*ehp = par_eh;

	bcopy (sp->sc_buf, (char *) (pkt + 1), len);

	pkt->type = ehp->ether_type;
	pkt->length = len + sizeof(struct packet_header);
  	/*
   	 * Hand the packet to the network module.
   	 */
	net_packet(ifp, new_kmsg, pkt->length,
		   ethernet_priority(new_kmsg));
	return(0);
}
#endif
