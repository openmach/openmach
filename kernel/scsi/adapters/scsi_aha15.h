/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	File: scsi_aha15.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	6/91
 *
 *	Definitions for the Adaptec AHA-15xx family
 *	of Intelligent SCSI Host Adapter boards
 */

#ifndef	_SCSI_AHA15_H_
#define	_SCSI_AHA15_H_

/*
 * Addresses/length in 24 bits
 *
 * BEWARE: your compiler must pack these correctly,
 * e.g. without gaps between two such contiguous structs
 * (GCC does)
 */
typedef struct {
	unsigned char	msb;
	unsigned char	mid;
	unsigned char	lsb;
} aha_address_t;

#define AHA_ADDRESS_SET(addr,val)	{\
		(addr).msb = ((val) >> 16);\
		(addr).mid = ((val) >>  8);\
		(addr).lsb =  (val)	  ;\
	}
#define AHA_ADDRESS_GET(addr,val) {\
		(val) = ((addr).msb << 16) |\
			((addr).mid <<  8) |\
			((addr).lsb      ) ;\
	}

#define	aha_length_t	aha_address_t
#define	AHA_LENGTH_SET	AHA_ADDRESS_SET
#define	AHA_LENGTH_GET	AHA_ADDRESS_GET

/*
 * Register map
 */

typedef struct {
	volatile unsigned char	aha_status;	/* r:  Status Register */
#define			aha_ctl aha_status	/* w:  Control Register */

	volatile unsigned char	aha_data;	/* rw: Data Port */
#define			aha_cmd aha_data	/* w:  Command register */

	volatile unsigned char	aha_intr;	/* ro: Interrupt Register */
} aha_regmap_t;

/* ..but on the 386 I/O is not memory mapped */
#define	AHA_STATUS_PORT(port)	((port))
#define	AHA_CONTROL_PORT(port)	((port))
#define	AHA_COMMAND_PORT(port)	((port)+1)
#define	AHA_DATA_PORT(port)	((port)+1)
#define	AHA_INTR_PORT(port)	((port)+2)

/* Status Register */
#define AHA_CSR_CMD_ERR		0x01		/* Invalid command */
#define AHA_CSR_xxx		0x02		/* undefined */
#define AHA_CSR_DATAI_FULL	0x04		/* In-port full */
#define AHA_CSR_DATAO_FULL	0x08		/* Out-port full */
#define AHA_CSR_IDLE		0x10		/* doin nuthin */
#define AHA_CSR_INIT_REQ	0x20		/* initialization required */
#define AHA_CSR_DIAG_FAIL	0x40		/* selftest failed */
#define AHA_CSR_SELF_TEST	0x80		/* selftesting */

/* Control Register */
#define AHA_CTL_xxx		0x0f		/* undefined */
#define AHA_CTL_SCSI_RST	0x10		/* reset SCSIbus */
#define AHA_CTL_INTR_CLR	0x20		/* Clear interrupt reg */
#define AHA_CTL_SOFT_RESET	0x40		/* Board only, no selftest */
#define AHA_CTL_HARD_RESET	0x80		/* Full reset, and SCSIbus */

/* Interrupt Flags register */
#define AHA_INTR_MBI_FULL	0x01		/* scan the In mboxes */
#define AHA_INTR_MBO_AVAIL	0x02		/* scan the Out mboxes */
#define AHA_INTR_DONE		0x04		/* command complete */
#define AHA_INTR_RST		0x08		/* saw a SCSIbus reset */
#define AHA_INTR_xxx		0x70		/* undefined */
#define AHA_INTR_PENDING	0x80		/* Any interrupt bit set */

/*
 * Command register
 */
#define AHA_CMD_NOP		0x00		/* */
#define AHA_CMD_INIT		0x01		/* mbox initialization */
	/* 4 bytes follow: # of Out mboxes (x2->total), and
			   msb, mid, lsb of mbox address */
struct aha_init {
	unsigned char		mb_count;
	aha_address_t		mb_ptr;
};
#define AHA_CMD_START		0x02		/* start SCSI cmd */
#define AHA_CMD_BIOS		0x03		
#define AHA_CMD_INQUIRY		0x04
	/* returns 4 bytes: */
struct aha_inq {
	unsigned char		board_id;
#	define	AHA_BID_1540_B16		0x00
#	define	AHA_BID_1540_B64		0x30
#	define	AHA_BID_1540B			0x41
#	define	AHA_BID_1640			0x42
#	define	AHA_BID_1740			0x43
#	define	AHA_BID_1542C			0x44
#	define  AHA_BID_1542CF			0x45  	/* BIOS v2.0x */

	unsigned char		options;
#	define	AHA_BOPT_STD			0x41	/* in 154x, standard model */

	unsigned char		frl_1;		/* rev level */
	unsigned char		frl_2;
};
#define AHA_CMD_MBO_IE		0x05
	/* 1 byte follows: */
#	define	AHA_MBO_DISABLE			0x00
#	define	AHA_MBO_ENABLE			0x01

#define AHA_CMD_SET_SELTO	0x06		/* select timeout */
	/* 4 bytes follow: */
struct aha_selto {
	unsigned char		enable;
	char			xxx;
	unsigned char		timeo_msb;
	unsigned char		timeo_lsb;
};
#define AHA_CMD_SET_BUSON	0x07
	/* 1 byte value follows: 2..15 default 11 usecs */
#define AHA_CMD_SET_BUSOFF	0x08
	/* 1 byte value follows: 1..64 default 4 usecs */
#define AHA_CMD_SET_XSPEED	0x09
	/* 1 byte value follows: */
#	define AHA_DMASPEED_5Mb		0x00
#	define AHA_DMASPEED_7Mb		0x01
#	define AHA_DMASPEED_8Mb		0x02
#	define AHA_DMASPEED_10Mb	0x03
#	define AHA_DMASPEED_6Mb		0x04
	/* values in the range 80..ff encoded as follows:
		bit 7	on	--> custom speed
		bits 6..4	read pulse width
			0	100ns
			1	150
			2	200
			3	250
			4	300
			5	350
			6	400
			7	450
		bit 3		strobe off time
			0	100ns
			1	150ns
		bits 2..0	write pulse width
			<same as read pulse>
	 */
#define AHA_CMD_FIND_DEVICES	0x0a
	/* returns 8 bytes, each one is a bitmask of the LUNs
	   available for the given target ID */
struct aha_devs {
	unsigned char		tgt_luns[8];
};
#define AHA_CMD_GET_CONFIG	0x0b
	/* returns 3 bytes: */
struct aha_conf {
	unsigned char		dma_arbitration;/* bit N -> channel N */
	unsigned char		intr_ch;/* bit N -> intr 9+N (but 13,16)*/
	unsigned char		my_scsi_id;	/* both of I and T role */
};
#define AHA_CMD_ENB_TGT_MODE	0x0c
	/* 2 bytes follow: */
struct aha_tgt {
	unsigned char		enable;
	unsigned char		luns;	/* bitmask */
};

#define AHA_CMD_GET_SETUP	0x0d
	/* 1 byte follows: allocation len (N) */
	/* returns N bytes, 17 significant: */
struct aha_setup {
	BITFIELD_3( unsigned char,
				initiate_SDT:1,
				enable_parity:1,
				res:6);
	unsigned char		xspeed;	/* see above */
	unsigned char		buson;
	unsigned char		busoff;
	unsigned char		n_mboxes;/* 0 if not initialized */
	aha_address_t		mb_ptr; /* garbage if not inited */
	struct {
	    BITFIELD_3( unsigned char,
	    			offset: 4,
				period: 3,	/* 200 + 50 * N */
				negotiated: 1);
	} SDT_params[8];
	unsigned char		no_disconnect;	/* bitmask */
};

#define AHA_CMD_WRITE_CH2	0x1a
	/* 3 bytes (aha_address_t) follow for the buffer pointer */
#define AHA_CMD_READ_CH2	0x1b
	/* 3 bytes (aha_address_t) follow for the buffer pointer */
#define AHA_CMD_WRITE_FIFO	0x1c
	/* 3 bytes (aha_address_t) follow for the buffer pointer */
#define AHA_CMD_READ_FIFO	0x1d
	/* 3 bytes (aha_address_t) follow for the buffer pointer */
#define AHA_CMD_ECHO		0x1f
	/* 1 byte follows, which should then be read back */
#define AHA_CMD_DIAG		0x20
#define AHA_CMD_SET_OPT		0x21
	/* 2+ bytes follow: */
struct aha_diag {
	unsigned char		parmlen;	/* bytes to follow */
	unsigned char		no_disconnect;	/* bitmask */
	/* rest is undefined */
};

#define AHA_EXT_BIOS            0x28    /* return extended bios info */
#define AHA_MBX_ENABLE          0x29    /* enable mail box interface */
struct aha_extbios {
        unsigned char  	flags;		/* Bit 3 == 1 extended bios enabled */
        unsigned char  	mailboxlock;    /* mail box lock code to unlock it */
};

/*
 * Command Control Block
 */
typedef struct {
	unsigned char		ccb_code;
#	define		AHA_CCB_I_CMD		0x00
#	define		AHA_CCB_T_CMD		0x01
#	define		AHA_CCB_I_CMD_SG	0x02
#	define		AHA_CCB_ICMD_R		0x03
#	define		AHA_CCB_ICMD_SG_R	0x04
#	define		AHA_CCB_BDEV_RST	0x81
	BITFIELD_4( unsigned char,
				ccb_lun:3,
				ccb_in:1,
				ccb_out:1,
				ccb_scsi_id:3);
	unsigned char		ccb_cmd_len;
	unsigned char		ccb_reqsns_len;	/* if 1 no automatic reqsns*/
	aha_length_t		ccb_datalen;
	aha_address_t		ccb_dataptr;
	aha_address_t		ccb_linkptr;
	unsigned char		ccb_linkid;
	unsigned char		ccb_hstatus;
#	define	AHA_HST_SUCCESS			0x00
#	define	AHA_HST_SEL_TIMEO		0x11
#	define	AHA_HST_DATA_OVRUN		0x12
#	define	AHA_HST_BAD_DISCONN		0x13
#	define	AHA_HST_BAD_PHASE_SEQ		0x14
#	define	AHA_HST_BAD_OPCODE		0x16
#	define	AHA_HST_BAD_LINK_LUN		0x17
#	define	AHA_HST_INVALID_TDIR		0x18
#	define	AHA_HST_DUPLICATED_CCB		0x19
#	define	AHA_HST_BAD_PARAM		0x1a

	scsi2_status_byte_t	ccb_status;
	unsigned char		ccb_xxx;
	unsigned char		ccb_xxx1;
	scsi_command_group_5	ccb_scsi_cmd;	/* cast as needed */
} aha_ccb_t;

/* For scatter/gather use a list of (len,ptr) segments, each field
   is 3 bytes (aha_address_t) long. Max 17 segments, min 1 */

/*
 * Ring descriptor, aka Mailbox
 */
typedef union {

    struct {
	volatile unsigned char	mb_cmd;		/* Out mbox */
#	define		mb_status mb_cmd	/* In mbox */

	aha_address_t		mb_ptr;
#define AHA_MB_SET_PTR(mbx,val)	AHA_ADDRESS_SET((mbx)->mb.mb_ptr,(val))
#define AHA_MB_GET_PTR(mbx,val)	AHA_ADDRESS_GET((mbx)->mb.mb_ptr,(val))

    } mb;

    struct {					/* ccb required In mbox */
	volatile unsigned char	mb_cmd;
	BITFIELD_4( unsigned char,
				mb_lun : 3,
				mb_isa_send : 1,
				mb_isa_recv : 1,
				mb_initiator_id : 3);
	unsigned char		mb_data_len_msb;
	unsigned char		mb_data_len_mid;
    } mbt;

    unsigned int	bits;			/* quick access */

} aha_mbox_t;

/* Out mbox, values for the mb_cmd field */
#define	AHA_MBO_FREE		0x00
#define	AHA_MBO_START		0x01
#define	AHA_MBO_ABORT		0x02

/* In mbox, values for the mb_status field */
#define	AHA_MBI_FREE		0x00
#define	AHA_MBI_SUCCESS		0x01
#define	AHA_MBI_ABORTED		0x02
#define	AHA_MBI_NOT_FOUND	0x03
#define	AHA_MBI_ERROR		0x04
#define AHA_MBI_NEED_CCB	0x10

/*
 * Scatter/gather segment lists
 */
typedef struct {
	aha_length_t	len;
	aha_address_t	ptr;
} aha_seglist_t;

#define	AHA_MAX_SEGLIST		17		/* which means max 64Kb */
#endif	/*_SCSI_AHA15_H_*/
