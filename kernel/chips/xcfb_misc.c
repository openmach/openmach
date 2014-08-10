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
 *	File: xcfb_misc.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Driver for the MAXine color framebuffer
 *
 */

#include <xcfb.h>
#if	(NXCFB > 0)

/*
 * NOTE: This driver relies heavily on the pm one.
 */

#include <device/device_types.h>

#include <chips/screen_defs.h>

#include <chips/xcfb_monitor.h>
#include <chips/pm_defs.h>
typedef pm_softc_t	xcfb_softc_t;

#include <chips/ims332.h>
#define	ims332		cursor_registers


/*
 * Initialize color map, for kernel use
 */
xcfb_init_colormap(sc)
	screen_softc_t	sc;
{
	xcfb_softc_t	*xcfb = (xcfb_softc_t*)sc->hw_state;
	user_info_t	*up = sc->up;
	color_map_t	Bg_Fg[2];
	register int	i;

	ims332_init_colormap( xcfb->ims332 );

	/* init bg/fg colors */
	for (i = 0; i < 3; i++) {
		up->dev_dep_2.pm.Bg_color[i] = 0x00;
		up->dev_dep_2.pm.Fg_color[i] = 0xff;
	}

	Bg_Fg[0].red = Bg_Fg[0].green = Bg_Fg[0].blue = 0x00;
	Bg_Fg[1].red = Bg_Fg[1].green = Bg_Fg[1].blue = 0xff;
	ims332_cursor_color( xcfb->ims332, Bg_Fg);
}

/*
 * Large viz small cursor
 */
xcfb_small_cursor_to_large(up, cursor)
	user_info_t	*up;
	cursor_sprite_t	cursor;
{
	unsigned short	new_cursor[32];
	unsigned char	*cursor_bytes;
	char		*sprite;
	register int	i;

	/* Clear out old cursor */
	bzero(	up->dev_dep_2.pm.cursor_sprite,
		sizeof(up->dev_dep_2.pm.cursor_sprite));

	/* small cursor is 32x2 bytes, fg first */
	cursor_bytes = (unsigned char *) cursor;

	/* use the upper left corner of the large cursor
	 * as a 64x1 cursor, fg&bg alternated */
	for (i = 0; i < 32; i++) {
		register short		nc = 0;
		register unsigned char	fg, bg;
		register int		j, k;

		fg = cursor_bytes[i];
		bg = cursor_bytes[i + 32];
		bg &= ~fg;
		for (j = 1, k = 0; j < 256; j <<= 1) {
			nc |= (bg & j) << (k++);
			nc |= (fg & j) << (k);
		}
		new_cursor[i] = nc;
	}

	/* Now stick it in the proper place */

	cursor_bytes = (unsigned char *) new_cursor;
	sprite = up->dev_dep_2.pm.cursor_sprite;
	for (i = 0; i < 64; i += 4) {
		*sprite++ = cursor_bytes[i + 0];
		*sprite++ = cursor_bytes[i + 1];
		*sprite++ = cursor_bytes[i + 2];
		*sprite++ = cursor_bytes[i + 3];
		sprite += 12; /* skip rest of the line */
	}
}


/*
 * Device-specific set status
 */
xcfb_set_status(sc, flavor, status, status_count)
	screen_softc_t	sc;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	xcfb_softc_t		*xcfb = (xcfb_softc_t*) sc->hw_state;

	switch (flavor) {

	case SCREEN_ADJ_MAPPED_INFO:
		return pm_set_status(sc, flavor, status, status_count);

	case SCREEN_LOAD_CURSOR:

		if (status_count < sizeof(cursor_sprite_t)/sizeof(int))
			return D_INVALID_SIZE;

		xcfb_small_cursor_to_large(sc->up, (cursor_sprite_t*) status);

		/* Fall through */

	case SCREEN_LOAD_CURSOR_LONG: /* 3max only */

		sc->flags |= SCREEN_BEING_UPDATED;
		ims332_cursor_sprite(xcfb->ims332, sc->up->dev_dep_2.pm.cursor_sprite);
		sc->flags &= ~SCREEN_BEING_UPDATED;

		break;
	     
	case SCREEN_SET_CURSOR_COLOR: {
		color_map_t		c[2];
		register cursor_color_t	*cc = (cursor_color_t*) status;

		c[0].red   = cc->Bg_rgb[0];
		c[0].green = cc->Bg_rgb[1];
		c[0].blue  = cc->Bg_rgb[2];
		c[1].red   = cc->Fg_rgb[0];
		c[1].green = cc->Fg_rgb[1];
		c[1].blue  = cc->Fg_rgb[2];

		sc->flags |= SCREEN_BEING_UPDATED;
		ims332_cursor_color (xcfb->ims332, c );
		sc->flags &= ~SCREEN_BEING_UPDATED;

		break;
	}

	case SCREEN_SET_CMAP_ENTRY: {
		color_map_entry_t	*e = (color_map_entry_t*) status;

		if (e->index < 256) {
			sc->flags |= SCREEN_BEING_UPDATED;
			ims332_load_colormap_entry( xcfb->ims332, e->index, &e->value);
			sc->flags &= ~SCREEN_BEING_UPDATED;
		}
		break;
	}

	default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

/*
 * Hardware initialization
 */
xcfb_init_screen(xcfb)
	xcfb_softc_t *xcfb;
{
	extern xcfb_monitor_type_t xcfb_get_monitor_type();

	ims332_init( xcfb->ims332, xcfb->vdac_registers, xcfb_get_monitor_type());
}

/*
 * Do what's needed when X exits
 */
xcfb_soft_reset(sc)
	screen_softc_t	sc;
{
	xcfb_softc_t	*xcfb = (xcfb_softc_t*) sc->hw_state;
	user_info_t	*up =  sc->up;
	extern cursor_sprite_t	dc503_default_cursor;

	/*
	 * Restore params in mapped structure
	 */
	pm_init_screen_params(sc,up);
	up->row = up->max_row - 1;

	up->dev_dep_2.pm.x26 = 2; /* you do not want to know */
	up->dev_dep_1.pm.x18 = (short*)2;

	/*
	 * Restore RAMDAC chip to default state
	 */
	xcfb_init_screen(xcfb);

	/*
	 * Load kernel's cursor sprite: just use the same pmax one
	 */
	xcfb_small_cursor_to_large(up, dc503_default_cursor);
	ims332_cursor_sprite(xcfb->ims332, up->dev_dep_2.pm.cursor_sprite);

	/*
	 * Color map and cursor color
	 */
	xcfb_init_colormap(sc);
}




#endif	(NXCFB > 0)
