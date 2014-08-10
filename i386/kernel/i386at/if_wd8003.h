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
 * Western Digital Mach Ethernet driver
 * Copyright (c) 1990 OSF Research Institute 
 */
/*
  Copyright 1990 by Open Software Foundation,
Cambridge, MA.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appears in all copies and
that both the copyright notice and this permission notice appear in
supporting documentation, and that the name of OSF or Open Software
Foundation not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

  OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/***********************************************************/
/*  Defines for the 583 chip.                              */
/***********************************************************/

/*--- 8390 Registers ---*/
#define OFF_8390	0x10	/* offset of the 8390 chip */

/* Register offsets */

#define IFWD_REG_0	0x00
#define IFWD_REG_1	0x01
#define IFWD_REG_2	0x02
#define IFWD_REG_3	0x03
#define IFWD_REG_4	0x04
#define IFWD_REG_5	0x05
#define IFWD_REG_6	0x06
#define IFWD_REG_7	0x07

/* Register offset definitions for all boards */

#define IFWD_LAR_0	0x08
#define IFWD_LAR_1	0x09
#define IFWD_LAR_2	0x0a
#define IFWD_LAR_3	0x0b
#define IFWD_LAR_4	0x0c
#define IFWD_LAR_5	0x0d
#define IFWD_BOARD_ID	0x0e
#define IFWD_CHKSUM	0x0f

/* revision number mask for BOARD_ID */
#define IFWD_BOARD_REV_MASK	0x1e

/* REG 1 */
#define IFWD_MEMSIZE	0x08
#define IFWD_16BIT	0x01

/* REG 5 */
#define IFWD_REG5_MEM_MASK	0x3f /* B23-B19 of address of the memory */
#define IFWD_LA19		0x01 /* B19 of address of the memory */
#define IFWD_MEM16ENB		0x80 /* Enable 16 bit memory access from bus */
#define IFWD_LAN16ENB		0x40 /* Enable 16 bit memory access from chip*/
#define IFWD_INIT_LAAR		IFWD_LA19
#define IFWD_SOFTINT            0x20 /* Enable interrupt from pc */

/* Defs for board rev numbers > 1 */
#define IFWD_MEDIA_TYPE		0x01
#define IFWD_SOFT_CONFIG	0x20
#define IFWD_RAM_SIZE		0x40
#define IFWD_BUS_TYPE		0x80

/* Register offsets for reading the EEPROM in the 584 chip */
#define IFWD_EEPROM_0		0x08
#define IFWD_EEPROM_1		0x09
#define IFWD_EEPROM_2		0x0A
#define IFWD_EEPROM_3		0x0B
#define IFWD_EEPROM_4		0x0C
#define IFWD_EEPROM_5		0x0D
#define IFWD_EEPROM_6		0x0E
#define IFWD_EEPROM_7		0x0F

/**** defs for manipulating the 584 ****/
#define IFWD_OTHER_BIT			0x02
#define IFWD_ICR_MASK			0x0C
#define IFWD_EAR_MASK			0x0F
#define IFWD_ENGR_PAGE			0xA0
/* #define IFWD_RLA			0x10  defined in ICR defs */
#define IFWD_EA6			0x80
#define IFWD_RECALL_DONE_MASK		0x10
#define IFWD_EEPROM_MEDIA_MASK		0x07
#define IFWD_STARLAN_TYPE		0x00
#define IFWD_ETHERNET_TYPE		0x01
#define IFWD_TP_TYPE			0x02
#define IFWD_EW_TYPE			0x03
#define IFWD_EEPROM_IRQ_MASK		0x18
#define IFWD_PRIMARY_IRQ		0x00
#define IFWD_ALTERNATE_IRQ_1		0x08
#define IFWD_ALTERNATE_IRQ_2		0x10
#define IFWD_ALTERNATE_IRQ_3		0x18
#define IFWD_EEPROM_RAM_SIZE_MASK	0xE0
#define IFWD_EEPROM_RAM_SIZE_RES1	0x00
#define IFWD_EEPROM_RAM_SIZE_RES2	0x20
#define IFWD_EEPROM_RAM_SIZE_8K		0x40
#define IFWD_EEPROM_RAM_SIZE_16K	0x60
#define IFWD_EEPROM_RAM_SIZE_32K	0x80
#define IFWD_EEPROM_RAM_SIZE_64K	0xA0
#define IFWD_EEPROM_RAM_SIZE_RES3	0xC0
#define IFWD_EEPROM_RAM_SIZE_RES4	0xE0
#define IFWD_EEPROM_BUS_TYPE_MASK	0x07
#define IFWD_EEPROM_BUS_TYPE_AT		0x00
#define IFWD_EEPROM_BUS_TYPE_MCA	0x01
#define IFWD_EEPROM_BUS_TYPE_EISA	0x02
#define IFWD_EEPROM_BUS_SIZE_MASK	0x18
#define IFWD_EEPROM_BUS_SIZE_8BIT	0x00
#define IFWD_EEPROM_BUS_SIZE_16BIT	0x08
#define IFWD_EEPROM_BUS_SIZE_32BIT	0x10
#define IFWD_EEPROM_BUS_SIZE_64BIT	0x18

/*****************************************************************************
 *                                                                           *
 *   Definitions for board ID.                                               *
 *                                                                           *
 *   note: board ID should be ANDed with the STATIC_ID_MASK                  *
 *         before comparing to a specific board ID                           *
 *	   The high order 16 bits correspond to the Extra Bits which do not  *
 *         change the boards ID.                                             *
 *                                                                           *
 *   Note: not all are implemented.  Rest are here for future enhancements...*
 *                                                                           *
 *****************************************************************************/

#define IFWD_STARLAN_MEDIA	0x00000001	/* StarLAN */
#define IFWD_ETHERNET_MEDIA	0x00000002	/* Ethernet */
#define IFWD_TWISTED_PAIR_MEDIA	0x00000003	/* Twisted Pair */
#define IFWD_EW_MEDIA		0x00000004	/* Ethernet and Twisted Pair */
#define IFWD_MICROCHANNEL	0x00000008	/* MicroChannel Adapter */
#define IFWD_INTERFACE_CHIP	0x00000010	/* Soft Config Adapter */
/* #define IFWD_UNUSED	        0x00000020 */	/* used to be INTELLIGENT */
#define IFWD_BOARD_16BIT	0x00000040	/* 16 bit capability */
#define IFWD_RAM_SIZE_UNKNOWN	0x00000000	/* 000 => Unknown RAM Size */
#define IFWD_RAM_SIZE_RES_1	0x00010000	/* 001 => Reserved */
#define IFWD_RAM_SIZE_8K	0x00020000	/* 010 => 8k RAM */
#define IFWD_RAM_SIZE_16K	0x00030000	/* 011 => 16k RAM */
#define IFWD_RAM_SIZE_32K	0x00040000	/* 100 => 32k RAM */
#define IFWD_RAM_SIZE_64K	0x00050000	/* 101 => 64k RAM */
#define IFWD_RAM_SIZE_RES_6	0x00060000	/* 110 => Reserved */
#define IFWD_RAM_SIZE_RES_7	0x00070000	/* 111 => Reserved */
#define IFWD_SLOT_16BIT		0x00080000	/* 16 bit board - 16 bit slot*/
#define IFWD_NIC_690_BIT	0x00100000	/* NIC is 690 */
#define IFWD_ALTERNATE_IRQ_BIT	0x00200000	/* Alternate IRQ is used */
#define IFWD_INTERFACE_584_CHIP	0x00400000	/* Interface chip is a 584 */

#define IFWD_MEDIA_MASK		0x00000007	/* Isolates Media Type */
#define IFWD_RAM_SIZE_MASK	0x00070000	/* Isolates RAM Size */
#define IFWD_STATIC_ID_MASK	0x0000FFFF	/* Isolates Board ID */

/* Word definitions for board types */
#define WD8003E		IFWD_ETHERNET_MEDIA
#define WD8003EBT	WD8003E		/* functionally identical to WD8003E */
#define WD8003S		IFWD_STARLAN_MEDIA
#define WD8003SH	WD8003S		/* functionally identical to WD8003S */
#define WD8003WT	IFWD_TWISTED_PAIR_MEDIA
#define WD8003W		(IFWD_TWISTED_PAIR_MEDIA | IFWD_INTERFACE_CHIP)
#define WD8003EB	(IFWD_ETHERNET_MEDIA | IFWD_INTERFACE_CHIP)
#define WD8003EP	WD8003EB    /* with IFWD_INTERFACE_584_CHIP bit set */a
#define WD8003EW	(IFWD_EW_MEDIA | IFWD_INTERFACE_CHIP)
#define WD8003ETA	(IFWD_ETHERNET_MEDIA | IFWD_MICROCHANNEL)
#define WD8003STA	(IFWD_STARLAN_MEDIA | IFWD_MICROCHANNEL)
#define WD8003EA	(IFWD_ETHERNET_MEDIA | IFWD_MICROCHANNEL | \
			 IFWD_INTERFACE_CHIP)
#define WD8003SHA	(IFWD_STARLAN_MEDIA | IFWD_MICROCHANNEL | \
			 IFWD_INTERFACE_CHIP)
#define WD8003WA	(IFWD_TWISTED_PAIR_MEDIA | IFWD_MICROCHANNEL | \
			 IFWD_INTERFACE_CHIP)
#define WD8013EBT	(IFWD_ETHERNET_MEDIA | IFWD_BOARD_16BIT)
#define WD8013EB	(IFWD_ETHERNET_MEDIA | IFWD_BOARD_16BIT | \
			 IFWD_INTERFACE_CHIP)
#define WD8013EP	WD8013EB    /* with IFWD_INTERFACE_584_CHIP bit set */
#define WD8013W		(IFWD_TWISTED_PAIR_MEDIA | IFWD_BOARD_16BIT | \
			 IFWD_INTERFACE_CHIP)
#define WD8013EW	(IFWD_EW_MEDIA | IFWD_BOARD_16BIT | \
			 IFWD_INTERFACE_CHIP)


/**** Western digital node bytes ****/
#define	WD_NODE_ADDR_0	0x00
#define	WD_NODE_ADDR_1	0x00
#define	WD_NODE_ADDR_2	0xC0

/*--- 83c583 registers ---*/
#define IFWD_MSR	0x00		/* memory select register */
				        /* In 584 Board's command register */
#define IFWD_ICR	0x01		/* interface configuration register */
                                        /* In 584 8013 bus size register */
#define IFWD_IAR	0x02		/* io address register */
#define IFWD_BIO	0x03		/* bios ROM address register */
#define IFWD_IRR	0x04		/* interrupt request register */
#define IFWD_GP1	0x05		/* general purpose register 1 */
#define IFWD_IOD	0x06		/* io data latch */
#define IFWD_GP2	0x07		/* general purpose register 2 */
#define IFWD_LAR	0x08		/* LAN address register	*/
#define IFWD_LAR2	0x09		/*			*/
#define IFWD_LAR3	0x0A		/*			*/
#define IFWD_LAR4	0x0B		/*			*/
#define IFWD_LAR5	0x0C		/*			*/
#define IFWD_LAR6	0x0D		/*			*/
#define IFWD_LAR7	0x0E		/*			*/
#define IFWD_LAR8	0x0F		/* LAN address register */

/********************* Register Bit Definitions **************************/
/* MSR definitions */
#define IFWD_RST	0x80	        /* 1 => reset */
#define IFWD_MENB	0x40		/* 1 => memory enable */
#define IFWD_SA18	0x20		/* Memory enable bits	*/
#define	IFWD_SA17	0x10		/*	telling where shared	*/
#define	IFWD_SA16	0x08		/*	mem is to start.	*/
#define IFWD_SA15	0x04		/*	Assume SA19 = 1		*/
#define IFWD_SA14	0x02		/*				*/
#define	IFWD_SA13	0x01		/*				*/

/* ICR definitions */
#define	IFWD_STR	0x80		/* Non-volatile EEPROM store	*/
#define	IFWD_RCL	0x40		/* Recall I/O Address from EEPROM */
#define	IFWD_RX7	0x20		/* Recall all but I/O and LAN address*/
#define IFWD_RLA	0x10		/* Recall LAN Address	*/
#define	IFWD_MSZ	0x08		/* Shared Memory Size	*/
#define	IFWD_DMAE	0x04		/* DMA Enable	*/
#define	IFWD_IOPE	0x02		/* I/O Port Enable */
#define IFWD_WTS	0x01		/* Word Transfer Select */

/* IAR definitions */
#define	IFWD_IA15	0x80		/* I/O Address Bits	*/
/*	.		*/
/*	.		*/
/*	.		*/
#define	IFWD_IA5	0x01		/*			*/

/* BIO definitions */
#define	IFWD_RS1	0x80		/* BIOS size bit 1 */
#define	IFWD_RS0	0x40		/* BIOS size bit 0 */
#define	IFWD_BA18	0x20		/* BIOS ROM Memory Address Bits */
#define	IFWD_BA17	0x10		/*				*/
#define	IFWD_BA16	0x08		/*				*/
#define	IFWD_BA15	0x04		/*				*/
#define IFWD_BA14	0x02		/* BIOS ROM Memory Address Bits */
#define	IFWD_WINT	0x01		/* W8003 interrupt	*/

/* IRR definitions */
#define	IFWD_IEN	0x80	/* Interrupt Enable	*/
#define	IFWD_IR1	0x40	/* Interrupt request bit 1	*/
#define	IFWD_IR0	0x20	/* Interrupt request bit 0	*/
#define	IFWD_AMD	0x10	/* Alternate mode	*/
#define IFWD_AINT	0x08	/* Alternate interrupt	*/
#define IFWD_BW1	0x04	/* BIOS Wait State Control bit 1	*/
#define IFWD_BW0	0x02	/* BIOS Wait State Control bit 0	*/
#define IFWD_OWS	0x01	/* Zero Wait State Enable	*/

/* GP1 definitions */

/* IOD definitions */

/* GP2 definitions */

/*************************************************************/
/*   Shared RAM buffer definitions                           */
/*************************************************************/

/**** NIC definitions ****/
#define NIC_8003_SRAM_SIZE 0x2000       /* size of shared RAM buffer */
#define	NIC_HEADER_SIZE	4		/* size of receive header */
#define	NIC_PAGE_SIZE	0x100		/* each page of rcv ring is 256 byte */

#define ETHER_ADDR_SIZE	6	/* size of a MAC address */

#ifdef MACH
#define	HZ		100
#endif

#define	DSF_LOCK	1
#define DSF_RUNNING	2

#define MOD_ENAL 1
#define MOD_PROM 2
