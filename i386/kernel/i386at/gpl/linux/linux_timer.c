/*
 * Linux timers.
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
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/kernel/sched.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <asm/system.h>

unsigned long volatile jiffies = 0;

/*
 * Mask of active timers.
 */
unsigned long timer_active = 0;

/*
 * List of timeout routines.
 */
struct timer_struct timer_table[32];

/*
 * The head for the timer-list has a "expires" field of MAX_UINT,
 * and the sorting routine counts on this..
 */
static struct timer_list timer_head =
{
  &timer_head, &timer_head, ~0, 0, NULL
};

#define SLOW_BUT_DEBUGGING_TIMERS 0

void
add_timer(struct timer_list *timer)
{
	unsigned long flags;
	struct timer_list *p;

#if SLOW_BUT_DEBUGGING_TIMERS
	if (timer->next || timer->prev) {
		printk("add_timer() called with non-zero list from %p\n",
			__builtin_return_address(0));
		return;
	}
#endif
	p = &timer_head;
	save_flags(flags);
	cli();
	do {
		p = p->next;
	} while (timer->expires > p->expires);
	timer->next = p;
	timer->prev = p->prev;
	p->prev = timer;
	timer->prev->next = timer;
	restore_flags(flags);
}

int
del_timer(struct timer_list *timer)
{
	unsigned long flags;
#if SLOW_BUT_DEBUGGING_TIMERS
	struct timer_list * p;

	p = &timer_head;
	save_flags(flags);
	cli();
	while ((p = p->next) != &timer_head) {
		if (p == timer) {
			timer->next->prev = timer->prev;
			timer->prev->next = timer->next;
			timer->next = timer->prev = NULL;
			restore_flags(flags);
			return 1;
		}
	}
	if (timer->next || timer->prev)
		printk("del_timer() called from %p with timer not initialized\n",
			__builtin_return_address(0));
	restore_flags(flags);
	return 0;
#else
	struct timer_list * next;
	int ret = 0;
	save_flags(flags);
	cli();
	if ((next = timer->next) != NULL) {
		(next->prev = timer->prev)->next = next;
		timer->next = timer->prev = NULL;
		ret = 1;
	}
	restore_flags(flags);
	return ret;
#endif
}

/*
 * Timer software interrupt handler.
 */
void
timer_bh()
{
	unsigned long mask;
	struct timer_struct *tp;
	struct timer_list * timer;

	cli();
	while ((timer = timer_head.next) != &timer_head
	       && timer->expires <= jiffies) {
		void (*fn)(unsigned long) = timer->function;
		unsigned long data = timer->data;

		timer->next->prev = timer->prev;
		timer->prev->next = timer->next;
		timer->next = timer->prev = NULL;
		sti();
		fn(data);
		cli();
	}
	sti();

	for (mask = 1, tp = timer_table; mask; tp++, mask <<= 1) {
		if (mask > timer_active)
			break;
		if ((mask & timer_active)
		    && tp->expires > jiffies) {
			timer_active &= ~mask;
			(*tp->fn)();
			sti();
		}
	}
}

int linux_timer_print = 0;

/*
 * Timer interrupt handler.
 */
void
linux_timer_intr()
{
	unsigned long mask;
	struct timer_struct *tp;
	extern int pic_mask[];

	jiffies++;

	for (mask = 1, tp = timer_table; mask; tp++, mask += mask) {
		if (mask > timer_active)
			break;
		if (!(mask & timer_active))
			continue;
		if (tp->expires > jiffies)
			continue;
		mark_bh(TIMER_BH);
	}
	if (timer_head.next->expires <= jiffies)
		mark_bh(TIMER_BH);
	if (tq_timer != &tq_last)
		mark_bh(TQUEUE_BH);
	if (linux_timer_print)
		printf ("linux_timer_intr: pic_mask[0] %x\n", pic_mask[0]);
}

