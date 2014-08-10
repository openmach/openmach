/*
 *      sr.h by David Giller
 *      CD-ROM disk driver header file
 *      
 *      adapted from:
 *	sd.h Copyright (C) 1992 Drew Eckhardt 
 *	SCSI disk driver header file by
 *		Drew Eckhardt 
 *
 *	<drew@colorado.edu>
 *
 *       Modified by Eric Youngdale eric@aib.com to
 *       add scatter-gather, multiple outstanding request, and other
 *       enhancements.
 */

#ifndef _SR_H
#define _SR_H

#include "scsi.h"

typedef struct
	{
	unsigned 	capacity;		/* size in blocks 			*/
	unsigned 	sector_size;		/* size in bytes 			*/
	Scsi_Device  	*device;		
	unsigned long   mpcd_sector;            /* for reading multisession-CD's        */
	char            xa_flags;               /* some flags for handling XA-CD's      */
	unsigned char	sector_bit_size;	/* sector size = 2^sector_bit_size	*/
	unsigned char	sector_bit_shift;	/* sectors/FS block = 2^sector_bit_shift*/
	unsigned 	needs_sector_size:1;   	/* needs to get sector size */
	unsigned 	ten:1;			/* support ten byte commands		*/
	unsigned 	remap:1;		/* support remapping			*/
	unsigned 	use:1;			/* is this device still supportable	*/
	unsigned	auto_eject:1;		/* auto-eject medium on last release.	*/
	} Scsi_CD;
	
extern Scsi_CD * scsi_CDs;

#endif
