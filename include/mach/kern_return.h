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
/*
 *	File:	h/kern_return.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	Kernel return codes.
 *
 */

#ifndef	_MACH_KERN_RETURN_H_
#define _MACH_KERN_RETURN_H_

#include <mach/machine/kern_return.h>

#define KERN_SUCCESS			0

#define KERN_INVALID_ADDRESS		1
		/* Specified address is not currently valid.
		 */

#define KERN_PROTECTION_FAILURE		2
		/* Specified memory is valid, but does not permit the
		 * required forms of access.
		 */

#define KERN_NO_SPACE			3
		/* The address range specified is already in use, or
		 * no address range of the size specified could be
		 * found.
		 */

#define KERN_INVALID_ARGUMENT		4
		/* The function requested was not applicable to this
		 * type of argument, or an argument
		 */

#define KERN_FAILURE			5
		/* The function could not be performed.  A catch-all.
		 */

#define KERN_RESOURCE_SHORTAGE		6
		/* A system resource could not be allocated to fulfill
		 * this request.  This failure may not be permanent.
		 */

#define KERN_NOT_RECEIVER		7
		/* The task in question does not hold receive rights
		 * for the port argument.
		 */

#define KERN_NO_ACCESS			8
		/* Bogus access restriction.
		 */

#define KERN_MEMORY_FAILURE		9
		/* During a page fault, the target address refers to a
		 * memory object that has been destroyed.  This
		 * failure is permanent.
		 */

#define KERN_MEMORY_ERROR		10
		/* During a page fault, the memory object indicated
		 * that the data could not be returned.  This failure
		 * may be temporary; future attempts to access this
		 * same data may succeed, as defined by the memory
		 * object.
		 */

/*	KERN_ALREADY_IN_SET		11	obsolete */

#define KERN_NOT_IN_SET			12
		/* The receive right is not a member of a port set.
		 */

#define KERN_NAME_EXISTS		13
		/* The name already denotes a right in the task.
		 */

#define KERN_ABORTED			14
		/* The operation was aborted.  Ipc code will
		 * catch this and reflect it as a message error.
		 */

#define KERN_INVALID_NAME		15
		/* The name doesn't denote a right in the task.
		 */

#define	KERN_INVALID_TASK		16
		/* Target task isn't an active task.
		 */

#define KERN_INVALID_RIGHT		17
		/* The name denotes a right, but not an appropriate right.
		 */

#define KERN_INVALID_VALUE		18
		/* A blatant range error.
		 */

#define	KERN_UREFS_OVERFLOW		19
		/* Operation would overflow limit on user-references.
		 */

#define	KERN_INVALID_CAPABILITY		20
		/* The supplied (port) capability is improper.
		 */

#define KERN_RIGHT_EXISTS		21
		/* The task already has send or receive rights
		 * for the port under another name.
		 */

#define	KERN_INVALID_HOST		22
		/* Target host isn't actually a host.
		 */

#define KERN_MEMORY_PRESENT		23
		/* An attempt was made to supply "precious" data
		 * for memory that is already present in a
		 * memory object.
		 */

#define KERN_WRITE_PROTECTION_FAILURE	24
		/*
		 * A page was marked as VM_PROT_NOTIFY and an attempt was
		 * made to write it
		 */
#define KERN_TERMINATED			26
		/* Object has been terminated and is no longer available.
		 */

#endif	/* _MACH_KERN_RETURN_H_ */
