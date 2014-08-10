/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the nema IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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

#include <mach/machine/asm.h>
#include <mach/machine/eflags.h>
#include <mach/machine/trap.h>
#include <mach/machine/seg.h>

#include "trap.h"
#include "idt_inittab.h"
#include "debug.h"

/*
 * This is a default idt_inittab
 * that simply invokes panic_trap on any trap.
 * All gates are interrupt gates,
 * so that interrupts will be immediately turned off.
 */


/*
 * No error code.  Clear error code and push trap number.
 */
#define	EXCEPTION(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_INTR_GATE);\
ENTRY(name)				;\
	pushl	$(0)			;\
	pushl	$(n)			;\
	jmp	alltraps

/*
 * User-accessible exception.  Otherwise, same as above.
 */
#define	EXCEP_USR(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_U|ACC_INTR_GATE);\
ENTRY(name)				;\
	pushl	$(0)			;\
	pushl	$(n)			;\
	jmp	alltraps

/*
 * Error code has been pushed.  Just push trap number.
 */
#define	EXCEP_ERR(n,name) \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_INTR_GATE);\
ENTRY(name)				;\
	pushl	$(n)			;\
	jmp	alltraps

/*
 * Special interrupt code: dispatches to a unique entrypoint,
 * not defined automatically here.
 */
#define	EXCEP_SPC(n,name)  \
	IDT_ENTRY(n,EXT(name),ACC_PL_K|ACC_INTR_GATE)


IDT_INITTAB_BEGIN

EXCEPTION(0x00,t_zero_div)
EXCEPTION(0x01,t_debug)
EXCEP_USR(0x03,t_int3)
EXCEP_USR(0x04,t_into)
EXCEP_USR(0x05,t_bounds)
EXCEPTION(0x06,t_invop)
EXCEPTION(0x07,t_nofpu)
EXCEPTION(0x08,a_dbl_fault)
EXCEPTION(0x09,a_fpu_over)
EXCEP_ERR(0x0a,a_inv_tss)
EXCEP_ERR(0x0b,t_segnp)
EXCEP_ERR(0x0c,t_stack_fault)
EXCEP_ERR(0x0d,t_gen_prot)
EXCEP_ERR(0x0e,t_page_fault)
EXCEPTION(0x0f,t_trap_0f)
EXCEPTION(0x10,t_fpu_err)
EXCEPTION(0x11,t_trap_11)
EXCEPTION(0x12,t_trap_12)
EXCEPTION(0x13,t_trap_13)
EXCEPTION(0x14,t_trap_14)
EXCEPTION(0x15,t_trap_15)
EXCEPTION(0x16,t_trap_16)
EXCEPTION(0x17,t_trap_17)
EXCEPTION(0x18,t_trap_18)
EXCEPTION(0x19,t_trap_19)
EXCEPTION(0x1a,t_trap_1a)
EXCEPTION(0x1b,t_trap_1b)
EXCEPTION(0x1c,t_trap_1c)
EXCEPTION(0x1d,t_trap_1d)
EXCEPTION(0x1e,t_trap_1e)
EXCEPTION(0x1f,t_trap_1f)

IDT_INITTAB_END

alltraps:
	pusha
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	jmp	EXT(trap_handler)

