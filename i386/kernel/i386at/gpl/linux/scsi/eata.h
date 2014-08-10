/*
 *	eata.h - used by the low-level driver for EATA/DMA SCSI host adapters.
 *
 */
#ifndef _EATA_H
#define _EATA_H

#include <linux/scsicam.h>

int eata2x_detect(Scsi_Host_Template *);
int eata2x_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int eata2x_abort(Scsi_Cmnd *);
int eata2x_reset(Scsi_Cmnd *);

#define EATA_VERSION "2.01.00"


#define EATA {                                                 \
		NULL, /* Ptr for modules */                    \
		NULL, /* usage count for modules */	       \
		NULL,                                          \
		NULL,                                          \
		"EATA/DMA 2.0x rev. " EATA_VERSION " ",        \
		eata2x_detect,				       \
		NULL, /* Release */     		       \
		NULL,					       \
		NULL,    			       	       \
		eata2x_queuecommand,			       \
		eata2x_abort,				       \
		eata2x_reset,				       \
		NULL,					       \
		scsicam_bios_param,   			       \
		0,   /* can_queue, reset by detect */          \
		7,   /* this_id, reset by detect */            \
		0,   /* sg_tablesize, reset by detect */       \
		0,   /* cmd_per_lun, reset by detect */        \
		0,   /* number of boards present */            \
		1,   /* unchecked isa dma, reset by detect */  \
		ENABLE_CLUSTERING                              \
		}
#endif
