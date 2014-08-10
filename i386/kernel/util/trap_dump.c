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

#include <mach/machine/eflags.h>
#include <mach/machine/proc_reg.h>

#include "vm_param.h"
#include "trap.h"

void trap_dump(struct trap_state *st)
{
	short flags;
	int from_user = (st->cs & 3) || (st->eflags & EFL_VM);
	unsigned *dump_sp = 0;
	int i;

	printf("Dump of trap_state at %08x:\n", st);
	printf("EAX %08x EBX %08x ECX %08x EDX %08x\n",
		st->eax, st->ebx, st->ecx, st->edx);
	printf("ESI %08x EDI %08x EBP %08x ESP %08x\n",
		st->esi, st->edi, st->ebp,
		from_user ? st->esp : (unsigned)&st->esp);
	printf("EIP %08x EFLAGS %08x\n", st->eip, st->eflags);
	printf("CS %04x SS %04x DS %04x ES %04x FS %04x GS %04x\n",
		st->cs & 0xffff, from_user ? st->ss & 0xffff : get_ss(),
		st->ds & 0xffff, st->es & 0xffff,
		st->fs & 0xffff, st->gs & 0xffff);
	printf("v86:            DS %04x ES %04x FS %04x GS %04x\n",
		st->v86_ds & 0xffff, st->v86_es & 0xffff,
		st->v86_gs & 0xffff, st->v86_gs & 0xffff);
	printf("trapno %d, error %08x, from %s mode\n",
		st->trapno, st->err, from_user ? "user" : "kernel");
	if (st->trapno == T_PAGE_FAULT)
		printf("page fault linear address %08x\n", st->cr2);

	/* Dump the top of the stack too.  */
	if (!from_user)
	{
		for (i = 0; i < 32; i++)
		{
			printf("%08x%c", (&st->esp)[i],
				((i & 7) == 7) ? '\n' : ' ');
		}
	}
}

