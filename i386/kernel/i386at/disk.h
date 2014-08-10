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

/* Grab the public part.  */
#include <mach/machine/disk.h>



#define MAX_ALTENTS     253		/* Maximum # of slots for alts */
					/* allowed for in the table. */

#define ALT_SANITY      0xdeadbeef      /* magic # to validate alt table */

struct  alt_table {
	u_short	alt_used;		/* # of alternates already assigned */
	u_short	alt_reserved;		/* # of alternates reserved on disk */
	long	alt_base;		/* 1st sector (abs) of the alt area */
	long	alt_bad[MAX_ALTENTS];	/* list of bad sectors/tracks	*/
};

struct alt_info {			/* table length should be multiple of 512 */
	long	alt_sanity;		/* to validate correctness */
	u_short	alt_version;		/* to corroborate vintage */
	u_short	alt_pad;		/* padding for alignment */
	struct alt_table alt_trk;	/* bad track table */
	struct alt_table alt_sec;	/* bad sector table */
};
typedef struct alt_info altinfo_t;

#define V_NUMPAR        16              /* maximum number of partitions */

#define VTOC_SANE       0x600DDEEE      /* Indicates a sane VTOC */
#define PDLOCATION	29		/* location of VTOC */

#define BAD_BLK         0x80                    /* needed for V_VERIFY */
/* BAD_BLK moved from old hdreg.h */


#define	HDPDLOC		29		/* location of pdinfo/vtoc */
#define	LBLLOC		1		/* label block for xxxbsd */

/* Partition permission flags */
#define V_OPEN          0x100           /* Partition open (for driver use) */
#define V_VALID         0x200           /* Partition is valid to use */



/* Sanity word for the physical description area */
#define VALID_PD		0xCA5E600D

struct localpartition	{
	u_int 	p_flag;			/*permision flags*/
	long	p_start;		/*physical start sector no of partition*/
	long	p_size;			/*# of physical sectors in partition*/
};
typedef struct localpartition localpartition_t;

struct evtoc {
	u_int 	fill0[6];
	u_int 	cyls;			/*number of cylinders per drive*/
	u_int 	tracks;			/*number tracks per cylinder*/
	u_int 	sectors;		/*number sectors per track*/
	u_int 	fill1[13];
	u_int 	version;		/*layout version*/
	u_int 	alt_ptr;		/*byte offset of alternates table*/
	u_short	alt_len;		/*byte length of alternates table*/
	u_int 	sanity;			/*to verify vtoc sanity*/
	u_int 	xcyls;			/*number of cylinders per drive*/
	u_int 	xtracks;		/*number tracks per cylinder*/
	u_int 	xsectors;		/*number sectors per track*/
	u_short	nparts;			/*number of partitions*/
	u_short	fill2;			/*pad for 286 compiler*/
	char	label[40];
	struct localpartition part[V_NUMPAR];/*partition headers*/
	char	fill[512-352];
};

union   io_arg {
	struct  {
		u_short	ncyl;		/* number of cylinders on drive */
		u_char	nhead;		/* number of heads/cyl */
		u_char	nsec;		/* number of sectors/track */
		u_short	secsiz;		/* number of bytes/sector */
		} ia_cd;		/* used for Configure Drive cmd */
	struct  {
		u_short	flags;		/* flags (see below) */
		long	bad_sector;	/* absolute sector number */
		long	new_sector;	/* RETURNED alternate sect assigned */
		} ia_abs;		/* used for Add Bad Sector cmd */
	struct  {
		u_short	start_trk;	/* first track # */
		u_short	num_trks;	/* number of tracks to format */
		u_short	intlv;		/* interleave factor */
		} ia_fmt;		/* used for Format Tracks cmd */
	struct	{
		u_short	start_trk;	/* first track	*/
		char	*intlv_tbl;	/* interleave table */
		} ia_xfmt;		/* used for the V_XFORMAT ioctl */
};


#define BOOTSZ		446		/* size of boot code in master boot block */
#define FD_NUMPART	4		/* number of 'partitions' in fdisk table */
#define ACTIVE		128		/* indicator of active partition */
#define	BOOT_MAGIC	0xAA55		/* signature of the boot record */
#define	UNIXOS		99		/* UNIX partition */
#define BSDOS		165
#define LINUXSWAP	130
#define LINUXOS		131
extern	int		OS;		/* what partition we came from */

/*
 * structure to hold the fdisk partition table
 */
struct ipart {
	u_char	bootid;			/* bootable or not */
	u_char	beghead;		/* beginning head, sector, cylinder */
	u_char	begsect;		/* begcyl is a 10-bit number. High 2 bits */
	u_char	begcyl;			/*     are in begsect. */
	u_char	systid;			/* OS type */
	u_char	endhead;		/* ending head, sector, cylinder */
	u_char	endsect;		/* endcyl is a 10-bit number.  High 2 bits */
	u_char	endcyl;			/*     are in endsect. */
	long	relsect;		/* first sector relative to start of disk */
	long	numsect;		/* number of sectors in partition */
};

/*
 * structure to hold master boot block in physical sector 0 of the disk.
 * Note that partitions stuff can't be directly included in the structure
 * because of lameo '386 compiler alignment design.
 */
struct  mboot {				     /* master boot block */
	char	bootinst[BOOTSZ];
	char	parts[FD_NUMPART * sizeof(struct ipart)];
	u_short	signature;
};

