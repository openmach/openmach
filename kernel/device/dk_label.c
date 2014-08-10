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
 *	File: rz_disk.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <device/device_types.h>
#include <device/disk_status.h>

/* Checksum a disk label */
unsigned
dkcksum(lp)
	struct disklabel *lp;
{
	register unsigned short *start, *end, sum = 0;

	start = (unsigned short *)lp;
	end = (unsigned short*)&lp->d_partitions[lp->d_npartitions];
	while (start < end) sum ^= *start++;
	return sum;
}

/* Perform some checks and then copy a disk label */
setdisklabel(lp, nlp)
	struct disklabel *lp, *nlp;
{
	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    (dkcksum(nlp) != 0))
		return D_INVALID_OPERATION;
	*lp = *nlp;
	return D_SUCCESS;
}

dkgetlabel(lp, flavor, data, count)
	struct disklabel *lp;
	int		flavor;
	int *		data;		/* pointer to OUT array */
	unsigned int	*count;		/* OUT */
{

	switch (flavor) {
	/* get */
	case DIOCGDINFO:
		*(struct disklabel *)data = *lp;
		*count = sizeof(struct disklabel)/sizeof(int);
		break;
	case DIOCGDINFO - (0x10<<16):
		*(struct disklabel *)data = *lp;
		*count = sizeof(struct disklabel)/sizeof(int) - 4;
		break;
	}
}

print_bsd_label(lp, str)
struct disklabel	*lp;
char			*str;
{
int i;
	printf("%s sectors %d, tracks %d, cylinders %d\n",
		str, lp->d_nsectors, lp->d_ntracks, lp->d_ncylinders);
	printf("%s secpercyl %d, secperunit %d, npartitions %d\n",
		str, lp->d_secpercyl, lp->d_secperunit, lp->d_npartitions);

	for (i = 0; i < lp->d_npartitions; i++) {
		printf("%s    %c: size = %d, offset = %d\n",
			str, 'a'+i,
			lp->d_partitions[i].p_size,
			lp->d_partitions[i].p_offset);
	}
}
