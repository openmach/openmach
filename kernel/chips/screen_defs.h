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
 *	File: screen_defs.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	11/90
 *
 *	Definitions for the Generic Screen Driver.
 */

#include <chips/screen.h>
#include <chips/screen_switch.h>
#include <device/device_types.h>

/*
 * Driver state
 */
typedef struct screen_softc {
	user_info_t		*up;
	char			**hw_state;	/* semi-opaque */

	struct screen_switch	sw;

	/* should also be a switch */
	io_return_t		(*kbd_set_status)();
	int			(*kbd_reset)();
	int			(*kbd_beep)();

	char			flags;
	char			mapped;
	char			blitc_state;
	char			standout;
	short			save_row;
	short			save_col;
	/*
	 * Eventually move here all that is Kdep in the user structure,
	 * to avoid crashing because of a bogus graphic server
	 */
	short		frame_scanline_width;	/* in pixels */
	short		frame_height;		/* in scanlines */
	short		frame_visible_width;	/* in pixels */
	short		frame_visible_height;	/* in pixels */

/* This is used by all screens, therefore it is sized maximally */
#	define MaxCharRows	68	/* 2DA screen & PMAG-AA */
#	define MaxCharCols	160	/* PMAG-AA */
#	define MinCharRows	57	/* pmax */
	unsigned char		ascii_screen[MaxCharRows*MaxCharCols];

} *screen_softc_t;

extern screen_softc_t	screen(/* int unit */);

/*
 * This global says if we have a graphic console
 * and where it is and if it is enabled
 */
extern short	screen_console;
#define SCREEN_CONS_ENBL	(0x0100)
#define	SCREEN_ISA_CONSOLE()	(screen_console & SCREEN_CONS_ENBL)
#define SCREEN_CONS_UNIT()	(screen_console & 0x00ff)

/*
 * A graphic screen needs a keyboard and a mouse/tablet
 */
#define	SCREEN_LINE_KEYBOARD	0
#define	SCREEN_LINE_POINTER	1
#define	SCREEN_LINE_OTHER	(-1)

/* kernel font */
#define KfontWidth	8
#define KfontHeight	15
extern unsigned char kfont_7x14[];

