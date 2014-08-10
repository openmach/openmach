#ifndef _WD7000_H

/* $Id: wd7000.h,v 1.1 1996/03/25 20:25:57 goel Exp $
 *
 * Header file for the WD-7000 driver for Linux
 *
 * John Boyd <boyd@cis.ohio-state.edu>  Jan 1994:
 * This file has been reduced to only the definitions needed for the
 * WD7000 host structure.
 *
 */

#include <linux/types.h>
#include <linux/kdev_t.h>

int wd7000_detect(Scsi_Host_Template *);
int wd7000_command(Scsi_Cmnd *);
int wd7000_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int wd7000_abort(Scsi_Cmnd *);
int wd7000_reset(Scsi_Cmnd *);
int wd7000_biosparam(Disk *, kdev_t, int *);

#ifndef NULL
#define NULL 0L
#endif

/*
 *  In this version, sg_tablesize now defaults to WD7000_SG, and will
 *  be set to SG_NONE for older boards.  This is the reverse of the
 *  previous default, and was changed so that the driver-level
 *  Scsi_Host_Template would reflect the driver's support for scatter/
 *  gather.
 *
 *  Also, it has been reported that boards at Revision 6 support scatter/
 *  gather, so the new definition of an "older" board has been changed
 *  accordingly.
 */
#define WD7000_Q    16
#define WD7000_SG   16

#define WD7000 { NULL, NULL,            \
	NULL,		                \
	NULL,		                \
	"Western Digital WD-7000",      \
	wd7000_detect,                  \
	NULL,				\
	NULL,				\
	wd7000_command,			\
	wd7000_queuecommand,		\
	wd7000_abort,			\
	wd7000_reset,			\
	NULL,                           \
	wd7000_biosparam,               \
	WD7000_Q, 7, WD7000_SG, 1, 0, 1, ENABLE_CLUSTERING}
#endif
