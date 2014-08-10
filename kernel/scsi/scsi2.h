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
 *	File: scsi2.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Additions and changes of the SCSI-II standard viz SCSI-I
 */

#ifndef	_SCSI_SCSI2_H_
#define	_SCSI_SCSI2_H_

#include <scsi/scsi_endian.h>

/*
 * Single byte messages
 *
 * originator:	I-nitiator T-arget
 * T-support:	M-andatory O-ptional
 */

#define SCSI_ABORT_TAG			0x0d	/* O I 2 */
#define SCSI_CLEAR_QUEUE		0x0e	/* O I 2 */
#define SCSI_INITIATE_RECOVERY		0x0f	/* O IT2 */
#define SCSI_RELEASE_RECOVERY		0x10	/* O I 2 */
#define SCSI_TERMINATE_IO_PROCESS	0x11	/* O I 2 */

/*
 * Two byte messages
 */
#define SCSI_SIMPLE_QUEUE_TAG		0x20	/* O IT2 */
#define SCSI_HEADOF_QUEUE_TAG		0x21	/* O I 2 */
#define SCSI_ORDERED_QUEUE_TAG		0x22	/* O I 2 */
#define SCSI_IGNORE_WIDE_RESIDUE	0x23	/* O  T2 */
					/* 0x24..0x2f reserved */

/*
 * Extended messages, codes and formats
 */

#define SCSI_WIDE_XFER_REQUEST		0x03	/* IT 2 */
typedef struct {
	unsigned char	xtn_msg_tag;		/* const 0x01 */
	unsigned char	xtn_msg_len;		/* const 0x02 */
	unsigned char	xtn_msg_code;		/* const 0x03 */
	unsigned char	xtn_msg_xfer_width;
} scsi_wide_xfer_t;

/*
 * NOTE: some command-specific mods and extensions
 * are actually defined in the scsi.h file for
 * readability reasons
 */

				/* GROUP 1 */

#define	SCSI_CMD_READ_DEFECT_DATA	0x37	/* O2 disk opti */
typedef scsi_command_group_1	scsi_cmd_read_defect_t;
#	define SCSI_CMD_RDD_LIST_TYPE		0x07
#	define SCSI_CMD_RDD_GLIST		0x08
#	define SCSI_CMD_RDD_PLIST		0x10

#define SCSI_CMD_WRITE_BUFFER		0x3b	/* O2 all */
typedef scsi_command_group_1	scsi_cmd_write_buffer_t;
#	define SCSI_CMD_BUF_MODE		0x07
#	define scsi_cmd_buf_id			scs_cmd_lba1
#	define scsi_cmd_buf_offset1		scs_cmd_lba2
#	define scsi_cmd_buf_offset2		scs_cmd_lba3
#	define scsi_cmd_buf_offset3		scs_cmd_lba4
#	define scsi_cmd_buf_alloc1		scs_cmd_xxx
#	define scsi_cmd_buf_alloc2		scs_cmd_xfer_len_1
#	define scsi_cmd_buf_alloc3		scs_cmd_xfer_len_2

#define SCSI_CMD_READ_BUFFER		0x3c	/* O2 all */
#define	scsi_cmd_read_buffer_t scsi_command_group_1

				/* GROUP 2 */

#define SCSI_CMD_CHANGE_DEFINITION	0x40	/* O2 all */
#define	scsi_cmd_change_def_t	scsi_command_group_2
#	define scsi_cmd_chg_save		scsi_cmd_lba1
#	define scsi_cmd_chg_definition		scsi_cmd_lba2
#	define SCSI_CMD_CHG_CURRENT		0x00
#	define SCSI_CMD_CHG_SCSI_1		0x01
#	define SCSI_CMD_CHG_CCS			0x02
#	define SCSI_CMD_CHG_SCSI_2		0x03

					/* 0x41 reserved */

#define SCSI_CMD_READ_SUBCH		0x42	/* O2 rom */
#define	scsi_cmd_read_subch_t	scsi_command_group_2
#	define SCSI_CMD_CD_MSF			0x02
#	define SCSI_CMD_RS_SUBQ			0x40
#	define scsi_cmd_rs_format	scsi_cmd_lba2
#	define SCSI_CMD_RS_FMT_SUBQ		0x00
#	define SCSI_CMD_RS_FMT_CURPOS		0x01
#	define SCSI_CMD_RS_FMT_CATALOG		0x02
#	define SCSI_CMD_RS_FMT_ISRC		0x03
#	define scsi_cmd_rs_trackno	scsi_cmd_xxx


#define SCSI_CMD_READ_TOC		0x43	/* O2 rom */
#define	scsi_cmd_read_toc_t	scsi_command_group_2
#	define scsi_cmd_rtoc_startT	scsi_cmd_xxx

#define SCSI_CMD_READ_HEADER		0x44	/* O2 rom */
#define	scsi_cmd_read_header_t	scsi_command_group_2

#define SCSI_CMD_PLAY_AUDIO		0x45	/* O2 rom */
#define	scsi_cmd_play_audio_t	scsi_command_group_2

#define SCSI_CMD_PLAY_AUDIO_MSF		0x47	/* O2 rom */
#define	scsi_cmd_play_audio_msf_t scsi_command_group_2
#	define scsi_cmd_pamsf_startM	scsi_cmd_lba2
#	define scsi_cmd_pamsf_startS	scsi_cmd_lba3
#	define scsi_cmd_pamsf_startF	scsi_cmd_lba4
#	define scsi_cmd_pamsf_endM	scsi_cmd_xxx
#	define scsi_cmd_pamsf_endS	scsi_cmd_xfer_len_1
#	define scsi_cmd_pamsf_endF	scsi_cmd_xfer_len_2

#define SCSI_CMD_PLAY_AUDIO_TI		0x48	/* O2 rom */
#define	scsi_cmd_play_audio_ti_t scsi_command_group_2
#	define scsi_cmd_pati_startT	scsi_cmd_lba3
#	define scsi_cmd_pati_startI	scsi_cmd_lba4
#	define scsi_cmd_pati_endT	scsi_cmd_xfer_len_1
#	define scsi_cmd_pati_endI	scsi_cmd_xfer_len_2

#define SCSI_CMD_PLAY_AUDIO_TR		0x49	/* O2 rom */
#define	scsi_cmd_play_audio_tr_t scsi_command_group_2
#	define scsi_cmd_patr_startT	scsi_cmd_xxx


#define SCSI_CMD_PAUSE_RESUME		0x4b	/* O2 rom */
#define	scsi_cmd_pausres_t	scsi_command_group_2
#	define SCSI_CMD_PAUSRES_RESUME		0x01
#	define scsi_cmd_pausres_res	scsi_cmd_xfer_len_2

#define SCSI_CMD_LOG_SELECT		0x4c	/* O2 all */
#define	scsi_cmd_logsel_t	scsi_command_group_2
#	define SCSI_CMD_LOG_SP			0x01
#	define SCSI_CMD_LOG_PCR			0x02
#	define scsi_cmd_log_page_control	scsi_cmd_lba1

#define SCSI_CMD_LOG_SENSE		0x4d	/* O2 all */
#define	scsi_cmd_logsense_t	scsi_command_group_2
#	define SCSI_CMD_LOG_PPC			0x02
#	define scsi_cmd_log_page_code		scsi_cmd_lba1
#	define scsi_cmd_log_param_ptr1		scsi_cmd_lba4
#	define scsi_cmd_log_param_ptr2		scsi_cmd_xxx


					/* 0x4e..0x54 reserved */

#define SCSI_CMD_MODE_SELECT_2		0x55	/* Z2 */
#define	scsi_cmd_mode_select_long_t	scsi_command_group_2
#	define SCSI_CMD_MSL2_PF		0x10
#	define SCSI_CMD_MSL2_SP		0x01

					/* 0x56..0x59 reserved */

#define SCSI_CMD_MODE_SENSE_2		0x5a	/* Z2 */
#define	scsi_cmd_mode_sense_long_t	scsi_command_group_2
#	define SCSI_CMD_MSS2_DBD	0x08

					/* 0x5b..0x5f reserved */

				/* GROUP 5 */

#define SCSI_CMD_PLAY_AUDIO_LONG	0xa5	/* O2 rom */
#define	scsi_cmd_play_audio_l_t		scsi_command_group_5

#define SCSI_CMD_PLAY_AUDIO_TR_LONG	0xa9	/* O2 rom */
#define	scsi_cmd_play_audio_tr_l_t	scsi_command_group_5
#	define scsi_cmd_patrl_startT	scsi_cmd_xxx1


/*
 * Command specific defines
 */
typedef struct {
	BITFIELD_2(unsigned char,
			periph_type : 5,
			periph_qual : 3);
#define	SCSI_SCANNER		0x06	/* periph_type values */
#define	SCSI_MEMORY		0x07
#define	SCSI_J_BOX		0x08
#define	SCSI_COMM		0x09
#define	SCSI_PREPRESS1		0x0a
#define	SCSI_PREPRESS2		0x0b

#define	SCSI_PERIPH_CONNECTED	0x00	/* periph_qual values */
#define	SCSI_PERIPH_DISCONN	0x20
#define	SCSI_PERIPH_NOTHERE	0x30

	BITFIELD_2(unsigned char,
			device_type : 7,
			rmb : 1);

	BITFIELD_3( unsigned char,
			ansi : 3,
			ecma : 3,
			iso : 2);

	BITFIELD_4( unsigned char,
			response_fmt : 4,
			res1 : 2,
			trmIOP : 1,
			aenc : 1);
	unsigned char	length;
	unsigned char	res2;
	unsigned char	res3;

	BITFIELD_8(unsigned char,
			SftRe : 1,
			CmdQue : 1,
			res4 : 1,
			Linked : 1,
			Sync : 1,
			Wbus16 : 1,
			Wbus32 : 1,
			RelAdr : 1);

	unsigned char	vendor_id[8];
	unsigned char	product_id[16];
	unsigned char	product_rev[4];
	unsigned char	vendor_uqe[20];
	unsigned char	reserved[40];
	unsigned char	vendor_uqe1[1];	/* varsize */
} scsi2_inquiry_data_t;
#define	SCSI_INQ_SUPP_PAGES	0x00
#define	SCSI_INQ_A_INFO		0x01	/* 0x01..0x1f, really */
#define	SCSI_INQ_SERIALNO	0x80
#define	SCSI_INQ_IMPL_OPDEF	0x81
#define	SCSI_INQ_A_IMPL_OPDEF	0x82

/* mode_select */
typedef struct {
	unsigned char	data_len;
	unsigned char	medium_type;
	unsigned char	device_specific;
	unsigned char	desc_len;
	/* block descriptors are optional, same struct as scsi1 */
	/* page info then follows, see individual pages */
} scsi2_mode_param_t;

/*
 * CDROM thingies
 */
typedef union {
	struct {
		unsigned char	xxx;
		unsigned char	minute;
		unsigned char	second;
		unsigned char	frame;
	} msf;
	struct {
		unsigned char	lba1;
		unsigned char	lba2;
		unsigned char	lba3;
		unsigned char	lba4;
	} lba;
} cdrom_addr_t;

typedef struct {
	unsigned char	len1;		/* MSB */
	unsigned char	len2;		/* LSB */
	unsigned char	first_track;
	unsigned char	last_track;
	struct cdrom_toc_desc {

		unsigned char	xxx;

		BITFIELD_2(unsigned char,
			control : 4,
			adr : 4);

		unsigned char	trackno;
		unsigned char	xxx1;
		cdrom_addr_t	absolute_address;
	} descs[1];			/* varsize */
} cdrom_toc_t;

typedef struct {
	unsigned char	xxx;

	unsigned char	audio_status;
#define SCSI_CDST_INVALID	0x00
#define SCSI_CDST_PLAYING	0x11
#define SCSI_CDST_PAUSED	0x12
#define SCSI_CDST_COMPLETED	0x13
#define SCSI_CDST_ERROR		0x14
#define SCSI_CDST_NO_STATUS	0x15

	unsigned char	len1;
	unsigned char	len2;
	struct cdrom_chanQ {
		unsigned char	format;
		BITFIELD_2(unsigned char,
			control : 4,
			adr : 4);
		unsigned char	trackno;
		unsigned char	indexno;
		cdrom_addr_t	absolute_address;
		cdrom_addr_t	relative_address;
		BITFIELD_2(unsigned char,
			xxx : 7,
			mcv : 1);
		unsigned char	catalog[15];
		BITFIELD_2(unsigned char,
			xxx1 : 7,
			tcv  : 1);
		unsigned char	isrc[15];
	} subQ;
} cdrom_chan_data_t;

/* subsets */
typedef struct {
	unsigned char	xxx;
	unsigned char	audio_status;
	unsigned char	len1;
	unsigned char	len2;
	struct {
		unsigned char	format;
		BITFIELD_2(unsigned char,
			control : 4,
			adr : 4);
		unsigned char	trackno;
		unsigned char	indexno;
		cdrom_addr_t	absolute_address;
		cdrom_addr_t	relative_address;
	} subQ;
} cdrom_chan_curpos_t;

typedef struct {
	unsigned char	xxx;
	unsigned char	audio_status;
	unsigned char	len1;
	unsigned char	len2;
	struct {
		unsigned char	format;
		unsigned char	xxx1[3];
		BITFIELD_2(unsigned char,
			xxx : 7,
			mcv : 1);
		unsigned char	catalog[15];
	} subQ;
} cdrom_chan_catalog_t;

typedef struct {
	unsigned char	xxx;
	unsigned char	audio_status;
	unsigned char	len1;
	unsigned char	len2;
	struct {
		unsigned char	format;
		BITFIELD_2(unsigned char,
			control : 4,
			adr : 4);
		unsigned char	trackno;
		unsigned char	xxx0;
		BITFIELD_2(unsigned char,
			xxx1 : 7,
			tcv  : 1);
		unsigned char	isrc[15];
	} subQ;
} cdrom_chan_isrc_t;

/* Audio page */
typedef struct {
	scsi_mode_sense_data_t	h;	/* includes bdescs */
	unsigned char	page_code;
#define	SCSI_CD_AUDIO_PAGE	0x0e
	unsigned char	page_len;
	BITFIELD_4(unsigned char,
		xxx1 : 1,
		sotc : 1,
		imm  : 1,
		xxx2 : 5);
	unsigned char	xxx3[2];
	BITFIELD_3(unsigned char,
		fmt : 4,
		xxx4 : 3,
		aprv : 1);
	unsigned char	bps1;
	unsigned char	bps2;
	BITFIELD_2(unsigned char,
		sel0 : 4,
		xxx5 : 4);
	unsigned char	vol0;
	BITFIELD_2(unsigned char,
		sel1 : 4,
		xxx6 : 4);
	unsigned char	vol1;
	BITFIELD_2(unsigned char,
		sel2 : 4,
		xxx7 : 4);
	unsigned char	vol2;
	BITFIELD_2(unsigned char,
		sel3 : 4,
		xxx8 : 4);
	unsigned char	vol3;
} cdrom_audio_page_t;

/*
 * Status byte (a-la scsi2)
 */

typedef union {
    struct {
	BITFIELD_3( unsigned char,
			scsi_status_reserved1:1,
			scsi_status_code:5,
			scsi_status_reserved2:2);
							/* more scsi_status_code values */
					/* 00..0c as in SCSI-I */
#	define SCSI_ST2_CMD_TERMINATED	0x11	/* 2 */
#	define SCSI_ST2_QUEUE_FULL	0x14	/* 2 */
					/* anything else is reserved */
    } st;
    unsigned char bits;
} scsi2_status_byte_t;

#endif	_SCSI_SCSI2_H_
