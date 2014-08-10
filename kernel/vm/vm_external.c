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
/*
 *	This module maintains information about the presence of
 *	pages not in memory.  Since an external memory object
 *	must maintain a complete knowledge of its contents, this
 *	information takes the form of hints.
 */

#include <mach/boolean.h>
#include <kern/zalloc.h>
#include <vm/vm_external.h>
#include <mach/vm_param.h>
#include <kern/assert.h>



boolean_t	vm_external_unsafe = FALSE;

zone_t		vm_external_zone = ZONE_NULL;

/*
 *	The implementation uses bit arrays to record whether
 *	a page has been written to external storage.  For
 *	convenience, these bit arrays come in two sizes
 *	(measured in bytes).
 */

#define		SMALL_SIZE	(VM_EXTERNAL_SMALL_SIZE/8)
#define		LARGE_SIZE	(VM_EXTERNAL_LARGE_SIZE/8)

zone_t		vm_object_small_existence_map_zone;
zone_t		vm_object_large_existence_map_zone;


vm_external_t	vm_external_create(size)
	vm_offset_t	size;
{
	vm_external_t	result;
	vm_size_t	bytes;
	
	if (vm_external_zone == ZONE_NULL)
		return(VM_EXTERNAL_NULL);

	result = (vm_external_t) zalloc(vm_external_zone);
	result->existence_map = (char *) 0;

	bytes = (atop(size) + 07) >> 3;
	if (bytes <= SMALL_SIZE) {
		result->existence_map =
		 (char *) zalloc(vm_object_small_existence_map_zone);
		result->existence_size = SMALL_SIZE;
	} else if (bytes <= LARGE_SIZE) {
		result->existence_map =
		 (char *) zalloc(vm_object_large_existence_map_zone);
		result->existence_size = LARGE_SIZE;
	}
	return(result);
}

void		vm_external_destroy(e)
	vm_external_t	e;
{
	if (e == VM_EXTERNAL_NULL)
		return;

	if (e->existence_map != (char *) 0) {
		if (e->existence_size <= SMALL_SIZE) {
			zfree(vm_object_small_existence_map_zone,
				(vm_offset_t) e->existence_map);
		} else {
			zfree(vm_object_large_existence_map_zone,
				(vm_offset_t) e->existence_map);
		}
	}
	zfree(vm_external_zone, (vm_offset_t) e);
}

vm_external_state_t _vm_external_state_get(e, offset)
	vm_external_t	e;
	vm_offset_t	offset;
{
	unsigned
	int		bit, byte;

	if (vm_external_unsafe ||
	    (e == VM_EXTERNAL_NULL) ||
	    (e->existence_map == (char *) 0))
		return(VM_EXTERNAL_STATE_UNKNOWN);

	bit = atop(offset);
	byte = bit >> 3;
	if (byte >= e->existence_size) return (VM_EXTERNAL_STATE_UNKNOWN);
	return( (e->existence_map[byte] & (1 << (bit & 07))) ?
		VM_EXTERNAL_STATE_EXISTS : VM_EXTERNAL_STATE_ABSENT );
}

void		vm_external_state_set(e, offset, state)
	vm_external_t	e;
	vm_offset_t	offset;
	vm_external_state_t state;
{
	unsigned
	int		bit, byte;

	if ((e == VM_EXTERNAL_NULL) || (e->existence_map == (char *) 0))
		return;

	if (state != VM_EXTERNAL_STATE_EXISTS)
		return;

	bit = atop(offset);
	byte = bit >> 3;
	if (byte >= e->existence_size) return;
	e->existence_map[byte] |= (1 << (bit & 07));
}

void		vm_external_module_initialize()
{
	vm_size_t	size = (vm_size_t) sizeof(struct vm_external);

	vm_external_zone = zinit(size, 16*1024*size, size,
				 0, "external page bitmaps");

	vm_object_small_existence_map_zone = zinit(SMALL_SIZE,
					round_page(LARGE_SIZE * SMALL_SIZE),
					round_page(SMALL_SIZE),
					ZONE_EXHAUSTIBLE,
					"object small existence maps");

	vm_object_large_existence_map_zone = zinit(LARGE_SIZE,
					round_page(8 * LARGE_SIZE),
					round_page(LARGE_SIZE),
					ZONE_EXHAUSTIBLE,
					"object large existence maps");
}
