/*
 * Linux auto-irq support.
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
 *  Written 1994 by Donald Becker.
 *
 *  The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 *  Center of Excellence in Space Data and Information Sciences
 *    Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 *  This code is a general-purpose IRQ line detector for devices with
 *  jumpered IRQ lines.  If you can make the device raise an IRQ (and
 *  that IRQ line isn't already being used), these routines will tell
 *  you what IRQ line it's using -- perfect for those oh-so-cool boot-time
 *  device probes!
 *
 *  To use this, first call autoirq_setup(timeout). TIMEOUT is how many
 *  'jiffies' (1/100 sec.) to detect other devices that have active IRQ lines,
 *  and can usually be zero at boot.  'autoirq_setup()' returns the bit
 *  vector of nominally-available IRQ lines (lines may be physically in-use,
 *  but not yet registered to a device).
 *  Next, set up your device to trigger an interrupt.
 *  Finally call autoirq_report(TIMEOUT) to find out which IRQ line was
 *  most recently active.  The TIMEOUT should usually be zero, but may
 *  be set to the number of jiffies to wait for a slow device to raise an IRQ.
 *
 *  The idea of using the setup timeout to filter out bogus IRQs came from
 *  the serial driver.
 */

#include <i386/pic.h>
#include <i386/ipl.h>

#include <linux/sched.h>
#include <linux/ptrace.h>

#include <asm/bitops.h>
#include <asm/system.h>

/*
 * IRQ to network device map.
 */
void *irq2dev_map[16];

/*
 * Set of fixed IRQs
 * (fpu, rtc, com1, PIC slave cascade, keyboard, timer).
 */
int irqs_busy = 0x2147;

static volatile int irq_number;	/* latest irq found */
static volatile int irq_bitmap;	/* bitmap of IRQs found */
static int irq_handled;		/* irq lines we have a handler on */

extern unsigned long loops_per_sec;

/*
 * Interrupt handler when probing an IRQ.
 */
static void
autoirq_probe(irq)
	int irq;
{
	/*
	 * Mark this IRQ as the last one
	 * that interrupted and disable it.
	 */
	irq_number = irq;
	set_bit(irq, (void *)&irq_bitmap);
	disable_irq(irq);
}

/*
 * Set up for auto-irq.
 */
int
autoirq_setup(waittime)
	int waittime;
{
	int i, mask;
	int timeout = jiffies + waittime;
	int boguscount = (waittime * loops_per_sec) / 100;

	/*
	 * Allocate all possible IRQs.
	 */
	irq_handled = 0;
	for (i = 0; i < 16; i++) {
		if (test_bit(i, (void *)&irqs_busy) == 0
		    && request_irq(i, autoirq_probe, 0, 0) == 0)
			set_bit(i, (void *)&irq_handled);
	}

	irq_number = 0;
	irq_bitmap = 0;

	/*
	 * Hang out at least <waittime>
	 * jiffies waiting for bogus IRQ hits.
	 */
	while (timeout > jiffies && --boguscount > 0)
		;

	/*
	 * Free IRQs that caused bogus hits.
	 */
	for (i = 0, mask = 0x01; i < 16; i++, mask <<= 1) {
		if (irq_bitmap & irq_handled & mask) {
			irq_handled &= ~mask;
			free_irq(i);
		}
	}

	return (irq_handled);
}

/*
 * Return the last IRQ that caused an interrupt.
 */
int
autoirq_report(waittime)
	int waittime;
{
	int i;
	int timeout = jiffies + waittime;
	int boguscount = (waittime * loops_per_sec) / 100;

	/*
	 * Hang out at least <waittime>
	 * jiffies waiting for the IRQ.
	 */
	while (timeout > jiffies && --boguscount > 0)
		if (irq_number)
			break;

	/*
	 * Retract the IRQ handlers that we handled.
	 */
	for (i = 0; i < 16; i++) {
		if (test_bit(i, (void *)&irq_handled))
			free_irq(i);
	}

	return (irq_number);
}
