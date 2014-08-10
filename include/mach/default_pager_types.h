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

#ifndef	_MACH_DEFAULT_PAGER_TYPES_H_
#define _MACH_DEFAULT_PAGER_TYPES_H_

/*
 *	Remember to update the mig type definitions
 *	in default_pager_types.defs when adding/removing fields.
 */

typedef struct default_pager_info {
	vm_size_t dpi_total_space;	/* size of backing store */
	vm_size_t dpi_free_space;	/* how much of it is unused */
	vm_size_t dpi_page_size;	/* the pager's vm page size */
} default_pager_info_t;


typedef struct default_pager_object {
	vm_offset_t dpo_object;		/* object managed by the pager */
	vm_size_t dpo_size;		/* backing store used for the object */
} default_pager_object_t;

typedef default_pager_object_t *default_pager_object_array_t;


typedef struct default_pager_page {
	vm_offset_t dpp_offset;		/* offset of the page in its object */
} default_pager_page_t;

typedef default_pager_page_t *default_pager_page_array_t;

typedef char default_pager_filename_t[256];

#endif	_MACH_DEFAULT_PAGER_TYPES_H_
