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

#include "vm_param.h"
#include "seg.h"
#include "idt.h"
#include "gdt.h"

struct real_gate idt[IDTSZ];

struct idt_init_entry
{
	unsigned entrypoint;
	unsigned short vector;
	unsigned short type;
};
extern struct idt_init_entry idt_inittab[];

void idt_init()
{
	struct idt_init_entry *iie = idt_inittab;

	/* Initialize the exception vectors from the idt_inittab.  */
	while (iie->entrypoint)
	{
		fill_idt_gate(iie->vector, iie->entrypoint, KERNEL_CS, iie->type, 0);
		iie++;
	}

	/* Load the IDT pointer into the processor.  */
	{
		struct pseudo_descriptor pdesc;

		pdesc.limit = sizeof(idt)-1;
		pdesc.linear_base = kvtolin(&idt);
		lidt(&pdesc);
	}
}

