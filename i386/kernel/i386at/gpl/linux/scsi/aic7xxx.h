/*+M*************************************************************************
 * Adaptec 274x/284x/294x device driver for Linux.
 *
 * Copyright (c) 1994 John Aycock
 *   The University of Calgary Department of Computer Science.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * $Id: aic7xxx.h,v 1.1 1996/03/25 20:25:23 goel Exp $
 *-M*************************************************************************/
#ifndef _aic7xxx_h
#define _aic7xxx_h

#define AIC7XXX_H_VERSION  "$Revision: 1.1 $"

/*
 * Scsi_Host_Template (see hosts.h) for AIC-7770/AIC-7870 - some fields
 * to do with card config are filled in after the card is detected.
 */
#define AIC7XXX	{						\
	NULL,							\
	NULL,							\
	NULL,							\
	aic7xxx_proc_info,					\
	NULL,							\
	aic7xxx_detect,						\
	NULL,							\
	aic7xxx_info,						\
	NULL,							\
	aic7xxx_queue,						\
	aic7xxx_abort,						\
	aic7xxx_reset,						\
	NULL,							\
	aic7xxx_biosparam,					\
	-1,			/* max simultaneous cmds      */\
	-1,			/* scsi id of host adapter    */\
	SG_ALL,			/* max scatter-gather cmds    */\
	2,			/* cmds per lun (linked cmds) */\
	0,			/* number of 7xxx's present   */\
	0,			/* no memory DMA restrictions */\
	ENABLE_CLUSTERING					\
}

extern int aic7xxx_queue(Scsi_Cmnd *, void (*)(Scsi_Cmnd *));
extern int aic7xxx_biosparam(Disk *, kdev_t, int[]);
extern int aic7xxx_detect(Scsi_Host_Template *);
extern int aic7xxx_command(Scsi_Cmnd *);
extern int aic7xxx_abort(Scsi_Cmnd *);
extern int aic7xxx_reset(Scsi_Cmnd *);

extern const char *aic7xxx_info(struct Scsi_Host *);

extern int aic7xxx_proc_info(char *, char **, off_t, int, int, int);

#endif /* _aic7xxx_h */
