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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef	_VM_MEMORY_OBJECT_H_
#define	_VM_MEMORY_OBJECT_H_

#include <mach/boolean.h>

/*
 *	We use "struct ipc_port *" instead of "ipc_port_t"
 *	to avoid include file circularities.
 */

extern struct ipc_port *memory_manager_default_reference();
extern boolean_t memory_manager_default_port();
extern void memory_manager_default_init();

extern struct ipc_port *memory_manager_default;

#endif	_VM_MEMORY_OBJECT_H_
