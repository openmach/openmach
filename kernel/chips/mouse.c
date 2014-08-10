/* 
 * Mach Operating System
 * Copyright (c) 1992-1989 Carnegie Mellon University
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
 *	File: mouse.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Driver code for Digital's mouse AND tablet
 */

/*
 * XXX This should be rewritten to support other
 * XXX sorts of mices and tablets.  But for now
 * XXX I have none to play with. Sorry about that.
 */

#include <lk.h>			/* one mouse per lk201 */
#if	NLK > 0

#include <mach/std_types.h>
#include <machine/machspl.h>		/* spl definitions */
#include <sys/time.h>
#include <kern/time_out.h>

#include <chips/serial_defs.h>
#include <chips/screen_defs.h>

#define	MOUSE_INCREMENTAL	0x52	/* R */
#define	MOUSE_PROMPTED		0x44	/* D */
#define	MOUSE_REQ_POSITION	0x50	/* P */
#define	MOUSE_SELFTEST		0x54	/* T */
#define MOUSE_RESERVED_FUNC	0x5a	/* Z, param byte follows */

#define TABLET_SAMPLE_55	0x4b	/* K */	/* in reps/sec */
#define TABLET_SAMPLE_72	0x4c	/* L */
#define TABLET_SAMPLE_120	0x4d	/* M */
#define TABLET_9600		0x42	/* B */

#define	TYPE_MOUSE		0x2
#define	TYPE_TABLET		0x4

#define	START_REPORT		0x80

typedef union {
	struct {
		unsigned char	r : 1, m : 1, l : 1, sy : 1, sx : 1;
		unsigned char	xpos;
		unsigned char	ypos;
	} ms;
	struct {
		unsigned char	pr : 1, buttons : 4;
		unsigned char	xlo, xhi;
		unsigned char	ylo, yhi;
	} tb;
	unsigned char raw[1];
} mouse_report_t;


/*
 *	Mouse state
 */
struct mouse_softc {
	user_info_t	*up;
	mouse_report_t	report;
	unsigned char	rep_bytes;
	unsigned char	rep_ptr;
	unsigned char	prev_buttons;
	unsigned char	flags;
#define	MS_TABLET	0x1
#define	MS_MOVING	0x2
	char		screen_unit;
	char		sl_unit;
} mouse_softc_data[NLK];

typedef struct mouse_softc	*mouse_softc_t;

mouse_softc_t	mouse_softc[NLK];


mouse_notify_mapped(
	int		unit,
	int		screen_unit,
	user_info_t	*up)
{
	mouse_softc_t	ms = &mouse_softc_data[unit];

	ms->up = up;
	ms->screen_unit = screen_unit;
}

/*
 *	Autoconfiguration
 */
mouse_probe(
	int	unit)
{
	mouse_softc[unit] = &mouse_softc_data[unit];
}

mouse_attach(
	int		unit,
	int		sl_unit)
{
	int		messg[4];
	spl_t		s;
	mouse_softc_t	ms;

	ms = mouse_softc[unit];
	ms->sl_unit = sl_unit;

	s = spltty();
	(*console_putc)(sl_unit, SCREEN_LINE_POINTER, MOUSE_SELFTEST);
	delay(1);
	messg[0] = (*console_getc)(sl_unit, SCREEN_LINE_POINTER, TRUE, TRUE);
	messg[1] = (*console_getc)(sl_unit, SCREEN_LINE_POINTER, TRUE, TRUE);
	messg[2] = (*console_getc)(sl_unit, SCREEN_LINE_POINTER, TRUE, TRUE);
	messg[3] = (*console_getc)(sl_unit, SCREEN_LINE_POINTER, TRUE, TRUE);

	delay(100000);	/* spec says less than 500 msecs */
	(*console_putc)(sl_unit, SCREEN_LINE_POINTER, MOUSE_INCREMENTAL);
	splx(s);

	ms->rep_bytes = 3;/* mouse */
	if (messg[2] | messg[3]) {
		printf(" bad pointer [%x %x %x %x] ",
			messg[0], messg[1], messg[2], messg[3]);
		if (messg[2] >= 0x20) printf("fatal ");
		if (messg[2] == 0x3e) printf("RAM/ROM");
		if (messg[2] == 0x3d) printf("button(s) %x", messg[3] & 0x1f);
	} else {
		int rev = messg[0] & 0xf;
		int loc = (messg[1] & 0xf0) >> 4;
		int tag = (messg[1] & 0xf);
		printf("( %s rev. %x.%x )",
			(tag == TYPE_MOUSE) ? "mouse" : "tablet",
			rev, loc);
		if (tag == TYPE_TABLET) {
			ms->flags = MS_TABLET;
			ms->rep_bytes = 5;
		}
	}
}

/*
 * 	Process a character from the mouse
 */
mouse_input(
	int	unit,
	register unsigned short data)
{
	mouse_softc_t	ms = mouse_softc[unit];
	register char	flg, but;

	data &= 0xff;

	/* sanity: might miss a byte sometimes */
	if (data & START_REPORT)
		ms->rep_ptr = 0;

	/* add byte to report */
	ms->report.raw[ms->rep_ptr++] = data;

	/* does this mean the mouse is moving */
	if (data && ((data & START_REPORT) == 0))
		ms->flags |= MS_MOVING;

	/* Report complete ? */
	if (ms->rep_ptr != ms->rep_bytes)
		return;
	ms->rep_ptr = 0;

	ssaver_bump(ms->screen_unit);

	/* check for mouse moved */
	flg = ms->flags;
	if (flg & MS_MOVING) {
		ms->flags = flg & ~MS_MOVING;
		mouse_motion_event(ms, flg);
	}

	/* check for button pressed */
	if (but = ms->prev_buttons ^ ms->report.raw[0]) {
		mouse_button_event(ms, flg, but);
		ms->prev_buttons = ms->report.raw[0];
	}
}

/*
 * The mouse/puck moved.
 * Find how much and post an event
 */
mouse_motion_event(
	mouse_softc_t	ms,
	int		flg)
{
	register int		x, y;

	if (flg & MS_TABLET) {

		flg = DEV_TABLET;

		x = (ms->report.tb.xhi << 8) | ms->report.tb.xlo;
		y = (ms->report.tb.yhi << 8) | ms->report.tb.ylo;

	} else {

		flg = DEV_MOUSE;

		x = ms->report.ms.xpos;
		if (!ms->report.ms.sx)	/* ??? */
			x = -x;

		y = ms->report.ms.ypos;
		if (ms->report.ms.sy)
			y = -y;

	}

	screen_motion_event(ms->screen_unit, flg, x, y);
}

/*
 * A mouse/puck button was pressed/released.
 * Find which one and post an event
 */
mouse_button_event(
	mouse_softc_t	ms,
	int		flg,
	int		bmask)
{
	register unsigned int	buttons, i;
	int			key, type;

	buttons = ms->report.raw[0];
	if (flg & MS_TABLET) {
		/* check each one of the four buttons */
		for (i = 0; i < 4; i += 1) {
			if ((bmask & (2<<i)) == 0)
				continue;/* did not change */
			type = (buttons & (2<<i)) ? EVT_BUTTON_DOWN : EVT_BUTTON_UP;
			key = i;

			screen_keypress_event(	ms->screen_unit,
						DEV_TABLET, key, type);
		}
	} else {
		ms->up->mouse_buttons = buttons & 0x7;
		/* check each one of the three buttons */
		for (i = 0; i < 3; i += 1) {
			if ((bmask & (1<<i)) == 0)
				continue;/* did not change */
			type = (buttons & (1<<i)) ? EVT_BUTTON_DOWN : EVT_BUTTON_UP;

			if (i & 1)
				key = KEY_MIDDLE_BUTTON;
			else if ((i & 2) == 0)
				key = KEY_RIGHT_BUTTON;
			else
				key = KEY_LEFT_BUTTON;

			screen_keypress_event(	ms->screen_unit,
						DEV_MOUSE, key, type);
		}
	}
}

/*
 * Generate escape sequences for position reporting
 * These are the same as xterm's.
 * Prefix:
 *	ESC [ M		button down
 *	ESC [ N		button up
 * Body:
 *	BUTTON COL ROW
 * Button:
 *	0 <-> left, 1 <-> middle, 2 <-> right
 * All body values are offset by the ascii SPACE character
 */
#define ESC	'\033'
#define	SPACE	' '

mouse_report_position(
	int	unit,
	int	col,
	int	row,
	int	key,
	int	type)
{
	cons_input(SCREEN_LINE_KEYBOARD, ESC, 0);
	cons_input(SCREEN_LINE_KEYBOARD, '[', 0);
	cons_input(SCREEN_LINE_KEYBOARD, (type==EVT_BUTTON_DOWN) ? 'M':'N', 0);
	
	cons_input(SCREEN_LINE_KEYBOARD, (key - 1) + SPACE, 0);/* quick remapping */
	cons_input(SCREEN_LINE_KEYBOARD, SPACE + col + 2, 0);
	cons_input(SCREEN_LINE_KEYBOARD, SPACE + row + 1, 0);
}

#endif	NLK > 0
