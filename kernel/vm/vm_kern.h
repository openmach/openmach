/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	vm/vm_kern.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Kernel memory management definitions.
 */

#ifndef	_VM_VM_KERN_H_
#define _VM_VM_KERN_H_

#include <mach/kern_return.h>
#include <vm/vm_map.h>

extern kern_return_t    projected_buffer_allocate();
extern kern_return_t    projected_buffer_deallocate();
extern kern_return_t    projected_buffer_map();
extern kern_return_t    projected_buffer_collect();

extern void		kmem_init();

extern kern_return_t	kmem_alloc();
extern kern_return_t	kmem_alloc_pageable();
extern kern_return_t	kmem_alloc_wired();
extern kern_return_t	kmem_alloc_aligned();
extern kern_return_t	kmem_realloc();
extern void		kmem_free();

extern vm_map_t		kmem_suballoc();

extern kern_return_t	kmem_io_map_copyout();
extern void		kmem_io_map_deallocate();

extern vm_map_t	kernel_map;
extern vm_map_t	kernel_pageable_map;
extern vm_map_t ipc_kernel_map;

#endif	_VM_VM_KERN_H_
