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
 *	File:	mach/norma_special_ports.h
 *
 *	Defines codes for remote access to special ports.  These are NOT
 *	port identifiers - they are only used for the norma_get_special_port
 *	and norma_set_special_port routines.
 */

#ifndef	_MACH_NORMA_SPECIAL_PORTS_H_
#define _MACH_NORMA_SPECIAL_PORTS_H_

#define	MAX_SPECIAL_KERNEL_ID	3
#define	MAX_SPECIAL_ID		32

/*
 * Provided by kernel
 */
#define NORMA_DEVICE_PORT	1
#define NORMA_HOST_PORT		2
#define NORMA_HOST_PRIV_PORT	3

/*
 * Not provided by kernel
 */
#define NORMA_NAMESERVER_PORT	(1 + MAX_SPECIAL_KERNEL_ID)

/*
 * Definitions for ease of use.
 *
 * In the get call, the host parameter can be any host, but will generally
 * be the local node host port. In the set call, the host must the per-node
 * host port for the node being affected.
 */

#define norma_get_device_port(host, node, port)	\
	(norma_get_special_port((host), (node), NORMA_DEVICE_PORT, (port)))

#define norma_set_device_port(host, port)	\
	(norma_set_special_port((host), NORMA_DEVICE_PORT, (port)))

#define norma_get_host_port(host, node, port)	\
	(norma_get_special_port((host), (node), NORMA_HOST_PORT, (port)))

#define norma_set_host_port(host, port)	\
	(norma_set_special_port((host), NORMA_HOST_PORT, (port)))

#define norma_get_host_priv_port(host, node, port)	\
	(norma_get_special_port((host), (node), NORMA_HOST_PRIV_PORT, (port)))

#define norma_set_host_priv_port(host, port)	\
	(norma_set_special_port((host), NORMA_HOST_PRIV_PORT, (port)))

#define norma_get_nameserver_port(host, node, port)	\
	(norma_get_special_port((host), (node), NORMA_NAMESERVER_PORT, (port)))

#define norma_set_nameserver_port(host, port)	\
	(norma_set_special_port((host), NORMA_NAMESERVER_PORT, (port)))

#endif	/* _MACH_NORMA_SPECIAL_PORTS_H_ */
