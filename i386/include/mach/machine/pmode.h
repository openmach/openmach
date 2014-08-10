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
#ifndef _MACH_I386_PMODE_H_
#define _MACH_I386_PMODE_H_

#include <mach/inline.h>
#include <mach/macro_help.h>
#include <mach/machine/proc_reg.h>



/* Enter protected mode on i386 machines.
   Assumes:
	* Running in real mode.
	* Interrupts are turned off.
	* A20 is enabled (if on a PC).
	* A suitable GDT is already loaded.

   You must supply a 16-bit code segment
   equivalent to the real-mode code segment currently in use.

   You must reload all segment registers except CS
   immediately after invoking this macro.
*/
#define i16_enter_pmode(prot_cs)					\
MACRO_BEGIN								\
	/* Switch to protected mode.  */				\
	asm volatile("
		movl	%0,%%cr0
		ljmp	%1,$1f
	1:
	" : : "r" (i16_get_cr0() | CR0_PE), "i" (KERNEL_16_CS));	\
MACRO_END



/* Leave protected mode and return to real mode.
   Assumes:
	* Running in protected mode
	* Interrupts are turned off.
	* Paging is turned off.
	* All currently loaded segment registers
	  contain 16-bit segments with limits of 0xffff.

   You must supply a real-mode code segment
   equivalent to the protected-mode code segment currently in use.

   You must reload all segment registers except CS
   immediately after invoking this function.
*/
MACH_INLINE i16_leave_pmode(int real_cs)
{
	/* Switch back to real mode.
	   Note: switching to the real-mode code segment
	   _must_ be done with an _immediate_ far jump,
	   not an indirect far jump.  At least on my Am386DX/40,
	   an indirect far jump leaves the code segment read-only.  */
	{
		extern unsigned short real_jmp[];

		real_jmp[3] = real_cs;
		asm volatile("
			movl	%0,%%cr0
			jmp	1f
		1:
		real_jmp:
		_real_jmp:
			ljmp	$0,$1f
		1:
		" : : "r" (i16_get_cr0() & ~CR0_PE));
	}
}



#endif _MACH_I386_PMODE_H_
