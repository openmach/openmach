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
 *	File: scsi_comm.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Middle layer of the SCSI driver: SCSI protocol implementation
 *
 * This file contains code for SCSI commands for COMMUNICATION devices.
 */

#include <mach/std_types.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi_defs.h>

#if  (NSCSI > 0)

char *sccomm_name(
	boolean_t	internal)
{
	return internal ? "cz" : "comm";
}

void scsi_get_message(
	register target_info_t	*tgt,
	io_req_t		ior)
{
	scsi_cmd_read_t		*cmd;
	register unsigned	len, max;

	max = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max) {
		ior->io_residual = len - max;
		len = max;
	}

	cmd = (scsi_cmd_read_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_GET_MESSAGE;
	cmd->scsi_cmd_lun_and_lba1 = tgt->lun << SCSI_LUN_SHIFT;
	cmd->scsi_cmd_lba2 	   = len >> 16;
	cmd->scsi_cmd_lba3 	   = len >>  8;
	cmd->scsi_cmd_xfer_len     = len;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_GET_MESSAGE;

	scsi_go(tgt, sizeof(*cmd), len, FALSE);
}

void scsi_send_message(
	register target_info_t	*tgt,
	io_req_t		ior)
{
	scsi_cmd_write_t	*cmd;
	register unsigned	len, max;

	len = ior->io_count;
	max = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	if (len > max) {
		ior->io_residual = len - max;
		len = max;
	}

	cmd = (scsi_cmd_write_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_SEND_MESSAGE;
	cmd->scsi_cmd_lun_and_lba1 = tgt->lun << SCSI_LUN_SHIFT;
	cmd->scsi_cmd_lba2 	   = len >> 16;
	cmd->scsi_cmd_lba3 	   = len >>  8;
	cmd->scsi_cmd_xfer_len     = len;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_SEND_MESSAGE;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}


#if 0
/* For now, these are not needed */
scsi_get_message_long
scsi_get_message_vlong
scsi_send_message_long
scsi_send_message_vlong
#endif

#endif  /* NSCSI > 0 */
