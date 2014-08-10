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
 *	File: scsi_disk.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Middle layer of the SCSI driver: SCSI protocol implementation
 *
 * This file contains code for SCSI commands for DISK devices.
 */

#include <string.h>

#include <mach/std_types.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>

#if  (NSCSI > 0)


char *scdisk_name(internal)
	boolean_t	internal;
{
	return internal ? "rz" : "disk";
}

/*
 * SCSI commands partially specific to disks
 */
void scdisk_read( tgt, secno, ior)
	register target_info_t	*tgt;
	register unsigned int	secno;
	io_req_t		ior;
{
	scsi_cmd_read_t		*cmd;
	register unsigned	len;
	unsigned int		max_dma_data;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	if (len < tgt->block_size)
		len = tgt->block_size;

	cmd = (scsi_cmd_read_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ;
	cmd->scsi_cmd_lun_and_lba1 = (secno>>16)&SCSI_LBA_MASK;
	cmd->scsi_cmd_lba2 	   = (secno>> 8)&0xff;
	cmd->scsi_cmd_lba3 	   = (secno    )&0xff;
	cmd->scsi_cmd_xfer_len     = len / tgt->block_size;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_READ;

	scsi_go(tgt, sizeof(*cmd), len, FALSE);
}

void scdisk_write( tgt, secno, ior)
	register target_info_t	*tgt;
	register unsigned int	secno;
	io_req_t		ior;
{
	scsi_cmd_write_t	*cmd;
	unsigned		len;	/* in bytes */
	unsigned int		max_dma_data;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	if (len < tgt->block_size)
		len = tgt->block_size;

	cmd = (scsi_cmd_write_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_WRITE;
	cmd->scsi_cmd_lun_and_lba1 = (secno>>16)&SCSI_LBA_MASK;
	cmd->scsi_cmd_lba2 	   = (secno>> 8)&0xff;
	cmd->scsi_cmd_lba3 	   = (secno    )&0xff;
	cmd->scsi_cmd_xfer_len     = len / tgt->block_size;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_WRITE;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}


int scdisk_mode_select(tgt, lbn, ior, mdata, mlen, save)
	register target_info_t	*tgt;
	register int 		lbn;
	io_req_t		ior;
	char			*mdata;
	int mlen, save;
{
	scsi_cmd_mode_select_t	*cmd;
	scsi_mode_select_param_t	*parm;

	bzero(tgt->cmd_ptr, sizeof(*cmd) + sizeof(*parm));
	cmd = (scsi_cmd_mode_select_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_MODE_SELECT;
	cmd->scsi_cmd_lun_and_lba1 = SCSI_CMD_MSL_PF;	/* XXX only if... */
	cmd->scsi_cmd_xfer_len = sizeof(scsi_mode_select_param_t);/* no vuq */

	parm = (scsi_mode_select_param_t*) (cmd + 1);
	if (mdata) {
		cmd->scsi_cmd_xfer_len = mlen;
		bcopy(mdata, (char*)parm, mlen);
		if (save)
			cmd->scsi_cmd_lun_and_lba1 |= SCSI_CMD_MSL_SP;
	} else {
	 	/* parm->medium_type = if (floppy)disk.. */
		parm->desc_len = 8;
		/* this really is the LBN */
		parm->descs[0].density_code = 0;/* XXX default XXX */
		parm->descs[0].reclen1 = (lbn>>16)&0xff;
		parm->descs[0].reclen2 = (lbn>> 8)&0xff;
		parm->descs[0].reclen3 = (lbn    )&0xff;
		mlen = sizeof(*parm);
	}

	tgt->cur_cmd = SCSI_CMD_MODE_SELECT;

	scsi_go_and_wait(tgt, sizeof(*cmd) + mlen, 0, ior);

	return tgt->done;
}

/*
 * SCSI commands fully specific to disks
 */
int scsi_read_capacity( tgt, lbn, ior)
	register target_info_t	*tgt;
	int			lbn;
	io_req_t		ior;
{
	scsi_cmd_read_capacity_t	*cmd;

	bzero(tgt->cmd_ptr, sizeof(*cmd));
	cmd = (scsi_cmd_read_capacity_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ_CAPACITY;
	/* all zeroes, unless... */
	if (lbn) {
		cmd->scsi_cmd_rcap_flags = SCSI_CMD_RCAP_PMI;
		cmd->scsi_cmd_lba1 = (lbn>>24);
		cmd->scsi_cmd_lba2 = (lbn>>16)&0xff;
		cmd->scsi_cmd_lba3 = (lbn>> 8)&0xff;
		cmd->scsi_cmd_lba4 = (lbn    )&0xff;
	}
	
	tgt->cur_cmd = SCSI_CMD_READ_CAPACITY;

	scsi_go_and_wait(tgt, sizeof(*cmd), sizeof(scsi_rcap_data_t),ior);

	return tgt->done;
}

void scsi_reassign_blocks( tgt, defect_list, n_defects, ior)
	register target_info_t	*tgt;
	unsigned int		*defect_list;	/* In ascending order ! */
	int			n_defects;
	io_req_t		ior;
{
	scsi_cmd_reassign_blocks_t	*cmd;
	scsi_Ldefect_data_t		*parm;

	cmd = (scsi_cmd_reassign_blocks_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_REASSIGN_BLOCKS;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 	   = 0;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_xfer_len     = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */

	parm = (scsi_Ldefect_data_t *) (cmd + 1);
	parm->res1 = parm->res2 = 0;
	n_defects *= 4;	/* in 4-byte-ints */
	parm->list_len_msb = n_defects >> 8;
	parm->list_len_lsb = n_defects;
	bcopy((char*)defect_list, (char*)parm->defects, n_defects);

	tgt->cur_cmd = SCSI_CMD_REASSIGN_BLOCKS;

	scsi_go(tgt, sizeof(*cmd) + sizeof(*parm) + (n_defects - 4), 0, FALSE);
}

void scsi_medium_removal( tgt, allow, ior)
	register target_info_t	*tgt;
	boolean_t		allow;
	io_req_t		ior;
{
	scsi_cmd_medium_removal_t	*cmd;

	cmd = (scsi_cmd_medium_removal_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_PREVENT_ALLOW_REMOVAL;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 	   = 0;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_pa_prevent   = allow ? 0 : 1;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */


	tgt->cur_cmd = SCSI_CMD_PREVENT_ALLOW_REMOVAL;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);
}

int scsi_format_unit( tgt, mode, vuqe, intlv, ior)
	register target_info_t	*tgt;
	int mode, vuqe;
	register unsigned int	intlv;
	io_req_t		ior;
{
	scsi_cmd_format_t	*cmd;
	char			*parms;

	cmd = (scsi_cmd_format_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_FORMAT_UNIT;
	cmd->scsi_cmd_lun_and_lba1 =
		mode & (SCSI_CMD_FMT_FMTDATA|SCSI_CMD_FMT_CMPLIST|SCSI_CMD_FMT_LIST_TYPE);
	cmd->scsi_cmd_lba2 	   = vuqe;
	cmd->scsi_cmd_lba3 	   = intlv >>  8;
	cmd->scsi_cmd_xfer_len     = intlv;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */

	parms = (char*) cmd + 1;
	if (ior->io_count)
		bcopy(ior->io_data, parms, ior->io_count);
	else
		bzero(parms, 0xff - sizeof(*cmd));

	tgt->cur_cmd = SCSI_CMD_FORMAT_UNIT;

	scsi_go_and_wait(tgt, sizeof(*cmd) + ior->io_count, 0, ior);
	return tgt->done;
}


/* Group 1 Commands */

void scsi_long_read( tgt, secno, ior)
	register target_info_t	*tgt;
	register unsigned int	secno; 
	io_req_t		ior;
{
	scsi_cmd_long_read_t	*cmd;
	register unsigned	len, n_blks;
	unsigned int		max_dma_data;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	if (len < tgt->block_size)
		len = tgt->block_size;
	n_blks = len /tgt->block_size;

	cmd = (scsi_cmd_long_read_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_LONG_READ;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = n_blks >> 8;
	cmd->scsi_cmd_xfer_len_2     = n_blks;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_LONG_READ;

	scsi_go(tgt, sizeof(*cmd), len, FALSE);
}

void scsi_long_write( tgt, secno, ior)
	register target_info_t	*tgt;
	register unsigned int	secno;
	io_req_t		ior;
{
	scsi_cmd_long_write_t	*cmd;
	unsigned		len;	/* in bytes */
	unsigned int		max_dma_data, n_blks;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	if (len < tgt->block_size)
		len = tgt->block_size;
	n_blks = len /tgt->block_size;

	cmd = (scsi_cmd_long_write_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_LONG_WRITE;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = n_blks >> 8;
	cmd->scsi_cmd_xfer_len_2     = n_blks;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_LONG_WRITE;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}

int scdisk_verify( tgt, secno, nsectrs, ior)
	register target_info_t	*tgt;
	int secno, nsectrs;
	io_req_t		ior;
{
	scsi_cmd_verify_long_t	*cmd;
	int			len;

	len = ior->io_count;

	cmd = (scsi_cmd_verify_long_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_VERIFY_1;
	cmd->scsi_cmd_lun_and_relbit = len ? SCSI_CMD_VFY_BYTCHK : 0;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = (nsectrs) >> 8;
	cmd->scsi_cmd_xfer_len_2     = nsectrs;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_VERIFY_1;

	scsi_go_and_wait(tgt, sizeof(*cmd) + len, 0, ior);
	return tgt->done;
}


int scsi_read_defect( tgt, mode, ior)
	register target_info_t	*tgt;
	register unsigned int	mode; 
	io_req_t		ior;
{
	scsi_cmd_long_read_t	*cmd;
	register unsigned	len;

	len = ior->io_count;
	if (len > 0xffff)
		len = 0xffff;

	cmd = (scsi_cmd_read_defect_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_READ_DEFECT_DATA;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1	     = mode & 0x1f;
	cmd->scsi_cmd_lba2	     = 0;
	cmd->scsi_cmd_lba3	     = 0;
	cmd->scsi_cmd_lba4	     = 0;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = (len) >> 8;
	cmd->scsi_cmd_xfer_len_2     = (len);
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	/* ++ HACK Alert */
/*	tgt->cur_cmd = SCSI_CMD_READ_DEFECT_DATA;*/
	tgt->cur_cmd = SCSI_CMD_LONG_READ;
	/* -- HACK Alert */

	scsi_go(tgt, sizeof(*cmd), len, FALSE);
	iowait(ior);
	return tgt->done;
}


#if	0 /* unused commands */
scsi_rezero_unit( tgt, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_rezero_t	*cmd;

	cmd = (scsi_cmd_rezero_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_REZERO_UNIT;
	cmd->scsi_cmd_lun_and_lba1 = 0;
	cmd->scsi_cmd_lba2 	   = 0;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_xfer_len     = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */


	tgt->cur_cmd = SCSI_CMD_REZERO_UNIT;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

}

scsi_seek( tgt, where, ior)
	register target_info_t	*tgt;
	register unsigned int	where;
	io_req_t		ior;
{
	scsi_cmd_seek_t	*cmd;

	cmd = (scsi_cmd_seek_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_SEEK;
	cmd->scsi_cmd_lun_and_lba1 = (where >> 16) & 0x1f;
	cmd->scsi_cmd_lba2 	   = where >>  8;
	cmd->scsi_cmd_lba3 	   = where;
	cmd->scsi_cmd_xfer_len     = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */


	tgt->cur_cmd = SCSI_CMD_SEEK;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

}

scsi_reserve( tgt, len, id, mode, ior)
	register target_info_t	*tgt;
	register unsigned int	len;
	unsigned char		id;
	io_req_t		ior;
{
	scsi_cmd_reserve_t	*cmd;

	cmd = (scsi_cmd_reserve_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_RESERVE;
	cmd->scsi_cmd_lun_and_lba1 = mode & 0x1f;
	cmd->scsi_cmd_reserve_id   = id;
	cmd->scsi_cmd_extent_llen1 = len >>  8;
	cmd->scsi_cmd_extent_llen2 = len;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */


	tgt->cur_cmd = SCSI_CMD_RESERVE;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

}

scsi_release( tgt, id, mode, ior)
	register target_info_t	*tgt;
	unsigned char		id, mode;
	io_req_t		ior;
{
	scsi_cmd_release_t	*cmd;

	cmd = (scsi_cmd_release_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_RELEASE;
	cmd->scsi_cmd_lun_and_lba1 = mode & 0x1f;
	cmd->scsi_cmd_reserve_id   = id;
	cmd->scsi_cmd_lba3 	   = 0;
	cmd->scsi_cmd_xfer_len     = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */


	tgt->cur_cmd = SCSI_CMD_RELEASE;

	scsi_go_and_wait(tgt, sizeof(*cmd), 0, ior);

}


/* Group 1 Commands */

scsi_long_seek( tgt, secno, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_long_seek_t	*cmd;

	cmd = (scsi_cmd_long_seek_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_LONG_SEEK;
	cmd->scsi_cmd_lun_and_relbit = 0;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = 0;
	cmd->scsi_cmd_xfer_len_2     = 0;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_LONG_SEEK;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}

scsi_write_verify( tgt, secno, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_write_vfy_t	*cmd;
	unsigned		len;	/* in bytes */
	unsigned int		max_dma_data, n_blks;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	if (len < tgt->block_size)
		len = tgt->block_size;
	n_blks = len / tgt->block_size;

	cmd = (scsi_cmd_write_vfy_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_WRITE_AND_VERIFY;
	cmd->scsi_cmd_lun_and_relbit = SCSI_CMD_VFY_BYTCHK;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = n_blks >> 8;
	cmd->scsi_cmd_xfer_len_2     = n_blks;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_WRITE_AND_VERIFY;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}

scsi_search_data( tgt, secno, how, flags, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_search_t	*cmd;
	unsigned		len;	/* in bytes */
	unsigned int		max_dma_data, n_blks;

	max_dma_data = scsi_softc[(unsigned char)tgt->masterno]->max_dma_data;

	if (how != SCSI_CMD_SEARCH_HIGH &&
	    how != SCSI_CMD_SEARCH_EQUAL &&
	    how != SCSI_CMD_SEARCH_LOW)
		panic("scsi_search_data");

	len = ior->io_count;
	if (len > max_dma_data)
		len = max_dma_data;
	n_blks = len / tgt->block_size;

	cmd = (scsi_cmd_search_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = how;
	cmd->scsi_cmd_lun_and_relbit = flags & 0x1e;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = n_blks >> 8;
	cmd->scsi_cmd_xfer_len_2     = n_blks;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = how;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}


scsi_set_limits( tgt, secno, nblocks, inhibit, ior)
	register target_info_t	*tgt;
	io_req_t		ior;
{
	scsi_cmd_set_limits_t	*cmd;

	cmd = (scsi_cmd_set_limits_t*) (tgt->cmd_ptr);
	cmd->scsi_cmd_code = SCSI_CMD_SET_LIMITS;
	cmd->scsi_cmd_lun_and_relbit = inhibit & 0x3;
	cmd->scsi_cmd_lba1	     = secno >> 24;
	cmd->scsi_cmd_lba2	     = secno >> 16;
	cmd->scsi_cmd_lba3	     = secno >>  8;
	cmd->scsi_cmd_lba4	     = secno;
	cmd->scsi_cmd_xxx	     = 0;
	cmd->scsi_cmd_xfer_len_1     = nblocks >> 8;
	cmd->scsi_cmd_xfer_len_2     = nblocks;
	cmd->scsi_cmd_ctrl_byte = 0;	/* not linked */
	
	tgt->cur_cmd = SCSI_CMD_SET_LIMITS;

	scsi_go(tgt, sizeof(*cmd), 0, FALSE);
}


#endif

#ifdef	SCSI2
scsi_lock_cache
scsi_prefetch
scsi_read_defect_data
scsi_sync_cache
scsi_write_same
#endif	SCSI2
#endif  /* NSCSI > 0 */
