#ifndef _NCR53C406A_H
#define _NCR53C406A_H

/*
 *  NCR53c406a.h
 * 
 *  Copyright (C) 1994 Normunds Saumanis (normunds@rx.tech.swh.lv)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 */

#ifndef NULL
#define NULL 0
#endif

/* NOTE:  scatter-gather support only works in PIO mode.
 * Use SG_NONE if DMA mode is enabled!
 */
#define NCR53c406a { \
     NULL			/* next */, \
     NULL			/* usage count */, \
     &proc_scsi_NCR53c406a      /* proc_dir */, \
     NULL			/* proc_info */, \
     "NCR53c406a"		/* name */, \
     NCR53c406a_detect		/* detect */, \
     NULL			/* release */, \
     NCR53c406a_info		/* info */, \
     NCR53c406a_command		/* command */, \
     NCR53c406a_queue		/* queuecommand */, \
     NCR53c406a_abort		/* abort */, \
     NCR53c406a_reset		/* reset */, \
     NULL			/* slave_attach */, \
     NCR53c406a_biosparm	/* biosparm */, \
     1				/* can_queue */, \
     7				/* SCSI ID of the chip */, \
     32				/*SG_ALL*/ /*SG_NONE*/, \
     1				/* commands per lun */, \
     0				/* number of boards in system */, \
     1				/* unchecked_isa_dma */, \
     ENABLE_CLUSTERING \
}

extern struct proc_dir_entry proc_scsi_NCR53c406a;

int NCR53c406a_detect(Scsi_Host_Template *);
const char* NCR53c406a_info(struct Scsi_Host *);

int NCR53c406a_command(Scsi_Cmnd *);
int NCR53c406a_queue(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int NCR53c406a_abort(Scsi_Cmnd *);
int NCR53c406a_reset(Scsi_Cmnd *);
int NCR53c406a_biosparm(Disk *, kdev_t, int []);

#endif /* _NCR53C406A_H */

/*
 * Overrides for Emacs so that we get a uniform tabbing style.
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

