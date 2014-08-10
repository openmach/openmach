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
 *	File: dtop_handlers.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Handler functions for devices attached to the DESKTOP bus.
 */

#include <dtop.h>
#if	NDTOP > 0

#include <mach_kdb.h>

#include <machine/machspl.h>		/* spl definitions */
#include <mach/std_types.h>
#include <device/io_req.h>
#include <device/tty.h>

#include <chips/busses.h>
#include <chips/serial_defs.h>
#include <chips/screen_defs.h>
#include <mips/PMAX/tc.h>

#include <chips/dtop.h>
#include <chips/lk201.h>

/*
 * Default handler function
 */
int
dtop_null_device_handler(
	 dtop_device_t	dev,
	 dtop_message_t	msg,
	 int		event,
	 unsigned char	outc)
{
	/* See if the message was to the default address (powerup) */

	/* Uhmm, donno how to handle this. Drop it */
	if (event == DTOP_EVENT_RECEIVE_PACKET)
		dev->unknown_report = *msg;
	return 0;
}

/*
 * Handler for locator devices (mice)
 */
int
dtop_locator_handler(
	 dtop_device_t	dev,
	 dtop_message_t	msg,
	 int		event,
	 unsigned char	outc)
{
	register unsigned short	buttons;
	register short		coord;
#if	BYTE_MSF
#	define	get_short(b0,b1)	(((b1)<<8)|(b0))
#else
#	define	get_short(b0,b1)	(((b0)<<8)|(b1))
#endif

	/*
	 * Do the position first
	 */
	{
		register int		i;
		register boolean_t	moved;
		int			delta_coords[L_COORD_MAX];

		/*
		 * Get all coords, see if it moved at all (buttons!)
		 */
		moved = FALSE;
		for (i = 0; i < dev->locator.n_coords; i++) {

			coord = get_short(msg->body[2+(i<<1)],
					  msg->body[3+(i<<1)]);

			if (dev->locator.relative) {
				/*
				 * Flame on
				 * I am getting tired of this, why do they have to
				 * keep this bug around ?  Religion ?  Damn, they
				 * design a keyboard for X11 use and forget the mouse ?
				 * Flame off
				 */
#define	BOGUS_DEC_X_AXIS
#ifdef	BOGUS_DEC_X_AXIS
				if (i == 1) coord = - coord;
#endif	/* BOGUS_DEC_X_AXIS */
				/* dev->locator.coordinate[i] += coord; */
			} else {
				register unsigned int	old_coord;

				old_coord = dev->locator.coordinate[i];
				dev->locator.coordinate[i] = coord;
				coord = old_coord - coord;
			}
			delta_coords[i] = coord;
			if (coord != 0)
				moved = TRUE;
		}
		if (moved) {
			/* scale and threshold done higher up */
			screen_motion_event( 0,
				dev->locator.type,
				delta_coords[0],
				delta_coords[1]);
		}
	}

	/*
	 * Time for the buttons now
	 */
#define	new_buttons	coord
	new_buttons = get_short(msg->body[0],msg->body[1]);
	buttons = new_buttons ^ dev->locator.prev_buttons;
	if (buttons) {
		register int	i, type;

		dev->locator.prev_buttons = new_buttons;
		for (i = 0; buttons; i++, buttons >>= 1) {

			if ((buttons & 1) == 0) continue;

			type = (new_buttons & (1<<i)) ? 
				EVT_BUTTON_DOWN : EVT_BUTTON_UP;
			screen_keypress_event(	0,
						dev->locator.type,
						dev->locator.button_code[i],
						type);
		}
	}
#undef	new_buttons
}

/*
 * Handler for keyboard devices
 * Special case: outc set for recv packet means
 * we are inside the kernel debugger
 */
int
dtop_keyboard_handler(
	 dtop_device_t	dev,
	 dtop_message_t	msg,
	 int		event,
	 unsigned char	outc)
{
	char		save[11];
	register int	msg_len, c;

	/*
	 * Well, really this code handles just an lk401 and in
	 * a very primitive way at that.  Should at least emulate
	 * an lk201 decently, and make that a pluggable module.
	 * Sigh.
	 */

	if (event != DTOP_EVENT_RECEIVE_PACKET) {
		switch (event) {
		case DTOP_EVENT_POLL:
		    {
			register unsigned int	t, t0;

			/*
			 * Note we will always have at least the
			 * end-of-list marker present (a zero)
			 * Here stop and trigger of autorepeat.
			 * Do not repeat shift keys, either.
			 */
			{
				register unsigned char	uc, i = 0;

rpt_char:
				uc = dev->keyboard.last_codes[i];

				if (uc == DTOP_KBD_EMPTY) {
					dev->keyboard.k_ar_state = K_AR_OFF;
					return 0;
				}
				if ((uc >= LK_R_SHIFT) && (uc <= LK_R_ALT)) {
					/* sometimes swapped. Grrr. */
					if (++i < dev->keyboard.last_codes_count) 
						goto rpt_char;
					dev->keyboard.k_ar_state = K_AR_OFF;
					return 0;
				}
				c = uc;
			}

			/*
			 * Got a char. See if enough time from stroke,
			 * or from last repeat.
			 */
			t0 = (dev->keyboard.k_ar_state == K_AR_TRIGGER) ? 30 : 500;
			t = approx_time_in_msec();
			if ((t - dev->keyboard.last_msec) < t0)
				return 0;

			dev->keyboard.k_ar_state = K_AR_TRIGGER;

			/*
			 * Simplest thing to do is to mimic lk201
			 */
			outc = lk201_input(0, LK_REPEAT);
			if ( ! screen_keypress_event(	0,
							DEV_KEYBD,
							c,
							EVT_BUTTON_UP)) {
				if (outc > 0) cons_input(0, outc, 0);
			} else
				screen_keypress_event(	0,
							DEV_KEYBD,
							c,
							EVT_BUTTON_DOWN);
			return 0;
		    }
		default:	gimmeabreak();/*fornow*/
		}
		return -1;
	}

	msg_len = msg->code.val.len;

	/* Check for errors */
	c = msg->body[0];
	if ((c < DTOP_KBD_KEY_MIN) && (c != DTOP_KBD_EMPTY)) {
		printf("Keyboard error: %x %x %x..\n", msg_len, c, msg->body[1]);
		if (c != DTOP_KBD_OUT_ERR) return -1;
		/* spec sez if scan list overflow still there is data */
		msg->body[0] = 0;
	}

	dev->keyboard.last_msec = approx_time_in_msec();

	switch (dev->keyboard.k_ar_state) {
	case K_AR_IDLE:
		/* if from debugger, timeouts might not be working yet */
		if (outc == 0xff)
			break;
		dtop_keyboard_autorepeat( dev );
		/* fall through */
	case K_AR_TRIGGER:
		dev->keyboard.k_ar_state = K_AR_ACTIVE;
		break;
	case K_AR_ACTIVE:
		break;
	case K_AR_OFF:	gimmeabreak();	/* ??? */
		dev->keyboard.k_ar_state = K_AR_IDLE;
	}

	/*
	 * We can only assume that pressed keys are reported in the
	 * same order (a minimum of sanity, please) across scans.
	 * To make things readable, do a first pass cancelling out
	 * all keys that are still pressed, and a second one generating
	 * events.  While generating events, do the upstrokes first
	 * from oldest to youngest, then the downstrokes from oldest
	 * to youngest.  This copes with lost packets and provides
	 * a reasonable model even if scans are too slow.
	 */

	/* make a copy of new state first */
	{
		register char	*p, *q, *e;

		p = save;
		q = (char*)msg->body;
		e = (char*)&msg->body[msg_len];

		while (q < e)
			*p++ = *q++;
	}

	/*
	 * Do the cancelling pass
	 */
	{
		register char	*ls, *le, *ns, *ne, *sync;

		ls = (char*)dev->keyboard.last_codes;
		le = (char*)&dev->keyboard.last_codes[dev->keyboard.last_codes_count];
		ns = (char*)msg->body;
		ne = (char*)&msg->body[msg_len];

		/* sync marks where to restart scanning, saving
		   time thanks to ordering constraints */
		for (sync = ns; ls < le; ls++) {
			register char	c = *ls;
			for (ns = sync; ns < ne; ns++)
				if (c == *ns) {
					*ls = *ns = 0;
					sync = ns + 1;
					break;
				}
			/* we could already tell if c is an upstroke,
			   but see the above discussion about errors */
		}
	}
	/*
	 * Now generate all upstrokes
	 */
	{
		register char	*ls, *le;
		register unsigned char	c;

		le = (char*)dev->keyboard.last_codes;
		ls = (char*)&dev->keyboard.last_codes[dev->keyboard.last_codes_count - 1];

		for ( ; ls >= le; ls--)
		    if (c = *ls) {
			/* keep kernel notion of lk201 state consistent */
			(void) lk201_input(0,c);			

			if (outc == 0)
			    screen_keypress_event(0,
						  DEV_KEYBD,
						  c,
						  EVT_BUTTON_UP);
		    }
	}
	/*
	 * And finally the downstrokes
	 */
	{
		register char	*ns, *ne, c, retc;

		ne = (char*)msg->body;
		ns = (char*)&msg->body[msg_len - 1];
		retc = 0;

		for ( ; ns >= ne; ns--)
		    if (c = *ns) {
			register unsigned char	data;

			data = c;
			c = lk201_input(0, data);

			if (c == -2) {	/* just returned from kdb */
				/* NOTE: many things have happened while
				   we were sitting on the stack, now it
				   is last_codes that holds the truth */
#if 1
				/* But the truth might not be welcome.
				   If we get out because we hit RETURN
				   on the rconsole line all is well,
				   but if we did it from the keyboard
				   we get here on the downstroke. Then
				   we will get the upstroke which we
				   would give to X11. People complained
				   about this extra keypress..  so they
				   lose everything. */

				dev->keyboard.last_codes_count = 1;
				dev->keyboard.last_codes[0] = 0;
#endif
				return -1;
			}

			/*
			 * If X11 had better code for the keyboard this
			 * would be an EVT_BUTTON_DOWN.  But that would
			 * screwup the REPEAT function. Grrr.
			 */
			/* outc non zero sez we are in the debugger */
			if (outc == 0) {
			    if (screen_keypress_event(0,
						  DEV_KEYBD,
						  data,
						  EVT_BUTTON_DOWN))
				c = -1; /* consumed by X */
			    else
				if (c > 0) cons_input(0, c, 0);
			}
			/* return the xlated keycode anyways */
			if ((c > 0) && (retc == 0))
			    retc = c;
		    }
		outc = retc;
	}
	/* install new scan state */
	{
		register char	*p, *q, *e;

		p = (char*)dev->keyboard.last_codes;
		q = (char*)save;
		e = (char*)&save[msg_len];

		while (q < e)
			*p++ = *q++;
		dev->keyboard.last_codes_count = msg_len;
	}
	return outc;
}

/*
 * Polled operations: we must do autorepeat by hand. Sigh.
 */
dtop_keyboard_autorepeat(
	dtop_device_t	dev)
{
	spl_t	s = spltty();

	if (dev->keyboard.k_ar_state != K_AR_IDLE)
		dtop_keyboard_handler( dev, 0, DTOP_EVENT_POLL, 0);

	if (dev->keyboard.k_ar_state == K_AR_OFF)
		dev->keyboard.k_ar_state = K_AR_IDLE;
	else
		timeout( dtop_keyboard_autorepeat, dev, dev->keyboard.poll_frequency);

	splx(s);
}

#endif	/*NDTOP>0*/
