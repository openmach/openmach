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
 * NEC 765/Intel 8272 floppy disk controller.
 */

/*
 * Ports
 */
#define FD_DOR(p)	(p)		/* digital output register */
#define FD_STATUS(p)	((p) + 2)	/* status register */
#define FD_DATA(p)	((p) + 3)	/* data register */
#define FD_RATE(p)	((p) + 5)	/* transfer rate register */
#define FD_DIR(p)	((p) + 5)	/* digital input register */

/*
 * Digital output register.
 */
#define DOR_IENABLE	0x08	/* enable interrupts and DMA */
#define DOR_RSTCLR	0x04	/* clear reset */

/*
 * Status register.
 */
#define ST_RQM		0x80	/* request for master */
#define ST_DIO		0x40	/* direction of data transfer
				   1 = fdc to cpu, 0 = cpu to fdc */
#define ST_NDM		0x20	/* non DMA mode */
#define ST_CB		0x10	/* controller busy */

/*
 * Digital input register.
 */
#define DIR_DSKCHG	0x80	/* diskette chnage has occured */

/*
 * ST0
 */
#define ST0_IC		0xc0	/* interrupt code */
#define ST0_SE		0x20	/* seek end */
#define ST0_EC		0x10	/* equipment check */
#define ST0_NR		0x08	/* not ready */
#define ST0_HD		0x04	/* head address */
#define ST0_US		0x03	/* unit select */

/*
 * ST1
 */
#define ST1_EC		0x80	/* end of cylinder */
#define ST1_DE		0x20	/* CRC data error */
#define ST1_OR		0x10	/* DMA overrun */
#define ST1_ND		0x04	/* sector not found */
#define ST1_NW		0x02	/* write-protected diskette */
#define ST1_MA		0x01	/* missing address mark */

/*
 * ST2
 */
#define ST2_CM		0x40	/* control mark */
#define ST2_DD		0x20	/* data error */
#define ST2_WC		0x10	/* wrong cylinder */
#define ST2_SH		0x08	/* scan equal hit */
#define ST2_SN		0x04	/* scan not satisfied */
#define ST2_BC		0x02	/* bad cylinder */
#define ST2_MD		0x01	/* missing address mark */

/*
 * ST3
 */
#define ST3_FT		0x80	/* fault */
#define ST3_WP		0x40	/* write protect */
#define ST3_RY		0x20	/* ready */
#define ST3_T0		0x10	/* track 0 */
#define ST3_TS		0x08	/* two side */
#define ST3_HD		0x04	/* head address */
#define ST3_US		0x03	/* unit select */

/*
 * Commands.
 */
#define CMD_SPECIFY	0x03
#define CMD_RECAL	0x07
#define CMD_SENSEI	0x08
#define CMD_SEEK	0x0f
#define CMD_FORMAT	0x4d
#define CMD_WRITE	0xc5
#define CMD_READ	0xe6

/*
 * Information required by FDC when formatting a diskette.
 */
struct format_info {
	unsigned char	cyl;
	unsigned char	head;
	unsigned char	sector;
	unsigned char	secsize;
};
