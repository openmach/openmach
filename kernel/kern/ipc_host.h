/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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

#ifndef	_KERN_IPC_HOST_H_
#define	_KERN_IPC_HOST_H_

#include <mach/port.h>
#include <kern/processor.h>

extern void ipc_host_init(void);

extern void ipc_processor_init(processor_t);

extern void ipc_pset_init(processor_set_t);
extern void ipc_pset_enable(processor_set_t);
extern void ipc_pset_disable(processor_set_t);
extern void ipc_pset_terminate(processor_set_t);

extern struct host *
convert_port_to_host(struct ipc_port *);

extern struct ipc_port *
convert_host_to_port(struct host *);

extern struct host *
convert_port_to_host_priv(struct ipc_port *);

extern processor_t
convert_port_to_processor(struct ipc_port *);

extern struct ipc_port *
convert_processor_to_port(processor_t);

extern processor_set_t
convert_port_to_pset(struct ipc_port *);

extern struct ipc_port *
convert_pset_to_port(processor_set_t);

extern processor_set_t
convert_port_to_pset_name(struct ipc_port *);

extern struct ipc_port *
convert_pset_name_to_port(processor_set_t);

#endif	_KERN_IPC_HOST_H_
