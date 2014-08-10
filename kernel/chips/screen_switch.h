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
 *	File: screen_switch.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Definitions of things that must be tailored to
 *	specific hardware boards for the Generic Screen Driver.
 */

#ifndef	SCREEN_SWITCH_H
#define	SCREEN_SWITCH_H	1

/*
 *	List of probe routines, scanned at cold-boot time
 *	to see which, if any, graphic display is available.
 *	This is done before autoconf, so that printing on
 *	the console works early on.  The alloc routine is
 *	called only on the first device that answers.
 *	Ditto for the setup routine, called later on.
 */
struct screen_probe_vector {
	int		(*probe)();
	unsigned int	(*alloc)();
	int		(*setup)();
};

/*
 *	Low-level operations on the graphic device, used
 *	by the otherwise device-independent interface code
 */
struct screen_switch {
	int	(*graphic_open)();	/* when X11 opens */
	int	(*graphic_close)();	/* .. or closes */
	int	(*set_status)();	/* dev-specific ops */
	int	(*get_status)();	/* dev-specific ops */
	int	(*char_paint)();	/* blitc */
	int	(*pos_cursor)();	/* cursor positioning */
	int	(*insert_line)();	/* ..and scroll down */
	int	(*remove_line)();	/* ..and scroll up */
	int	(*clear_bitmap)();	/* blank screen */
	int	(*video_on)();		/* screen saver */
	int	(*video_off)();
	int	(*intr_enable)();
	int	(*map_page)();		/* user-space mapping */
};

/*
 *	Each graphic device needs page-aligned memory
 *	to be mapped in user space later (for events
 *	and such).  Size and content of this memory
 *	is unfortunately device-dependent, even if
 *	it did not need to (puns).
 */
extern char  *screen_data;

extern struct screen_probe_vector screen_probe_vector[];

extern int screen_noop(), screen_find();

#endif	SCREEN_SWITCH_H
