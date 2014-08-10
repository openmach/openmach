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
 * cram.h
 */

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
 * outb(CMOS_ADDR, addr);
 * result = inb(CMOS_DATA);
 * 
 * where "addr" tells what value you want to read (some are listed 
 * below).  Interrupts should be disabled while you do this.
 */

/* I/O ports */

#define CMOS_ADDR	0x70		/* port for CMOS ram address */
#define CMOS_DATA	0x71		/* port for CMOS ram data */


/* Addresses, related masks, and potential results */

#define CMOS_EB		0x14		/* read Equipment Byte */
#define CM_SCRMSK	0x30		/* mask for EB query to get screen */
#define CM_EGA_VGA	0x00		/* "not CGA or MONO" */
#define CM_CGA_40	0x10
#define CM_CGA_80	0x20
#define CM_MONO_80	0x30

