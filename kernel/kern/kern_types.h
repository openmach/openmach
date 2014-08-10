/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

#ifndef	_KERN_KERN_TYPES_H_
#define	_KERN_KERN_TYPES_H_

#include <mach/port.h>		/* for mach_port_t */

/*
 *	Common kernel type declarations.
 *	These are handles to opaque data structures defined elsewhere.
 *
 *	These types are recursively included in each other`s definitions.
 *	This file exists to export the common declarations to each
 *	of the definitions, and to other files that need only the
 *	type declarations.
 */

/*
 * Task structure, from kern/task.h
 */
typedef struct task *		task_t;
#define	TASK_NULL		((task_t) 0)

typedef	mach_port_t *		task_array_t;	/* should be task_t * */

/*
 * Thread structure, from kern/thread.h
 */
typedef	struct thread *		thread_t;
#define	THREAD_NULL		((thread_t) 0)

typedef	mach_port_t *		thread_array_t;	/* should be thread_t * */

/*
 * Processor structure, from kern/processor.h
 */
typedef	struct processor *	processor_t;
#define	PROCESSOR_NULL		((processor_t) 0)

/*
 * Processor set structure, from kern/processor.h
 */
typedef	struct processor_set *	processor_set_t;
#define	PROCESSOR_SET_NULL	((processor_set_t) 0)

#endif	/* _KERN_KERN_TYPES_H_ */
