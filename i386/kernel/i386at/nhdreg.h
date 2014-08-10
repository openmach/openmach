/* 
 * Copyright (c) 1994 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/*
 * Hard disk controller.
 */

#define HD_DATA(p)	(p)		/* data register */
#define HD_ERROR(p)	((p) + 1)	/* error register */
#define HD_PRECOMP(p)	((p) + 1)	/* precomp register */
#define HD_SECTCNT(p)	((p) + 2)	/* sector count register */
#define HD_SECT(p)	((p) + 3)	/* sector number register */
#define HD_CYLLO(p)	((p) + 4)	/* cylinder number low */
#define HD_CYLHI(p)	((p) + 5)	/* cylinder number high */
#define HD_DRVHD(p)	((p) + 6)	/* drive head register */
#define HD_STATUS(p)	((p) + 7)	/* status register */
#define HD_CMD(p)	((p) + 7)	/* command register */

/*
 * Status register
 */
#define ST_BUSY		0x80		/* controller is busy */
#define ST_READY	0x40		/* drive is ready */
#define ST_WRTFLT	0x20		/* write fault */
#define ST_SEEK		0x10		/* seek complete */
#define ST_DREQ		0x08		/* data request */
#define ST_ECC		0x04		/* ECC corrected data */
#define ST_INDEX	0x02		/* index pulse */
#define ST_ERROR	0x01		/* an operation resulted in error */

/*
 * Error register
 */
#define ERR_DAM		0x01		/* data address mark not found */
#define ERR_TR0		0x02		/* track 0 not found */
#define ERR_ABORT	0x04		/* command aborted */
#define ERR_ID		0x10		/* sector not found */
#define ERR_ECC		0x40		/* uncorrectable ECC error */
#define ERR_BADBLK	0x80		/* bad block detected */

/*
 * Commands
 */
#define CMD_RESTORE	0x10
#define CMD_READ	0x20
#define CMD_WRITE	0x30
#define CMD_SETPARAM	0x91
#define CMD_READMULTI	0xc4
#define CMD_WRITEMULTI	0xc5
#define CMD_SETMULTI	0xc6
#define CMD_IDENTIFY	0xec

#if 0
#define PDLOCATION	29	/* XXX: belongs in <i386at/disk.h> */
#endif

#define BAD_BLK		0x80
#define SECSIZE		512

/*
 * Information returned by IDENTIFY command.
 */
struct hdident {
	u_short	id_config;	/* flags */
	u_short	id_npcyl;	/* # physical cylinders */
	u_short	id_rsvd2;	/* reserved (word 2) */
	u_short	id_nptrk;	/* # physical tracks */
	u_short	id_bptrk;	/* unformatted bytes/track */
	u_short	id_bpsect;	/* unformatted bytes/sector */
	u_short	id_npsect;	/* # physical sectors/track */
	u_short	id_vendor0;	/* vendor unique */
	u_short	id_vendor1;	/* vendor unique */
	u_short	id_vendor2;	/* vendor unique */
	u_char	id_serno[20];	/* serial #: 0 = unspecified */
	u_short	id_buftype;	/* ??? */
	u_short	id_bufsize;	/* 512 byte increments: 0 = unspecified */
	u_short	id_eccbytes;	/* for R/W LONG commands: 0 = unspecified */
	u_char	id_rev[8];	/* firmware revision: 0 = unspecified */
	u_char	id_model[40];	/* model name: 0 = unspecified */
	u_char	id_multisize;	/* max multiple I/O size: 0 = unsupported */
	u_char	id_vendor3;	/* vendor unique */
	u_short	id_dwordio;	/* 0 = unsupported; 1 = implemented */
	u_char	id_vendor4;	/* vendor unique */
	u_char	id_capability;	/* 0:DMA 1:LBA 2:IORDYsw 3:IORDY:sup */
	u_short	id_rsvd50;	/* reserved (word 50) */
	u_char	id_vendor5;	/* vendor unique */
	u_char	id_pio;		/* 0=slow, 1=medium, 2=fast */
	u_char	id_vendor6;	/* vendor unique */
	u_char	id_dma;		/* 0=slow, 1=medium, 2=fast */
	u_short	id_valid;	/* 0:logical 1:eide */
	u_short	id_nlcyl;	/* # logical cylinders */
	u_short	id_nltrk;	/* # logical tracks */
	u_short	id_nlsect;	/* # logical sectors/track */
	u_short	id_capacity0;	/* logical total sectors on drive */
	u_short	id_capacity1;	/*  (2 words, misaligned int) */
	u_char	id_multisect;	/* current multiple sector count */
	u_char	id_multivalid;	/* bit 0=1, multisect field is valid */
	u_short	id_totsect;	/* total number of sectors */
	u_short	id_dma1;	/* single word DMA info */
	u_short	id_dmamulti;	/* multiple word DMA info */
	u_short	id_eidepiomode;	/* 0:mode3 1:mode4 */
	u_short	id_eidedmamin;	/* min multiple word DMA cycle time (ns) */
	u_short	id_eidedmatime;	/* recomended DMA cycle time (ns) */
	u_short	id_eidepio;	/* min cycle time (ns), no IORDY */
	u_short	id_eidepioiordy;/* min cycle time (ns, with IORDY */
	u_short	id_rsvd69;	/* reserved (word 69) */
	u_short	id_rsvd70;	/* reserved (word 70) */
};
