/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * Support routines for FP emulator.
 */

#include <fpe.h>

#include <cpus.h>

#include <mach/std_types.h>
#include <mach/exception.h>
#include <mach/thread_status.h>

#include <kern/cpu_number.h>
#include <kern/thread.h>

#include <vm/vm_kern.h>

#include <mach/machine/eflags.h>
#include "vm_param.h"
#include <i386/pmap.h>
#include <i386/thread.h>
#include <i386/fpu.h>
#include "proc_reg.h"
#include "seg.h"
#include "idt.h"
#include "gdt.h"

#if	NCPUS > 1
#include <i386/mp_desc.h>
#endif

extern vm_offset_t	kvtophys();

/*
 * Symbols exported from FPE emulator.
 */
extern char	fpe_start[];	/* start of emulator text;
				   also emulation entry point */
extern char	fpe_end[];	/* end of emulator text */
extern int	fpe_reg_segment;
				/* word holding segment number for
				   FPE register/status area */
extern char	fpe_recover[];	/* emulation fault recovery entry point */

extern void	fix_desc();

#if	NCPUS > 1
#define	curr_gdt(mycpu)		(mp_gdt[mycpu])
#define	curr_idt(mycpu)		(mp_desc_table[mycpu]->idt)
#else
#define	curr_gdt(mycpu)		(gdt)
#define	curr_idt(mycpu)		(idt)
#endif

#define	gdt_desc_p(mycpu,sel) \
	((struct real_descriptor *)&curr_gdt(mycpu)[sel_idx(sel)])
#define	idt_desc_p(mycpu,idx) \
	((struct real_gate *)&curr_idt(mycpu)[idx])

void	set_user_access();	/* forward */

/*
 * long pointer for calling FPE register recovery routine.
 */
struct long_ptr {
	unsigned long	offset;
	unsigned short	segment;
};

struct long_ptr fpe_recover_ptr;

/*
 * Initialize descriptors for FP emulator.
 */
void
fpe_init()
{
	register struct real_descriptor *gdt_p;
	register struct real_gate *idt_p;

	/*
	 * Map in the pages for the FP emulator:
	 * read-only, user-accessible.
	 */
	set_user_access(pmap_kernel(),
			(vm_offset_t)fpe_start,
			(vm_offset_t)fpe_end,
			FALSE);

	/*
	 * Put the USER_FPREGS segment value in the FP emulator.
	 */
	fpe_reg_segment = USER_FPREGS;

	/*
	 * Change exception 7 gate (coprocessor not present)
	 * to a trap gate to the FPE code segment.
	 */
	idt_p = idt_desc_p(cpu_number(), 7);
	idt_p->offset_low  = 0;			/* offset of FPE entry */
	idt_p->offset_high = 0;
	idt_p->selector	  = FPE_CS;		/* FPE code segment */
	idt_p->word_count = 0;
	idt_p->access 	  = ACC_P|ACC_PL_K|ACC_TRAP_GATE;
						/* trap gate */
						/* kernel privileges only,
						   so INT $7 does not call
						   the emulator */

	/*
	 * Build GDT entry for FP code segment.
	 */
	gdt_p = gdt_desc_p(cpu_number(), FPE_CS);
	gdt_p->base_low   = ((vm_offset_t) fpe_start) & 0xffff;
	gdt_p->base_med   = (((vm_offset_t) fpe_start) >> 16) & 0xff;
	gdt_p->base_high  = ((vm_offset_t) fpe_start) >> 24;
	gdt_p->limit_low  = (vm_offset_t) fpe_end
			  - (vm_offset_t) fpe_start
			  - 1;
	gdt_p->limit_high = 0;
	gdt_p->granularity = SZ_32;
	gdt_p->access	  = ACC_P|ACC_PL_K|ACC_CODE_CR;
						/* conforming segment,
						   usable by kernel */

	/*
	 * Build GDT entry for user FP state area - template,
	 * since each thread has its own.
	 */
	gdt_p = gdt_desc_p(cpu_number(), USER_FPREGS);
	/* descriptor starts as 0 */
	gdt_p->limit_low  = sizeof(struct i386_fp_save)
			  + sizeof(struct i386_fp_regs)
			  - 1;
	gdt_p->limit_high = 0;
	gdt_p->granularity = 0;
	gdt_p->access = ACC_PL_U|ACC_DATA_W;
					/* start as "not present" */

	/*
	 * Set up the recovery routine pointer
	 */
	fpe_recover_ptr.offset = fpe_recover - fpe_start;
	fpe_recover_ptr.segment = FPE_CS;

	/*
	 * Set i386 to emulate coprocessor.
	 */
	set_cr0((get_cr0() & ~CR0_MP) | CR0_EM);
}

/*
 * Enable FPE use for a new thread.
 * Allocates the FP save area.
 */
boolean_t
fp_emul_error(regs)
	struct i386_saved_state *regs;
{
	register struct i386_fpsave_state *ifps;
	register vm_offset_t	start_va;

	if ((regs->err & 0xfffc) != (USER_FPREGS & ~SEL_PL))
	    return FALSE;

	/*
	 * Make the FPU save area user-accessible (by FPE)
	 */
	ifps = current_thread()->pcb->ims.ifps;
	if (ifps == 0) {
	    /*
	     * No FP register state yet - allocate it.
	     */
	    fp_state_alloc();
	    ifps = current_thread()->pcb->ims.ifps;
	}
	    
	panic("fp_emul_error: FP emulation is probably broken because of VM changes; fix! XXX");
	start_va = (vm_offset_t) &ifps->fp_save_state;
	set_user_access(current_map()->pmap,
		start_va,
		start_va + sizeof(struct i386_fp_save),
		TRUE);

	/*
	 * Enable FPE use for this thread
	 */
	enable_fpe(ifps);

	return TRUE;
}

/*
 * Enable FPE use.  ASSUME that kernel does NOT use FPU
 * except to handle user exceptions.
 */
void
enable_fpe(ifps)
	register struct i386_fpsave_state *ifps;
{
	struct real_descriptor *dp;
	vm_offset_t	start_va;

	dp = gdt_desc_p(cpu_number(), USER_FPREGS);
	start_va = (vm_offset_t)&ifps->fp_save_state;

	dp->base_low = start_va & 0xffff;
	dp->base_med = (start_va >> 16) & 0xff;
	dp->base_high = start_va >> 24;
	dp->access |= ACC_P;
}

void
disable_fpe()
{
	/*
	 *	The kernel might be running with fs & gs segments
	 *	which refer to USER_FPREGS, if we entered the kernel
	 *	from a FP-using thread.  We have to clear these segments
	 *	lest we get a Segment Not Present trap.  This would happen
	 *	if the kernel took an interrupt or fault after clearing
	 *	the present bit but before exiting to user space (which
	 *	would reset fs & gs from the current user thread).
	 */

	asm volatile("xorl %eax, %eax");
	asm volatile("movw %ax, %fs");
	asm volatile("movw %ax, %gs");

	gdt_desc_p(cpu_number(), USER_FPREGS)->access &= ~ACC_P;
}

void
set_user_access(pmap, start, end, writable)
	pmap_t		pmap;
	vm_offset_t	start;
	vm_offset_t	end;
	boolean_t	writable;
{
	register vm_offset_t	va;
	register pt_entry_t *	dirbase = pmap->dirbase;
	register pt_entry_t *	ptep;
	register pt_entry_t *	pdep;

	start = i386_trunc_page(start);
	end   = i386_round_page(end);

	for (va = start; va < end; va += I386_PGBYTES) {

	    pdep = &dirbase[lin2pdenum(kvtolin(va))];
	    *pdep |= INTEL_PTE_USER;
	    ptep = (pt_entry_t *)ptetokv(*pdep);
	    ptep = &ptep[ptenum(va)];
	    *ptep |= INTEL_PTE_USER;
	    if (!writable)
		*ptep &= ~INTEL_PTE_WRITE;
	}
}

/*
 * Route exception through emulator fixup routine if
 * it occured within the emulator.
 */
extern void exception();

void
fpe_exception_fixup(exc, code, subcode)
	int	exc, code, subcode;
{
	thread_t	thread = current_thread();
	pcb_t		pcb = thread->pcb;

	if (pcb->iss.efl & EFL_VM) {
	    /*
	     * The emulator doesn`t handle V86 mode.
	     * If this is a GP fault on the emulator`s
	     * code segment, change it to an FP not present
	     * fault.
	     */
	    if (exc == EXC_BAD_INSTRUCTION
	     && code == EXC_I386_GPFLT
	     && subcode == FPE_CS + 1)
	    {
		exc = EXC_ARITHMETIC;	/* arithmetic error: */
		code = EXC_I386_NOEXT;	/* no FPU */
		subcode = 0;
	    }
	}
	else
	if ((pcb->iss.cs & 0xfffc) == FPE_CS) {
	    /*
	     * Pass registers to emulator,
	     * to let it fix them up.
	     * The emulator fixup routine knows about
	     * an i386_thread_state.
	     */
	    struct i386_thread_state	tstate;
	    unsigned int		count;

	    count = i386_THREAD_STATE_COUNT;
	    (void) thread_getstatus(thread,
				i386_REGS_SEGS_STATE,
				(thread_state_t) &tstate,
				&count);

	    /*
	     * long call to emulator register recovery routine
	     */
	    asm volatile("pushl %0; lcall %1; addl $4,%%esp"
			:
			: "r" (&tstate),
			  "m" (*(char *)&fpe_recover_ptr) );

	    (void) thread_setstatus(thread,
				i386_REGS_SEGS_STATE,
				(thread_state_t) &tstate,
				count);
	    /*
	     * In addition, check for a GP fault on 'int 16' in
	     * the emulator, since the interrupt gate is protected.
	     * If so, change it to an arithmetic error.
	     */
	    if (exc == EXC_BAD_INSTRUCTION
	     && code == EXC_I386_GPFLT
	     && subcode == 8*16+2)	/* idt[16] */
	    {
		exc = EXC_ARITHMETIC;
		code = EXC_I386_EXTERR;
		subcode = pcb->ims.ifps->fp_save_state.fp_status;
	    }
	}
	exception(exc, code, subcode);
}
