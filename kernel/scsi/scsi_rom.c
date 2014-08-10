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
 *	File: scsi_rom.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Middle layer of the SCSI driver: SCSI protocol implementation
 *
 * This file contains code for SCSI commands for CD-ROM devices.
 */

#include <mach/std_types.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#if  (NSCSI > 0)

char *sccdrom_name(
	boolean_t	 internal)
{
	return internal ? "rz" : "CD-ROM";
}

int scsi_pause_resume(
	target_info_t	*tgt,
	boolean_t	stop_it,
	io_req_t	ior)
{
	scsi_cmd_pausres_t	*cmd;

	cmd = (scsi_cmd_pausres_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PAUSE_RESUME;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 	   = 0;
	cmd->scsi_cmd_lba2 	   = 0;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_lba4 	   = 0;
	cmd->scsi_cmd_xxx 	   = 0;
	cmd->scsi_cmd_xfer_len_1    = 0;
	cmd->scsi_cmd_pausres_res  = stop_it ? 0 : SCSI_CMD_PAUSRES_RESUME;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PAUSE_RESUME;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio(
	target_info_t	*tgt,
	unsigned int	start,
	unsigned int	len,
	boolean_t	relative_address,
	io_req_t	ior)
{
	scsi_cmd_play_audio_t	*cmd;

	cmd = (scsi_cmd_play_audio_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO;
	cmd->scsi_cmd_lun_and_relbit = relative_address ? SCSI_RELADR : 0;
	cmd->scsi_cmd_lba1 	   = start >> 24;
	cmd->scsi_cmd_lba2 	   = start >> 16;
	cmd->scsi_cmd_lba3 	   = start >>  8;
	cmd->scsi_cmd_lba4 	   = start >>  0;
	cmd->scsi_cmd_xxx 	   = 0;
	cmd->scsi_cmd_xfer_len_1    = len >> 8;
	cmd->scsi_cmd_xfer_len_2    = len >> 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio_long(
        target_info_t      *tgt,
        unsigned int    start,
        unsigned int    len,
        boolean_t       relative_address,
        io_req_t        ior)
{
        scsi_cmd_play_audio_l_t   *cmd;

        cmd = (scsi_cmd_play_audio_l_t*) (tgt->cmd_ptr);
        cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO_LONG;
        cmd->scsi_cmd_lun_and_relbit = relative_address ? SCSI_RELADR : 0;
        cmd->scsi_cmd_lba1         = start >> 24;
        cmd->scsi_cmd_lba2         = start >> 16;
        cmd->scsi_cmd_lba3         = start >>  8;
        cmd->scsi_cmd_lba4         = start >>  0;
        cmd->scsi_cmd_xfer_len_1    = len >> 24;
        cmd->scsi_cmd_xfer_len_2    = len >> 16;
        cmd->scsi_cmd_xfer_len_3    = len >>  8;
        cmd->scsi_cmd_xfer_len_4    = len >>  0;
        cmd->scsi_cmd_xxx1	   = 0;
        cmd->scsi_cmd_ctrl_byte = 0;    /* not linked */

        tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO_LONG;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio_msf(
	target_info_t	*tgt,
	int		sm,
	int		ss,
	int		sf,
	int		em,
	int		es,
	int		ef,
	io_req_t	ior)
{
	scsi_cmd_play_audio_msf_t	*cmd;

	cmd = (scsi_cmd_play_audio_msf_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO_MSF;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 = 0;
	cmd->scsi_cmd_pamsf_startM = sm;
	cmd->scsi_cmd_pamsf_startS = ss;
	cmd->scsi_cmd_pamsf_startF = sf;
	cmd->scsi_cmd_pamsf_endM = em;
	cmd->scsi_cmd_pamsf_endS = es;
	cmd->scsi_cmd_pamsf_endF = ef;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO_MSF;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio_track_index(
	target_info_t	*tgt,
	int		st,
	int		si,
	int		et,
	int		ei,
	io_req_t	ior)
{
	scsi_cmd_play_audio_ti_t	*cmd;

	cmd = (scsi_cmd_play_audio_ti_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO_TI;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 = 0;
	cmd->scsi_cmd_lba2 = 0;
	cmd->scsi_cmd_pati_startT = st;
	cmd->scsi_cmd_pati_startI = si;
	cmd->scsi_cmd_xxx = 0;
	cmd->scsi_cmd_pati_endT = et;
	cmd->scsi_cmd_pati_endI = ei;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO_TI;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio_track_relative(
	target_info_t	*tgt,
	unsigned int	lba,
	int		st,
	unsigned int	len,
	io_req_t	ior)
{
	scsi_cmd_play_audio_tr_t	*cmd;

	cmd = (scsi_cmd_play_audio_tr_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO_TR;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 	   = lba >> 24;
	cmd->scsi_cmd_lba2 	   = lba >> 16;
	cmd->scsi_cmd_lba3 	   = lba >>  8;
	cmd->scsi_cmd_lba4 	   = lba >>  0;
	cmd->scsi_cmd_patr_startT  = st;
	cmd->scsi_cmd_xfer_len_1    = len >> 8;
	cmd->scsi_cmd_xfer_len_2    = len >> 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO_TR;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_play_audio_track_relative_long(
	target_info_t	*tgt,
	unsigned int	lba,
	int		st,
	unsigned int	len,
	io_req_t	ior)
{
	scsi_cmd_play_audio_tr_l_t	*cmd;

	cmd = (scsi_cmd_play_audio_tr_l_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PLAY_AUDIO_TR_LONG;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1 	   = lba >> 24;
	cmd->scsi_cmd_lba2 	   = lba >> 16;
	cmd->scsi_cmd_lba3 	   = lba >>  8;
	cmd->scsi_cmd_lba4 	   = lba >>  0;
	cmd->scsi_cmd_xfer_len_1    = len >> 24;
	cmd->scsi_cmd_xfer_len_2    = len >> 16;
	cmd->scsi_cmd_xfer_len_3    = len >>  8;
	cmd->scsi_cmd_xfer_len_4    = len >>  0;
	cmd->scsi_cmd_patrl_startT  = st;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_PLAY_AUDIO_TR_LONG;

        scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

        return tgt->done;
}

scsi_read_header(
	target_info_t	*tgt,
	boolean_t	msf_format,
	unsigned int	lba,
	unsigned int	allocsize,
	io_req_t	ior)
{
	scsi_cmd_read_header_t	*cmd;

	cmd = (scsi_cmd_read_header_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ_HEADER;
	cmd->scsi_cmd_lun_and_relbit = msf_format ? SCSI_CMD_CD_MSF : 0;
	cmd->scsi_cmd_lba1 	   = lba >> 24;
	cmd->scsi_cmd_lba2 	   = lba >> 16;
	cmd->scsi_cmd_lba3 	   = lba >>  8;
	cmd->scsi_cmd_lba4 	   = lba >>  0;
	cmd->scsi_cmd_xxx 	   = 0;
	cmd->scsi_cmd_xfer_len_1    = allocsize >> 8;
	cmd->scsi_cmd_xfer_len_2    = allocsize >> 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_READ_HEADER;

        scsi_go_and_wait(tgt, sizeof(*cmd), allocsize, ior);

        return tgt->done;
}

scsi_read_subchannel(
	target_info_t	*tgt,
	boolean_t	msf_format,
	unsigned int	data_format,
	unsigned int	trackno,
	io_req_t	ior)
{
	scsi_cmd_read_subch_t	*cmd;
	int			allocsize;

	switch (data_format) {
	case SCSI_CMD_RS_FMT_SUBQ:
		allocsize = sizeof(cdrom_chan_data_t);
		trackno = 0; break;
	case SCSI_CMD_RS_FMT_CURPOS:
		allocsize = sizeof(cdrom_chan_curpos_t);
		trackno = 0; break;
	case SCSI_CMD_RS_FMT_CATALOG:
		allocsize = sizeof(cdrom_chan_catalog_t);
		trackno = 0; break;
	case SCSI_CMD_RS_FMT_ISRC:
		allocsize = sizeof(cdrom_chan_isrc_t); break;
	}

	cmd = (scsi_cmd_read_subch_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ_SUBCH;
	cmd->scsi_cmd_lun_and_relbit = msf_format ? SCSI_CMD_CD_MSF : 0;
	cmd->scsi_cmd_lba1 	   = SCSI_CMD_RS_SUBQ;
	cmd->scsi_cmd_rs_format	   = data_format;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_lba4 	   = 0;
	cmd->scsi_cmd_rs_trackno   = trackno;
	cmd->scsi_cmd_xfer_len_1    = allocsize >> 8;
	cmd->scsi_cmd_xfer_len_2    = allocsize >> 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_READ_SUBCH;

        scsi_go_and_wait(tgt, sizeof(*cmd), allocsize, ior);

        return tgt->done;
}

scsi_read_toc(
	target_info_t	*tgt,
	boolean_t	msf_format,
	int		trackno,
	int		allocsize,
	io_req_t	ior)
{
	scsi_cmd_read_toc_t	*cmd;

	cmd = (scsi_cmd_read_toc_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ_TOC;
	cmd->scsi_cmd_lun_and_relbit = msf_format ? SCSI_CMD_CD_MSF : 0;
	cmd->scsi_cmd_lba1 	   = 0;
	cmd->scsi_cmd_lba2 	   = 0;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_lba4 	   = 0;
	cmd->scsi_cmd_rtoc_startT  = trackno;
	cmd->scsi_cmd_xfer_len_1    = allocsize >> 8;
	cmd->scsi_cmd_xfer_len_2    = allocsize >> 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_READ_TOC;

        scsi_go_and_wait(tgt, sizeof(*cmd), allocsize, ior);

        return tgt->done;
}

/* move elsewhere ifworks */
scsi2_mode_select(
	target_info_t	*tgt,
	boolean_t	save,
	unsigned char	*page,
	int		pagesize,
	io_req_t	ior)
{
	scsi_cmd_mode_select_t	*cmd;
	scsi2_mode_param_t	*parm;

	bzero(tgt->cmd_ptr, sizeof(*cmd) + sizeof(*parm));
	cmd = (scsi_cmd_mode_select_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_MODE_SELECT;
	cmd->scsi_cmd_lun_and_lba1 = SCSI_CMD_MSL_PF | (save ? SCSI_CMD_MSL_SP : 0);
	cmd->scsi_cmd_xfer_len = pagesize;

	parm = (scsi2_mode_param_t*) (cmd + 1);

	bcopy(page, parm, pagesize);

	tgt->cur_cmd = SCSI_CMD_MODE_SELECT;

	scsi_go_and_wait(tgt, sizeof(*cmd) + pagesize, 0, ior);

	return tgt->done;
}

/*
 * obnoxious
 */
cdrom_vendor_specific(
	target_info_t	*tgt,
	scsi_command_group_2	*cmd,
	unsigned char	*params,
	int		paramlen,
	int		retlen,
	io_req_t	ior)
{
	bcopy(cmd, tgt->cmd_ptr, sizeof(*cmd));
	if (paramlen)
		bcopy(params, tgt->cmd_ptr + sizeof(*cmd), paramlen);

	tgt->cur_cmd = cmd->scsi_cmd_code;

	scsi_go_and_wait(tgt, sizeof(*cmd) + paramlen, retlen, ior);

	return tgt->done;
}
#endif  /* NSCSI > 0 */
