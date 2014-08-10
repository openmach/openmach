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
 *	File: rz_tape.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Top layer of the SCSI driver: interface with the MI.
 *	This file contains operations specific to TAPE-like devices.
 */

#include <mach/std_types.h>
#include <scsi/compat_30.h>

#include <sys/ioctl.h>
#ifdef	MACH_KERNEL
#include <device/tape_status.h>
#else	/*MACH_KERNEL*/
#include <mips/PMAX/tape_status.h>
#endif	/*MACH_KERNEL*/

#include <scsi/scsi.h>
#include <scsi/scsi_defs.h>
#include <scsi/rz.h>

#if  (NSCSI > 0)


void sctape_start(); /* forward */

int scsi_tape_timeout = 5*60;	/* secs, tk50 is slow when positioning far apart */

int sctape_open(tgt, req)
	target_info_t	*tgt;
	io_req_t	req;
{
	io_return_t	ret;
	io_req_t	ior;
	int		i;
	scsi_mode_sense_data_t *mod;

#ifdef	MACH_KERNEL
	req->io_device->flag |= D_EXCL_OPEN;
#endif	/*MACH_KERNEL*/

	/* Preferably allow tapes to disconnect */
	if (BGET(scsi_might_disconnect,(unsigned char)tgt->masterno,tgt->target_id))
		BSET(scsi_should_disconnect,(unsigned char)tgt->masterno,tgt->target_id);

	/*
	 * Dummy ior for proper sync purposes
	 */
	io_req_alloc(ior,0);
	ior->io_count = 0;

	/*
	 * Do a mode sense first, some drives might be picky
	 * about changing params [even if the standard might
	 * say otherwise, sigh.]
	 */
	do {
		ior->io_op = IO_INTERNAL;
		ior->io_next = 0;
		ior->io_error = 0;
		ret = scsi_mode_sense(tgt, 0, 32, ior);
	} while (ret == SCSI_RET_RETRY);

	mod = (scsi_mode_sense_data_t *)tgt->cmd_ptr;
	if (scsi_debug) {
		int	p[5];
		bcopy((char*)mod, (char*)p, sizeof(p));
		printf("[modsns(%x): x%x x%x x%x x%x x%x]", ret,
			p[0], p[1], p[2], p[3], p[4]);
	}
	if (ret == SCSI_RET_DEVICE_DOWN)
		goto out;
	if (ret == SCSI_RET_SUCCESS) {
		tgt->dev_info.tape.read_only = mod->wp;
		tgt->dev_info.tape.speed = mod->speed;
		tgt->dev_info.tape.density = mod->bdesc[0].density_code;
	} /* else they all default sensibly, using zeroes */

	/* Some tapes have limits on record-length */
again:
	ior->io_op = IO_INTERNAL;
	ior->io_next = 0;
	ior->io_error = 0;
	ret = scsi_read_block_limits( tgt, ior);
	if (ret == SCSI_RET_RETRY) goto again;
	if (!ior->io_error && (ret == SCSI_RET_SUCCESS)) {
		scsi_blimits_data_t	*lim;
		int			maxl;

		lim = (scsi_blimits_data_t *) tgt->cmd_ptr;

		tgt->block_size = (lim->minlen_msb << 8) |
				   lim->minlen_lsb;

		maxl =	(lim->maxlen_msb << 16) |
			(lim->maxlen_sb  <<  8) |
			 lim->maxlen_lsb;
		if (maxl == 0)
			maxl = (unsigned)-1;
		tgt->dev_info.tape.maxreclen = maxl;
		tgt->dev_info.tape.fixed_size = (maxl == tgt->block_size);
	} else {
		/* let the user worry about it */
		/* default: tgt->block_size = 1; */
		tgt->dev_info.tape.maxreclen = (unsigned)-1;
		tgt->dev_info.tape.fixed_size = FALSE;
	}

	/* Try hard to do a mode select */
	for (i = 0; i < 5; i++) {
		ior->io_op = IO_INTERNAL;
		ior->io_error = 0;
		ret = sctape_mode_select(tgt, 0, 0, FALSE, ior);
		if (ret == SCSI_RET_SUCCESS)
			break;
	}
	if (scsi_watchdog_period < scsi_tape_timeout)
		scsi_watchdog_period += scsi_tape_timeout;

#if 0	/* this might imply rewind, which we do not want, although yes, .. */
	/* we want the tape loaded */
	ior->io_op = IO_INTERNAL;
	ior->io_next = 0;
	ior->io_error = 0;
	ret = scsi_start_unit(tgt, SCSI_CMD_SS_START, ior);
#endif
	req->io_device->bsize = tgt->block_size;
out:
	io_req_free(ior);
	return ret;
}


io_return_t sctape_close(tgt)
	target_info_t	*tgt;
{
	io_return_t	ret = SCSI_RET_SUCCESS;
	io_req_t	ior;

	/*
	 * Dummy ior for proper sync purposes
	 */
	io_req_alloc(ior,0);
	ior->io_op = IO_INTERNAL;
	ior->io_next = 0;
	ior->io_count = 0;

	if (tgt->ior) printf("TAPE: Close with pending requests ?? \n");

	/* write a filemark if we xtnded/truncated the tape */
	if (tgt->flags & TGT_WRITTEN_TO) {
		tgt->ior = ior;
		ior->io_error = 0;
		ret = scsi_write_filemarks(tgt, 2, ior);
		if (ret != SCSI_RET_SUCCESS)
			 printf("%s%d: wfmark failed x%x\n",
			 (*tgt->dev_ops->driver_name)(TRUE), tgt->target_id, ret);
		/*
		 * Don't bother repositioning if we'll rewind it
		 */
		if (tgt->flags & TGT_REWIND_ON_CLOSE)
			goto rew;
retry:
		tgt->ior = ior;
		ior->io_op = IO_INTERNAL;
		ior->io_error = 0;
		ior->io_next = 0;
		ret = scsi_space(tgt, SCSI_CMD_SP_FIL, -1, ior);
		if (ret != SCSI_RET_SUCCESS) {
			if (ret == SCSI_RET_RETRY) {
				timeout(wakeup, tgt, hz);
				await(tgt);
				goto retry;
			}
			printf("%s%d: bspfile failed x%x\n",
			 (*tgt->dev_ops->driver_name)(TRUE), tgt->target_id, ret);
		}
	}
rew:
	if (tgt->flags & TGT_REWIND_ON_CLOSE) {
		/* Rewind tape */
		ior->io_error = 0;
		ior->io_op = IO_INTERNAL;
		ior->io_error = 0;
		tgt->ior = ior;
		(void) scsi_rewind(tgt, ior, FALSE);
		iowait(ior);
		if (tgt->done == SCSI_RET_RETRY) {
			timeout(wakeup, tgt, 5*hz);
			await(tgt);
			goto rew;
		}
	}
		io_req_free(ior);

	tgt->flags &= ~(TGT_ONLINE|TGT_WRITTEN_TO|TGT_REWIND_ON_CLOSE);
	return ret;
}

int sctape_strategy(ior)
	register io_req_t	ior;
{
	target_info_t  *tgt;
	register int    i = ior->io_unit;

	tgt = scsi_softc[rzcontroller(i)]->target[rzslave(i)];

	if (((ior->io_op & IO_READ) == 0) &&
	    tgt->dev_info.tape.read_only) {
		ior->io_error = D_INVALID_OPERATION;
		ior->io_op |= IO_ERROR;
		ior->io_residual = ior->io_count;
		iodone(ior);
		return ior->io_error;
	}

	return rz_simpleq_strategy( ior, sctape_start);
}

static void
do_residue(ior, sns, bsize)
	io_req_t	  ior;
	scsi_sense_data_t *sns;
	int		  bsize;
{
	int residue;

	/* Not an error situation */
	ior->io_error = 0;
	ior->io_op &= ~IO_ERROR;

	if (!sns->addr_valid) {
		ior->io_residual = ior->io_count;
		return;
	}

	residue = sns->u.xtended.info0 << 24 |
		  sns->u.xtended.info1 << 16 |
		  sns->u.xtended.info2 <<  8 |
		  sns->u.xtended.info3;
	/* fixed ? */
	residue *= bsize;
	/*
	 * NOTE: residue == requested - actual
	 * We only care if > 0
	 */
	if (residue < 0) residue = 0;/* sanity */
	ior->io_residual += residue;
}

void sctape_start( tgt, done)
	target_info_t	*tgt;
	boolean_t	done;
{
	io_req_t		head, ior = tgt->ior;

	if (ior == 0)
		return;

	if (done) {

		/* see if we must retry */
		if ((tgt->done == SCSI_RET_RETRY) &&
		    ((ior->io_op & IO_INTERNAL) == 0)) {
			delay(1000000);/*XXX*/
			goto start;
		} else
		/* got a bus reset ? ouch, that hurts */
		if (tgt->done == (SCSI_RET_ABORTED|SCSI_RET_RETRY)) {
			/*
			 * we really cannot retry because the tape position
			 * is lost.
			 */
			printf("Lost tape position\n");
			ior->io_error = D_IO_ERROR;
			ior->io_op |= IO_ERROR;
		} else

		/* check completion status */

		if (tgt->cur_cmd == SCSI_CMD_REQUEST_SENSE) {
			scsi_sense_data_t *sns;

			ior->io_op = ior->io_temporary;
			ior->io_error = D_IO_ERROR;
			ior->io_op |= IO_ERROR;

			sns = (scsi_sense_data_t *)tgt->cmd_ptr;

			if (scsi_debug)
				scsi_print_sense_data(sns);

			if (scsi_check_sense_data(tgt, sns)) {
			    if (sns->u.xtended.ili) {
				if (ior->io_op & IO_READ) {
				    do_residue(ior, sns, tgt->block_size);
				    if (scsi_debug)
					printf("Tape Short Read (%d)\n",
						ior->io_residual);
				}
			    } else if (sns->u.xtended.eom) {
				do_residue(ior, sns, tgt->block_size);
				if (scsi_debug)
					printf("End of Physical Tape!\n");
			    } else if (sns->u.xtended.fm) {
				do_residue(ior, sns, tgt->block_size);
				if (scsi_debug)
					printf("File Mark\n");
			    }
			}
		}

		else if (tgt->done != SCSI_RET_SUCCESS) {

		    if (tgt->done == SCSI_RET_NEED_SENSE) {

			ior->io_temporary = ior->io_op;
			ior->io_op = IO_INTERNAL;
			if (scsi_debug)
				printf("[NeedSns x%x x%x]", ior->io_residual, ior->io_count);
			scsi_request_sense(tgt, ior, 0);
			return;

		    } else if (tgt->done == SCSI_RET_RETRY) {
			/* only retry here READs and WRITEs */
			if ((ior->io_op & IO_INTERNAL) == 0) {
				ior->io_residual = 0;
				goto start;
			} else{
				ior->io_error = D_WOULD_BLOCK;
				ior->io_op |= IO_ERROR;
			}
		    } else {
			ior->io_error = D_IO_ERROR;
			ior->io_op |= IO_ERROR;
		    }
		}

		if (scsi_debug)
			printf("[Resid x%x]", ior->io_residual);

		/* dequeue next one */
		head = ior;

		simple_lock(&tgt->target_lock);
		ior = head->io_next;
		tgt->ior = ior;
		if (ior)
			ior->io_prev = head->io_prev;
		simple_unlock(&tgt->target_lock);

		iodone(head);

		if (ior == 0)
			return;
	}
	ior->io_residual = 0;
start:
	if (ior->io_op & IO_READ) {
		tgt->flags &= ~TGT_WRITTEN_TO;
		sctape_read( tgt, ior );
	} else if ((ior->io_op & IO_INTERNAL) == 0) {
		tgt->flags |= TGT_WRITTEN_TO;
		sctape_write( tgt, ior );
	}
}

io_return_t
sctape_get_status( dev, tgt, flavor, status, status_count)
	int		dev;
	target_info_t	*tgt;
	dev_flavor_t	flavor;
	dev_status_t	status;
	natural_t	*status_count;
{
	switch (flavor) {
	case DEV_GET_SIZE:
		
		status[DEV_GET_SIZE_DEVICE_SIZE] = 0;
		status[DEV_GET_SIZE_RECORD_SIZE] = tgt->block_size;
		*status_count = DEV_GET_SIZE_COUNT;
		break;
	case TAPE_STATUS: {
		struct tape_status *ts = (struct tape_status *) status;

		ts->mt_type		= MT_ISSCSI;
		ts->speed		= tgt->dev_info.tape.speed;
		ts->density		= tgt->dev_info.tape.density;
		ts->flags		= (tgt->flags & TGT_REWIND_ON_CLOSE) ?
						TAPE_FLG_REWIND : 0;
		if (tgt->dev_info.tape.read_only)
			ts->flags |= TAPE_FLG_WP;
#ifdef	MACH_KERNEL
		*status_count = TAPE_STATUS_COUNT;
#endif

		break;
	    }
	/* U*x compat */
	case MTIOCGET: {
		struct mtget *g = (struct mtget *) status;

		bzero(g, sizeof(struct mtget));
		g->mt_type = 0x7;	/* Ultrix compat */
#ifdef	MACH_KERNEL
		*status_count = sizeof(struct mtget)/sizeof(int);
#endif
		break;
	    }
	default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

io_return_t
sctape_set_status( dev, tgt, flavor, status, status_count)
	int		dev;
	target_info_t	*tgt;
	dev_flavor_t	flavor;
	dev_status_t	status;
	natural_t	status_count;
{
	scsi_ret_t		ret;

	switch (flavor) {
	case TAPE_STATUS: {
		struct tape_status *ts = (struct tape_status *) status;
		if (ts->flags & TAPE_FLG_REWIND)
			tgt->flags |= TGT_REWIND_ON_CLOSE;
		else
			tgt->flags &= ~TGT_REWIND_ON_CLOSE;

		if (ts->speed || ts->density) {
			unsigned int ospeed, odensity;
			io_req_t	ior;

			io_req_alloc(ior,0);
			ior->io_op = IO_INTERNAL;
			ior->io_error = 0;
			ior->io_next = 0;
			ior->io_count = 0;

			ospeed = tgt->dev_info.tape.speed;
			odensity = tgt->dev_info.tape.density;
			tgt->dev_info.tape.speed = ts->speed;
			tgt->dev_info.tape.density = ts->density;

			ret = sctape_mode_select(tgt, 0, 0, (ospeed == ts->speed), ior);
			if (ret != SCSI_RET_SUCCESS) {
				tgt->dev_info.tape.speed = ospeed;
				tgt->dev_info.tape.density = odensity;
			}

			io_req_free(ior);
		}

		break;
	    }
	/* U*x compat */
	case MTIOCTOP: {
		struct tape_params *mt = (struct tape_params *) status;
		io_req_t	ior;

		if (scsi_debug)
			printf("[sctape_sstatus: %x %x %x]\n",
				flavor, mt->mt_operation, mt->mt_repeat_count);

		io_req_alloc(ior,0);
retry:
		ior->io_count = 0;
		ior->io_op = IO_INTERNAL;
		ior->io_error = 0;
		ior->io_next = 0;
		tgt->ior = ior;

		/* compat: in U*x it is a short */
		switch ((short)(mt->mt_operation)) {
		case MTWEOF:	/* write an end-of-file record */
			ret = scsi_write_filemarks(tgt, mt->mt_repeat_count, ior);
			break;
		case MTFSF:	/* forward space file */
			ret = scsi_space(tgt, SCSI_CMD_SP_FIL, mt->mt_repeat_count, ior);
			break;
		case MTBSF:	/* backward space file */
			ret = scsi_space(tgt, SCSI_CMD_SP_FIL, -mt->mt_repeat_count,ior);
			break;
		case MTFSR:	/* forward space record */
			ret = scsi_space(tgt, SCSI_CMD_SP_BLOCKS, mt->mt_repeat_count, ior);
			break;
		case MTBSR:	/* backward space record */
			ret = scsi_space(tgt, SCSI_CMD_SP_BLOCKS, -mt->mt_repeat_count, ior);
			break;
		case MTREW:	/* rewind */
		case MTOFFL:	/* rewind and put the drive offline */
			ret = scsi_rewind(tgt, ior, TRUE);
			iowait(ior);
			if ((short)(mt->mt_operation) == MTREW) break;
			ior->io_op = 0;
			ior->io_next = 0;
			ior->io_error = 0;
			(void) scsi_start_unit(tgt, 0, ior);
			break;
		case MTNOP:	/* no operation, sets status only */
		case MTCACHE:	/* enable controller cache */
		case MTNOCACHE:	/* disable controller cache */
			ret = SCSI_RET_SUCCESS;
			break;
		default:
			tgt->ior = 0;
			io_req_free(ior);
			return D_INVALID_OPERATION;
		}

		if (ret == SCSI_RET_RETRY) {
			timeout(wakeup, ior, 5*hz);
			await(ior);
			goto retry;
		}

		io_req_free(ior);
		if (ret != SCSI_RET_SUCCESS)
			return D_IO_ERROR;
		break;
	}
	case MTIOCIEOT:
	case MTIOCEEOT:
	default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}
#endif  /* NSCSI > 0 */
