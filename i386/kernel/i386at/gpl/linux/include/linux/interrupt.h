/* interrupt.h */
#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

#include <linux/kernel.h>
#include <asm/bitops.h>

struct bh_struct {
	void (*routine)(void *);
	void *data;
};

extern unsigned long bh_active;
extern unsigned long bh_mask;
extern struct bh_struct bh_base[32];

asmlinkage void do_bottom_half(void);

/* Who gets which entry in bh_base.  Things which will occur most often
   should come first - in which case NET should be up the top with SERIAL/TQUEUE! */
   
enum {
	TIMER_BH = 0,
	CONSOLE_BH,
	TQUEUE_BH,
	SERIAL_BH,
	NET_BH,
	IMMEDIATE_BH,
	KEYBOARD_BH,
	CYCLADES_BH,
	CM206_BH
};

extern inline void mark_bh(int nr)
{
	set_bit(nr, &bh_active);
}

extern inline void disable_bh(int nr)
{
	clear_bit(nr, &bh_mask);
}

extern inline void enable_bh(int nr)
{
	set_bit(nr, &bh_mask);
}

extern inline void start_bh_atomic(void)
{
	intr_count++;
	barrier();
}

extern inline void end_bh_atomic(void)
{
	barrier();
	intr_count--;
}

/*
 * Autoprobing for irqs:
 *
 * probe_irq_on() and probe_irq_off() provide robust primitives
 * for accurate IRQ probing during kernel initialization.  They are
 * reasonably simple to use, are not "fooled" by spurious interrupts,
 * and, unlike other attempts at IRQ probing, they do not get hung on
 * stuck interrupts (such as unused PS2 mouse interfaces on ASUS boards).
 *
 * For reasonably foolproof probing, use them as follows:
 *
 * 1. clear and/or mask the device's internal interrupt.
 * 2. sti();
 * 3. irqs = probe_irq_on();      // "take over" all unassigned idle IRQs
 * 4. enable the device and cause it to trigger an interrupt.
 * 5. wait for the device to interrupt, using non-intrusive polling or a delay.
 * 6. irq = probe_irq_off(irqs);  // get IRQ number, 0=none, negative=multiple
 * 7. service the device to clear its pending interrupt.
 * 8. loop again if paranoia is required.
 *
 * probe_irq_on() returns a mask of allocated irq's.
 *
 * probe_irq_off() takes the mask as a parameter,
 * and returns the irq number which occurred,
 * or zero if none occurred, or a negative irq number
 * if more than one irq occurred.
 */
extern unsigned long probe_irq_on(void);	/* returns 0 on failure */
extern int probe_irq_off(unsigned long);	/* returns 0 or negative on failure */

#endif
