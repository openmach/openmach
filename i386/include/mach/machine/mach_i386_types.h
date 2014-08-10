/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * Type definitions for i386 interface routines.
 */

#ifndef	_MACH_MACH_I386_TYPES_H_
#define	_MACH_MACH_I386_TYPES_H_

/*
 * Array of devices.
 */
typedef	device_t	*device_list_t;

/*
 * i386 segment descriptor.
 */
struct descriptor {
	unsigned int	low_word;
	unsigned int	high_word;
};

typedef struct descriptor descriptor_t;
typedef	struct descriptor *descriptor_list_t;

#endif	/* _MACH_MACH_I386_TYPES_H_ */
