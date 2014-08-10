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
 *	File: sfb_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	11/92
 *
 *	Driver for the Smart Color Frame Buffer Display,
 *	hardware-level operations.
 */

#include <sfb.h>
#if	(NSFB > 0)
#include <platforms.h>

#include <machine/machspl.h>
#include <mach/std_types.h>
#include <chips/busses.h>
#include <chips/screen_defs.h>
#include <chips/pm_defs.h>
#include <machine/machspl.h>

typedef pm_softc_t	sfb_softc_t;

#ifdef	DECSTATION
#include <mips/PMAX/pmagb_ba.h>
#include <mips/PMAX/tc.h>
#endif

#ifdef	FLAMINGO
#include <mips/PMAX/pmagb_ba.h>		/* XXXX fixme */
#include <alpha/DEC/tc.h>
#define sparsify(x)	((1L << 28) | (((x) & 0x7ffffff) << 1) | \
			 ((x) & ~0x7ffffffL))
#endif

#ifndef	sparsify
#define sparsify(x) x
#endif

/*
 * Definition of the driver for the auto-configuration program.
 */

int	sfb_probe(), sfb_intr();
void	sfb_attach();

vm_offset_t	sfb_std[NSFB] = { 0 };
struct	bus_device *sfb_info[NSFB];
struct	bus_driver sfb_driver = 
        { sfb_probe, 0, sfb_attach, 0, sfb_std, "sfb", sfb_info,
	  0, 0, BUS_INTR_DISABLED};

/*
 * Probe/Attach functions
 */

sfb_probe(
	vm_offset_t	addr,
	struct bus_device *device)
{
	static probed_once = 0;

	/*
	 * Probing was really done sweeping the TC long ago
	 */
	if (tc_probe("sfb") == 0)
		return 0;
	if (probed_once++ > 1) {
		printf("[mappable] ");
		device->address = addr;
	}
	return 1;
}

void sfb_attach(
	struct bus_device *ui)
{
	/* ... */
	printf(": smart frame buffer");
}


/*
 * Interrupt routine
 */

sfb_intr(
	int	unit,
	spl_t	spllevel)
{
	register volatile char *ack;

	/* acknowledge interrupt */
	ack = (volatile char *) sfb_info[unit]->address + SFB_OFFSET_ICLR;
	*ack = 0;

#if	mips
	splx(spllevel);
#endif
	lk201_led(unit);
}

sfb_vretrace(
	sfb_softc_t	*sfb,
	boolean_t	on)
{
	sfb_regs	*regs;

	regs = (sfb_regs *) ((char *)sfb->framebuffer - SFB_OFFSET_VRAM + SFB_OFFSET_REGS);

	regs->intr_enable = (on) ? 1 : 0;
}

/*
 * Boot time initialization: must make device
 * usable as console asap.
 */
#define sfb_set_status			cfb_set_status

extern int
	sfb_soft_reset(), sfb_set_status(),
	sfb_pos_cursor(), bt459_video_on(),
	bt459_video_off(), sfb_vretrace(),
	pm_get_status(), pm_char_paint(),
	pm_insert_line(), pm_remove_line(),
	pm_clear_bitmap(), pm_map_page();

static struct screen_switch sfb_sw = {
	screen_noop,		/* graphic_open */
	sfb_soft_reset,		/* graphic_close */
	sfb_set_status,		/* set_status */
	pm_get_status,		/* get_status */
	pm_char_paint,		/* char_paint */
	sfb_pos_cursor,		/* pos_cursor */
	pm_insert_line,		/* insert_line */
	pm_remove_line,		/* remove_line */
	pm_clear_bitmap,	/* clear_bitmap */
	bt459_video_on,		/* video_on */
	bt459_video_off,	/* video_off */
	sfb_vretrace,		/* intr_enable */
	pm_map_page		/* map_page */
};

sfb_cold_init(
	int		unit,
	user_info_t	*up)
{
	sfb_softc_t	*sfb;
	screen_softc_t	sc = screen(unit);
	vm_offset_t	base = tc_probe("sfb");
	int		hor_p, ver_p;
	boolean_t	makes_sense;

	bcopy(&sfb_sw, &sc->sw, sizeof(sc->sw));
	sc->flags |= COLOR_SCREEN;

	/*
	 * I am confused here by the documentation.  One document
	 * sez there are three boards:
	 *	"PMAGB-BA" can do 1280x1024 @66Hz or @72Hz
	 *	"PMAGB-BC" can do 1024x864  @60Hz or 1280x1024 @72Hz
	 *	"PMAGB-BE" can do 1024x768  @72Hz or 1280x1024 @72Hz
	 * Another document sez things differently:
	 *	"PMAGB-BB" can do 1024x768  @72Hz
	 *	"PMAGB-BD" can do 1024x864  @60Hz or 1280x1024 @72Hz
	 *
	 * I would be inclined to believe the first one, which came
	 * with an actual piece of hardware attached (a PMAGB-BA).
	 * But I could swear I got a first board (which blew up
	 * instantly) and it was calling itself PMAGB-BB...
	 *
	 * Since I have not seen any other hardware I will make
	 * this code as hypothetical as I can.  Should work :-))
	 */

	makes_sense = FALSE;

	{
		sfb_regs	*regs;

		regs = (sfb_regs *) ((char *)base + SFB_OFFSET_REGS);
		hor_p = (regs->vhor_setup & 0x1ff) * 4;
		ver_p = regs->vvert_setup & 0x7ff;

		if (((hor_p == 1280) && (ver_p == 1024)) ||
		    ((hor_p == 1024) && (ver_p == 864)) ||
		    ((hor_p == 1024) && (ver_p == 768)))
			makes_sense = TRUE;
	}	

	if (makes_sense) {
		sc->frame_scanline_width = hor_p;
		sc->frame_height = ver_p;
		sc->frame_visible_width = hor_p;
		sc->frame_visible_height = ver_p;
	} else {
		sc->frame_scanline_width = 1280;
		sc->frame_height = 1024;
		sc->frame_visible_width = 1280;
		sc->frame_visible_height = 1024;
	}

	pm_init_screen_params(sc,up);
	(void) screen_up(unit, up);

	sfb = pm_alloc( unit, sparsify(base + SFB_OFFSET_BT459),
			base + SFB_OFFSET_VRAM, -1);

	screen_default_colors(up);

	sfb_soft_reset(sc);

	/*
	 * Clearing the screen at boot saves from scrolling
	 * much, and speeds up booting quite a bit.
	 */
	screen_blitc( unit, 'C'-'@');/* clear screen */
}

#if	0	/* this is how you find out about a new screen */
fill(addr,n,c)
	char *addr;
{
	while (n-- > 0) *addr++ = c;
}
#endif


#endif	(NSFB > 0)
