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
 *	File:	mc_clock.h
 *	Author: Alessandro Forin
 *	Date:	8/90
 *
 *	Definitions for the MC146818 Clock Driver
 */

/*
 *	Functions this module implements
 */

extern void	resettodr(/* */);	/* reset time-of-day register */
extern void	startrtclock(/* */);	/* start real-time clock */
extern void	stopclocks(/* */);	/* stop  real-time clock */
extern boolean_t ackrtclock(/* */);	/* acknowledge interrupt, if any */
extern boolean_t todr_running;		/* status */

extern boolean_t mc_new_century;	/* patch this after year 2000 (honest!) */

extern void	delay(/* int usecs */);	/* waste that many microseconds */
extern void	config_delay(/* int speed */);		/* for delay() */
#define		MC_DELAY_PMAX	8
#define		MC_DELAY_3MAX	12

extern void	set_clock_addr(/* vm_offset_t addr */);	/* RAM location */

/*
 *	Real-Time Clock plus RAM device (MC146818)
 */

/*
 * RAM Memory Map (as seen by the chip)
 */
typedef struct {
	volatile unsigned char	mc_second;
	volatile unsigned char	mc_alarm_second;
	volatile unsigned char	mc_minute;
	volatile unsigned char	mc_alarm_minute;
	volatile unsigned char	mc_hour;
	volatile unsigned char	mc_alarm_hour;
	volatile unsigned char	mc_day_of_week;
	volatile unsigned char	mc_day_of_month;
	volatile unsigned char	mc_month;
	volatile unsigned char	mc_year;
	volatile unsigned char	mc_register_A;
	volatile unsigned char	mc_register_B;
	volatile unsigned char	mc_register_C;
	volatile unsigned char	mc_register_D;
	unsigned char		mc_non_volatile_ram[50];
} mc_clock_t;

/*
 * Register A defines (read/write)
 */

#define	MC_REG_A_RS	0x0f		/* Interrupt rate (and SQwave) select */
#define	MC_REG_A_DV	0x70		/* Divider select */
#define	MC_REG_A_UIP	0x80		/* Update In Progress (read-only bit) */

/* Time base configuration */
#define	MC_BASE_4_MHz	0x00
#define	MC_BASE_1_MHz	0x10
#define	MC_BASE_32_KHz	0x20
#define	MC_BASE_NONE	0x60		/* actually, both of these reset */
#define	MC_BASE_RESET	0x70

/* Interrupt rate table */
#define	MC_RATE_NONE	0x0		/* disabled */
#define	MC_RATE_1	0x1		/* 256Hz if MC_BASE_32_KHz, else 32768Hz */
#define	MC_RATE_2	0x2		/* 128Hz if MC_BASE_32_KHz, else 16384Hz */
#define	MC_RATE_8192_Hz	0x3		/* Tpi: 122.070 usecs */
#define	MC_RATE_4096_Hz	0x4		/* Tpi: 244.141 usecs */
#define	MC_RATE_2048_Hz	0x5		/* Tpi: 488.281 usecs */
#define	MC_RATE_1024_Hz	0x6		/* Tpi: 976.562 usecs */
#define	MC_RATE_512_Hz	0x7		/* Tpi: 1.953125 ms */
#define	MC_RATE_256_Hz	0x8		/* Tpi: 3.90625 ms */
#define	MC_RATE_128_Hz	0x9		/* Tpi: 7.8125 ms */
#define	MC_RATE_64_Hz	0xa		/* Tpi: 15.625 ms */
#define	MC_RATE_32_Hz	0xb		/* Tpi: 31.25 ms */
#define	MC_RATE_16_Hz	0xc		/* Tpi: 62.5 ms */
#define	MC_RATE_8_Hz	0xd		/* Tpi: 125 ms */
#define	MC_RATE_4_Hz	0xe		/* Tpi: 250 ms */
#define	MC_RATE_2_Hz	0xf		/* Tpi: 500 ms */

/* Update cycle time */
#define MC_UPD_4_MHz	 248		/* usecs */
#define MC_UPD_1_MHz	 248		/* usecs */
#define MC_UPD_32_KHz	1984		/* usecs */
#define MC_UPD_MINIMUM	 244		/* usecs, guaranteed if UIP=0 */

/*
 * Register B defines (read/write)
 */

#define MC_REG_B_DSE	0x01		/* Daylight Savings Enable */
#define MC_REG_B_24HM	0x02		/* 24/12 Hour Mode */
#define MC_REG_B_DM	0x04		/* Data Mode, 1=Binary 0=BCD */
#define MC_REG_B_SQWE	0x08		/* Sqare Wave Enable */
#define MC_REG_B_UIE	0x10		/* Update-ended Interrupt Enable */
#define MC_REG_B_AIE	0x20		/* Alarm Interrupt Enable */
#define MC_REG_B_PIE	0x40		/* Periodic Interrupt Enable */
#define MC_REG_B_SET	0x80		/* Set NVram info, e.g. update time or ..*/
#define MC_REG_B_STOP	MC_REG_B_SET	/* Stop updating the timing info */

/*
 * Register C defines (read-only)
 */

#define MC_REG_C_ZEROES	0x0f		/* Reads as zero bits  */
#define MC_REG_C_UF	0x10		/* Update-ended interrupt flag */
#define MC_REG_C_AF	0x20		/* Alarm interrupt flag */
#define MC_REG_C_PF	0x40		/* Periodic interrupt flag */
#define MC_REG_C_IRQF	0x80		/* Interrupt request flag */

/*
 * Register D defines (read-only)
 */

#define MC_REG_D_ZEROES	0x7f		/* Reads as zero bits  */
#define MC_REG_D_VRT	0x80		/* Valid RAM and Time */

