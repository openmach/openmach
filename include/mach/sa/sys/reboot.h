/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990 Carnegie Mellon University
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
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)reboot.h	7.5 (Berkeley) 6/27/88
 */
/*
 * Warning: The contents of this file are deprecated;
 * it should only ever be used for BSD and Mach 3 compatibility.
 * As the above copyright notice suggests, this file originated in BSD;
 * it is mostly the same, except the flags after RB_DFLTROOT
 * have diverged from BSD.
 */
#ifndef	_MACH_SYS_REBOOT_H_
#define	_MACH_SYS_REBOOT_H_

/*
 * Arguments to reboot system call.
 * These are converted to switches, and passed to startup program,
 * and on to init.
 */
#define	RB_AUTOBOOT	0	/* flags for system auto-booting itself */

#define	RB_ASKNAME	0x01	/* -a: ask for file name to reboot from */
#define	RB_SINGLE	0x02	/* -s: reboot to single user only */
#define	RB_KDB		0x04	/* -d: kernel debugger symbols loaded */
#define	RB_HALT		0x08	/* -h: enter KDB at bootup */
				/*     for host_reboot(): don't reboot,
				       just halt */
#define	RB_INITNAME	0x10	/* -i: name given for /etc/init (unused) */
#define	RB_DFLTROOT	0x20	/*     use compiled-in rootdev */
#define	RB_NOBOOTRC	0x20	/* -b: don't run /etc/rc.boot */
#define RB_ALTBOOT	0x40	/*     use /boot.old vs /boot */
#define	RB_UNIPROC	0x80	/* -u: start only one processor */

#define	RB_SHIFT	8	/* second byte is for ux */

#define	RB_DEBUGGER	0x1000	/*     for host_reboot(): enter kernel
				       debugger from user level */

/* Corresponding BSD definitions, where they disagree with the Mach flags.  */
#define BSD_RB_NOSYNC   0x04    /* dont sync before reboot */
#define BSD_RB_KDB      0x40    /* give control to kernel debugger */
#define BSD_RB_RDONLY   0x80    /* mount root fs read-only */
#define BSD_RB_DUMP     0x100   /* dump kernel memory before reboot */
#define BSD_RB_MINIROOT 0x200   /* mini-root present in memory at boot time */
#define BSD_RB_CONFIG   0x400   /* invoke user configuration routing */


/*
 * Constants for converting boot-style device number to type,
 * adaptor (uba, mba, etc), unit number and partition number.
 * Type (== major device number) is in the low byte
 * for backward compatibility.  Except for that of the "magic
 * number", each mask applies to the shifted value.
 * Format:
 *	 (4) (4) (4) (4)  (8)     (8)
 *	--------------------------------
 *	|MA | AD| CT| UN| PART  | TYPE |
 *	--------------------------------
 */
#define	B_ADAPTORSHIFT		24
#define	B_ADAPTORMASK		0x0f
#define	B_ADAPTOR(val)		(((val) >> B_ADAPTORSHIFT) & B_ADAPTORMASK)
#define B_CONTROLLERSHIFT	20
#define B_CONTROLLERMASK	0xf
#define	B_CONTROLLER(val)	(((val)>>B_CONTROLLERSHIFT) & B_CONTROLLERMASK)
#define B_UNITSHIFT		16
#define B_UNITMASK		0xf
#define	B_UNIT(val)		(((val) >> B_UNITSHIFT) & B_UNITMASK)
#define B_PARTITIONSHIFT	8
#define B_PARTITIONMASK		0xff
#define	B_PARTITION(val)	(((val) >> B_PARTITIONSHIFT) & B_PARTITIONMASK)
#define	B_TYPESHIFT		0
#define	B_TYPEMASK		0xff
#define	B_TYPE(val)		(((val) >> B_TYPESHIFT) & B_TYPEMASK)

#define	B_MAGICMASK	((u_int)0xf0000000U)
#define	B_DEVMAGIC	((u_int)0xa0000000U)

#define MAKEBOOTDEV(type, adaptor, controller, unit, partition) \
	(((type) << B_TYPESHIFT) | ((adaptor) << B_ADAPTORSHIFT) | \
	((controller) << B_CONTROLLERSHIFT) | ((unit) << B_UNITSHIFT) | \
	((partition) << B_PARTITIONSHIFT) | B_DEVMAGIC)

#endif	/* _MACH_SYS_REBOOT_H_ */
