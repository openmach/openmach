/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File: vs42x_rb.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	5/91
 *
 *	This file contains definitions for the VS42X-RB Storage
 *	controller, which includes a Disk controller and a
 *	SCSI controller.
 */

#ifndef	_VS42X_RB_H_
#define	_VS42X_RB_H_

/*
 * Phys addresses for the Vax3100
 */
#define VAX3100_STC_BASE	0x200c0000
#define VAX3100_STC_RAM_COMPAT	0x200d0000
#define	VAX3100_STC_RAM		0x202d0000

#define	VAX3100_STC_HDC9224	0x00000000	/* offsets from BASE */
#define VAX3100_STC_5380_A	0x00000080
#define VAX3100_STC_5380_B	0x00000180
#define VAX3100_STC_DMAREG_A	0x000000a0
#define VAX3100_STC_DMAREG_B	0x000001a0
#define	VAX3100_STC_RAM_MODE	0x000000e0

#define	VAX3100_STC_DMAREG_OFF	(0xa0-0x80)	/* offset from 5380 */

#define	SCI_REG_SIZE		512

/*
 * RAM Buffer for this storage system
 */
#define	SCI_RAM_SIZE		128*1024
#define	SCI_RAM_COMPATSIZE	32*1024
#define SCI_RAM_EXPMODE		0x01		/* char-size mode register */

/*
 * DMA controller for the SCSI subsystem
 * (Defines for the NCR 5380 are elsewhere)
 */

typedef struct {
	unsigned int	sci_dma_adr;	/* +000000a0 */
	char				pad0[0xc0-0xa0-4];
	unsigned int	sci_dma_count;	/* +000000c0 */
	unsigned int	sci_dma_dir;	/* +000000c4 */
	char				pad1[0xe0-0xc4-4];
	unsigned char	sci_dma_rammode;/* +000000e0 */
} *sci_dmaregs_t;

#define SCI_DMADR_PUT(ptr,adr)	(ptr)->sci_dma_adr = (unsigned)(adr) & SCI_DMA_COUNT_MASK;
#define SCI_DMADR_GET(ptr,adr)	(adr) = (ptr)->sci_dma_adr;

#define	SCI_DMA_COUNT_MASK	0x0001ffff
#define SCI_TC_GET(ptr,cnt)	{\
		(cnt) = (ptr)->sci_dma_count;\
		if ((cnt) & 0x00010000) (cnt) |= ~SCI_DMA_COUNT_MASK;\
		(cnt) = -(cnt);\
	}
#define SCI_TC_PUT(ptr,cnt)	(ptr)->sci_dma_count = -(cnt);

#define	SCI_DMA_DIR_READ	0x00000001
#define	SCI_DMA_DIR_WRITE	0x00000000

/*
 * Disk controller subsytem (ST506/412), uses a
 * HDC 9224 Universal Disk Controller chip and
 * addresses up to 4 disks.
 */
typedef struct {
	unsigned char	hdc_rap;		/* rw: reg addres ptr */
	char						pad0[3];
	unsigned char	hdc_cmd;		/* w:  controller command */
#define			hdc_status hdc_cmd	/* r:  interrupt status */
	char						pad1[3];
} *sci_hdcregs_t;

/*
 * Register Address Pointer
 */
#define UDC_DMA7	0	/* rw: DMA address bits 7:0 */
#define UDC_DMA15	1	/* rw: DMA address bits 15:8 */
#define UDC_DMA23	2	/* rw: DMA address bits 23:16 */
#define UDC_DSECT	3	/* rw: desired sector */
#define UDC_DHEAD	4	/* wo: desired head */
#define UDC_CHEAD	4	/* ro: current head */
#define UDC_DCYL	5	/* wo: desired cylinder */
#define UDC_CCYL	5	/* ro: current cylinder */
#define UDC_SCNT	6	/* wo: sector count */
#define UDC_RTCNT	7	/* wo: retry count */
#define UDC_MODE	8	/* wo: operating mode */
#define UDC_CSTAT	8	/* ro: chip status */
#define UDC_TERM	9	/* wo: termination conditions */
#define UDC_DSTAT	9	/* ro: drive status */
#define UDC_DATA	10	/* rw: data */

/*
 * Controller Commands
 */
#define HDCC_RESET		0x00

#define HDCC_SET_REGP		0x40	/* low 4 bits is regnum */

#define HDCC_DESELECT		0x01

#define HDCC_SELECT		0x20
#	define HDCC_SELECT_IDMASK	0x03
#	define HDCC_SELECT_DR_HD	0x04
#	define HDCC_SELECT_DR_SD	0x08
#	define HDCC_SELECT_DR_DD	0x0c

#define HDCC_RESTORE_HD		0x03

#define HDCC_RESTORE_RX		0x02

#define HDCC_STEP		0x04
#	define HDCC_STEP_OUT		0x02
#	define HDCC_STEP_SKWAIT		0x01

#define HDCC_POLL		0x10	/* low 4 bits is drive mask */

#define HDCC_SEEK		0x50
#	define HDCC_SEEK_STEP		0x04
#	define HDCC_SEEK_SKWAIT		0x02
#	define HDCC_SEEK_VFY		0x01

#define HDCC_FORMAT		0x60
#	define HDCC_FORMAT_DDMARK	0x10

#define HDCC_READ_T		0x5a
#	define HDCC_READ_XDATA		0x01

#define HDCC_READ_P		0x58

#define HDCC_READ_L		0x5c
#	define HDCC_READ_L_BYPASS	0x02

#define HDCC_WRITE_P		0x80
#	define HDCC_WRITE_BYPASS	0x40
#	define HDCC_WRITE_DDMARK	0x10

#define HDCC_WRITE_L		0xa0

/*
 * Interrupt Status Register
 */
#define HDCI_BADSECT	0x01
#define HDCI_OVRUN	0x02
#define HDCI_RDYCHNG	0x04
#define HDCI_TERMCOD	0x18
#	define HDC_T_SUCCESS	0x00
#	define HDC_T_EREAD_ID	0x08
#	define HDC_T_EVFY	0x10
#	define HDC_T_EXFER	0x18
#define HDCI_DONE	0x20
#define HDCI_DMAREQ	0x40
#define HDCI_INT	0x80		/* interrupt pending */

/*
 * Desired/Current Head
 */
#define UDC_HEAD_HMASK		0x0f		/* desired head no */
#define UDC_HEAD_CMASK		0x70		/* desired cyl 10:8 */
#define UDC_HEAD_BADSEC		0x80

/*
 * Sector Count
 */
#define HDC_MAXDATA		256*512

/*
 * Retry Count
 */
#define UDC_RTCNT_MASK		0xf0
#define UDC_RTCNT_RXDIS		0x08	/* mbz */
#define UDC_RTCNT_INVRDY	0x04
#define UDC_RTCNT_MOTOR		0x02
#define UDC_RTCNT_LOSPEED	0x01

/*
 * Mode
 */
#define UDC_MODE_HD		0x80	/* hard disk mode mb1 */
#define UDC_MODE_CHKCOD		0x60	/* error checkin code */
#	define UDC_MODE_CRC	0x00
#	define UDC_MODE_EECC	0x20	/* NA */
#	define UDC_MODE_IECC	0x40	/* hard disks internal 32 ecc */
#	define UDC_MODE_AECC	0x60	/* NA */
#define UDC_MODE_DENS		0x10	/* mbz */
#define UDC_MODE_SRATE		0x07
#	define UDC_MODE_RATE_HD		0x00	/* hard disk */
#	define UDC_MODE_RATE_DD		0x01	/* double den rx23 */
#	define UDC_MODE_RATE_SD		0x02	/* single den rz23 */
#	define UDC_MODE_RATE_RD		0x06	/* restore drive */

#define UDC_MODE_RX23_DD	0x81
#define UDC_MODE_RX23_SD	0x82
#define UDC_MODE_RDxx		0xc0
#define UDC_MODE_RD_RESTORE	0xc6

/*
 * Status
 */
#define UDC_CSTAT_RETRIED	0x80
#define UDC_CSTAT_ECC		0x40
#define UDC_CSTAT_ECC_ERR	0x20
#define UDC_CSTAT_DELDATA	0x10
#define UDC_CSTAT_SYN_ERR	0x08
#define UDC_CSTAT_COMP_ERR	0x04
#define UDC_CSTAT_SELMASK	0x03
#	define UDC_CSTAT_SELHD0		0x00
#	define UDC_CSTAT_SELHD1		0x01
#	define UDC_CSTAT_SELRX		0x02
#	define UDC_CSTAT_SELHD2		0x03

/*
 * Termination
 */
#define UDC_TERM_CRCPRE		0x80	/* mb1 */
#define UDC_TERM_IDONE		0x20
#define UDC_TERM_DELDAT		0x10
#define UDC_TERM_STAT3		0x08	/* mbz */
#define UDC_TERM_WPROT		0x04
#define UDC_TERM_IRDCHNG	0x02
#define UDC_TERM_WFLT		0x01

/*
 * Drive status
 */
#define UDC_DSTAT_SELACK	0x80
#define UDC_DSTAT_INDEX		0x40
#define UDC_DSTAT_SKCOM		0x20
#define UDC_DSTAT_TRK0		0x10
#define UDC_DSTAT_STAT3		0x08	/* mbz */
#define UDC_DSTAT_WPROT		0x04
#define UDC_DSTAT_READY		0x02
#define UDC_DSTAT_WFLT		0x01


#endif	_VS42X_RB_H_
