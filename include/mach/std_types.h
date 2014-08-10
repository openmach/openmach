/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989,1988 Carnegie Mellon University
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
 *	Mach standard external interface type definitions.
 *
 */

#ifndef	_MACH_STD_TYPES_H_
#define	_MACH_STD_TYPES_H_

#define	EXPORT_BOOLEAN

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/machine/vm_types.h>

typedef	vm_offset_t	pointer_t;
typedef	vm_offset_t	vm_address_t;

#ifdef	MACH_KERNEL
#include <ipc/ipc_port.h>
#endif	/* MACH_KERNEL */

#endif	/* _MACH_STD_TYPES_H_ */
