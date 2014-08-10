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

#include <platforms.h>

#include <sys/types.h>
#include <i386/ipl.h>
#include <i386/pic.h>
#include <i386/machspl.h>

spl_t	curr_ipl;
int	pic_mask[NSPL];
int	curr_pic_mask;

int	iunit[NINTR] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

int 	nintr = NINTR;
int	npics = NPICS;

char	*master_icw, *master_ocw, *slaves_icw, *slaves_ocw;

u_short PICM_ICW1, PICM_OCW1, PICS_ICW1, PICS_OCW1 ;
u_short PICM_ICW2, PICM_OCW2, PICS_ICW2, PICS_OCW2 ;
u_short PICM_ICW3, PICM_OCW3, PICS_ICW3, PICS_OCW3 ;
u_short PICM_ICW4, PICS_ICW4 ;

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

picinit()
{

	u_short i;

	asm("cli");

	/*
	** 1. Form pic mask table
	*/
#if 0
	printf (" Let the console driver screw up this line ! \n");
#endif

	form_pic_mask();

	/*
	** 1a. Select current SPL.
	*/

	curr_ipl = SPLHI;
	curr_pic_mask = pic_mask[SPLHI];

	/*
	** 2. Generate addresses to each PIC port.
	*/

	master_icw = (char *)PIC_MASTER_ICW;
	master_ocw = (char *)PIC_MASTER_OCW;
	slaves_icw = (char *)PIC_SLAVE_ICW;
	slaves_ocw = (char *)PIC_SLAVE_OCW;

#ifdef	PS2
#else	/* PS2 */
	/*
	** 3. Select options for each ICW and each OCW for each PIC.
	*/

	PICM_ICW1 = 
 	(ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8 | CASCADE_MODE | ICW4__NEEDED);

	PICS_ICW1 = 
 	(ICW_TEMPLATE | EDGE_TRIGGER | ADDR_INTRVL8 | CASCADE_MODE | ICW4__NEEDED);

	PICM_ICW2 = PICM_VECTBASE;
	PICS_ICW2 = PICS_VECTBASE;

#ifdef	AT386
	PICM_ICW3 = ( SLAVE_ON_IR2 );
	PICS_ICW3 = ( I_AM_SLAVE_2 );
#endif	AT386
#ifdef	iPSC386
        PICM_ICW3 = ( SLAVE_ON_IR7 );
        PICS_ICW3 = ( I_AM_SLAVE_7 );
#endif	iPSC386

#ifdef	iPSC386
        /* Use Buffered mode for iPSC386 */
        PICM_ICW4 = (SNF_MODE_DIS | BUFFERD_MODE | I_AM_A_MASTR |
                     NRML_EOI_MOD | I8086_EMM_MOD);
        PICS_ICW4 = (SNF_MODE_DIS | BUFFERD_MODE | I_AM_A_SLAVE |
                     NRML_EOI_MOD | I8086_EMM_MOD);
#else	iPSC386
	PICM_ICW4 = 
 	(SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD | I8086_EMM_MOD);
	PICS_ICW4 = 
 	(SNF_MODE_DIS | NONBUFD_MODE | NRML_EOI_MOD | I8086_EMM_MOD);
#endif	iPSC386

	PICM_OCW1 = (curr_pic_mask & 0x00FF);
	PICS_OCW1 = ((curr_pic_mask & 0xFF00)>>8);

	PICM_OCW2 = NON_SPEC_EOI;
	PICS_OCW2 = NON_SPEC_EOI;

	PICM_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );
	PICS_OCW3 = (OCW_TEMPLATE | READ_NEXT_RD | READ_IR_ONRD );


	/* 
	** 4.	Initialise master - send commands to master PIC
	*/ 

	outb ( master_icw, PICM_ICW1 );
	outb ( master_ocw, PICM_ICW2 );
	outb ( master_ocw, PICM_ICW3 );
	outb ( master_ocw, PICM_ICW4 );

	outb ( master_ocw, PICM_MASK );
	outb ( master_icw, PICM_OCW3 );

	/*
	** 5.	Initialise slave - send commands to slave PIC
	*/

	outb ( slaves_icw, PICS_ICW1 );
	outb ( slaves_ocw, PICS_ICW2 );
	outb ( slaves_ocw, PICS_ICW3 );
	outb ( slaves_ocw, PICS_ICW4 );


	outb ( slaves_ocw, PICS_OCW1 );
	outb ( slaves_icw, PICS_OCW3 );

	/*
	** 6. Initialise interrupts
	*/
	outb ( master_ocw, PICM_OCW1 );

#endif	/* PS2 */

#if 0
	printf(" spl set to %x \n", curr_pic_mask);
#endif

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

#if	defined(AT386) || defined(PS2)
#define SLAVEMASK       (0xFFFF ^ SLAVE_ON_IR2)
#endif	/* defined(AT386) || defined(PS2) */
#ifdef	iPSC386
#define SLAVEMASK       (0xFFFF ^ SLAVE_ON_IR7)
#endif	iPSC386

#define SLAVEACTV	0xFF00

form_pic_mask()
{
	unsigned int i, j, bit, mask;

	for (i=SPL0; i < NSPL; i++) {
	 	for (j=0x00, bit=0x01, mask = 0; j < NINTR; j++, bit<<=1)
			if (intpri[j] <= i)
				mask |= bit;

	 	if ((mask & SLAVEACTV) != SLAVEACTV )	
			mask &= SLAVEMASK;

		pic_mask[i] = mask;
	}
}

intnull(unit_dev)
{
	printf("intnull(%d)\n", unit_dev);
}

int prtnull_count = 0;
prtnull(unit)
{
	++prtnull_count;
}
