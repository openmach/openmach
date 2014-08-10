/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * Ethernet definitions.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	8/89
 */

#ifndef	_DEVICE_IF_ETHER_H_
#define _DEVICE_IF_ETHER_H_

#include <sys/types.h>

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct	ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

#define	ETHERMTU	1500
#define	ETHERMIN	(60-14)

#ifdef	KERNEL
u_char	etherbroadcastaddr[6];

extern char *	ether_sprintf();
#endif	KERNEL

#endif	/*_DEVICE_IF_ETHER_H_*/
