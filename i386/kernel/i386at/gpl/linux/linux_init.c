/*
 * Linux initialization.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 	Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/init/main.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>

#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>

#include <vm/vm_page.h>

#include <i386/ipl.h>
#include <i386/pic.h>
#include <i386/pit.h>
#include <i386/machspl.h>
#include <i386/pmap.h>
#include <i386/vm_param.h>

#include <i386at/gpl/linux/linux_emul.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/string.h>

#include <asm/system.h>

/*
 * Set if the machine has an EISA bus.
 */
int EISA_bus = 0;

/*
 * Timing loop count.
 */
unsigned long loops_per_sec = 1;

/*
 * End of physical memory.
 */
unsigned long high_memory;

/*
 * Flag to indicate auto-configuration is in progress.
 */
int linux_auto_config = 1;

/*
 * Hard drive parameters obtained from the BIOS.
 */
struct drive_info_struct {
	char dummy[32];
} drive_info;

/*
 * Forward declarations.
 */
static void calibrate_delay(void);

extern int hz;
extern vm_offset_t phys_last_addr;

extern void timer_bh(void *);
extern void tqueue_bh(void *);
extern void startrtclock(void);
extern void linux_version_init(void);
extern void linux_kmem_init(void);
extern unsigned long pci_init(unsigned long, unsigned long);
extern void linux_net_emulation_init (void);
extern void device_setup(void);
extern void linux_printk(char *, ...);
extern int linux_timer_intr();

/*
 * Amount of contiguous memory to allocate for initialization.
 */
#define CONTIG_ALLOC (512 * 1024)

/*
 * Initialize Linux drivers.
 */
void
linux_init()
{
	char *p;
	int i, addr;
	int (*old_clock_handler)(), old_clock_pri;
	unsigned memory_start, memory_end;
	vm_page_t pages;

	/*
	 * Initialize memory size.
	 */
	high_memory = phys_last_addr;

	/*
	 * Ensure interrupts are disabled.
	 */
	(void) splhigh();

	/*
	 * Program counter 0 of 8253 to interrupt hz times per second.
	 */
	outb(PITCTL_PORT, PIT_C0|PIT_SQUAREMODE|PIT_READMODE);
	outb(PITCTR0_PORT, CLKNUM / hz);
	outb(PITCTR0_PORT, (CLKNUM / hz) >> 8);

	/*
	 * Install our clock interrupt handler.
	 */
	old_clock_handler = ivect[0];
	old_clock_pri = intpri[0];
	ivect[0] = linux_timer_intr;
	intpri[0] = SPLHI;
	form_pic_mask();

	/*
	 * Enable interrupts.
	 */
	(void) spl0();

	/*
	 * Set Linux version.
	 */
	linux_version_init();

	/*
	 * Check if the machine has an EISA bus.
	 */
	p = (char *)0x0FFFD9;
	if (*p++ == 'E' && *p++ == 'I' && *p++ == 'S' && *p == 'A')
		EISA_bus = 1;

	/*
	 * Permanently allocate standard device ports.
	 */
	request_region(0x00, 0x20, "dma1");
	request_region(0x40, 0x20, "timer");
	request_region(0x70, 0x10, "rtc");
	request_region(0x80, 0x20, "dma page reg");
	request_region(0xc0, 0x20, "dma2");
	request_region(0xf0, 0x02, "fpu");
	request_region(0xf8, 0x08, "fpu");

	/*
	 * Install software interrupt handlers.
	 */
	bh_base[TIMER_BH].routine = timer_bh;
	bh_base[TIMER_BH].data = 0;
	enable_bh(TIMER_BH);
	bh_base[TQUEUE_BH].routine = tqueue_bh;
	bh_base[TQUEUE_BH].data = 0;
	enable_bh(TQUEUE_BH);

	/*
	 * Set loop count.
	 */
	calibrate_delay();

	/*
	 * Initialize drive info.
	 */
	addr = *((unsigned *)phystokv(0x104));
	memcpy (&drive_info,
		(void *)((addr & 0xffff) + ((addr >> 12) & 0xffff0)), 16);
	addr = *((unsigned *)phystokv(0x118));
	memcpy ((char *)&drive_info + 16,
		(void *)((addr & 0xffff) + ((addr >> 12) & 0xffff0)), 16);

	/*
	 * Initialize Linux memory allocator.
	 */
	linux_kmem_init();

	/*
	 * Allocate contiguous memory below 16 MB.
	 */
	memory_start = (unsigned long)alloc_contig_mem(CONTIG_ALLOC,
						       16 * 1024 * 1024,
						       0, &pages);
	if (memory_start == 0)
		panic("linux_init: alloc_contig_mem failed");
	memory_end = memory_start + CONTIG_ALLOC;

	/*
	 * Initialize PCI bus.
	 */
	memory_start = pci_init(memory_start, memory_end);

	if (memory_start > memory_end)
		panic("linux_init: ran out memory");

	/*
	 * Free unused memory.
	 */
	while (pages && pages->phys_addr < round_page(memory_start))
		pages = (vm_page_t)pages->pageq.next;
	if (pages)
		free_contig_mem(pages);

	/*
	 * Initialize devices.
	 */
	linux_net_emulation_init();
	cli();
	device_setup();

	/*
	 * Disable interrupts.
	 */
	(void) splhigh();

	/*
	 * Restore clock interrupt handler.
	 */
	ivect[0] = old_clock_handler;
	intpri[0] = old_clock_pri;
	form_pic_mask();

	linux_auto_config = 0;
}

#ifndef NBPW
#define NBPW 32
#endif

/*
 * Allocate contiguous memory with the given constraints.
 * This routine is horribly inefficient but it is presently
 * only used during initialization so it's not that bad.
 */
void *
alloc_contig_mem(unsigned size, unsigned limit,
		 unsigned mask, vm_page_t *pages)
{
	int i, j, bits_len;
	unsigned *bits, len;
	void *m;
	vm_page_t p, page_list, tail, prev;
	vm_offset_t addr, max_addr;

	if (size == 0)
		return (NULL);
	size = round_page(size);
	if ((size >> PAGE_SHIFT) > vm_page_free_count)
		return (NULL);

	/* Allocate bit array.  */
	max_addr = phys_last_addr;
	if (max_addr > limit)
		max_addr = limit;
	bits_len = ((((max_addr >> PAGE_SHIFT) + NBPW - 1) / NBPW)
		    * sizeof(unsigned));
	bits = (unsigned *)kalloc(bits_len);
	if (!bits)
		return (NULL);
	memset (bits, 0, bits_len);

	/*
	 * Walk the page free list and set a bit for every usable page.
	 */
	simple_lock(&vm_page_queue_free_lock);
	p = vm_page_queue_free;
	while (p) {
		if (p->phys_addr < limit)
			(bits[(p->phys_addr >> PAGE_SHIFT) / NBPW]
			 |= 1 << ((p->phys_addr >> PAGE_SHIFT) % NBPW));
		p = (vm_page_t)p->pageq.next;
	}

	/*
	 * Scan bit array for contiguous pages.
	 */
	len = 0;
	m = NULL;
	for (i = 0; len < size && i < bits_len / sizeof (unsigned); i++)
		for (j = 0; len < size && j < NBPW; j++)
			if (!(bits[i] & (1 << j))) {
				len = 0;
				m = NULL;
			} else {
				if (len == 0) {
					addr = ((vm_offset_t)(i * NBPW + j)
						<< PAGE_SHIFT);
					if ((addr & mask) == 0) {
						len += PAGE_SIZE;
						m = (void *) addr;
					}
				} else
					len += PAGE_SIZE;
			}

	if (len != size) {
		simple_unlock(&vm_page_queue_free_lock);
		kfree ((vm_offset_t)bits, bits_len);
		return (NULL);
	}

	/*
	 * Remove pages from free list
	 * and construct list to return to caller.
	 */
	page_list = NULL;
	for (len = 0; len < size; len += PAGE_SIZE, addr += PAGE_SIZE) {
		prev = NULL;
		for (p = vm_page_queue_free; p; p = (vm_page_t)p->pageq.next) {
			if (p->phys_addr == addr)
				break;
			prev = p;
		}
		if (!p)
			panic("alloc_contig_mem: page not on free list");
		if (prev)
			prev->pageq.next = p->pageq.next;
		else
			vm_page_queue_free = (vm_page_t)p->pageq.next;
		p->free = FALSE;
		p->pageq.next = NULL;
		if (!page_list)
			page_list = tail = p;
		else {
			tail->pageq.next = (queue_entry_t)p;
			tail = p;
		}
		vm_page_free_count--;
	}

	simple_unlock(&vm_page_queue_free_lock);
	kfree((vm_offset_t)bits, bits_len);
	if (pages)
		*pages = page_list;
	return (m);
}

/*
 * Free memory allocated by alloc_contig_mem.
 */
void
free_contig_mem(vm_page_t pages)
{
  int i;
  vm_page_t p;

  for (p = pages, i = 0; p->pageq.next; p = (vm_page_t)p->pageq.next, i++)
    p->free = TRUE;
  p->free = TRUE;
  simple_lock(&vm_page_queue_free_lock);
  vm_page_free_count += i + 1;
  p->pageq.next = (queue_entry_t)vm_page_queue_free;
  vm_page_queue_free = pages;
  simple_unlock(&vm_page_queue_free_lock);
}

/*
 * Calibrate delay loop.
 * Lifted straight from Linux.
 */
static void
calibrate_delay()
{
	int ticks;

	printk("Calibrating delay loop.. ");
	while (loops_per_sec <<= 1) {
		/* Wait for "start of" clock tick.  */
		ticks = jiffies;
		while (ticks == jiffies)
			/* nothing */;
		/* Go .. */
		ticks = jiffies;
		__delay(loops_per_sec);
		ticks = jiffies - ticks;
		if (ticks >= hz) {
			loops_per_sec = muldiv(loops_per_sec,
						     hz, ticks);
			printk("ok - %lu.%02lu BogoMips\n",
			       loops_per_sec / 500000,
			       (loops_per_sec / 5000) % 100);
			return;
		}
	}
	printk("failed\n");
}
