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
Copyright (c) 1988,1989 Prime Computer, Inc.  Natick, MA 01760
All Rights Reserved.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and
without fee is hereby granted, provided that the above
copyright notice appears in all copies and that both the
copyright notice and this permission notice appear in
supporting documentation, and that the name of Prime
Computer, Inc. not be used in advertising or publicity
pertaining to distribution of the software without
specific, written prior permission.

THIS SOFTWARE IS PROVIDED "AS IS", AND PRIME COMPUTER,
INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN
NO EVENT SHALL PRIME COMPUTER, INC.  BE LIABLE FOR ANY
SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN ACTION OF CONTRACT, NEGLIGENCE, OR
OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <mach/machine/pio.h>

#include <sys/types.h>

#include "ipl.h"
#include "pic.h"


u_short pic_mask[SPLHI+1];

int		curr_ipl;
u_short		curr_pic_mask;

u_short		orig_pic_mask;
int		orig_pic_mask_initialized;

u_char intpri[NINTR];

/*
** picinit() - This routine 
**		* Establishes a table of interrupt vectors
**		* Establishes a table of interrupt priority levels
**		* Establishes a table of interrupt masks to be put
**			in the PICs.
**		* Establishes location of PICs in the system 
**		* Initialises them
**
**	At this stage the interrupt functionality of this system should be 
**	coplete.
**
*/


/*
** 1. First we form a table of PIC masks - rather then calling form_pic_mask()
**	each time there is a change of interrupt level - we will form a table
**	of pic masks, as there are only 7 interrupt priority levels.
**
** 2. The next thing we must do is to determine which of the PIC interrupt
**	request lines have to be masked out, this is done by calling 
**	form_pic_mask() with a (int_lev) of zero, this will find all the 
**	interrupt lines that have priority 0, (ie to be ignored).
**	Then we split this up for the master/slave PICs.
**
** 2. Initialise the PICs , master first, then the slave.
**	All the register field definitions are described in pic_jh.h, also
**	the settings of these fields for the various registers are selected.
**
*/

pic_init(int master_base, int slave_base)
{
	u_short PICM_OCW1, PICS_OCW1 ;
	u_short PICM_OCW2, PICS_OCW2 ;
	u_short PICM_OCW3, PICS_OCW3 ;
	u_short i;

	if (!orig_pic_mask_initialized)
	{
		unsigned omaster, oslave;

		omaster = inb(MASTER_OCW);
		PIC_DELAY();
		oslave = inb(SLAVES_OCW);
		PIC_DELAY();

		orig_pic_mask = omaster | (oslave << 8);
		orig_pic_mask_initialized = 1;
	}


	/*
	** 1. Form pic mask table
	*/

	form_pic_mask();

	/*
	** 1a. Select current SPL.
	*/

	curr_ipl = SPLHI;
	curr_pic_mask = pic_mask[SPLHI];

	/*
	** 3. Select options for each ICW and each OCW for each PIC.
	*/

#if 0
	PICM_ICW1 = (ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
			| CASCADE_MODE | ICW4__NEEDED);

	PICS_ICW1 = (ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8
			| CASCADE_MODE | ICW4__NEEDED);

	PICM_ICW2 = master_base;
	PICS_ICW2 = slave_base;

	PICM_ICW3 = ( SLAVE_ON_IR2 );
	PICS_ICW3 = ( I_AM_SLAVE_2 );

	PICM_ICW4 = (SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
			| I8086_EMM_MOD);
	PICS_ICW4 = (SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD
			| I8086_EMM_MOD);
#endif

	PICM_OCW1 = (curr_pic_mask & 0x00FF);
	PICS_OCW1 = ((curr_pic_mask & 0xFF00)>>8);

	PICM_OCW2 = NON_SPEC_EOI;
	PICS_OCW2 = NON_SPEC_EOI;

	PICM_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );
	PICS_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );


	/* 
	** 4.	Initialise master - send commands to master PIC
	*/ 

	outb ( MASTER_ICW, PICM_ICW1 );
	PIC_DELAY();
	outb ( MASTER_OCW, master_base );
	PIC_DELAY();
	outb ( MASTER_OCW, PICM_ICW3 );
	PIC_DELAY();
	outb ( MASTER_OCW, PICM_ICW4 );
	PIC_DELAY();

#if 0
	outb ( MASTER_OCW, PICM_MASK );
	PIC_DELAY();
	outb ( MASTER_ICW, PICM_OCW3 );
	PIC_DELAY();
#endif

	/*
	** 5.	Initialise slave - send commands to slave PIC
	*/

	outb ( SLAVES_ICW, PICS_ICW1 );
	PIC_DELAY();
	outb ( SLAVES_OCW, slave_base );
	PIC_DELAY();
	outb ( SLAVES_OCW, PICS_ICW3 );
	PIC_DELAY();
	outb ( SLAVES_OCW, PICS_ICW4 );
	PIC_DELAY();

#if 0
	outb ( SLAVES_OCW, PICS_OCW1 );
	PIC_DELAY();
	outb ( SLAVES_ICW, PICS_OCW3 );
	PIC_DELAY();

	/*
	** 6. Initialise interrupts
	*/
	outb ( MASTER_OCW, PICM_OCW1 );
	PIC_DELAY();
#endif

	outb(MASTER_OCW, orig_pic_mask);
	PIC_DELAY();
	outb(SLAVES_OCW, orig_pic_mask >> 8);
	PIC_DELAY();

#if 0
	/* XXX */
	if (master_base != 8)
	{
		outb(0x21, 0xff);
		PIC_DELAY();
		outb(0xa1, 0xff);
		PIC_DELAY();
	}
#endif

	outb(MASTER_ICW, NON_SPEC_EOI);
	PIC_DELAY();
	outb(SLAVES_ICW, NON_SPEC_EOI);
	PIC_DELAY();

	inb(0x60);

}

/*
** form_pic_mask(int_lvl) 
**
**	For a given interrupt priority level (int_lvl), this routine goes out 
** and scans through the interrupt level table, and forms a mask based on the
** entries it finds there that have the same or lower interrupt priority level
** as (int_lvl). It returns a 16-bit mask which will have to be split up between
** the 2 pics.
**
*/

#define SLAVEMASK       (0xFFFF ^ SLAVE_ON_IR2)
#define SLAVEACTV	0xFF00

form_pic_mask()
{
	unsigned short i, j, bit, mask;

	for (i=SPL0; i <= SPLHI; i++) {
	 	for (j=0x00, bit=0x01, mask = 0; j < NINTR; j++, bit<<=1)
			if (intpri[j] <= i)
				mask |= bit;

	 	if ((mask & SLAVEACTV) != SLAVEACTV )	
			mask &= SLAVEMASK;

		pic_mask[i] = mask;
	}
}

#if 0

intnull(unit_dev)
{
	printf("intnull(%d)\n", unit_dev);
}

int prtnull_count = 0;
prtnull(unit)
{
	++prtnull_count;
}

#endif 0
