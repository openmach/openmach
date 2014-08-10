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
 
#include <mach/boolean.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <vm/vm_map.h>
#include "vm_param.h"
#include <mach/vm_prot.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <i386/pmap.h>
#include <mach/machine/vm_param.h>

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 */
pmap_zero_page(p)
	vm_offset_t p;
{
	assert(p != vm_page_fictitious_addr);
	bzero(phystokv(p), PAGE_SIZE);
}

/*
 *	pmap_copy_page copies the specified (machine independent) pages.
 */
pmap_copy_page(src, dst)
	vm_offset_t src, dst;
{
	assert(src != vm_page_fictitious_addr);
	assert(dst != vm_page_fictitious_addr);

	bcopy(phystokv(src), phystokv(dst), PAGE_SIZE);
}

/*
 *	copy_to_phys(src_addr_v, dst_addr_p, count)
 *
 *	Copy virtual memory to physical memory
 */
copy_to_phys(src_addr_v, dst_addr_p, count)
	vm_offset_t src_addr_v, dst_addr_p;
	int count;
{
	assert(dst_addr_p != vm_page_fictitious_addr);
	bcopy(src_addr_v, phystokv(dst_addr_p), count);
}

/*
 *	copy_from_phys(src_addr_p, dst_addr_v, count)
 *
 *	Copy physical memory to virtual memory.  The virtual memory
 *	is assumed to be present (e.g. the buffer pool).
 */
copy_from_phys(src_addr_p, dst_addr_v, count)
	vm_offset_t src_addr_p, dst_addr_v;
	int count;
{
	assert(src_addr_p != vm_page_fictitious_addr);
	bcopy(phystokv(src_addr_p), dst_addr_v, count);
}

/*
 *	kvtophys(addr)
 *
 *	Convert a kernel virtual address to a physical address
 */
vm_offset_t
kvtophys(addr)
vm_offset_t addr;
{
	pt_entry_t *pte;

	if ((pte = pmap_pte(kernel_pmap, addr)) == PT_ENTRY_NULL)
		return 0;
	return i386_trunc_page(*pte) | (addr & INTEL_OFFMASK);
}
