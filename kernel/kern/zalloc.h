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
 *	File:	zalloc.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	 1985
 *
 */

#ifndef	_KERN_ZALLOC_H_
#define _KERN_ZALLOC_H_

#include <mach/machine/vm_types.h>
#include <kern/macro_help.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <machine/zalloc.h>

/*
 *	A zone is a collection of fixed size blocks for which there
 *	is fast allocation/deallocation access.  Kernel routines can
 *	use zones to manage data structures dynamically, creating a zone
 *	for each type of data structure to be managed.
 *
 */

struct zone {
	decl_simple_lock_data(,lock)	/* generic lock */
#ifdef ZONE_COUNT
	int		count;		/* Number of elements used now */
#endif
	vm_offset_t	free_elements;
	vm_size_t	cur_size;	/* current memory utilization */
	vm_size_t	max_size;	/* how large can this zone grow */
	vm_size_t	elem_size;	/* size of an element */
	vm_size_t	alloc_size;	/* size used for more memory */
	boolean_t	doing_alloc;	/* is zone expanding now? */
	char		*zone_name;	/* a name for the zone */
	unsigned int	type;		/* type of memory */
	lock_data_t	complex_lock;	/* Lock for pageable zones */
	struct zone	*next_zone;	/* Link for all-zones list */
};
typedef struct zone *zone_t;

#define		ZONE_NULL	((zone_t) 0)

/* Exported to everyone */
zone_t		zinit(vm_size_t size, vm_size_t max, vm_size_t alloc,
		      unsigned int memtype, char *name);
vm_offset_t	zalloc(zone_t zone);
vm_offset_t	zget(zone_t zone);
void		zfree(zone_t zone, vm_offset_t elem);
void		zcram(zone_t zone, vm_offset_t newmem, vm_size_t size);

/* Exported only to vm/vm_init.c */
void		zone_bootstrap();
void		zone_init();

/* Exported only to vm/vm_pageout.c */
void		consider_zone_gc();


/* Memory type bits for zones */
#define ZONE_PAGEABLE		0x00000001
#define ZONE_COLLECTABLE	0x00000002	/* Garbage-collect this zone when memory runs low */
#define ZONE_EXHAUSTIBLE	0x00000004	/* zalloc() on this zone is allowed to fail */
#define ZONE_FIXED		0x00000008	/* Panic if zone is exhausted (XXX) */

/* Machine-dependent code can provide additional memory types.  */
#define ZONE_MACHINE_TYPES	0xffff0000


#ifdef ZONE_COUNT
#define zone_count_up(zone) ((zone)->count++)
#define zone_count_down(zone) ((zone)->count--)
#else
#define zone_count_up(zone)
#define zone_count_down(zone)
#endif



/* These quick inline versions only work for small, nonpageable zones (currently).  */

static __inline vm_offset_t ZALLOC(zone_t zone)
{
	simple_lock(&zone->lock);
	if (zone->free_elements == 0) {
		simple_unlock(&zone->lock);
		return zalloc(zone);
	} else {
		vm_offset_t element = zone->free_elements;
		zone->free_elements = *((vm_offset_t *)(element));
		zone_count_up(zone);
		simple_unlock(&zone->lock);
		return element;
	}
}

static __inline void ZFREE(zone_t zone, vm_offset_t element)
{
	*((vm_offset_t *)(element)) = zone->free_elements;
	zone->free_elements = (vm_offset_t) (element);
	zone_count_down(zone);
}



#endif	_KERN_ZALLOC_H_
