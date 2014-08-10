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

#ifndef	_MACHID_LIB_H_
#define	_MACHID_LIB_H_

#include <mach/machine/vm_types.h>
#include <mach/default_pager_types.h>
#include <mach_debug/vm_info.h>
#include <servers/machid_types.h>

/* values for mach_type_t */

#define MACH_TYPE_NONE			0
#define	MACH_TYPE_TASK			1
#define MACH_TYPE_THREAD		2
#define MACH_TYPE_PROCESSOR_SET		3
#define MACH_TYPE_PROCESSOR_SET_NAME	4
#define MACH_TYPE_PROCESSOR		5
#define MACH_TYPE_HOST			6
#define MACH_TYPE_HOST_PRIV		7
#define MACH_TYPE_OBJECT		8
#define MACH_TYPE_OBJECT_CONTROL	9
#define MACH_TYPE_OBJECT_NAME		10
#define MACH_TYPE_MASTER_DEVICE		11
#define MACH_TYPE_DEFAULT_PAGER		12

/* convert a mach_type_t to a string */

extern char *mach_type_string(/* mach_type_t */);

/* at the moment, mach/kern_return.h doesn't define these,
   but maybe it will define some of them someday */

#ifndef	KERN_INVALID_THREAD
#define	KERN_INVALID_THREAD		KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_THREAD */

#ifndef	KERN_INVALID_PROCESSOR_SET
#define	KERN_INVALID_PROCESSOR_SET	KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_PROCESSOR_SET */

#ifndef	KERN_INVALID_PROCESSOR_SET_NAME
#define	KERN_INVALID_PROCESSOR_SET_NAME	KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_PROCESSOR_SET_NAME */

#ifndef	KERN_INVALID_HOST_PRIV
#define KERN_INVALID_HOST_PRIV		KERN_INVALID_HOST
#endif	/* KERN_INVALID_HOST_PRIV */

#ifndef	KERN_INVALID_PROCESSOR
#define KERN_INVALID_PROCESSOR		KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_PROCESSOR */

#ifndef	KERN_INVALID_DEFAULT_PAGER
#define KERN_INVALID_DEFAULT_PAGER	KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_DEFAULT_PAGER */

#ifndef	KERN_INVALID_MEMORY_OBJECT
#define KERN_INVALID_MEMORY_OBJECT	KERN_INVALID_ARGUMENT
#endif	/* KERN_INVALID_MEMORY_OBJECT */

/*
 *	Some machid library functions access the machid server
 *	using these two ports.
 */

extern mach_port_t machid_server_port;	/* machid server */
extern mach_port_t machid_auth_port;	/* machid authentication port */

/*
 *	The kernel and default pager provide several functions
 *	for accessing the internal VM data structures.
 *	The machid server provides access to these functions.
 *	However, they are inconvenient to use directly.
 *	These library functions present this capability
 *	in an easier-to-use form.
 */

typedef struct object {
    struct object *o_link;		/* hash table link */

    vm_object_info_t o_info;		/* object name and attributes */
    /* vpi_offset fields are biased by voi_paging_offset */
    vm_page_info_t *o_pages;		/* object pages */
    unsigned int o_num_pages;		/* number of pages */
    vm_page_info_t *o_hint;		/* hint pointer into o_pages */
    mdefault_pager_t o_dpager;		/* default pager for the object */
    default_pager_object_t o_dpager_info;	/* default pager info */
    struct object *o_shadow;		/* pointer to shadow object */

    unsigned int o_flag;
} object_t;

/* get object chain, optionally getting default-pager and resident-page info */

extern object_t *get_object(/* mobject_name_t object,
			       boolean_t dpager, pages */);

/* convert object to privileged host */

extern mhost_priv_t get_object_host(/* mobject_name_t object */);

/* convert privileged host to default pager */

extern mdefault_pager_t get_host_dpager(/* mhost_priv_t host */);

/* convert object to default pager */

extern mdefault_pager_t get_object_dpager(/* mobject_name_t object */);

/* get object/size info from the default pager */

extern void get_dpager_objects(/* mdefault_pager_t dpager,
				  default_pager_object_t **objectsp,
				  unsigned int *numobjectsp */);

/* find a particular object in array from get_dpager_objects */

extern default_pager_object_t *
find_dpager_object(/* mobject_name_t object,
		      default_pager_object_t *objects,
		      unsigned int count */);

/* the object offset is already biased by voi_paging_offset */

extern vm_page_info_t *
lookup_page_object_prim(/* object_t *object, vm_offset_t offset */);

/* the object offset is already biased by voi_paging_offset */

extern void
lookup_page_object(/* object_t *chain, vm_offset_t offset,
		     object_t **objectp, vm_page_info_t **infop */);

/* the object offset is not biased; follows shadow pointers */

extern void
lookup_page_chain(/* object_t *chain, vm_offset_t offset,
		     object_t **objectp, vm_page_info_t **infop */);

/* returns range (inclusive/exclusive) for pages in the object,
   biased by voi_paging_offset */

extern void
get_object_bounds(/* object_t *object,
		     vm_offset_t *startp, vm_offset_t *endp */);

#endif	/* _MACHID_LIB_H_ */
