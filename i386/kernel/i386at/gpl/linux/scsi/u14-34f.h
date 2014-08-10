/*
 *   u14-34f.h - used by the low-level driver for UltraStor 14F/34F
 */
#ifndef _U14_34F_H
#define _U14_34F_H

int u14_34f_detect(Scsi_Host_Template *);
int u14_34f_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int u14_34f_abort(Scsi_Cmnd *);
int u14_34f_reset(Scsi_Cmnd *);
int u14_34f_biosparam(Disk *, kdev_t, int *);

#define U14_34F_VERSION "2.01.00"

#define ULTRASTOR_14_34F {                                            \
		NULL, /* Ptr for modules */                           \
		NULL, /* usage count for modules */		      \
		NULL,		                                      \
		NULL,		                                      \
		"UltraStor 14F/34F rev. " U14_34F_VERSION " ",        \
		u14_34f_detect,                                       \
		NULL, /* Release */                                   \
		NULL,						      \
		NULL,                                                 \
		u14_34f_queuecommand,                                 \
		u14_34f_abort,                                        \
		u14_34f_reset,                                        \
		NULL,                                                 \
		u14_34f_biosparam,                                    \
		0,   /* can_queue, reset by detect */                 \
		7,   /* this_id, reset by detect */                   \
		0,   /* sg_tablesize, reset by detect */              \
		0,   /* cmd_per_lun, reset by detect */               \
		0,   /* number of boards present */                   \
		1,   /* unchecked isa dma, reset by detect */         \
		ENABLE_CLUSTERING                                     \
		}
#endif
