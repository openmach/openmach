/*
 * Copyright (c) 1994 The University of Utah and
 * the Center for Software Science (CSS).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSS DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSS requests users of this software to return to css-dist@cs.utah.edu any
 * improvements that they make and grant CSS redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSS
 */

#include <mach/machine/tss.h>

void tss_dump(struct i386_tss *tss)
{
	printf("Dump of TSS at %08x:\n", tss);
	printf("back_link %04x\n", tss->back_link & 0xffff);
	printf("ESP0 %08x SS0 %04x\n", tss->esp0, tss->ss0 & 0xffff);
	printf("ESP1 %08x SS1 %04x\n", tss->esp1, tss->ss1 & 0xffff);
	printf("ESP2 %08x SS2 %04x\n", tss->esp2, tss->ss2 & 0xffff);
	printf("CR3 %08x\n", tss->cr3);
	printf("EIP %08x EFLAGS %08x\n", tss->eip, tss->eflags);
	printf("EAX %08x EBX %08x ECX %08x EDX %08x\n",
		tss->eax, tss->ebx, tss->ecx, tss->edx);
	printf("ESI %08x EDI %08x EBP %08x ESP %08x\n",
		tss->esi, tss->edi, tss->ebp, tss->esp);
	printf("CS %04x SS %04x DS %04x ES %04x FS %04x GS %04x\n",
		tss->cs & 0xffff, tss->ss & 0xffff,
		tss->ds & 0xffff, tss->es & 0xffff,
		tss->fs & 0xffff, tss->gs & 0xffff);
	printf("LDT %04x\n", tss->ldt & 0xffff);
	printf("trace_trap %04x\n", tss->trace_trap);
	printf("IOPB offset %04x\n", tss->io_bit_map_offset);
}

