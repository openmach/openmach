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
 *	File: pm_defs.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Definitions specific to the "pm" simple framebuffer driver,
 *	exported across sub-modules.  Some other framebuffer drivers
 *	that share code with pm use these defs also.
 */

/* Hardware state (to be held in the screen descriptor) */

typedef struct {
	char		*cursor_registers;	/* opaque, for sharing */
	unsigned short	cursor_state;		/* some regs are W-only */
	short		unused;			/* padding, free */
	char		*vdac_registers;	/* opaque, for sharing */
	unsigned char	*framebuffer;
	unsigned char	*plane_mask;
} pm_softc_t;

extern pm_softc_t	*pm_alloc(/* unit, curs, framebuf, planem */);

/* user mapping sizes */
#define	USER_INFO_SIZE	PAGE_SIZE
#define	PMASK_SIZE	PAGE_SIZE
#define	BITMAP_SIZE(sc)						\
	((sc)->frame_height * (((sc)->flags & COLOR_SCREEN) ?	\
		sc->frame_scanline_width :			\
		sc->frame_scanline_width>>3))

#define	PM_SIZE(sc)	(USER_INFO_SIZE+PMASK_SIZE+BITMAP_SIZE(sc))
