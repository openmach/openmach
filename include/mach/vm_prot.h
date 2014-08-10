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
 *	File:	mach/vm_prot.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Virtual memory protection definitions.
 *
 */

#ifndef	_MACH_VM_PROT_H_
#define	_MACH_VM_PROT_H_

/*
 *	Types defined:
 *
 *	vm_prot_t		VM protection values.
 */

typedef int		vm_prot_t;

/*
 *	Protection values, defined as bits within the vm_prot_t type
 */

#define	VM_PROT_NONE	((vm_prot_t) 0x00)

#define VM_PROT_READ	((vm_prot_t) 0x01)	/* read permission */
#define VM_PROT_WRITE	((vm_prot_t) 0x02)	/* write permission */
#define VM_PROT_EXECUTE	((vm_prot_t) 0x04)	/* execute permission */

/*
 *	The default protection for newly-created virtual memory
 */

#define VM_PROT_DEFAULT	(VM_PROT_READ|VM_PROT_WRITE)

/*
 *	The maximum privileges possible, for parameter checking.
 */

#define VM_PROT_ALL	(VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE)

/*
 *	An invalid protection value.
 *	Used only by memory_object_lock_request to indicate no change
 *	to page locks.  Using -1 here is a bad idea because it
 *	looks like VM_PROT_ALL and then some.
 */
#define VM_PROT_NO_CHANGE	((vm_prot_t) 0x08)

/*
 *	This protection value says whether special notification is to be used.
 */
#define VM_PROT_NOTIFY		((vm_prot_t) 0x10)
#endif	/* _MACH_VM_PROT_H_ */
