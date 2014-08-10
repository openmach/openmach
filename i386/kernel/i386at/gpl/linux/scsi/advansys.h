/* $Id: advansys.h,v 1.1 1996/03/25 20:25:17 goel Exp $ */
/*
 * advansys.h - Linux Host Driver for AdvanSys SCSI Adapters
 *
 * Copyright (c) 1995-1996 Advanced System Products, Inc.
 *
 * This driver may be modified and freely distributed provided that
 * the above copyright message and this comment are included in the
 * distribution. The latest version of this driver is available at
 * the AdvanSys FTP and BBS sites listed below.
 *
 * Please send questions, comments, and bug reports to:
 * bobf@advansys.com (Bob Frey)
 */

#ifndef _ADVANSYS_H
#define _ADVANSYS_H

/* The driver can be used in Linux 1.2.X or 1.3.X. */
#if !defined(LINUX_1_2) && !defined(LINUX_1_3)
#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif /* LINUX_VERSION_CODE */
#if LINUX_VERSION_CODE > 65536 + 3 * 256
#define LINUX_1_3
#else /* LINUX_VERSION_CODE */
#define LINUX_1_2
#endif /* LINUX_VERSION_CODE */
#endif /* !defined(LINUX_1_2) && !defined(LINUX_1_3) */

/*
 * Scsi_Host_Template function prototypes.
 */
int advansys_detect(Scsi_Host_Template *);
int advansys_release(struct Scsi_Host *);
const char *advansys_info(struct Scsi_Host *);
int advansys_command(Scsi_Cmnd *);
int advansys_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int advansys_abort(Scsi_Cmnd *);
int advansys_reset(Scsi_Cmnd *);
#ifdef LINUX_1_2
int advansys_biosparam(Disk *, int, int[]);
#else /* LINUX_1_3 */
int advansys_biosparam(Disk *, kdev_t, int[]);
extern struct proc_dir_entry proc_scsi_advansys;
int advansys_proc_info(char *, char **, off_t, int, int, int);
#endif /* LINUX_1_3 */

/* init/main.c setup function */
void advansys_setup(char *, int *);

/*
 * AdvanSys Host Driver Scsi_Host_Template (struct SHT) from hosts.h.
 */
#ifdef LINUX_1_2
#define ADVANSYS { \
	NULL,					/* struct SHT *next */ \
	NULL,					/* int *usage_count */ \
	"advansys",				/* char *name */ \
	advansys_detect,		/* int (*detect)(struct SHT *) */ \
	advansys_release,		/* int (*release)(struct Scsi_Host *) */ \
	advansys_info,			/* const char *(*info)(struct Scsi_Host *) */ \
	advansys_command, 		/* int (*command)(Scsi_Cmnd *) */ \
	advansys_queuecommand, \
			/* int (*queuecommand)(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *)) */ \
	advansys_abort,			/* int (*abort)(Scsi_Cmnd *) */ \
	advansys_reset,			/* int (*reset)(Scsi_Cmnd *) */ \
	NULL,					/* int (*slave_attach)(int, int) */ \
	advansys_biosparam,		/* int (* bios_param)(Disk *, int, int []) */ \
	/* \
	 * The following fields are set per adapter in advansys_detect(). \
	 */ \
	0,						/* int can_queue */ \
	0,						/* int this_id */ \
	0,						/* short unsigned int sg_tablesize */ \
	0,						/* short cmd_per_lun */ \
	0,						/* unsigned char present */	\
	/* \
	 * Because the driver may control an ISA adapter 'unchecked_isa_dma' \
	 * must be set. The flag will be cleared in advansys_detect for non-ISA \
	 * adapters. Refer to the comment in scsi_module.c for more information. \
	 */ \
	1,						/* unsigned unchecked_isa_dma:1 */ \
	/* \
	 * All adapters controlled by this driver are capable of large \
	 * scatter-gather lists. This apparently obviates any performance
	 * gain provided by setting 'use_clustering'. \
	 */ \
	DISABLE_CLUSTERING,		/* unsigned use_clustering:1 */ \
}
#else /* LINUX_1_3 */
#define ADVANSYS { \
	NULL,					/* struct SHT *next */ \
	NULL,					/* long *usage_count */ \
	&proc_scsi_advansys,	/* struct proc_dir_entry *proc_dir */ \
	advansys_proc_info,	\
			/* int (*proc_info)(char *, char **, off_t, int, int, int) */ \
	"advansys",				/* const char *name */ \
	advansys_detect,		/* int (*detect)(struct SHT *) */ \
	advansys_release,		/* int (*release)(struct Scsi_Host *) */ \
	advansys_info,			/* const char *(*info)(struct Scsi_Host *) */ \
	advansys_command, 		/* int (*command)(Scsi_Cmnd *) */ \
	advansys_queuecommand, \
			/* int (*queuecommand)(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *)) */ \
	advansys_abort,			/* int (*abort)(Scsi_Cmnd *) */ \
	advansys_reset,			/* int (*reset)(Scsi_Cmnd *) */ \
	NULL,					/* int (*slave_attach)(int, int) */ \
	advansys_biosparam,		/* int (* bios_param)(Disk *, kdev_t, int []) */ \
	/* \
	 * The following fields are set per adapter in advansys_detect(). \
	 */ \
	0,						/* int can_queue */ \
	0,						/* int this_id */ \
	0,						/* short unsigned int sg_tablesize */ \
	0,						/* short cmd_per_lun */ \
	0,						/* unsigned char present */	\
	/* \
	 * Because the driver may control an ISA adapter 'unchecked_isa_dma' \
	 * must be set. The flag will be cleared in advansys_detect for non-ISA \
	 * adapters. Refer to the comment in scsi_module.c for more information. \
	 */ \
	1,						/* unsigned unchecked_isa_dma:1 */ \
	/* \
	 * All adapters controlled by this driver are capable of large \
	 * scatter-gather lists. This apparently obviates any performance
	 * gain provided by setting 'use_clustering'. \
	 */ \
	DISABLE_CLUSTERING,		/* unsigned use_clustering:1 */ \
}
#endif /* LINUX_1_3 */
#endif /* _ADVANSYS_H */
