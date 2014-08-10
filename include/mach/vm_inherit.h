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
 *	File:	mach/vm_inherit.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Virtual memory map inheritance definitions.
 *
 */

#ifndef	_MACH_VM_INHERIT_H_
#define	_MACH_VM_INHERIT_H_

/*
 *	Types defined:
 *
 *	vm_inherit_t	inheritance codes.
 */

typedef int		vm_inherit_t;	/* might want to change this */

/*
 *	Enumeration of valid values for vm_inherit_t.
 */

#define	VM_INHERIT_SHARE	((vm_inherit_t) 0)	/* share with child */
#define	VM_INHERIT_COPY		((vm_inherit_t) 1)	/* copy into child */
#define VM_INHERIT_NONE		((vm_inherit_t) 2)	/* absent from child */

#define VM_INHERIT_DEFAULT	VM_INHERIT_COPY

#endif	/* _MACH_VM_INHERIT_H_ */
