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
/*
 * HISTORY
 * machid_vmstuff.c,v
 * Revision 1.1.2.1  1994/10/15  23:30:11  baford
 * added from Johannes Helander's modified CMU USER collection
 *
 * Revision 2.2  92/01/22  23:11:14  rpd
 * 	Created.
 * 	[92/01/22            rpd]
 * 
 */

#if 0
#include <stdio.h>
#include <strings.h>
#endif
#include <mach.h>
#include <mach_debug/mach_debug.h>
#include <mach_error.h>
#include <servers/machid.h>
#include <servers/machid_debug.h>
#include <servers/machid_dpager.h>
#include <servers/machid_types.h>
#include <servers/machid_lib.h>

static void fill_object_dpager();
static void fill_object_pages();

#define NUM_BUCKETS	1001

static object_t *buckets[NUM_BUCKETS];

#define VOI_STATE_DEFAULT_PAGER \
	(VOI_STATE_INTERNAL|VOI_STATE_PAGER_CREATED|\
	 VOI_STATE_PAGER_READY|VOI_STATE_PAGER_INITIALIZED)

object_t *
get_object(name, dpager, pages)
    mobject_name_t name;
    boolean_t dpager, pages;
{
    object_t **b;	/* bucket */
    object_t *o;	/* object */
    kern_return_t kr;

    /* check for null object */

    if (name == 0)
	return 0;

    /* check in hash table */

    for (o = *(b = &buckets[name % NUM_BUCKETS]); o != 0; o = o->o_link)
	if (o->o_info.voi_object == name)
	    return o;

    /* create new object */

    o = (object_t *) malloc(sizeof *o);
    if (o == 0)
	quit(1, "vmstuff: malloc failed\n");
    bzero((char *) o, sizeof *o);

    kr = machid_vm_object_info(machid_server_port, machid_auth_port,
			       name, &o->o_info);
    if (kr != KERN_SUCCESS) {
	free((char *) o);
	return 0;
    }

    /* add to bucket */

    o->o_link = *b;
    *b = o;

    /* fill-in additional requested information */

    if (dpager)
	fill_object_dpager(o);
    if (pages)
	fill_object_pages(o);

    /* create shadow link */

    o->o_shadow = get_object(o->o_info.voi_shadow, dpager, pages);

    return o;
}

static void
fill_object_dpager(o)
    object_t *o;
{
    mobject_name_t object = o->o_info.voi_object;
    mdefault_pager_t dpager;
    default_pager_object_t *dpobjects, *dpobject;
    unsigned int count;

    if ((o->o_info.voi_state & VOI_STATE_DEFAULT_PAGER) !=
					VOI_STATE_DEFAULT_PAGER)
	return;

    if ((dpager = get_object_dpager(object)) == 0)
	return;

    get_dpager_objects(dpager, &dpobjects, &count);

    if ((dpobject = find_dpager_object(object, dpobjects, count)) == 0)
	return;

    o->o_dpager = dpager;
    o->o_dpager_info = *dpobject;
}

static int
pagecmp(one, two)
    char *one, *two;
{
    vm_offset_t oone = ((vm_page_info_t *) one)->vpi_offset;
    vm_offset_t otwo = ((vm_page_info_t *) two)->vpi_offset;

    if (oone == otwo)
	return 0;
    else if (oone < otwo)
	return -1;
    else
	return 1;
}

static int
dppagecmp(one, two)
    char *one, *two;
{
    vm_offset_t oone = ((default_pager_page_t *) one)->dpp_offset;
    vm_offset_t otwo = ((default_pager_page_t *) two)->dpp_offset;

    if (oone == otwo)
	return 0;
    else if (oone < otwo)
	return -1;
    else
	return 1;
}

static void
fill_object_pages(o)
    object_t *o;
{
    mobject_name_t object = o->o_info.voi_object;
    vm_page_info_t pages_buf[1024];
    vm_page_info_t *pages = pages_buf;
    unsigned int pcount = sizeof pages_buf/sizeof pages_buf[0];
    default_pager_page_t dppages_buf[1024];
    default_pager_page_t *dppages = dppages_buf;
    unsigned int dpcount = sizeof dppages_buf/sizeof dppages_buf[0];
    unsigned int i, j;
    kern_return_t kr;

    kr = machid_vm_object_pages(machid_server_port, machid_auth_port,
				object, &pages, &pcount);
    if (kr != KERN_SUCCESS)
	pcount = 0;
    if (pcount != 0) {
	for (i = 0; i < pcount; i++) {
	    /* prevent spurious differences */

	    if (!(pages[i].vpi_state & VPI_STATE_INACTIVE))
		pages[i].vpi_state &= ~VPI_STATE_REFERENCE;

	    pages[i].vpi_offset += o->o_info.voi_paging_offset;
	}

	qsort((char *) pages, (int) pcount,
	      (int) sizeof *pages, pagecmp);
    }

    if (o->o_dpager == 0)
	kr = KERN_FAILURE;
    else
	kr = machid_default_pager_object_pages(machid_server_port,
					       machid_auth_port,
					       o->o_dpager, object,
					       &dppages, &dpcount);
    if (kr != KERN_SUCCESS)
	dpcount = 0;
    if (dpcount != 0) {
	qsort((char *) dppages, (int) dpcount,
	      (int) sizeof *dppages, dppagecmp);
    }

    /* merge the information into one array */

    if ((pcount != 0) || (dpcount != 0)) {
	o->o_pages = (vm_page_info_t *)
	    malloc(sizeof *o->o_pages * (pcount + dpcount)); /* upper-bound */
	if (o->o_pages == 0)
	    quit(1, "vmstuff: malloc failed\n");

	for (i = 0, j = 0; (i < pcount) || (j < dpcount);) {
	    vm_page_info_t *p = &o->o_pages[o->o_num_pages++];

	    if ((i < pcount) && (j < dpcount) &&
		(pages[i].vpi_offset == dppages[j].dpp_offset)) {
		*p = pages[i];
		p->vpi_state |= VPI_STATE_PAGER;
		i++, j++;
	    } else if ((j == dpcount) ||
		       ((i < pcount) &&
			(pages[i].vpi_offset < dppages[j].dpp_offset))) {
		*p = pages[i];
		i++;
	    } else {
		bzero((char *) p, sizeof *p);
		p->vpi_offset = dppages[j].dpp_offset;
		p->vpi_state |= VPI_STATE_PAGER;
		j++;
	    }
	}
    }

    if ((pages != pages_buf) && (pcount != 0)) {
	kr = vm_deallocate(mach_task_self(), (vm_offset_t) pages,
			   (vm_size_t) (pcount * sizeof *pages));
	if (kr != KERN_SUCCESS)
	    quit(1, "vmstuff: vm_deallocate: %s\n",
		 mach_error_string(kr));
    }

    if ((dppages != dppages_buf) && (dpcount != 0)) {
	kr = vm_deallocate(mach_task_self(), (vm_offset_t) dppages,
			   (vm_size_t) (dpcount * sizeof *dppages));
	if (kr != KERN_SUCCESS)
	    quit(1, "vmstuff: vm_deallocate: %s\n",
		 mach_error_string(kr));
    }
}

mhost_priv_t
get_object_host(object)
    mobject_name_t object;
{
    mhost_priv_t host_priv;
    kern_return_t kr;

    kr = machid_mach_lookup(machid_server_port, machid_auth_port,
			    object, MACH_TYPE_HOST_PRIV, &host_priv);
    if (kr != KERN_SUCCESS)
	host_priv = 0;

    return host_priv;
}

mdefault_pager_t
get_host_dpager(host)
    mhost_priv_t host;
{
    static mdefault_pager_t cache_dpager;
    static mhost_priv_t cache_host;

    if (cache_host != host) {
	mdefault_pager_t dpager;
	kern_return_t kr;

	kr = machid_host_default_pager(machid_server_port, machid_auth_port,
				       host, &dpager);
	if (kr != KERN_SUCCESS)
	    return 0;

	cache_host = host;
	cache_dpager = dpager;
    }

    return cache_dpager;
}

mdefault_pager_t
get_object_dpager(object)
    mobject_name_t object;
{
    mhost_priv_t host;

    host = get_object_host(object);
    if (host == 0)
	return 0;

    return get_host_dpager(host);
}

static int
dpobjectcmp(one, two)
    char *one, *two;
{
    mobject_name_t One = ((default_pager_object_t *) one)->dpo_object;
    mobject_name_t Two = ((default_pager_object_t *) two)->dpo_object;

    if (One == Two)
	return 0;
    else if (One < Two)
	return -1;
    else
	return 1;
}

void
get_dpager_objects(default_pager, objectsp, numobjectsp)
    mach_id_t default_pager;
    default_pager_object_t **objectsp;
    unsigned int *numobjectsp;
{
    static mach_id_t cache_default_pager;
    static default_pager_object_t *cache_objects;
    static unsigned int cache_numobjects;

    if (cache_default_pager != default_pager) {
	kern_return_t kr;

	if (cache_numobjects != 0) {
	    kr = vm_deallocate(mach_task_self(), (vm_offset_t) cache_objects,
		    (vm_size_t) (cache_numobjects * sizeof *cache_objects));
	    if (kr != KERN_SUCCESS)
		quit(1, "ms: vm_deallocate: %s\n",
		     mach_error_string(kr));
	}

	cache_default_pager = default_pager;
	cache_numobjects = 0;
	cache_objects = 0;

	kr = machid_default_pager_objects(machid_server_port, machid_auth_port,
					  default_pager,
					  &cache_objects, &cache_numobjects);
	if ((kr == KERN_SUCCESS) &&
	    (cache_numobjects != 0)) {
	    qsort((char *) cache_objects, (int) cache_numobjects,
		  (int) sizeof *cache_objects, (int (*)()) dpobjectcmp);
	}
    }

    *objectsp = cache_objects;
    *numobjectsp = cache_numobjects;
}

default_pager_object_t *
find_dpager_object(object, objects, count)
    mobject_name_t object;
    default_pager_object_t *objects;
    unsigned int count;
{
    if (count == 0) {
	return 0;
    } else if (count == 1) {
	if (objects->dpo_object == object)
	    return objects;
	else
	    return 0;
    } else {
	unsigned int mid = count/2;
	default_pager_object_t *midobjects = objects + mid;

	if (midobjects->dpo_object == object)
	    return midobjects;
	else if (midobjects->dpo_object < object)
	    return find_dpager_object(object, midobjects, count - mid);
	else
	    return find_dpager_object(object, objects, mid);
    }
}

vm_page_info_t *
lookup_page_object_prim(o, offset)
    object_t *o;
    vm_offset_t offset;
{
    vm_page_info_t *p, *lastp;

    if (o->o_num_pages == 0)
	return 0;

    /* generally get sequential access, so use hint */

    if (((p = o->o_hint) == 0) ||
	(offset < p->vpi_offset))
	p = o->o_pages;

    /* scan forward for the page */

    for (lastp = o->o_pages + o->o_num_pages;; p++) {
	if (p == lastp)
	    return 0;

	if (offset == p->vpi_offset)
	    return o->o_hint = p;

	if (offset < p->vpi_offset)
	    return 0;
    }
}

void
lookup_page_object(object, offset, objectp, infop)
    object_t *object;
    vm_offset_t offset;
    object_t **objectp;
    vm_page_info_t **infop;
{
    vm_page_info_t *info;

    info = lookup_page_object_prim(object, offset);
    if (info == 0)
	object = 0;

    *objectp = object;
    *infop = info;
}

void
lookup_page_chain(chain, offset, objectp, infop)
    object_t *chain;
    vm_offset_t offset;
    object_t **objectp;
    vm_page_info_t **infop;
{
    object_t *object;
    vm_page_info_t *info = 0;

    for (object = chain; object != 0; object = object->o_shadow) {
	info = lookup_page_object_prim(object,
			offset + object->o_info.voi_paging_offset);
	if (info != 0)
	    break;

	offset += object->o_info.voi_shadow_offset;
    }

    *objectp = object;
    *infop = info;
}

void
get_object_bounds(object, startp, endp)
    object_t *object;
    vm_offset_t *startp, *endp;
{
    vm_offset_t offset = object->o_info.voi_paging_offset;
    vm_offset_t start = 0 + offset;
    vm_offset_t end = object->o_info.voi_size + offset;

    if (object->o_num_pages != 0) {
	vm_page_info_t *first = &object->o_pages[0];
	vm_page_info_t *last = &object->o_pages[object->o_num_pages - 1];
	vm_size_t pagesize = object->o_info.voi_pagesize;

	/* check for "negative" pages, due to paging offset */

	if (start > first->vpi_offset)
	    start = first->vpi_offset;

	/* check for pages beyond the kernel's size */

	if (end < (last->vpi_offset + pagesize))
	    end = last->vpi_offset + pagesize;
    }

    *startp = start;
    *endp = end;
}
