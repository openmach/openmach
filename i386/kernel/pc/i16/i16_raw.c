/* 
 * Copyright (c) 1995-1994 The University of Utah and
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
/*
	This file rovides a default implementation
	of real/pmode switching code.
	Assumes that, as far as it's concerned,
	low linear address always map to physical addresses.
	(The low linear mappings can be changed,
	but must be changed back before switching back to real mode.)

	Provides:
		i16_raw_switch_to_pmode()
		i16_raw_switch_to_real_mode()

		i16_raw_start()
			Called in real mode.
			Initializes the pmode switching system,
			switches to pmode for the first time,
			and calls the 32-bit function raw_start().

	Depends on:

		paging.h:
			raw_paging_enable()
			raw_paging_disable()
			raw_paging_init()

		a20.h:
			i16_enable_a20()
			i16_disable_a20()

		real.h:
			real_cs
*/

#include <mach/boolean.h>
#include <mach/machine/code16.h>
#include <mach/machine/vm_param.h>
#include <mach/machine/proc_reg.h>
#include <mach/machine/pio.h>
#include <mach/machine/seg.h>
#include <mach/machine/eflags.h>
#include <mach/machine/pmode.h>

#include "config.h"
#include "cpu.h"
#include "i16.h"
#include "vm_param.h"
#include "pic.h"
#include "debug.h"
#include "i16_a20.h"
#include "i16_switch.h"

int irq_master_base, irq_slave_base;

/* Set to true when everything is initialized properly.  */
static boolean_t inited;

/* Saved value of eflags register for real mode.  */
static unsigned real_eflags;



#ifdef ENABLE_PAGING
#define RAW_PAGING_ENABLE() raw_paging_enable()
#define RAW_PAGING_DISABLE() raw_paging_disable()
#define RAW_PAGING_INIT() raw_paging_init()
#else
#define RAW_PAGING_ENABLE() ((void)0)
#define RAW_PAGING_DISABLE() ((void)0)
#define RAW_PAGING_INIT() ((void)0)
#endif


CODE16

void i16_raw_switch_to_pmode()
{
	/* No interrupts from now on please.  */
	i16_cli();

	/* Save the eflags register for switching back later.  */
	real_eflags = get_eflags();

	/* Enable the A20 address line.  */
	i16_enable_a20();

	/* Load the GDT.
	   Note that we have to do this each time we enter pmode,
	   not just the first,
	   because other real-mode programs may have switched to pmode
	   and back again in the meantime, trashing the GDT pointer.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = sizeof(cpu[0].tables.gdt)-1;
		pdesc.linear_base = boot_image_pa
				    + (vm_offset_t)&cpu[0].tables.gdt;
		i16_set_gdt(&pdesc);
	}

	/* Switch into protected mode.  */
	i16_enter_pmode(KERNEL_16_CS);

	/* Reload all the segment registers from the new GDT.  */
	set_ds(KERNEL_DS);
	set_es(KERNEL_DS);
	set_fs(0);
	set_gs(0);
	set_ss(KERNEL_DS);

	i16_do_32bit(

		if (inited)
		{
			/* Turn paging on if necessary.  */
			RAW_PAGING_ENABLE();

			/* Load the CPU tables into the processor.  */
			cpu_tables_load(&cpu[0]);

			/* Program the PIC so the interrupt vectors won't
			   conflict with the processor exception vectors.  */
			pic_init(PICM_VECTBASE, PICS_VECTBASE);
		}

		/* Make sure our flags register is appropriate.  */
		set_eflags((get_eflags()
			   & ~(EFL_IF | EFL_DF | EFL_NT))
			   | EFL_IOPL_USER);
	);
}

void i16_raw_switch_to_real_mode()
{
	/* Make sure interrupts are disabled.  */
	cli();

	/* Avoid sending DOS bogus coprocessor exceptions.
	   XXX should we save/restore all of CR0?  */
	i16_clts();

	i16_do_32bit(
		/* Turn paging off if necessary.  */
		RAW_PAGING_DISABLE();

		/* Reprogram the PIC back to the settings DOS expects.  */
		pic_init(0x08, 0x70);
	);

	/* Make sure all the segment registers are 16-bit.
	   The code segment definitely is already,
	   because we're running 16-bit code.  */
	set_ds(KERNEL_16_DS);
	set_es(KERNEL_16_DS);
	set_fs(KERNEL_16_DS);
	set_gs(KERNEL_16_DS);
	set_ss(KERNEL_16_DS);

	/* Switch back to real mode.  */
	i16_leave_pmode(real_cs);

	/* Load the real-mode segment registers.  */
	set_ds(real_cs);
	set_es(real_cs);
	set_fs(real_cs);
	set_gs(real_cs);
	set_ss(real_cs);

	/* Load the real-mode IDT.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = 0xffff;
		pdesc.linear_base = 0;
		i16_set_idt(&pdesc);
	}

	/* Disable the A20 address line.  */
	i16_disable_a20();

	/* Restore the eflags register to its original real-mode state.
	   Note that this will leave interrupts disabled
	   since it was saved after the cli() above.  */
	set_eflags(real_eflags);
}

void i16_raw_start()
{
	/* Make sure we're not already in protected mode.  */
	if (i16_get_msw() & CR0_PE)
		i16_die("The processor is in an unknown "
			"protected mode environment.");

	do_debug(i16_puts("Real mode detected"));

	/* Minimally initialize the GDT.  */
	i16_gdt_init_temp();

	/* Switch to protected mode for the first time.
	   This won't load all the processor tables and everything yet,
	   since they're not fully initialized.  */
	i16_raw_switch_to_pmode();

	/* We can now hop in and out of 32-bit mode at will.  */
	i16_do_32bit(

		/* Now that we can access all physical memory,
		   collect the memory regions we discovered while in 16-bit mode
		   and add them to our free memory list.
		   We can't do this before now because the free list nodes
		   are stored in the free memory itself,
		   which is probably out of reach of our 16-bit segments.  */
		phys_mem_collect();

		/* Initialize paging if necessary.
		   Do it before initializing the other processor tables
		   because they might have to be located
		   somewhere in high linear memory.  */
		RAW_PAGING_INIT();

		/* Initialize the processor tables.  */
		cpu_init(&cpu[0]);

		/* Initialize the hardware interrupt vectors in the IDT.  */
		irq_master_base = PICM_VECTBASE;
		irq_slave_base = PICS_VECTBASE;
		idt_irq_init();

		inited = TRUE;

		/* Switch to real mode and back again once more,
		   to make sure everything's loaded properly.  */
		do_16bit(
			i16_raw_switch_to_real_mode();
			i16_raw_switch_to_pmode();
		);

		raw_start();
	);
}

void (*i16_switch_to_real_mode)() = i16_raw_switch_to_real_mode;
void (*i16_switch_to_pmode)() = i16_raw_switch_to_pmode;

