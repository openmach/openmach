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
 *	File: bt478.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Routines for the bt478 Cursor/RAMDAC chip
 */

#include <platforms.h>

#include <chips/bt478.h>
#include <chips/screen.h>

#ifdef	DECSTATION

typedef struct {
	volatile unsigned char	addr_mapwa;
	char						pad0[3];
	volatile unsigned char	addr_map;
	char						pad1[3];
	volatile unsigned char	addr_mask;
	char						pad2[3];
	volatile unsigned char	addr_mapra;
	char						pad3[3];
	volatile unsigned char	addr_overwa;
	char						pad4[3];
	volatile unsigned char	addr_over;
	char						pad5[3];
	volatile unsigned char	addr_xxxx;
	char						pad6[3];
	volatile unsigned char	addr_overra;
	char						pad7[3];
} bt478_padded_regmap_t;

#else	/*DECSTATION*/

typedef bt478_regmap_t	bt478_padded_regmap_t;
#define	wbflush()

#endif	/*DECSTATION*/


/*
 * Cursor color
 */
static
bt478_load_cc(bt478, bg, fg)
	register bt478_padded_regmap_t	*bt478;
	unsigned int		*bg, *fg;
{
	register int    i;

	/* See init function for gotchas */

	bt478->addr_overwa = 4;
	wbflush();
	for (i = 0; i < 3; i++) {
		bt478->addr_over = (*bg++) >> 8;
		wbflush();
	}

	bt478->addr_overwa = 8;
	wbflush();
	bt478->addr_over = 0x00;
	wbflush();
	bt478->addr_over = 0x00;
	wbflush();
	bt478->addr_over = 0x7f;
	wbflush();

	bt478->addr_overwa = 12;
	wbflush();
	for (i = 0; i < 3; i++) {
		bt478->addr_over = (*fg++) >> 8;
		wbflush();
	}

}


bt478_cursor_color(bt478, color)
	bt478_padded_regmap_t	*bt478;
	cursor_color_t	*color;
{
	register int    	i;
	register unsigned int	*p;

	/* Do it twice, in case of collisions */

	bt478_load_cc(bt478, color->Bg_rgb, color->Fg_rgb);

	p = color->Bg_rgb;
	for (i = 0; i < 3; i++) {
		bt478->addr_over = (*p++) >> 8;
		wbflush();
	}

	p = color->Fg_rgb;
	for (i = 0; i < 3; i++) {
		bt478->addr_over = (*p++) >> 8;
		wbflush();
	}

	bt478_load_cc(bt478, color->Bg_rgb, color->Fg_rgb);
}

/*
 * Color map
 */
bt478_load_colormap( regs, map)
	bt478_padded_regmap_t	*regs;
	color_map_t	*map;
{
	register int i;

	regs->addr_mapwa = 0;
	wbflush();
	for (i = 0; i < 256; i++, map++) {
		regs->addr_map = map->red;
		wbflush();
		regs->addr_map = map->green;
		wbflush();
		regs->addr_map = map->blue;
		wbflush();
	}
}

bt478_load_colormap_entry( regs, entry, map)
	bt478_padded_regmap_t	*regs;
	color_map_t	*map;
{
	regs->addr_mapwa = entry & 0xff;
	wbflush();
	regs->addr_map = map->red;
	wbflush();
	regs->addr_map = map->green;
	wbflush();
	regs->addr_map = map->blue;
	wbflush();
}

/*
 * Video on/off (unused)
 */
bt478_video_on(pregs, up)
	bt478_padded_regmap_t	**pregs;
{
	(*pregs)->addr_mask = 0xff;
}

bt478_video_off(pregs, up)
	bt478_padded_regmap_t	**pregs;
{
	(*pregs)->addr_mask = 0;
}

/*
 * Initialization
 */
static
bt478_overlay(regs, plane)
	bt478_padded_regmap_t	*regs;
	unsigned char	*plane;
{
	*plane = 0xff;

	/* Overlay planes 0 and 1 are wired zero, overlay plane 2
	   is plane "B" of the cursor (second half of it), plane 3
	   is plane "A" of the cursor.  Soo, we get three colors
	   for the cursor, at map entries 4, 8 and 12 */
#	define	ovv(i,r,g,b)			\
	regs->addr_overwa = i; wbflush();	\
	regs->addr_over = r; wbflush();		\
	regs->addr_over = b; wbflush();		\
	regs->addr_over = g; wbflush();

	ovv(4,0,0,0); ovv(8,0,0,0x7f); ovv(12,0xff,0xff,0xff);

#	undef ovv

	/* enable data input */
	regs->addr_mask = 0xff;
}

bt478_init_bw_map(regs, plane)
	bt478_padded_regmap_t	*regs;
{
	register int i;

	/* Set overlay color registers */
	bt478_overlay(regs, plane);

	/* loadup vdac map */
#	define mvv(i,v)	{			\
	regs->addr_mapwa = i; wbflush();	\
	regs->addr_map = v; wbflush();		\
	regs->addr_map = v; wbflush();		\
	regs->addr_map = v; wbflush();}

	for (i = 0; i < 128; i++) mvv(i,0x00);
	for (i = i; i < 256; i++) mvv(i,0xff);

}

bt478_init_color_map( regs, plane)
	bt478_padded_regmap_t	*regs;
{
	register int    i;

	bt478_overlay(regs, plane);

	mvv(0,0);
	mvv(1,0xff);
	mvv(255,0xff);

#	undef mvv
}

