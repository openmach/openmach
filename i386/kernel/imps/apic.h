/* 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _IMPS_APIC_
#define _IMPS_APIC_

typedef struct ApicReg
{
	unsigned r;	/* the actual register */
	unsigned p[3];	/* pad to the next 128-bit boundary */
} ApicReg;

typedef struct ApicIoUnit
{
	ApicReg select;
	ApicReg window;
} ApicIoUnit;
#define APIC_IO_UNIT_ID			0x00
#define APIC_IO_VERSION			0x01
#define APIC_IO_REDIR_LOW(int_pin)	(0x10+(int_pin)*2)
#define APIC_IO_REDIR_HIGH(int_pin)	(0x11+(int_pin)*2)

typedef struct ApicLocalUnit
{
	ApicReg reserved0;
	ApicReg reserved1;
	ApicReg unit_id;
	ApicReg version;
	ApicReg reserved4;
	ApicReg reserved5;
	ApicReg reserved6;
	ApicReg reserved7;
	ApicReg task_pri;
	ApicReg reservedb;
	ApicReg reservedc;
	ApicReg eoi;
	ApicReg remote;
	ApicReg logical_dest;
	ApicReg dest_format;
	ApicReg spurious_vector;
	ApicReg isr[8];
	ApicReg tmr[8];
	ApicReg irr[8];
	ApicReg reserved28[8];
	ApicReg	int_command[2];
	ApicReg timer_vector;
	ApicReg reserved33;
	ApicReg reserved34;
	ApicReg lint0_vector;
	ApicReg lint1_vector;
	ApicReg reserved37;
	ApicReg init_count;
	ApicReg cur_count;
	ApicReg reserved3a;
	ApicReg reserved3b;
	ApicReg reserved3c;
	ApicReg reserved3d;
	ApicReg divider_config;
	ApicReg reserved3f;
} ApicLocalUnit;


/* Address at which the local unit is mapped in kernel virtual memory.
   Must be constant.  */
#define APIC_LOCAL_VA	0xc1000000

#define apic_local_unit (*((volatile ApicLocalUnit*)APIC_LOCAL_VA))


/* Set or clear a bit in a 255-bit APIC mask register.
   These registers are spread through eight 32-bit registers.  */
#define APIC_SET_MASK_BIT(reg, bit) \
	((reg)[(bit) >> 5].r |= 1 << ((bit) & 0x1f))
#define APIC_CLEAR_MASK_BIT(reg, bit) \
	((reg)[(bit) >> 5].r &= ~(1 << ((bit) & 0x1f)))

#endif _IMPS_APIC_
