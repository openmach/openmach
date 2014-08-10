/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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

#ifndef	_KERN_SYSCALL_EMULATION_H_
#define	_KERN_SYSCALL_EMULATION_H_

#ifndef	ASSEMBLER
#include <mach/machine/vm_types.h>
#include <kern/lock.h>

typedef	vm_offset_t	eml_routine_t;

typedef struct eml_dispatch {
	decl_simple_lock_data(, lock)	/* lock for reference count */
	int		ref_count;	/* reference count */
	int 		disp_count; 	/* count of entries in vector */
	int		disp_min;	/* index of lowest entry in vector */
	eml_routine_t	disp_vector[1];	/* first entry in array of dispatch */
					/* routines (array has disp_count */
					/* elements) */
} *eml_dispatch_t;

typedef vm_offset_t	*emulation_vector_t; /* Variable-length array */

#define EML_ROUTINE_NULL	(eml_routine_t)0
#define EML_DISPATCH_NULL	(eml_dispatch_t)0

#define	EML_SUCCESS		(0)

#define	EML_MOD			(err_kern|err_sub(2))
#define	EML_BAD_TASK		(EML_MOD|0x0001)
#define	EML_BAD_CNT		(EML_MOD|0x0002)
#endif	ASSEMBLER

#endif	_KERN_SYSCALL_EMULATION_H_
