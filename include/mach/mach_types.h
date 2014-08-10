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
 *	File:	mach/mach_types.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1986
 *
 *	Mach external interface definitions.
 *
 */

#ifndef	_MACH_MACH_TYPES_H_
#define _MACH_MACH_TYPES_H_

#include <mach/host_info.h>
#include <mach/machine.h>
#include <mach/machine/vm_types.h>
#include <mach/memory_object.h>
#include <mach/pc_sample.h>
#include <mach/port.h>
#include <mach/processor_info.h>
#include <mach/task_info.h>
#include <mach/task_special_ports.h>
#include <mach/thread_info.h>
#include <mach/thread_special_ports.h>
#include <mach/thread_status.h>
#include <mach/time_value.h>
#include <mach/vm_attributes.h>
#include <mach/vm_inherit.h>
#include <mach/vm_prot.h>
#include <mach/vm_statistics.h>

#ifdef	MACH_KERNEL
#include <kern/task.h>		/* for task_array_t */
#include <kern/thread.h>	/* for thread_array_t */
#include <kern/processor.h>	/* for processor_array_t,
				       processor_set_array_t,
				       processor_set_name_array_t */
#include <kern/syscall_emulation.h>
				/* for emulation_vector_t */
#include <norma_vm.h>
#if	NORMA_VM
typedef struct xmm_obj	*mach_xmm_obj_t;
extern mach_xmm_obj_t	xmm_kobj_lookup();
#endif	/* NORMA_VM */
#else	/* MACH_KERNEL */
typedef	mach_port_t	task_t;
typedef task_t		*task_array_t;
typedef	task_t		vm_task_t;
typedef task_t		ipc_space_t;
typedef	mach_port_t	thread_t;
typedef	thread_t	*thread_array_t;
typedef mach_port_t	host_t;
typedef mach_port_t	host_priv_t;
typedef mach_port_t	processor_t;
typedef mach_port_t	*processor_array_t;
typedef mach_port_t	processor_set_t;
typedef mach_port_t	processor_set_name_t;
typedef mach_port_t	*processor_set_array_t;
typedef mach_port_t	*processor_set_name_array_t;
typedef vm_offset_t	*emulation_vector_t;
#endif	/* MACH_KERNEL */

/*
 *	Backwards compatibility, for those programs written
 *	before mach/{std,mach}_types.{defs,h} were set up.
 */
#include <mach/std_types.h>

#endif	/* _MACH_MACH_TYPES_H_ */
