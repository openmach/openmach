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
 *	File: screen.c
 * 	Author: Alessandro Forin, Robert V. Baron, Joseph S. Barrera,
 *		at Carnegie Mellon University
 *	Date:	9/90
 *
 *	Generic Screen Driver routines.
 */

#include <bm.h>
#if	NBM > 0
#include <dtop.h>

#include <machine/machspl.h>		/* spl definitions */
#include <chips/screen_defs.h>

#include <chips/lk201.h>

#include <mach/std_types.h>
#include <sys/time.h>
#include <kern/time_out.h>
#include <device/io_req.h>

#include <vm/vm_map.h>
#include <device/ds_routines.h>
#include <machine/machspl.h>

#define	Ctrl(x) ((x)-'@')

#define	SCREEN_BLITC_NORMAL		0
#define	SCREEN_BLITC_ROW		1
#define	SCREEN_BLITC_COL		2

#define	SCREEN_ASCII_INVALID	'\0'	/* ascii_screen not valid here */

struct screen_softc screen_softc_data[NBM];
struct screen_softc *screen_softc[NBM];

short screen_console = 0;

/* Forward decls */

void screen_blitc(
	int	unit,
	register unsigned char c);

void screen_blitc_at(
	register screen_softc_t	sc,
	unsigned char		c,
	short 			row,
	short 			col);


/*
 8-D	A "Screen" has a bitmapped display, a keyboard and a mouse
 *
 */

#if NDTOP > 0
extern int 	dtop_kbd_probe(), dtop_set_status(), dtop_kbd_reset(),
		dtop_ring_bell();
#endif /* NDTOP */
extern int 	lk201_probe(), lk201_set_status(), lk201_reset(),
		lk201_ring_bell();

struct kbd_probe_vector {
	int	(*probe)();
	int	(*set_status)();
	int	(*reset)();
	int	(*beep)();
} kbd_vector[] = {
#if NDTOP > 0
	{dtop_kbd_probe, dtop_set_status, dtop_kbd_reset, dtop_ring_bell},
#endif
	{lk201_probe, lk201_set_status, lk201_reset, lk201_ring_bell},
	{0,}
};

screen_find_kbd(int unit)
{
	struct kbd_probe_vector *p = kbd_vector;

	for (; p->probe; p++) {
		if ((*p->probe) (unit)) {
			screen_softc[unit]->kbd_set_status = p->set_status;
			screen_softc[unit]->kbd_reset = p->reset;
			screen_softc[unit]->kbd_beep = p->beep;
			return 1;
		}
	}
	return 0;
}

/*
 * The screen probe routine looks for the associated
 * keyboard and mouse, at the same unit number.
 */
screen_probe(int unit)
{
	if (unit >= NBM)
		return 0;
	screen_softc[unit] = &screen_softc_data[unit];	
	if (!screen_find())
		return 0;
	if (!screen_find_kbd(unit))
		return 0;
	mouse_probe(unit);
	return 1;
}

screen_softc_t
screen(int unit)
{
	return screen_softc[unit];
}

/*
 * This is an upcall from the specific display
 * hardware, to register its descriptor
 */
screen_attach(
	int	unit,
	char	**hwp)
{
	register screen_softc_t	sc = screen_softc[unit];

	sc->hw_state = hwp;
	sc->blitc_state = SCREEN_BLITC_NORMAL;
}

/*
 * This is another upcall (for now) to register
 * the user-mapped information
 */
void
screen_up(
	int		unit,
	user_info_t	*screen_data)
{
	register screen_softc_t	sc = screen_softc[unit];

	sc->up = screen_data;
	mouse_notify_mapped(unit, unit, screen_data);
	screen_event_init(screen_data);
	ascii_screen_initialize(sc);
}

/*
 * Screen saver
 */
#define SSAVER_MIN_TIME (2*60)	/* Minimum fade interval 		*/
long ssaver_last = 0;	/* Last tv_sec that the keyboard was touched 	*/
long ssaver_time = 0;	/* Number of seconds before screen is blanked	*/

void
ssaver_bump(int unit)
{
	register long tnow = time.tv_sec;

	if ((tnow - ssaver_last) > ssaver_time)
		screen_on_off(unit, TRUE);
	ssaver_last = tnow;
}

void
screen_saver(int unit)
{
	register screen_softc_t	sc = screen_softc[unit];

	/* wakeup each minute */
	timeout(screen_saver, unit, hz * 60);
	if ((time.tv_sec - ssaver_last) >= ssaver_time)
		/* this does nothing if already off */
		screen_on_off(unit, FALSE);
}

/*
 * Screen open routine.  We are also notified
 * of console operations if our screen is acting
 * as a console display.
 */
screen_open(
	int		unit,
	boolean_t	console_only)
{
	register screen_softc_t	sc = screen_softc[unit];

	/*
	 * Start screen saver on first (console) open
	 */
	if (!ssaver_time) {
		ssaver_time = 10*60;		/* 10 minutes to fade	*/
		ssaver_bump(unit);			/* .. from now		*/
		screen_saver(unit);		/* Start timer		*/
	}
	/*
	 * Really opening the screen or just notifying ?
	 */
	if (!console_only) {
#if 0
		(*sc->sw.init_colormap)(sc);
#endif
		screen_event_init(sc->up);
		ascii_screen_initialize(sc);
		(*sc->sw.graphic_open)(sc->hw_state);
		sc->mapped = TRUE;
	}
}

/*
 * Screen close
 */
screen_close(
	int		unit,
	boolean_t	console_only)
{
	register screen_softc_t	sc = screen_softc[unit];

	/*
	 * Closing of the plain console has no effect
	 */
	if (!console_only) {
		user_info_t	*up = sc->up;

		screen_default_colors(up);
		/* mapped info, cursor and colormap resetting */
		(*sc->sw.graphic_close)(sc);

		/* turn screen on, and blank it */
		screen_on_off(unit, TRUE);
		ascii_screen_initialize(sc);
		(*sc->sw.clear_bitmap)(sc);

		/* position cursor circa page end */
		up->row = up->max_row - 1;
		up->col = 0;

		/* set keyboard back our way */
		(*sc->kbd_reset)(unit);
		lk201_lights(unit, LED_OFF);

		sc->mapped = FALSE;
	}
}

screen_default_colors(
	user_info_t	*up)
{
	register int	i;

	/* restore bg and fg colors */
	for (i = 0; i < 3; i++) {
		up->dev_dep_2.pm.Bg_color[i] = 0x00;
		up->dev_dep_2.pm.Fg_color[i] = 0xff;
	}
}

/*
 * Write characters to the screen
 */
screen_write(
	int			unit,
	register io_req_t	ior)
{
	register int		count;
	register unsigned char	*data;
	vm_offset_t		addr;

	if (unit == 1)	/* no writes to the mouse */
		return D_INVALID_OPERATION;

	data  = (unsigned char*) ior->io_data;
	count = ior->io_count;
	if (count == 0)
		return (D_SUCCESS);

	if (!(ior->io_op & IO_INBAND)) {
		vm_map_copy_t copy = (vm_map_copy_t) data;
		kern_return_t kr;

		kr = vm_map_copyout(device_io_map, &addr, copy);
		if (kr != KERN_SUCCESS)
			return (kr);
		data = (unsigned char *) addr;
	}

	/* Spill chars out, might fault data in */
	while (count--)
		screen_blitc(unit, *data++);

	if (!(ior->io_op & IO_INBAND))
		(void) vm_deallocate(device_io_map, addr, ior->io_count);

	return (D_SUCCESS);
}

/*
 * Read from the screen.  This really means waiting
 * for an event, which can be either a keypress on
 * the keyboard (or pointer) or a mouse movement.
 * If there are no available events we queue the
 * request for later.
 */
queue_head_t screen_read_queue = { &screen_read_queue, &screen_read_queue };
boolean_t    screen_read_done();

screen_read(
	int			unit,
	register io_req_t	ior)
{
	register user_info_t	*up = screen_softc[unit]->up;
	register spl_t		s = spltty();

	if (up->evque.q_head != up->evque.q_tail) {
		splx(s);
		return (D_SUCCESS);
	}
	ior->io_dev_ptr = (char *) up;
	ior->io_done = screen_read_done;
	enqueue_tail(&screen_read_queue, (queue_entry_t) ior);
	splx(s);
	return (D_IO_QUEUED);
}

boolean_t
screen_read_done(
	register io_req_t	ior)
{
	register user_info_t *up = (user_info_t *) ior->io_dev_ptr;
	register spl_t    s = spltty();

	if (up->evque.q_head != up->evque.q_tail) {
		splx(s);
		(void) ds_read_done(ior);
		return (TRUE);
	}
	enqueue_tail(&screen_read_queue, (queue_entry_t) ior);
	splx(s);
	return (FALSE);
}

static
screen_event_posted(
	register user_info_t	*up)
{
	if (up->evque.q_head != up->evque.q_tail) {
		register io_req_t	ior;
		while ((ior = (io_req_t)dequeue_head(&screen_read_queue)))
			iodone(ior);
	}
}

boolean_t compress_mouse_events = TRUE;

/*
 * Upcall from input pointer devices
 */
screen_motion_event(
	int	unit,
	int	device,
	int	x,
	int	y)
{
	register screen_softc_t	sc = screen_softc[unit];
	register user_info_t	*up = sc->up;
	register unsigned	next;
	unsigned int		ev_time;

	/*
	 * Take care of scale/threshold issues
	 */
	if (device == DEV_MOUSE) {
		register int	scale;

		scale = up->mouse_scale;

		if (scale >= 0) {
			register int		threshold;
			register boolean_t	neg;

			threshold = up->mouse_threshold;

			neg = (x < 0);
			if (neg) x = -x;
			if (x >= threshold)
				x += (x - threshold) * scale;
			if (neg) x = -x;

			neg = (y < 0);
			if (neg) y = -y;
			if (y >= threshold)
				y += (y - threshold) * scale;
			if (neg) y = -y;

		}

		/* we expect mices in incremental mode */
		x += up->mouse_loc.x;
		y += up->mouse_loc.y;

	} else if (device == DEV_TABLET) {

		/* we expect tablets in absolute mode */
		x = (x * up->dev_dep_2.pm.tablet_scale_x) / 1000;
		y = ((2200 - y) * up->dev_dep_2.pm.tablet_scale_y) / 1000;
	
	} /* else who are you */

	/*
	 * Clip if necessary
	 */
	{
		register int		max;

		if (x > (max = up->max_cur_x))
			x = max;
		if (y > (max = up->max_cur_y))
			y = max;
	}

	/*
	 * Did it actually move
	 */
	if ((up->mouse_loc.x == x) &&
	    (up->mouse_loc.y == y))
		return;

	/*
	 * Update mouse location, and cursor
	 */
	up->mouse_loc.x = x;
	up->mouse_loc.y = y;

	screen_set_cursor(sc, x, y);

	/*
	 * Add point to track.
	 */
	{
		register screen_timed_point_t	*tr;

		/* simply add and overflow if necessary */
		next = up->evque.t_next;
		if (next >= MAX_TRACK)
			next = MAX_TRACK-1;
		tr		= &up->point_track[next++];
		up->evque.t_next = (next == MAX_TRACK) ? 0 : next;

		ev_time 	= (unsigned) approx_time_in_msec();
		tr->time	= ev_time;
		tr->x		= x;
		tr->y		= y;
	}

	/*
	 * Don't post event if mouse is within bounding box,
	 * Note our y-s are upside down
	 */
	if (y <  up->mouse_box.bottom &&
	    y >= up->mouse_box.top &&
	    x <  up->mouse_box.right &&
	    x >= up->mouse_box.left)
		return;
	up->mouse_box.bottom = 0;	/* X11 wants it ? */

	/*
	 * Post motion event now
	 */
#define round(x)	((x) & (MAX_EVENTS - 1))
	{
		register unsigned int	head = up->evque.q_head;
		register unsigned int	tail = up->evque.q_tail;
		register screen_event_t	*ev;

		if (round(tail + 1) == head)	/* queue full, drop it */
			return;

		/* see if we can spare too many motion events */
		next = round(tail - 1);
		if (compress_mouse_events &&
		    (tail != head) && (next != head)) {
			ev = & up->event_queue[next];
			if (ev->type == EVT_PTR_MOTION) {
				ev->x 		= x;
				ev->y		= y;
				ev->time	= ev_time;
				ev->device	= device;
				screen_event_posted(up);
				return;
			}
		}
		ev		= & up->event_queue[tail];
		ev->type	= EVT_PTR_MOTION;
		ev->time	= ev_time;
		ev->x		= x;
		ev->y		= y;
		ev->device	= device;

		/* added to queue */
		up->evque.q_tail = round(tail + 1);
	}

	/*
	 * Wakeup any sleepers
	 */
	screen_event_posted(up);
}

/*
 * Upcall from keypress input devices
 * Returns wether the event was consumed or not.
 */
boolean_t
screen_keypress_event(
	int	unit,
	int	device,
	int	key,
	int	type)
{
	register screen_softc_t	sc = screen_softc[unit];
	register user_info_t	*up = sc->up;
	register unsigned int	head, tail;
	register screen_event_t	*ev;

	if (!sc->mapped) {
		int col, row;

		if (device != DEV_MOUSE)
			return FALSE;
		/* generate escapes for mouse position */
		col = up->mouse_loc.x / 8;
		row = up->mouse_loc.y / 15;
		mouse_report_position(unit, col, row, key, type);
		return TRUE;
	}

	head = up->evque.q_head;
	tail = up->evque.q_tail;

	if (round(tail + 1) == head)	/* queue full */
		return TRUE;

	ev		= & up->event_queue[tail];
	ev->key		= key;
	ev->type	= type;
	ev->device	= device;
	ev->time 	= approx_time_in_msec();
	ev->x		= up->mouse_loc.x;
	ev->y		= up->mouse_loc.y;

	up->evque.q_tail = round(tail + 1);

	screen_event_posted(up);

	return TRUE;
}
#undef	round

/*
 * Event queue initialization
 */
screen_event_init(
	user_info_t	*up)
{
	up->evque.q_size = MAX_EVENTS;
	up->evque.q_head = 0;
	up->evque.q_tail = 0;
;	up->evque.t_size = MAX_TRACK;
	up->evque.t_next = 0;
	up->evque.timestamp = approx_time_in_msec();

}

/*
 * Set/Get status functions.
 * ...
 */
io_return_t
screen_set_status(
	int		unit,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	status_count)
{
	register screen_softc_t	sc = screen_softc[unit];
	register user_info_t	*up = sc->up;
	io_return_t		ret = D_SUCCESS;

/* XXX checks before getting here */

	switch (flavor) {

	case SCREEN_INIT:
		ascii_screen_initialize(sc);
		break;

	case SCREEN_ON:
		screen_on_off(unit, TRUE);
		break;

	case SCREEN_OFF:
		screen_on_off(unit, FALSE);
		break;

	case SCREEN_FADE: {
		register int tm = * (int *) status;

		untimeout(screen_saver, unit);	/* stop everything and 	   */
		if (tm == -1) 			/* don't reschedule a fade */
			break; 
		if (tm < SSAVER_MIN_TIME)
			tm = SSAVER_MIN_TIME;
		ssaver_time = tm;
		ssaver_bump(unit);
		screen_saver(unit);
		break;
	}

	case SCREEN_SET_CURSOR: {
		screen_point_t		*loc = (screen_point_t*) status;

		if (status_count < sizeof(screen_point_t)/sizeof(int))
			return D_INVALID_SIZE;

		sc->flags |= SCREEN_BEING_UPDATED;
		up->mouse_loc = *loc;
		sc->flags &= ~SCREEN_BEING_UPDATED;

		screen_set_cursor(sc, loc->x, loc->y);

		break;
	}

	/* COMPAT: these codes do nothing, but we understand */
	case _IO('q', 8):	/* KERNLOOP */
	case _IO('q', 9):	/* KERNUNLOOP */
	case _IO('g', 21):	/* KERN_UNLOOP */
		break;

	/*
	 * Anything else is either device-specific,
	 * or for the keyboard
	 */
	default:
		ret = (*sc->sw.set_status)(sc, flavor, status, status_count);
		if (ret == D_INVALID_OPERATION)
			ret = (*sc->kbd_set_status)(unit, flavor,
						    status, status_count);
		break;
	}
	return ret;
}

io_return_t
screen_get_status(
	int		unit,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	*count)
{
	register screen_softc_t	sc = screen_softc[unit];

	if (flavor == SCREEN_STATUS_FLAGS) {
		*(int *)status	= sc->flags;
		*count		= 1;
		return D_SUCCESS;
	} else if (flavor == SCREEN_HARDWARE_INFO) {
		screen_hw_info_t	*hinfo;

		hinfo = (screen_hw_info_t*)status;
		hinfo->frame_width = sc->frame_scanline_width;
		hinfo->frame_height = sc->frame_height;
		hinfo->frame_visible_width = sc->frame_visible_width;
		hinfo->frame_visible_height = sc->frame_visible_height;
		*count = sizeof(screen_hw_info_t)/sizeof(int);
		return D_SUCCESS;
	} else

		return (*sc->sw.get_status)(sc, flavor, status, count);
}

/*
 * Routine to handle display and control characters sent to screen
 */
void
screen_blitc(
	int	unit,
	register unsigned char c)
{
	register screen_softc_t	sc = screen_softc[unit];
	register user_info_t	*up = sc->up;
	register unsigned char	*ap;
	register int i;

	/*
	 * Handle cursor positioning sequence
	 */
	switch (sc->blitc_state) {
	case SCREEN_BLITC_NORMAL:
		break;

	case SCREEN_BLITC_ROW:
		c -= ' ';
		if (c >= up->max_row) {
			up->row = up->max_row - 1;
		} else {
			up->row = c;
		}
		sc->blitc_state = SCREEN_BLITC_COL;
		return;

	case SCREEN_BLITC_COL:
		c -= ' ';
		if (c >= up->max_col) {
			up->col = up->max_col - 1;
		} else {
			up->col = c;
		}
		sc->blitc_state = SCREEN_BLITC_NORMAL;
		goto move_cursor;
	}

	c &= 0xff;

	/* echo on rconsole line */
	rcputc(c);

	/* we got something to say, turn on the TV */
	ssaver_bump(unit);

	switch (c) {
						/* Locate cursor*/
	case Ctrl('A'):				/* ^A -> cm	*/
		sc->blitc_state = SCREEN_BLITC_ROW;
		return;

						/* Home cursor  */
	case Ctrl('B'):				/* ^B -> ho	*/
		up->row = 0;
		up->col = 0;
		break;

						/* Clear screen */
	case Ctrl('C'):				/* ^C -> cl	*/
		up->row = 0;
		up->col = 0;
		(*sc->sw.clear_bitmap)(sc);
		break;

						/* Move forward */
	case Ctrl('D'):				/* ^D -> nd	*/
		screen_advance_position(sc);
		break;

						/* Clear to eol */
	case Ctrl('E'):				/* ^E -> ce	*/
		ap = &sc->ascii_screen[up->max_col*up->row + up->col];
		for (i = up->col; i < up->max_col; i++, ap++) {
			if (sc->standout || *ap != ' ') {
				if (sc->standout) {
					*ap = SCREEN_ASCII_INVALID;
				} else {
					*ap = ' ';
				}
				screen_blitc_at(sc, ' ', up->row, i);
			}
		}
		return;

						/* Cursor up 	*/
	case Ctrl('F'):				/* ^F -> up	*/
		if (up->row != 0) up->row--;
		break;

	case Ctrl('G'):				/* ^G -> bell	*/
		(*sc->kbd_beep)(unit);
		return;

						/* Backspace	*/
	case Ctrl('H'):				/* ^H -> bs	*/
		if (--up->col < 0)
			up->col = 0;
		break;

	case Ctrl('I'):				/* ^I -> tab	*/
		up->col += (8 - (up->col & 0x7));
		break;

	case Ctrl('J'):				/* ^J -> lf	*/
		if (up->row+1 >= up->max_row)
			(*sc->sw.remove_line)(sc, 0);
		else
			up->row++;
		break;

						/* Start rev-video */
	case Ctrl('K'):				/* ^K -> so	*/
		sc->standout = 1;
		return;

						/* End rev-video */
	case Ctrl('L'):				/* ^L -> se	*/
		sc->standout = 0;
		return;

	case Ctrl('M'):				/* ^M -> return	*/
		up->col = 0;
		break;

						/* Save cursor position */
	case Ctrl('N'):				/* ^N -> sc	*/
		sc->save_col = up->col;
		sc->save_row = up->row;
		return;

						/* Restore cursor position */
	case Ctrl('O'):				/* ^O -> rc	*/
		up->row = sc->save_row;
		up->col = sc->save_col;
		break;

						/* Add blank line */
	case Ctrl('P'):				/* ^P -> al	*/
		(*sc->sw.insert_line)(sc, up->row);
		return;

						/* Delete line	*/
	case Ctrl('Q'):				/* ^Q -> dl	*/
		(*sc->sw.remove_line)(sc, up->row);
		return;

	default:
		/*
		 * If the desired character is already there, then don't
		 * bother redrawing it. Always redraw standout-ed chars,
		 * so that we can assume that all cached characters are
		 * un-standout-ed. (This could be fixed.)
		 */
		ap = &sc->ascii_screen[up->max_col*up->row + up->col];
		if (sc->standout || c != *ap) {
			if (sc->standout) {
				*ap = SCREEN_ASCII_INVALID;
			} else {
				*ap = c;
			}
			screen_blitc_at(sc, c, up->row, up->col);
		}
		screen_advance_position(sc);
		break;
	}

move_cursor:
	screen_set_cursor(sc, up->col*8, up->row*15);

}


/*
 * Advance current position, wrapping and scrolling when necessary
 */
screen_advance_position(
	register screen_softc_t	sc)
{
	register user_info_t	*up = sc->up;

	if (++up->col >= up->max_col) {
		up->col = 0 ;
		if (up->row+1 >= up->max_row) {
			(*sc->sw.remove_line)(sc, 0);
		} else {
			up->row++;
		}
	}
}


/*
 * Routine to display a character at a given position
 */
void
screen_blitc_at(
	register screen_softc_t	sc,
	unsigned char		c,
	short 			row,
	short 			col)
{
	/*
	 * Silently ignore non-printable chars
	 */
	if (c < ' ' || c > 0xfd)
		return;
	(*sc->sw.char_paint)(sc, c, row, col);
}

/*
 * Update sc->ascii_screen array after deleting ROW
 */
ascii_screen_rem_update(
	register screen_softc_t	sc,
	int	row)
{
	register user_info_t	*up = sc->up;
	register unsigned int	col_w, row_w;
	register unsigned char	*c, *end;

	/* cache and sanity */
	col_w	= up->max_col;
	if (col_w > MaxCharCols)
		col_w = MaxCharCols;
	row_w	= up->max_row;
	if (row_w > MaxCharRows)
		row_w = MaxCharRows;

	/* scroll up */
	c	= &sc->ascii_screen[row * col_w];
	end	= &sc->ascii_screen[(row_w-1) * col_w];
	for (; c < end; c++)		/* bcopy ? XXX */
		*c = *(c + col_w);

	/* zero out line that entered at end */
	c	= end;
	end	= &sc->ascii_screen[row_w * col_w];
	for (; c < end; c++)
		*c = ' ';

}

/*
 * Update sc->ascii_screen array after opening new ROW
 */
ascii_screen_ins_update(
	register screen_softc_t	sc,
	int	row)
{
	register user_info_t	*up = sc->up;
	register unsigned int	col_w, row_w;
	register unsigned char	*c, *end;

	/* cache and sanity */
	col_w	= up->max_col;
	if (col_w > MaxCharCols)
		col_w = MaxCharCols;
	row_w	= up->max_row;
	if (row_w > MaxCharRows)
		row_w = MaxCharRows;

	/* scroll down */
	c	= &sc->ascii_screen[row_w * col_w - 1];
	end	= &sc->ascii_screen[(row + 1) * col_w];
	for (; c >= end; c--)
		*c = *(c - col_w);

	/* zero out line that entered at row */
	c	= end - 1;
	end	= &sc->ascii_screen[row * col_w];
	for (; c >= end; c--)
		*c = ' ';
}

/*
 * Init charmap
 */
ascii_screen_fill(
	register screen_softc_t	sc,
	char	c)
{
	register user_info_t	*up = sc->up;
	register int i, to;

	to = up->max_row * up->max_col;
	for (i = 0; i < to; i++) {
		sc->ascii_screen[i] = c;
	}
}

ascii_screen_initialize(
	register screen_softc_t	sc)
{
	ascii_screen_fill(sc, SCREEN_ASCII_INVALID);
}

/*
 * Cursor positioning
 */
screen_set_cursor(
	register screen_softc_t	sc,
	register int x,
	register int y)
{
	register user_info_t	*up = sc->up;

	/* If we are called from interrupt level..  */
	if (sc->flags & SCREEN_BEING_UPDATED)
	       return;
	sc->flags |= SCREEN_BEING_UPDATED;
	/*
	 * Note that that was not atomic, but this is
	 * a two-party game on the same processor and
	 * not a real parallel program.
	 */

	/* Sanity checks (ignore noise) */
	if (y < up->min_cur_y || y > up->max_cur_y)
		y = up->cursor.y;
	if (x < up->min_cur_x || x > up->max_cur_x)
		x = up->cursor.x;

	/*
	 * Track cursor position
	 */
	up->cursor.x = x;
	up->cursor.y = y;

	(*sc->sw.pos_cursor)(*(sc->hw_state), x, y);

	sc->flags &= ~SCREEN_BEING_UPDATED;
}

screen_on_off(
	int		unit,
	boolean_t	on)
{
	register screen_softc_t	sc = screen_softc[unit];

	if (sc->sw.video_on == 0)	/* sanity */
		return;

	if (on)
		(*sc->sw.video_on)(sc->hw_state, sc->up);
	else
		(*sc->sw.video_off)(sc->hw_state, sc->up);
}

screen_enable_vretrace(
	int		unit,
	boolean_t	on)
{
	register screen_softc_t	sc = screen_softc[unit];
	(*sc->sw.intr_enable)(sc->hw_state, on);
}

/*
 * For our purposes, time does not need to be
 * precise but just monotonic and approximate
 * to about the millisecond.  Instead of div/
 * mul by 1000 we div/mul by 1024 (shifting).
 *
 * Well, it almost worked. The only problem
 * is that X somehow checks the time against
 * gettimeofday() and .. turns screen off at
 * startup if we use approx time. SO we are
 * back to precise time, sigh.
 */
approx_time_in_msec()
{
#if 0
	return ((time.seconds << 10) + (time.microseconds >> 10));
#else
	return ((time.seconds * 1000) + (time.microseconds / 1000));
#endif
}

/*
 * Screen mapping to user space
 * This is called on a per-page basis
 */
screen_mmap(
	int		dev,
	vm_offset_t	off,
	int		prot)
{
	/* dev is safe, but it is the mouse's one */
	register screen_softc_t	sc = screen_softc[dev-1];

	(*sc->sw.map_page)(sc, off, prot);
}

#endif	NBM > 0
