/*
 * Linux software interrupts.
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
 *	linux/kernel/softirq.c
 *
 *	Copyright (C) 1992 Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/system.h>
#include <asm/bitops.h>

/*
 * Mask of pending interrupts.
 */
unsigned long bh_active = 0;

/*
 * Mask of enabled interrupts.
 */
unsigned long bh_mask = 0;

/*
 * List of software interrupt handlers.
 */
struct bh_struct bh_base[32];


/*
 * Software interrupt handler.
 */
void
linux_soft_intr()
{
	unsigned long active;
	unsigned long mask, left;
	struct bh_struct *bh;

	bh = bh_base;
	active = bh_active & bh_mask;
	for (mask = 1, left = ~0;
	     left & active; bh++, mask += mask, left += left) {
		if (mask & active) {
			void (*fn)(void *);

			bh_active &= ~mask;
			fn = bh->routine;
			if (fn == 0)
				goto bad_bh;
			(*fn)(bh->data);
		}
	}
	return;
 bad_bh:
	printf("linux_soft_intr: bad interrupt handler entry 0x%08lx\n", mask);
}
