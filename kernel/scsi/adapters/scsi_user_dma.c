/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
/*
 *	File: scsi_user_dma.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	4/91
 *
 *	Mach 2.5 compat file, to handle case of DMA to user space
 *	[e.g. fsck and other raw device accesses]
 */

#ifdef	MACH_KERNEL
/* We do not need this in 3.0 */
#else	/*MACH_KERNEL*/

#include <mach/std_types.h>
#include <scsi/adapters/scsi_user_dma.h>

#include <kern/assert.h>

#include <vm/vm_kern.h>
#include <mach/vm_param.h>	/* round_page() */

/* bp -> pmap */
#include <sys/buf.h>
#include <sys/proc.h>

/*
 * Initialization, called once per device
 */
fdma_init(fdma, size)
	fdma_t		fdma;
	vm_size_t	size;
{
	vm_offset_t	addr;

	size = round_page(size);
	addr = kmem_alloc_pageable(kernel_map, size);
	if (addr == 0) panic("fdma_init");

	fdma->kernel_virtual = addr;
	fdma->max_data = size;
	fdma->user_virtual = -1;

}

/*
 * Remap a buffer from user space to kernel space.
 * Note that physio() has already validated
 * and wired the user's address range.
 */
fdma_map(fdma, bp)
	fdma_t			fdma;
	struct buf		*bp;
{
	pmap_t			pmap;
	vm_offset_t		user_addr;
	vm_size_t		size;
	vm_offset_t		kernel_addr;
	vm_offset_t		off;
	vm_prot_t		prot;

	/*
	 * If this is not to user space, or no data xfer is
	 * involved, no need to do anything.
	 */
	user_addr = (vm_offset_t)bp->b_un.b_addr;
	if (!(bp->b_flags & B_PHYS) || (user_addr == 0)) {
		fdma->user_virtual = -1;
		return;
	}
	/*
	 * We are going to clobber the buffer pointer, so
	 * remember what it was to restore it later.
	 */
	fdma->user_virtual = user_addr;

	/*
	 * Account for initial offset into phys page
	 */
	off = user_addr - trunc_page(user_addr);

	/*
	 * Check xfer size makes sense, note how many pages we'll remap
	 */
	size = bp->b_bcount + off;
	assert((size <= fdma->max_data));
	fdma->xfer_size_rnd = round_page(size);

	pmap = bp->b_proc->task->map->pmap;

	/*
	 * Use minimal protection possible
	 */
	prot = VM_PROT_READ;
	if (bp->b_flags & B_READ)
		prot |= VM_PROT_WRITE;

	/*
	 * Loop through all phys pages, taking them from the
	 * user pmap (they are wired) and inserting them into
	 * the kernel pmap.
	 */
	user_addr -= off;
	kernel_addr = fdma->kernel_virtual;
	bp->b_un.b_addr = (char *)kernel_addr + off;

	for (size = fdma->xfer_size_rnd; size; size -= PAGE_SIZE) {
		register vm_offset_t phys;

		phys = pmap_extract(pmap, user_addr);
		pmap_enter(kernel_pmap, kernel_addr, phys, prot, TRUE);
		user_addr += PAGE_SIZE;
		kernel_addr += PAGE_SIZE;
	}
}

/*
 * Called at end of xfer, to restore the buffer
 */
fdma_unmap(fdma, bp)
	fdma_t		fdma;
	struct buf		*bp;
{
	register vm_offset_t end_addr;

	/*
	 * Check we actually did remap it
	 */
	if (fdma->user_virtual == -1)
		return;

	/*
	 * Restore the buffer
	 */
	bp->b_un.b_addr = (char *)fdma->user_virtual;
	fdma->user_virtual = -1;

	/*
	 * Eliminate the mapping, pmap module might mess up
	 * the pv list otherwise.  Some might actually tolerate it.
	 */
	end_addr = fdma->kernel_virtual + fdma->xfer_size_rnd;
	pmap_remove(kernel_pmap, fdma->kernel_virtual, end_addr);

}

#endif	/*MACH_KERNEL*/
