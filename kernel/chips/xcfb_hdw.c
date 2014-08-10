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
 *	File: xcfb_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Driver for the MAXine Color Frame Buffer Display,
 *	hardware-level operations.
 */

#include <xcfb.h>
#if	(NXCFB > 0)

#include <platforms.h>

#include <machine/machspl.h>
#include <mach/std_types.h>
#include <chips/busses.h>

#include <chips/screen_defs.h>
#include <chips/pm_defs.h>
#include <machine/machspl.h>

#ifdef	MAXINE

#include <chips/xcfb_monitor.h>

#include <mips/PMAX/pmag_xine.h>
#include <mips/PMAX/tc.h>
#define	enable_interrupt(s,o,x)	(*tc_enable_interrupt)(s,o,x)

#else	/* MAXINE */
You must say the magic words to get the coockies:
#define enable_interrupt(slot,on,xx)
#define	IMS332_ADDRESS
#define	VRAM_OFFSET
#define	IMS332_RESET_ADDRESS
#endif	/* MAXINE */

typedef pm_softc_t	xcfb_softc_t;


/*
 * Definition of the driver for the auto-configuration program.
 */

int	xcfb_probe(), xcfb_intr();
static void	xcfb_attach();

vm_offset_t	xcfb_std[NXCFB] = { 0 };
struct	bus_device *xcfb_info[NXCFB];
struct	bus_driver xcfb_driver = 
        { xcfb_probe, 0, xcfb_attach, 0, xcfb_std, "xcfb", xcfb_info,
	  0, 0, BUS_INTR_DISABLED};

/*
 * Probe/Attach functions
 */

xcfb_probe( /* reg, ui */)
{
	static probed_once = 0;

	/*
	 * Probing was really done sweeping the TC long ago
	 */
	if (tc_probe("xcfb") == 0)
		return 0;
	if (probed_once++ > 1)
		printf("[mappable] ");
	return 1;
}

static void
xcfb_attach(ui)
	struct bus_device *ui;
{
	/* ... */
	printf(": color display");
}


/*
 * Interrupt routine
 */

xcfb_intr(unit,spllevel)
	spl_t	spllevel;
{
	/* interrupt has been acknowledge already */
#if 0
	splx(spllevel);
	/* XXX make it load the unsafe things */
#endif
}

xcfb_vretrace(xcfb, on)
	xcfb_softc_t	*xcfb;
{
	int i;

	for (i = 0; i < NXCFB; i++)
		if (xcfb_info[i]->address == (vm_offset_t)xcfb->framebuffer)
			break;
	if (i == NXCFB) return;

	enable_interrupt(xcfb_info[i]->adaptor, on, 0);
}

/*
 * Boot time initialization: must make device
 * usable as console asap.
 */

/* some of these values are made up using ad hocery */
static struct xcfb_monitor_type monitors[] = {
	{ "VRM17", 1024, 768, 1024, 1024, 16, 33,
		  6, 2, 2, 21, 326, 16, 10, 10 },
	/* XXX Add VRC16 */
	{ "VR262", 1024, 864, 1024, 1024, 16, 35,
		  5, 3, 3, 37, 330, 16, 10, 10 },
	{ "VR299", 1024, 864, 1024, 1024, 16, 35,
		  5, 3, 3, 37, 330, 16, 10, 10 },
	{ 0 }};

/* set from prom command line */
extern unsigned char *monitor_types[4];

xcfb_monitor_type_t xcfb_get_monitor_type()
{
	xcfb_monitor_type_t	m;

	m = monitors;
	if (monitor_types[3])
		while (m->name) {	
			/* xcfb is always on the motherboard (slot 3),
			   fix if you need */
			if (!strcmp(monitor_types[3], m->name))
				break;
			m++;
		}
	if (!m->name)		/* the first is the default */
		m = monitors;
	return m;
}
	

extern int
	xcfb_soft_reset(), xcfb_set_status(),
	ims332_pos_cursor(), ims332_video_on(),
	ims332_video_off(), xcfb_vretrace(),
	pm_get_status(), pm_char_paint(),
	pm_insert_line(), pm_remove_line(),
	pm_clear_bitmap(), pm_map_page();

static struct screen_switch xcfb_sw = {
	screen_noop,		/* graphic_open */
	xcfb_soft_reset,	/* graphic_close */
	xcfb_set_status,	/* set_status */
	pm_get_status,		/* get_status */
	pm_char_paint,		/* char_paint */
	ims332_pos_cursor,	/* pos_cursor */
	pm_insert_line,		/* insert_line */
	pm_remove_line,		/* remove_line */
	pm_clear_bitmap,	/* clear_bitmap */
	ims332_video_on,	/* video_on */
	ims332_video_off,	/* video_off */
	xcfb_vretrace,		/* intr_enable */
	pm_map_page		/* map_page */
};

xcfb_cold_init(unit, up)
	user_info_t	*up;
{
	xcfb_softc_t	*xcfb;
	screen_softc_t	sc = screen(unit);
	int		base = tc_probe("xcfb");
	xcfb_monitor_type_t m = xcfb_get_monitor_type();

	bcopy(&xcfb_sw, &sc->sw, sizeof(sc->sw));

	sc->flags |= COLOR_SCREEN; /* XXX should come from m->flags? */
	sc->frame_scanline_width = m->frame_scanline_width;
	sc->frame_height = m->frame_height;
	sc->frame_visible_width = m->frame_visible_width;
	sc->frame_visible_height = m->frame_visible_height;

	pm_init_screen_params(sc, up);
	(void) screen_up(unit, up);

	xcfb = pm_alloc(unit, IMS332_ADDRESS, base + VRAM_OFFSET, 0);
	xcfb->vdac_registers = (char *)IMS332_RESET_ADDRESS;

	screen_default_colors(up);

	xcfb_soft_reset(sc);

	/*
	 * Clearing the screen at boot saves from scrolling
	 * much, and speeds up booting quite a bit.
	 */
	screen_blitc( unit, 'C'-'@');/* clear screen */
}


#endif	(NXCFB > 0)
