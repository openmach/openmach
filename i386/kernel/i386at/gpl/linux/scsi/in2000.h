#ifndef _IN2000_H

/* $Id: in2000.h,v 1.1 1996/03/25 20:25:38 goel Exp $
 *
 * Header file for the Always IN 2000 driver for Linux
 *
 */

#include <linux/types.h>
#include <linux/ioport.h>

/* The IN-2000 is based on a WD33C93 */

#define	INSTAT	(base + 0x0)	/* R: Auxiliary Status; W: register select */
#define	INDATA	(base + 0x1)	/* R/W: Data port */
#define	INFIFO	(base + 0x2)	/* R/W FIFO, Word access only */
#define	INREST	(base + 0x3)	/* W: Reset everything */
#define	INFCNT	(base + 0x4)	/* R: FIFO byte count */
#define	INFRST	(base + 0x5)	/* W: Reset Fifo count and to write */
#define	INFWRT	(base + 0x7)	/* W: Set FIFO to read */
#define	INFLED	(base + 0x8)	/* W: Set LED; R: Dip Switch settings */
#define	INNLED	(base + 0x9)	/* W: reset LED */
#define	INVERS	(base + 0xa)	/* R: Read hw version, end-reset */
#define	ININTR	(base + 0xc)	/* W: Interrupt Mask Port */
#define G2CNTRL_HRDY	0x20		/* Sets HOST ready */

/* WD33C93 defines */
#define	OWNID	0
#undef	CONTROL
#define	CONTROL	1
#define	TIMEOUT	2
#define	TOTSECT	3
#define	TOTHEAD	4
#define	TOTCYLH 5
#define	TOTCYLL	6
#define	LADRSHH	7
#define	LADRSHL	8
#define	LADRSLH	9
#define	LADRSLL	10
#define	SECTNUM	11
#define	HEADNUM	12
#define	CYLNUMH	13
#define	CYLNUML	14
#define	TARGETU	15
#define	CMDPHAS	16
#define	SYNCTXR	17
#define	TXCNTH	18
#define	TXCNTM	19
#define TXCNTL	20
#define DESTID	21
#define	SRCID	22
#define	SCSIST	23
#define	COMMAND	24
#define	WDDATA	25
#define	AUXSTAT	31

/* OWNID Register Bits */
#define	OWN_EAF	0x08
#define	OWN_EHP	0x10
#define	OWN_FS0	0x40
#define	OWN_FS1	0x80
/* AUX Register Bits */
#define	AUX_DBR	0
#define	AUX_PE	1
#define	AUX_CIP	0x10
#define	AUX_BSY	0x20
#define	AUX_LCI	0x40
#define	AUX_INT	0x80

/* Select timeout const, 1 count = 8ms */
#define IN2000_TMOUT 0x1f

/* These belong in scsi.h also */
#undef any2scsi
#define any2scsi(up, p)				\
(up)[0] = (((unsigned long)(p)) >> 16);		\
(up)[1] = (((unsigned long)(p)) >> 8);		\
(up)[2] = ((unsigned long)(p));

#undef scsi2int
#define scsi2int(up) ( ((((long)*(up))&0x1f) << 16) + (((long)(up)[1]) << 8) + ((long)(up)[2]) )

#undef xany2scsi
#define xany2scsi(up, p)	\
(up)[0] = ((long)(p)) >> 24;	\
(up)[1] = ((long)(p)) >> 16;	\
(up)[2] = ((long)(p)) >> 8;	\
(up)[3] = ((long)(p));

#define xscsi2int(up) ( (((long)(up)[0]) << 24) + (((long)(up)[1]) << 16) \
		      + (((long)(up)[2]) <<  8) +  ((long)(up)[3]) )

#define MAX_CDB 12
#define MAX_SENSE 14
#define MAX_STATUS 32

int in2000_detect(Scsi_Host_Template *);
int in2000_command(Scsi_Cmnd *);
int in2000_queuecommand(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int in2000_abort(Scsi_Cmnd *);
int in2000_reset(Scsi_Cmnd *);
int in2000_biosparam(Disk *, kdev_t, int*);

#ifndef NULL
	#define NULL 0
#endif


/* next may be "SG_NONE" or "SG_ALL" or nr. of (1k) blocks per R/W Cmd. */
#define IN2000_SG SG_ALL
#define IN2000 {NULL, NULL,  \
                NULL, NULL, \
		"Always IN2000", in2000_detect, NULL,	\
		NULL, in2000_command,		\
		in2000_queuecommand,		\
		in2000_abort,			\
		in2000_reset,			\
		NULL,				\
		in2000_biosparam,               \
		1, 7, IN2000_SG, 1, 0, 0}

#endif
