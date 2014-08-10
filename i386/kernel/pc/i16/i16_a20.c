/* 
 * Copyright (c) 1995-1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */


#include <mach/machine/pio.h>
#include <mach/machine/code16.h>

#include "i16_a20.h"


/* Keyboard stuff for turning on the A20 address line (gak!).  */
#define K_RDWR 		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS 	0x64		/* keyboard status (read-only) */
#define K_CMD	 	0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL 	0x01		/* output buffer full */
#define K_IBUF_FUL 	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */

#define KB_ENABLE_A20	0xdf	/* Linux and my BIOS uses this,
				   and I trust them more than Mach 3.0,
				   but I'd like to know what the difference is
				   and if it matters.  */
			/*0x9f*/	/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */
#define KB_DISABLE_A20	0xdd


CODE16


/*
   This routine ensures that the keyboard command queue is empty
   (after emptying the output buffers)

   No timeout is used - if this hangs there is something wrong with
   the machine, and we probably couldn't proceed anyway.
   XXX should at least die properly
*/
static void i16_empty_8042(void)
{
	int status;

retry:
	i16_nanodelay(1000);
	status = i16_inb(K_STATUS);

	if (status & K_OBUF_FUL)
	{
		i16_nanodelay(1000);
		i16_inb(K_RDWR);
		goto retry;
	}

	if (status & K_IBUF_FUL)
		goto retry;
}

int i16_raw_test_a20(void);

/* Enable the A20 address line.  */
void i16_raw_enable_a20(void)
{
	int v;

	/* XXX try int 15h function 24h */

	if (i16_raw_test_a20())
		return;

	/* PS/2 */
	v = i16_inb(0x92);
	i16_nanodelay(1000);
	i16_outb(0x92,v | 2);

	if (i16_raw_test_a20())
		return;

	/* AT */
	i16_empty_8042();
	i16_outb(K_CMD, KC_CMD_WOUT);
	i16_empty_8042();
	i16_outb(K_RDWR, KB_ENABLE_A20);
	i16_empty_8042();

	/* Wait until the a20 line gets enabled.  */
	while (!i16_raw_test_a20());
}

/* Disable the A20 address line.  */
void i16_raw_disable_a20(void)
{
	int v;

	if (!i16_raw_test_a20())
		return;

	/* PS/2 */
	v = i16_inb(0x92);
	i16_nanodelay(1000);
	i16_outb(0x92, v & ~2);

	if (!i16_raw_test_a20())
		return;

	/* AT */
	i16_empty_8042();
	i16_outb(K_CMD, KC_CMD_WOUT);
	i16_empty_8042();
	i16_outb(K_RDWR, KB_DISABLE_A20);
	i16_empty_8042();

	/* Wait until the a20 line gets disabled.  */
	while (i16_raw_test_a20());
}


void (*i16_enable_a20)(void) = i16_raw_enable_a20;
void (*i16_disable_a20)(void) = i16_raw_disable_a20;

