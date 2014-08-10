/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
 * HISTORY
 * $Log: pc_sample.h,v $
 * Revision 1.1  1994/11/02  02:24:15  law
 * Initial revision
 *
 * Revision 2.2  93/11/17  19:06:01  dbg
 * 	Moved kernel internal definitions here from mach/pc_sample.h.
 * 	[93/09/24            dbg]
 * 
 */

/*
 *	Kernel definitions for PC sampling.
 */
#ifndef	_KERN_PC_SAMPLE_H_
#define	_KERN_PC_SAMPLE_H_

#include <mach/pc_sample.h>
#include <mach/machine/vm_types.h>
#include <kern/kern_types.h>
#include <kern/macro_help.h>

/*
 *	Control structure for sampling, included in
 *	threads and tasks.  If sampletypes is 0, no
 *	sampling is done.
 */

struct sample_control {
    vm_offset_t		buffer;
    unsigned int	seqno;
    sampled_pc_flavor_t sampletypes;
};

typedef struct sample_control	sample_control_t;

/*
 *	Routines to take PC samples.
 */
extern void take_pc_sample(
	thread_t	thread,
	sample_control_t *cp,
	sampled_pc_flavor_t flavor);

/*
 *	Macro to do quick flavor check for sampling,
 *	on both threads and tasks.
 */
#define	take_pc_sample_macro(thread, flavor) \
    MACRO_BEGIN \
	task_t	task; \
 \
	if ((thread)->pc_sample.sampletypes & (flavor)) \
	    take_pc_sample((thread), &(thread)->pc_sample, (flavor)); \
 \
	task = (thread)->task; \
	if (task->pc_sample.sampletypes & (flavor)) \
	    take_pc_sample((thread), &task->pc_sample, (flavor)); \
    MACRO_END

#endif	/* _KERN_PC_SAMPLE_H_ */
