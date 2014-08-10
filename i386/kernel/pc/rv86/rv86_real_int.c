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

#include <mach/machine/seg.h>
#include <mach/machine/proc_reg.h>
#include <mach/machine/far_ptr.h>
#include <mach/machine/eflags.h>

#include "vm_param.h"
#include "real.h"
#include "real_tss.h"
#include "cpu.h"
#include "debug.h"


/*

	There seem to be three main ways to handle v86 mode:

	* The v86 environment is just an extension of the normal kernel environment:
	  you can switch to and from v86 mode just as you can change any other processor state.
	  You always keep running on the separate "logical" stack,
	  which is the kernel stack when running in protected mode,
	  or the user stack when running in v86 mode.
	  When in v86 mode, the "actual" kernel stack is just a stub
	  big enough to switch back to the "normal" kernel stack,
	  which was being used as the user stack while running in v86 mode.
	  Thus, v86 and protected-mode "segments" of stack data
	  can be interleaved together on the same logical stack.

		- To make a real int call from kernel pmode,
		  switch to v86 mode and execute an int instruction,
		  then switch back to protected mode.

		- To reflect an interrupt to v86 mode:

			> If the processor was running in v86 mode,
			  just adjust the kernel and user stacks
			  to emulate a real-mode interrupt, and return.

			> If the processor was running in pmode,
			  switch to v86 mode and re-trigger the interrupt
			  with a software int instruction.

		- To handle an interrupt in pmode:

			> If the processor was running in v86 mode,
			  switch from the stub stack to the user stack that was in use
			  (could be different from the stack we set originally,
			  because BIOS/DOS code might have switched stacks!),
			  call the interrupt handler, switch back, and return.

			> If the processor was running in pmode,
			  just call the interrupt handler and return.

	  This method only works if the whole "kernel" is <64KB
	  and generally compatible with real-mode execution.
	  This is the model my DOS extender currently uses.

	  One major disadvantage of this method
	  is that interrupt handlers can't run "general" protected-mode code,
	  such as typical code compiled by GCC.
	  This is because, if an interrupt occurs while in v86 mode,
	  the v86-mode ss:sp may point basically anywhere in the low 1MB,
	  and it therefore it can't be used directly as a pmode stack;
	  and the only other stack available is the miniscule stub stack.
	  Since "general" protected-mode code expects a full-size stack
	  with an SS equal to the normal protected-mode DS,
	  neither of these available stacks will suffice.
	  It is impossible to switch back to the original kernel stack
	  because arbitrary DOS or BIOS code might have switched from it
	  to a different stack somewhere else in the low 1MB,
	  and we have no way of telling where the SP was when that happened.
	  The upshot is that interrupt handlers must be extremely simple;
	  in MOSS, all they do is post a signal to "the process,"
	  and return immediately without actually handling the interrupt.

	* The v86 environment is a separate "task" with its own user and kernel stacks;
	  you switch back and forth as if between multiple ordinary tasks,
	  the tasks can preempt each other, go idle waiting for events, etc.

		- To make a real int call from kernel pmode,
		  the task making the call essentially does a synchronous IPC to the v86 task.
		  If the v86 task is busy with another request or a reflected interrupt,
		  the calling task will go idle until the v86 task is available.

		- Reflecting an interrupt to v86 mode
		  basically amounts to sending a Unix-like "signal" to the v86 task:

			> If the processor was running in the v86 task,
			  just adjust the kernel and user stacks
			  to emulate a real-mode interrupt, and return.

			> If the processor was running in a protected-mode task
			  (or another v86-mode task),
			  post a signal to the v86 task, wake it up if it's asleep,
			  and invoke the scheduler to switch to the v86 task
			  if it has a higher priority than the currently running task.

		- To handle an interrupt in pmode,
		  just call the interrupt handler and return.
		  It doesn't matter whether the interrupt was from v86 or pmode,
		  because the kernel stacks look the same in either case.

	  One big problem with this method is that if interrupts are to be handled in v86 mode,
	  all the typical problems of handling interrupts in user-mode tasks pop up.
	  In particular, an interrupt can now cause preemption,
	  so this will break an interruptible but nonpreemptible environment.
	  (The problem is not that the interrupted task is "preempted"
	  to switch temporarily to the v86 task to handle the interrupt;
	  the problem is that when the v86 task is done handling the interrupt,
	  the scheduler will be invoked and some task other than the interrupted task may be run.)

	  Of course, this is undoubtedly the right solution
	  if that's the interrupt model the OS is using anyway
	  (i.e. if the OS already supports user-level protected-mode interrupts).

	* A bastardization of the two above approaches:
	  treat the v86 environment as a separate "task",
	  but a special one that doesn't behave at all like other tasks.
	  The v86 "task" in this case is more of an "interrupt co-stack"
	  that grows and shrinks alongside the normal interrupt stack
	  (or the current kernel stack, if interrupts are handled on the kernel stack).
	  Interrupts and real calls can cause switches between these two interrupt stacks,
	  but they can't cause preemption in the normal sense.
	  The route taken while building the stacks is exactly the opposite
	  the route taken while tearing it down.

	  Now two "kernel stack pointers" have to be maintained all the time instead of one.
	  When running in protected mode:

	  	- The ESP register contains the pmode stack pointer.
		- Some global variable contains the v86 stack pointer.

	  When running in v86 mode:

	  	- The ESP register contains the v86 stack pointer.
		  (Note that BIOS/DOS code can switch stacks,
		  so at any given time it may point practically anywhere!)
		- The current tss's esp0 contains the pmode stack pointer.

	  Whenever a switch is made, a stack frame is placed on the new co-stack
	  indicating that the switch was performed.

		- To make a real int call from kernel pmode,
		  build a real-mode interrupt stack frame on the v86 interrupt stack,
		  build a v86-mode trap stack frame on the pmode stack,
		  set the tss's esp0 to point to the end of that stack frame,
		  and iret from it.
		  Then when the magic "done-with-real-call" int instruction is hit,
		  the pmode interrupt handler will see it
		  and know to simply destroy the v86 trap stack on the pmode stack.

		- Handling an interrupt can always be thought of as going "through" pmode:
		  switching from the v86 stack to the pmode stack
		  if the processor was in v86 mode when the interrupt was taken,
		  and switching from the pmode stack back to the v86 stack as described above
		  if the interrupt is to be reflected to v86 mode.

		  Of course, optimized paths are possible:

		- To reflect an interrupt to v86 mode:

			> If the processor was running in v86 mode,
			  just adjust the kernel and user stack frames and return.

			> If the processor was running in pmode,
			  do as described above for explicit real int calls.

		- To handle an interrupt in pmode:

			> If the processor was running in v86 mode,
			  switch to the pmode stack,
			  stash the old v86 stack pointer variable on the pmode stack,
			  and set the v86 stack pointer variable to the new location.
			  Call the interrupt handler,
			  then tear down everything and return to v86 mode.

	Observation:
	In the first and third models,
	explicit real int calls are entirely symmetrical
	to hardware interrupts from pmode to v86 mode.
	This is valid because of the interruptible but nonpreemptible model:
	no scheduling is involved, and the stack(s) will always be torn down
	in exactly the opposite order in which they were built up.
	In the second model,
	explicit real calls are quite different,
	because the BIOS is interruptible but nonpreemptible:
	you can reflect an interrupt into the v86 task at any time,
	but you can only make an explicit request to that task when it's ready
	(i.e. no other requests or interrupts are outstanding).

*/



#define RV86_USTACK_SIZE 1024

vm_offset_t rv86_ustack_pa;
vm_offset_t rv86_return_int_pa;
struct far_pointer_32 rv86_usp;
struct far_pointer_16 rv86_rp;

void rv86_real_int(int intnum, struct real_call_data *rcd)
{
	unsigned short old_tr;
	unsigned int old_eflags;

	/* If this is the first time this routine is being called,
	   initialize the kernel stack.  */
	if (!rv86_ustack_pa)
	{
		rv86_ustack_pa = 0xa0000 - RV86_USTACK_SIZE; /* XXX */

		assert(rv86_ustack_pa < 0x100000);

		/* Use the top two bytes of the ustack for an 'int $0xff' instruction.  */
		rv86_return_int_pa = rv86_ustack_pa + RV86_USTACK_SIZE - 2;
		*(short*)phystokv(rv86_return_int_pa) = 0xffcd;

		/* Set up the v86 stack pointer.  */
		rv86_usp.seg = rv86_rp.seg = rv86_ustack_pa >> 4;
		rv86_usp.ofs = rv86_rp.ofs = (rv86_ustack_pa & 0xf) + RV86_USTACK_SIZE - 2;

		/* Pre-allocate a real-mode interrupt stack frame.  */
		rv86_usp.ofs -= 6;
	}

	/* Make sure interrupts are disabled.  */
	old_eflags = get_eflags();

	/* Switch to the TSS to use in v86 mode.  */
	old_tr = get_tr();
	cpu[0].tables.gdt[REAL_TSS_IDX].access &= ~ACC_TSS_BUSY;
	set_tr(REAL_TSS);

	asm volatile("
		pushl	%%ebp
		pushl	%%eax
		call	rv86_real_int_asm
		popl	%%eax
		popl	%%ebp
	" :
	  : "a" (rcd), "S" (intnum)
	  : "eax", "ebx", "ecx", "edx", "esi", "edi");

	/* Switch to the original TSS.  */
	cpu[0].tables.gdt[old_tr/8].access &= ~ACC_TSS_BUSY;
	set_tr(old_tr);

	/* Restore the original processor flags.  */
	set_eflags(old_eflags);
}

void (*real_int)(int intnum, struct real_call_data *rcd) = rv86_real_int;

