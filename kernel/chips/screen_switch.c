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
 *	File: screen_switch.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Autoconfiguration code for the Generic Screen Driver.
 */

#include <platforms.h>

#if	defined(DECSTATION) || defined(FLAMINGO)
#include <fb.h>
#include <gx.h>
#include <cfb.h>
#include <mfb.h>
#include <xcfb.h>
#include <sfb.h>
#endif

#ifdef	VAXSTATION
#define NGX 0
#define NCFB 0
#define NXCFB 0
#endif

#include <chips/screen_switch.h>

/* When nothing needed */
int screen_noop()
{}

/*
 * Vector of graphic interface drivers to probe.
 * Zero terminate this list.
 */


#if	NGX > 0
extern int gq_probe(), gq_cold_init();
extern unsigned int gq_mem_need();

extern int ga_probe(), ga_cold_init();
extern unsigned int ga_mem_need();
#endif	/* NGX > 0 */

#if	NCFB > 0
extern int cfb_probe(), cfb_cold_init();
extern unsigned int pm_mem_need();
#endif	/* NCFB > 0 */

#if	NMFB > 0
extern int fb_probe(), fb_cold_init();
extern unsigned int pm_mem_need();
#endif	/* NMFB > 0 */

#if	NXCFB > 0
extern int xcfb_probe(), xcfb_cold_init();
extern unsigned int pm_mem_need();
#endif	/* NXCFB > 0 */

#if	NSFB > 0
extern int sfb_probe(), sfb_cold_init();
extern unsigned int pm_mem_need();
#endif	/* NSFB > 0 */

#if	NFB > 0
extern int pm_probe(), pm_cold_init();
extern unsigned int pm_mem_need();
#endif	/* NFB > 0 */

struct screen_probe_vector screen_probe_vector[] = {

#if	NGX > 0
	gq_probe, gq_mem_need, gq_cold_init, /* 3max 3D color option */
	ga_probe, ga_mem_need, ga_cold_init, /* 3max 2D color option */
#endif	/* NGX > 0 */

#if	NSFB > 0
	sfb_probe, pm_mem_need, sfb_cold_init, /* Smart frame buffer */
#endif	/* NSFB > 0 */

#if	NMFB > 0
	fb_probe, pm_mem_need, fb_cold_init, /* 3max/3min 1D(?) mono option */
#endif	/* NMFB > 0 */

#if	NCFB > 0
	cfb_probe, pm_mem_need, cfb_cold_init, /* 3max 1D(?) color option */
#endif	/* NCFB > 0 */

#if	NXCFB > 0
	xcfb_probe, pm_mem_need, xcfb_cold_init,/* MAXine frame buffer */
#endif	/* NXCFB > 0 */

#if	NFB > 0
	pm_probe, pm_mem_need, pm_cold_init, /* "pm" mono/color (pmax) */
#endif
	0,
};

char	*screen_data;	/* opaque */

int screen_find()
{
	struct screen_probe_vector *p = screen_probe_vector;
	for (;p->probe; p++)
		if ((*p->probe)()) {
			(*p->setup)(0/*XXX*/, screen_data);
			return 1;
		}
	return 0;
}

unsigned int
screen_memory_alloc(avail)
	char *avail;
{
	struct screen_probe_vector *p = screen_probe_vector;
	int             size;
	for (; p->probe; p++)
		if ((*p->probe) ()) {
			screen_data = avail;
			size = (*p->alloc) ();
			bzero(screen_data, size);
			return size;
		}
	return 0;

}

