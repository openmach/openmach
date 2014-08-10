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

#ifndef	_MACH_DEBUG_ZONE_INFO_H_
#define _MACH_DEBUG_ZONE_INFO_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>

/*
 *	Remember to update the mig type definitions
 *	in mach_debug_types.defs when adding/removing fields.
 */

#define ZONE_NAME_MAX_LEN		80

typedef struct zone_name {
	char		zn_name[ZONE_NAME_MAX_LEN];
} zone_name_t;

typedef zone_name_t *zone_name_array_t;


typedef struct zone_info {
	integer_t	zi_count;	/* Number of elements used now */
	vm_size_t	zi_cur_size;	/* current memory utilization */
	vm_size_t	zi_max_size;	/* how large can this zone grow */
	vm_size_t	zi_elem_size;	/* size of an element */
	vm_size_t	zi_alloc_size;	/* size used for more memory */
/*boolean_t*/integer_t	zi_pageable;	/* zone pageable? */
/*boolean_t*/integer_t	zi_sleepable;	/* sleep if empty? */
/*boolean_t*/integer_t	zi_exhaustible;	/* merely return if empty? */
/*boolean_t*/integer_t	zi_collectable;	/* garbage collect elements? */
} zone_info_t;

typedef zone_info_t *zone_info_array_t;

#endif	_MACH_DEBUG_ZONE_INFO_H_
