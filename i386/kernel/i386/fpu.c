/* 
 * Mach Operating System
 * Copyright (c) 1992-1990 Carnegie Mellon University
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
 * Support for 80387 floating point or FP emulator.
 */
#include <cpus.h>
#include <fpe.h>
#include <platforms.h>

#include <mach/exception.h>
#include <mach/machine/thread_status.h>
#include <mach/machine/fp_reg.h>

#include <machine/machspl.h>	/* spls */
#include <kern/mach_param.h>
#include <kern/thread.h>
#include <kern/zalloc.h>

#include <i386/thread.h>
#include <i386/fpu.h>
#include "cpu_number.h"

#if 0
#include <i386/ipl.h>
extern int curr_ipl;
#define ASSERT_IPL(L) \
{ \
      if (curr_ipl != L) { \
	      printf("IPL is %d, expected %d\n", curr_ipl, L); \
	      panic("fpu: wrong ipl"); \
      } \
}
#else
#define ASSERT_IPL(L)
#endif

extern void i386_exception();

int		fp_kind = FP_387;	/* 80387 present */
zone_t		ifps_zone;		/* zone for FPU save area */

#if	NCPUS == 1
volatile thread_t	fp_thread = THREAD_NULL;
					/* thread whose state is in FPU */
					/* always THREAD_NULL if emulating
					   FPU */
volatile thread_t	fp_intr_thread = THREAD_NULL;


#define	clear_fpu() \
    { \
	set_ts(); \
	fp_thread = THREAD_NULL; \
    }

#else	/* NCPUS > 1 */
#define	clear_fpu() \
    { \
	set_ts(); \
    }

#endif


/*
 * Look for FPU and initialize it.
 * Called on each CPU.
 */
void
init_fpu()
{
	unsigned short	status, control;

	/*
	 * Check for FPU by initializing it,
	 * then trying to read the correct bit patterns from
	 * the control and status registers.
	 */
	set_cr0(get_cr0() & ~(CR0_EM|CR0_TS));	/* allow use of FPU */

	fninit();
	status = fnstsw();
	fnstcw(&control);

	if ((status & 0xff) == 0 &&
	    (control & 0x103f) == 0x3f)
	{
	    /*
	     * We have a FPU of some sort.
	     * Compare -infinity against +infinity
	     * to check whether we have a 287 or a 387.
	     */
	    volatile double fp_infinity, fp_one, fp_zero;
	    fp_one = 1.0;
	    fp_zero = 0.0;
	    fp_infinity = fp_one / fp_zero;
	    if (fp_infinity == -fp_infinity) {
		/*
		 * We have an 80287.
		 */
		fp_kind = FP_287;
		asm volatile(".byte 0xdb; .byte 0xe4");	/* fnsetpm */
	    }
	    else {
		/*
		 * We have a 387.
		 */
		fp_kind = FP_387;
	    }
	    /*
	     * Trap wait instructions.  Turn off FPU for now.
	     */
	    set_cr0(get_cr0() | CR0_TS | CR0_MP);
	}
	else {
#if	FPE
	    /*
	     * Use the floating-point emulator.
	     */
	    fp_kind = FP_SOFT;
	    fpe_init();
#else	/* no fpe */
	    /*
	     * NO FPU.
	     */
	    fp_kind = FP_NO;
	    set_cr0(get_cr0() | CR0_EM);
#endif
	}
}

/*
 * Initialize FP handling.
 */
void
fpu_module_init()
{
	ifps_zone = zinit(sizeof(struct i386_fpsave_state),
			  THREAD_MAX * sizeof(struct i386_fpsave_state),
			  THREAD_CHUNK * sizeof(struct i386_fpsave_state),
			  0, "i386 fpsave state");
}

/*
 * Free a FPU save area.
 * Called only when thread terminating - no locking necessary.
 */
void
fp_free(fps)
	struct i386_fpsave_state *fps;
{
ASSERT_IPL(SPL0);
#if	NCPUS == 1
	if ((fp_thread != THREAD_NULL) && (fp_thread->pcb->ims.ifps == fps)) {
		/* 
		 * Make sure we don't get FPU interrupts later for
		 * this thread
		 */
		fwait();

		/* Mark it free and disable access */
	    clear_fpu();
	}
#endif	/* NCPUS == 1 */
	zfree(ifps_zone, (vm_offset_t) fps);
}

/*
 * Set the floating-point state for a thread.
 * If the thread is not the current thread, it is
 * not running (held).  Locking needed against
 * concurrent fpu_set_state or fpu_get_state.
 */
kern_return_t
fpu_set_state(thread, state)
	thread_t	thread;
	struct i386_float_state *state;
{
	register pcb_t	pcb = thread->pcb;
	register struct i386_fpsave_state *ifps;
	register struct i386_fpsave_state *new_ifps;

ASSERT_IPL(SPL0);
	if (fp_kind == FP_NO)
	    return KERN_FAILURE;

#if	NCPUS == 1

	/*
	 * If this thread`s state is in the FPU,
	 * discard it; we are replacing the entire
	 * FPU state.
	 */
	if (fp_thread == thread) {
	    fwait();			/* wait for possible interrupt */
	    clear_fpu();		/* no state in FPU */
	}
#endif

	if (state->initialized == 0) {
	    /*
	     * new FPU state is 'invalid'.
	     * Deallocate the fp state if it exists.
	     */
	    simple_lock(&pcb->lock);
	    ifps = pcb->ims.ifps;
	    pcb->ims.ifps = 0;
	    simple_unlock(&pcb->lock);

	    if (ifps != 0) {
		zfree(ifps_zone, (vm_offset_t) ifps);
	    }
	}
	else {
	    /*
	     * Valid state.  Allocate the fp state if there is none.
	     */
	    register struct i386_fp_save *user_fp_state;
	    register struct i386_fp_regs *user_fp_regs;

	    user_fp_state = (struct i386_fp_save *) &state->hw_state[0];
	    user_fp_regs  = (struct i386_fp_regs *)
			&state->hw_state[sizeof(struct i386_fp_save)];

	    new_ifps = 0;
	Retry:
	    simple_lock(&pcb->lock);
	    ifps = pcb->ims.ifps;
	    if (ifps == 0) {
		if (new_ifps == 0) {
		    simple_unlock(&pcb->lock);
		    new_ifps = (struct i386_fpsave_state *) zalloc(ifps_zone);
		    goto Retry;
		}
		ifps = new_ifps;
		new_ifps = 0;
		pcb->ims.ifps = ifps;
	    }

	    /*
	     * Ensure that reserved parts of the environment are 0.
	     */
	    bzero((char *)&ifps->fp_save_state, sizeof(struct i386_fp_save));

	    ifps->fp_save_state.fp_control = user_fp_state->fp_control;
	    ifps->fp_save_state.fp_status  = user_fp_state->fp_status;
	    ifps->fp_save_state.fp_tag     = user_fp_state->fp_tag;
	    ifps->fp_save_state.fp_eip     = user_fp_state->fp_eip;
	    ifps->fp_save_state.fp_cs      = user_fp_state->fp_cs;
	    ifps->fp_save_state.fp_opcode  = user_fp_state->fp_opcode;
	    ifps->fp_save_state.fp_dp      = user_fp_state->fp_dp;
	    ifps->fp_save_state.fp_ds      = user_fp_state->fp_ds;

#if	FPE
	    if (fp_kind == FP_SOFT) {
		/*
		 * The emulator stores the registers by physical
		 * register number, not from top-of-stack.
		 * Shuffle the registers into the correct order.
		 */
		register char *src;	/* user regs */
		register char *dst;	/* kernel regs */
		int	i;

		src = (char *)user_fp_regs;
		dst = (char *)&ifps->fp_regs;
		i = (ifps->fp_save_state.fp_status & FPS_TOS)
			>> FPS_TOS_SHIFT;	/* physical register
						   for st(0) */
		if (i == 0)
		    bcopy(src, dst, 8 * 10);
		else {
		    bcopy(src,
			  dst + 10 * i,
			  10 * (8 - i));
		    bcopy(src + 10 * (8 - i),
			  dst,
			  10 * i);
		}
	    }
	    else
		ifps->fp_regs = *user_fp_regs;
#else	/* no FPE */
	    ifps->fp_regs = *user_fp_regs;
#endif	/* FPE */

	    simple_unlock(&pcb->lock);
	    if (new_ifps != 0)
		zfree(ifps_zone, (vm_offset_t) ifps);
	}

	return KERN_SUCCESS;
}

/*
 * Get the floating-point state for a thread.
 * If the thread is not the current thread, it is
 * not running (held).  Locking needed against
 * concurrent fpu_set_state or fpu_get_state.
 */
kern_return_t
fpu_get_state(thread, state)
	thread_t	thread;
	register struct i386_float_state *state;
{
	register pcb_t	pcb = thread->pcb;
	register struct i386_fpsave_state *ifps;

ASSERT_IPL(SPL0);
	if (fp_kind == FP_NO)
	    return KERN_FAILURE;

	simple_lock(&pcb->lock);
	ifps = pcb->ims.ifps;
	if (ifps == 0) {
	    /*
	     * No valid floating-point state.
	     */
	    simple_unlock(&pcb->lock);
	    bzero((char *)state, sizeof(struct i386_float_state));
	    return KERN_SUCCESS;
	}

	/* Make sure we`ve got the latest fp state info */
	clear_ts();
	fp_save(thread);
	clear_fpu();

	state->fpkind = fp_kind;
	state->exc_status = 0;

	{
	    register struct i386_fp_save *user_fp_state;
	    register struct i386_fp_regs *user_fp_regs;

	    state->initialized = ifps->fp_valid;

	    user_fp_state = (struct i386_fp_save *) &state->hw_state[0];
	    user_fp_regs  = (struct i386_fp_regs *)
			&state->hw_state[sizeof(struct i386_fp_save)];

	    /*
	     * Ensure that reserved parts of the environment are 0.
	     */
	    bzero((char *)user_fp_state,  sizeof(struct i386_fp_save));

	    user_fp_state->fp_control = ifps->fp_save_state.fp_control;
	    user_fp_state->fp_status  = ifps->fp_save_state.fp_status;
	    user_fp_state->fp_tag     = ifps->fp_save_state.fp_tag;
	    user_fp_state->fp_eip     = ifps->fp_save_state.fp_eip;
	    user_fp_state->fp_cs      = ifps->fp_save_state.fp_cs;
	    user_fp_state->fp_opcode  = ifps->fp_save_state.fp_opcode;
	    user_fp_state->fp_dp      = ifps->fp_save_state.fp_dp;
	    user_fp_state->fp_ds      = ifps->fp_save_state.fp_ds;

#if	FPE
	    if (fp_kind == FP_SOFT) {
		/*
		 * The emulator stores the registers by physical
		 * register number, not from top-of-stack.
		 * Shuffle the registers into the correct order.
		 */
		register char *src;	/* kernel regs */
		register char *dst;	/* user regs */
		int	i;

		src = (char *)&ifps->fp_regs;
		dst = (char *)user_fp_regs;
		i = (ifps->fp_save_state.fp_status & FPS_TOS)
			>> FPS_TOS_SHIFT;	/* physical register
						   for st(0) */
		if (i == 0)
		    bcopy(src, dst, 8 * 10);
		else {
		    bcopy(src + 10 * i,
			  dst,
			  10 * (8 - i));
		    bcopy(src,
			  dst + 10 * (8 - i),
			  10 * i);
		}
	    }
	    else
		*user_fp_regs = ifps->fp_regs;
#else	/* no FPE */
	    *user_fp_regs = ifps->fp_regs;
#endif	/* FPE */
	}
	simple_unlock(&pcb->lock);

	return KERN_SUCCESS;
}

/*
 * Initialize FPU.
 *
 * Raise exceptions for:
 *	invalid operation
 *	divide by zero
 *	overflow
 *
 * Use 53-bit precision.
 */
void fpinit()
{
	unsigned short	control;

ASSERT_IPL(SPL0);
	clear_ts();
	fninit();
	fnstcw(&control);
	control &= ~(FPC_PC|FPC_RC); /* Clear precision & rounding control */
	control |= (FPC_PC_53 |		/* Set precision */ 
			FPC_RC_RN | 	/* round-to-nearest */
			FPC_ZE |	/* Suppress zero-divide */
			FPC_OE |	/*  and overflow */
			FPC_UE |	/*  underflow */
			FPC_IE |	/* Allow NaNQs and +-INF */
			FPC_DE |	/* Allow denorms as operands  */
			FPC_PE);	/* No trap for precision loss */
	fldcw(control);
}

/*
 * Coprocessor not present.
 */
fpnoextflt()
{
	/*
	 * Enable FPU use.
	 */
ASSERT_IPL(SPL0);
	clear_ts();
#if	NCPUS == 1

	/*
	 * If this thread`s state is in the FPU, we are done.
	 */
	if (fp_thread == current_thread())
	    return;

	/* Make sure we don't do fpsave() in fp_intr while doing fpsave()
	 * here if the current fpu instruction generates an error.
	 */
	fwait();
	/*
	 * If another thread`s state is in the FPU, save it.
	 */
	if (fp_thread != THREAD_NULL) {
	    fp_save(fp_thread);
	}

	/*
	 * Give this thread the FPU.
	 */
	fp_thread = current_thread();

#endif	/* NCPUS == 1 */

	/*
	 * Load this thread`s state into the FPU.
	 */
	fp_load(current_thread());
}

/*
 * FPU overran end of segment.
 * Re-initialize FPU.  Floating point state is not valid.
 */
fpextovrflt()
{
	register thread_t	thread = current_thread();
	register pcb_t		pcb;
	register struct i386_fpsave_state *ifps;

#if	NCPUS == 1

	/*
	 * Is exception for the currently running thread?
	 */
	if (fp_thread != thread) {
	    /* Uh oh... */
	    panic("fpextovrflt");
	}
#endif

	/*
	 * This is a non-recoverable error.
	 * Invalidate the thread`s FPU state.
	 */
	pcb = thread->pcb;
	simple_lock(&pcb->lock);
	ifps = pcb->ims.ifps;
	pcb->ims.ifps = 0;
	simple_unlock(&pcb->lock);

	/*
	 * Re-initialize the FPU.
	 */
	clear_ts();
	fninit();

	/*
	 * And disable access.
	 */
	clear_fpu();

	if (ifps)
	    zfree(ifps_zone, (vm_offset_t) ifps);

	/*
	 * Raise exception.
	 */
	i386_exception(EXC_BAD_ACCESS, VM_PROT_READ|VM_PROT_EXECUTE, 0);
	/*NOTREACHED*/
}

/*
 * FPU error. Called by AST.
 */
fpexterrflt()
{
	register thread_t	thread = current_thread();

ASSERT_IPL(SPL0);
#if	NCPUS == 1
	/*
	 * Since FPU errors only occur on ESC or WAIT instructions,
	 * the current thread should own the FPU.  If it didn`t,
	 * we should have gotten the task-switched interrupt first.
	 */
	if (fp_thread != THREAD_NULL) {
	    panic("fpexterrflt");
		return;
	}

	/*
	 * Check if we got a context switch between the interrupt and the AST
	 * This can happen if the interrupt arrived after the FPU AST was
	 * checked. In this case, raise the exception in fp_load when this
	 * thread next time uses the FPU. Remember exception condition in
	 * fp_valid (extended boolean 2).
	 */
	if (fp_intr_thread != thread) {
		if (fp_intr_thread == THREAD_NULL) {
			panic("fpexterrflt: fp_intr_thread == THREAD_NULL");
			return;
		}
		fp_intr_thread->pcb->ims.ifps->fp_valid = 2;
		fp_intr_thread = THREAD_NULL;
		return;
	}
	fp_intr_thread = THREAD_NULL;
#else	/* NCPUS == 1 */
	/*
	 * Save the FPU state and turn off the FPU.
	 */
	fp_save(thread);
#endif	/* NCPUS == 1 */

	/*
	 * Raise FPU exception.
	 * Locking not needed on pcb->ims.ifps,
	 * since thread is running.
	 */
	i386_exception(EXC_ARITHMETIC,
		       EXC_I386_EXTERR,
		       thread->pcb->ims.ifps->fp_save_state.fp_status);
	/*NOTREACHED*/
}

/*
 * Save FPU state.
 *
 * Locking not needed:
 * .	if called from fpu_get_state, pcb already locked.
 * .	if called from fpnoextflt or fp_intr, we are single-cpu
 * .	otherwise, thread is running.
 */
fp_save(thread)
	register thread_t	thread;
{
	register pcb_t pcb = thread->pcb;
	register struct i386_fpsave_state *ifps = pcb->ims.ifps;

	if (ifps != 0 && !ifps->fp_valid) {
	    /* registers are in FPU */
	    ifps->fp_valid = TRUE;
	    fnsave(&ifps->fp_save_state);
	}
}

/*
 * Restore FPU state from PCB.
 *
 * Locking not needed; always called on the current thread.
 */
fp_load(thread)
	register thread_t	thread;
{
	register pcb_t pcb = thread->pcb;
	register struct i386_fpsave_state *ifps;

ASSERT_IPL(SPL0);
	ifps = pcb->ims.ifps;
	if (ifps == 0) {
	    ifps = (struct i386_fpsave_state *) zalloc(ifps_zone);
	    bzero(ifps, sizeof *ifps);
	    pcb->ims.ifps = ifps;
	    fpinit();
#if 1
/* 
 * I'm not sure this is needed. Does the fpu regenerate the interrupt in
 * frstor or not? Without this code we may miss some exceptions, with it
 * we might send too many exceptions.
 */
	} else if (ifps->fp_valid == 2) {
		/* delayed exception pending */

		ifps->fp_valid = TRUE;
		clear_fpu();
		/*
		 * Raise FPU exception.
		 * Locking not needed on pcb->ims.ifps,
		 * since thread is running.
		 */
		i386_exception(EXC_ARITHMETIC,
			       EXC_I386_EXTERR,
			       thread->pcb->ims.ifps->fp_save_state.fp_status);
		/*NOTREACHED*/
#endif
	} else {
	    frstor(ifps->fp_save_state);
	}
	ifps->fp_valid = FALSE;		/* in FPU */
}

/*
 * Allocate and initialize FP state for current thread.
 * Don't load state.
 *
 * Locking not needed; always called on the current thread.
 */
void
fp_state_alloc()
{
	pcb_t	pcb = current_thread()->pcb;
	struct i386_fpsave_state *ifps;

	ifps = (struct i386_fpsave_state *)zalloc(ifps_zone);
	bzero(ifps, sizeof *ifps);
	pcb->ims.ifps = ifps;

	ifps->fp_valid = TRUE;
	ifps->fp_save_state.fp_control = (0x037f
			& ~(FPC_IM|FPC_ZM|FPC_OM|FPC_PC))
			| (FPC_PC_53|FPC_IC_AFF);
	ifps->fp_save_state.fp_status = 0;
	ifps->fp_save_state.fp_tag = 0xffff;	/* all empty */
}

#if	AT386 || PS2
/*
 *	Handle a coprocessor error interrupt on the AT386.
 *	This comes in on line 5 of the slave PIC at SPL1.
 */
fpintr()
{
	spl_t	s;
	thread_t thread = current_thread();

ASSERT_IPL(SPL1);
	/*
	 * Turn off the extended 'busy' line.
	 */
	outb(0xf0, 0);

	/*
	 * Save the FPU context to the thread using it.
	 */
#if	NCPUS == 1
	if (fp_thread == THREAD_NULL) {
		printf("fpintr: FPU not belonging to anyone!\n");
		clear_ts();
		fninit();
		clear_fpu();
		return;
	}

	if (fp_thread != thread) {
	    /*
	     * FPU exception is for a different thread.
	     * When that thread again uses the FPU an exception will be
	     * raised in fp_load. Remember the condition in fp_valid (== 2).
	     */
	    clear_ts();
	    fp_save(fp_thread);
	    fp_thread->pcb->ims.ifps->fp_valid = 2;
	    fninit();
	    clear_fpu();
	    /* leave fp_intr_thread THREAD_NULL */
	    return;
	}
	if (fp_intr_thread != THREAD_NULL)
	    panic("fp_intr: already caught intr");
	fp_intr_thread = thread;
#endif	/* NCPUS == 1 */

	clear_ts();
	fp_save(thread);
	fninit();
	clear_fpu();

	/*
	 * Since we are running on the interrupt stack, we must
	 * signal the thread to take the exception when we return
	 * to user mode.  Use an AST to do this.
	 *
	 * Don`t set the thread`s AST field.  If the thread is
	 * descheduled before it takes the AST, it will notice
	 * the FPU error when it reloads its FPU state.
	 */
	s = splsched();
	ast_on(cpu_number(), AST_I386_FP);
	splx(s);
}
#endif	/* AT386 || PS2 */
