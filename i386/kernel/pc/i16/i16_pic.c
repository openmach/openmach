/* 
 * Copyright (c) 1995 The University of Utah and
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

#include "pic.h"
#include "i16.h"

CODE16

/* Program the PICs to use a different set of interrupt vectors.
   Assumes processor I flag is off.  */
void i16_pic_set_master(int base)
{
	unsigned char old_mask;

	/* Save the original interrupt mask.  */
	old_mask = inb(MASTER_OCW);	PIC_DELAY();

	/* Initialize the master PIC.  */
	outb(MASTER_ICW, PICM_ICW1);	PIC_DELAY();
	outb(MASTER_OCW, base);		PIC_DELAY();
	outb(MASTER_OCW, PICM_ICW3);	PIC_DELAY();
	outb(MASTER_OCW, PICM_ICW4);	PIC_DELAY();

	/* Restore the original interrupt mask.  */
	outb(MASTER_OCW, old_mask);	PIC_DELAY();
}

void i16_pic_set_slave(int base)
{
	unsigned char old_mask;

	/* Save the original interrupt mask.  */
	old_mask = inb(SLAVES_OCW);	PIC_DELAY();

	/* Initialize the slave PIC.  */
	outb(SLAVES_ICW, PICS_ICW1);	PIC_DELAY();
	outb(SLAVES_OCW, base);		PIC_DELAY();
	outb(SLAVES_OCW, PICS_ICW3);	PIC_DELAY();
	outb(SLAVES_OCW, PICS_ICW4);	PIC_DELAY();

	/* Restore the original interrupt mask.  */
	outb(SLAVES_OCW, old_mask);	PIC_DELAY();
}

