/*
 *	sd.h Copyright (C) 1992 Drew Eckhardt 
 *	SCSI disk driver header file by
 *		Drew Eckhardt 
 *
 *	<drew@colorado.edu>
 *
 *	 Modified by Eric Youngdale eric@aib.com to
 *	 add scatter-gather, multiple outstanding request, and other
 *	 enhancements.
 */
#ifndef _SD_H
#define _SD_H
/*
    $Header: /n/fast/usr/lsrc/mach/CVS/mach4-i386/kernel/i386at/gpl/linux/scsi/sd.h,v 1.1 1996/03/25 20:25:47 goel Exp $
*/

#ifndef _SCSI_H
#include "scsi.h"
#endif

#ifndef _GENDISK_H
#include <linux/genhd.h>
#endif

extern struct hd_struct * sd;

typedef struct scsi_disk {
    unsigned capacity;		    /* size in blocks */
    unsigned sector_size;	    /* size in bytes */
    Scsi_Device	 *device;	    
    unsigned char ready;	    /* flag ready for FLOPTICAL */
    unsigned char write_prot;	    /* flag write_protect for rmvable dev */
    unsigned char sector_bit_size;  /* sector_size = 2 to the  bit size power */
    unsigned char sector_bit_shift; /* power of 2 sectors per FS block */
    unsigned ten:1;		    /* support ten byte read / write */
    unsigned remap:1;		    /* support remapping  */
    unsigned has_part_table:1;	    /* has partition table */
} Scsi_Disk;

extern Scsi_Disk * rscsi_disks;

extern int revalidate_scsidisk(kdev_t dev, int maxusage);

#endif

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */

