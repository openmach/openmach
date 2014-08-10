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
 *	File: ims332.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Routines for the Inmos IMS-G332 Colour video controller
 */

#include <platforms.h>

#include <chips/ims332.h>
#include <chips/screen.h>

#include <chips/xcfb_monitor.h>

/*
 * Generic register access
 */
typedef volatile unsigned char *ims332_padded_regmap_t;

#ifdef	MAXINE

unsigned int
ims332_read_register(regs, regno)
	unsigned char	*regs;
{
	unsigned char		*rptr;
	register unsigned int	val, v1;

	/* spec sez: */
	rptr = regs + 0x80000 + (regno << 4);
	val = * ((volatile unsigned short *) rptr );
	v1  = * ((volatile unsigned short *) regs );

	return (val & 0xffff) | ((v1 & 0xff00) << 8);
}

ims332_write_register(regs, regno, val)
	unsigned char		*regs;
	register unsigned int	val;
{
	unsigned char		*wptr;

	/* spec sez: */
	wptr = regs + 0xa0000 + (regno << 4);
	* ((volatile unsigned int *)(regs))   = (val >> 8) & 0xff00;
	* ((volatile unsigned short *)(wptr)) = val;
}

#define	assert_ims332_reset_bit(r)	*r &= ~0x40
#define	deassert_ims332_reset_bit(r)	*r |=  0x40

#else	/*MAXINE*/

#define	ims332_read_register(p,r)			\
		((unsigned int *)(p)) [ (r) ]
#define	ims332_write_register(p,r,v)			\
		((unsigned int *)(p)) [ (r) ] = (v)

#endif	/*MAXINE*/


/*
 * Color map
 */
ims332_load_colormap( regs, map)
	ims332_padded_regmap_t	*regs;
	color_map_t	*map;
{
	register int    i;

	for (i = 0; i < 256; i++, map++)
		ims332_load_colormap_entry(regs, i, map);
}

ims332_load_colormap_entry( regs, entry, map)
	ims332_padded_regmap_t	*regs;
	color_map_t	*map;
{
	/* ?? stop VTG */
	ims332_write_register(regs, IMS332_REG_LUT_BASE + (entry & 0xff),
			      (map->blue << 16) |
			      (map->green << 8) |
			      (map->red));
}

ims332_init_colormap( regs)
	ims332_padded_regmap_t	*regs;
{
	color_map_t		m;

	m.red = m.green = m.blue = 0;
	ims332_load_colormap_entry( regs, 0, &m);

	m.red = m.green = m.blue = 0xff;
	ims332_load_colormap_entry( regs, 1, &m);
	ims332_load_colormap_entry( regs, 255, &m);

	/* since we are at it, also fix cursor LUT */
	ims332_load_colormap_entry( regs, IMS332_REG_CURSOR_LUT_0, &m);
	ims332_load_colormap_entry( regs, IMS332_REG_CURSOR_LUT_1, &m);
	/* *we* do not use this, but the prom does */
	ims332_load_colormap_entry( regs, IMS332_REG_CURSOR_LUT_2, &m);
}

#if	1/*debug*/
ims332_print_colormap( regs)
	ims332_padded_regmap_t	*regs;
{
	register int    i;

	for (i = 0; i < 256; i++) {
		register unsigned int	color;

		color = ims332_read_register( regs, IMS332_REG_LUT_BASE + i);
		printf("%x->[x%x x%x x%x]\n", i,
			(color >> 16) & 0xff,
			(color >> 8) & 0xff,
			color & 0xff);
	}
}
#endif

/*
 * Video on/off
 *
 * It is unfortunate that X11 goes backward with white@0
 * and black@1.  So we must stash away the zero-th entry
 * and fix it while screen is off.  Also must remember
 * it, sigh.
 */
struct vstate {
	ims332_padded_regmap_t	*regs;
	unsigned short	off;
};

ims332_video_off(vstate, up)
	struct vstate	*vstate;
	user_info_t	*up;
{
	register ims332_padded_regmap_t	*regs = vstate->regs;
	register unsigned		*save, csr;

	if (vstate->off)
		return;

	/* Yes, this is awful */
	save = (unsigned *)up->dev_dep_2.gx.colormap;

	*save = ims332_read_register(regs, IMS332_REG_LUT_BASE);

	ims332_write_register(regs, IMS332_REG_LUT_BASE, 0);

	ims332_write_register( regs, IMS332_REG_COLOR_MASK, 0);

	/* cursor now */
	csr = ims332_read_register(regs, IMS332_REG_CSR_A);
	csr |= IMS332_CSR_A_DISABLE_CURSOR;
	ims332_write_register(regs, IMS332_REG_CSR_A, csr);

	vstate->off = 1;
}

ims332_video_on(vstate, up)
	struct vstate	*vstate;
	user_info_t	*up;
{
	register ims332_padded_regmap_t	*regs = vstate->regs;
	register unsigned		*save, csr;

	if (!vstate->off)
		return;

	/* Like I said.. */
	save = (unsigned *)up->dev_dep_2.gx.colormap;

	ims332_write_register(regs, IMS332_REG_LUT_BASE, *save);

	ims332_write_register( regs, IMS332_REG_COLOR_MASK, 0xffffffff);

	/* cursor now */
	csr = ims332_read_register(regs, IMS332_REG_CSR_A);
	csr &= ~IMS332_CSR_A_DISABLE_CURSOR;
	ims332_write_register(regs, IMS332_REG_CSR_A, csr);

	vstate->off = 0;
}

/*
 * Cursor
 */
ims332_pos_cursor(regs,x,y)
	ims332_padded_regmap_t	*regs;
	register int	x,y;
{
	ims332_write_register( regs, IMS332_REG_CURSOR_LOC,
		((x & 0xfff) << 12) | (y & 0xfff) );
}


ims332_cursor_color( regs, color)
	ims332_padded_regmap_t	*regs;
	color_map_t	*color;
{
	/* Bg is color[0], Fg is color[1] */
	ims332_write_register(regs, IMS332_REG_CURSOR_LUT_0,
			      (color->blue << 16) |
			      (color->green << 8) |
			      (color->red));
	color++;
	ims332_write_register(regs, IMS332_REG_CURSOR_LUT_1,
			      (color->blue << 16) |
			      (color->green << 8) |
			      (color->red));
}

ims332_cursor_sprite( regs, cursor)
	ims332_padded_regmap_t	*regs;
	unsigned short		*cursor;
{
	register int i;

	/* We *could* cut this down a lot... */
	for (i = 0; i < 512; i++, cursor++)
		ims332_write_register( regs,
			IMS332_REG_CURSOR_RAM+i, *cursor);
}

/*
 * Initialization
 */
ims332_init(regs, reset, mon)
	ims332_padded_regmap_t	*regs;
	unsigned int		*reset;
	xcfb_monitor_type_t	mon;
{
	int shortdisplay, broadpulse, frontporch;

	assert_ims332_reset_bit(reset);
	delay(1);	/* specs sez 50ns.. */
	deassert_ims332_reset_bit(reset);

	/* CLOCKIN appears to receive a 6.25 Mhz clock --> PLL 12 for 75Mhz monitor */
	ims332_write_register(regs, IMS332_REG_BOOT, 12 | IMS332_BOOT_CLOCK_PLL);

	/* initialize VTG */
	ims332_write_register(regs, IMS332_REG_CSR_A,
				IMS332_BPP_8 | IMS332_CSR_A_DISABLE_CURSOR);
	delay(50);	/* spec does not say */

	/* datapath registers (values taken from prom's settings) */

	frontporch = mon->line_time - (mon->half_sync * 2 +
				       mon->back_porch +
				       mon->frame_visible_width / 4);

	shortdisplay = mon->line_time / 2 - (mon->half_sync * 2 +
					     mon->back_porch + frontporch);
	broadpulse = mon->line_time / 2 - frontporch;

	ims332_write_register( regs, IMS332_REG_HALF_SYNCH,     mon->half_sync);
	ims332_write_register( regs, IMS332_REG_BACK_PORCH,     mon->back_porch);
	ims332_write_register( regs, IMS332_REG_DISPLAY,
			      mon->frame_visible_width / 4);
	ims332_write_register( regs, IMS332_REG_SHORT_DIS,	shortdisplay);
	ims332_write_register( regs, IMS332_REG_BROAD_PULSE,	broadpulse);
	ims332_write_register( regs, IMS332_REG_V_SYNC,		mon->v_sync * 2);
	ims332_write_register( regs, IMS332_REG_V_PRE_EQUALIZE,
			      mon->v_pre_equalize);
	ims332_write_register( regs, IMS332_REG_V_POST_EQUALIZE,
			      mon->v_post_equalize);
	ims332_write_register( regs, IMS332_REG_V_BLANK,	mon->v_blank * 2);
	ims332_write_register( regs, IMS332_REG_V_DISPLAY,
			      mon->frame_visible_height * 2);
	ims332_write_register( regs, IMS332_REG_LINE_TIME,	mon->line_time);
	ims332_write_register( regs, IMS332_REG_LINE_START,	mon->line_start);
	ims332_write_register( regs, IMS332_REG_MEM_INIT, 	mon->mem_init);
	ims332_write_register( regs, IMS332_REG_XFER_DELAY,	mon->xfer_delay);

	ims332_write_register( regs, IMS332_REG_COLOR_MASK, 0xffffff);

	ims332_init_colormap( regs );

	ims332_write_register(regs, IMS332_REG_CSR_A,
		IMS332_BPP_8 | IMS332_CSR_A_DMA_DISABLE | IMS332_CSR_A_VTG_ENABLE);

}
