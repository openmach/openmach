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
 File:         blit.c
 Description:  Device Driver for Bell Tech Blit card

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

/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifdef	MACH_KERNEL
#include <sys/types.h>
#include <device/errno.h>
#else	MACH_KERNEL
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/signal.h>
#include <sys/user.h>
#endif	MACH_KERNEL
#include <vm/vm_kern.h>
#include <mach/vm_param.h>
#include <machine/machspl.h>

#include <i386at/blitreg.h>
#include <i386at/blitvar.h>
#include <i386at/blituser.h>
#include <i386at/kd.h>
#include <i386at/kdsoft.h>

#include <blit.h>


/*
 * This driver really only supports 1 card, though parts of it were
 * written to support multiple cards.  If you want to finish the job
 * and really support multiple cards, then you'll have to:
 *
 * (1) make sure that driver functions pass around a pointer telling
 * which card they're talking about.
 *
 * (2) coordinate things with the kd driver, so that one card is used
 * for the console and the other is simply an additional display.
 */
#define MAXBLITS	1

#if	NBLIT > MAXBLITS 
/* oh, no, you don't want to do this...; */

#else
#if	NBLIT > 0

#define AUTOINIT	0

	/*
	 *	Forward Declarations
	 */
static tiledesc();
static loadall();

#if	AUTOINIT
int blitattach(), blitprobe();
#endif

int blitioctl(), blitopen(), blitclose(), blitmmap();


static void setstatus();
#define CARD_RESET		0
#define CARD_MAPPED		1
#define CARD_MAYBE_PRESENT	2
#define CARD_PRESENT		3
#define BIU_INIT		4
#define UNUSED1			5
#define DP_INIT			6
#define UNUSED2			7


#if	AUTOINIT
struct mb_device *blitinfo[NBLIT];

struct mb_driver blitdriver = {
	blitprobe,
	0,				/* slave routine */
	blitattach,
	0, 0, 0,			/* go, done, intr routines */
	BLIT_MAPPED_SIZE,
	"blit", blitinfo,		/* device info */
	0, 0,				/* no controller */
	0				/* no flags */
					/* rest zeros */
};
#endif	/* AUTOINIT */


/*
 * Per-card bookkeeping information for driver.
 * 
 * "scrstrip" and "dpctlregs" point to data areas that are passed to 
 * the Display Processor.  They are allocated out of the spare 
 * graphics memory.  "scrstrip" is used to describe an entire screen.  
 * "dpctlregs" contains assorted parameters for the display 
 * controller. 
 * 
 * "firstfree" is an offset into the graphics memory.  Memory starting 
 * there can be allocated by users.
 */

struct blitsoft {
	struct blitdev *blt;		/* ptr to mapped card */
	caddr_t physaddr;		/* start of mapped card */
	boolean_t open;			/* is device open? */
	struct screen_descrip *scrstrip;
	DPCONTROLBLK *dpctlregs;
	int firstfree;
} blitsoft[NBLIT];


/* 
 * The following array contains the initial settings for
 * the Display Processor Control Block Registers.
 * The video timing signals in this array are for the
 * Bell Technologies Blit Express running in 1664 x 1200 x 1 mode.
 * Please treat as read-only.
 */

DPCONTROLBLK blit_mparm = {
	DP_DSP_ON,			/* video status */
	0x00ff,				/* interrupt mask - all disabled */
	0x0010,				/* trip point */
	0x00ff,				/* frame interrupt interval */
	0x0000,				/* reserved */
	CRTM_NONINTER |	CRTM_SUPHIGH_SPEED, /* CRT controller mode */
	41,				/* horizontal synch stop */
	57,				/* horiz field start */
	265,				/* horiz field stop */
	265,				/* line length */
	15,				/* vert synch stop */
	43,				/* vert field start */
	1243,				/* vert field stop */
	1244,				/* frame length */
	0x0000, 0x0000,			/* descriptor pointer */
	0x0000,				/* reserved */
	0x0101,				/* x, y zoom factors */
	0x0000,				/* FldColor */
	0x00ff,				/* BdrColor */
	0x0000,				/* 1Bpp Pad */
	0x0000,				/* 2Bpp Pad */
	0x0000,				/* 4Bpp Pad */
	DP_CURSOR_CROSSHAIR,		/* cursor style & mode */
	0x00A0, 0x0050,			/* cursor x & y loc. */
	/* cursor pattern */
	0xfffe, 0xfffc, 0xc018, 0xc030, 0xc060, 0xc0c0, 0xc0c0, 0xc060, 
	0xc430, 0xce18, 0xdb0c, 0xf186, 0xe0c3, 0xc066, 0x803c, 0x0018 
};   

void blitreboot();

/***********
 *
 * Initialization.
 *
 ***********/


/* 
 * Probe - is the board there?
 *
 * in:	reg = start of mapped Blit memory.
 *
 * out: returns size of mapped Blit memory if the board is present,
 *	0 otherwise.
 *
 * effects: if the board is present, it is reset and left visible in 
 *	Unix mode.
 */

#if	AUTOINIT
/*ARGSUSED*/
int
blitprobe(reg, unit)
	caddr_t reg;
	int unit;
{
	struct blitdev *blt = (struct blitdev *)reg;

	if (blit_present())
		return(BLIT_MAPPED_SIZE); /* go */
	else
		return(0);		/* no-go */
}
#endif	/* AUTOINIT */


/*
 * Temporary initialization routine.  This will go away when we have
 * autoconfig.
 */

blitinit()
{
	if (!blit_present())
		return;

	blit_init();
}


/* 
 * Allocate needed objects from Blit's memory.
 */
blit_memory_init(bs)
	struct blitsoft *bs;
{
	struct blitdev *blt = bs->blt;
	struct blitmem *bm = (struct blitmem *)blt->graphmem;
	u_char *p = bm->spare;

	if ((int)p % 2 == 1)
		++p;

	bs->scrstrip = (struct screen_descrip *)p;
	p += sizeof(struct screen_descrip);
	if ((int)p % 2 == 1)
		++p;

	bs->dpctlregs = (DPCONTROLBLK *)p;
	p += sizeof(DPCONTROLBLK);
	if ((int)p % 2 == 1)
		++p;

	/*
	 * Note: if you use the 786 graphics processor for character
	 * processing, you should copy the font from the ROM into
	 * graphics memory and change font_start to point to it.
	 * Otherwise, the 786 will have problems accessing the font.
	 */

	bs->firstfree = p - blt->graphmem;
}


/* 
 * Reset the Blit board and leave it visible.
 */

blit_reset_board()
{
	union blit_config_reg config;
	
	config.byte = inb(BLIT_CONFIG_ADDR);
	config.reg.reset = 1;
	outb(BLIT_CONFIG_ADDR, config.byte);
	config.reg.reset = 0;
	config.reg.mode = BLIT_UNIX_MODE;
	config.reg.invisible = BLIT_VISIBLE;
	outb(BLIT_CONFIG_ADDR, config.byte);
	setstatus(CARD_RESET);
}


#if	AUTOINIT
/* 
 * Attach - finish initialization by setting up the 786.
 */

blitattach(md)
	struct mb_device *md;
{
	struct blitdev *blt = (struct blitdev *)md->md_addr;

	blit_init(xyz);
}
#endif	/* AUTOINIT */


/*
 * Initialize Bus Interface Unit.
 */

init_biu(blt)
	struct blitdev *blt;
{
	WRITEREG8(blt, INTER_RELOC, 0);
	WRITEREG8(blt, BIU_CONTROL, BIU_16BIT);

	/* WRITEREG16(blt, DRAM_REFRESH, 0x003f); */
	WRITEREG16(blt, DRAM_REFRESH, 0x0018);	/* refresh rate */
	WRITEREG16(blt, DRAM_CONTROL,  
		    MEMROWS1 | FASTPG_INTERLV | HEIGHT_256K);
	WRITEREG16(blt, DP_PRIORITY, (7 << 3) | 7); /* max pri */
	WRITEREG16(blt, GP_PRIORITY, (1 << 3) | 1); /* almost min pri */
	WRITEREG16(blt, EXT_PRIORITY, 5 << 3);

	/* now freeze the settings */
	WRITEREG16(blt, BIU_CONTROL, BIU_16BIT | BIU_WP1);

	/* Put graphics processor into Poll state. */
	WRITEREG16(blt, GP_OPCODE_REG, (OP_LINK|GECL));
}


/* 
 * Initialize the Display Processor.
 * XXX - assumes only 1 card is installed, assumes monochrome display.
 */

init_dp(bs)
	struct blitsoft *bs;
{
	struct blitdev *blt = bs->blt;
	struct blitmem *bm = (struct blitmem *)blt->graphmem;

	/*
	 * Set up strip header and tile descriptor for the whole 
	 * screen.  It's not clear why the C bit should be turned on, 
	 * but it seems to get rid of the nasty flickering you can get 
	 * by positioning an xterm window along the top of the screen.
	 */
	bs->scrstrip->strip.lines = BLIT_MONOHEIGHT - 1;
	bs->scrstrip->strip.linkl = 0;
	bs->scrstrip->strip.linkh = 0;
	bs->scrstrip->strip.tiles = DP_C_BIT | (1 - 1);
	tiledesc(&bs->scrstrip->tile,
		 0, 0,			/* x, y */
		 BLIT_MONOWIDTH,	/* width of strip */
		 BLIT_MONOWIDTH,	/* width of bitmap */
		 VM_TO_ADDR786(bm->fb.mono_fb, blt), /* the actual bitmap */
		 1);			/* bits per pixel */
	
	/* Copy into DP register block. */
	*(bs->dpctlregs) = blit_mparm;
	bs->dpctlregs->descl = DP_ADDRLOW(VM_TO_ADDR786(bs->scrstrip, blt));
	bs->dpctlregs->desch = DP_ADDRHIGH(VM_TO_ADDR786(bs->scrstrip, blt));

	/* Load the DP with the register block */
	loadall(blt, bs->dpctlregs);
}


/*
 * Fill in a tile descriptor.
 */

static
tiledesc(tile, x, y, w, ww, adx, bpp)
	TILEDESC *tile;			/* pointer to tile descriptor */
	int x;				/* starting x in bitmap */
	int y;				/* starting y in bitmap */
	int w;				/* width of strip (in bits_) */
	int ww;				/* actual width of bitmap (bits) */
	addr786_t adx;			/* start of bitmap */
	int bpp;			/* bits per pixel */
{
	u_short bm_width;
	short rghtp;
	short adr_left, adr_right;
	addr786_t bmstadr;
	u_short start_stop_bit;

	bm_width = 2 * (((ww + 1) * bpp) / 16);
	rghtp = x + w - 1;
	adr_left = ((x * bpp) / 16) * 2;
	adr_right = ((rghtp * bpp) / 16) * 2;
	bmstadr = (ww * y) + adr_left + (int)adx;
	start_stop_bit = ((((16 - 1) - ((x * bpp) % 16)) << 4) +
			((16 - ((rghtp + 1) * bpp) % 16) % 16) +
			(bpp << 8));

	tile->bitmapw = bm_width;
	tile->meml = DP_ADDRLOW(bmstadr);
	tile->memh = DP_ADDRHIGH(bmstadr);
	tile->bppss = start_stop_bit;
	tile->fetchcnt = adr_right - adr_left;
	tile->flags = 0;
}


/*
 * Cause the Display Processor to load its Control Registers from 
 * "vm_addr".
 */

static
loadall(blt, vm_addr)
struct blitdev *blt;
DPCONTROLBLK *vm_addr;
{
	addr786_t blit_addr = VM_TO_ADDR786(vm_addr, blt);
	int i;

	/* set up dp address */
	WRITEREG16(blt, DP_PARM1_REG, DP_ADDRLOW(blit_addr));
	WRITEREG16(blt, DP_PARM2_REG, DP_ADDRHIGH(blit_addr));
  
	/* set blanking video */
	WRITEREG16(blt, DEF_VIDEO_REG, 0);

	/* load opcode to start dp */
	WRITEREG16(blt, DP_OPCODE_REG, DP_LOADALL);

	/* wait for acceptance */
	for (i = 0; i < DP_RDYTIMEOUT; ++i)
		if (READREG(blt, DP_OPCODE_REG) & DECL)
			break;

	if (i >= DP_RDYTIMEOUT) {
		printf("Blit Display Processor timeout (loading registers)\n");
	hang:
		goto hang;
	}

#ifdef	notdef
	/* wait for acceptance */
	CDELAY((READREG(blt, DP_OPCODE_REG) & DECL) != 0, DP_RDYTIMEOUT);
	if ((READREG(blt, DP_OPCODE_REG) & DECL) == 0) {
		printf("Blit Display Processor timeout (loading registers)\n");
	hang:
		goto hang;
	}
#endif	/* notdef */
}


/*
 * blit_present: returns YES if Blit is present.  For the first call,
 * the hardware is probed.  After that, a flag is used.
 * Sets blitsoft[0].blt and blitsoft[0].physaddr.
 */

#define TEST_BYTE	0xa5		/* should not be all 0's or 1's */

boolean_t
blit_present()
{
	static boolean_t present = FALSE;
	static boolean_t initialized = FALSE;
	struct blitdev *blt;
	boolean_t blit_rom_ok();
	struct blitdev *mapblit();
	void freeblit();

	/*
	 * We set "initialized" early on so that if the Blit init. code
	 * fails, kdb will still be able to use the EGA or VGA display
	 * (if present).
	 */
	if (initialized)
		return(present);
	initialized = TRUE;

	blit_reset_board();
	blt = mapblit((caddr_t)BLIT_BASE_ADDR, BLIT_MAPPED_SIZE);
	setstatus(CARD_MAPPED);
	if (blt == NULL)
		panic("blit: can't map display");
	blt->graphmem[0] = TEST_BYTE;
	present = FALSE;
	if (blt->graphmem[0] == TEST_BYTE) {
		setstatus(CARD_MAYBE_PRESENT);
		present = blit_rom_ok(blt);
	}
	if (present) {
		blitsoft[0].blt = blt;
		blitsoft[0].physaddr = (caddr_t)BLIT_BASE_ADDR;
		setstatus(CARD_PRESENT);
	}
	else
		freeblit((vm_offset_t)blt, BLIT_MAPPED_SIZE);
	return(present);
}

#undef TEST_BYTE


/*
 * mapblit: map the card into kernel vm and return the (virtual)
 * address.
 */
struct blitdev *
mapblit(physaddr, length)
caddr_t physaddr;			/* start of card */
int length;				/* num bytes to map */
{
	vm_offset_t vmaddr;
#ifdef	MACH_KERNEL
	vm_offset_t io_map();
#else	MACH_KERNEL
	vm_offset_t pmap_map_bd();
#endif	MACH_KERNEL

	if (physaddr != (caddr_t)trunc_page(physaddr))
		panic("Blit card not on page boundary");

#ifdef	MACH_KERNEL
	vmaddr = io_map((vm_offset_t)physaddr, length);
	if (vmaddr == 0)
#else	MACH_KERNEL
	if (kmem_alloc_pageable(kernel_map,
				&vmaddr, round_page(BLIT_MAPPED_SIZE))
							!= KERN_SUCCESS)
#endif	MACH_KERNEL
		panic("can't alloc VM for Blit card");

	(void)pmap_map_bd(vmaddr, (vm_offset_t)physaddr,
			(vm_offset_t)physaddr+length,
			VM_PROT_READ | VM_PROT_WRITE);
	return((struct blitdev *)vmaddr);
}


/*
 * freeblit: free card from memory.
 * XXX - currently a no-op.
 */
void
freeblit(va, length)
vm_offset_t va;				/* virt addr start of card */
int length;
{
}


/*
 * blit_init: initialize globals & hardware, and set cursor.  Could be
 * called twice, once as part of kd initialization and once as part of
 * blit initialization.  Should not be called before blit_present() is 
 * called. 
 */

void
blit_init()
{
	static boolean_t initialized = FALSE;
	struct blitmem *gmem;		/* start of blit graphics memory */
	int card;
	void getfontinfo(), clear_blit();

	if (initialized)
		return;

	for (card = 0; card < NBLIT; ++card) {
		if (card > 0) {
			blitsoft[card].blt = NULL;
			blitsoft[card].physaddr = NULL;
		}
		blitsoft[card].open = FALSE;
		blitsoft[card].scrstrip = NULL;
		blitsoft[card].dpctlregs = NULL;
		blitsoft[card].firstfree = 0;
	}

	/*
	 * blit_memory_init allocates memory used by the Display Processor,
	 * so it comes before the call to init_dp.  blit_memory_init
	 * potentially copies the font from ROM into the graphics memory,
	 * so it comes after the call to getfontinfo.
	 */
	getfontinfo(blitsoft[0].blt);	/* get info & check assumptions */
	blit_memory_init(&blitsoft[0]);

	/* init 786 */
	init_biu(blitsoft[0].blt);
	setstatus(BIU_INIT);
	init_dp(&blitsoft[0]);
	setstatus(DP_INIT);

	gmem = (struct blitmem *)blitsoft[0].blt->graphmem;
	vid_start = gmem->fb.mono_fb;
	kd_lines = 25;
	kd_cols = 80;
	kd_attr = KA_NORMAL;

	/*
	 * Use generic bitmap routines, no 786 assist (see
	 * blit_memory_init). 
	 */
	kd_dput = bmpput;
	kd_dmvup = bmpmvup;
	kd_dmvdown = bmpmvdown;
	kd_dclear = bmpclear;
	kd_dsetcursor = bmpsetcursor;
	kd_dreset = blitreboot;

	clear_blit(blitsoft[0].blt);
	(*kd_dsetcursor)(0);

	initialized = TRUE;
}


/*
 * blit_rom_ok: make sure we're looking at the ROM for a monochrome
 * Blit.
 */

boolean_t
blit_rom_ok(blt)
	struct blitdev *blt;
{
	short magic;
	short bpp;

	magic = READROM(blt->eprom, EP_MAGIC1);
	if (magic != EP_MAGIC1_VAL) {
#ifdef notdef
		printf("blit: magic1 bad (0x%x)\n", magic);
#endif
		return(FALSE);
	}
	magic = READROM(blt->eprom, EP_MAGIC2);
	if (magic != EP_MAGIC2_VAL) {
#ifdef notdef
		printf("blit: magic2 bad (0x%x)\n", magic);
#endif
		return(FALSE);
	}
	bpp = READROM(blt->eprom, EP_BPP);
	if (bpp != 1) {
#ifdef notdef
		printf("blit: not monochrome board (bpp = 0x%x)\n", bpp);
#endif
		return(FALSE);
	}

	return(TRUE);
}


/*
 * getfontinfo: get information about the font and make sure that
 * our simplifying assumptions are valid.
 */

void
getfontinfo(blt)
	struct blitdev *blt;
{
	u_char *rom = blt->eprom;
	short fontoffset;
	short pick_cursor_height();

	fb_width = BLIT_MONOWIDTH;
	fb_height = BLIT_MONOHEIGHT;
	chars_in_font = READROM(rom, EP_NUMCHARS);
	char_width = READROM(rom, EP_CHARWIDTH);
	char_height = READROM(rom, EP_CHARHEIGHT);
	fontoffset = READROM(rom, EP_FONTSTART);
	xstart = READROM(rom, EP_XSTART);
	ystart = READROM(rom, EP_YSTART);
	char_black = BLIT_BLACK_BYTE;
	char_white = BLIT_WHITE_BYTE;

	font_start = rom + fontoffset;
	
	/*
	 * Check byte-alignment assumption.
	 * XXX - does it do any good to panic when initializing the
	 * console driver?
	 */
	if (char_width % 8 != 0)
		panic("blit: char width not integral num of bytes");
	if (xstart % 8 != 0) {
		/* move it to a more convenient location */
		printf("blit: console corner moved.\n");
		xstart = 8 * (xstart/8);
	}

	cursor_height = pick_cursor_height();
	char_byte_width = char_width / 8;
	fb_byte_width = BLIT_MONOWIDTH / 8;
	font_byte_width = char_byte_width * chars_in_font;
}


/*
 * pick_cursor_height: pick a size for the cursor, based on the font
 * size.
 */

short
pick_cursor_height()
{
	int scl_avail;			/* scan lines available for console */
	int scl_per_line;		/* scan lines per console line */

	/* 
	 * scan lines avail. = total lines - top margin; 
	 * no bottom margin (XXX).
	 */
	scl_avail = BLIT_MONOHEIGHT - ystart;

	scl_per_line = scl_avail / kd_lines;
	if (scl_per_line < char_height)
		return(1);
	else
		return(scl_per_line - char_height);
}


/* 
 * setstatus: Give a status indication to the user.  Ideally, we'd 
 * just set the 3 user-controlled LED's.  Unfortunately, that doesn't 
 * seem to work.  So, we ring the bell.
 */

static void
setstatus(val)
	int val;
{
	union blit_diag_reg diag;
	
	diag.byte = inb(BLIT_DIAG_ADDR);
	diag.reg.led0 = (val & 1) ? BLIT_LED_ON : BLIT_LED_OFF;
	diag.reg.led1 = (val & 2) ? BLIT_LED_ON : BLIT_LED_OFF;
	diag.reg.led2 = (val & 4) ? BLIT_LED_ON : BLIT_LED_OFF;
	outb(BLIT_DIAG_ADDR, diag.byte);

#ifdef DEBUG
	for (val &= 7; val > 0; val--) {
		feep();
		pause();
	}
	for (val = 0; val < 10; val++) {
		pause();
	}
#endif
}



/***********
 *
 * Other (non-initialization) routines.
 *
 ***********/


/* 
 * Open - Verify that minor device is OK and not in use, then clear 
 * the screen.
 */

/*ARGSUSED*/
int
blitopen(dev, flag)
	dev_t dev;
	int flag;
{
	void clear_blit();
	int which = minor(dev);

	if (!blit_present() || which >= NBLIT)
		return(ENXIO);
	if (blitsoft[which].open)
		return(EBUSY);

	clear_blit(blitsoft[which].blt);
	blitsoft[which].open = TRUE;
	return(0);			/* ok */
}


/*
 * Close - free any kernel memory structures that were allocated while
 * the device was open (currently none).
 */

/*ARGSUSED*/
blitclose(dev, flag)
	dev_t dev;
	int flag;
{
	int which = minor(dev);

	if (!blitsoft[which].open)
		panic("blit: closing not-open device??");
	blitsoft[which].open = FALSE;
}


/* 
 * Mmap.
 */

/*ARGSUSED*/
int
blitmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	if ((u_int) off >= BLIT_MAPPED_SIZE)
		return(-1);

	/* Get page frame number for the page to be mapped. */
	return(i386_btop(blitsoft[minor(dev)].physaddr + off));
}


/* 
 * Ioctl.
 */

#ifdef	MACH_KERNEL
io_return_t blit_get_stat(dev, flavor, data, count)
	dev_t		dev;
	int		flavor;
	int		*data;	/* pointer to OUT array */
	unsigned int 	*count;	/* OUT */
{
	int	which = minor(dev);

	switch (flavor) {
	    case BLIT_1ST_UNUSED:
		if (*count < 1)
		    return (D_INVALID_OPERATION);
		*data = blitsoft[which].firstfree;
		*count = 1;
		break;
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}
#else	MACH_KERNEL
/*ARGSUSED*/
int
blitioctl(dev, cmd, data, flag)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
{
	int which = minor(dev);
	int err = 0;

	switch (cmd) {
	case BLIT_1ST_UNUSED:
		*(int *)data = blitsoft[which].firstfree;
		break;
	default:
		err = ENOTTY;
	}

	return(err);
}
#endif	MACH_KERNEL

/*
 * clear_blit: clear blit's screen.
 */

void
clear_blit(blt)
	struct blitdev *blt;
{
	(*kd_dclear)(0, kd_lines*kd_cols, KA_NORMAL);
}

/* 
 * Put the board into DOS mode in preparation for rebooting.
 */

void
blitreboot()
{
	union blit_config_reg config;
	
	config.byte = inb(BLIT_CONFIG_ADDR);
	config.reg.mode = BLIT_DOS_MODE;
	config.reg.invisible = BLIT_VISIBLE;
	outb(BLIT_CONFIG_ADDR, config.byte);
}

#endif	/* NBLIT > 0 */
#endif	/* NBLIT > MAXBLITS */
