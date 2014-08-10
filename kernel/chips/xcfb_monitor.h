/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY ALLOW FREE USE
 * OF THIS SOFTWARE IN ITS "AS IS" CONDITION.  CARNEGIE MELLON AND
 * HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 * 	File: xcfb_monitor.h
 *	Author: Jukka Partanen, Helsinki University of Technology 1992.
 *
 *	A type describing the physical properties of a monitor
 */

#ifndef _XCFB_MONITOR_H_
#define _XCFB_MONITOR_H_

typedef struct xcfb_monitor_type {
	char *name;
	short frame_visible_width; /* pixels */
	short frame_visible_height;
	short frame_scanline_width;
	short frame_height;
	short half_sync;	/* screen units (= 4 pixels) */
	short back_porch;
	short v_sync;		/* lines */
	short v_pre_equalize;
	short v_post_equalize;
	short v_blank;
	short line_time;	/* screen units */
	short line_start;
	short mem_init;		/* some units */
	short xfer_delay;
} *xcfb_monitor_type_t;

#endif /* _XCFB_MONITOR_H_ */
