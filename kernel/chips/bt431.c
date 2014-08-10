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
 *	File: bt431.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	8/91
 *
 *	Routines for the bt431 Cursor
 */

#include <platforms.h>

#include <chips/bt431.h>
#include <chips/screen.h>

#ifdef	DECSTATION
/*
 * This configuration uses two twin 431s
 */
#define	set_value(x)	(((x)<<8)|((x)&0xff))
#define	get_value(x)	((x)&0xff)

typedef struct {
	volatile unsigned short	addr_lo;
	short				pad0;
	volatile unsigned short	addr_hi;
	short				pad1;
	volatile unsigned short	addr_cmap;
	short				pad2;
	volatile unsigned short	addr_reg;
	short				pad3;
} bt431_padded_regmap_t;

#else	/*DECSTATION*/

#define set_value(x)	x
#define get_value(x)	x
typedef bt431_regmap_t	bt431_padded_regmap_t;
#define	wbflush()

#endif	/*DECSTATION*/

/*
 * Generic register access
 */
void
bt431_select_reg( regs, regno)
	bt431_padded_regmap_t	*regs;
{
	regs->addr_lo = set_value(regno&0xff);
	regs->addr_hi = set_value((regno >> 8) & 0xff);
	wbflush();
}

void 
bt431_write_reg( regs, regno, val)
	bt431_padded_regmap_t	*regs;
{
	bt431_select_reg( regs, regno );
	regs->addr_reg = set_value(val);
	wbflush();
}

unsigned char
bt431_read_reg( regs, regno)
	bt431_padded_regmap_t	*regs;
{
	bt431_select_reg( regs, regno );
	return get_value(regs->addr_reg);
}

/* when using autoincrement */
#define	bt431_write_reg_autoi( regs, regno, val)	\
	{						\
		(regs)->addr_reg = set_value(val);	\
		wbflush();				\
	}
#define	bt431_read_reg_autoi( regs, regno)		\
		get_value(((regs)->addr_reg))

#define	bt431_write_cmap_autoi( regs, regno, val)	\
	{						\
		(regs)->addr_cmap = (val);		\
		wbflush();				\
	}
#define	bt431_read_cmap_autoi( regs, regno)		\
		((regs)->addr_cmap)


/*
 * Cursor ops
 */
bt431_cursor_on(regs)
	bt431_padded_regmap_t	*regs;
{
	bt431_write_reg( regs, BT431_REG_CMD,
			 BT431_CMD_CURS_ENABLE|BT431_CMD_OR_CURSORS|
			 BT431_CMD_4_1_MUX|BT431_CMD_THICK_1);
}

bt431_cursor_off(regs)
	bt431_padded_regmap_t	*regs;
{
	bt431_write_reg( regs, BT431_REG_CMD, BT431_CMD_4_1_MUX);
}

bt431_pos_cursor(regs,x,y)
	bt431_padded_regmap_t	*regs;
	register int	x,y;
{
#define lo(v)	((v)&0xff)
#define hi(v)	(((v)&0xf00)>>8)

	/*
	 * Cx = x + D + H - P
	 *  P = 37 if 1:1, 52 if 4:1, 57 if 5:1
	 *  D = pixel skew between outdata and external data
	 *  H = pixels between HSYNCH falling and active video
	 *
	 * Cy = y + V - 32
	 *  V = scanlines between HSYNCH falling, two or more
	 *	clocks after VSYNCH falling, and active video
	 */

	bt431_write_reg( regs, BT431_REG_CXLO, lo(x + 360));
	/* use autoincr feature */
	bt431_write_reg_autoi( regs, BT431_REG_CXHI, hi(x + 360));
	bt431_write_reg_autoi( regs, BT431_REG_CYLO, lo(y + 36));
	bt431_write_reg_autoi( regs, BT431_REG_CYHI, hi(y + 36));
}


bt431_cursor_sprite( regs, cursor)
	bt431_padded_regmap_t	*regs;
	register unsigned short	*cursor;
{
	register int	i;

	bt431_select_reg( regs, BT431_REG_CRAM_BASE+0);
	for (i = 0; i < 512; i++)
		bt431_write_cmap_autoi( regs, BT431_REG_CRAM_BASE+i, *cursor++);
}

#if 1
bt431_print_cursor(regs)
	bt431_padded_regmap_t	*regs;
{
	unsigned short curs[512];
	register int i;

	bt431_select_reg( regs, BT431_REG_CRAM_BASE+0);
	for (i = 0; i < 512; i++) {
		curs[i] = bt431_read_cmap_autoi( regs, BT431_REG_CRAM_BASE+i);
	}
	for (i = 0; i < 512; i += 16)
		printf("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			curs[i], curs[i+1], curs[i+2], curs[i+3],
			curs[i+4], curs[i+5], curs[i+6], curs[i+7],
			curs[i+8], curs[i+9], curs[i+10], curs[i+11],
			curs[i+12], curs[i+13], curs[i+14], curs[i+15]);
}

#endif

/*
 * Initialization
 */
unsigned /*char*/short bt431_default_cursor[64*8] = {
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0xffff, 0, 0, 0, 0, 0, 0, 0,
	0,
};

bt431_init(regs)
	bt431_padded_regmap_t	*regs;
{
	register int	i;

	/* use 4:1 input mux */
	bt431_write_reg( regs, BT431_REG_CMD,
			 BT431_CMD_CURS_ENABLE|BT431_CMD_OR_CURSORS|
			 BT431_CMD_4_1_MUX|BT431_CMD_THICK_1);

	/* home cursor */
	bt431_write_reg_autoi( regs, BT431_REG_CXLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_CXHI, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_CYLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_CYHI, 0x00);

	/* no crosshair window */
	bt431_write_reg_autoi( regs, BT431_REG_WXLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WXHI, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WYLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WYHI, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WWLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WWHI, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WHLO, 0x00);
	bt431_write_reg_autoi( regs, BT431_REG_WHHI, 0x00);

	/* load default cursor */
	bt431_cursor_sprite( regs, bt431_default_cursor);
}
