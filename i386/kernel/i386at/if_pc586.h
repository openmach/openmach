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
 *	Olivetti PC586 Mach Ethernet driver v1.0
 *	Copyright Ing. C. Olivetti & C. S.p.A. 1988, 1989
 *	All rights reserved.
 *
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

#include	<i386at/i82586.h>	/* chip/board specific defines	*/

#define	STATUS_TRIES	15000
#define	ETHER_ADD_SIZE	6	/* size of a MAC address 		*/
#define ETHER_PCK_SIZE	1500	/* maximum size of an ethernet packet 	*/

/*
 * 	Board Specific Defines:
 */

#define OFFSET_NORMMODE	0x3000
#define OFFSET_CHANATT	0x3002
#define OFFSET_RESET   	0x3004
#define OFFSET_INTENAB	0x3006
#define OFFSET_XFERMODE	0x3008
#define OFFSET_SYSTYPE	0x300a
#define OFFSET_INTSTAT	0x300c
#define OFFSET_PROM	0x2000 

#define EXTENDED_ADDR	0x20000
#define OFFSET_SCP      (0x7ff6 - 0x4000)
#define OFFSET_ISCP     (0x7fee - 0x4000)
#define OFFSET_SCB      (0x7fde - 0x4000)
#define OFFSET_RU       (0x4000 - 0x4000)
#define OFFSET_RBD	(0x4228 - 0x4000)
#define OFFSET_CU       (0x7814 - 0x4000)

#define OFFSET_TBD      (0x7914 - 0x4000)
#define OFFSET_TBUF     (0x79a4 - 0x4000)
#define N_FD              25  
#define N_RBD             25 
#define N_TBD		  18
#define RCVBUFSIZE       540
#define DL_DEAD         0xffff

#define CMD_0           0     
#define CMD_1           0xffff

#define PC586NULL	0xffff			/* pc586 NULL for lists	*/

#define	DSF_LOCK	1
#define DSF_RUNNING	2

#define MOD_ENAL 1
#define MOD_PROM 2

/*
 * Driver (not board) specific defines and structures:
 */

typedef	struct	{
	rbd_t	r;
	char	rbd_pad[2];
	char	rbuffer[RCVBUFSIZE];
} ru_t;

#ifdef	MACH_KERNEL
#else	MACH_KERNEL
#ifndef TRUE
#define TRUE            1     
#endif 	TRUE
#define	HZ		100
#endif	MACH_KERNEL
