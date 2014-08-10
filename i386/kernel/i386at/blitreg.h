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
 File:         blitreg.h
 Description:  Bell Tech Blit card hardware description

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
 * Some code taken from Bob Glossman's 1987 "minimal Blit Express 
 * driver", copyright unknown.  Probably copyright Intel, too.
 */


#ifndef	blitreg_DEFINED
#define blitreg_DEFINED


/* 
 * Registers accessible through AT I/O space.  These addresses can be 
 * changed by changing bits 4-8 of the Blit's DIP switch.
 */

#define BLIT_CONFIG_ADDR	0x304
#define BLIT_DIAG_ADDR		0x306

#if	defined(sun386) || defined(i386)


/* 
 * Layout of Blit control register.
 */

union blit_config_reg {
	struct config_bits {
		unsigned dos_segment : 4;
		unsigned reset : 1;
		unsigned mode : 1;
#define BLIT_UNIX_MODE	1
#define BLIT_DOS_MODE	0
		unsigned invisible : 1;
#define BLIT_INVISIBLE	1
#define BLIT_VISIBLE	0
		unsigned unused : 1;
	} reg;
	u_char byte;
};


/* 
 * Blit Diag register.
 * The UNIX base address is currently hardwired to BLIT_BASE_ADDR.
 */

#define BLIT_BASE_ADDR	0xd80000	/* base of blit memory (phys addr) */

union blit_diag_reg {
	struct diag_bits {
		unsigned unix_base_addr : 5; /* phys addr (ignored) */
		unsigned led0 : 1;
		unsigned led1 : 1;
		unsigned led2 : 1;
#define BLIT_LED_ON	1
#define BLIT_LED_OFF	0
	} reg;
	u_char byte;
};

#endif	/* sun386 || i386 */


/* 
 * Graphics memory, 786 registers, static RAM, and EPROM, all 
 * accessible through mapped memory.
 */

#define BLIT_MONOWIDTH	1664
#define BLIT_MONOHEIGHT	1200
#define BLIT_MONOFBSIZE	((BLIT_MONOWIDTH*BLIT_MONOHEIGHT)/8)
					/* byte size of monochrome fb */

#define BLIT_MEMSIZE	0x100000	/* num bytes mapped graphics memory */

#define BLIT_REGSIZE	128		/* bytes taken by 786 registers */
#define BLIT_REGPAD	(0x10000  - BLIT_REGSIZE)
					/* padding between reg's and SRAM */

#define BLIT_SRAMSIZE	0x4000		/* num bytes mapped for SRAM */
#define BLIT_SRAMPAD	(0x10000 - BLIT_SRAMSIZE)
					/* padding between SRAM and EPROM */

#define BLIT_EPROMSIZE	0x20000		/* num bytes mapped for EPROM */


/*      
 * Layout of the Blit's mapped memory.  The physical address is (or
 * will be, eventually) determined by the Diag register (above).
 */

struct  blitdev {
	u_char graphmem[BLIT_MEMSIZE];
	u_char reg786[BLIT_REGSIZE];
	u_char pad1[BLIT_REGPAD];
	u_char sram[BLIT_SRAMSIZE];
	u_char pad2[BLIT_SRAMPAD];
	u_char eprom[BLIT_EPROMSIZE];
};

#define BLIT_MAPPED_SIZE	sizeof(struct blitdev)


/*
 * Offsets for 786 registers (i.e., indices into reg786[]).
 */

#define INTER_RELOC	0x00	/* Internal Relocation Register */
#define BIU_CONTROL	0x04	/* BIU Control Register */
#define DRAM_REFRESH	0x06	/* DRAM Refresh control register */
#define DRAM_CONTROL    0x08	/* DRAM control register */
#define DP_PRIORITY	0x0A	/* DP priority register */
#define GP_PRIORITY	0x0C	/* GP priority register*/
#define EXT_PRIORITY	0x0E	/* External Priority Register*/
#define GP_OPCODE_REG	0x20	/* GP opcode register */
#define GP_PARM1_REG	0x22	/* GP Parameter 1 Register */
#define GP_PARM2_REG	0x24	/* GP Parameter 2 Register*/
#define GP_STAT_REG	0x26	/* GP Status Register*/
#define DP_OPCODE_REG	0x40	/* DP opcode register */
#define DP_PARM1_REG	0x42	/* DP Parameter 1 Register*/
#define DP_PARM2_REG	0x44	/* DP Parameter 2 Register*/
#define DP_PARM3_REG	0x46	/* DP Parameter 3 Register*/
#define DP_STAT_REG	0x48	/* DP Status Register*/
#define DEF_VIDEO_REG	0x4A	/* DP Default Video Register*/


/* 
 * 786 BIU Control Register values.
 */

#define BIU_WP1		0x02		/* Write Protect One; 1 = on */
#define BIU_16BIT	0x10	/* access 786 registers as words; 0 = bytes */


/* 
 * 786 DRAM/VRAM Control Register values.
 */

/* RW bits */
#define MEMROWS1	0
#define MEMROWS2	0x20
#define MEMROWS3	0x40
#define MEMROWS4	0x60

/* DC bits */
#define PG_NONINTERLV		0
#define FASTPG_NONINTERLV	0x10
#define PG_INTERLV		0x08
#define FASTPG_INTERLV		0x18

/* HT bits */
#define HEIGHT_8K	0
#define HEIGHT_16K	0x1
#define HEIGHT_32K	0x2
#define HEIGHT_64K	0x3
#define HEIGHT_128K	0x4
#define HEIGHT_256K	0x5
#define HEIGHT_512K	0x6
#define HEIGHT_1M	0x7


/* 
 * 786 Graphics Processor opcodes.
 */

#define GECL	0x001			/* end of command list */
#define OP_LINK	0x200			/* LINK - "link next cmd" */


/* 
 * 786 Display Processor opcodes.
 */

#define DECL	1			/* end of list */
#define DP_LOADALL	0x500


/* 
 * Macros for accessing 786 registers (see BIU_16BIT) and EPROM.
 */

#define WRITEREG8(base,offset,val) \
	(base)->reg786[(offset)] = (val) & 0xff, \
	(base)->reg786[(offset)+1] = ((val) & 0xff00) >> 8

#define WRITEREG16(base,offset,val) \
	(*((u_short *)((base)->reg786+(offset)))) = (val)

#define READREG(base,offset) \
	(*((u_short *)(((base)->reg786+(offset)))))

#define WRITEROM(romp,offset,val) \
	(*((u_short *)((romp)+(offset)))) = (val)

#define READROM(romp,offset) \
	(*((u_short *)(((romp)+(offset)))))


/* 
 * Layout of Display Processor Control Block Registers.  This block is 
 * allocated somewhere in the Blit's graphics memory, and a pointer to 
 * it is passed to the Display Processor.
 * 
 * NOTE: The 786 only sees the memory mapped by the Blit.  Thus all 
 * addresses passed to the 786 are relative to the start of the Blit's 
 * mapped memory.
 */

typedef int addr786_t;			/* 0 = start of Blit mapped memory */

typedef struct {
	u_short vidstat;		/* video status */
	u_short intrmask;		/* interrupt mask */
	u_short trip_point;
	u_short frame_intr;		/* frame interrupt */
	u_short reserved1;
	u_short crtmode;		/* CRT controller mode */
	u_short hsyncstop;		/* monitor parameters */
	u_short hfldstart;
	u_short hfldstop;
	u_short linelength;
	u_short vsyncstop;
	u_short vfldstart;
	u_short vfldstop;
	u_short vframelen;
	u_short descl;			/* descriptor pointer low part */
	u_short desch;			/* descriptor pointer high part */
	u_short reserved2;
	u_short xyzoom;
	u_short fldcolor;
	u_short bordercolor;
	u_short bpp_pad1;
	u_short bpp_pad2;
	u_short bpp_pad4;
	u_short csrmode;		/* & CsrPad */
	u_short cursorx;		/* cursor x location */
	u_short cursory;		/* cursor y location */
	u_short cursorpat[16];		/* cursor pattern */
} DPCONTROLBLK;


/* 
 * Values for 786 Display Processor Control Block Registers.
 */

/* video status */
#define DP_DSP_ON	1		/* display on */
#define DP_CSR_ON	2		/* cursor on */

/* CRT controller modes */
#define CRTM_NONINTER		0	/* non-interlaced */
#define CRTM_INTERLCD		0x40	/* interlaced */
#define CRTM_INTERSYN		0x60	/* interlaced - sync */
#define CRTM_WIN_STAT_ENABLE	0x10	/* window status enable */
#define CRTM_SYNC_SLAVE_MODE	0x08	/* on = operate as slave */
#define CRTM_BLANK_SLAVE_MODE	0x04	/* on = Blank is input */
#define CRTM_NORMAL_SPEED	0x00
#define CRTM_HIGH_SPEED		0x01
#define CRTM_VRYHIGH_SPEED	0x02
#define CRTM_SUPHIGH_SPEED	0x03

/* cursor style */
#define DP_CURSOR_16X16		0x8000	/* off = 8x8 */
#define DP_CURSOR_CROSSHAIR	0x4000	/* off = block cursor */
#define DP_CURSOR_TRANSPRNT	0x2000	/* off = cursor is opaque */


/* 
 * Types for dealing with 786 Display Processor.
 */

typedef struct {
	u_short lines;			/* (lines in strip) - 1 */
	u_short linkl;			/* link to next strip low part */
	u_short linkh;			/* link to next strip high part */
	u_short tiles;			/* C bit, (tiles in strip) - 1 */
} STRIPHEADER;

/* 
 * If the C bit is turned on, the display processor "automatically 
 * displays the background color" for areas not defined by the strips.
 * See section 3.1.3.2 of the '786 User's Manual.
 */
#define DP_C_BIT	0x8000

typedef struct {
	u_short bitmapw;		/* width of bitmap */
	u_short meml;			/* btb mem address low part */
	u_short memh;			/* btb mem address high part */
	u_short bppss;			/* bpp, start and stop fields */
	u_short fetchcnt;		/* fetch count */
	u_short flags;			/* various flags */
} TILEDESC;


/* 
 * Macros for encoding addresses for strip headers & tile descriptors.
 * addr786 is relative to the start of the Blit's mapped memory.
 */

#define DP_ADDRLOW(addr786)		(((int)(addr786)) & 0xffff)
#define DP_ADDRHIGH(addr786)		((((int)(addr786)) >> 16) & 0x3f)


/*
 * Byte offsets to useful data words within the EPROM.
 */

#define EP_MAGIC1	0
#define EP_MAGIC1_VAL	0x7856
#define EP_MAGIC2	2
#define EP_MAGIC2_VAL	0x6587
#define EP_DPSTART	4		/* start of DP ctl block */
					/* (0 = start of EPROM) */
#define EP_DPLEN	6		/* byte length of DP control block */

#define EP_FONTSTART	8		/* start of font */
					/* (0 = start of EPROM) */
#define EP_FONTLEN	10		/* byte length of font */
#define EP_CHARWIDTH	12		/* bit width of each char in font */
#define EP_CHARHEIGHT	14
#define EP_NUMCHARS	16		/* num chars in font */

/* where in the bitmap the 25x80 console screen starts */
#define EP_XSTART	18
#define EP_YSTART	20

#define EP_SCREENWIDTH	22		/* pixels per scan line */
#define EP_SCREENHEIGHT	24		/* number of scan lines */

#define EP_FIXUP_X	26		/* magic numbers for displaying */
#define EP_FIXUP_Y	28		/* hardware cursor */

#define EP_BPP		30		/* bits per pixel */


/*
 * Miscellaneous.
 */

#define BLIT_BLACK_BIT	0		/* try saying that 3 times fast */
#define BLIT_WHITE_BIT	1
#define BLIT_BLACK_BYTE	0
#define BLIT_WHITE_BYTE	0xff


#endif	/* blitreg_DEFINED */
