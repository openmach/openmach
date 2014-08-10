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
 *	File: rz_labels.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Definitions of various vendor's disk label formats.
 */

/* modified by Kevin T. Van Maren for the unified partition code */

#ifndef	_RZ_LABELS_H_
#define	_RZ_LABELS_H_

/*
 * This function looks for, and converts to BSD format
 * a vendor's label.  It is only called if we did not
 * find a standard BSD label on the disk pack.
 */
extern boolean_t	rz_vendor_label();

/*
 * Definition of the DEC disk label,
 * which is located (you guessed it)
 * at the end of the 4.3 superblock.
 */

struct dec_partition_info {
	unsigned int	n_sectors;	/* how big the partition is */
	unsigned int	offset;		/* sector no. of start of part. */
};

typedef struct {
	int	magic;
#	define	DEC_LABEL_MAGIC		0x032957
	int	in_use;
	struct  dec_partition_info partitions[8];
} dec_label_t;

/*
 * Physical location on disk.
 * This is independent of the filesystem we use,
 * although of course we'll be in trouble if we
 * screwup the 4.3 SBLOCK..
 */

#define	DEC_LABEL_BYTE_OFFSET	((2*8192)-sizeof(dec_label_t))


/*
 * Definitions for the primary boot information
 * This is common, cuz the prom knows it.
 */

typedef struct {
	int		pad[2];
	unsigned int	magic;
#	define		DEC_BOOT0_MAGIC	0x2757a
	int		mode;
	unsigned int	phys_base;
	unsigned int	virt_base;
	unsigned int	n_sectors;
	unsigned int	start_sector;
} dec_boot0_t;

typedef struct {
	dec_boot0_t	vax_boot;
					/* BSD label still fits in pad */
	char			pad[0x1e0-sizeof(dec_boot0_t)];
	unsigned long		block_count;
	unsigned long		starting_lbn;
	unsigned long		flags;
	unsigned long		checksum; /* add cmpl-2 all but here */
} alpha_boot0_t;

/*
 * Definition of the Omron disk label,
 * which is located at sector 0. It
 * _is_ sector 0, actually.
 */
struct omron_partition_info {
	unsigned long	offset;
	unsigned long	n_sectors;
};

typedef struct {
	char		packname[128];	/* in ascii */

	char		pad[512-(128+8*8+11*2+4)];

	unsigned short	badchk;	/* checksum of bad track */
	unsigned long	maxblk;	/* # of total logical blocks */
	unsigned short	dtype;	/* disk drive type */
	unsigned short	ndisk;	/* # of disk drives */
	unsigned short	ncyl;	/* # of data cylinders */
	unsigned short	acyl;	/* # of alternate cylinders */
	unsigned short	nhead;	/* # of heads in this partition */
	unsigned short	nsect;	/* # of 512 byte sectors per track */
	unsigned short	bhead;	/* identifies proper label locations */
	unsigned short	ppart;	/* physical partition # */
	struct omron_partition_info
			partitions[8];

	unsigned short	magic;	/* identifies this label format */
#	define	OMRON_LABEL_MAGIC	0xdabe

	unsigned short	cksum;	/* xor checksum of sector */

} omron_label_t;

/*
 * Physical location on disk.
 */

#define	OMRON_LABEL_BYTE_OFFSET	0


/*
 * Definition of the i386AT disk label, which lives inside sector 0.
 * This is the info the BIOS knows about, which we use for bootstrapping.
 * It is common across all disks known to BIOS.
 */

struct bios_partition_info {

	unsigned char	bootid;	/* bootable or not */
#	define BIOS_BOOTABLE	128

	unsigned char	beghead;/* beginning head, sector, cylinder */
	unsigned char	begsect;/* begcyl is a 10-bit number. High 2 bits */
	unsigned char	begcyl;	/*     are in begsect. */

	unsigned char	systid;	/* filesystem type */
#	define	UNIXOS		99	/* GNU HURD? */
#       define  BSDOS          165	/* 386BSD */
#       define  LINUXSWAP      130
#       define  LINUXOS        131
#       define  DOS_EXTENDED    05	/* container for logical partitions */

#	define	HPFS		07	/* OS/2 Native */
#	define	OS_2_BOOT	10	/* OS/2 Boot Manager */
#	define	DOS_12		01	/* 12 bit FAT */
#	define	DOS_16_OLD	04	/* < 32MB */
#	define	DOS_16		06	/* >= 32MB (#4 not used anymore) */

                                /* these numbers can't be trusted because */
                                /* of newer, larger drives */
	unsigned char	endhead;/* ending head, sector, cylinder */
	unsigned char	endsect;/* endcyl is a 10-bit number.  High 2 bits */
	unsigned char	endcyl;	/*     are in endsect. */

	unsigned long	offset;
	unsigned long	n_sectors;
};

typedef struct {
/*	struct bios_partition_info	bogus compiler alignes wrong
			partitions[4];
*/
	char		partitions[4*sizeof(struct bios_partition_info)];
	unsigned short	magic;
#	define	BIOS_LABEL_MAGIC	0xaa55
} bios_label_t;

/*
 * Physical location on disk.
 */

#define	BIOS_LABEL_BYTE_OFFSET	446

/*
 * Definitions for the primary boot information
 * This _is_ block 0
 */

#define	BIOS_BOOT0_SIZE	BIOS_LABEL_BYTE_OFFSET

typedef struct {
	char		boot0[BIOS_BOOT0_SIZE];	/* boot code */
/*	bios_label_t label;	bogus compiler alignes wrong */
	char		label[sizeof(bios_label_t)];
} bios_boot0_t;

/* Moved from i386at/nhdreg.h */
#define PDLOCATION      29      /* VTOC sector */


/* these are the partition types that can contain sub-partitions */
/* enum types... */
#define DISKPART_NONE   0       /* smallest piece flag !?! */
#define DISKPART_DOS    1 
#define DISKPART_BSD    2
#define DISKPART_VTOC   3
#define DISKPART_OMRON  4
#define DISKPART_DEC    5       /* VAX disks? */
#define DISKPART_UNKNOWN 99



/* for NEW partition code */
/* this is the basic partition structure.  an array of these is
    filled, with element 0 being the whole drive, element 1-n being
    the n top-level partitions, followed by 0+ groups of 1+ sub-partitions. */
typedef struct diskpart {
        short type;     /* DISKPART_xxx (see above) */
        short fsys;     /* file system (if known) */
        int nsubs;      /* number of sub-slices */
        struct diskpart *subs;  /* pointer to the sub-partitions */
        int start;      /* relative to the start of the DRIVE */
        int size;       /* # sectors in this piece */
} diskpart;

int get_only_partition(void *driver_info, int (*bottom_read_fun)(),
                        struct diskpart *array, int array_size,
                        int disk_size, char *drive_name);
        
struct diskpart *lookup_part(struct diskpart *array, int dev_number);
#endif	_RZ_LABELS_H_

