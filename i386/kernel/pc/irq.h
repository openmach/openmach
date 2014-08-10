/* 
 * Copyright (c) 1995 The University of Utah and
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
#ifndef _I386_PC_IRQ_H_
#define _I386_PC_IRQ_H_

/* On normal PCs, there are always 16 IRQ lines.  */
#define IRQ_COUNT 16

/* Start of hardware interrupt vectors in the IDT.  */
#define IDT_IRQ_BASE 0x20

/* Variables storing the master and slave PIC interrupt vector base.  */
extern int irq_master_base, irq_slave_base;

/* Routine called just after entering protected mode for the first time,
   to set up the IRQ interrupt vectors in the protected-mode IDT.
   It should initialize IDT entries irq_master_base through irq_master_base+7,
   and irq_slave_base through irq_slave_base+7.  */
extern void idt_irq_init(void);

/* Fill an IRQ gate in a CPU's IDT.
   Always uses an interrupt gate; just set `access' to the privilege level.  */
#define fill_irq_gate(cpu, irq_num, entry, selector, access)			\
	fill_idt_gate(cpu, (irq_num) < 8					\
			   ? irq_master_base+(irq_num)				\
			   : irq_slave_base+(irq_num)-8,			\
		  entry, selector, ACC_INTR_GATE | (access))

#endif _I386_PC_IRQ_H_
