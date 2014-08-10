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
 *	Taken from (bsd)net/if.h.  Modified for MACH kernel.
 */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)if.h	7.3 (Berkeley) 6/27/88
 */

#ifndef	_IF_HDR_
#define	_IF_HDR_

#include <kern/lock.h>
#include <kern/queue.h>

/*
 * Queue for network output and filter input.
 */
struct ifqueue {
	queue_head_t	ifq_head;	/* queue of io_req_t */
	int		ifq_len;	/* length of queue */
	int		ifq_maxlen;	/* maximum length of queue */
	int		ifq_drops;	/* number of packets dropped
					   because queue full */
	decl_simple_lock_data(,
			ifq_lock)	/* lock for queue and counters */
};

/*
 * Header for network interface drivers.
 */
struct ifnet {
	short	if_unit;		/* unit number */
	short	if_flags;		/* up/down, broadcast, etc. */
	short	if_timer;		/* time until if_watchdog called */
	short	if_mtu;			/* maximum transmission unit */
	short	if_header_size;		/* length of header */
	short	if_header_format;	/* format of hardware header */
	short	if_address_size;	/* length of hardware address */
	short	if_alloc_size;		/* size of read buffer to allocate */
	char	*if_address;		/* pointer to hardware address */
	struct ifqueue if_snd;		/* output queue */
	queue_head_t if_rcv_port_list;	/* input filter list */
	decl_simple_lock_data(,
		if_rcv_port_list_lock)	/* lock for filter list */
/* statistics */
	int	if_ipackets;		/* packets received */
	int	if_ierrors;		/* input errors */
	int	if_opackets;		/* packets sent */
	int	if_oerrors;		/* output errors */
	int	if_collisions;		/* collisions on csma interfaces */
	int	if_rcvdrops;		/* packets received but dropped */
};

#define	IFF_UP		0x0001		/* interface is up */
#define	IFF_BROADCAST	0x0002		/* interface can broadcast */
#define	IFF_DEBUG	0x0004		/* turn on debugging */
#define	IFF_LOOPBACK	0x0008		/* is a loopback net */
#define	IFF_POINTOPOINT	0x0010		/* point-to-point link */
#define	IFF_RUNNING	0x0040		/* resources allocated */
#define	IFF_NOARP	0x0080		/* no address resolution protocol */
#define	IFF_PROMISC	0x0100		/* receive all packets */
#define	IFF_ALLMULTI	0x0200		/* receive all multicast packets */
#define	IFF_BRIDGE	0x0100		/* support token ring routing field */
#define	IFF_SNAP	0x0200		/* support extended sap header */

/* internal flags only: */
#define	IFF_CANTCHANGE	(IFF_BROADCAST | IFF_POINTOPOINT | IFF_RUNNING)

/*
 * Output queues (ifp->if_snd)
 * have queues of messages stored on ifqueue structures.  Entries
 * are added to and deleted from these structures by these macros, which
 * should be called with ipl raised to splimp().
 * XXX locking XXX
 */

#define	IF_QFULL(ifq)		((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define	IF_DROP(ifq)		((ifq)->ifq_drops++)
#define	IF_ENQUEUE(ifq, ior) { \
	simple_lock(&(ifq)->ifq_lock); \
	enqueue_tail(&(ifq)->ifq_head, (queue_entry_t)ior); \
	(ifq)->ifq_len++; \
	simple_unlock(&(ifq)->ifq_lock); \
}
#define	IF_PREPEND(ifq, ior) { \
	simple_lock(&(ifq)->ifq_lock); \
	enqueue_head(&(ifq)->ifq_head, (queue_entry_t)ior); \
	(ifq)->ifq_len++; \
	simple_unlock(&(ifq)->ifq_lock); \
}

#define	IF_DEQUEUE(ifq, ior) { \
	simple_lock(&(ifq)->ifq_lock); \
	if (((ior) = (io_req_t)dequeue_head(&(ifq)->ifq_head)) != 0) \
	    (ifq)->ifq_len--; \
	simple_unlock(&(ifq)->ifq_lock); \
}

#define	IFQ_MAXLEN	50

#define	IFQ_INIT(ifq) { \
	queue_init(&(ifq)->ifq_head); \
	simple_lock_init(&(ifq)->ifq_lock); \
	(ifq)->ifq_len = 0; \
	(ifq)->ifq_maxlen = IFQ_MAXLEN; \
	(ifq)->ifq_drops = 0; \
}

#define	IFNET_SLOWHZ	1		/* granularity is 1 second */

#endif	_IF_HDR_
