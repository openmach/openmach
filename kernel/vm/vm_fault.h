/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	vm/vm_fault.h
 *
 *	Page fault handling module declarations.
 */

#ifndef	_VM_VM_FAULT_H_
#define _VM_VM_FAULT_H_

#include <mach/kern_return.h>

/*
 *	Page fault handling based on vm_object only.
 */

typedef	kern_return_t	vm_fault_return_t;
#define VM_FAULT_SUCCESS		0
#define VM_FAULT_RETRY			1
#define VM_FAULT_INTERRUPTED		2
#define VM_FAULT_MEMORY_SHORTAGE 	3
#define VM_FAULT_FICTITIOUS_SHORTAGE 	4
#define VM_FAULT_MEMORY_ERROR		5

extern void vm_fault_init();
extern vm_fault_return_t vm_fault_page();

extern void		vm_fault_cleanup();
/*
 *	Page fault handling based on vm_map (or entries therein)
 */

extern kern_return_t	vm_fault();
extern void		vm_fault_wire();
extern void		vm_fault_unwire();

extern kern_return_t	vm_fault_copy();	/* Copy pages from
						 * one object to another
						 */
#endif	_VM_VM_FAULT_H_
