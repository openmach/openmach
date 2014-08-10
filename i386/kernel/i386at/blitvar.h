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
/* **********************************************************************
 File:         blitvar.h
 Description:  Definitions used by Blit driver other than h/w definition.

 $ Header: $

 Copyright Ing. C. Olivetti & C. S.p.A. 1988, 1989.
 All rights reserved.
********************************************************************** */
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <i386at/blitreg.h>
#include <sys/types.h>
#include <mach/boolean.h>


/* 
 * This is how we use the Blit's graphics memory.  The frame buffer 
 * goes at the front, and the rest is used for miscellaneous 
 * allocations.  Users can use the "spare" memory, but they should do 
 * an ioctl to find out which part of the memory is really free.
 */

struct blitmem {
	union blitfb {
		u_char mono_fb[BLIT_MONOFBSIZE];
		u_char color_fb[1];	/* place-holder */
	} fb;
	u_char spare[BLIT_MEMSIZE - sizeof(union blitfb)];
};


/*
 * Macro to get from blitdev pointer to monochrome framebuffer.
 */
#define       BLIT_MONOFB(blt, fbptr) \
	{ struct blitmem *mymem = (struct blitmem *)((blt)->graphmem); \
	fbptr = mymem->fb.mono_fb; \
	}


/* 
 * Single-tile description that can be used to describe the entire 
 * screen. 
 */

struct screen_descrip {
	STRIPHEADER strip;
	TILEDESC tile;
};


/* 
 * Number of microseconds we're willing to wait for display processor 
 * to load its command block.
 */

#define DP_RDYTIMEOUT			1000000


/* 
 * Conversion macros.
 */

#define VM_TO_ADDR786(vmaddr, blit_base) \
	((int)(vmaddr) - (int)(blit_base))


extern boolean_t blit_present();
extern void blit_init();
