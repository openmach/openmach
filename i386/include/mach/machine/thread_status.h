/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *	File:	thread_status.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	This file contains the structure definitions for the thread
 *	state as applied to I386 processors.
 */

#ifndef	_MACH_I386_THREAD_STATUS_H_
#define _MACH_I386_THREAD_STATUS_H_

#include <mach/machine/fp_reg.h>
/*
 *	i386_thread_state	this is the structure that is exported
 *				to user threads for use in status/mutate
 *				calls.  This structure should never
 *				change.
 *
 *	i386_float_state	exported to use threads for access to 
 *				floating point registers. Try not to 
 *				change this one, either.
 *
 *	i386_isa_port_map_state	exported to user threads to allow
 *				selective in/out operations
 *
 */

#define i386_THREAD_STATE	1
#define i386_FLOAT_STATE	2
#define i386_ISA_PORT_MAP_STATE	3
#define	i386_V86_ASSIST_STATE	4
#define	i386_REGS_SEGS_STATE	5

/*
 * This structure is used for both
 * i386_THREAD_STATE and i386_REGS_SEGS_STATE.
 */
struct i386_thread_state {
	unsigned int	gs;
	unsigned int	fs;
	unsigned int	es;
	unsigned int	ds;
	unsigned int	edi;
	unsigned int	esi;
	unsigned int	ebp;
	unsigned int	esp;
	unsigned int	ebx;
	unsigned int	edx;
	unsigned int	ecx;
	unsigned int	eax;
	unsigned int	eip;
	unsigned int	cs;
	unsigned int	efl;
	unsigned int	uesp;
	unsigned int	ss;
};
#define i386_THREAD_STATE_COUNT	(sizeof (struct i386_thread_state)/sizeof(unsigned int))

/* 
 * Floating point state.
 *
 * fpkind tells in what way floating point operations are supported.  
 * See the values for fp_kind in <mach/i386/fp_reg.h>.
 * 
 * If the kind is FP_NO, then calls to set the state will fail, and 
 * thread_getstatus will return garbage for the rest of the state.
 * If "initialized" is false, then the rest of the state is garbage.  
 * Clients can set "initialized" to false to force the coprocessor to 
 * be reset.
 * "exc_status" is non-zero if the thread has noticed (but not 
 * proceeded from) a coprocessor exception.  It contains the status 
 * word with the exception bits set.  The status word in "fp_status" 
 * will have the exception bits turned off.  If an exception bit in 
 * "fp_status" is turned on, then "exc_status" should be zero.  This 
 * happens when the coprocessor exception is noticed after the system 
 * has context switched to some other thread.
 * 
 * If kind is FP_387, then "state" is a i387_state.  Other kinds might
 * also use i387_state, but somebody will have to verify it (XXX).
 * Note that the registers are ordered from top-of-stack down, not
 * according to physical register number.
 */

#define FP_STATE_BYTES \
	(sizeof (struct i386_fp_save) + sizeof (struct i386_fp_regs))

struct i386_float_state {
	int		fpkind;			/* FP_NO..FP_387 (readonly) */
	int		initialized;
	unsigned char	hw_state[FP_STATE_BYTES]; /* actual "hardware" state */
	int		exc_status;		/* exception status (readonly) */
};
#define i386_FLOAT_STATE_COUNT (sizeof(struct i386_float_state)/sizeof(unsigned int))


#define PORT_MAP_BITS 0x400
struct i386_isa_port_map_state {
	unsigned char	pm[PORT_MAP_BITS>>3];
};

#define i386_ISA_PORT_MAP_STATE_COUNT (sizeof(struct i386_isa_port_map_state)/sizeof(unsigned int))

/*
 * V8086 assist supplies a pointer to an interrupt
 * descriptor table in task space.
 */
struct i386_v86_assist_state {
	unsigned int	int_table;	/* interrupt table address */
	int		int_count;	/* interrupt table size */
};

struct v86_interrupt_table {
	unsigned int	count;	/* count of pending interrupts */
	unsigned short	mask;	/* ignore this interrupt if true */
	unsigned short	vec;	/* vector to take */
};

#define	i386_V86_ASSIST_STATE_COUNT \
	    (sizeof(struct i386_v86_assist_state)/sizeof(unsigned int))

#endif	/* _MACH_I386_THREAD_STATUS_H_ */
