#ifndef _QLOGIC_H
#define _QLOGIC_H

int qlogic_detect(Scsi_Host_Template * );
const char * qlogic_info(struct Scsi_Host *);
int qlogic_command(Scsi_Cmnd *);
int qlogic_queuecommand(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int qlogic_abort(Scsi_Cmnd *);
int qlogic_reset(Scsi_Cmnd *);
int qlogic_biosparam(Disk *, kdev_t, int[]);

#ifndef NULL
#define NULL (0)
#endif

#define QLOGIC {		\
	NULL,			\
	NULL,			\
	NULL,			\
	NULL,		        \
	NULL,			\
	qlogic_detect,		\
	NULL,			\
	qlogic_info,		\
	qlogic_command, 	\
	qlogic_queuecommand,	\
	qlogic_abort,		\
	qlogic_reset,		\
	NULL,			\
	qlogic_biosparam,	\
	0,			\
	-1,			\
	SG_ALL,			\
	1,			\
	0,			\
	0,			\
	DISABLE_CLUSTERING	\
}

#endif /* _QLOGIC_H */
