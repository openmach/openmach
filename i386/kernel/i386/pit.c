/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
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

#include <platforms.h>
#include <kern/time_out.h>
#include <i386/ipl.h>
#include <i386/pit.h>

int pitctl_port  = PITCTL_PORT;		/* For 386/20 Board */
int pitctr0_port = PITCTR0_PORT;	/* For 386/20 Board */
int pitctr1_port = PITCTR1_PORT;	/* For 386/20 Board */
int pitctr2_port = PITCTR2_PORT;	/* For 386/20 Board */
/* We want PIT 0 in square wave mode */

int pit0_mode = PIT_C0|PIT_SQUAREMODE|PIT_READMODE ;


unsigned int delaycount;		/* loop count in trying to delay for
					 * 1 millisecond
					 */
unsigned long microdata=50;		/* loop count for 10 microsecond wait.
					   MUST be initialized for those who
					   insist on calling "tenmicrosec"
					   it before the clock has been
					   initialized.
					 */
unsigned int clknumb = CLKNUM;		/* interrupt interval for timer 0 */

#ifdef PS2
extern int clock_int_handler();

#include <sys/types.h>
#include <i386ps2/abios.h>
static struct generic_request *clock_request_block;
static int     clock_flags;
char cqbuf[200];        /*XXX temporary.. should use kmem_alloc or whatever..*/
#endif  /* PS2 */

clkstart()
{
	unsigned int	flags;
	unsigned char	byte;
	int s;

	intpri[0] = SPLHI;
	form_pic_mask();

	findspeed();
	microfind();
	s = sploff();         /* disable interrupts */

#ifdef	PS2
        abios_clock_start();
#endif /* PS2 */

	/* Since we use only timer 0, we program that.
	 * 8254 Manual specifically says you do not need to program
	 * timers you do not use
	 */
	outb(pitctl_port, pit0_mode);
	clknumb = CLKNUM/hz;
	byte = clknumb;
	outb(pitctr0_port, byte);
	byte = clknumb>>8;
	outb(pitctr0_port, byte); 
	splon(s);         /* restore interrupt state */
}

#define COUNT   10000   /* should be a multiple of 1000! */

findspeed()
{
	unsigned int flags;
	unsigned char byte;
	unsigned int leftover;
	int i;
	int j;
	int s;

	s = sploff();                 /* disable interrupts */
	/* Put counter in count down mode */
#define PIT_COUNTDOWN PIT_READMODE|PIT_NDIVMODE
	outb(pitctl_port, PIT_COUNTDOWN);
	/* output a count of -1 to counter 0 */
	outb(pitctr0_port, 0xff);
	outb(pitctr0_port, 0xff);
	delaycount = COUNT;
	spinwait(1);
	/* Read the value left in the counter */
	byte = inb(pitctr0_port);	/* least siginifcant */
	leftover = inb(pitctr0_port);	/* most significant */
	leftover = (leftover<<8) + byte ;
	/* Formula for delaycount is :
	 *  (loopcount * timer clock speed)/ (counter ticks * 1000)
	 * 1000 is for figuring out milliseconds 
	 */
        /* we arrange calculation so that it doesn't overflow */
        delaycount = ((COUNT/1000) * CLKNUM) / (0xffff-leftover);
        printf("findspeed: delaycount=%d (tics=%d)\n",
	       delaycount, (0xffff-leftover));
	splon(s);         /* restore interrupt state */
}

#ifdef PS2

abios_clock_start()
{
        struct generic_request  temp_request_block;
        int rc;

        nmi_enable();   /* has to happen somewhere! */
        temp_request_block.r_current_req_blck_len = ABIOS_MIN_REQ_SIZE;
        temp_request_block.r_logical_id = abios_next_LID(SYSTIME_ID,
                                                        ABIOS_FIRST_LID);
        temp_request_block.r_unit = 0;
        temp_request_block.r_function = ABIOS_LOGICAL_PARAMETER;
        temp_request_block.r_return_code = ABIOS_UNDEFINED;

        abios_common_start(&temp_request_block,0);
        if (temp_request_block.r_return_code != ABIOS_DONE) {
                panic("couldn init abios time code!\n");
	      }

        /*
         * now build the clock request for the hardware system clock
         */
        clock_request_block = (struct generic_request *)cqbuf;
        clock_request_block->r_current_req_blck_len =
                                temp_request_block.r_request_block_length;
        clock_request_block->r_logical_id = temp_request_block.r_logical_id;
        clock_request_block->r_unit = 0;
        clock_request_block->r_function = ABIOS_DEFAULT_INTERRUPT;
        clock_request_block->r_return_code = ABIOS_UNDEFINED;
        clock_flags = temp_request_block.r_logical_id_flags;
}

ackrtclock()
{
        if (clock_request_block) {
	  clock_request_block->r_return_code = ABIOS_UNDEFINED;
	  abios_common_interrupt(clock_request_block,clock_flags);
	}
      }
#endif /* PS2 */


spinwait(millis)
	int millis;		/* number of milliseconds to delay */
{
	int i, j;

	for (i=0;i<millis;i++)
		for (j=0;j<delaycount;j++)
			;
}

#define MICROCOUNT      1000    /* keep small to prevent overflow */
microfind()
{
	unsigned int flags;
	unsigned char byte;
	unsigned short leftover;
	int s;


	s = sploff();                 /* disable interrupts */

	/* Put counter in count down mode */
	outb(pitctl_port, PIT_COUNTDOWN);
	/* output a count of -1 to counter 0 */
	outb(pitctr0_port, 0xff);
	outb(pitctr0_port, 0xff);
	microdata=MICROCOUNT;
	tenmicrosec();
	/* Read the value left in the counter */
	byte = inb(pitctr0_port);	/* least siginifcant */
	leftover = inb(pitctr0_port);	/* most significant */
	leftover = (leftover<<8) + byte ;
	/* Formula for delaycount is :
	 *  (loopcount * timer clock speed)/ (counter ticks * 1000)
	 *  Note also that 1000 is for figuring out milliseconds
	 */
        microdata = (MICROCOUNT * CLKNUM) / ((0xffff-leftover)*100000);
	if (!microdata)
		microdata++;

	splon(s);         /* restore interrupt state */
}
