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
 *	File: pm_misc.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Driver for the VFB01/02 Mono/Color framebuffer (pmax)
 *	Hardware-independent operations, mostly shared with
 *	the CFB driver (see each individual function header),
 *	and possibly others.
 */


#include <platforms.h>

#include <fb.h>

#if	defined(DECSTATION) || defined(FLAMINGO)
#include <cfb.h>
#include <mfb.h>
#include <xcfb.h>
#include <sfb.h>
#define	NPM	(NFB+NCFB+NMFB+NXCFB+NSFB)
#endif	/*DECSTATION*/

#ifdef	VAXSTATION
#define	NPM	(NFB)
#endif	/*VAXSTATION*/


#if	(NPM > 0)

#include <mach/vm_param.h>		/* PAGE_SIZE */
#include <device/device_types.h>
#include <vm/vm_map.h>			/* kernel_pmap */

#include <chips/screen_defs.h>
#include <chips/pm_defs.h>


#ifdef	DECSTATION
#define	machine_btop	mips_btop
#define MONO_BM		(256*1024)
#endif	/*DECSTATION*/

#ifdef	VAXSTATION
#define	machine_btop	vax_btop
/*
	For now we use the last page of the frame for
	the user_info structure.
*/
#define MONO_BM		(256*1024-PAGE_SIZE)
#endif	/*VAXSTATION*/

#ifdef	FLAMINGO
#define	machine_btop	alpha_btop
#define MONO_BM		(256*1024)
#define	LOG2_SIZEOF_LONG	3	/* 64bit archies */
#endif	/* FLAMINGO */

#ifndef	LOG2_SIZEOF_LONG
#define	LOG2_SIZEOF_LONG	2	/* 32bit archies */
#endif


/* Hardware state */
pm_softc_t	pm_softc_data[NPM];

pm_softc_t*
pm_alloc(
	int		unit,
	char		*cur,
	unsigned char	*fb,
	unsigned char	*pl)
{
	pm_softc_t     *pm = &pm_softc_data[unit];

	pm->cursor_registers	= cur;
	pm->framebuffer 	= fb;
	pm->plane_mask 		= pl;
	pm->vdac_registers	= 0;	/* later, if ever */

	screen_attach(unit, (char *) pm);

	return pm;
}


/*
 * Routine to paint a char on a simple framebuffer.
 * This is common to the pm, fb and cfb drivers.
 */
pm_char_paint(
	screen_softc_t	sc,
	int		c,
	int		row,
	int		col)
{
	register int	incr;
	int		line_size;
	register unsigned char	*font, *bmap;
	pm_softc_t	*pm = (pm_softc_t*)sc->hw_state;

	/*
	 * Here are the magic numbers that drive the loops below:
	 *	incr		bytes between scanlines of the glyph
	 *	line_size	bytes in a row, using the system font
	 *
	 * This code has been optimized to avoid multiplications,
	 * and is therefore much less obvious than it could be.
	 */
	if (sc->flags & MONO_SCREEN) {
		/*
		 * B&W screen: 1 bit/pixel
		 *	incr --> 1 * BytesPerLine, with possible stride
		 */
		incr = sc->frame_scanline_width >> 3;
	} else {
		/*
		 * Color screen: 8 bits/pixel
		 *	incr --> 8 * BytesPerLine, with possible stride
		 */
		incr = sc->frame_scanline_width;
		col <<= 3;
	}

	/* not all compilers are smart about multiply by 15 */
#if	(KfontHeight==15)
#	define TIMES_KfontHeight(w)	(((w)<<4)-(w))
#else
#	define TIMES_KfontHeight(w)	((w)*KfontHeight)
#endif
	line_size = TIMES_KfontHeight(incr);

	bmap = pm->framebuffer + col + (row * line_size);
	font = &kfont_7x14[ (int)(c - ' ') * 15];
	if (sc->flags & MONO_SCREEN) {
		/*
		 * Unroll simple loops, take note of common cases
		 */
		if (sc->standout) {
#			define mv()	*bmap = ~*font++; bmap += incr;
			mv();mv();mv();mv();mv();mv();mv();mv();
			mv();mv();mv();mv();mv();mv();mv();
#			undef	mv
		} else if (c == ' ') {
#			define mv()	*bmap = 0; bmap += incr;
			mv();mv();mv();mv();mv();mv();mv();mv();
			mv();mv();mv();mv();mv();mv();mv();
#			undef	mv
		} else {
#			define mv()	*bmap = *font++; bmap += incr;
			mv();mv();mv();mv();mv();mv();mv();mv();
			mv();mv();mv();mv();mv();mv();mv();
#			undef	mv
		}
	} else {
		/*
		 * 8 bits per pixel --> paint one byte per each font bit.
		 * In order to spread out the 8 bits of a glyph line over
		 * the 64 bits per scanline use a simple vector multiply,
		 * taking 4 bits at a time to get the two resulting words
		 */
		static unsigned int spread[16] = {
			0x00000000, 0x00000001, 0x00000100, 0x00000101,
			0x00010000, 0x00010001, 0x00010100, 0x00010101,
			0x01000000, 0x01000001, 0x01000100, 0x01000101,
			0x01010000, 0x01010001, 0x01010100, 0x01010101,
		};
		register int	rev_video = sc->standout;
		register int	j;
		for (j = 0; j < KfontHeight; j++) {
			register unsigned char c = *font++;
			if (rev_video) c = ~c;
#if	(LOG2_SIZEOF_LONG==3)
			*((long*)bmap) =  (long)spread[ c & 0xf ] |
					((long)(spread[ (c>>4) & 0xf ]) << 32);
#else
			((int*)bmap)[0] = spread[ c & 0xf ];
			((int*)bmap)[1] = spread[ (c>>4) & 0xf ];
#endif
			bmap += incr;
		}
	}
}

/*
 * Delete the line at the given row.
 * This is common to the pm, fb and cfb drivers.
 */
pm_remove_line(
	screen_softc_t	sc,
	short		row)
{
	register long	*dest, *src;
	register long	*end;
	register long	temp0,temp1,temp2,temp3;
	register long	i, scaninc, blockcnt;
	long		line_size, incr;
	unsigned char	*framebuffer;
	pm_softc_t	*pm = (pm_softc_t*)sc->hw_state;
	long		CharRows, CharCols;

	CharRows = sc->up->max_row;
	CharCols = sc->up->max_col;
	framebuffer = pm->framebuffer;

	/* Inner loop works 4 long words at a time (writebuffer deep) */
#	define BlockSizeShift	(2+LOG2_SIZEOF_LONG)

	/*  To copy one (MONO) line, we need to iterate this many times */
#	define Blocks    (CharCols>>BlockSizeShift)

	/*  Skip this many bytes to get to the next line */
#	define Slop(w)   ((w) - (blockcnt<<BlockSizeShift))

	if (sc->flags & MONO_SCREEN) {
		blockcnt = Blocks;
		/* See comments in pm_char_paint() */
		incr = sc->frame_scanline_width >> 3;
	} else {
		blockcnt = Blocks << 3;
		/* See comments in pm_char_paint() */
		incr = sc->frame_scanline_width;
	}
	line_size = TIMES_KfontHeight(incr);

	scaninc = (Slop(incr)) >> LOG2_SIZEOF_LONG;	/* pointers are long* */

	dest = (long *)(framebuffer + row * line_size);
	src  = (long *)((char*)dest + line_size);
	end  = (long *)(framebuffer + CharRows * line_size);
	while (src < end) {
		i = 0;
		do {
			temp0 = src[0];
			temp1 = src[1];
			temp2 = src[2];
			temp3 = src[3];
			dest[0] = temp0;
			dest[1] = temp1;
			dest[2] = temp2;
			dest[3] = temp3;
			dest += 4;
			src += 4;
			i++;
		} while (i < blockcnt);
		src += scaninc;
		dest += scaninc;
	}

	/* Now zero out the last line */
	bzero(framebuffer + (CharRows - 1) * line_size,  line_size);

	ascii_screen_rem_update(sc, row);
}


/*
 * Open a new blank line at the given row.
 * This is common to the pm, fb and cfb drivers.
 */
pm_insert_line(
	screen_softc_t	sc,
	short		row)
{
	register long	*dest, *src;
	register long	*end;
	register long	temp0,temp1,temp2,temp3;
	register long	i, scaninc, blockcnt;
	long		line_size, incr;
	unsigned char	*framebuffer;
	pm_softc_t	*pm = (pm_softc_t*)sc->hw_state;
	long		CharRows, CharCols;

	CharRows = sc->up->max_row;
	CharCols = sc->up->max_col;

	framebuffer = pm->framebuffer;

	/* See above for comments */
	if (sc->flags & MONO_SCREEN) {
		blockcnt = Blocks;
		/* See comments in pm_char_paint() */
		incr = sc->frame_scanline_width >> 3;
	} else {
		blockcnt = Blocks << 3;
		/* See comments in pm_char_paint() */
		incr = sc->frame_scanline_width;
	}
	line_size = TIMES_KfontHeight(incr);

	scaninc = Slop(incr) + ((2 * blockcnt) << BlockSizeShift);
	scaninc >>= LOG2_SIZEOF_LONG;		/* pointers are long* */
	dest = (long *)(framebuffer + (CharRows - 1) * line_size);
	src  = (long *)((char*)dest - line_size);
	end  = (long *)(framebuffer + row * line_size);
	while (src >= end) {
		i = 0;
		do {
			temp0 = src[0];
			temp1 = src[1];
			temp2 = src[2];
			temp3 = src[3];
			dest[0] = temp0;
			dest[1] = temp1;
			dest[2] = temp2;
			dest[3] = temp3;
			dest += 4;
			src += 4;
			i++;
		} while (i < blockcnt);
		src -= scaninc;
		dest -= scaninc;
	}

	/* Now zero out the line being opened */
	bzero(framebuffer + row * line_size, line_size);

	ascii_screen_ins_update(sc, row);
}

#undef	Slop


/*
 * Initialize screen parameters in the
 * user-mapped descriptor.
 * This is common to various drivers.
 */
pm_init_screen_params(
	screen_softc_t	sc,
	user_info_t	*up)
{
	register int	vis_x, vis_y;

	up->frame_scanline_width = sc->frame_scanline_width;
	up->frame_height = sc->frame_height;

	vis_x = sc->frame_visible_width;
	vis_y = sc->frame_visible_height;

	up->max_x		= vis_x;
	up->max_y		= vis_y;
	up->max_cur_x		= vis_x - 1;
	up->max_cur_y		= vis_y - 1;
	up->min_cur_x		= -15;
	up->min_cur_y		= -15;
	up->max_row		= vis_y / KfontHeight;
	up->max_col		= vis_x / KfontWidth;

	up->version		= 11;

	up->mouse_threshold	= 4;	
	up->mouse_scale		= 2;

	up->dev_dep_2.pm.tablet_scale_x	= ((vis_x - 1) * 1000) / 2200;
	up->dev_dep_2.pm.tablet_scale_y	= ((vis_y - 1) * 1000) / 2200;
}

/*
 * Clear the screen
 * Used by pm, fb and cfb
 */
pm_clear_bitmap(
	screen_softc_t	sc)
{
	pm_softc_t	*pm = (pm_softc_t *) sc->hw_state;
	unsigned int	screen_size;

	/* Do not touch the non visible part */
	screen_size = sc->frame_scanline_width * sc->frame_visible_height;
	blkclr((char *)pm->framebuffer,
		  (sc->flags & MONO_SCREEN) ? (screen_size>>3) : screen_size);

	/* clear ascii screenmap */
	ascii_screen_fill(sc, ' ');
}


/*
 * Size of the user-mapped structure
 * Used by both pm and cfb
 */
pm_mem_need()
{
	return USER_INFO_SIZE;
}

/*
 * Device-specific get status.
 * Used by fb and cfb also.
 */
pm_get_status(
	screen_softc_t	sc,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	*status_count)
{
	if (flavor == SCREEN_GET_OFFSETS) {
		unsigned	*offs = (unsigned *) status;

		offs[0] = PM_SIZE(sc);		/* virtual size */
		offs[1] = 0;			/* offset of user_info_t */
		*status_count = 2;
		return D_SUCCESS;
	} else
		return D_INVALID_OPERATION;
}

/*
 * Driver-specific set status
 * Only partially used by fb and cfb.
 */
pm_set_status(
	screen_softc_t	sc,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	status_count)
{
	switch (flavor) {
	case SCREEN_ADJ_MAPPED_INFO: {
		unsigned	user_addr = *(unsigned *) status;
		user_info_t	*up = sc->up;

		/* Make it point to the event_queue, in user virtual */
		up->evque.events = (screen_event_t *)(user_addr +
			((char*)up->event_queue - (char*)up));

		/* Make it point to the point_track, in user virtual */
		up->evque.track = (screen_timed_point_t *)(user_addr +
			((char*)up->point_track - (char*)up));

		up->dev_dep_1.pm.planemask = (unsigned char *)(user_addr + USER_INFO_SIZE);

		up->dev_dep_1.pm.bitmap = up->dev_dep_1.pm.planemask + PMASK_SIZE;

		break;
	}

	case SCREEN_LOAD_CURSOR: {

		sc->flags |= SCREEN_BEING_UPDATED;
		dc503_load_cursor(sc->hw_state, (unsigned short*)status);
		sc->flags &= ~SCREEN_BEING_UPDATED;

	    	break;
	}

#ifdef	DECSTATION
	case SCREEN_SET_CURSOR_COLOR: {
		pm_softc_t		*pm = (pm_softc_t*) sc->hw_state;

		sc->flags |= SCREEN_BEING_UPDATED;
		bt478_cursor_color (pm->vdac_registers, (cursor_color_t*) status);
		sc->flags &= ~SCREEN_BEING_UPDATED;

		break;
	}
	     
	case SCREEN_SET_CMAP_ENTRY: {
		pm_softc_t		*pm = (pm_softc_t*) sc->hw_state;
		color_map_entry_t	*e = (color_map_entry_t*) status;

		if (e->index < 256) {
			sc->flags |= SCREEN_BEING_UPDATED;
			bt478_load_colormap_entry( pm->vdac_registers, e->index, &e->value);
			sc->flags &= ~SCREEN_BEING_UPDATED;
		}

		break;
	}
#endif	/*DECSTATION*/
	default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

/*
 * Map pages to user space
 */
vm_offset_t pm_map_page_empty = (vm_offset_t) 0;

integer_t
pm_map_page(
	screen_softc_t	sc,
	vm_offset_t	off,
	int		prot)
{
	int		bitmapsize;
	integer_t	addr;
	pm_softc_t	*pm = (pm_softc_t *)sc->hw_state;
	extern vm_offset_t	pmap_extract( pmap_t map, vm_offset_t addr);

	bitmapsize = BITMAP_SIZE(sc);

#define	OFF0	USER_INFO_SIZE	 		/* user mapped info */
#define	OFF1	OFF0+PMASK_SIZE			/* plane mask register */
#define	OFF2	OFF1+bitmapsize			/* frame buffer mem */

	if (off < OFF0)
#ifdef	DECSTATION
		addr = kvtophys(sc->up);
#else
		addr = (integer_t) pmap_extract(kernel_pmap,
						(vm_offset_t)sc->up);
#endif
	else
	if (off < OFF1) {
#ifdef	VAXSTATION
		if (pm_map_page_empty == 0) {
			pm_map_page_empty = vm_page_grab_phys_addr();
		}
		addr = (integer_t)pmap_extract(kernel_pmap, pm_map_page_empty);
#else
		addr = (integer_t) pm->plane_mask;
#endif
		off -= OFF0;
	} else
	if (off < OFF2) {
#ifdef	DECSTATION
		addr = (integer_t)pm->framebuffer;
#else
		addr = (integer_t)pmap_extract(kernel_pmap,
					       (vm_offset_t)pm->framebuffer);
#endif
		off -= OFF1;
	} else 
		return D_INVALID_SIZE;	/* ??? */

	addr = machine_btop(addr + off);
	return (addr);
}


/*
 *-----------------------------------------------------------
 *	The rest of this file is stricly pmax/pvax-specific
 *-----------------------------------------------------------
 */
#if	(NFB > 0)

/*
 * Do what's needed when the X server exits
 */
pm_soft_reset(
	screen_softc_t	sc)
{
	pm_softc_t	*pm = (pm_softc_t*) sc->hw_state;
	user_info_t	*up = sc->up;

	/*
	 * Restore params in mapped structure
	 */
	pm_init_screen_params(sc, up);
	up->row = up->max_row - 2;
	dc503_init(pm);

#ifdef	DECSTATION
	if (sc->flags & MONO_SCREEN)
		bt478_init_bw_map(pm->vdac_registers, pm->plane_mask);
	else
		bt478_init_color_map(pm->vdac_registers, pm->plane_mask);
#endif	/*DECSTATION*/
}
#endif	/* NFB > 0 */


#endif	/* NPM > 0 */
