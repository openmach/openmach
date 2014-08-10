#define DEBUG	1
/* 
 * Mach Operating System
 * Copyright (c) 1994,1993,1992 Carnegie Mellon University
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
 * HISTORY
 * 17-Feb-94  David Golub (dbg) at Carnegie-Mellon University
 *	Fix from Bob Baron to fix transmitter problems.
 *
 * $Log: if_de6c.c,v $
 * Revision 1.1  1994/11/08  20:47:24  baford
 * merged in CMU's MK83-MK83a diffs
 *
 * Revision 2.2  93/11/17  18:29:25  dbg
 * 	Moved source into kernel/i386at/DLINK/if_de6c.c, since we
 * 	can't release it but don't want to lose it.
 * 	[93/11/17            dbg]
 * 
 * 	Removed u_long.
 * 	[93/03/25            dbg]
 * 
 * 	Created.
 * 	I have used if_3c501.c as a typical driver template and
 * 	spliced in the appropriate particulars for the 
 * 	d-link 600.
 * 	[92/08/13            rvb]
 * 
 *
 *	File:	if_de6c.c
 *	Author: Robert V. Baron
 */

/*
 *	File:	if_3c501.c
 *	Author: Philippe Bernadat
 *	Date:	1989
 * 	Copyright (c) 1989 OSF Research Institute 
 *
 * 	3COM Etherlink d-link "600" Mach Ethernet drvier
 */
/*
  Copyright 1990 by Open Software Foundation,
Cambridge, MA.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appears in all copies and
that both the copyright notice and this permission notice appear in
supporting documentation, and that the name of OSF or Open Software
Foundation not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

  OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 *	I have tried to make it clear what is device specific code
 * and what code supports the general BSD ethernet interface. d-link
 * specific code is preceded by a line or two of 
 * "d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON"
 * and followed by a line or two of
 * "d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF"
 *
 *	The main routines that do device specific processing are:
 *	  de6cintr	- interrupt dispatcher
 *	  de6crcv	- rcv packets and switch to new buffers
 *	  de6cxmt	- xmt packet and wait for xmtbusy to clear
 *	  de6calive	- probe for device
 *	  de6cinit	- device initialization
 *	  de6cintoff	- turn it off.
 *	There are a couple of interesting macros at the head of this
 * file and some support subroutines at the end.
 *
 *	Lastly, to get decent performance on i386SX class machines, it
 * was necessary to recode the read and write d-link memory routines in
 * assembler.  The deread and dewrite routines that are used are in
 * if_de6s.s
 * 
 */

/* Questions:

	Make sure that iopl maps 378, 278 and 3bc.

	If you set command w/o MODE and page bit, what happens?

	Could I get xmt interrupts; currently I spin - this is not an issue?

	enable promiscuous?

	Can you assert TXEN and RXen simulatneously?
*/

#include <de6c.h>
#include <par.h>

#ifdef	MACH_KERNEL
#include	<kern/time_out.h>
#include	<device/device_types.h>
#include	<device/errno.h>
#include	<device/io_req.h>
#include	<device/if_hdr.h>
#include	<device/if_ether.h>
#include	<device/net_status.h>
#include	<device/net_io.h>
#include	<chips/busses.h>
#else	MACH_KERNEL
#include	<sys/param.h>
#include	<mach/machine/vm_param.h>
#include	<sys/systm.h>
#include	<sys/mbuf.h>
#include	<sys/buf.h>
#include	<sys/protosw.h>
#include	<sys/socket.h>
#include	<sys/vmmac.h>
#include	<sys/ioctl.h>
#include	<sys/errno.h>
#include	<sys/syslog.h>

#include	<net/if.h>
#include	<net/netisr.h>
#include	<net/route.h>
#include	<i386at/atbus.h>

#ifdef	INET
#include	<netinet/in.h>
#include	<netinet/in_systm.h>
#include	<netinet/in_var.h>
#include	<netinet/ip.h>
#include	<netinet/if_ether.h>
#endif

#ifdef	NS
#include	<netns/ns.h>
#include	<netns/ns_if.h>
#endif
#endif	MACH_KERNEL

#include	<vm/vm_kern.h>
#include	<i386/ipl.h>
#include	<i386/pio.h>
#include	<i386at/if_de6c.h>

#define	SPLNET	spl6

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
#define	de6cwrite(sp, addr, buf, len) \
	de6cwriteasm(addr, buf, len, DATA(sp->port), sp->latency)

#define	de6cread(sp, addr, buf, len) \
	de6creadasm(addr, buf, len, DATA(sp->port), sp->latency)

#define DATA_OUT(sp, p, f, z) \
	de6coutb(sp, DATA(p), ((z)<<4)  | f);\
	de6coutb(sp, DATA(p), ((z)&0xf0)| f | STROBE)

#define STAT_IN(sp, p, in) \
	de6coutb(sp, DATA(p), STATUS); \
	in = inb(STAT(port)); \
	de6coutb(sp, DATA(p), NUL_CMD | STROBE)

#define	XMTidx 3
#define XMT_BSY_WAIT 10000
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

int	de6cprobe();
void	de6cattach();
int	de6cintr();
int	de6cinit();
int	de6coutput();
int	de6cioctl();
int	de6creset();
void	de6cwatch();

static vm_offset_t de6c_std[NDE6C] = { 0 };
static struct bus_device *de6c_info[NDE6C];

#ifdef	MACH_KERNEL
struct	bus_driver	de6cdriver = 
	{de6cprobe, 0, de6cattach, 0, de6c_std, "de", de6c_info, };
extern struct bus_device *lprinfo[];

#define MM io_req_t
#define	PIC	sysdep1
#define DEV	bus_device
#else	MACH_KERNEL
int	(*de6cintrs[])() = {	de6cintr, 0};
struct	isa_driver	de6cdriver = 
	{de6cprobe, 0, de6cattach, "de", 0, 0, 0};
extern struct isa_dev *lprinfo[];

#define MM struct mbuf *
#define	PIC	dev_pic
#define DEV	isa_dev
#endif	MACH_KERNEL

int	watchdog_id;

typedef struct { 
#ifdef	MACH_KERNEL
	struct	ifnet	ds_if;		/* generic interface header */
	u_char	ds_addr[6];		/* Ethernet hardware address */
#else	MACH_KERNEL
	struct	arpcom	de6c_ac;
#define	ds_if	de6c_ac.ac_if
#define	ds_addr	de6c_ac.ac_enaddr
#endif	MACH_KERNEL
	int	flags;
        int	timer;
        u_char	address[6];
	short	mode;
	int	port;
	int	latency;
	int	xmt;
	int	rcv;
	int	rcvoff;
	int	rcvspin;
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	int	produce;
	int	consume;
	int	rcvlen[XMTidx];
	int	alive;
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
	int 	(*oldvect)();
	int	oldunit;
} de6c_softc_t;

de6c_softc_t	de6c_softc[NDE6C];

int de6cactive[NDE6C];

/*
 *	Patch to change latency value
 */
int de6c_latency = 30;	/* works on NEC Versa (pottsylvania.mach) */

#ifdef	DEBUG
int de6crcv0, de6crcv1, de6crcv2, de6crcv3;
int de6cdo_rcvintr = 0, de6cdo_watch = 0;
int de6cdo_xmt = 0;
#define D(X) X
#else	/* DEBUG */
#define D(X)
#endif	/* DEBUG */

/*
 * de6cprobe:
 *	We are not directly probed.  The lprattach will call de6cattach.
 *	But what we have is plausible for a probe.
 */
de6cprobe(port, dev)
struct	DEV		*dev;
{
#ifdef	MACH_KERNEL
	int		unit = dev->unit;
#else	MACH_KERNEL
	int		unit = dev->dev_unit;
#endif	MACH_KERNEL

	if ((unit < 0) || (unit >= NDE6C)) {
		return(0);
	}
	return(1);
}

/*
 * de6cattach:
 *
 *	Called from lprattach
 *
 */
void de6cattach(dev)
#ifdef	MACH_KERNEL
struct bus_device	*dev;
#else	MACH_KERNEL
struct isa_dev		*dev;
#endif	MACH_KERNEL
{
	de6c_softc_t	*sp;
	struct	ifnet	*ifp;
#ifdef	MACH_KERNEL
	int		unit = dev->unit;
	int		port = (int)dev->address;
#else	MACH_KERNEL
	int		unit = dev->dev_unit;
	int		port = (int)dev->dev_addr;
#endif	MACH_KERNEL

	sp = &de6c_softc[unit];
	sp->port = port;
	sp->timer = -1;
	sp->flags = 0;
	sp->mode = 0;

	ifp = &(sp->ds_if);
	ifp->if_unit = unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST;

#ifdef	MACH_KERNEL
	ifp->if_header_size = sizeof(struct ether_header);
	ifp->if_header_format = HDR_ETHERNET;
	ifp->if_address_size = 6;
	ifp->if_address = (char *)&sp->address[0];
	if_init_queues(ifp);
#else	MACH_KERNEL
	ifp->if_name = "de";
	ifp->if_init = de6cinit;
	ifp->if_output = de6coutput;
	ifp->if_ioctl = de6cioctl;
	ifp->if_reset = de6creset;
	ifp->if_next = NULL;
	if_attach(ifp);
#endif	MACH_KERNEL

	sp->alive = de6calive(sp);
}

de6calive(sp)
de6c_softc_t	*sp;
{
	int		port = sp->port;
	int 		unit = sp->ds_if.if_unit;
	struct DEV	*dev = lprinfo[unit];
	int		i;

#ifdef	MACH_KERNEL
#else	/* MACH_KERNEL */
	extern int tcp_recvspace;		/* empircal messure */
#endif	/* MACH_KERNEL */

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	de6coutb(sp, CMD(port), SLT_NIC);
	DATA_OUT(sp, port, COMMAND, RESET);
	DATA_OUT(sp, port, COMMAND, STOP_RESET);
	sp->latency = 101;
	if (!de6cgetid(sp, sp->ds_addr)) {
		de6coutb(sp, CMD(port), SLT_PRN);
		return 0;
	}

#ifdef	MACH_KERNEL
#else	/* MACH_KERNEL */
	tcp_recvspace = 0x300;		/* empircal messure */
#endif	/* MACH_KERNEL */

#ifdef	de6cwrite
	sp->latency = de6c_latency;
#else	/* de6cwrite */
	sp->latency = 0;
#endif	/* de6cwrite */

	for (i = 0; i++ < 10;) {
		if (de6cmemcheck(sp))
			break;
		sp->latency += 10;
	}

	de6cgetid(sp, sp->ds_addr);
	de6cgetid(sp, sp->address);
	de6csetid(sp, sp->address);
	de6coutb(sp, CMD(port), SLT_PRN);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

#ifdef	MACH_KERNEL
#if	 NPAR > 0
	printf("\n");
#endif	/* NPAR > 0 */
	printf("  de%d:  at lpr%d, port = %x, spl = %d, pic = %d. ",
		unit, unit, dev->address, dev->sysdep, dev->sysdep1);

	printf("ethernet id [%x:%x:%x:%x:%x:%x]",
		sp->address[0],sp->address[1],sp->address[2], 
		sp->address[3],sp->address[4],sp->address[5]);

	if (sp->latency > 1) {
		printf("\n");
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
	}
#else	MACH_KERNEL
	printf("de%d:  port = %x, spl = %d, pic = %d. ",
		unit, dev->dev_addr, dev->dev_spl, dev->dev_pic);

	printf("ethernet id [%x:%x:%x:%x:%x:%x]\n",
		sp->address[0],sp->address[1],sp->address[2], 
		sp->address[3],sp->address[4],sp->address[5]);

	if (sp->latency > 1) {
		printf("de%d:", unit);
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
		printf("  LATENCY = %d", sp->latency);
		printf("\n");
	}
#endif	MACH_KERNEL

	return 1;
}

/*
 * de6cwatch():
 *
 */
void de6cwatch(b_ptr)
short *b_ptr;
{
#ifdef	DEBUG_MORE
	int unit = *b_ptr;
	de6c_softc_t *sp = &de6c_softc[unit];

	if(!de6cdo_watch) return;
	de6cintr(unit);
	if (sp->ds_if.if_flags & IFF_RUNNING)
		timeout(de6cwatch, b_ptr, de6cdo_watch);
#endif	/* DEBUG_MORE */
}

#ifdef	MACH_KERNEL
void de6cstart(int);	/* forward */

de6coutput(dev, ior)
	dev_t		dev;
	io_req_t	ior;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NDE6C ||
		de6c_softc[unit].port == 0)
	    return (ENXIO);

	return (net_write(&de6c_softc[unit].ds_if, de6cstart, ior));
}

io_return_t
de6csetinput(
	dev_t		dev,
	mach_port_t	receive_port,
	int		priority,
	filter_t	filter[],
	natural_t	filter_count)
{
	register int unit = minor(dev);

	if (unit < 0 || unit >= NDE6C ||
		de6c_softc[unit].port == 0)
	    return ENXIO;

	return net_set_filter(&de6c_softc[unit].ds_if,
			receive_port, priority,
			filter, filter_count);
}

#else	MACH_KERNEL
/*
 * de6coutput:
 *
 *	This routine is called by the "if" layer to output a packet to
 *	the network.  This code resolves the local ethernet address, and
 *	puts it into the mbuf if there is room.  If not, then a new mbuf
 *	is allocated with the header information and precedes the data
 *	to be transmitted.
 *
 * input:	ifnet structure pointer, an mbuf with data, and address
 *		to be resolved
 * output:	mbuf is updated to hold enet address, or a new mbuf
 *	  	with the address is added
 *
 */
de6coutput(ifp, m0, dst)
struct ifnet	*ifp;
struct mbuf	*m0;
struct sockaddr *dst;
{
	register de6c_softc_t 		*sp = &de6c_softc[ifp->if_unit];
	int 				type, opri, error;
 	u_char				edst[6];
	struct in_addr 			idst;
	register struct mbuf 		*m = m0;
	register struct ether_header	*eh;
	register int			 off;
	int				usetrailers;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		de6cintoff(ifp->if_unit);
		error = ENETDOWN;
		goto bad;
	}
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
 		if (!arpresolve(&sp->de6c_ac, m, &idst, edst, &usetrailers)){
			return (0);	/* if not yet resolved */
		}
		off = ntohs((u_short)mtod(m, struct ip *)->ip_len) - m->m_len;

		if (usetrailers && off > 0 && (off & 0x1ff) == 0 &&
		    m->m_off >= MMINOFF + 2 * sizeof (u_short)) {
			type = ETHERTYPE_TRAIL + (off>>9);
			m->m_off -= 2 * sizeof (u_short);
			m->m_len += 2 * sizeof (u_short);
			*mtod(m, u_short *) = htons((u_short)ETHERTYPE_IP);
			*(mtod(m, u_short *) + 1) = htons((u_short)m->m_len);
			goto gottrailertype;
		}
		type = ETHERTYPE_IP;
		off = 0;
		goto gottype;
#endif
#ifdef NS
	case AF_NS:
		type = ETHERTYPE_NS;
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		(caddr_t)edst, sizeof (edst));
		off = 0;
		goto gottype;
#endif

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
 		bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
		type = eh->ether_type;
		goto gottype;

	default:
		printf("de6c%d: can't handle af%d\n", ifp->if_unit,
			dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

gottrailertype:
	/*
	 * Packet to be sent as trailer: move first packet
	 * (control information) to end of chain.
	 */
	while (m->m_next)
		m = m->m_next;
	m->m_next = m0;
	m = m0->m_next;
	m0->m_next = 0;
	m0 = m;

gottype:
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	if (m->m_off > MMAXOFF ||
	    MMINOFF + sizeof (struct ether_header) > m->m_off) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
		m->m_next = m0;
		m->m_off = MMINOFF;
		m->m_len = sizeof (struct ether_header);
	} else {
		m->m_off -= sizeof (struct ether_header);
		m->m_len += sizeof (struct ether_header);
	}
	eh = mtod(m, struct ether_header *);
	eh->ether_type = htons((u_short)type);
 	bcopy((caddr_t)edst, (caddr_t)eh->ether_dhost, sizeof (edst));
 	bcopy((caddr_t)sp->address,(caddr_t)eh->ether_shost, sizeof(edst));
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	opri = SPLNET();
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		splx(opri);
		m_freem(m);
		return (ENOBUFS);
	}
	IF_ENQUEUE(&ifp->if_snd, m);
	de6cstart(ifp->if_unit);
	splx(opri);
	return (0);

bad:
	m_freem(m0);
	return (error);
}
#endif	MACH_KERNEL

/*
 * de6creset:
 *
 *	This routine is in part an entry point for the "if" code.  Since most 
 *	of the actual initialization has already (we hope already) been done
 *	by calling de6cattach().
 *
 * input	: unit number or board number to reset
 * output	: board is reset
 *
 */
de6creset(unit)
int	unit;
{
	de6c_softc[unit].ds_if.if_flags &= ~IFF_RUNNING;
	return(de6cinit(unit));
}



/*
 * de6cinit:
 *
 *	Another routine that interfaces the "if" layer to this driver.  
 *	Simply resets the structures that are used by "upper layers".  
 *
 * input	: board number
 * output	: structures (if structs) and board are reset
 *
 */	
de6cinit(unit)
int	unit;
{
	de6c_softc_t	*sp = &de6c_softc[unit];
	struct	ifnet	*ifp = &(sp->ds_if);
	int		port = sp->port;
	int		pic = lprinfo[unit]->PIC;
	spl_t		oldpri;

#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	if (ifp->if_addrlist == (struct ifaddr *)0) {
		return;
	}
#endif	MACH_KERNEL
	oldpri = SPLNET();

	if (ivect[pic] != de6cintr) {
		sp->oldvect = ivect[pic];
			      ivect[pic] = de6cintr;
		sp->oldunit = iunit[pic];
			      iunit[pic] = unit;
	}

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	sp->consume = 0;
	sp->produce = 0;
	de6coutb(sp, CMD(port), SLT_NIC);
	DATA_OUT(sp, port, COMMAND, RESET);
	DATA_OUT(sp, port, COMMAND, STOP_RESET);
	de6coutb(sp, CMD(port), IRQEN);
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|RXEN);
	de6coutb(sp, CMD(port), SLT_PRN);
#if	0
	if (sp->mode & IFF_PROMISC) {
		/* handle promiscuous case */;
	}
#endif	0
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
	sp->ds_if.if_flags |= IFF_RUNNING;
	sp->flags |= DSF_RUNNING;
	sp->timer = 5;
	timeout(de6cwatch, &(ifp->if_unit), 3);
	de6cstart(unit);
	splx(oldpri);
}

#ifdef	MACH_KERNEL
/*ARGSUSED*/
de6copen(dev, flag)
	dev_t	dev;
	int	flag;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NDE6C ||
		de6c_softc[unit].port == 0)
	    return (ENXIO);

	de6c_softc[unit].ds_if.if_flags |= IFF_UP;
	de6cinit(unit);
	return(0);
}
#endif	MACH_KERNEL

/*
 * de6cstart:
 *
 *	This is yet another interface routine that simply tries to output a
 *	in an mbuf after a reset.
 *
 * input	: board number
 * output	: stuff sent to board if any there
 *
 */

/* NOTE: called at SPLNET */
void de6cstart(
	int	unit)
{
	struct	ifnet	*ifp = &(de6c_softc[unit].ds_if);
	MM 		m;

	for(;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m != (MM) 0)
			de6cxmt(unit, m);
		else
			return;
	}
}

#ifdef	MACH_KERNEL
/*ARGSUSED*/
io_return_t
de6cgetstat(
	dev_t		dev,
	int		flavor,
	dev_status_t	status,		/* pointer to OUT array */
	natural_t	*count)		/* out */
{
	register int	unit = minor(dev);
	register de6c_softc_t *sp;

	if (unit < 0 || unit >= NDE6C ||
		de6c_softc[unit].port == 0)
	    return (ENXIO);

	sp = &de6c_softc[unit];
	if (! sp->alive)
		if (! (sp->alive = de6calive(sp)))
			return ENXIO;

	return (net_getstat(&de6c_softc[unit].ds_if,
			    flavor,
			    status,
			    count));
}

io_return_t
de6csetstat(
	dev_t		dev,
	int		flavor,
	dev_status_t	status,
	natural_t	count)
{
	register int	unit = minor(dev);
	register de6c_softc_t *sp;

	if (unit < 0 || unit >= NDE6C ||
		de6c_softc[unit].port == 0)
	    return (ENXIO);

	sp = &de6c_softc[unit];
	if (! sp->alive)
		if (! (sp->alive = de6calive(sp)))
			return ENXIO;


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

		if (ns->flags & IFF_ALLMULTI)
		    mode |= MOD_ENAL;
		if (ns->flags & IFF_PROMISC)
		    mode |= MOD_PROM;

		/*
		 * Force a compilete reset if the receive mode changes
		 * so that these take effect immediately.
		 */
		if (sp->mode != mode) {
		    sp->mode = mode;
		    if (sp->flags & DSF_RUNNING) {
			sp->flags &= ~(DSF_LOCK | DSF_RUNNING);
			de6cinit(unit);
		    }
		}
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
   		de6csetid(sp->port, ec->addr);
		break;
	    }

	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}
#else	MACH_KERNEL

/*
 * de6cioctl:
 *
 *	This routine processes an ioctl request from the "if" layer
 *	above.
 *
 * input	: pointer the appropriate "if" struct, command, and data
 * output	: based on command appropriate action is taken on the
 *	 	  de6c board(s) or related structures
 * return	: error is returned containing exit conditions
 *
 */
de6cioctl(ifp, cmd, data)
struct ifnet	*ifp;
int	cmd;
caddr_t	data;
{
	register struct ifaddr		*ifa = (struct ifaddr *)data;
	register de6c_softc_t 		*sp = &de6c_softc[ifp->if_unit];
	int 				opri, error;
	short 				mode = 0;

	if (! sp->alive)
		if (! (sp->alive = de6calive(sp)))
			return ENXIO;

 	opri = SPLNET();
	error = 0;
	switch (cmd) {
		case SIOCSIFADDR:
			ifp->if_flags |= IFF_UP;
			de6cinit(ifp->if_unit);
			switch (ifa->ifa_addr.sa_family) {
#ifdef INET
				case AF_INET:
					((struct arpcom *)ifp)->ac_ipaddr =
						IA_SIN(ifa)->sin_addr;
					arpwhohas((struct arpcom *)ifp, 
						  &IA_SIN(ifa)->sin_addr);
					break;
#endif
#ifdef NS
				case AF_NS:
		    			{
					register struct ns_addr *ina = 
					&(IA_SNS(ifa)->sns_addr);
					if (ns_nullhost(*ina))
						ina->x_host = 
						*(union ns_host *)(ds->ds_addr);
					else
						de6cseteh(ina->x_host.c_host,
						de6c_softc[ifp->if_unit].port);
					break;
		    			}
#endif
			}
			break;
		case SIOCSIFFLAGS:
			if (ifp->if_flags & IFF_ALLMULTI)
				mode |= MOD_ENAL;
			if (ifp->if_flags & IFF_PROMISC)
				mode |= MOD_PROM;
			/*
			 * force a complete reset if the receive multicast/
			 * promiscuous mode changes so that these take 
			 * effect immediately.
			 *
			 */
			if (sp->mode != mode) {
				sp->mode = mode;
				if (sp->flags & DSF_RUNNING) {
			    		sp->flags &=
						~(DSF_LOCK|DSF_RUNNING);
					de6cinit(ifp->if_unit);
				}
			}
			if ((ifp->if_flags & IFF_UP) == 0 &&
		    	    sp->flags & DSF_RUNNING) {
				sp->timer = -1;
				de6cintoff(ifp->if_unit);
			} else 
			if (ifp->if_flags & IFF_UP &&
		    	    (sp->flags & DSF_RUNNING) == 0)
				de6cinit(ifp->if_unit);
			break;
	default:
		error = EINVAL;
	}
	splx(opri);
	return (error);
}
#endif	MACH_KERNEL

/*
 * de6cintr:
 *
 *	This function is the interrupt handler for the de6c ethernet
 *	board.  This routine will be called whenever either a packet
 *	is received, or a packet has successfully been transfered and
 *	the unit is ready to transmit another packet.
 *
 * input	: board number that interrupted
 * output	: either a packet is received, or a packet is transfered
 *
 */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
#ifdef	DEBUG_MORE
de6crcvintr(unit)
int unit;
{
	if(!de6cdo_rcvintr)
		return;
	de6cintr(unit);
}
#endif	/* DEBUG_MORE */

de6cintr(unit)
int unit;
{
	register de6c_softc_t	*sp = &de6c_softc[unit];
	int			port = sp->port;
	int			in;

	if (de6cactive[unit] || !(sp->flags & DSF_RUNNING))
		return;
	de6cactive[unit]++;
	de6coutb(sp, CMD(port), SLT_NIC);
	STAT_IN(sp, port, in);

	if ((in & (GOOD|TXBUSY)) == (GOOD|TXBUSY)) {
		/* on L40's means that we are disconnected */
		printf("de6intr%d: Card was disconnected; turning off network.\n", unit);
		de6cintoff(unit);
		de6cactive[unit]--;
		return;
	}

	if (in & GOOD)
		de6crcv(unit, in);
	else
/*rvb:tmp		printf("intr: %x\n", in)*/;


	de6coutb(sp, CMD(port), SLT_PRN);
	de6cactive[unit]--;
}
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

/*
 * de6crcv:
 *
 *	This routine is called by the interrupt handler to initiate a
 *	packet transfer from the board to the "if" layer above this
 *	driver.  This routine checks if a buffer has been successfully
 *	received by the de6c.  If so, the routine de6cread is called
 *	to do the actual transfer of the board data (including the
 *	ethernet header) into a packet (consisting of an mbuf chain).
 *
 * input	: number of the board to check
 * output	: if a packet is available, it is "sent up"
 *
 */
de6crcv(unit, in)
int	unit, in;
{
	register de6c_softc_t	*sp = &de6c_softc[unit];
	register struct ifnet	*ifp = &sp->ds_if;
	int			port = sp->port;
	int			bo;
	int			collision = 0;
	int			spins = 0;
	u_short			len;
	struct ether_header	header;
	int			tlen;
	register struct	ifqueue	*inq;
	int			opri;
	struct	ether_header 	eh;
#ifdef	MACH_KERNEL
	ipc_kmsg_t		new_kmsg;
	struct ether_header	*ehp;
	struct packet_header	*pkt;
#else	MACH_KERNEL
    	struct	mbuf		*m, *tm;
#endif	MACH_KERNEL

	sp->rcv++;

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	D(de6crcv0++);
#define MT (sp->consume == sp->produce)
	while (in & GOOD || !MT) {
		spins++;
		D(de6crcv1++);
		if (in & GOOD) {
			sp->rcvlen[sp->produce] = de6clen(sp);
			if ( ((sp->produce + 1) % XMTidx) != sp->consume) {
				if (++sp->produce == XMTidx)
					sp->produce = 0;
				DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
				DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|RXEN);
			} else collision = 1;
		}
		len = sp->rcvlen[sp->consume];
		bo = sp->consume*BFRSIZ;
		if (len < 60) {
			printf("de%d: len(%d) < 60\n", unit, len);
			goto out;
			return;
		}
		de6cread(sp, bo, &eh, sizeof(struct ether_header));
		bo += sizeof(struct ether_header);
		len -= 18;
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

#ifdef	MACH_KERNEL
		new_kmsg = net_kmsg_get();
		if (new_kmsg == IKM_NULL) {
		    /*
		     * Drop the packet.
		     */
		    sp->ds_if.if_rcvdrops++;
		    goto out;
		    return;
		}

		ehp = (struct ether_header *)
			(&net_kmsg(new_kmsg)->header[0]);
		pkt = (struct packet_header *)
			(&net_kmsg(new_kmsg)->packet[0]);

		/*
		 * Get header.
		 */
		*ehp = eh;

		/*
		 * Get body
		 */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
		de6cread(sp, bo, (char *)(pkt + 1), len);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
		pkt->type = ehp->ether_type;
		pkt->length = len + sizeof(struct packet_header);

		/*
		 * Hand the packet to the network module.
		 */
		net_packet(ifp, new_kmsg, pkt->length,
			   ethernet_priority(new_kmsg));

#else	MACH_KERNEL
		eh.ether_type = htons(eh.ether_type);
		m =(struct mbuf *)0;
		while ( len ) {
			if (m == (struct mbuf *)0) {
				m = m_get(M_DONTWAIT, MT_DATA);
				if (m == (struct mbuf *)0) {
					printf("de6crcv: Lost frame\n");
					goto out;
					return;
				}
				tm = m;
				tm->m_off = MMINOFF;
		/*
		 * first mbuf in the packet must contain a pointer to the
		 * ifnet structure.  other mbufs that follow and make up
		 * the packet do not need this pointer in the mbuf.
		 *
		 */
				*(mtod(tm, struct ifnet **)) = ifp;
				tm->m_len = sizeof(struct ifnet **);
			}
			else {
				tm->m_next = m_get(M_DONTWAIT, MT_DATA);
				tm = tm->m_next;
				tm->m_off = MMINOFF;
				tm->m_len = 0;
				if (tm == (struct mbuf *)0) {
					m_freem(m);
					printf("de6crcv: No mbufs, lost frame\n");
					goto out;
					return;
				}
			}
			tlen = MIN( MLEN - tm->m_len, len );
			tm->m_next = (struct mbuf *)0;
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
			de6cread(sp, bo, mtod(tm, char *)+tm->m_len, tlen);
			bo += tlen;
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
			tm->m_len += tlen;
			len -= tlen;
		}
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
		STAT_IN(sp, port, in);
		if (in & GOOD) {		/* got another */
			D(de6crcv2++);
			sp->rcvlen[sp->produce] = de6clen(sp);
			if ( ((sp->produce + 1) % XMTidx) != sp->consume) {
				if (++sp->produce == XMTidx)
					sp->produce = 0;
				DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
				DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|RXEN);
			} else collision = 1;
		}
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

		/*
		 * received packet is now in a chain of mbuf's.  next step is
		 * to pass the packet upwards.
		 *
		 */
		switch (eh.ether_type) {

#ifdef INET
			case ETHERTYPE_IP:
				schednetisr(NETISR_IP);
				inq = &ipintrq;
				break;
			case ETHERTYPE_ARP:
				arpinput(&sp->de6c_ac, m);
				goto out;
				return;
#endif
#ifdef NS
			case ETHERTYPE_NS:
				schednetisr(NETISR_NS);
				inq = &nsintrq;
				break;
#endif
			default:
				m_freem(m);
				goto out;
				return;
		}
		opri = SPLNET();
		if (IF_QFULL(inq)) {
			IF_DROP(inq);
			splx(opri);
			m_freem(m);
			goto out;
			return;
		}
		IF_ENQUEUE(inq, m);
		splx(opri);
#endif	MACH_KERNEL
out:
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
		STAT_IN(sp, port, in);
		if (in & GOOD) {		/* got another */
			D(de6crcv3++);
		}
/*2*/	/* implies wrap and pause */
		if (collision) {
			collision = 0;
			D(printf("*C* "));
			sp->rcvoff++;
			if (++sp->produce == XMTidx)
				sp->produce = 0;
			DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
			DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|RXEN);
		}
/*2*/	/* implies wrap and pause */
		if (++sp->consume == XMTidx)
			sp->consume = 0;
		if (spins > 10) {
			spins = 0;
			D(printf("*R* "));
			sp->rcvspin++;
			/* how should we recover here ??? */;
			/* return does not work */;
			/* de6cinit(unit) gets ugly if we are called from
			   de6cxmt */;
		}
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
	}
}


/*
 * de6cxmt:
 *
 *	This routine fills in the appropriate registers and memory
 *	locations on the d-link "600" board and starts the board off on
 *	the transmit.
 *
 * input	: board number of interest, and a pointer to the mbuf
 * output	: board memory and registers are set for xfer and attention
 *
 */
/* NOTE: called at SPLNET */
/*
 * This implies that rcv interrupts will be blocked.
 */
#define	max(a,b)	(((a) > (b)) ? (a) : (b))

char de6mt[ETHERMIN];
de6cxmt(unit, m)
int	unit;
MM	m;
{
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	register struct	mbuf	*tm_p;
#endif	MACH_KERNEL
	int			i;
	int			in;
	int			bo, boo;
	de6c_softc_t		*sp = &de6c_softc[unit];
	int			port = sp->port;
	u_short			count = 0;
	u_short			bytes_in_msg;
	static int		m_length();

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	if (de6cactive[unit] >= 2)		/* a funny loop caused by */
		return;				/* a flood of arps */
	if (de6cactive[unit]++ == 0)
		de6coutb(sp, CMD(port), SLT_NIC);
	STAT_IN(sp, port, in);

	D(if (de6cdo_xmt) printf("xmt: stat[-] = %x\n", in));
	if (in & GOOD) {
		de6crcv(unit, in);
	}
	sp->xmt++;
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

#ifdef	MACH_KERNEL
	count = m->io_count;
	bytes_in_msg = max(count, ETHERMIN + sizeof(struct ether_header));
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	boo = bo = XMTidx*BFRSIZ+(BFRSIZ-bytes_in_msg);
	de6cwrite(sp, bo, m->io_data, count);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
	bo += count;
	iodone(m);
#else	MACH_KERNEL
	bytes_in_msg = max(m_length(m), ETHERMIN + sizeof(struct ether_header));
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	boo = bo = XMTidx*BFRSIZ+(BFRSIZ-bytes_in_msg);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

	for (tm_p = m; tm_p != (struct mbuf *)0; tm_p = tm_p->m_next) {
		if (count + tm_p->m_len > ETHERMTU + sizeof(struct ether_header))
			break;
		if (tm_p->m_len == 0)
			continue;
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
		de6cwrite(sp, bo, mtod(tm_p, caddr_t), tm_p->m_len);
		bo += tm_p->m_len;
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
		count += tm_p->m_len;
	}
	m_freem(m);
#endif	MACH_KERNEL

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	if (bytes_in_msg - count > 0)
		de6cwrite(sp, bo, de6mt, bytes_in_msg - count);

	DATA_OUT(sp, port, TX_ADR,  boo       & 0xff);
	DATA_OUT(sp, port, TX_ADR, (boo >> 8) & 0xff);
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|TXEN);

	for (i = 0; i < XMT_BSY_WAIT; i++) {
		STAT_IN(sp, port, in);
		D(if (de6cdo_xmt) printf("xmt: stat[%d] = %x\n", i, in));
		if (in & GOOD) {
			/*
			 * this does indeed happen
			 * printf("!#");
			 */
			de6crcv(unit, in);
		}
		if (!(in & TXBUSY)) {
			goto out;
			return;
		}
	}
	printf("dexmt: stat[??] = %x\n", i, in);
out:
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4));
	DATA_OUT(sp, port, COMMAND, RX_BP|(sp->produce<<4)|RXEN);

	if (--de6cactive[unit] == 0)
		de6coutb(sp, CMD(port), SLT_PRN);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
}

/*
 * de6cintoff:
 *
 *	This function turns interrupts off for the de6c board indicated.
 *
 */
de6cintoff(unit)
int unit;
{
 	de6c_softc_t	*sp = &de6c_softc[unit];
	int		port = sp->port;
	int		pic = lprinfo[unit]->PIC;

	printf("de%d: Turning off d-link \"600\"\n", unit);
	sp->ds_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
	sp->flags &= ~(DSF_LOCK | DSF_RUNNING);

/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
	de6coutb(sp, CMD(port), SLT_NIC);
	DATA_OUT(sp, port, COMMAND, RESET);
	DATA_OUT(sp, port, COMMAND, STOP_RESET);
	de6coutb(sp, CMD(port), SLT_PRN);
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

	outb(CMD(sp->port), 0x07);
	ivect[pic] = sp->oldvect;
	iunit[pic] = sp->oldunit;
}

#ifdef	MACH_KERNEL
#else	MACH_KERNEL
/*
 * The length of an mbuf chain
 */
static
m_length(m)
	register struct mbuf *m;
{
	register int len = 0;
	
	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	return len;
}
#endif	MACH_KERNEL


/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
/* d-link 600 ON; d-link 600 ON; d-link 600 ON; d-link 600 ON */
de6cgetid(sp, buf)
de6c_softc_t *sp;
u_char *buf;
{
	de6cread(sp, EADDR, buf, 6);
	if ((buf[0] != 0x00) || (buf[1] != 0xde) || (buf[2] != 0x15))
		return 0;
	buf[0] = 0x80;
	buf[1] = 0x00;
	buf[2] = 0xc8;
	/* for this model d-link we assert 0x70 as the high mfr's nibble. */
	buf[3] = 0x70 | (buf[3] & 0xf);
	return 1;
}

de6csetid(sp, buf)
de6c_softc_t *sp;
char *buf;
{
	de6cwrite(sp, EADDR, buf, 6);
}

/*
 * get length of packet just rcv'd.
 * includes ether header and crc
 */
de6clen(sp)
de6c_softc_t *sp;
{
	int	port = sp->port;
	int	in;
	int	i;

	de6coutb(sp, DATA(port), RX_LEN);
	in = inb(STAT(port));
	de6coutb(sp, DATA(port), RX_LEN|STROBE);
	i = ((in>>4) | (inb(STAT(port)) & 0xf0));

	de6coutb(sp, DATA(port), RX_LEN);
	in = inb(STAT(port));
	de6coutb(sp, DATA(port), RX_LEN|STROBE);
	i |= ((in>>4) | (inb(STAT(port)) & 0xf0)) << 8;

	return i;
}

#if	0
de6cread(sp, address, buf, len)
de6c_softc_t *sp;
unsigned char *buf;
{
	int 	port = sp->port;
	u_char	in;

	DATA_OUT(sp, port, RW_ADR,  address       & 0xff);
	DATA_OUT(sp, port, RW_ADR, (address >> 8) & 0xff);

	while (len--) {
		de6coutb(sp, DATA(port), READ);
		in = inb(STAT(port));
		de6coutb(sp, DATA(port), READ|STROBE);
		*buf++ = ((in>>4) | (inb(STAT(port)) & 0xf0));
	}
}

de6cwrite(sp, address, buf, len)
de6c_softc_t *sp;
unsigned char *buf;
{
	int 	port = sp->port;
	int 	out;

	DATA_OUT(sp, port, RW_ADR,  address       & 0xff);
	DATA_OUT(sp, port, RW_ADR, (address >> 8) & 0xff);

	while (len--) {
		out = *buf++;
		DATA_OUT(sp, port, WRITE, out);

	}
}
#endif	0

#ifndef	de6cread
de6cread(sp, address, buf, len)
de6c_softc_t *sp;
unsigned char *buf;
{
	int 			port = sp->port;
	register volatile int	i;
	unsigned char		in;

	outb(port, ((address)<<4)  | RW_ADR);
	i = sp->latency; while (i-- > 0);
	outb(port, ((address)&0xf0)| RW_ADR | 0x8);
	i = sp->latency; while (i-- > 0);

	outb(port, ((address>>8)<<4)  | RW_ADR);
	i = sp->latency; while (i-- > 0);
	outb(port, ((address>>8)&0xf0)| RW_ADR | 0x8);
	i = sp->latency; while (i-- > 0);

	while (len--) {
		outb(port, READ);
		i = sp->latency; while (i-- > 0);
		in = inb(STAT(port));
		outb(port, READ|0x08);
		i = sp->latency; while (i-- > 0);
		*buf++ = ((in>>4) | (inb(STAT(port)) & 0xf0));
	}
}
#endif	/* de6cread */

#ifndef	de6cwrite
de6cwrite(sp, address, buf, len)
de6c_softc_t *sp;
unsigned char *buf;
{
	int 			port = sp->port;
	register volatile int	i;
	unsigned char		out;

	outb(port, ((address)<<4)  | RW_ADR);
	i = sp->latency; while (i-- > 0);
	outb(port, ((address)&0xf0)| RW_ADR | 0x8);
	i = sp->latency; while (i-- > 0);

	outb(port, ((address>>8)<<4)  | RW_ADR);
	i = sp->latency; while (i-- > 0);
	outb(port, ((address>>8)&0xf0)| RW_ADR | 0x8);
	i = sp->latency; while (i-- > 0);

	while (len--) {
		out = *buf++;
		outb(port, ((out)<<4)  | WRITE);
		i = sp->latency; while (i-- > 0);
		outb(port, ((out)&0xf0)| WRITE | 0x8);
		i = sp->latency; while (i-- > 0);
	}
}
#endif	/* de6cwrite */

de6coutb(sp, p, v)
de6c_softc_t *sp;
{
register volatile int i = sp->latency; 

	outb(p, v);
	while (i-- > 0);
}

de6cmemcheck(sp)
de6c_softc_t *sp;
{
	int i;
	int off = 0;
	int ret = 1;
#ifdef	MACH_KERNEL
	unsigned short *memchk;
	unsigned short *chkmem;
	if (kmem_alloc(kernel_map, (vm_offset_t *)&memchk, BFRS * BFRSIZ) !=
	    KERN_SUCCESS ||
	    kmem_alloc(kernel_map, (vm_offset_t *)&chkmem, BFRS * BFRSIZ) !=
	    KERN_SUCCESS) {
		printf("de6c: memory allocation failure!!\n");
		return 0;
	}
#else	/* MACH_KERNEL */
	unsigned short *memchk = (unsigned short *) kmem_alloc(kernel_map, BFRS * BFRSIZ);
	unsigned short *chkmem = (unsigned short *) kmem_alloc(kernel_map, BFRS * BFRSIZ);
	if ( ! ((int) memchk) || ! ((int) chkmem)) {
		printf("de6c: memory allocation failure!!\n");
		return 0;
	}
#endif	/* MACH_KERNEL */

	for (i = 0; i < BFRS * BFRSIZ/sizeof (short); i++)
		memchk[i] = i;
	bzero(chkmem, BFRS * BFRSIZ);


	for (off = 0; off < BFRS * BFRSIZ; off += BFRSIZ/2) {
		de6cwrite(sp, off, memchk+(off/sizeof (short)), BFRSIZ/2);
		de6cread (sp, off, chkmem+(off/sizeof (short)), BFRSIZ/2);
	}

	for (i = 0; i < BFRS * (BFRSIZ/sizeof (short)); i++)
		if (memchk[i] != chkmem[i]) {
			printf("de: tilt:seq [%x:%d] %x != %x\n",
				i, i, memchk[i], chkmem[i]);
			ret = 0;
			break;
		}

	kmem_free(kernel_map, (vm_offset_t) memchk, BFRS * BFRSIZ);
	kmem_free(kernel_map, (vm_offset_t) chkmem, BFRS * BFRSIZ);

	return ret;
}
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */
/* d-link 600 OFF; d-link 600 OFF; d-link 600 OFF; d-link 600 OFF */

#ifdef	DEBUG
#define STATIC
STATIC int print_pkt(), print_bdy();
STATIC int print_e_hdr(), print_ip_hdr(), print_ip();
STATIC int print_ipa(), print_arp(), print_e(), print_chars();

STATIC
print_pkt(p, len)
unsigned char *p;
{
	int j, k;
	int type;

	if (len < 18)
		printf("print_pkt: too small %d\n", len);

	type = print_e_hdr(p);

	switch (type) {
	case 0x806:
		print_arp(p+14);
		break;
	case 0x800:
		print_ip(p+14, len - 18);
		break;
	default:
		for (j = 14; j < len; j +=20) {
		    for (k = 0; k < 20; k++)
	 		printf("%2x ", p[j+k]);
		    printf("\n");
		}
	}
}

STATIC
print_bdy(p, len, type)
unsigned char *p;
{
	int j, k;

	if (len < 18)
		printf("print_pkt: too small %d|n", len);

	switch (type) {
	case 0x806:
		print_arp(p);
		break;
	case 0x800:
		print_ip(p, len);
		break;
	default:
		for (j = 0; j < len; j +=20) {
		    for (k = 0; k < 20; k++)
	 		printf("%2x ", p[j+k]);
		    printf("\n");
		}
	}
}

STATIC
print_e_hdr(p)
unsigned char *p;
{
	int type = ntohs(((unsigned short *)p)[6]);

	printf("S=%x:%x:%x:%x:%x:%x, ", p[6], p[7], p[8], p[9], p[10], p[11]);
	printf("D=%x:%x:%x:%x:%x:%x, ", p[0], p[1], p[2], p[3], p[4], p[5]);
	printf("T=%x\n", type);

	return type;
}

STATIC
print_ip_hdr(u)
u_char *u;
{

	int l = ntohs(*(u_short *)(u+2));
	
	print_ipa(u+12);
	printf(" -> ");
	print_ipa(u+12+4);
	printf(" L%d(0x%x)\n", l, l);
}

STATIC
print_ip(p, len)
unsigned char *p;
{
	int j,k;

	print_ip_hdr(p);
	for (k =0; k < 12; k++)
	 	printf("%2x ", p[k]);
	print_ipa(p+12);
	printf(" ");
	print_ipa(p+12+4);
	printf("\n");
	for (j = 20; j < len; j +=16) {
	    for (k = 0; k < 16; k++)
	 	printf("%2x ", p[j+k]);
	    print_chars(&p[j], 16);
	    printf("\n");
	}
}

STATIC
print_ipa(u)
u_char *u;
{
	printf("%d.%d.%d.%d", u[0], u[1], u[2], u[3]);
}

STATIC
print_arp(p)
#ifdef	MACH_KERNEL
{}
#else	MACH_KERNEL
struct arphdr *p;
{
	u_char *u = (u_char *)(p+1);

	printf("op = %x, pro = %x, hln = %x, pln = %x\n",
		ntohs(p->ar_op), ntohs(p->ar_pro), p->ar_hln, p->ar_pln);

	print_e(u);
	print_ipa(u+p->ar_hln);
	printf("        seeks\n");

	print_e(u+p->ar_hln+p->ar_pln);
	print_ipa(u+p->ar_hln+p->ar_pln+p->ar_hln);
	printf("\n");

}
#endif	MACH_KERNEL

STATIC
print_e(u)
u_char *u;
{
	printf("%x:%x:%x:%x:%x:%x ", u[0], u[1], u[2], u[3], u[4], u[5]);
}

STATIC
print_chars(u, len)
u_char *u;
{
	int c;

	printf("|>");
	while (len--) {
		c = *u++;
		if (c < 0x7f && c > 0x1f)
			printf("%c", c);
		else 
			printf(" ");
		}
	printf("<|");
}
#endif	DEBUG
