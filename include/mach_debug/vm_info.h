/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	File:	mach_debug/vm_info.h
 *	Author:	Rich Draves
 *	Date:	March, 1990
 *
 *	Definitions for the VM debugging interface.
 */

#ifndef	_MACH_DEBUG_VM_INFO_H_
#define _MACH_DEBUG_VM_INFO_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_inherit.h>
#include <mach/vm_prot.h>
#include <mach/memory_object.h>

/*
 *	Remember to update the mig type definitions
 *	in mach_debug_types.defs when adding/removing fields.
 */

typedef struct vm_region_info {
	vm_offset_t vri_start;		/* start of region */
	vm_offset_t vri_end;		/* end of region */

/*vm_prot_t*/natural_t vri_protection;	/* protection code */
/*vm_prot_t*/natural_t vri_max_protection;	/* maximum protection */
/*vm_inherit_t*/natural_t vri_inheritance;	/* inheritance */
	natural_t vri_wired_count;	/* number of times wired */
	natural_t vri_user_wired_count; /* number of times user has wired */

	vm_offset_t vri_object;		/* the mapped object */
	vm_offset_t vri_offset;		/* offset into object */
/*boolean_t*/integer_t vri_needs_copy;	/* does object need to be copied? */
	natural_t vri_sharing;	/* share map references */
} vm_region_info_t;

typedef vm_region_info_t *vm_region_info_array_t;


typedef natural_t vm_object_info_state_t;

#define VOI_STATE_PAGER_CREATED		0x00000001
#define VOI_STATE_PAGER_INITIALIZED	0x00000002
#define VOI_STATE_PAGER_READY		0x00000004
#define VOI_STATE_CAN_PERSIST		0x00000008
#define VOI_STATE_INTERNAL		0x00000010
#define VOI_STATE_TEMPORARY		0x00000020
#define VOI_STATE_ALIVE			0x00000040
#define VOI_STATE_LOCK_IN_PROGRESS	0x00000080
#define VOI_STATE_LOCK_RESTART		0x00000100
#define VOI_STATE_USE_OLD_PAGEOUT	0x00000200

typedef struct vm_object_info {
	vm_offset_t voi_object;		/* this object */
	vm_size_t voi_pagesize;		/* object's page size */
	vm_size_t voi_size;		/* object size (valid if internal) */
	natural_t voi_ref_count;	/* number of references */
	natural_t voi_resident_page_count; /* number of resident pages */
	natural_t voi_absent_count;	/* number requested but not filled */
	vm_offset_t voi_copy;		/* copy object */
	vm_offset_t voi_shadow;		/* shadow object */
	vm_offset_t voi_shadow_offset;	/* offset into shadow object */
	vm_offset_t voi_paging_offset;	/* offset into memory object */
/*memory_object_copy_strategy_t*/integer_t voi_copy_strategy;
					/* how to handle data copy */
	vm_offset_t voi_last_alloc;	/* offset of last allocation */
	natural_t voi_paging_in_progress; /* paging references */
	vm_object_info_state_t voi_state; /* random state bits */
} vm_object_info_t;

typedef vm_object_info_t *vm_object_info_array_t;


typedef natural_t vm_page_info_state_t;

#define VPI_STATE_BUSY		0x00000001
#define VPI_STATE_WANTED	0x00000002
#define VPI_STATE_TABLED	0x00000004
#define VPI_STATE_FICTITIOUS	0x00000008
#define VPI_STATE_PRIVATE	0x00000010
#define VPI_STATE_ABSENT	0x00000020
#define VPI_STATE_ERROR		0x00000040
#define VPI_STATE_DIRTY		0x00000080
#define VPI_STATE_PRECIOUS	0x00000100
#define VPI_STATE_OVERWRITING	0x00000200
#define VPI_STATE_INACTIVE	0x00000400
#define VPI_STATE_ACTIVE	0x00000800
#define VPI_STATE_LAUNDRY	0x00001000
#define VPI_STATE_FREE		0x00002000
#define VPI_STATE_REFERENCE	0x00004000

#define VPI_STATE_PAGER		0x80000000	/* pager has the page */

typedef struct vm_page_info {
	vm_offset_t vpi_offset;		/* offset in object */
	vm_offset_t vpi_phys_addr;	/* physical address */
	natural_t vpi_wire_count;	/* number of times wired */
/*vm_prot_t*/natural_t vpi_page_lock;	/* XP access restrictions */
/*vm_prot_t*/natural_t vpi_unlock_request;	/* outstanding unlock requests */
	vm_page_info_state_t vpi_state;	/* random state bits */
} vm_page_info_t;

typedef vm_page_info_t *vm_page_info_array_t;

#endif	_MACH_DEBUG_VM_INFO_H_
