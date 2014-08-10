/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989, 1988 Carnegie Mellon University
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
 *	File:	model_dep.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Basic initialization for I386 - ISA bus machines.
 */

#include <platforms.h>
#include <mach_kdb.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <mach/machine/multiboot.h>

#include "vm_param.h"
#include <kern/time_out.h>
#include <sys/time.h>
#include <vm/vm_page.h>
#include <i386/machspl.h>
#include <i386/pmap.h>
#include "proc_reg.h"

/* Location of the kernel's symbol table.
   Both of these are 0 if none is available.  */
#if MACH_KDB
static vm_offset_t kern_sym_start, kern_sym_end;
#else
#define kern_sym_start	0
#define kern_sym_end	0
#endif

/* These indicate the total extent of physical memory addresses we're using.
   They are page-aligned.  */
vm_offset_t phys_first_addr = 0;
vm_offset_t phys_last_addr;

/* Virtual address of physical memory, for the kvtophys/phystokv macros.  */
vm_offset_t phys_mem_va;

struct multiboot_info *boot_info;

/* Command line supplied to kernel.  */
char *kernel_cmdline = "";

/* This is used for memory initialization:
   it gets bumped up through physical memory
   that exists and is not occupied by boot gunk.
   It is not necessarily page-aligned.  */
static vm_offset_t avail_next = 0x1000; /* XX end of BIOS data area */

/* Possibly overestimated amount of available memory
   still remaining to be handed to the VM system.  */
static vm_size_t avail_remaining;

/* Configuration parameter:
   if zero, only use physical memory in the low 16MB of addresses.
   Only SCSI still has DMA problems.  */
#ifdef LINUX_DEV
int use_all_mem = 1;
#else
#include "nscsi.h"
#if	NSCSI > 0
int use_all_mem = 0;
#else
int use_all_mem = 1;
#endif
#endif

extern char	version[];

extern void	setup_main();

void		inittodr();	/* forward */

int		rebootflag = 0;	/* exported to kdintr */

/* XX interrupt stack pointer and highwater mark, for locore.S.  */
vm_offset_t int_stack_top, int_stack_high;

#ifdef LINUX_DEV
extern void linux_init(void);
#endif

/*
 * Find devices.  The system is alive.
 */
void machine_init()
{
	/*
	 * Initialize the console.
	 */
	cninit();

	/*
	 * Set up to use floating point.
	 */
	init_fpu();

#ifdef LINUX_DEV
	/*
	 * Initialize Linux drivers.
	 */
	linux_init();
#endif

	/*
	 * Find the devices
	 */
	probeio();

	/*
	 * Get the time
	 */
	inittodr();

	/*
	 * Tell the BIOS not to clear and test memory.
	 */
	*(unsigned short *)phystokv(0x472) = 0x1234;

	/*
	 * Unmap page 0 to trap NULL references.
	 */
	pmap_unmap_page_zero();
}

/*
 * Halt a cpu.
 */
halt_cpu()
{
	asm volatile("cli");
	while(1);
}

/*
 * Halt the system or reboot.
 */
halt_all_cpus(reboot)
	boolean_t	reboot;
{
	if (reboot) {
	    kdreboot();
	}
	else {
	    rebootflag = 1;
	    printf("In tight loop: hit ctl-alt-del to reboot\n");
	    (void) spl0();
	}
	for (;;)
	    continue;
}

void exit(int rc)
{
	halt_all_cpus(0);
}

void db_reset_cpu()
{
	halt_all_cpus(1);
}


/*
 * Compute physical memory size and other parameters.
 */
void
mem_size_init()
{
	/* Physical memory on all PCs starts at physical address 0.
	   XX make it a constant.  */
	phys_first_addr = 0;

	phys_last_addr = 0x100000 + (boot_info->mem_upper * 0x400);
	avail_remaining
	  = phys_last_addr - (0x100000 - (boot_info->mem_lower * 0x400)
			      - 0x1000);

	printf("AT386 boot: physical memory from 0x%x to 0x%x\n",
	       phys_first_addr, phys_last_addr);

	if ((!use_all_mem) && phys_last_addr > 16 * 1024*1024) {
		printf("** Limiting useable memory to 16 Meg to avoid DMA problems.\n");
		/* This is actually enforced below, in init_alloc_aligned.  */
	}

	phys_first_addr = round_page(phys_first_addr);
	phys_last_addr = trunc_page(phys_last_addr);
}

/*
 * Basic PC VM initialization.
 * Turns on paging and changes the kernel segments to use high linear addresses.
 */
i386at_init()
{
	/* XXX move to intel/pmap.h */
	extern pt_entry_t *kernel_page_dir;

	/*
	 * Initialize the PIC prior to any possible call to an spl.
	 */
	picinit();

	/*
	 * Find memory size parameters.
	 */
	mem_size_init();

	/*
	 *	Initialize kernel physical map, mapping the
	 *	region from loadpt to avail_start.
	 *	Kernel virtual address starts at VM_KERNEL_MIN_ADDRESS.
	 *	XXX make the BIOS page (page 0) read-only.
	 */
	pmap_bootstrap();

	/*
	 * Turn paging on.
	 * We'll have to temporarily install a direct mapping
	 * between physical memory and low linear memory,
	 * until we start using our new kernel segment descriptors.
	 * One page table (4MB) should do the trick.
	 * Also, set the WP bit so that on 486 or better processors
	 * page-level write protection works in kernel mode.
	 */
	kernel_page_dir[lin2pdenum(0)] =
		kernel_page_dir[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS)];
	set_cr3((unsigned)kernel_page_dir);
	set_cr0(get_cr0() | CR0_PG | CR0_WP);
	flush_instr_queue();

	/*
	 * Initialize and activate the real i386 protected-mode structures.
	 */
	gdt_init();
	idt_init();
	int_init();
	ldt_init();
	ktss_init();

	/* Get rid of the temporary direct mapping and flush it out of the TLB.  */
	kernel_page_dir[lin2pdenum(0)] = 0;
	set_cr3((unsigned)kernel_page_dir);



	/* XXX We'll just use the initialization stack we're already running on
	   as the interrupt stack for now.  Later this will have to change,
	   because the init stack will get freed after bootup.  */
	asm("movl %%esp,%0" : "=m" (int_stack_top));

	/* Interrupt stacks are allocated in physical memory,
	   while kernel stacks are allocated in kernel virtual memory,
	   so phys_last_addr serves as a convenient dividing point.  */
	int_stack_high = phys_last_addr;
}

/*
 *	C boot entrypoint - called by boot_entry in boothdr.S.
 *	Running in 32-bit flat mode, but without paging yet.
 */
void c_boot_entry(vm_offset_t bi)
{
	/* Stash the boot_image_info pointer.  */
	boot_info = (struct multiboot_info*)phystokv(bi);

	/* XXX we currently assume phys_mem_va is always 0 here -
	   if it isn't, we must tweak the pointers in the boot_info.  */

	/* Before we do _anything_ else, print the hello message.
	   If there are no initialized console devices yet,
	   it will be stored and printed at the first opportunity.  */
	printf(version);
	printf("\n");

	/* Find the kernel command line, if there is one.  */
	if (boot_info->flags & MULTIBOOT_CMDLINE)
		kernel_cmdline = (char*)phystokv(boot_info->cmdline);

#if	MACH_KDB
	/*
	 * Locate the kernel's symbol table, if the boot loader provided it.
	 * We need to do this before i386at_init()
	 * so that the symbol table's memory won't be stomped on.
	 */
	if ((boot_info->flags & MULTIBOOT_AOUT_SYMS)
	    && boot_info->syms.a.addr)
	{
		vm_size_t symtab_size, strtab_size;

		kern_sym_start = (vm_offset_t)phystokv(boot_info->syms.a.addr);
		symtab_size = (vm_offset_t)phystokv(boot_info->syms.a.tabsize);
		strtab_size = (vm_offset_t)phystokv(boot_info->syms.a.strsize);
		kern_sym_end = kern_sym_start + 4 + symtab_size + strtab_size;

		printf("kernel symbol table at %08x-%08x (%d,%d)\n",
		       kern_sym_start, kern_sym_end,
		       symtab_size, strtab_size);
	}
#endif	MACH_KDB

	/*
	 * Do basic VM initialization
	 */
	i386at_init();

#if	MACH_KDB
	/*
	 * Initialize the kernel debugger's kernel symbol table.
	 */
	if (kern_sym_start)
	{
		aout_db_sym_init(kern_sym_start, kern_sym_end, "mach", 0);
	}

	/*
	 * Cause a breakpoint trap to the debugger before proceeding
	 * any further if the proper option flag was specified
	 * on the kernel's command line.
	 * XXX check for surrounding spaces.
	 */
	if (strstr(kernel_cmdline, "-d ")) {
		cninit();		/* need console for debugger */
		Debugger();
	}
#endif	MACH_KDB

	machine_slot[0].is_cpu = TRUE;
	machine_slot[0].running = TRUE;
	machine_slot[0].cpu_type = CPU_TYPE_I386;
	machine_slot[0].cpu_subtype = CPU_SUBTYPE_AT386;

	/*
	 * Start the system.
	 */
	setup_main();

}

#include <mach/vm_prot.h>
#include <vm/pmap.h>
#include <mach/time_value.h>

timemmap(dev,off,prot)
	vm_prot_t prot;
{
	extern time_value_t *mtime;

#ifdef	lint
	dev++; off++;
#endif	lint

	if (prot & VM_PROT_WRITE) return (-1);

	return (i386_btop(pmap_extract(pmap_kernel(), (vm_offset_t) mtime)));
}

startrtclock()
{
	clkstart();
}

void
inittodr()
{
	time_value_t	new_time;

	new_time.seconds = 0;
	new_time.microseconds = 0;

	(void) readtodc(&new_time.seconds);

	{
	    spl_t	s = splhigh();
	    time = new_time;
	    splx(s);
	}
}

void
resettodr()
{
	writetodc();
}

unsigned int pmap_free_pages()
{
	return atop(avail_remaining);
}

/* Always returns page-aligned regions.  */
boolean_t
init_alloc_aligned(vm_size_t size, vm_offset_t *addrp)
{
	vm_offset_t addr;
	extern char start[], end[];
	int i;

	/* Memory regions to skip.  */
	vm_offset_t boot_info_start_pa = kvtophys(boot_info);
	vm_offset_t boot_info_end_pa = boot_info_start_pa + sizeof(*boot_info);
	vm_offset_t cmdline_start_pa = boot_info->flags & MULTIBOOT_CMDLINE
		? boot_info->cmdline : 0;
	vm_offset_t cmdline_end_pa = cmdline_start_pa
		? cmdline_start_pa+strlen((char*)phystokv(cmdline_start_pa))+1
		: 0;
	vm_offset_t mods_start_pa = boot_info->flags & MULTIBOOT_MODS
		? boot_info->mods_addr : 0;
	vm_offset_t mods_end_pa = mods_start_pa
		? mods_start_pa
		  + boot_info->mods_count * sizeof(struct multiboot_module)
		: 0;

	retry:

	/* Page-align the start address.  */
	avail_next = round_page(avail_next);

	/* Check if we have reached the end of memory.  */
	if (avail_next == phys_last_addr)
		return FALSE;

	/* Tentatively assign the current location to the caller.  */
	addr = avail_next;

	/* Bump the pointer past the newly allocated region
	   and see where that puts us.  */
	avail_next += size;

	/* Skip past the I/O and ROM area.  */
	if ((avail_next > (boot_info->mem_lower * 0x400)) && (addr < 0x100000))
	{
		avail_next = 0x100000;
		goto retry;
	}

	/* If we're only supposed to use the low 16 megs, enforce that.  */
	if ((!use_all_mem) && (addr >= 16 * 1024*1024)) {
		return FALSE;
	}

	/* Skip our own kernel code, data, and bss.  */
	if ((avail_next >= (vm_offset_t)start) && (addr < (vm_offset_t)end))
	{
		avail_next = (vm_offset_t)end;
		goto retry;
	}

	/* Skip any areas occupied by valuable boot_info data.  */
	if ((avail_next > boot_info_start_pa) && (addr < boot_info_end_pa))
	{
		avail_next = boot_info_end_pa;
		goto retry;
	}
	if ((avail_next > cmdline_start_pa) && (addr < cmdline_end_pa))
	{
		avail_next = cmdline_end_pa;
		goto retry;
	}
	if ((avail_next > mods_start_pa) && (addr < mods_end_pa))
	{
		avail_next = mods_end_pa;
		goto retry;
	}
	if ((avail_next > kern_sym_start) && (addr < kern_sym_end))
	{
		avail_next = kern_sym_end;
		goto retry;
	}
	if (boot_info->flags & MULTIBOOT_MODS)
	{
		struct multiboot_module *m = (struct multiboot_module *)
			phystokv(boot_info->mods_addr);
		for (i = 0; i < boot_info->mods_count; i++)
		{
			if ((avail_next > m[i].mod_start)
			    && (addr < m[i].mod_end))
			{
				avail_next = m[i].mod_end;
				goto retry;
			}
			/* XXX string */
		}
	}

	avail_remaining -= size;

	*addrp = addr;
	return TRUE;
}

boolean_t pmap_next_page(addrp)
	vm_offset_t *addrp;
{
	return init_alloc_aligned(PAGE_SIZE, addrp);
}

/* Grab a physical page:
   the standard memory allocation mechanism
   during system initialization.  */
vm_offset_t
pmap_grab_page()
{
	vm_offset_t addr;
	if (!pmap_next_page(&addr))
		panic("Not enough memory to initialize Mach");
	return addr;
}

boolean_t pmap_valid_page(x)
	vm_offset_t x;
{
	/* XXX is this OK?  What does it matter for?  */
	return (((phys_first_addr <= x) && (x < phys_last_addr)) &&
		!(((boot_info->mem_lower * 1024) <= x) && (x < 1024*1024)));
}

#ifndef NBBY
#define NBBY	8
#endif
#ifndef NBPW
#define NBPW	(NBBY * sizeof(int))
#endif
#define DMA_MAX	(16*1024*1024)

/*
 * Allocate contiguous pages below 16 MB
 * starting at specified boundary for DMA.
 */
vm_offset_t
alloc_dma_mem(size, align)
	vm_size_t size;
	vm_offset_t align;
{
	int *bits, i, j, k, n;
	int npages, count, bit, mask;
	int first_page, last_page;
	vm_offset_t addr;
	vm_page_t p, prevp;

	npages = round_page(size) / PAGE_SIZE;
	mask = align ? (align - 1) / PAGE_SIZE : 0;

	/*
	 * Allocate bit array.
	 */
	n = ((DMA_MAX / PAGE_SIZE) + NBPW - 1) / NBPW;
	i = n * NBPW;
	bits = (unsigned *)kalloc(i);
	if (bits == 0) {
		printf("alloc_dma_mem: unable alloc bit array\n");
		return (0);
	}
	bzero((char *)bits, i);

	/*
	 * Walk the page free list and set a bit for
	 * every usable page in bit array.
	 */
	simple_lock(&vm_page_queue_free_lock);
	for (p = vm_page_queue_free; p; p = (vm_page_t)p->pageq.next) {
		if (p->phys_addr < DMA_MAX) {
			i = p->phys_addr / PAGE_SIZE;
			bits[i / NBPW] |= 1 << (i % NBPW);
		}
	}

	/*
	 * Search for contiguous pages by scanning bit array.
	 */
	for (i = 0, first_page = -1; i < n; i++) {
		for (bit = 1, j = 0; j < NBPW; j++, bit <<= 1) {
			if (bits[i] & bit) {
				if (first_page < 0) {
					k = i * NBPW + j;
					if (!mask
					    || (((k & mask) + npages)
						<= mask + 1)) {
						first_page = k;
						if (npages == 1)
							goto found;
						count = 1;
					}
				} else if (++count == npages)
					goto found;
			} else
				first_page = -1;
		}
	}
	addr = 0;
	goto out;

 found:
	/*
	 * Remove pages from the free list.
	 */
	addr = first_page * PAGE_SIZE;
	last_page = first_page + npages;
	vm_page_free_count -= npages;
	p = vm_page_queue_free;
	prevp = 0;
	while (1) {
		i = p->phys_addr / PAGE_SIZE;
		if (i >= first_page && i < last_page) {
			if (prevp)
				prevp->pageq.next = p->pageq.next;
			else
				vm_page_queue_free = (vm_page_t)p->pageq.next;
			p->free = FALSE;
			if (--npages == 0)
				break;
		} else
			prevp = p;
		p = (vm_page_t)p->pageq.next;
	}

 out:
	simple_unlock(&vm_page_queue_free_lock);
	kfree((vm_offset_t)bits, n * NBPW);
	return (addr);
}
