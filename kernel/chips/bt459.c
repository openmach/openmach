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
 *	File: bt459.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Routines for the bt459 RAMDAC	
 */

#include <platforms.h>

#include <chips/bt459.h>
#include <chips/screen.h>

#ifdef	DECSTATION

typedef struct {
	volatile unsigned char	addr_lo;
	char						pad0[3];
	volatile unsigned char	addr_hi;
	char						pad1[3];
	volatile unsigned char	addr_reg;
	char						pad2[3];
	volatile unsigned char	addr_cmap;
	char						pad3[3];
} bt459_ds_padded_regmap_t;
#define	bt459_padded_regmap_t	bt459_ds_padded_regmap_t

#define	mb()	/* no write/read reordering problems */

#endif	/* DECSTATION */

#ifdef	FLAMINGO

/* Sparse space ! */
typedef struct {
	volatile unsigned int	addr_lo;
	int						pad0;
	volatile unsigned int	addr_hi;
	int						pad1;
	volatile unsigned int	addr_reg;
	int						pad2;
	volatile unsigned int	addr_cmap;
	int						pad3;
} bt459_fl_padded_regmap_t;
#define	bt459_padded_regmap_t	bt459_fl_padded_regmap_t

#define mb()	wbflush()

#endif	/* FLAMINGO */


#ifndef bt459_padded_regmap_t
typedef bt459_regmap_t	bt459_padded_regmap_t;
#define	wbflush()
#endif

/*
 * Generic register access
 */
#define bt459_select_reg_macro(r,n)		\
	(r)->addr_lo = (n); mb();		\
	(r)->addr_hi = (n) >> 8;		\
	wbflush();

void
bt459_select_reg(
	bt459_padded_regmap_t	*regs,
	int			regno)
{
	bt459_select_reg_macro( regs, regno);
}

void 
bt459_write_reg(
	bt459_padded_regmap_t	*regs,
	int			regno,
	unsigned char		val)
{
	bt459_select_reg_macro( regs, regno );
	regs->addr_reg = val;
	wbflush();
}

unsigned char
bt459_read_reg(
	bt459_padded_regmap_t	*regs,
	int			regno)
{
	bt459_select_reg_macro( regs, regno );
	return regs->addr_reg;
}


/*
 * Color map
 */
bt459_load_colormap_entry(
	bt459_padded_regmap_t	*regs,
	int			entry,
	color_map_t		*map)
{
	bt459_select_reg(regs, entry & 0xff);

	regs->addr_cmap = map->red;
	wbflush();
	regs->addr_cmap = map->green;
	wbflush();
	regs->addr_cmap = map->blue;
	wbflush();
}

bt459_init_colormap(
	bt459_padded_regmap_t	*regs)
{
	register int    i;

	bt459_select_reg(regs, 0);
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();

	regs->addr_cmap = 0xff;
	wbflush();
	regs->addr_cmap = 0xff;
	wbflush();
	regs->addr_cmap = 0xff;
	wbflush();

	bt459_select_reg(regs, 255);
	regs->addr_cmap = 0xff;
	wbflush();
	regs->addr_cmap = 0xff;
	wbflush();
	regs->addr_cmap = 0xff;
	wbflush();

}

#if	1/*debug*/
bt459_print_colormap(
	bt459_padded_regmap_t	*regs)
{
	register int    i;

	for (i = 0; i < 256; i++) {
		register unsigned char red, green, blue;

		bt459_select_reg(regs, i);
		red   = regs->addr_cmap; wbflush();
		green = regs->addr_cmap; wbflush();
		blue  = regs->addr_cmap; wbflush();
		printf("%x->[x%x x%x x%x]\n", i,
			red, green, blue);

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
	bt459_padded_regmap_t	*regs;
	unsigned short	off;
};

bt459_video_off(
	struct vstate	*vstate,
	user_info_t	*up)
{
	register bt459_padded_regmap_t	*regs = vstate->regs;
	unsigned char		*save;

	if (vstate->off)
		return;

	/* Yes, this is awful */
	save = (unsigned char *)up->dev_dep_2.gx.colormap;

	bt459_select_reg(regs, 0);
	*save++ = regs->addr_cmap;
	*save++ = regs->addr_cmap;
	*save++ = regs->addr_cmap;

	bt459_select_reg(regs, 0);
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();
	regs->addr_cmap = 0;
	wbflush();

	bt459_write_reg( regs, BT459_REG_PRM, 0);
	bt459_write_reg( regs, BT459_REG_CCR, 0);

	vstate->off = 1;
}

bt459_video_on(
	struct vstate	*vstate,
	user_info_t	*up)
{
	register bt459_padded_regmap_t	*regs = vstate->regs;
	unsigned char		*save;

	if (!vstate->off)
		return;

	/* Like I said.. */
	save = (unsigned char *)up->dev_dep_2.gx.colormap;

	bt459_select_reg(regs, 0);
	regs->addr_cmap = *save++;
	wbflush();
	regs->addr_cmap = *save++;
	wbflush();
	regs->addr_cmap = *save++;
	wbflush();

	bt459_write_reg( regs, BT459_REG_PRM, 0xff);
	bt459_write_reg( regs, BT459_REG_CCR, 0xc0);

	vstate->off = 0;
}

/*
 * Cursor
 */
bt459_pos_cursor(
	bt459_padded_regmap_t	*regs,
	register int		x,
	register int		y)
{
#define lo(v)	((v)&0xff)
#define hi(v)	(((v)&0xf00)>>8)
	bt459_write_reg( regs, BT459_REG_CXLO, lo(x + 219));
	bt459_write_reg( regs, BT459_REG_CXHI, hi(x + 219));
	bt459_write_reg( regs, BT459_REG_CYLO, lo(y + 34));
	bt459_write_reg( regs, BT459_REG_CYHI, hi(y + 34));
}


bt459_cursor_color(
	bt459_padded_regmap_t	*regs,
	color_map_t		*color)
{
	register int    i;

	bt459_select_reg_macro( regs, BT459_REG_CCOLOR_2);
	for (i = 0; i < 2; i++) {
		regs->addr_reg = color->red;
		wbflush();
		regs->addr_reg = color->green;
		wbflush();
		regs->addr_reg = color->blue;
		wbflush();
		color++;
	}
}

bt459_cursor_sprite(
	bt459_padded_regmap_t	*regs,
	unsigned char		*cursor)
{
	register int i, j;

	/*
	 * As per specs, must run a check to see if we
	 * had contention. If so, re-write the cursor.
	 */
	for (i = 0, j = 0; j < 2; j++) {
	    /* loop once to write */
	    for ( ; i < 1024; i++)
		bt459_write_reg( regs, BT459_REG_CRAM_BASE+i, cursor[i]);

	    /* loop to check, if fail write again */
	    for (i = 0; i < 1024; i++)
		if (bt459_read_reg( regs, BT459_REG_CRAM_BASE+i) != cursor[i])
			break;
	    if (i == 1024)
	    	break;/* all is well now */
	}
}

/*
 * Initialization
 */
bt459_init(
	bt459_padded_regmap_t	*regs,
	volatile char		*reset,
	int			mux)
{
	if (bt459_read_reg(regs, BT459_REG_ID) != 0x4a)
		panic("bt459");

	if (mux == 4) {
		/* use 4:1 input mux */
		bt459_write_reg( regs, BT459_REG_CMD0, 0x40);
	} else if (mux == 5) {
		/* use 5:1 input mux */
		bt459_write_reg( regs, BT459_REG_CMD0, 0xc0);
	} /* else donno */

	*reset = 0;	/* force chip reset */

	/* no zooming, no panning */
	bt459_write_reg( regs, BT459_REG_CMD1, 0x00);

	/* signature test, X-windows cursor, no overlays, SYNC* PLL,
	   normal RAM select, 7.5 IRE pedestal, do sync */
	bt459_write_reg( regs, BT459_REG_CMD2, 0xc2);

	/* get all pixel bits */	
	bt459_write_reg( regs, BT459_REG_PRM,  0xff);

	/* no blinking */
	bt459_write_reg( regs, BT459_REG_PBM,  0x00);

	/* no overlay */
	bt459_write_reg( regs, BT459_REG_ORM,  0x00);

	/* no overlay blink */
	bt459_write_reg( regs, BT459_REG_OBM,  0x00);

	/* no interleave, no underlay */
	bt459_write_reg( regs, BT459_REG_ILV,  0x00);

	/* normal operation, no signature analysis */
	bt459_write_reg( regs, BT459_REG_TEST, 0x00);

	/* no blinking, 1bit cross hair, XOR reg&crosshair,
	   no crosshair on either plane 0 or 1,
	   regular cursor on both planes */
	bt459_write_reg( regs, BT459_REG_CCR,  0xc0);

	/* home cursor */
	bt459_write_reg( regs, BT459_REG_CXLO, 0x00);
	bt459_write_reg( regs, BT459_REG_CXHI, 0x00);
	bt459_write_reg( regs, BT459_REG_CYLO, 0x00);
	bt459_write_reg( regs, BT459_REG_CYHI, 0x00);

	/* no crosshair window */
	bt459_write_reg( regs, BT459_REG_WXLO, 0x00);
	bt459_write_reg( regs, BT459_REG_WXHI, 0x00);
	bt459_write_reg( regs, BT459_REG_WYLO, 0x00);
	bt459_write_reg( regs, BT459_REG_WYHI, 0x00);
	bt459_write_reg( regs, BT459_REG_WWLO, 0x00);
	bt459_write_reg( regs, BT459_REG_WWHI, 0x00);
	bt459_write_reg( regs, BT459_REG_WHLO, 0x00);
	bt459_write_reg( regs, BT459_REG_WHHI, 0x00);
}
