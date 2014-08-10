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
 *	File: fb_misc.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	7/91
 *
 *	Driver for the PMAG-AA simple mono framebuffer
 *
 */

#include <mfb.h>
#if	(NMFB > 0)

/*
 * NOTE: This driver relies heavily on the pm one.
 */

#include <device/device_types.h>
#include <chips/screen_defs.h>
#include <chips/pm_defs.h>
typedef pm_softc_t	fb_softc_t;

#include <chips/bt455.h>

/*
 * Initialize color map, for kernel use
 */
fb_init_colormap(sc)
	screen_softc_t	sc;
{
	fb_softc_t	*fb = (fb_softc_t*)sc->hw_state;
	user_info_t	*up = sc->up;
	color_map_t	Bg_Fg[2];
	register int	i;

	bt455_init_colormap( fb->vdac_registers );

	/* init bg/fg colors */
	for (i = 0; i < 3; i++) {
		up->dev_dep_2.pm.Bg_color[i] = 0x00;
		up->dev_dep_2.pm.Fg_color[i] = 0xff;
	}

	Bg_Fg[0].red = Bg_Fg[0].green = Bg_Fg[0].blue = 0x00;
	Bg_Fg[1].red = Bg_Fg[1].green = Bg_Fg[1].blue = 0xff;
	bt455_cursor_color( fb->vdac_registers, Bg_Fg);
}

/*
 * Large viz small cursor
 */
fb_small_cursor_to_large(up, cursor)
	user_info_t	*up;
	cursor_sprite_t	cursor;
{
	unsigned char	*curbytes, *sprite;
	int             i;
	/* Our cursor turns out mirrored, donno why */
	static unsigned char	mirror[256] = {
		0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 
		0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0, 
		0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 
		0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8, 
		0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 
		0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4, 
		0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 
		0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc, 
		0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 
		0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2, 
		0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 
		0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa, 
		0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 
		0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6, 
		0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 
		0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe, 
		0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 
		0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1, 
		0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 
		0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9, 
		0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 
		0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5, 
		0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 
		0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd, 
		0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 
		0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3, 
		0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 
		0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb, 
		0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 
		0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7, 
		0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 
		0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
	};

	/* Clear out old cursor */
	bzero(	up->dev_dep_2.pm.cursor_sprite,
		sizeof(up->dev_dep_2.pm.cursor_sprite));

	/* small cursor is 32x2 bytes, image(fg) first then mask(bg) */
	curbytes = (unsigned char *) cursor;

	/* we have even byte --> image, odd byte --> mask;
	   line size is 8 bytes instead of 2 */
	sprite   = (unsigned char *) up->dev_dep_2.pm.cursor_sprite;

	for (i = 0; i < 32; i += 2) {
		*sprite++ = mirror[curbytes[i]];	/* fg */
		*sprite++ = mirror[curbytes[i + 32]];	/* bg */
		*sprite++ = mirror[curbytes[i + 1]];	/* fg */
		*sprite++ = mirror[curbytes[i + 33]];	/* bg */
		sprite += 12; /* skip rest of the line */
	}
}

/*
 * Device-specific set status
 */
fb_set_status(sc, flavor, status, status_count)
	screen_softc_t	sc;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	fb_softc_t		*fb = (fb_softc_t*) sc->hw_state;

	switch (flavor) {

	case SCREEN_ADJ_MAPPED_INFO:
		return pm_set_status(sc, flavor, status, status_count);

	case SCREEN_LOAD_CURSOR:

		if (status_count < sizeof(cursor_sprite_t)/sizeof(int))
			return D_INVALID_SIZE;
		fb_small_cursor_to_large(sc->up, (cursor_sprite_t*) status);

		/* Fall through */

	case SCREEN_LOAD_CURSOR_LONG: { /* 3max/3min only */

		sc->flags |= SCREEN_BEING_UPDATED;
		bt431_cursor_sprite(fb->cursor_registers, sc->up->dev_dep_2.pm.cursor_sprite);
		sc->flags &= ~SCREEN_BEING_UPDATED;

		break;
	}
	     
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
		bt455_cursor_color (fb->vdac_registers, c );
		sc->flags &= ~SCREEN_BEING_UPDATED;

		break;
	}

	case SCREEN_SET_CMAP_ENTRY: {
		color_map_entry_t	*e = (color_map_entry_t*) status;

		if (e->index < 8) { /* 8&9 are fg&bg, do not touch */
			sc->flags |= SCREEN_BEING_UPDATED;
			bt455_load_colormap_entry( fb->vdac_registers, e->index, &e->value);
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
 * Do what's needed when X exits
 */
fb_soft_reset(sc)
	screen_softc_t	sc;
{
	fb_softc_t	*fb = (fb_softc_t*) sc->hw_state;
	user_info_t	*up =  sc->up;
	extern cursor_sprite_t	bt431_default_cursor;

	/*
	 * Restore params in mapped structure
	 */
	pm_init_screen_params(sc,up);
	up->row = up->max_row - 1;

	up->dev_dep_2.pm.x26 = 2; /* you do not want to know */
	up->dev_dep_1.pm.x18 = (short*)2;

	/*
	 * Restore RAMDAC chip to default state, and init cursor
	 */
	bt455_init(fb->vdac_registers);
	bt431_init(fb->cursor_registers);

	/*
	 * Load kernel's cursor sprite
	 */
	bt431_cursor_sprite(fb->cursor_registers, bt431_default_cursor);

	/*
	 * Color map and cursor color
	 */
	fb_init_colormap(sc);
}

#endif	(NMFB > 0)
