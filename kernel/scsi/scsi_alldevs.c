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
 *	File: scsi_alldevs.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Middle layer of the SCSI driver: SCSI protocol implementation
 *	This file contains code for SCSI commands defined for all device types.
 */

#include <mach/std_types.h>
#include <sys/types.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#if  (NSCSI > 0)

void scsi_print_add_sense_keys(); /* forward */

/*
 * Utilities
 */
void scsi_go_and_wait(tgt, insize, outsize, ior)
	target_info_t	*tgt;
	int insize, outsize;
	io_req_t	ior;
{
	register scsi_softc_t	*sc = scsi_softc[(unsigned char)tgt->masterno];

	tgt->ior = ior;

	(*sc->go)(tgt, insize, outsize, ior==0);

	if (ior)
		iowait(ior);
	else
		while (tgt->done == SCSI_RET_IN_PROGRESS);
}

void scsi_go(tgt, insize, outsize, cmd_only)
	target_info_t	*tgt;
	int insize, outsize, cmd_only;
{
	register scsi_softc_t	*sc = scsi_softc[(unsigned char)tgt->masterno];

	(*sc->go)(tgt, insize, outsize, cmd_only);
}

int sizeof_scsi_command(
	unsigned char	cmd)
{
	switch ((cmd & SCSI_CODE_GROUP) >> 5) {
	    case 0: return sizeof(scsi_command_group_0);
	    case 1: return sizeof(scsi_command_group_1);
	    case 2: return sizeof(scsi_command_group_2);
	    /* 3,4 reserved */
	    case 5: return sizeof(scsi_command_group_5);
	    /* 6,7 vendor specific (!!) */
	    case 6: return sizeof(scsi_command_group_2);
	}
}

/*
 * INQUIRY (Almost mandatory)
 */
int scsi_inquiry( tgt, pagecode)
	register target_info_t	*tgt;
	int			pagecode;
{
	scsi_cmd_inquiry_t	*cmd;
	boolean_t		no_ify = TRUE;

retry:
	cmd = (scsi_cmd_inquiry_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_INQUIRY;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba3 = 0;
	cmd->scsi_cmd_xfer_len = 0xff;	/* max len always */
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
/*#ifdef	SCSI2*/
	if (pagecode != SCSI_INQ_STD_DATA) {
		cmd->scsi_cmd_lun_and_lba1 |= SCSI_CMD_INQ_EVPD;
		cmd->scsi_cmd_page_code = pagecode;
	} else
/*#endif	SCSI2*/
		cmd->scsi_cmd_page_code = 0;

	tgt->cur_cmd = SCSI_CMD_INQUIRY;

	/*
	 * Note: this is sent when we do not know much about the
	 * target, so we might not put an identify message upfront
	 */
	scsi_go(tgt, sizeof(*cmd), 0xff, no_ify);

	/*
	 * This spin loop is because we are called at autoconf
	 * time where we cannot thread_block(). Sigh.
	 */
	while (tgt->done == SCSI_RET_IN_PROGRESS) ;
	if (tgt->done == SCSI_RET_RETRY)	/* sync negotiation ? */
		goto retry;
	if ((tgt->done != SCSI_RET_SUCCESS) && no_ify) {
		no_ify = FALSE;
		goto retry;
	}
	return tgt->done;
}

void scsi_print_inquiry( inq, pagecode, result)
	scsi2_inquiry_data_t	*inq;
	int 			pagecode;
	char			*result;
{
	static char *periph_names[10] = {
		"disk", "tape", "printer", "processor", "WORM-disk",
		"CD-ROM", "scanner", "memory", "jukebox", "communication"
	};
	static char *periph_state[4] = {
		"online", "offline", "?", "absent"
	};

	char dev[SCSI_TARGET_NAME_LEN], *devname;
	register int i, j = 0;

	if (pagecode != SCSI_INQ_STD_DATA)
		return;

	devname = result ? result : dev;

	if (!result) {
		printf("\n\t%s%s %s (%s %x)",
			(inq->rmb) ? "" : "non-", "removable SCSI",
			(inq->periph_type > 10) ?
				"?device?" : periph_names[inq->periph_type],
			periph_state[inq->periph_qual & 0x3],
			inq->device_type);
		printf("\n\t%s%s%s",
			inq->iso ? "ISO-compliant, " : "",
			inq->ecma ? "ECMA-compliant, " : "",
			inq->ansi ? "ANSI-compliant, " : "");
		if (inq->ansi)
			printf("%s%d, ", "SCSI-", inq->ansi);
		if (inq->response_fmt == 2)
			printf("%s%s%s%s%s%s%s%s%s%s%s", "Supports: ",
			inq->aenc ? "AENC, " : "",
			inq->trmIOP ? "TrmIOP, " : "",
			inq->RelAdr ? "RelAdr, " : "",
			inq->Wbus32 ? "32 bit xfers, " : "",
			inq->Wbus16 ? "16 bis xfers, " : "",
			inq->Sync ? "Sync xfers, " : "",
			inq->Linked ? "Linked cmds, " : "",
			inq->CmdQue ? "Tagged cmd queues, " : "",
			inq->SftRe ? "Soft" : "Hard", " RESET, ");
	}

	for (i = 0; i < 8; i++)
		if (inq->vendor_id[i] != ' ')
			devname[j++] = inq->vendor_id[i];
	devname[j++] = ' ';
	for (i = 0; i < 16; i++)
		if (inq->product_id[i] != ' ')
			devname[j++] = inq->product_id[i];
	devname[j++] = ' ';
	for (i = 0; i < 4; i++)
		if (inq->product_rev[i] != ' ')
			devname[j++] = inq->product_rev[i];
#if unsafe
	devname[j++] = ' ';
	for (i = 0; i < 8; i++)
		if (inq->vendor_uqe[i] != ' ')
			devname[j++] = inq->vendor_uqe[i];
#endif
	devname[j] = 0;

	if (!result)
		printf("(%s, %s%s)\n", devname, "SCSI ",
			(inq->periph_type > 10) ?
				"?device?" : periph_names[inq->periph_type]);
}

/*
 * REQUESTE SENSE (Mandatory, All)
 */

int scsi_request_sense(tgt, ior, data)
	register target_info_t	*tgt;
	io_req_t		ior;
	char			**data;
{
	scsi_cmd_request_sense_t *cmd;

	cmd = (scsi_cmd_request_sense_t *) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_REQUEST_SENSE;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_lba3 = 0;
	cmd->scsi_cmd_allocation_length = 0xff;	/* max len always */
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */

	tgt->cur_cmd = SCSI_CMD_REQUEST_SENSE;

	if (ior==0)
		scsi_go_and_wait (tgt, sizeof(*cmd), 0xff, ior);
	else {
		scsi_go(tgt, sizeof(*cmd), 0xff, FALSE);
		return tgt->done;
	}

	if (data)
		*data = tgt->cmd_ptr;

	(void) scsi_check_sense_data(tgt, tgt->cmd_ptr);

	return tgt->done;
}

boolean_t
scsi_check_sense_data(tgt, sns)
	register target_info_t	*tgt;
	scsi_sense_data_t	*sns;
{
	unsigned char   code;

	if (sns->error_class != SCSI_SNS_XTENDED_SENSE_DATA) {
		printf("Bad sense data, vuqe class x%x code x%x\n",
			sns->error_class, sns->error_code);
		return FALSE;	/* and good luck */
	} else {
		code = sns->u.xtended.sense_key;

		switch (code) {
		case SCSI_SNS_NOSENSE:
		case SCSI_SNS_EQUAL:
			return TRUE;
			break;
		case SCSI_SNS_RECOVERED:
			scsi_error(tgt, SCSI_ERR_BAD | SCSI_ERR_SENSE,
				   code, sns->u.xtended.add_bytes);
			return TRUE;
			break;
		case SCSI_SNS_UNIT_ATN:
			scsi_error(tgt, SCSI_ERR_SENSE,
				   code, sns->u.xtended.add_bytes);
			return TRUE;
			break;
		case SCSI_SNS_NOTREADY:
			tgt->done = SCSI_RET_RETRY;
			return TRUE;
		case SCSI_SNS_ILLEGAL_REQ:
			if (tgt->flags & TGT_OPTIONAL_CMD)
				return TRUE;
			/* fall through */
		default:
/* e.g.
		case SCSI_SNS_MEDIUM_ERR:
		case SCSI_SNS_HW_ERR:
		case SCSI_SNS_PROTECT:
		case SCSI_SNS_BLANK_CHK:
		case SCSI_SNS_VUQE:
		case SCSI_SNS_COPY_ABRT:
		case SCSI_SNS_ABORTED:
		case SCSI_SNS_VOLUME_OVFL:
		case SCSI_SNS_MISCOMPARE:
		case SCSI_SNS_RESERVED:
*/
			scsi_error(tgt, SCSI_ERR_GRAVE|SCSI_ERR_SENSE,
				   code, sns->u.xtended.add_bytes);
			return FALSE;
			break;
		}
	}
}

/*
 * START STOP UNIT (Optional, disk prin work rom tape[load/unload])
 */
int scsi_start_unit( tgt, ss, ior)
	register target_info_t	*tgt;
	int 			ss;
	io_req_t		ior;
{
	scsi_cmd_start_t	*cmd;

	cmd = (scsi_cmd_start_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_START_STOP_UNIT;
	cmd->scsi_cmd_lun_and_lba1 = SCSI_CMD_SS_IMMED;/* 0 won't work ? */
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_lba3 = 0;
	cmd->scsi_cmd_ss_flags = ss;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_START_STOP_UNIT;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);
	return tgt->done;
}

/*
 * TEST UNIT READY (Optional, All)
 * Note: this is where we do the synch negotiation at autoconf
 */
int scsi_test_unit_ready( tgt, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_test_unit_ready_t	*cmd;

	cmd = (scsi_cmd_test_unit_ready_t*) (tgt->cmd_ptr);

	cmd->scsi_cmd_code = SCSI_CMD_TEST_UNIT_READY;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_lba3 = 0;
	cmd->scsi_cmd_ss_flags = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_TEST_UNIT_READY;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

	return tgt->done;
}

/*
 * RECEIVE DIAGNOSTIC RESULTS (Optional, All)
 */
int scsi_receive_diag( tgt, result, result_len, ior)
	register target_info_t	*tgt;
	char			*result;
	int			result_len;
	io_req_t		ior;
{
	scsi_cmd_receive_diag_t	*cmd;

	cmd = (scsi_cmd_receive_diag_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_RECEIVE_DIAG_RESULTS;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_lba3 = result_len >> 8 & 0xff;
	cmd->scsi_cmd_xfer_len = result_len & 0xff;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_RECEIVE_DIAG_RESULTS;

	scsi_go_and_wait(tgt, sizeof(*cmd), result_len, ior);

	bcopy(tgt->cmd_ptr, (char*)result, result_len);

	return tgt->done;
}


int scsi_mode_sense( tgt, pagecode, len, ior)
	register target_info_t	*tgt;
	int			pagecode;
	int			len;
	io_req_t		ior;
{
	scsi_cmd_mode_sense_t	*cmd;

	cmd = (scsi_cmd_mode_sense_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_MODE_SENSE;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_ms_pagecode = pagecode;
	cmd->scsi_cmd_lba3 = 0;
	cmd->scsi_cmd_xfer_len = len;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_MODE_SENSE;

	scsi_go_and_wait(tgt, sizeof(*cmd), len, ior);

	return tgt->done;
}

#if	0 /* unused */

/*
 * COPY (Optional, All)
 */
void scsi_copy( tgt, params, params_len, ior)
	register target_info_t	*tgt;
	char			*params;
	io_req_t		ior;
{
	scsi_cmd_copy_t	*cmd;

	cmd = (scsi_cmd_copy_t*) (tgt->cmd_ptr;
	cmd->scsi_cmd_code = SCSI_CMD_COPY;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 = params_len>>16 & 0xff;
	cmd->scsi_cmd_lba3 = params_len >> 8 & 0xff;
	cmd->scsi_cmd_xfer_len = params_len & 0xff;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	bcopy(params, cmd + 1, params_len);

	tgt->cur_cmd = SCSI_CMD_COPY;

	scsi_go_and_wait(tgt, sizeof(*cmd) + params_len, 0, ior);
}

/*
 * SEND DIAGNOSTIC (Optional, All)
 */
void scsi_send_diag( tgt, flags, params, params_len, ior)
	register target_info_t	*tgt;
	char			*params;
	io_req_t		ior;
{
	scsi_cmd_send_diag_t	*cmd;

	cmd = (scsi_cmd_send_diag_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_SEND_DIAGNOSTICS;
	cmd->scsi_cmd_lun_and_lba1 = flags & 0x7;
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_lba3 = params_len >> 8 & 0xff;
	cmd->scsi_cmd_xfer_len = params_len & 0xff;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	bcopy(params, cmd + 1, params_len);

	tgt->cur_cmd = SCSI_CMD_SEND_DIAGNOSTICS;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);
}

/*
 * COMPARE (Optional, All)
 */
void scsi_compare( tgt, params, params_len, ior)
	register target_info_t	*tgt;
	char			*params;
	io_req_t		ior;
{
	scsi_cmd_compare_t	*cmd;

	cmd = (scsi_cmd_compare_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_COMPARE;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 = 0;
	cmd->scsi_cmd_1_paraml1 = params_len >> 16 & 0xff;
	cmd->scsi_cmd_1_paraml2 = params_len >> 8 & 0xff;
	cmd->scsi_cmd_1_paraml3 = params_len & 0xff;
	cmd->scsi_cmd_xxx = 0;
	cmd->scsi_cmd_xfer_len_1 = 0;
	cmd->scsi_cmd_xfer_len_2 = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	bcopy(params, cmd + 1, params_len);

	tgt->cur_cmd = SCSI_CMD_COMPARE;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);
}

/*
 * COPY AND VERIFY (Optional, All)
 */
void scsi_copy_and_verify( tgt, params, params_len, bytchk, ior)
	register target_info_t	*tgt;
	char			*params;
	io_req_t		ior;
{
	scsi_cmd_compare_t	*cmd;

	cmd = (scsi_cmd_compare_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_COMPARE;
	cmd->scsi_cmd_lun_and_relbit = bytchk ? SCSI_CMD_CPY_BYTCHK : 0;
	cmd->scsi_cmd_lba1 = 0;
	cmd->scsi_cmd_1_paraml1 = params_len >> 16 & 0xff;
	cmd->scsi_cmd_1_paraml2 = params_len >> 8 & 0xff;
	cmd->scsi_cmd_1_paraml3 = params_len & 0xff;
	cmd->scsi_cmd_xxx = 0;
	cmd->scsi_cmd_xfer_len_1 = 0;
	cmd->scsi_cmd_xfer_len_2 = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	bcopy(params, cmd + 1, params_len);

	tgt->cur_cmd = SCSI_CMD_COMPARE;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);
}

#endif

#ifdef	SCSI2
scsi_change_definition
scsi_log_select
scsi_log_sense
scsi_long_mode_select
scsi_read_buffer
scsi_write_buffer
#endif	SCSI2

/*
 * Warn user of some device error
 */
int	scsi_debug = 0;

static char *sns_msg[SCSI_SNS_RESERVED+1] = {
	"No Sense Data",/* shouldn't happen */
	"Recovered",
	"Unit not ready",
	"Medium",
	"Hardware failure",
	"Illegal request",
	"Unit Attention Condition",
	"Protection",
	"Blank Check",
	"Vendor Unique",
	"Copy Operation Aborted",
	"Aborted Command",
	"Equal Comparison",
	"Volume Overflow",
	"Miscompare",
	"Reserved"
};

void
scsi_error( tgt, code, info, addtl)
	target_info_t	*tgt;
	unsigned	code;
	unsigned	info;
	char		*addtl;
{
	char		unit;
	char		*msg, *cmd;
	scsi2_status_byte_t	status;
	if (scsi_debug)
		code |= SCSI_ERR_GRAVE;

	if (tgt)
		unit = tgt->unit_no + '0';
	else
		unit = '?';


	switch (SCSI_ERR_CLASS(code)) {
	case SCSI_ERR_STATUS:
		cmd = "Bad status return";
		status.bits = info;
		switch (status.st.scsi_status_code) {
		case SCSI_ST_GOOD:
		case SCSI_ST_CONDITION_MET:
		case SCSI_ST_INT_GOOD:
		case SCSI_ST_INT_MET:
			return;	/* all is fine */
		case SCSI_ST_CHECK_CONDITION:
			msg = "Check condition"; break;
		case SCSI_ST_RES_CONFLICT:
			msg = "Reservation conflict"; break;
		case SCSI_ST_BUSY:
			msg = "Target busy"; break;
		case SCSI_ST2_QUEUE_FULL:
			msg = "Queue full"; break;
		case SCSI_ST2_CMD_TERMINATED:
			msg = "Command terminated"; break;
		default:
			msg = "Strange"; break;
		}
		break;
	case SCSI_ERR_SENSE:
		cmd = "Sensed a";
		msg = sns_msg[info & 0xf];
		break;
	case SCSI_ERR_MSEL:
		cmd = "Mode select broken"; msg = ""; break;
	default:
		cmd = "Generic"; msg = "";
	}
	if (SCSI_ERR_GRAVITY(code)) {
		printf("\n%s%c: %s %s %sx%x", "target ", unit, cmd, msg,
			"error, code ", info);
		if (addtl) {
			unsigned int	add[3];
			bcopy(addtl, (char*)add, 3*sizeof(int));
			printf("%s x%x x%x x%x", ", additional info ",
				add[0], add[1], add[2]);
		}
		printf("\n");
	}
}

void scsi_print_sense_data(sns)
	scsi_sense_data_t	*sns;
{
	printf("Sense data: %s%s, segment %d", 
		sns_msg[sns->u.xtended.sense_key], " error",
		sns->u.xtended.segment_number);
	if (sns->u.xtended.ili) printf(", IncorrectLengthIndicator");
	if (sns->u.xtended.eom) printf(", EndOfMedium");
	if (sns->u.xtended.fm) printf(", FileMark");

	if (sns->addr_valid) {
		unsigned int info;
		info =  (sns->u.xtended.info0 << 24) |
			(sns->u.xtended.info1 << 16) |
			(sns->u.xtended.info2 <<  8) |
			 sns->u.xtended.info3;
		printf(", Info x%x", info);
	}

	if (sns->u.xtended.add_len > 6)
		scsi_print_add_sense_keys(sns->u.xtended.add_bytes[4],
					  sns->u.xtended.add_bytes[5]);
}

/*
 * Table of the official SCSI-2 error messages
 * Last update:
 *	X3T9.2/86-109, Revision 10c, March 9, 1990
 */
static struct addtl_sns_keys_msg {
	unsigned char	byte12;
	unsigned char	byte13;
	char		*means;
} addtl_sns_msgs[] = {
	{ 0x00, 0x00, "No additional sense information" },
	{ 0x00, 0x01, "Filemark detected" },
	{ 0x00, 0x02, "End-of-partition/medium detected" },
	{ 0x00, 0x03, "Setmark detected" },
	{ 0x00, 0x04, "Beginning of partition/medium detected" },
	{ 0x00, 0x05, "End-of-data detected" },
	{ 0x00, 0x06, "I/O process terminated" },
	{ 0x00, 0x11, "Audio play operation in progress" },
	{ 0x00, 0x12, "Audio play operation paused" },
	{ 0x00, 0x13, "Audio play operation successfully completed" },
	{ 0x00, 0x14, "Audio play operation stopped due to error" },
	{ 0x00, 0x15, "No current audio status to return" },
	{ 0x01, 0x00, "No index/sector signal" },
	{ 0x02, 0x00, "No seek complete" },
	{ 0x03, 0x00, "Peripheral device write fault" },
	{ 0x03, 0x01, "No write current" },
	{ 0x03, 0x02, "Excessive write errors" },
	{ 0x04, 0x00, "Logical unit not ready, cause not reportable" },
	{ 0x04, 0x01, "Logical unit is in process of becoming ready" },
	{ 0x04, 0x02, "Logical unit not ready, initializing command required" },
	{ 0x04, 0x03, "Logical unit not ready, manual intervention required" },
	{ 0x04, 0x04, "Logical unit not ready, format in progress" },
	{ 0x05, 0x00, "Logical unit does not respond to selection" },
	{ 0x06, 0x00, "No reference position found" },
	{ 0x07, 0x00, "Multiple peripheral devices selected" },
	{ 0x08, 0x00, "Logical unit communication failure" },
	{ 0x08, 0x01, "Logical unit communication time-out" },
	{ 0x08, 0x02, "Logical unit communication parity error" },
	{ 0x09, 0x00, "Track following error" },
	{ 0x09, 0x01, "Tracking servo failure" },
	{ 0x09, 0x02, "Focus servo failure" },
	{ 0x09, 0x03, "Spindle servo failure" },
	{ 0x0a, 0x00, "Error log overflow" },
	{ 0x0c, 0x00, "Write error" },
	{ 0x0c, 0x01, "Write error recovered with auto-reallocation" },
	{ 0x0c, 0x02, "Write error - auto-reallocation failed" },
	{ 0x10, 0x00, "Id CRC or ECC error" },
	{ 0x10, 0x04, "Recovered data with LEC" },
	{ 0x11, 0x00, "Unrecovered read error" },
	{ 0x11, 0x01, "Read retries exhausted" },
	{ 0x11, 0x02, "Error too long to correct" },
	{ 0x11, 0x03, "Multiple read errors" },
	{ 0x11, 0x04, "Unrecovered read error - auto-reallocate failed" },
	{ 0x11, 0x05, "L-EC uncorrectable error" },
	{ 0x11, 0x06, "CIRC unrecovered error" },
	{ 0x11, 0x07, "Data resynchronization error" },
	{ 0x11, 0x08, "Incomplete block read" },
	{ 0x11, 0x09, "No gap found" },
	{ 0x11, 0x0a, "Miscorrected error" },
	{ 0x11, 0x0b, "Unrecovered read error - recommend reassignment" },
	{ 0x11, 0x0c, "Unrecovered read error - recommend rewrite the data" },
	{ 0x12, 0x00, "Address mark not found for id field" },
	{ 0x13, 0x00, "Address mark not found for data field" },
	{ 0x14, 0x00, "Recorded entity not found" },
	{ 0x14, 0x01, "Record not found" },
	{ 0x14, 0x02, "Filemark or setmark not found" },
	{ 0x14, 0x03, "End-of-data not found" },
	{ 0x14, 0x04, "Block sequence error" },
	{ 0x15, 0x00, "Random positioning error" },
	{ 0x15, 0x01, "Mechanical positioning error" },
	{ 0x15, 0x02, "Positioning error detected by read of medium" },
	{ 0x16, 0x00, "Data synchronization mark error" },
	{ 0x17, 0x00, "Recovered data with no error correction applied" },
	{ 0x17, 0x01, "Recovered data with retries" },
	{ 0x17, 0x02, "Recovered data with positive head offset" },
	{ 0x17, 0x03, "Recovered data with negative head offset" },
	{ 0x17, 0x04, "Recovered data with retries and/or CIRC applied" },
	{ 0x17, 0x05, "Recovered data using previous sector id" },
	{ 0x17, 0x06, "Recovered data without ECC - data auto-reallocated" },
	{ 0x17, 0x07, "Recovered data without ECC - recommend reassignment" },
	{ 0x18, 0x00, "Recovered data with error correction applied" },
	{ 0x18, 0x01, "Recovered data with error correction and retries applied" },
	{ 0x18, 0x02, "Recovered data - data auto-reallocated" },
	{ 0x18, 0x03, "Recovered data with CIRC" },
	{ 0x18, 0x05, "Recovered data - recommended reassignment" },
	{ 0x19, 0x00, "Defect list error" },
	{ 0x19, 0x01, "Defect list not available" },
	{ 0x19, 0x02, "Defect list error in primary list" },
	{ 0x19, 0x03, "Defect list error in grown list" },
	{ 0x1a, 0x00, "Parameter list length error" },
	{ 0x1b, 0x00, "Synchronous data transfer error" },
	{ 0x1c, 0x00, "Defect list not found" },
	{ 0x1c, 0x01, "Primary defect list not found" },
	{ 0x1c, 0x02, "Grown defect list not found" },
	{ 0x1d, 0x00, "Miscompare during verify operation" },
	{ 0x1e, 0x00, "Recovered id with ECC correction" },
	{ 0x20, 0x00, "Invalid command operation code" },
	{ 0x21, 0x00, "Logical block address out of range" },
	{ 0x21, 0x01, "Invalid element address" },
	{ 0x22, 0x00, "Illegal function" },
	{ 0x24, 0x00, "Invalid field in CDB" },
	{ 0x24, 0x02, "Log parameters changed" },
	{ 0x25, 0x00, "Logical unit not supported" },
	{ 0x26, 0x00, "Invalid field in parameter list" },
	{ 0x26, 0x01, "Parameter not supported" },
	{ 0x26, 0x02, "Parameter value invalid" },
	{ 0x26, 0x03, "Threshold parameters not supported" },
	{ 0x27, 0x00, "Write protected" },
	{ 0x28, 0x00, "Not ready to ready transition (medium may have changed)" },
	{ 0x28, 0x01, "Import or export element accessed" },
	{ 0x29, 0x00, "Power on, reset, or bus device reset occurred" },
	{ 0x2a, 0x00, "Parameters changed" },
	{ 0x2a, 0x01, "Mode parameters changed" },
	{ 0x2b, 0x00, "Copy cannot execute since host cannot disconnect" },
	{ 0x2c, 0x00, "Command sequence error" },
	{ 0x2c, 0x01, "Too many windows specified" },
	{ 0x2c, 0x02, "Invalid combination of windows specified" },
	{ 0x2d, 0x00, "Overwrite error on update in place" },
	{ 0x2f, 0x00, "Commands cleared by another initiator" },
	{ 0x30, 0x00, "Incompatible medium installed" },
	{ 0x30, 0x01, "Cannot read medium - unknown format" },
	{ 0x30, 0x02, "Cannot read medium - incompatible format" },
	{ 0x30, 0x03, "Cleaning cartridge installed" },
	{ 0x31, 0x00, "Medium format corrupted" },
	{ 0x31, 0x01, "Format command failed" },
	{ 0x32, 0x00, "No defect spare location available" },
	{ 0x32, 0x01, "Defect list update failure" },
	{ 0x33, 0x00, "Tape length error" },
	{ 0x36, 0x00, "Ribbon, ink, or toner failure" },
	{ 0x37, 0x00, "Rounded parameter" },
	{ 0x39, 0x00, "Saving parameters not supported" },
	{ 0x3a, 0x00, "Medium not present" },
	{ 0x3b, 0x00, "Sequential positioning error" },
	{ 0x3b, 0x01, "Tape position error at beginning of medium" },
	{ 0x3b, 0x02, "Tape position error at end of medium" },
	{ 0x3b, 0x03, "Tape or electronic vertical forms unit not ready" },
	{ 0x3b, 0x04, "Slew failure" },
	{ 0x3b, 0x05, "Paper jam" },
	{ 0x3b, 0x06, "Failed to sense top-of-form" },
	{ 0x3b, 0x07, "Failed to sense bottom-of-form" },
	{ 0x3b, 0x08, "Reposition error" },
	{ 0x3b, 0x09, "Read past end of medium" },
	{ 0x3b, 0x0a, "Read past beginning of medium" },
	{ 0x3b, 0x0b, "Position past end of medium" },
	{ 0x3b, 0x0c, "Position past beginning of medium" },
	{ 0x3b, 0x0d, "Medium destination element full" },
	{ 0x3b, 0x0e, "Medium source element empty" },
	{ 0x3d, 0x00, "Invalid bits in identify message" },
	{ 0x3e, 0x00, "Logical unit has not self-configured yet" },
	{ 0x3f, 0x00, "Target operating conditions have changed" },
	{ 0x3f, 0x01, "Microcode has been changed" },
	{ 0x3f, 0x02, "Changed operating definition" },
	{ 0x3f, 0x03, "Inquiry data has changed" },
	{ 0x40, 0x00, "RAM failure" },
	{ 0x40, 0xff, "Diagnostic failure on component <NN>" },
	{ 0x41, 0x00, "Data path failure" },
	{ 0x42, 0x00, "Power on or self-test failure" },
	{ 0x43, 0x00, "Message error" },
	{ 0x44, 0x00, "Internal target failure" },
	{ 0x45, 0x00, "Select or reselect failure" },
	{ 0x46, 0x00, "Unsuccessful soft reset" },
	{ 0x47, 0x00, "SCSI parity error" },
	{ 0x48, 0x00, "Initiator detected message received" },
	{ 0x49, 0x00, "Invalid message error" },
	{ 0x4a, 0x00, "Command phase error" },
	{ 0x4b, 0x00, "Data phase error" },
	{ 0x4c, 0x00, "Logical unit failed self-configuration" },
	{ 0x4e, 0x00, "Overlapped commands attempted" },
	{ 0x50, 0x00, "Write append error" },
	{ 0x50, 0x01, "Write append position error" },
	{ 0x50, 0x02, "Position error related to timing" },
	{ 0x51, 0x00, "Erase failure" },
	{ 0x52, 0x00, "Cartridge fault" },
	{ 0x53, 0x00, "Media load or eject failed" },
	{ 0x53, 0x01, "Unload tape failure" },
	{ 0x53, 0x02, "Medium removal prevented" },
	{ 0x54, 0x00, "SCSI to host system interface failure" },
	{ 0x55, 0x00, "System resource failure" },
	{ 0x57, 0x00, "Unable to recover table-of-contents" },
	{ 0x58, 0x00, "Generation does not exist" },
	{ 0x59, 0x00, "Updated block read" },
	{ 0x5a, 0x00, "Operator request or state change input (unspecified)" },
	{ 0x5a, 0x01, "Operator medium removal request" },
	{ 0x5a, 0x02, "Operator selected write protect" },
	{ 0x5a, 0x03, "Operator selected write permit" },
	{ 0x5b, 0x00, "Log exception" },
	{ 0x5b, 0x01, "Threshold condition met" },
	{ 0x5b, 0x02, "Log counter at maximum" },
	{ 0x5b, 0x03, "Log list codes exhausted" },
	{ 0x5c, 0x00, "RPL status change" },
	{ 0x5c, 0x01, "Spindles synchronized" },
	{ 0x5c, 0x02, "Spindles not synchronized" },
	{ 0x60, 0x00, "Lamp failure" },
	{ 0x61, 0x00, "Video acquisition error" },
	{ 0x61, 0x01, "Unable to acquire video" },
	{ 0x61, 0x02, "Out of focus" },
	{ 0x62, 0x00, "Scan head positioning error" },
	{ 0x63, 0x00, "End of user area encountered on this track" },
	{ 0x64, 0x00, "Illegal mode for this track" },
	{ 0, 0, 0}
};

void scsi_print_add_sense_keys(key, qualif)
	register unsigned key, qualif;
{
	register struct addtl_sns_keys_msg	*msg;

	for (msg = addtl_sns_msgs; msg->means; msg++) {
		if (msg->byte12 != key) continue;
		if ((msg->byte12 == 0x40) && qualif) {
			printf(", %s, NN=x%x", msg->means, qualif);
			return;
		}
		if (msg->byte13 == qualif) {
			printf(" %s", msg->means);
			return;
		}
	};
	printf(", Unknown additional sense keys: 0x%x 0x%x\n", key, qualif);
}
#endif  /* NSCSI > 0 */
