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
 *	File:	if_3c501.c
 *	Author: Philippe Bernadat
 *	Date:	1989
 * 	Copyright (c) 1989 OSF Research Institute 
 *
 * 	3COM Etherlink 3C501 Mach Ethernet drvier
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

#include <at3c501.h>

#ifdef	MACH_KERNEL
#include	<kern/time_out.h>
#include	<device/device_types.h>
#include	<device/errno.h>
#include	<device/io_req.h>
#include	<device/if_hdr.h>
#include	<device/if_ether.h>
#include	<device/net_status.h>
#include	<device/net_io.h>
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

#include	<i386/ipl.h>
#include	<chips/busses.h>
#include	<i386at/if_3c501.h>

#define	SPLNET	spl6

int	at3c501probe();
void	at3c501attach();
int	at3c501intr();
int	at3c501init();
int	at3c501output();
int	at3c501ioctl();
int	at3c501reset();
int	at3c501watch();

static vm_offset_t at3c501_std[NAT3C501] = { 0 };
static struct bus_device *at3c501_info[NAT3C501];
struct	bus_driver	at3c501driver = 
	{at3c501probe, 0, at3c501attach, 0, at3c501_std, "et", at3c501_info, };

int	watchdog_id;

typedef struct { 
#ifdef	MACH_KERNEL
	struct	ifnet	ds_if;		/* generic interface header */
	u_char	ds_addr[6];		/* Ethernet hardware address */
#else	MACH_KERNEL
	struct	arpcom	at3c501_ac;
#define	ds_if	at3c501_ac.ac_if
#define	ds_addr	at3c501_ac.ac_enaddr
#endif	MACH_KERNEL
	int	flags;
        int     timer;
	char 	*base;
        u_char   address[ETHER_ADD_SIZE];
	short	mode;
	int	badxmt;
	int	badrcv;
	int	spurious;
	int	rcv;
	int	xmt;
} at3c501_softc_t;

at3c501_softc_t	at3c501_softc[NAT3C501];

/*
 * at3c501probe:
 *
 *	This function "probes" or checks for the 3c501 board on the bus to see
 *	if it is there.  As far as I can tell, the best break between this
 *	routine and the attach code is to simply determine whether the board
 *	is configured in properly.  Currently my approach to this is to write
 *	and read a string from the Packet Buffer on the board being probed.
 *	If the string comes back properly then we assume the board is there.
 *	The config code expects to see a successful return from the probe
 *	routine before 	attach will be called.
 *
 * input	: address device is mapped to, and unit # being checked
 * output	: a '1' is returned if the board exists, and a 0 otherwise
 *
 */
at3c501probe(port, dev)
struct bus_device	*dev;
{
	caddr_t		base = (caddr_t)dev->address;
	int		unit = dev->unit;
	char		inbuf[50];
	char		*str = "3c501 ethernet board %d out of range\n";
	int 		strsize = strlen(str);

	if ((unit < 0) || (unit >= NAT3C501)) {
		printf(str, unit);
		return(0);
	}

	/* reset */
	outb(IE_CSR(base), IE_RESET);

	/* write a string to the packet buffer */

	outb(IE_CSR(base), IE_RIDE | IE_SYSBFR);
	outw(IE_GP(base), 0);
	loutb(IE_BFR(base), str, strsize);

	/* read it back */

	outb(IE_CSR(base), IE_RIDE | IE_SYSBFR);
	outw(IE_GP(base), 0);
	linb(IE_BFR(base), inbuf, strsize);
	/* compare them */

#ifdef	MACH_KERNEL
	if (strncmp(str, inbuf, strsize))
#else	MACH_KERNEL
	if (bcmp(str, inbuf, strsize))
#endif	MACH_KERNEL
	{
		return(0);
	}
	at3c501_softc[unit].base = base;

	return(1);
}

/*
 * at3c501attach:
 *
 *	This function attaches a 3C501 board to the "system".  The rest of
 *	runtime structures are initialized here (this routine is called after
 *	a successful probe of the board).  Once the ethernet address is read
 *	and stored, the board's ifnet structure is attached and readied.
 *
 * input	: bus_device structure setup in autoconfig
 * output	: board structs and ifnet is setup
 *
 */
void at3c501attach(dev)
struct bus_device	*dev;
{
	at3c501_softc_t	*sp;
	struct	ifnet	*ifp;
	u_char	unit;
	caddr_t		base;
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	extern int	tcp_recvspace;
	tcp_recvspace = 0x300;		/* empircal messure */
#endif	MACH_KERNEL

	take_dev_irq(dev);
	unit = (u_char)dev->unit;	
	printf(", port = %x, spl = %d, pic = %d. ",
		dev->address, dev->sysdep, dev->sysdep1);

	sp = &at3c501_softc[unit];
	base = sp->base;
	if (base != (caddr_t)dev->address) {
		printf("3C501 board %d attach address error\n", unit);
		return;
	}
	sp->timer = -1;
	sp->flags = 0;
	sp->mode = 0;
	outb(IE_CSR(sp->base), IE_RESET);
	at3c501geteh(base, sp->ds_addr);
	at3c501geteh(base, sp->address);
	at3c501seteh(base, sp->address);
	printf("ethernet id [%x:%x:%x:%x:%x:%x]",
		sp->address[0],sp->address[1],sp->address[2], 
		sp->address[3],sp->address[4],sp->address[5]);
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
	ifp->if_name = "et";
	ifp->if_init = at3c501init;
	ifp->if_output = at3c501output;
	ifp->if_ioctl = at3c501ioctl;
	ifp->if_reset = at3c501reset;
	ifp->if_next = NULL;
	if_attach(ifp);
#ifdef notdef
	watchdog_id = timeout(at3c501watch, &(ifp->if_unit), 20*HZ);
#endif
#endif	MACH_KERNEL
}

/*
 * at3c501watch():
 *
 */
at3c501watch(b_ptr)

caddr_t	b_ptr;

{
	int	x,
		y,
		opri,
		unit;
	at3c501_softc_t *is;

	unit = *b_ptr;
#ifdef	MACH_KERNEL
	timeout(at3c501watch,b_ptr,20*hz);
#else	MACH_KERNEL
	watchdog_id = timeout(at3c501watch,b_ptr,20*HZ);
#endif	MACH_KERNEL
	is = &at3c501_softc[unit];
	printf("\nxmt/bad	rcv/bad	spurious\n");
	printf("%d/%d		%d/%d	%d\n", is->xmt, is->badxmt, \
		is->rcv, is->badrcv, is->spurious);
	is->rcv=is->badrcv=is->xmt=is->badxmt=is->spurious=0;
}

/*
 * at3c501geteh:
 *
 *	This function gets the ethernet address (array of 6 unsigned
 *	bytes) from the 3c501 board prom. 
 *
 */

at3c501geteh(base, ep)
caddr_t	base;
char *ep;
{
	int	i;

	for (i = 0; i < ETHER_ADD_SIZE; i++) {
		outw(IE_GP(base), i);
		*ep++ = inb(IE_SAPROM(base));
	}
}

/*
 * at3c501seteh:
 *
 *	This function sets the ethernet address (array of 6 unsigned
 *	bytes) on the 3c501 board. 
 *
 */

at3c501seteh(base, ep)
caddr_t	base;
char *ep;
{
	int	i;

	for (i = 0; i < ETHER_ADD_SIZE; i++) {
		outb(EDLC_ADDR(base) + i, *ep++);
	}
}

#ifdef	MACH_KERNEL
int at3c501start();	/* forward */

at3c501output(dev, ior)
	dev_t		dev;
	io_req_t	ior;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NAT3C501 ||
		at3c501_softc[unit].base == 0)
	    return (ENXIO);

	return (net_write(&at3c501_softc[unit].ds_if, at3c501start, ior));
}

at3c501setinput(dev, receive_port, priority, filter, filter_count)
	dev_t		dev;
	mach_port_t	receive_port;
	int		priority;
	filter_t	filter[];
	u_int		filter_count;
{
	register int unit = minor(dev);

	if (unit < 0 || unit >= NAT3C501 ||
		at3c501_softc[unit].base == 0)
	    return (ENXIO);

	return (net_set_filter(&at3c501_softc[unit].ds_if,
			receive_port, priority,
			filter, filter_count));
}

#else	MACH_KERNEL
/*
 * at3c501output:
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
at3c501output(ifp, m0, dst)
struct ifnet	*ifp;
struct mbuf	*m0;
struct sockaddr *dst;
{
	int 				type, error;
	spl_t				opri;
 	u_char				edst[6];
	struct in_addr 			idst;
	register at3c501_softc_t 	*is;
	register struct mbuf 		*m = m0;
	register struct ether_header	*eh;
	register int			 off;
	int				usetrailers;

	is = &at3c501_softc[ifp->if_unit];
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		printf("3C501 Turning off board %d\n", ifp->if_unit);
		at3c501intoff(ifp->if_unit);
		error = ENETDOWN;
		goto bad;
	}
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
 		if (!arpresolve(&is->at3c501_ac, m, &idst, edst, &usetrailers)){
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
		printf("at3c501%d: can't handle af%d\n", ifp->if_unit,
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
 	bcopy((caddr_t)is->address,(caddr_t)eh->ether_shost,
								sizeof(edst));
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
	/*
 	 * Some action needs to be added here for checking whether the
	 * board is already transmitting.  If it is, we don't want to
 	 * start it up (ie call at3c501start()).  We will attempt to send
 	 * packets that are queued up after an interrupt occurs.  Some
 	 * flag checking action has to happen here and/or in the start
 	 * routine.  This note is here to remind me that some thought
 	 * is needed and there is a potential problem here.
	 *
	 */
	at3c501start(ifp->if_unit);
	splx(opri);
	return (0);

bad:
	m_freem(m0);
	return (error);
}
#endif	MACH_KERNEL

/*
 * at3c501reset:
 *
 *	This routine is in part an entry point for the "if" code.  Since most 
 *	of the actual initialization has already (we hope already) been done
 *	by calling at3c501attach().
 *
 * input	: unit number or board number to reset
 * output	: board is reset
 *
 */
at3c501reset(unit)
int	unit;
{
	at3c501_softc[unit].ds_if.if_flags &= ~IFF_RUNNING;
	return(at3c501init(unit));
}



/*
 * at3c501init:
 *
 *	Another routine that interfaces the "if" layer to this driver.  
 *	Simply resets the structures that are used by "upper layers".  
 *	As well as calling at3c501hwrst that does reset the at3c501 board.
 *
 * input	: board number
 * output	: structures (if structs) and board are reset
 *
 */	
at3c501init(unit)
int	unit;
{
	struct	ifnet	*ifp;
	int		stat;
	spl_t		oldpri;

	ifp = &(at3c501_softc[unit].ds_if);
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	if (ifp->if_addrlist == (struct ifaddr *)0) {
		return;
	}
#endif	MACH_KERNEL
	oldpri = SPLNET();
	if ((stat = at3c501hwrst(unit)) == TRUE) {
		at3c501_softc[unit].ds_if.if_flags |= IFF_RUNNING;
		at3c501_softc[unit].flags |= DSF_RUNNING;
		at3c501start(unit);
	}
	else
		printf("3C501 trouble resetting board %d\n", unit);
	at3c501_softc[unit].timer = 5;
	splx(oldpri);
	return(stat);

}

#ifdef	MACH_KERNEL
/*ARGSUSED*/
at3c501open(dev, flag)
	dev_t	dev;
	int	flag;
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NAT3C501 ||
		at3c501_softc[unit].base == 0)
	    return (ENXIO);

	at3c501_softc[unit].ds_if.if_flags |= IFF_UP;
	at3c501init(unit);
	return(0);
}
#endif	MACH_KERNEL

/*
 * at3c501start:
 *
 *	This is yet another interface routine that simply tries to output a
 *	in an mbuf after a reset.
 *
 * input	: board number
 * output	: stuff sent to board if any there
 *
 */
at3c501start(unit)
int	unit;

{
#ifdef	MACH_KERNEL
	io_req_t	m;
#else	MACH_KERNEL
	struct	mbuf	*m;
#endif	MACH_KERNEL
	struct	ifnet	*ifp;

	ifp = &(at3c501_softc[unit].ds_if);
	for(;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
#ifdef	MACH_KERNEL
		if (m != 0)
#else	MACH_KERNEL
		if (m != (struct mbuf *)0)
#endif	MACH_KERNEL
			at3c501xmt(unit, m);
		else
			return;
	}
}

#ifdef	MACH_KERNEL
/*ARGSUSED*/
at3c501getstat(dev, flavor, status, count)
	dev_t		dev;
	int		flavor;
	dev_status_t	status;		/* pointer to OUT array */
	u_int		*count;		/* out */
{
	register int	unit = minor(dev);

	if (unit < 0 || unit >= NAT3C501 ||
		at3c501_softc[unit].base == 0)
	    return (ENXIO);

	return (net_getstat(&at3c501_softc[unit].ds_if,
			    flavor,
			    status,
			    count));
}

at3c501setstat(dev, flavor, status, count)
	dev_t		dev;
	int		flavor;
	dev_status_t	status;
	u_int		count;
{
	register int	unit = minor(dev);
	register at3c501_softc_t *sp;

	if (unit < 0 || unit >= NAT3C501 ||
		at3c501_softc[unit].base == 0)
	    return (ENXIO);

	sp = &at3c501_softc[unit];

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
			at3c501init(unit);
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
   		at3c501seteh(sp->base, ec->addr);
		break;
	    }

	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}
#else	MACH_KERNEL

/*
 * at3c501ioctl:
 *
 *	This routine processes an ioctl request from the "if" layer
 *	above.
 *
 * input	: pointer the appropriate "if" struct, command, and data
 * output	: based on command appropriate action is taken on the
 *	 	  at3c501 board(s) or related structures
 * return	: error is returned containing exit conditions
 *
 */
int		curr_ipl;
u_short	curr_pic_mask;
u_short 	pic_mask[];

at3c501ioctl(ifp, cmd, data)
struct ifnet	*ifp;
int	cmd;
caddr_t	data;
{
	register struct ifaddr		*ifa = (struct ifaddr *)data;
	register at3c501_softc_t 	*is;
	int 				error;
	spl_t				opri;
	short 				mode = 0;

	is = &at3c501_softc[ifp->if_unit];
 	opri = SPLNET();
	error = 0;
	switch (cmd) {
		case SIOCSIFADDR:
			ifp->if_flags |= IFF_UP;
			at3c501init(ifp->if_unit);
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
						at3c501seteh(ina->x_host.c_host,
						at3c501_softc[ifp->if_unit].base);
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
			if (is->mode != mode) {
				is->mode = mode;
				if (is->flags & DSF_RUNNING) {
			    		is->flags &=
						~(DSF_LOCK|DSF_RUNNING);
					at3c501init(ifp->if_unit);
				}
			}
			if ((ifp->if_flags & IFF_UP) == 0 &&
		    	    is->flags & DSF_RUNNING) {
				printf("AT3C501 ioctl: turning off board %d\n", 
				       ifp->if_unit);
				is->flags &= ~(DSF_LOCK | DSF_RUNNING);
				is->timer = -1;
				at3c501intoff(ifp->if_unit);
			} else 
			if (ifp->if_flags & IFF_UP &&
		    	    (is->flags & DSF_RUNNING) == 0)
				at3c501init(ifp->if_unit);
			break;
	default:
		error = EINVAL;
	}
	splx(opri);
	return (error);
}
#endif	MACH_KERNEL


/*
 * at3c501hwrst:
 *
 *	This routine resets the at3c501 board that corresponds to the 
 *	board number passed in.
 *
 * input	: board number to do a hardware reset
 * output	: board is reset
 *
 */
#define XMT_STAT (EDLC_16|EDLC_JAM|EDLC_UNDER|EDLC_IDLE)
#define RCV_STAT (EDLC_STALE|EDLC_ANY|EDLC_SHORT|EDLC_DRIBBLE|EDLC_OVER|EDLC_FCS)
int 
at3c501hwrst(unit)
int unit;
{
	u_char	stat;
	caddr_t	base = at3c501_softc[unit].base;

	outb(IE_CSR(base), IE_RESET);
	outb(IE_CSR(base), 0);
	at3c501seteh(base, at3c501_softc[unit].address);
	if ((stat = inb(IE_CSR(base))) != IE_RESET) {
		printf("at3c501reset: can't reset CSR: %x\n", stat);
		return(FALSE);
	}
	if ((stat = inb(EDLC_XMT(base))) & XMT_STAT) {
		printf("at3c501reset: can't reset XMT: %x\n", stat);
		return(FALSE);
	}
	if (((stat = inb(EDLC_RCV(base))) & RCV_STAT) != EDLC_STALE) {
		printf("at3c501reset: can't reset RCV: %x\n", stat);
		return(FALSE);
	}
	if (at3c501config(unit) == FALSE) {
		printf("at3c501hwrst(): failed to config\n");
		return(FALSE);
	}
	outb(IE_RP(base), 0);
	outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
	return(TRUE);
}

/*
 * at3c501intr:
 *
 *	This function is the interrupt handler for the at3c501 ethernet
 *	board.  This routine will be called whenever either a packet
 *	is received, or a packet has successfully been transfered and
 *	the unit is ready to transmit another packet.
 *
 * input	: board number that interrupted
 * output	: either a packet is received, or a packet is transfered
 *
 */
at3c501intr(unit)
int unit;
{
	at3c501rcv(unit);
	at3c501start(unit);

	return(0);
}


/*
 * at3c501rcv:
 *
 *	This routine is called by the interrupt handler to initiate a
 *	packet transfer from the board to the "if" layer above this
 *	driver.  This routine checks if a buffer has been successfully
 *	received by the at3c501.  If so, the routine at3c501read is called
 *	to do the actual transfer of the board data (including the
 *	ethernet header) into a packet (consisting of an mbuf chain).
 *
 * input	: number of the board to check
 * output	: if a packet is available, it is "sent up"
 *
 */
at3c501rcv(unit)
int	unit;
{
	int stat;
	caddr_t base;
#ifdef	MACH_KERNEL
	ipc_kmsg_t	new_kmsg;
	struct ether_header *ehp;
	struct packet_header *pkt;
#else	MACH_KERNEL
    	struct	mbuf	*m,	*tm;
#endif	MACH_KERNEL
	u_short	len;
	register struct ifnet *ifp;
	struct ether_header header;
	int	tlen;
	register at3c501_softc_t	*is;
	register struct	ifqueue		*inq;
	spl_t	opri;
	struct	ether_header eh;

	is = &at3c501_softc[unit];
	ifp = &is->ds_if;
	base = at3c501_softc[unit].base;
	is->rcv++;
	if (inb(IE_CSR(base)) & IE_RCVBSY)
		is->spurious++;
	while (!((stat=inb(EDLC_RCV(base))) & EDLC_STALE)) {
		outb(IE_CSR(base), IE_SYSBFR);
		if (!(stat & EDLC_ANY)) {
			outw(IE_GP(base), 0);
			len = inw(IE_RP(base))-sizeof(struct ether_header);
			outb(IE_RP(base), 0);
			outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
			is->badrcv++;
#ifdef DEBUG
			printf("at3c501rcv: received %d bad bytes", len);
			if (stat & EDLC_SHORT)
				printf(" Short frame");
			if (stat & EDLC_OVER) 
				printf(" Data overflow");
			if (stat & EDLC_DRIBBLE)
				printf(" Dribble error");
			if (stat & EDLC_FCS)
				printf(" CRC error");
			printf("\n");
#endif DEBUG
		} else {
			outw(IE_GP(base), 0);
			len = inw(IE_RP(base));
			if (len < 60) {
				outb(IE_RP(base), 0);
				outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
				return;
			}
			linb(IE_BFR(base), &eh, sizeof(struct ether_header));
#ifdef	MACH_KERNEL
			new_kmsg = net_kmsg_get();
			if (new_kmsg == IKM_NULL) {
			    /*
			     * Drop the packet.
			     */
			    is->ds_if.if_rcvdrops++;

			    outb(IE_RP(base), 0);
			    outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
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
			linb(IE_BFR(base),
			     (char *)(pkt + 1),
			     len - sizeof(struct ether_header));

			outb(IE_RP(base), 0);
			outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);

			pkt->type = ehp->ether_type;
			pkt->length = len - sizeof(struct ether_header)
					  + sizeof(struct packet_header);

			/*
			 * Hand the packet to the network module.
			 */
			net_packet(ifp, new_kmsg, pkt->length,
				   ethernet_priority(new_kmsg));

#else	MACH_KERNEL
			eh.ether_type = htons(eh.ether_type);
			m =(struct mbuf *)0;
#ifdef DEBUG
			printf("received %d bytes\n", len);
#endif DEBUG
			len -= sizeof(struct ether_header);
			while ( len ) {
				if (m == (struct mbuf *)0) {
					m = m_get(M_DONTWAIT, MT_DATA);
					if (m == (struct mbuf *)0) {
						printf("at3c501rcv: Lost frame\n");
						outb(IE_RP(base), 0);
						outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);

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
						printf("at3c501rcv: No mbufs, lost frame\n");
						outb(IE_RP(base), 0);
						outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
						return;
					}
				}
				tlen = MIN( MLEN - tm->m_len, len );
				tm->m_next = (struct mbuf *)0;
				linb(IE_BFR(base), mtod(tm, char *)+tm->m_len, tlen );
				tm->m_len += tlen;
				len -= tlen;
			}
			outb(IE_RP(base), 0);
			outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
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
					arpinput(&is->at3c501_ac, m);
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
					return;
			}
			opri = SPLNET();
			if (IF_QFULL(inq)) {
				IF_DROP(inq);
				splx(opri);
				m_freem(m);
				return;
			}
			IF_ENQUEUE(inq, m);
			splx(opri);
#endif	MACH_KERNEL
		}
	}
}


/*
 * at3c501xmt:
 *
 *	This routine fills in the appropriate registers and memory
 *	locations on the 3C501 board and starts the board off on
 *	the transmit.
 *
 * input	: board number of interest, and a pointer to the mbuf
 * output	: board memory and registers are set for xfer and attention
 *
 */
at3c501xmt(unit, m)
int	unit;
#ifdef	MACH_KERNEL
io_req_t	m;
#else	MACH_KERNEL
struct	mbuf	*m;
#endif	MACH_KERNEL
{
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	register struct	mbuf	*tm_p;
#endif	MACH_KERNEL
	int			i;
	at3c501_softc_t		*is = &at3c501_softc[unit];
	caddr_t			base = is->base;
	u_short			count = 0;
	u_short			bytes_in_msg;

	is->xmt++;
	outb(IE_CSR(base), IE_SYSBFR);
#ifdef	MACH_KERNEL
	count = m->io_count;
#define	max(a,b)	(((a) > (b)) ? (a) : (b))
	bytes_in_msg = max(count,
			   ETHERMIN + sizeof(struct ether_header));
#else	MACH_KERNEL
	bytes_in_msg = max(m_length(m), ETHERMIN + sizeof(struct ether_header));
#endif	MACH_KERNEL
	outw(IE_GP(base), BFRSIZ-bytes_in_msg);
#ifdef	MACH_KERNEL
	loutb(IE_BFR(base), m->io_data, count);
#else	MACH_KERNEL
	for (tm_p = m; tm_p != (struct mbuf *)0; tm_p = tm_p->m_next) {
		if (count + tm_p->m_len > ETHERMTU + sizeof(struct ether_header))
			break;
		if (tm_p->m_len == 0)
			continue;
		loutb(IE_BFR(base), mtod(tm_p, caddr_t), tm_p->m_len);
		count += tm_p->m_len;
	}
#endif	MACH_KERNEL
	while (count < bytes_in_msg) {
		outb(IE_BFR(base), 0);
		count++;
	}
	do {
		if (!(int)m) {
			outb(IE_CSR(base), IE_SYSBFR);
		}
		outw(IE_GP(base), BFRSIZ-bytes_in_msg);
		outb(IE_CSR(base), IE_RIDE|IE_XMTEDLC);
		if (m) {
#ifdef	MACH_KERNEL
			iodone(m);
			m = 0;
#else	MACH_KERNEL
			m_freem(m);
			m = (struct mbuf *) 0;
#endif	MACH_KERNEL
		}
		for (i=0; inb(IE_CSR(base)) & IE_XMTBSY; i++);
		if ((i=inb(EDLC_XMT(base))) & EDLC_JAM) {
			is->badxmt++;
#ifdef DEBUG
			printf("at3c501xmt jam\n");
#endif DEBUG
		}
	} while ((i & EDLC_JAM) && !(i & EDLC_16));

	if (i & EDLC_16) {
		printf("%");
	}
	return;

}

/*
 * at3c501config:
 *
 *	This routine does a standard config of the at3c501 board.
 *
 */
at3c501config(unit)
int	unit;
{
	caddr_t	base = at3c501_softc[unit].base;
	u_char	stat;

	/* Enable DMA & Interrupts */

	outb(IE_CSR(base), IE_RIDE|IE_SYSBFR);

	/* No Transmit Interrupts */

	outb(EDLC_XMT(base), 0);
	inb(EDLC_XMT(base));

	/* Setup Receive Interrupts */

	outb(EDLC_RCV(base), EDLC_BROAD|EDLC_SHORT|EDLC_GOOD|EDLC_DRIBBLE|EDLC_OVER);
	inb(EDLC_RCV(base));

	outb(IE_CSR(base), IE_RIDE|IE_SYSBFR);
	outb(IE_RP(base), 0);
	outb(IE_CSR(base), IE_RIDE|IE_RCVEDLC);
	return(TRUE);
}

/*
 * at3c501intoff:
 *
 *	This function turns interrupts off for the at3c501 board indicated.
 *
 */
at3c501intoff(unit)
int unit;
{
	caddr_t base = at3c501_softc[unit].base;
	outb(IE_CSR(base), 0);
}

#ifdef	MACH_KERNEL
#else	MACH_KERNEL
/*
 * The length of an mbuf chain
 */
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
