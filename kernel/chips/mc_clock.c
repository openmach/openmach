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
 *	File:	mc_clock.c
 *	Author: Alessandro Forin
 *	Date:	8/90
 *
 *	Driver for the MC146818 Clock
 */

#include <mc.h>
#if	NMC > 0
#include <platforms.h>

#include <mach/std_types.h>
#include <machine/machspl.h>		/* spl definitions */
#include <chips/busses.h>

#include <sys/time.h>
#include <kern/time_out.h>
#include <chips/mc_clock.h>

#ifdef	DECSTATION
#include <mips/mips_cpu.h>
#include <mips/clock.h>
#endif	/*DECSTATION*/

#ifdef	FLAMINGO
#include <alpha/clock.h>
#endif	/*FLAMINGO*/

#define private static
#define public


/* Architecture-specific defines */

#ifdef	DECSTATION

#define	MC_DEFAULT_ADDRESS	(mc_clock_ram_t *)PHYS_TO_K1SEG(0x1d000000)
#define	MC_DOES_DELAYS		1

/*
 * Both the Pmax and the 3max implementations of the chip map
 * bytes of the chip's RAM to 32 bit words (low byte).
 * For convenience, we redefine here the chip's RAM layout
 * making padding explicit. 
 */

typedef struct {
	volatile unsigned char	mc_second;
								char pad0[3];
	volatile unsigned char	mc_alarm_second;
								char pad1[3];
	volatile unsigned char	mc_minute;
								char pad2[3];
	volatile unsigned char	mc_alarm_minute;
								char pad3[3];
	volatile unsigned char	mc_hour;
								char pad4[3];
	volatile unsigned char	mc_alarm_hour;
								char pad5[3];
	volatile unsigned char	mc_day_of_week;
								char pad6[3];
	volatile unsigned char	mc_day_of_month;
								char pad7[3];
	volatile unsigned char	mc_month;
								char pad8[3];
	volatile unsigned char	mc_year;
								char pad9[3];
	volatile unsigned char	mc_register_A;
								char pad10[3];
	volatile unsigned char	mc_register_B;
								char pad11[3];
	volatile unsigned char	mc_register_C;
								char pad12[3];
	volatile unsigned char	mc_register_D;
								char pad13[3];
	unsigned char		mc_non_volatile_ram[50 * 4];	/* unused */
} mc_clock_ram_t;

#define	MC_CLOCK_PADDED	1

#endif	/*DECSTATION*/


#ifdef	FLAMINGO
#define	MC_DEFAULT_ADDRESS	0L

/* padded, later */

#endif	/* FLAMINGO */



#ifndef	MC_CLOCK_PADDED
typedef mc_clock_t mc_clock_ram_t;	/* No padding needed */
#endif

/*
 * Functions provided herein
 */
int mc_probe( vm_offset_t addr, struct bus_ctlr * );
private void mc_attach();

int mc_intr();

void mc_open(), mc_close(), mc_write();
private unsigned int mc_read();

private void mc_wait_for_uip( mc_clock_ram_t *clock );


/*
 * Status
 */
boolean_t mc_running = FALSE;
boolean_t mc_new_century = FALSE;	/* "year" info overfloweth */

private int days_per_month[12] = {
	31, 28,	31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

private unsigned int	mc_read();		/* forward */
private void		mc_wait_for_uip();

/*
 * Where is the chip's RAM mapped
 */
private mc_clock_ram_t	*rt_clock = MC_DEFAULT_ADDRESS;

/*
 * (Auto?)Configuration
 */
private vm_offset_t mc_std[NMC] = { 0 };
private struct bus_device *mc_info[NMC];

struct bus_driver mc_driver =
       { mc_probe, 0, mc_attach, 0, mc_std, "mc", mc_info, };


mc_probe(vm_offset_t addr, struct bus_ctlr *ui)
{
	rt_clock = (mc_clock_ram_t *)addr;
	return 1;
}

private void
mc_attach()
{
	printf(": MC146818 or like Time-Of-Year chip");
}

/*
 *	Interrupt routine
 */
#if	MC_DOES_DELAYS

private int		config_step = 3;
private volatile int	had_intr;

mc_intr(spllevel)
	spl_t	spllevel;
{
	/*
	 * Interrupt flags are read-to-clear.
	 */
	if (config_step > 2)
		return (rt_clock->mc_register_C & MC_REG_C_IRQF);
	had_intr = (rt_clock->mc_register_C & MC_REG_C_IRQF) ? 1 : 0;
	if (config_step++ == 0)
		accurate_config_delay(spllevel);
	return had_intr;
}
#else	/* MC_DOES_DELAYS */

mc_intr()
{
	return (rt_clock->mc_register_C);	/* clear intr */
}

#endif	/* MC_DOES_DELAYS */

/*
 * Start real-time clock.
 */
void
mc_open()
{
	/*
	 * All we should need to do is to enable interrupts, but
	 * since we do not know what OS last ran on this box
	 * we'll reset it all over again.  Just kidding..
	 */
	unsigned	unix_seconds_now;

	/*
	 * Check for battery backup power.  If we do not have it,
	 * warn the user.  Time will be bogus only after power up.
	 */
	if ((rt_clock->mc_register_D & MC_REG_D_VRT) == 0)
		printf("WARNING: clock batteries are low\n");

	/*
	 * Read the current time settings, check if the year info
	 * has been screwed up.  
	 */
	unix_seconds_now = mc_read();

	if (unix_seconds_now < (SECYR * (1990 - YRREF)))
		printf("The prom has clobbered the clock\n");

	time.tv_sec = (long)unix_seconds_now;
	mc_write();

	mc_running = TRUE;
}

void
mc_close()
{
	/*
	 * Disable interrupts, but keep the chip running.
	 * Note we are called at splhigh and an interrupt
	 * might be pending already.
	 */

	mc_intr(0);
	rt_clock->mc_register_B &= ~(MC_REG_B_UIE|MC_REG_B_AIE|MC_REG_B_PIE);
	mc_running = FALSE;
#if	MC_DOES_DELAYS
	config_step = 0;
#endif
}


/*
 * Set time-of-day.  Must be called at splhigh()
 */
void
mc_write()
{
	register mc_clock_ram_t *clock = rt_clock;
	register unsigned years, months, days, hours, minutes, seconds;
	register unsigned unix_seconds = time.tv_sec;
	int             frequence_selector, temp;
	int             bogus_hz = 0;

	/*
	 * Convert U*x time into absolute time 
	 */

	years = YRREF;
	while (1) {
		seconds = SECYR;
		if (LEAPYEAR(years))
			seconds += SECDAY;
		if (unix_seconds < seconds)
			break;
		unix_seconds -= seconds;
		years++;
	}

	months = 0;
	while (1) {
		seconds = days_per_month[months++] * SECDAY;
		if (months == 2 /* February */ && LEAPYEAR(years))
			seconds += SECDAY;
		if (unix_seconds < seconds)
			break;
		unix_seconds -= seconds;
	}

	days = unix_seconds / SECDAY;
	unix_seconds -= SECDAY * days++;

	hours = unix_seconds / SECHOUR;
	unix_seconds -= SECHOUR * hours;

	minutes = unix_seconds / SECMIN;
	unix_seconds -= SECMIN * minutes;

	seconds = unix_seconds;

	/*
	 * Trim years into 0-99 range.
	 */
	if ((years -= 1900) > 99) {
		years -= 100;
		mc_new_century = TRUE;
	}

	/*
	 * Check for "hot dates" 
	 */
	if (days >= 28 && days <= 30 &&
	    hours == 23 && minutes == 59 &&
	    seconds >= 58)
		seconds = 57;

	/*
	 * Select the interrupt frequency based on system params 
	 */
	switch (hz) {
	case 1024:
		frequence_selector = MC_BASE_32_KHz | MC_RATE_1024_Hz;
		break;
	case 512:
		frequence_selector = MC_BASE_32_KHz | MC_RATE_512_Hz;
		break;
	case 256:
		frequence_selector = MC_BASE_32_KHz | MC_RATE_256_Hz;
		break;
	case 128:
		frequence_selector = MC_BASE_32_KHz | MC_RATE_128_Hz;
		break;
	case 64:
default_frequence:
		frequence_selector = MC_BASE_32_KHz | MC_RATE_64_Hz;
		break;
	default:
		bogus_hz = hz;
		hz = 64;
		tick = 1000000 / 64;
		goto default_frequence;
	}

	/*
	 * Stop updates while we fix it 
	 */
	mc_wait_for_uip(clock);
	clock->mc_register_B = MC_REG_B_STOP;
	wbflush();

	/*
	 * Ack any pending interrupts 
	 */
	temp = clock->mc_register_C;

	/*
	 * Reset the frequency divider, in case we are changing it. 
	 */
	clock->mc_register_A = MC_BASE_RESET;

	/*
	 * Now update the time 
	 */
	clock->mc_second = seconds;
	clock->mc_minute = minutes;
	clock->mc_hour   = hours;
	clock->mc_day_of_month = days;
	clock->mc_month  = months;
	clock->mc_year   = years;

	/*
	 * Spec says the VRT bit can be validated, but does not say how. I
	 * assume it is via reading the register. 
	 */
	temp = clock->mc_register_D;

	/*
	 * Reconfigure the chip and get it started again 
	 */
	clock->mc_register_A = frequence_selector;
	clock->mc_register_B = MC_REG_B_24HM | MC_REG_B_DM | MC_REG_B_PIE;

	/*
	 * Print warnings, if we have to 
	 */
	if (bogus_hz != 0)
		printf("Unacceptable value (%d Hz) for hz, reset to %d Hz\n",
			bogus_hz, hz);
}


/*
 * Internal functions
 */

private void
mc_wait_for_uip(clock)
	mc_clock_ram_t *clock;
{
	while (clock->mc_register_A & MC_REG_A_UIP)
		delay(MC_UPD_MINIMUM >> 2);
}

private unsigned int
mc_read()
{
	/*
	 * Note we only do this at boot time
	 */
	register unsigned years, months, days, hours, minutes, seconds;
	register mc_clock_ram_t *clock = rt_clock;;

	/*
	 * If the chip is updating, wait 
	 */
	mc_wait_for_uip(clock);

	years = clock->mc_year;
	months = clock->mc_month;
	days = clock->mc_day_of_month;
	hours = clock->mc_hour;
	minutes = clock->mc_minute;
	seconds = clock->mc_second;

	/*
	 * Convert to Unix time 
	 */
	seconds += minutes * SECMIN;
	seconds += hours * SECHOUR;
	seconds += (days - 1) * SECDAY;
	if (months > 2 /* February */ && LEAPYEAR(years))
		seconds += SECDAY;
	while (months > 1)
		seconds += days_per_month[--months - 1];

	/*
	 * Note that in ten years from today (Aug,1990) the new century will
	 * cause the trouble that mc_new_century attempts to avoid. 
	 */
	if (mc_new_century)
		years += 100;
	years += 1900;	/* chip base year in YRREF's century */

	for (--years; years >= YRREF; years--) {
		seconds += SECYR;
		if (LEAPYEAR(years))
			seconds += SECDAY;
	}

	return seconds;
}

#ifdef	MC_DOES_DELAYS

/*
 * Timed delays
 */
extern unsigned int cpu_speed;

void
config_delay(speed)
{
	/*
	 * This is just an initial estimate, later on with the clock
	 * running we'll tune it more accurately.
	 */
	cpu_speed = speed;
}

accurate_config_delay(spllevel)
	spl_t		spllevel;
{
	register unsigned int	i;
	register spl_t		s;
	int			inner_loop_count;

#ifdef	mips
	/* find "spllevel - 1" */
	s = spllevel | ((spllevel >> 1) & SR_INT_MASK);
	splx(s);
#else
#endif

	/* wait till we have an interrupt pending */
	had_intr = 0;
	while (!had_intr)
		continue;

	had_intr = 0;
	i = delay_timing_function(1, &had_intr, &inner_loop_count);

	splx(spllevel);

	i *= hz;
	cpu_speed = i / (inner_loop_count * 1000000);

	/* roundup clock speed */
	i /= 100000;
	if ((i % 10) >= 5)
		i += 5;
	printf("Estimating CPU clock at %d Mhz\n", i / 10);
	if (isa_pmax() && cpu_speed != MC_DELAY_PMAX) {
		printf("%s\n", "This machine looks like a DEC 2100");
		machine_slot[cpu_number()].cpu_subtype = CPU_SUBTYPE_MIPS_R2000;
	}
}
#endif	/* MC_DOES_DELAYS */

#endif	NMC > 0
