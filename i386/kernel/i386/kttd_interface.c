/* 
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
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

#include "mach_ttd.h"

#if MACH_TTD

#include <mach/machine/eflags.h>

#include <kern/thread.h>
#include <kern/processor.h>
#include <mach/thread_status.h>
#include <mach/vm_param.h>
#include <i386/seg.h>
#include <sys/types.h>

#include <ttd/ttd_types.h>
#include <ttd/ttd_stub.h>
#include <machine/kttd_machdep.h>

/*
 * Shamelessly copied from the ddb sources:
 */
struct	 i386_saved_state *kttd_last_saved_statep;
struct	 i386_saved_state kttd_nested_saved_state;
unsigned last_kttd_sp;

struct i386_saved_state kttd_regs;	/* was ddb_regs */

extern int		kttd_debug;
extern boolean_t	kttd_enabled;
extern vm_offset_t	virtual_end;

#define	I386_BREAKPOINT	0xcc

/*
 *	kernel map
 */
extern vm_map_t	kernel_map;

boolean_t kttd_console_init(void)
{
	/*
	 * Get local machine's IP address via bootp.
	 */
	return(ttd_ip_bootp());
}

/*
 *	Execute a break instruction that will invoke ttd
 */
void kttd_break(void)
{
	if (!kttd_enabled)
		return;
	asm("int3");
}

/*
 * Halt all processors on the 386at (not really applicable).
 */
void kttd_halt_processors(void)
{
	/* XXX Fix for Sequent!!! */
	/* Only one on AT386, so ignore for now... */
}

/*
 * Determine whether or not the ehternet device driver supports
 * ttd.
 */
boolean_t kttd_supported(void)
{
	return ((int)ttd_get_packet != NULL);
}

/*
 * Return the ttd machine type for the i386at
 */
ttd_machine_type get_ttd_machine_type(void)
{
	return TTD_AT386;
}

void kttd_machine_getregs(struct i386_gdb_register_state *ttd_state)
{
	ttd_state->gs = kttd_regs.gs;
	ttd_state->fs = kttd_regs.fs;
	ttd_state->es = kttd_regs.es;
	ttd_state->ds = kttd_regs.ds;
	ttd_state->edi = kttd_regs.edi;
	ttd_state->esi = kttd_regs.esi;
	ttd_state->ebp = kttd_regs.ebp;

	/*
	 * This is set up to point to the right place in
	 * kttd_trap and .
	 */
	ttd_state->esp = kttd_regs.uesp;

	ttd_state->ebx = kttd_regs.ebx;
	ttd_state->edx = kttd_regs.edx;
	ttd_state->ecx = kttd_regs.ecx;
	ttd_state->eax = kttd_regs.eax;
	ttd_state->eip = kttd_regs.eip;
	ttd_state->cs = kttd_regs.cs;
	ttd_state->efl = kttd_regs.efl;
	ttd_state->ss = kttd_regs.ss;
}

void kttd_machine_setregs(struct i386_gdb_register_state *ttd_state)
{
	if (kttd_regs.gs != ttd_state->gs) {
		if (kttd_debug)
			printf("gs 0x%x:0x%x, ", kttd_regs.gs, ttd_state->gs);
		kttd_regs.gs = ttd_state->gs;
	}
	if (kttd_regs.fs != ttd_state->fs) {
		if (kttd_debug)
			printf("fs 0x%x:0x%x, ", kttd_regs.fs, ttd_state->fs);
		kttd_regs.fs = ttd_state->fs;
	}
	if (kttd_regs.es != ttd_state->es) {
		if (kttd_debug)
			printf("es 0x%x:0x%x, ", kttd_regs.es, ttd_state->es);
		kttd_regs.es = ttd_state->es;
	}
	if (kttd_regs.ds != ttd_state->ds) {
		if (kttd_debug)
			printf("ds 0x%x:0x%x, ", kttd_regs.ds, ttd_state->ds);
		kttd_regs.ds = ttd_state->ds;
	}
	if (kttd_regs.edi != ttd_state->edi) {
		if (kttd_debug)
			printf("edi 0x%x:0x%x, ", kttd_regs.edi, ttd_state->edi);
		kttd_regs.edi = ttd_state->edi;
	}
	if (kttd_regs.esi != ttd_state->esi) {
		if (kttd_debug)
			printf("esi 0x%x:0x%x, ", kttd_regs.esi, ttd_state->esi);
		kttd_regs.esi = ttd_state->esi;
	}
	if (kttd_regs.ebp != ttd_state->ebp) {
		if (kttd_debug)
			printf("ebp 0x%x:0x%x, ", kttd_regs.ebp, ttd_state->ebp);
		kttd_regs.ebp = ttd_state->ebp;
	}
	if (kttd_regs.ebx != ttd_state->ebx) {
		if (kttd_debug)
			printf("ebx 0x%x:0x%x, ", kttd_regs.ebx, ttd_state->ebx);
		kttd_regs.ebx = ttd_state->ebx;
	}
	if (kttd_regs.edx != ttd_state->edx) {
		if (kttd_debug)
			printf("edx 0x%x:0x%x, ", kttd_regs.edx, ttd_state->edx);
		kttd_regs.edx = ttd_state->edx;
	}
	if (kttd_regs.ecx != ttd_state->ecx) {
		if (kttd_debug) 
			printf("ecx 0x%x:0x%x, ", kttd_regs.ecx, ttd_state->ecx);
		kttd_regs.ecx = ttd_state->ecx;
	}
	if (kttd_regs.eax != ttd_state->eax) {
		if (kttd_debug)
			printf("eax 0x%x:0x%x, ", kttd_regs.eax, ttd_state->eax);
		kttd_regs.eax = ttd_state->eax;
	}
	if (kttd_regs.eip != ttd_state->eip) {
		if (kttd_debug)
			printf("eip 0x%x:0x%x, ", kttd_regs.eip, ttd_state->eip);
		kttd_regs.eip = ttd_state->eip;
	}
	if (kttd_regs.cs != ttd_state->cs) {
		if (kttd_debug)
			printf("cs 0x%x:0x%x, ", kttd_regs.cs, ttd_state->cs);
		kttd_regs.cs = ttd_state->cs;
	}
	if (kttd_regs.efl != ttd_state->efl) {
		if (kttd_debug)
			printf("efl 0x%x:0x%x, ", kttd_regs.efl, ttd_state->efl);
		kttd_regs.efl = ttd_state->efl;
	}
#if	0
	/*
	 * We probably shouldn't mess with the uesp or the ss? XXX
	 */
	if (kttd_regs.ss != ttd_state->ss) {
		if (kttd_debug)
			printf("ss 0x%x:0x%x, ", kttd_regs.ss, ttd_state->ss);
		kttd_regs.ss = ttd_state->ss;
	}
#endif	0

}

/*
 *	Enable a page for access, faulting it in if necessary
 */
boolean_t kttd_mem_access(vm_offset_t offset, vm_prot_t access)
{
	kern_return_t	code;

	/*
	 *	VM_MIN_KERNEL_ADDRESS if the beginning of equiv
	 *	mapped kernel memory.  virtual_end is the end.
	 *	If it's in between it's always accessible
	 */
	if (offset >= VM_MIN_KERNEL_ADDRESS && offset < virtual_end)
		return TRUE;

	if (offset >= virtual_end) {
		/*
		 *    	fault in the memory just to make sure we can access it
		 */
		if (kttd_debug)
			printf(">>>>>>>>>>Faulting in memory: 0x%x, 0x%x\n",
			       trunc_page(offset), access);
		code = vm_fault(kernel_map, trunc_page(offset), access, FALSE, 
				FALSE, (void (*)()) 0);
	}else{
		/*
		 * Check for user thread
		 */
#if	1
		if ((current_thread() != THREAD_NULL) && 
		    (current_thread()->task->map->pmap != kernel_pmap) &&
		    (current_thread()->task->map->pmap != PMAP_NULL)) {
			code = vm_fault(current_thread()->task->map,
					trunc_page(offset), access, FALSE,
					FALSE, (void (*)()) 0);
		}else{
			/*
			 * Invalid kernel address (below VM_MIN_KERNEL_ADDRESS)
			 */
			return FALSE;
		}
#else
		if (kttd_debug)
			printf("==========Would've tried to map in user area 0x%x\n",
			       trunc_page(offset));
		return FALSE;
#endif	/* 0 */
	}

	return (code == KERN_SUCCESS);
}

/*
 *	See if we modified the kernel text and if so flush the caches.
 *	This routine is never called with a range that crosses a page
 *	boundary.
 */
void kttd_flush_cache(vm_offset_t offset, vm_size_t length)
{
	/* 386 doesn't need this */
	return;
}

/*
 * Insert a breakpoint into memory.
 */
boolean_t kttd_insert_breakpoint(vm_address_t address,
				 ttd_saved_inst *saved_inst)
{
	/*
	 * Saved old memory data:
	 */
	*saved_inst = *(unsigned char *)address;

	/*
	 * Put in a Breakpoint:
	 */
	*(unsigned char *)address = I386_BREAKPOINT;

	return TRUE;
}

/*
 * Remove breakpoint from memory.
 */
boolean_t kttd_remove_breakpoint(vm_address_t address,
				 ttd_saved_inst saved_inst)
{
	/*
	 * replace it:
	 */
	*(unsigned char *)address = (saved_inst & 0xff);

	return TRUE;
}

/*
 * Set single stepping mode.  Assumes that program counter is set
 * to the location where single stepping is to begin.  The 386 is
 * an easy single stepping machine, ie. built into the processor.
 */
boolean_t kttd_set_machine_single_step(void)
{
	/* Turn on Single Stepping */
	kttd_regs.efl |= EFL_TF;

	return TRUE;
}

/*
 * Clear single stepping mode.
 */
boolean_t kttd_clear_machine_single_step(void)
{
	/* Turn off the trace flag */
	kttd_regs.efl &= ~EFL_TF;

	return TRUE;
}


/*
 * kttd_type_to_ttdtrap:
 *
 * Fills in the task and thread info structures with the reason
 * for entering the Teledebugger (bp, single step, pg flt, etc.)
 *
 */
void kttd_type_to_ttdtrap(int type)
{
	/* XXX Fill this in sometime for i386 */
}

/*
 * kttd_trap:
 *
 *  This routine is called from the trap or interrupt handler when a
 * breakpoint instruction is encountered or a single step operation
 * completes. The argument is a pointer to a machine dependent
 * saved_state structure that was built on the interrupt or kernel stack.
 *
 */
boolean_t kttd_trap(int	type, int code, struct i386_saved_state *regs)
{
	int s;

	if (kttd_debug)
		printf("kttd_TRAP, before splhigh()\n");

	/*
	 * TTD isn't supported by the driver.
	 *
	 * Try to switch off to kdb if it is resident.
	 * Otherwise just hang (this might be panic).
	 *
	 * Check to make sure that TTD is supported.
	 * (Both by the machine's driver's, and bootp if using ether). 
	 */
	if (!kttd_supported()) {
		kttd_enabled = FALSE;
		return FALSE;
	}

	s = splhigh();

	/*
	 * We are already in TTD!
	 */
	if (++kttd_active > MAX_KTTD_ACTIVE) {
		printf("kttd_trap: RE-ENTERED!!!\n");
	}

	if (kttd_debug)
		printf("kttd_TRAP, after splhigh()\n");

	/*  Should switch to kttd's own stack here. */

	kttd_regs = *regs;

	if ((regs->cs & 0x3) == 0) {
	    /*
	     * Kernel mode - esp and ss not saved
	     */
	    kttd_regs.uesp = (int)&regs->uesp;	/* kernel stack pointer */
	    kttd_regs.ss   = KERNEL_DS;
	}

	/*
	 * If this was not entered via an interrupt (type != -1)
	 * then we've entered via a bpt, single, etc. and must
	 * set the globals.
	 *
	 * Setup the kttd globals for entry....
	 */
	if (type != -1) {
		kttd_current_request = NULL;
		kttd_current_length = 0;
		kttd_current_kmsg = NULL;
		kttd_run_status = FULL_STOP;
	}else{
		/*
		 * We know that we can only get here if we did a kttd_intr
		 * since it's the way that we are called with type -1 (via
		 * the trampoline), so we don't have to worry about entering
		 * from Cntl-Alt-D like the mips does.
		 */
		/*
		 * Perform sanity check!
		 */
		if ((kttd_current_request == NULL) ||
		    (kttd_current_length == 0) ||
		    (kttd_current_kmsg == NULL) ||
		    (kttd_run_status != ONE_STOP)) {

			printf("kttd_trap: INSANITY!!!\n");
		}
	}

	kttd_task_trap(type, code, (regs->cs & 0x3) != 0);

	regs->eip    = kttd_regs.eip;
	regs->efl    = kttd_regs.efl;
	regs->eax    = kttd_regs.eax;
	regs->ecx    = kttd_regs.ecx;
	regs->edx    = kttd_regs.edx;
	regs->ebx    = kttd_regs.ebx;
	if (regs->cs & 0x3) {
	    /*
	     * user mode - saved esp and ss valid
	     */
	    regs->uesp = kttd_regs.uesp;		/* user stack pointer */
	    regs->ss   = kttd_regs.ss & 0xffff;	/* user stack segment */
	}
	regs->ebp    = kttd_regs.ebp;
	regs->esi    = kttd_regs.esi;
	regs->edi    = kttd_regs.edi;
	regs->es     = kttd_regs.es & 0xffff;
	regs->cs     = kttd_regs.cs & 0xffff;
	regs->ds     = kttd_regs.ds & 0xffff;
	regs->fs     = kttd_regs.fs & 0xffff;
	regs->gs     = kttd_regs.gs & 0xffff;

	if (--kttd_active < MIN_KTTD_ACTIVE)
		printf("ttd_trap: kttd_active < 0\n");

	if (kttd_debug) {
		printf("Leaving kttd_trap, kttd_active = %d\n", kttd_active);
	}

	/*
	 * Only reset this if we entered kttd_trap via an async trampoline.
	 */
	if (type == -1) {
		if (kttd_run_status == RUNNING)
			printf("kttd_trap: $$$$$ run_status already RUNNING! $$$$$\n");
		kttd_run_status = RUNNING;
	}

	/* Is this right? XXX */
	kttd_run_status = RUNNING;

	(void) splx(s);

	/*
	 * Return true, that yes we handled the trap.
	 */
	return TRUE;
}

/*
 *	Enter KTTD through a network packet trap.
 *	We show the registers as of the network interrupt
 *	instead of those at its call to KDB.
 */
struct int_regs {
	int	gs;
	int	fs;
	int	edi;
	int	esi;
	int	ebp;
	int	ebx;
	struct i386_interrupt_state *is;
};

void
kttd_netentry(int_regs)
	struct int_regs	*int_regs;
{
	struct i386_interrupt_state *is = int_regs->is;
	int	s;

	if (kttd_debug)
		printf("kttd_NETENTRY before slphigh()\n");

	s = splhigh();

	if (kttd_debug)
		printf("kttd_NETENTRY after slphigh()\n");

	if (is->cs & 0x3) {
	    /*
	     * Interrupted from User Space
	     */
	    kttd_regs.uesp = ((int *)(is+1))[0];
	    kttd_regs.ss   = ((int *)(is+1))[1];
	}
	else {
	    /*
	     * Interrupted from Kernel Space
	     */
	    kttd_regs.ss  = KERNEL_DS;
	    kttd_regs.uesp= (int)(is+1);
	}
	kttd_regs.efl = is->efl;
	kttd_regs.cs  = is->cs;
	kttd_regs.eip = is->eip;
	kttd_regs.eax = is->eax;
	kttd_regs.ecx = is->ecx;
	kttd_regs.edx = is->edx;
	kttd_regs.ebx = int_regs->ebx;
	kttd_regs.ebp = int_regs->ebp;
	kttd_regs.esi = int_regs->esi;
	kttd_regs.edi = int_regs->edi;
	kttd_regs.ds  = is->ds;
	kttd_regs.es  = is->es;
	kttd_regs.fs  = int_regs->fs;
	kttd_regs.gs  = int_regs->gs;

	kttd_active++;
	kttd_task_trap(-1, 0, (kttd_regs.cs & 0x3) != 0);
	kttd_active--;

	if (kttd_regs.cs & 0x3) {
	    ((int *)(is+1))[0] = kttd_regs.uesp;
	    ((int *)(is+1))[1] = kttd_regs.ss & 0xffff;
	}
	is->efl = kttd_regs.efl;
	is->cs  = kttd_regs.cs & 0xffff;
	is->eip = kttd_regs.eip;
	is->eax = kttd_regs.eax;
	is->ecx = kttd_regs.ecx;
	is->edx = kttd_regs.edx;
	int_regs->ebx = kttd_regs.ebx;
	int_regs->ebp = kttd_regs.ebp;
	int_regs->esi = kttd_regs.esi;
	int_regs->edi = kttd_regs.edi;
	is->ds  = kttd_regs.ds & 0xffff;
	is->es  = kttd_regs.es & 0xffff;
	int_regs->fs = kttd_regs.fs & 0xffff;
	int_regs->gs = kttd_regs.gs & 0xffff;

	if (kttd_run_status == RUNNING)
		printf("kttd_netentry: %%%%% run_status already RUNNING! %%%%%\n");
	kttd_run_status = RUNNING;

	(void) splx(s);
}

#endif MACH_TTD
