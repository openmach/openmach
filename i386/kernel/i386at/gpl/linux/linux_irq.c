/*
 * Linux IRQ management.
 * Copyright (C) 1995 Shantanu Goel.
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
 */

/*
 *	linux/arch/i386/kernel/irq.c
 *
 *	Copyright (C) 1992 Linus Torvalds
 */

#include <sys/types.h>

#include <kern/assert.h>

#include <i386/machspl.h>
#include <i386/ipl.h>
#include <i386/pic.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/irq.h>

/*
 * Priority at which a Linux handler should be called.
 * This is used at the time of an IRQ allocation.  It is
 * set by emulation routines for each class of device.
 */
spl_t linux_intr_pri;

/*
 * Flag indicating an interrupt is being handled.
 */
unsigned long intr_count = 0;

/*
 * List of Linux interrupt handlers.
 */
static void (*linux_handlers[16])(int, struct pt_regs *);

extern spl_t curr_ipl;
extern int curr_pic_mask;
extern int pic_mask[];

extern int intnull(), prtnull();

/*
 * Generic interrupt handler for Linux devices.
 * Set up a fake `struct pt_regs' then call the real handler.
 */
static int
linux_intr(irq)
	int irq;
{
	struct pt_regs regs;

	kstat.interrupts[irq]++;
	intr_count++;
	(*linux_handlers[irq])(irq, &regs);
	intr_count--;
}

/*
 * Mask an IRQ.
 */
void
disable_irq(irq)
	unsigned int irq;
{
	int i, flags;

	assert (irq < NR_IRQS);

	save_flags(flags);
	cli();
	for (i = 0; i < intpri[irq]; i++)
		pic_mask[i] |= 1 << irq;
	if (curr_pic_mask != pic_mask[curr_ipl]) {
		curr_pic_mask = pic_mask[curr_ipl];
		outb(PIC_MASTER_OCW, curr_pic_mask);
		outb(PIC_SLAVE_OCW, curr_pic_mask >> 8);
	}
	restore_flags(flags);
}

/*
 * Unmask an IRQ.
 */
void
enable_irq(irq)
	unsigned int irq;
{
	int mask, i, flags;

	assert (irq < NR_IRQS);

	mask = 1 << irq;
	if (irq >= 8)
		mask |= 1 << 2;
	save_flags(flags);
	cli();
	for (i = 0; i < intpri[irq]; i++)
		pic_mask[i] &= ~mask;
	if (curr_pic_mask != pic_mask[curr_ipl]) {
		curr_pic_mask = pic_mask[curr_ipl];
		outb(PIC_MASTER_OCW, curr_pic_mask);
		outb(PIC_SLAVE_OCW, curr_pic_mask >> 8);
	}
	restore_flags(flags);
}

/*
 * Attach a handler to an IRQ.
 */
int
request_irq(unsigned int irq, void (*handler)(int, struct pt_regs *),
	    unsigned long flags, const char *device)
{
	assert(irq < 16);

	if (ivect[irq] == intnull || ivect[irq] == prtnull) {
		if (!handler)
			return (-LINUX_EINVAL);
		linux_handlers[irq] = handler;
		ivect[irq] = linux_intr;
		iunit[irq] = irq;
		intpri[irq] = linux_intr_pri;
		enable_irq(irq);
		return (0);
	}
	return (-LINUX_EBUSY);
}

/*
 * Deallocate an irq.
 */
void
free_irq(unsigned int irq)
{
	if (irq > 15)
		panic("free_irq: bad irq number");

	disable_irq(irq);
	ivect[irq] = (irq == 7) ? prtnull : intnull;
	iunit[irq] = irq;
	intpri[irq] = SPL0;
}

/*
 * IRQ probe interrupt handler.
 */
void
probe_intr(irq)
	int irq;
{
	disable_irq(irq);
}

/*
 * Set for an irq probe.
 */
unsigned long
probe_irq_on()
{
	unsigned i, irqs = 0;
	unsigned long delay;

	assert (curr_ipl == 0);

	/*
	 * Allocate all available IRQs.
	 */
	for (i = 15; i > 0; i--)
		if (request_irq(i, probe_intr, 0, "probe") == 0)
			irqs |= 1 << i;

	/*
	 * Wait for spurious interrupts to mask themselves out.
	 */
	for (delay = jiffies + 2; delay > jiffies; )
		;

	/*
	 * Free IRQs that caused spurious interrupts.
	 */
	for (i = 15; i > 0; i--) {
		if (irqs & (1 << i) & pic_mask[0]) {
			irqs ^= 1 << i;
			free_irq(i);
		}
	}

	return (irqs);
}

/*
 * Return the result of an irq probe.
 */
int
probe_irq_off(unsigned long irqs)
{
	unsigned i, irqs_save = irqs;

	assert (curr_ipl == 0);

	irqs &= pic_mask[0];

	/*
	 * Deallocate IRQs.
	 */
	for (i = 15; i > 0; i--)
		if (irqs_save & (1 << i))
			free_irq(i);

	/*
	 * Return IRQ number.
	 */
	if (!irqs)
		return (0);
	i = ffz(~irqs);
	if (irqs != (irqs & (1 << i)))
		i = -i;
	return (i);
}
