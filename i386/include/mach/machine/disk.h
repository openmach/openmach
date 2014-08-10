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

/*
 * disk.h
 */

#if	defined(__linux__) || defined(__masix__)
#define PART_DISK	4		/* partition number for entire disk */
#else
#define PART_DISK	2		/* partition number for entire disk */
#endif


/* driver ioctl() commands */

#define V_CONFIG        _IOW('v',1,union io_arg)/* Configure Drive */
#define V_REMOUNT       _IO('v',2)    		/* Remount Drive */
#define V_ADDBAD        _IOW('v',3,union io_arg)/* Add Bad Sector */
#define V_GETPARMS      _IOR('v',4,struct disk_parms)   /* Get drive/partition parameters */
#define V_FORMAT        _IOW('v',5,union io_arg)/* Format track(s) */
#define V_PDLOC		_IOR('v',6,int)		/* Ask driver where pdinfo is on disk */

#define V_ABS		_IOW('v',9,int)		/* set a sector for an absolute addr */
#define V_RDABS		_IOW('v',10,struct absio)/* Read a sector at an absolute addr */
#define V_WRABS		_IOW('v',11,struct absio)/* Write a sector to absolute addr */
#define V_VERIFY	_IOWR('v',12,union vfy_io)/* Read verify sector(s) */
#define V_XFORMAT	_IO('v',13)		/* Selectively mark sectors as bad */
#define V_SETPARMS	_IOW('v',14,int)	/* Set drivers parameters */


/*
 * Data structure for the V_VERIFY ioctl
 */
union	vfy_io	{
	struct	{
		long	abs_sec;	/* absolute sector number        */
		u_short	num_sec;	/* number of sectors to verify   */
		u_short	time_flg;	/* flag to indicate time the ops */
		}vfy_in;
	struct	{
		long	deltatime;	/* duration of operation */
		u_short	err_code;	/* reason for failure    */
		}vfy_out;
};


/* data structure returned by the Get Parameters ioctl: */
struct  disk_parms {
/*00*/	char	dp_type;		/* Disk type (see below) */
	u_char	dp_heads;		/* Number of heads */
	u_short	dp_cyls;		/* Number of cylinders */
/*04*/	u_char	dp_sectors;		/* Number of sectors/track */
	u_short	dp_secsiz;		/* Number of bytes/sector */
					/* for this partition: */
/*08*/	u_short	dp_ptag;		/* Partition tag */
	u_short	dp_pflag;		/* Partition flag */
/*0c*/	long	dp_pstartsec;		/* Starting absolute sector number */
/*10*/	long	dp_pnumsec;		/* Number of sectors */
/*14*/	u_char	dp_dosheads;		/* Number of heads */
	u_short	dp_doscyls;		/* Number of cylinders */
/*18*/	u_char	dp_dossectors;		/* Number of sectors/track */
};

/* Disk types for disk_parms.dp_type: */
#define DPT_WINI        1               /* Winchester disk */
#define DPT_FLOPPY      2               /* Floppy */
#define DPT_OTHER       3               /* Other type of disk */
#define DPT_NOTDISK     0               /* Not a disk device */

/* Data structure for V_RDABS/V_WRABS ioctl's */
struct absio {
	long	abs_sec;		/* Absolute sector number (from 0) */
	char	*abs_buf;		/* Sector buffer */
};

