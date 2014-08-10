/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 *	File:	vm_param.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	I386 machine dependent virtual memory parameters.
 *	Most of the declarations are preceeded by I386_ (or i386_)
 *	which is OK because only I386 specific code will be using
 *	them.
 */

#ifndef	_I386_KERNEL_UTIL_VM_PARAM_H_
#define _I386_KERNEL_UTIL_VM_PARAM_H_

#include <mach/vm_param.h>


/* This variable is expected always to contain
   the kernel virtual address at which physical memory is mapped.
   It may change as paging is turned on or off.  */
extern vm_offset_t phys_mem_va;


/* Calculate a kernel virtual address from a physical address.  */
#define phystokv(pa)	((vm_offset_t)(pa) + phys_mem_va)

/* Same, but in reverse.
   This only works for the region of kernel virtual addresses
   that directly map physical addresses.  */
#define kvtophys(va)	((vm_offset_t)(va) - phys_mem_va)


/* This variable contains the kernel virtual address
   corresponding to linear address 0.
   In the absence of paging,
   linear addresses are always the same as physical addresses.  */
#ifndef linear_base_va
#define linear_base_va phys_mem_va
#endif

/* Convert between linear and kernel virtual addresses.  */
#define lintokv(la)	((vm_offset_t)(la) + linear_base_va)
#define kvtolin(va)	((vm_offset_t)(va) - linear_base_va)


/* This variable keeps track of where in physical memory
   our boot image was loaded.
   It holds the physical address
   corresponding to the boot image's virtual address 0.
   When paging is disabled, this is simply -phys_mem_va.
   However, when paging is enabled,
   phys_mem_va points to the place physical memory is mapped into exec space,
   and has no relationship to where in physical memory the boot image is.
   Thus, this variable always contains the location of the boot image
   whether or not paging is enabled.  */
extern vm_offset_t boot_image_pa;

/* Code segment we originally had when we started in real mode.
   Always equal to boot_image_pa >> 4.  */
extern unsigned short real_cs;



#endif /* _I386_KERNEL_UTIL_VM_PARAM_H_ */
