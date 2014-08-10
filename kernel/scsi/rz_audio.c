/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
 *	File: rz_audio.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	3/93
 *
 *	Top layer of the SCSI driver: interface with the MI.
 *	This file contains operations specific to audio CD-ROM devices.
 *	Unlike many others, it sits on top of the rz.c module.
 */

#include <mach/std_types.h>
#include <kern/strings.h>
#include <machine/machspl.h>		/* spl definitions */
#include <vm/vm_kern.h>
#include <device/ds_routines.h>

#include <scsi/compat_30.h>
#include <scsi/scsi.h>
#include <scsi/scsi2.h>
#include <scsi/scsi_defs.h>
#include <scsi/rz.h>

#if (NSCSI > 0)

#define private static

/* some data is two BCD digits in one byte */
#define	bcd_to_decimal(b)	(((b)&0xf) + 10 * (((b) >> 4) & 0xf))
#define decimal_to_bcd(b)	((((b) / 10) << 4)  |  ((b) % 10))

/*
 * Regular use of a CD-ROM is for data, and is handled
 * by the default set of operations. Ours is for funtime..
 */

extern char	*sccdrom_name();
int		cd_strategy();
void		cd_start();

private scsi_devsw_t	scsi_audio = {
	sccdrom_name, 0, 0, 0, cd_strategy, cd_start, 0, 0
};

private char unsupported[] = "Device does not support it.";

/*
 * Unfortunately, none of the vendors appear to
 * abide by the SCSI-2 standard and many of them
 * violate or stretch even the SCSI-1 one.
 * Therefore, we keep a red-list here of the worse
 * offendors and how to deal with them.
 * The user is notified of the problem and invited
 * to solicit his vendor to upgrade the firmware.
 * [They had plenty of time to do so]
 */
typedef struct red_list {
	char	*vendor;
	char	*product;
	char	*rev;
	/*
	 * The standard MANDATES [par 13.1.6] the play_audio command
	 * at least as a way to discover if the device
	 * supports audio operations at all. This is the only way
	 * we need to use it.
	 */
	scsi_ret_t	(*can_play_audio)( target_info_t *, char *, io_req_t);
	/*
	 * The standard defines the use of start_stop_unit to
	 * cause the drive to eject the disk.
	 */
	scsi_ret_t	(*eject)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines read_subchannel as a way to
	 * get the current playing position.
	 */
	scsi_ret_t	(*current_position)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines read_table_of_content to get
	 * the listing of audio tracks available.
	 */
	scsi_ret_t	(*read_toc)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines read_subchannel as the way to
	 * report the current audio status (playing/stopped/...).
	 */
	scsi_ret_t	(*get_status)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines two ways to issue a play command,
	 * depending on the type of addressing used.
	 */
	scsi_ret_t	(*play_msf)( target_info_t *, char *, io_req_t );
	scsi_ret_t	(*play_ti)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines the pause_resume command to
	 * suspend or resume playback of audio data.
	 */
	scsi_ret_t	(*pause_resume)( target_info_t *, char *, io_req_t );
	/*
	 * The standard defines the audio page among the
	 * mode selection options as a way to control
	 * both volume and connectivity of the channels
	 */
	scsi_ret_t	(*volume_control)( target_info_t *, char *, io_req_t );
} red_list_t;

#define	if_it_can_do(some_cmd)						\
	if (tgt->dev_info.cdrom.violates_standards &&			\
	    tgt->dev_info.cdrom.violates_standards->some_cmd)		\
	    rc = (*tgt->dev_info.cdrom.violates_standards->some_cmd)	\
			(tgt,cmd,ior);	\
	else

/*
 * So now that you know what they should have implemented :-),
 * check at the end of the file what the naughty boys did instead.
 */
/* private red_list_t	audio_replacements[];	/ * at end */

/*
 * Forward decls
 */
private void decode_status( char *buf, unsigned char audio_status );
void zero_ior( io_req_t );

/*
 * Open routine.  Does some checking, sets up
 * the replacement pointer.
 */
io_return_t
cd_open(
	int		dev,
	dev_mode_t	mode,
	io_req_t	req)
{
	scsi_softc_t	*sc = 0;
	target_info_t	*tgt;
	int		ret;
	scsi_ret_t	rc;
	io_req_t	ior = 0;
	vm_offset_t	mem = 0;
	extern boolean_t rz_check();

	if (!rz_check(dev, &sc, &tgt)) {
		/*
		 * Probe it again: might have installed a new device
		 */
		if (!sc || !scsi_probe(sc, &tgt, rzslave(dev), ior))
			return D_NO_SUCH_DEVICE;
		bzero(&tgt->dev_info, sizeof(tgt->dev_info));
	}

	/*
	 * Check this is indeded a CD-ROM
	 */
	if (tgt->dev_ops != &scsi_devsw[SCSI_CDROM]) {
		rz_close(dev);
		return D_NO_SUCH_DEVICE;
	}

	/*
	 * Switch to audio ops, unless some wrong
	 */
	tgt->dev_ops = &scsi_audio;

	/*
	 * Bring unit online
	 */
	ret = rz_open(dev, mode, req);
	if (ret) goto bad;

	/* Pessimistic */
	ret = D_INVALID_OPERATION;

	/*
	 * Check if this device is on the red list
	 */
	{
	    scsi2_inquiry_data_t	*inq;
	    private void 		check_red_list();

	    scsi_inquiry(tgt, SCSI_INQ_STD_DATA);
	    inq = (scsi2_inquiry_data_t*)tgt->cmd_ptr;

	    check_red_list( tgt, inq );

	}

	/*
	 * Allocate dynamic data
	 */
	if (kmem_alloc(kernel_map, &mem, PAGE_SIZE) != KERN_SUCCESS)
	  return D_NO_MEMORY;
	tgt->dev_info.cdrom.result = (void *)mem;
	tgt->dev_info.cdrom.result_available = FALSE;

	/*
	 * See if this CDROM can play audio data
	 */
	io_req_alloc(ior,0);		
	zero_ior( ior );

	{
		char *cmd = 0;
		if_it_can_do(can_play_audio)
			rc = scsi_play_audio( tgt, 0, 0, FALSE, ior);
	}

	if (rc != SCSI_RET_SUCCESS) goto bad;

	io_req_free(ior);
	return D_SUCCESS;

bad:
	if (ior) io_req_free(ior);
	if (mem) kmem_free(kernel_map, mem, PAGE_SIZE);
	tgt->dev_ops = &scsi_devsw[SCSI_CDROM];
	return ret;
}

/*
 * Close routine.
 */
io_return_t
cd_close(
	int	dev)
{
	scsi_softc_t	*sc;
	target_info_t	*tgt;
	vm_offset_t	mem;

	if (!rz_check(dev, &sc, &tgt))
		return D_NO_SUCH_DEVICE;
	if (!tgt || (tgt->dev_ops != &scsi_audio))
		return D_NO_SUCH_DEVICE;

	/*
	 * Cleanup state
	 */
	mem = (vm_offset_t) tgt->dev_info.cdrom.result;
	tgt->dev_info.cdrom.result = (void *)0;
	tgt->dev_info.cdrom.result_available = FALSE;

	(void) kmem_free(kernel_map, mem, PAGE_SIZE);

	(void) rz_close(dev);

	tgt->dev_ops = &scsi_devsw[SCSI_CDROM];
	return D_SUCCESS;
}

/*
 * Write routine.  It is passed an ASCII string
 * with the command to be executed.
 */
io_return_t
cd_write(
	int		dev,
	io_req_t	ior)
{
	register kern_return_t	rc;
	boolean_t		wait = FALSE;
	io_return_t		ret;
	int			count;
	register char		*data;
	vm_offset_t		addr;

	data  = ior->io_data;
	count = ior->io_count;
	if (count == 0)
	    return D_SUCCESS;

	if (!(ior->io_op & IO_INBAND)) {
	    /*
	     * Copy out-of-line data into kernel address space.
	     * Since data is copied as page list, it will be
	     * accessible.
	     */
	    vm_map_copy_t copy = (vm_map_copy_t) data;
	    kern_return_t kr;

	    kr = vm_map_copyout(device_io_map, &addr, copy);
	    if (kr != KERN_SUCCESS)
		return kr;
	    data = (char *) addr;
	}

	if (scsi_debug)	printf("Got command '%s'\n", data);

	ret = cd_command( dev, data, count, ior);

	if (!(ior->io_op & IO_INBAND))
	    (void) vm_deallocate(device_io_map, addr, ior->io_count);
	return D_SUCCESS;
}

/*
 * Read routine. Returns an ASCII string with the results
 * of the last command executed.
 */
io_return_t
cd_read(
	int		dev,
	io_req_t	ior)
{
	target_info_t	*tgt;
	kern_return_t	rc;
	natural_t	count;

	/*
	 * Allocate memory for read buffer.
	 */
	count = (natural_t)ior->io_count;
	if (count > PAGE_SIZE)
		return D_INVALID_SIZE;	/* sanity */

	rc = device_read_alloc(ior, count);
	if (rc != KERN_SUCCESS)
		return rc;

	if (scsi_debug) printf("Got read req for %d bytes\n", count);

	/*
	 * See if last cmd left some to say
	 */
	tgt = scsi_softc[rzcontroller(dev)]->target[rzslave(dev)];
	if (tgt->dev_info.cdrom.result_available) {
		int len;

		tgt->dev_info.cdrom.result_available = FALSE;
		len = strlen(tgt->dev_info.cdrom.result)+1;

		if (count > len)
			count = len;
		bcopy(tgt->dev_info.cdrom.result, ior->io_data, count);

	} else {
#		define noway "No results pending"
		count = (count > sizeof(noway)) ? sizeof(noway) : count;
		bcopy(noway, ior->io_data, count);
	}

	ior->io_residual = ior->io_count - count;
	return D_SUCCESS;
}

/*
 * This does all the work
 */
io_return_t
cd_command(
	int		dev,
	char		*cmd,
	int		count,
	io_req_t	req)
{
	target_info_t	*tgt;
	io_req_t	ior;
	io_return_t	ret = D_INVALID_OPERATION;
	scsi_ret_t	rc;
	char		*buf;

	tgt = scsi_softc[rzcontroller(dev)]->target[rzslave(dev)];

	buf = tgt->dev_info.cdrom.result;
	tgt->dev_info.cdrom.result_available = FALSE;

	io_req_alloc(ior,0);		
	zero_ior( ior );

	switch (cmd[0]) {

	    case 'E':
	    		/* "Eject" */
		/* too many dont support it. Sigh */
		tgt->flags |= TGT_OPTIONAL_CMD;
		(void) scsi_medium_removal( tgt, TRUE, ior);
		tgt->flags &= ~TGT_OPTIONAL_CMD;

		zero_ior( ior );

		if_it_can_do(eject)
			rc = scsi_start_unit(tgt, SCSI_CMD_SS_EJECT, ior);
		break;

	    case 'G':
		switch (cmd[4]) {

		    case 'P':
	    		/* "Get Position MSF|ABS" */
		      if_it_can_do(current_position) {
			rc = scsi_read_subchannel(tgt,
						  cmd[13] == 'M',
						  SCSI_CMD_RS_FMT_CURPOS,
						  0,
						  ior);
			if (rc == SCSI_RET_SUCCESS) {
			    cdrom_chan_curpos_t	*st;
			    st = (cdrom_chan_curpos_t *)tgt->cmd_ptr;
			    if (cmd[13] == 'M')
				sprintf(buf, "MSF Position %d %d %d %d %d %d",
			    	      (integer_t)st->subQ.absolute_address.msf.minute,
				      (integer_t)st->subQ.absolute_address.msf.second,
				      (integer_t)st->subQ.absolute_address.msf.frame,
				      (integer_t)st->subQ.relative_address.msf.minute,
				      (integer_t)st->subQ.relative_address.msf.second,
				      (integer_t)st->subQ.relative_address.msf.frame);
			    else
				sprintf(buf, "ABS Position %d %d", (integer_t)
			    	      (st->subQ.absolute_address.lba.lba1<<24)+
			    		(st->subQ.absolute_address.lba.lba2<<16)+
			    		(st->subQ.absolute_address.lba.lba3<< 8)+
			    		 st->subQ.absolute_address.lba.lba4,
				       (integer_t)
			    		(st->subQ.relative_address.lba.lba1<<24)+
			    		(st->subQ.relative_address.lba.lba2<<16)+
			    		(st->subQ.relative_address.lba.lba3<< 8)+
			    		 st->subQ.relative_address.lba.lba4);
			    tgt->dev_info.cdrom.result_available = TRUE;
			}
		      }
			break;

		    case 'T':
	    		/* "Get TH" */
			if_it_can_do(read_toc) {
			    rc = scsi_read_toc(tgt, TRUE, 1, PAGE_SIZE, ior);
			    if (rc == SCSI_RET_SUCCESS) {
			      cdrom_toc_t *toc = (cdrom_toc_t *)tgt->cmd_ptr;
			      sprintf(buf, "toc header: %d %d %d",
					(toc->len1 << 8) + toc->len2,
					toc->first_track,
					toc->last_track);
			      tgt->dev_info.cdrom.result_available = TRUE;
			    }
			}
			break;

		    case 'S':
	    		/* "Get Status" */
			if_it_can_do(get_status) {
			    rc = scsi_read_subchannel(tgt,
						      TRUE,
						      SCSI_CMD_RS_FMT_CURPOS,
						      0,
						      ior);
			    if (rc == SCSI_RET_SUCCESS) {
				cdrom_chan_curpos_t *st;
				st = (cdrom_chan_curpos_t *)tgt->cmd_ptr;
				decode_status(buf, st->audio_status);
			        tgt->dev_info.cdrom.result_available = TRUE;
			    }
			}
			break;
		}
		break;

	    case 'P':
		switch (cmd[5]) {
		    case 'A':
	    		/* "Play A startM startS startF endM endS endF" */
			if_it_can_do(play_msf) {

				int sm, ss, sf, em, es, ef;

				sscanf(&cmd[7], "%d %d %d %d %d %d",
				       &sm, &ss, &sf, &em, &es, &ef);

				rc = scsi_play_audio_msf(tgt,
							 sm, ss, sf,
							 em, es, ef,
							 ior);
			}
			break;

		    case 'T':
	    		/* "Play TI startT startI endT endI" */
			if_it_can_do(play_ti) {

				int st, si, et, ei;

				sscanf(&cmd[8], "%d %d %d %d",
				       &st, &si, &et, &ei);

				rc = scsi_play_audio_track_index(tgt,
					st, si, et, ei, ior);
			}
			break;
		}
		break;

	    case 'R':
	    		/* "Resume" */
		if_it_can_do(pause_resume)
			rc = scsi_pause_resume(tgt, FALSE, ior);
		break;

	    case 'S':
		switch (cmd[2]) {

		    case 'a':
	    		/* "Start" */
			rc = scsi_start_unit(tgt, SCSI_CMD_SS_START, ior);
			break;

		    case 'o':
	    		/* "Stop" */
			if_it_can_do(pause_resume)
				rc = scsi_pause_resume(tgt, TRUE, ior);
			break;

		    case 't':
	    		/* "Set V chan0vol chan1vol chan2vol chan3vol" */
			if_it_can_do(volume_control) {

			    int v0, v1, v2, v3;
			    cdrom_audio_page_t	au, *aup;

			    rc = scsi_mode_sense(tgt,
					SCSI_CD_AUDIO_PAGE,
					sizeof(au),
					ior);
			    if (rc == SCSI_RET_SUCCESS) {

				sscanf(&cmd[6], "%d %d %d %d",
					&v0, &v1, &v2, &v3);

				aup = (cdrom_audio_page_t *) tgt->cmd_ptr;
				au = *aup;
				/* au.h.bdesc ... */
				au.vol0 = v0;
				au.vol1 = v1;
				au.vol2 = v2;
				au.vol3 = v3;
				au.imm = 1;
				au.aprv = 0;

				zero_ior( ior );

				rc = scsi2_mode_select(tgt, FALSE,
						&au, sizeof(au), ior);
			    }
			}
			break;
		}
		break;

	    case 'T':
	    		/* "Toc MSF|ABS trackno" */
		if_it_can_do(read_toc) {

		    int t, m;

		    sscanf(&cmd[8], "%d", &t);
		    rc = scsi_read_toc( tgt, cmd[4]=='M', t, PAGE_SIZE, ior);

		    if (rc == SCSI_RET_SUCCESS) {

			cdrom_toc_t	*toc = (cdrom_toc_t *)tgt->cmd_ptr;

			sprintf(buf, "TOC from track %d:\n", t);
			m = (toc->len1 << 8) + toc->len2;
			m -= 4; /* header */
			for (t = 0; m > 0; t++, m -= sizeof(struct cdrom_toc_desc)) {
			    buf += strlen(buf);
			    if (cmd[4] == 'M')
			      sprintf(buf, "%d %d %d %d %d %d\n",
			    	toc->descs[t].control,
			    	toc->descs[t].adr,
			    	toc->descs[t].trackno,
			    	(integer_t)toc->descs[t].absolute_address.msf.minute,
			    	(integer_t)toc->descs[t].absolute_address.msf.second,
			    	(integer_t)toc->descs[t].absolute_address.msf.frame);
			    else
			      sprintf(buf, "%d %d %d %d\n",
			    	toc->descs[t].control,
			    	toc->descs[t].adr,
			    	toc->descs[t].trackno,
			    	(toc->descs[t].absolute_address.lba.lba1<<24)+
			    	(toc->descs[t].absolute_address.lba.lba2<<16)+
			    	(toc->descs[t].absolute_address.lba.lba3<<8)+
			    	 toc->descs[t].absolute_address.lba.lba4);
			}
			tgt->dev_info.cdrom.result_available = TRUE;
		    }
		}
		break;
	}

	if (rc == SCSI_RET_SUCCESS)
		ret = D_SUCCESS;

	/* We are stateless, but.. */
	if (rc == SCSI_RET_NEED_SENSE) {
		zero_ior( ior );
		tgt->ior = ior;
		scsi_request_sense(tgt, ior, 0);
		iowait(ior);
		if (scsi_check_sense_data(tgt, tgt->cmd_ptr))
			scsi_print_sense_data(tgt->cmd_ptr);
	}

	io_req_free(ior);
	return ret;
}

private	char st_invalid [] = "Drive would not say";
private	char st_playing [] = "Playing";
private	char st_paused  [] = "Suspended";
private	char st_complete[] = "Done playing";
private	char st_error   [] = "Stopped in error";
private	char st_nothing [] = "Idle";

private void
decode_status(
	char		*buf,
	unsigned char	audio_status)
{
    switch (audio_status) {
	case SCSI_CDST_INVALID:
		sprintf(buf, st_invalid); break;
	case SCSI_CDST_PLAYING:
		sprintf(buf, st_playing); break;
	case SCSI_CDST_PAUSED:
		sprintf(buf, st_paused); break;
	case SCSI_CDST_COMPLETED:
		sprintf(buf, st_complete); break;
	case SCSI_CDST_ERROR:
		sprintf(buf, st_error); break;
	case SCSI_CDST_NO_STATUS:
		sprintf(buf, st_nothing); break;
    }
}

/* some vendor specific use this instead */
private void
decode_status_1(
	char		*buf,
	unsigned char	audio_status)
{
	switch (audio_status) {
	    case 0:	sprintf(buf, st_playing ); break;
	    case 1:
	    case 2:	sprintf(buf, st_paused ); break;
	    case 3:	sprintf(buf, st_complete ); break;
	    default:
		sprintf(buf, "Unknown status" ); break;
	}
}


private void
curse_the_vendor(
	red_list_t	*list,
	boolean_t	not_really)
{
	if (not_really) return;

	printf("%s\n%s\n%s\n%s\n",
		"The CDROM you use is not fully SCSI-2 compliant.",
		"We invite You to contact Your vendor and ask",
		"that they provide You with a firmware upgrade.",
		"Here is a list of some known deficiencies");

	printf("Vendor: %s Product: %s.. Revision: %s..\n",
		list->vendor, list->product, list->rev);

#define check(x,y,z) \
	if (list->x) printf("Command code x%x %s not supported\n", y, z);

	check(can_play_audio, SCSI_CMD_PLAY_AUDIO, "PLAY_AUDIO");
	check(eject, SCSI_CMD_START_STOP_UNIT,
		"START_STOP_UNIT, flag EJECT(0x2) in byte 5");
	check(current_position, SCSI_CMD_READ_SUBCH, "READ_SUBCHANNEL");
	check(read_toc, SCSI_CMD_READ_TOC, "READ_TOC");
/*	check(get_status, ...); duplicate of current_position */
	check(play_msf, SCSI_CMD_PLAY_AUDIO_MSF, "PLAY_AUDIO_MSF");
	check(play_ti, SCSI_CMD_PLAY_AUDIO_TI, "PLAY_AUDIO_TRACK_INDEX");
	check(pause_resume, SCSI_CMD_PAUSE_RESUME, "PAUSE_RESUME");
	check(volume_control, SCSI_CMD_MODE_SELECT,
		"MODE_SELECT, AUDIO page(0xe)");

#undef	check
	printf("Will work around these problems...\n");
}

/*
 * Ancillaries
 */
cd_strategy(ior)
	register io_req_t	ior;
{
	return rz_simpleq_strategy( ior, cd_start);
}

void cd_start( tgt, done)
	target_info_t	*tgt;
	boolean_t	done;
{
	io_req_t	ior;

	ior = tgt->ior;
	if (done && ior) {
		tgt->ior = 0;
		iodone(ior);
		return;
	}
	panic("cd start"); /* uhu? */
}

/*
 * When the hardware cannot
 */
private scsi_ret_t
op_not_supported(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	/*
	 * The command is not implemented, no way around it
	 */
	sprintf(tgt->dev_info.cdrom.result, unsupported);
	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

/****************************************/
/*	Vendor Specific Operations	*/
/****************************************/

	/*   DEC RRD42   */

#define SCSI_CMD_DEC_SET_ADDRESS_FORMAT		0xc0
#	define scsi_cmd_saf_fmt	scsi_cmd_xfer_len_2

#define SCSI_CMD_DEC_PLAYBACK_STATUS		0xc4
typedef struct {
    unsigned char	xxx;
    BITFIELD_2(unsigned char,
    	is_msf: 1,
	xxx1:   7);
    unsigned char	data_len1;
    unsigned char	data_len0;
    unsigned char	audio_status;
    BITFIELD_2(unsigned char,
        control	: 4,
	xxx2 : 4);
    cdrom_addr_t address;
    BITFIELD_2(unsigned char,
        chan0_select : 4,
	xxx3 : 4);
    unsigned char	chan0_volume;
    BITFIELD_2(unsigned char,
        chan1_select : 4,
	xxx4 : 4);
    unsigned char	chan1_volume;
    BITFIELD_2(unsigned char,
        chan2_select : 4,
	xxx5 : 4);
    unsigned char	chan2_volume;
    BITFIELD_2(unsigned char,
        chan3_select : 4,
	xxx6 : 4);
    unsigned char	chan3_volume;
} dec_playback_status_t;

#define SCSI_CMD_DEC_PLAYBACK_CONTROL		0xc9
typedef struct {
    unsigned char	xxx0;
    BITFIELD_2(unsigned char,
        fmt  : 1,
	xxx1 : 7);
    unsigned char	xxx[8];
    BITFIELD_2(unsigned char,
        chan0_select : 4,
	xxx3 : 4);
    unsigned char	chan0_volume;
    BITFIELD_2(unsigned char,
        chan1_select : 4,
	xxx4 : 4);
    unsigned char	chan1_volume;
    BITFIELD_2(unsigned char,
        chan2_select : 4,
	xxx5 : 4);
    unsigned char	chan2_volume;
    BITFIELD_2(unsigned char,
        chan3_select : 4,
	xxx6 : 4);
    unsigned char	chan3_volume;
} dec_playback_control_t;


#if	0

private scsi_ret_t
rrd42_status(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_ret_t		rc;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_command_group_2	c;
	dec_playback_status_t	*st;

	/* We might have to specify addressing fmt */
	if (cmd[4] == 'P') {
		scsi_command_group_2 saf;

		bzero(&saf, sizeof(saf));
		saf.scsi_cmd_code = SCSI_CMD_DEC_SET_ADDRESS_FORMAT;
		saf.scsi_cmd_saf_fmt = (cmd[13] == 'A') ? 0 : 1;

		rc = cdrom_vendor_specific(tgt, &saf, 0, 0, 0, ior);

		if (rc != SCSI_RET_SUCCESS) return rc;

		zero_ior( ior );
	}

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_DEC_PLAYBACK_STATUS;
	c.scsi_cmd_xfer_len_2 = sizeof(*st);
	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*st), ior);

	if (rc != SCSI_RET_SUCCESS) return rc;

	st = (dec_playback_status_t *) tgt->cmd_ptr;

	if (cmd[4] == 'S')
		decode_status( buf, st->audio_status+0x11 );
	else {
		if (st->is_msf)
		    sprintf(buf, "MSF Position %d %d %d",
		    	(integer_t)st->address.msf.minute,
		    	(integer_t)st->address.msf.second,
		    	(integer_t)st->address.msf.frame);
		else
		    sprintf(buf, "ABS Position %d", (integer_t)
		    	(st->address.lba.lba1<<24)+
		    	(st->address.lba.lba2<<16)+
		    	(st->address.lba.lba3<< 8)+
		    	 st->address.lba.lba4);
	}
	tgt->dev_info.cdrom.result_available = TRUE;
	return rc;
}
#endif

private scsi_ret_t
rrd42_set_volume(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	dec_playback_control_t		req;
	int				v0, v1, v2, v3;

	sscanf(&cmd[6], "%d %d %d %d",	&v0, &v1, &v2, &v3);

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_DEC_PLAYBACK_CONTROL;
	c.scsi_cmd_xfer_len_2 = sizeof(req);
	bzero(&req, sizeof(req));
	if (v0) {
		req.chan0_select = 1;
		req.chan0_volume = v0;
	}
	if (v1) {
		req.chan1_select = 2;
		req.chan1_volume = v1;
	}
	if (v2) {
		req.chan2_select = 4;
		req.chan2_volume = v2;
	}
	if (v3) {
		req.chan3_select = 8;
		req.chan3_volume = v3;
	}
	return cdrom_vendor_specific(tgt, &c, &req, sizeof(req), 0, ior);
}

	/* NEC CD-ROM */

#define SCSI_CMD_NEC_READ_TOC           	0xde
typedef struct {
    unsigned char	xxx[9];
    unsigned char	first_track;
    unsigned char	xxx1[9];
    unsigned char	last_track;
    unsigned char	xxx2[9];
    unsigned char	lead_out_addr[3];
    struct {
	BITFIELD_2(unsigned char,
	    adr  : 4,
	    ctrl : 4);
	unsigned char	xxx3[6];
	unsigned char	address[3];
    } track_info[1]; /* VARSIZE */
} nec_toc_data_t;

#define SCSI_CMD_NEC_SEEK_TRK			0xd8
#define SCSI_CMD_NEC_PLAY_AUDIO			0xd9
#define SCSI_CMD_NEC_PAUSE			0xda
#define	SCSI_CMD_NEC_EJECT			0xdc

#define SCSI_CMD_NEC_READ_SUBCH_Q		0xdd
typedef struct {
    unsigned char	audio_status;	/* see decode_status_1 */
    BITFIELD_2(unsigned char,
        ctrl : 4,
	xxx1 : 4);
    unsigned char	trackno;
    unsigned char	indexno;
    unsigned char	relative_address[3];
    unsigned char	absolute_address[3];
} nec_subch_data_t;

/*
 * Reserved bits in byte1
 */
#define NEC_LR_PLAY_MODE	0x01	/* RelAdr bit overload */
#define	NEC_LR_STEREO		0x02	/* mono/stereo */

/*
 * Vendor specific bits in the control byte.
 * NEC uses them to specify the addressing mode
 */
#define NEC_CTRL_A_ABS		0x00	/* XXX not sure about this */
#define NEC_CTRL_A_MSF		0x40	/* min/sec/frame */
#define NEC_CTRL_A_TI		0x80	/* track/index */
#define NEC_CTRL_A_CURRENT	0xc0	/* same as last specified */

private scsi_ret_t
nec_eject(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_NEC_EJECT;

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
nec_subchannel(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	nec_subch_data_t	*st;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_NEC_READ_SUBCH_Q;
	c.scsi_cmd_lun_and_relbit = sizeof(*st);	/* Sic! */

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*st), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	st = (nec_subch_data_t *) tgt->cmd_ptr;

	/* Status or Position ? */

	if (cmd[4] == 'S') {
	    decode_status_1( buf, st->audio_status);
	} else {

	    /* XXX can it do ABS addressing e.g. 'logical' ? */

	    sprintf(buf, "MSF Position %d %d %d %d %d %d",
		    (integer_t)bcd_to_decimal(st->absolute_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->absolute_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->absolute_address[2]), /* frm */
		    (integer_t)bcd_to_decimal(st->relative_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->relative_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->relative_address[2])); /* frm */
	}

	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

private scsi_ret_t
nec_read_toc(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	nec_toc_data_t		*t;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;
	int			first, last, i;

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_NEC_READ_TOC;
	c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE|NEC_LR_STEREO;

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, 512/*XXX*/, ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	t = (nec_toc_data_t *) tgt->cmd_ptr;

	first = bcd_to_decimal(t->first_track);
	last  = bcd_to_decimal(t->last_track);

	/*
	 * "Get TH" wants summary, "TOC MSF|ABS from_track" wants all
	 */
	if (cmd[0] == 'G') {
	    sprintf(buf, "toc header: %d %d %d",
		sizeof(*t) + sizeof(t->track_info) * (last - first - 1),
		first, last);
	    goto out;
	}

	/*
	 * The whole shebang
	 */
	sscanf(&cmd[8], "%d", &i);
	sprintf(buf, "TOC from track %d:\n", i);

	last -= first;
	i -= first;
	while ((i >= 0) && (i <= last)) {
	    buf += strlen(buf);
	    if (cmd[4] == 'M')
	      sprintf(buf, "%d %d %d %d %d %d\n",
			t->track_info[i].ctrl,
			t->track_info[i].adr,
			first + i,
			bcd_to_decimal(t->track_info[i].address[0]),
			bcd_to_decimal(t->track_info[i].address[1]),
			bcd_to_decimal(t->track_info[i].address[2]));
	    else
/* THIS IS WRONG */
	      sprintf(buf, "%d %d %d %d\n",
			t->track_info[i].ctrl,
			t->track_info[i].adr,
			first + i,
			bcd_to_decimal(t->track_info[i].address[0]) * 10000 +
			bcd_to_decimal(t->track_info[i].address[1]) * 100 +
			bcd_to_decimal(t->track_info[i].address[2]));
	    i++;
	}
	/* To know how long the last track is */
	buf += strlen(buf);
	if (cmd[4] == 'M')
	  sprintf(buf, "%d %d %d %d %d %d\n",
		  0, 1, 0xaa /* User expects this */,
		  bcd_to_decimal(t->lead_out_addr[0]),
		  bcd_to_decimal(t->lead_out_addr[1]),
		  bcd_to_decimal(t->lead_out_addr[2]));
	    else
/* THIS IS WRONG */
	  sprintf(buf, "%d %d %d %d\n",
		  0, 1, 0xaa /* User expects this */,
		  bcd_to_decimal(t->lead_out_addr[0]) * 10000 +
		  bcd_to_decimal(t->lead_out_addr[1]) * 100 +
		  bcd_to_decimal(t->lead_out_addr[2]));
out:
	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}


private scsi_ret_t
nec_play(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	int			sm, ss, sf, em, es, ef;
	int			st, si, et, ei;
	scsi_ret_t		rc;

	/*
	 * Seek to desired position
	 */
	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_NEC_SEEK_TRK;
	c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE;

	/*
	 * Play_msf or Play_ti
	 */
	if (cmd[5] == 'A') {
		/* "Play A startM startS startF endM endS endF" */

		sscanf(&cmd[7], "%d %d %d %d %d %d",
		       &sm, &ss, &sf, &em, &es, &ef);

		c.scsi_cmd_lba1 = decimal_to_bcd(sm);
		c.scsi_cmd_lba2 = decimal_to_bcd(ss);
		c.scsi_cmd_lba3 = decimal_to_bcd(sf);
	        c.scsi_cmd_ctrl_byte = NEC_CTRL_A_MSF;

	} else {
		/* "Play TI startT startI endT endI" */

		sscanf(&cmd[8], "%d %d %d %d", &st, &si, &et, &ei);

		c.scsi_cmd_lba1 = decimal_to_bcd(st);
		c.scsi_cmd_lba2 = decimal_to_bcd(si);
		c.scsi_cmd_lba3 = 0;
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_TI;

	}

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	/*
	 * Now ask it to play until..
	 */
	zero_ior( ior );

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_NEC_PLAY_AUDIO;
	c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE|NEC_LR_STEREO;

	if (cmd[5] == 'A') {
		c.scsi_cmd_lba1 = decimal_to_bcd(em);
		c.scsi_cmd_lba2 = decimal_to_bcd(es);
		c.scsi_cmd_lba3 = decimal_to_bcd(ef);
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_MSF;
	} else {
		c.scsi_cmd_lba1 = decimal_to_bcd(et);
		c.scsi_cmd_lba2 = decimal_to_bcd(ei);
		c.scsi_cmd_lba3 = 0;
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_TI;
	}

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
nec_pause_resume(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
	/*
	 * "Resume" or "Stop"
	 */
	if (cmd[0] == 'R') {
	        c.scsi_cmd_code = SCSI_CMD_NEC_PLAY_AUDIO;
	        c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE|NEC_LR_STEREO;
	        c.scsi_cmd_ctrl_byte = NEC_CTRL_A_CURRENT;
	} else {
	        c.scsi_cmd_code = SCSI_CMD_NEC_PAUSE;
	}

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

	/* TOSHIBA CD-ROM DRIVE:XM 3232 */

#define	SCSI_CMD_TOSHIBA_SEEK_TRK		0xc0
#define	SCSI_CMD_TOSHIBA_PLAY_AUDIO		0xc1
#define	SCSI_CMD_TOSHIBA_PAUSE_AUDIO		0xc2
#define	SCSI_CMD_TOSHIBA_EJECT			0xc4

#define	SCSI_CMD_TOSHIBA_READ_SUBCH_Q		0xc6
typedef nec_subch_data_t toshiba_subch_data_t;
/* audio status -> decode_status_1 */

#define	SCSI_CMD_TOSHIBA_READ_TOC_ENTRY		0xc7
typedef struct {
    unsigned char       first_track;
    unsigned char       last_track;
    unsigned char       xxx[2];
} toshiba_toc_header_t;
typedef struct {
    unsigned char       address[4];
} toshiba_toc_data_t;


private scsi_ret_t
toshiba_eject(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_EJECT;

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
toshiba_subchannel(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	toshiba_subch_data_t	*st;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_READ_SUBCH_Q;
	c.scsi_cmd_lun_and_relbit = sizeof(*st);	/* Sic! */

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*st), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	st = (toshiba_subch_data_t *) tgt->cmd_ptr;

	/* Status or Position ? */

	if (cmd[4] == 'S') {
	    decode_status_1( buf, st->audio_status);
	} else {

	    /* XXX can it do ABS addressing e.g. 'logical' ? */

	    sprintf(buf, "MSF Position %d %d %d %d %d %d",
		    (integer_t)bcd_to_decimal(st->absolute_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->absolute_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->absolute_address[2]), /* frm */
		    (integer_t)bcd_to_decimal(st->relative_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->relative_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->relative_address[2])); /* frm */
	}

	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

private scsi_ret_t
toshiba_read_toc(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	toshiba_toc_data_t	*t;
	toshiba_toc_header_t	*th;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;
	int			first, last, i;

	/* TOC header first */
	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_READ_TOC_ENTRY;
	c.scsi_cmd_lun_and_relbit = 0;
	c.scsi_cmd_lba1 = 0;

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*th), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	th = (toshiba_toc_header_t *) tgt->cmd_ptr;

	first = bcd_to_decimal(th->first_track);
	last  = bcd_to_decimal(th->last_track);

	/*
	 * "Get TH" wants summary, "TOC MSF|ABS from_track" wants all
	 */
	if (cmd[0] == 'G') {
	    sprintf(buf, "toc header: %d %d %d",
		sizeof(*th) + sizeof(*t) * (last - first + 1),
		first, last);
	    goto out;
	}

	/*
	 * The whole shebang
	 */
	sscanf(&cmd[8], "%d", &i);
	sprintf(buf, "TOC from track %d:\n", i);

	while (i <= last) {
	    bzero(&c, sizeof(c));

	    c.scsi_cmd_code = SCSI_CMD_TOSHIBA_READ_TOC_ENTRY;
	    c.scsi_cmd_lun_and_relbit = 2;
	    c.scsi_cmd_lba1 = decimal_to_bcd(i);

	    zero_ior( ior );
	    rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*t), ior);
	    if (rc != SCSI_RET_SUCCESS) break;
	
	    t = (toshiba_toc_data_t *) tgt->cmd_ptr;

	    buf += strlen(buf);
	    if (cmd[4] == 'M')
	      sprintf(buf, "0 0 %d %d %d %d\n",
			i,
			bcd_to_decimal(t->address[0]),
			bcd_to_decimal(t->address[1]),
			bcd_to_decimal(t->address[2]));
	    else
/* THIS IS WRONG */
	      sprintf(buf, "0 0 %d %d\n",
			i,
			bcd_to_decimal(t->address[0]) * 10000 +
			bcd_to_decimal(t->address[1]) * 100 +
			bcd_to_decimal(t->address[2]));
	    i++;
	}

	/* Must simulate the lead-out track */
	bzero(&c, sizeof(c));

	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_READ_TOC_ENTRY;
	c.scsi_cmd_lun_and_relbit = 1;
	c.scsi_cmd_lba1 = 0;

	zero_ior( ior );
	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(*t), ior);
	if (rc != SCSI_RET_SUCCESS) goto out;
	
	t = (toshiba_toc_data_t *) tgt->cmd_ptr;

	buf += strlen(buf);
	if (cmd[4] == 'M')
	  sprintf(buf, "0 0 %d %d %d %d\n",
			i,
			bcd_to_decimal(t->address[0]),
			bcd_to_decimal(t->address[1]),
			bcd_to_decimal(t->address[2]));
	else
/* THIS IS WRONG */
	  sprintf(buf, "0 0 %d %d\n",
			i,
			bcd_to_decimal(t->address[0]) * 10000 +
			bcd_to_decimal(t->address[1]) * 100 +
			bcd_to_decimal(t->address[2]));
	i++;

out:
	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}


private scsi_ret_t
toshiba_play(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	int			sm, ss, sf, em, es, ef;
	int			st, si, et, ei;
	scsi_ret_t		rc;

	/*
	 * Seek to desired position
	 */
	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_SEEK_TRK;

	/*
	 * Play_msf or Play_ti
	 */
	if (cmd[5] == 'A') {
		/* "Play A startM startS startF endM endS endF" */

		sscanf(&cmd[7], "%d %d %d %d %d %d",
		       &sm, &ss, &sf, &em, &es, &ef);

		c.scsi_cmd_lba1 = decimal_to_bcd(sm);
		c.scsi_cmd_lba2 = decimal_to_bcd(ss);
		c.scsi_cmd_lba3 = decimal_to_bcd(sf);
	        c.scsi_cmd_ctrl_byte = NEC_CTRL_A_MSF;

	} else {
		/* "Play TI startT startI endT endI" */

		sscanf(&cmd[8], "%d %d %d %d", &st, &si, &et, &ei);

		c.scsi_cmd_lba1 = decimal_to_bcd(st);
		c.scsi_cmd_lba2 = decimal_to_bcd(si);
		c.scsi_cmd_lba3 = 0;
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_TI;

	}

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
	if (rc != SCSI_RET_SUCCESS) return rc;
	
	/*
	 * Now ask it to play until..
	 */
	zero_ior( ior );

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_TOSHIBA_PLAY_AUDIO;
	c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE|NEC_LR_STEREO;

	if (cmd[5] == 'A') {
		c.scsi_cmd_lba1 = decimal_to_bcd(em);
		c.scsi_cmd_lba2 = decimal_to_bcd(es);
		c.scsi_cmd_lba3 = decimal_to_bcd(ef);
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_MSF;
	} else {
		c.scsi_cmd_lba1 = decimal_to_bcd(et);
		c.scsi_cmd_lba2 = decimal_to_bcd(ei);
		c.scsi_cmd_lba3 = 0;
		c.scsi_cmd_ctrl_byte = NEC_CTRL_A_TI;
	}

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
toshiba_pause_resume(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
	/*
	 * "Resume" or "Stop"
	 */
	if (cmd[0] == 'R') {
		/* ???? would have to remember last cmd ???? */
/* broken ! */
	        c.scsi_cmd_code = SCSI_CMD_TOSHIBA_PLAY_AUDIO;
	        c.scsi_cmd_lun_and_relbit = NEC_LR_PLAY_MODE|NEC_LR_STEREO;
	        c.scsi_cmd_ctrl_byte = NEC_CTRL_A_CURRENT;
	} else {
	        c.scsi_cmd_code = SCSI_CMD_TOSHIBA_PAUSE_AUDIO;
	}

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}


#if	0
	/* I have info on these drives, but no drive to test */

	/* PIONEER DRM-600 */

#define SCSI_CMD_PIONEER_EJECT			0xc0

#define SCSI_CMD_PIONEER_READ_TOC		0xc1
typedef struct {
	unsigned char	first_track;
	unsigned char	last_track;
	unsigned char	xxx[2];
} pioneer_toc_hdr_t;
typedef struct {
	unsigned char	ctrl;
	unsigned char	address[3];
} pioneer_toc_info_t;

#define SCSI_CMD_PIONEER_READ_SUBCH		0xc2
typedef struct {
    BITFIELD_2(unsigned char,
        ctrl : 4,
	xxx1 : 4);
    unsigned char	trackno;
    unsigned char	indexno;
    unsigned char	relative_address[3];
    unsigned char	absolute_address[3];
} pioneer_subch_data_t;

#define SCSI_CMD_PIONEER_SEEK_TRK		0xc8
#define SCSI_CMD_PIONEER_PLAY_AUDIO		0xc9
#define SCSI_CMD_PIONEER_PAUSE			0xca

#define SCSI_CMD_PIONEER_AUDIO_STATUS		0xcc
typedef struct {
	unsigned char	audio_status;
	unsigned char	xxx[5];
} pioneer_status_t;

/*
 * Reserved bits in byte1
 */
#define PIONEER_LR_END_ADDR	0x10
#define PIONEER_LR_PAUSE	0x10
#define PIONEER_LR_RESUME	0x00

/*
 * Vendor specific bits in the control byte.
 */
#define PIONEER_CTRL_TH		0x00	/* TOC header */
#define PIONEER_CTRL_TE		0x80	/* one TOC entry */
#define PIONEER_CTRL_LO		0x40	/* lead-out track info */

#define PIONEER_CTRL_A_MSF	0x40	/* min/sec/frame addr */

private scsi_ret_t
pioneer_eject(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_EJECT;

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
pioneer_position(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	scsi_ret_t		rc;
	char			*buf = tgt->dev_info.cdrom.result;
	pioneer_subch_data_t	*st;

	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_READ_SUBCH;
        c.scsi_cmd_xfer_len_2 = sizeof(pioneer_subch_data_t); /* 9 bytes */

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(pioneer_subch_data_t), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;

	st = (pioneer_subch_data_t *) tgt->cmd_ptr;

	/* XXX can it do ABS addressing e.g. 'logical' ? */

	sprintf(buf, "MSF Position %d %d %d %d %d %d",
		    (integer_t)bcd_to_decimal(st->absolute_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->absolute_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->absolute_address[2]), /* frm */
		    (integer_t)bcd_to_decimal(st->relative_address[0]), /* min */
		    (integer_t)bcd_to_decimal(st->relative_address[1]), /* sec */
		    (integer_t)bcd_to_decimal(st->relative_address[2])); /* frm */

	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

private scsi_ret_t
pioneer_toc(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	pioneer_toc_hdr_t	*th;
	pioneer_toc_info_t	*t;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;
	int			first, last, i;

	/* Read header first */
	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_READ_TOC;
        c.scsi_cmd_xfer_len_2 = sizeof(pioneer_toc_hdr_t);
        c.scsi_cmd_ctrl_byte = PIONEER_CTRL_TH;

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(pioneer_toc_hdr_t), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;

	th = (pioneer_toc_hdr_t *)tgt->cmd_ptr;
	first = bcd_to_decimal(th->first_track);
	last = bcd_to_decimal(th->last_track);

	/*
	 * "Get TH" wants summary, "TOC MSF|ABS from_track" wants all
	 */
	if (cmd[0] == 'G') {
	    sprintf(buf, "toc header: %d %d %d", 0, first, last);
	    goto out;
	}

	/*
	 * Must do it one track at a time
	 */
	sscanf(&cmd[8], "%d", &i);
	sprintf(buf, "TOC from track %d:\n", i);

	for ( ; i <= last; i++) {
	    zero_ior(ior);
	    bzero(&c, sizeof(c));
	    c.scsi_cmd_code = SCSI_CMD_PIONEER_READ_TOC;
	    c.scsi_cmd_lba4 = decimal_to_bcd(i);
	    c.scsi_cmd_xfer_len_2 = sizeof(pioneer_toc_info_t);
	    c.scsi_cmd_ctrl_byte = PIONEER_CTRL_TE;

	    rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(pioneer_toc_info_t), ior);
	    if (rc != SCSI_RET_SUCCESS) break;

	    t = (pioneer_toc_info_t *)tgt->cmd_ptr;

	    buf += strlen(buf);
	    if (cmd[4] == 'M')
	      sprintf(buf, "%d %d %d %d %d %d\n",
			t->ctrl, 0, i,
			bcd_to_decimal(t->address[0]),
			bcd_to_decimal(t->address[1]),
			bcd_to_decimal(t->address[2]));
	    else
/* THIS IS WRONG */
	      sprintf(buf, "%d %d %d %d\n",
			t->ctrl, 0, i,
			bcd_to_decimal(t->address[0]) * 10000 +
			bcd_to_decimal(t->address[1]) * 100 +
			bcd_to_decimal(t->address[2]));
	}
	/* To know how long the last track is */
	zero_ior(ior);
	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_PIONEER_READ_TOC;
	c.scsi_cmd_xfer_len_2 = sizeof(pioneer_toc_info_t);
	c.scsi_cmd_ctrl_byte = PIONEER_CTRL_LO;

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(pioneer_toc_info_t), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;

	buf += strlen(buf);
	t = (pioneer_toc_info_t *)tgt->cmd_ptr;
	if (cmd[4] == 'M')
	      sprintf(buf, "%d %d %d %d %d %d\n",
			t->ctrl, 0, 0xaa /* User expects this */,
			bcd_to_decimal(t->address[0]),
			bcd_to_decimal(t->address[1]),
			bcd_to_decimal(t->address[2]));
	else
/* THIS IS WRONG */
	      sprintf(buf, "%d %d %d %d\n",
			t->ctrl, 0, 0xaa /* User expects this */,
			bcd_to_decimal(t->address[0]) * 10000 +
			bcd_to_decimal(t->address[1]) * 100 +
			bcd_to_decimal(t->address[2]));

out:
	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

private scsi_ret_t
pioneer_status(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	pioneer_status_t	*st;
	char			*buf = tgt->dev_info.cdrom.result;
	scsi_ret_t		rc;

	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_AUDIO_STATUS;
        c.scsi_cmd_xfer_len_2 = sizeof(pioneer_status_t); /* 6 bytes */

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, sizeof(pioneer_status_t), ior);
	if (rc != SCSI_RET_SUCCESS) return rc;

	st = (pioneer_status_t*) tgt->cmd_ptr;
	decode_status_1( buf, st->audio_status);

	tgt->dev_info.cdrom.result_available = TRUE;
	return SCSI_RET_SUCCESS;
}

private scsi_ret_t
pioneer_play(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;
	int			sm, ss, sf, em, es, ef;
	int			st, si, et, ei;
	scsi_ret_t		rc;

	/*
	 * Seek to desired position
	 */
	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_SEEK_TRK;
	/*
	 * Play_msf or Play_ti
	 */
	if (cmd[5] == 'A') {
		/* "Play A startM startS startF endM endS endF" */

		sscanf(&cmd[7], "%d %d %d %d %d %d",
		       &sm, &ss, &sf, &em, &es, &ef);

		c.scsi_cmd_lba2 = decimal_to_bcd(sm);
		c.scsi_cmd_lba3 = decimal_to_bcd(ss);
		c.scsi_cmd_lba4 = decimal_to_bcd(sf);
	        c.scsi_cmd_ctrl_byte = PIONEER_CTRL_A_MSF;

	} else {
		/* "Play TI startT startI endT endI" */

		sscanf(&cmd[8], "%d %d %d %d", &st, &si, &et, &ei);

		c.scsi_cmd_lba3 = decimal_to_bcd(st);
		c.scsi_cmd_lba4 = decimal_to_bcd(si);
		c.scsi_cmd_ctrl_byte = 0x80;	/* Pure speculation!! */

	}

	rc = cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
	if (rc != SCSI_RET_SUCCESS) return rc;

	/*
	 * Now ask it to play until..
	 */
	zero_ior( ior );

	bzero(&c, sizeof(c));
	c.scsi_cmd_code = SCSI_CMD_PIONEER_PLAY_AUDIO;
	c.scsi_cmd_lun_and_relbit = PIONEER_LR_END_ADDR;

	if (cmd[5] == 'A') {
		c.scsi_cmd_lba2 = decimal_to_bcd(em);
		c.scsi_cmd_lba3 = decimal_to_bcd(es);
		c.scsi_cmd_lba4 = decimal_to_bcd(ef);
		c.scsi_cmd_ctrl_byte = PIONEER_CTRL_A_MSF;
	} else {
		c.scsi_cmd_lba3 = decimal_to_bcd(et);
		c.scsi_cmd_lba4 = decimal_to_bcd(ei);
		c.scsi_cmd_ctrl_byte = 0x80;	/* Pure speculation! */
	}

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

private scsi_ret_t
pioneer_pause_resume(
	target_info_t	*tgt,
	char		*cmd,
	io_req_t	ior)
{
	scsi_command_group_2	c;

	bzero(&c, sizeof(c));
        c.scsi_cmd_code = SCSI_CMD_PIONEER_PAUSE;
	/*
	 * "Resume" or "Stop"
	 */
	if (cmd[0] == 'S')
		c.scsi_cmd_lun_and_relbit = PIONEER_LR_PAUSE;
	else
		c.scsi_cmd_lun_and_relbit = PIONEER_LR_RESUME;

	return cdrom_vendor_specific(tgt, &c, 0, 0, 0, ior);
}

	/* DENON DRD-253 */

#define SCSI_CMD_DENON_PLAY_AUDIO		0x22
#define SCSI_CMD_DENON_EJECT			0xe6
#define SCSI_CMD_DENON_PAUSE_AUDIO		0xe7
#define SCSI_CMD_DENON_READ_TOC			0xe9
#define SCSI_CMD_DENON_READ_SUBCH		0xeb


	/* HITACHI 1750 */

#define SCSI_CMD_HITACHI_PLAY_AUDIO_MSF		0xe0
#define SCSI_CMD_HITACHI_PAUSE_AUDIO		0xe1
#define SCSI_CMD_HITACHI_EJECT			0xe4
#define SCSI_CMD_HITACHI_READ_SUBCH		0xe5
#define SCSI_CMD_HITACHI_READ_TOC		0xe8

#endif

/*
 * Tabulate all of the above
 */
private red_list_t	cdrom_exceptions[] = {

#if 0
	For documentation purposes, here are some SCSI-2 compliant drives:

	Vendor		Product			Rev	 Comments

	"SONY    "	"CD-ROMCDU-541   "	"2.6a"	 The NeXT drive
#endif

	/* vendor, product, rev */
	/* can_play_audio */
	/* eject */
	/* current_position */
	/* read_toc */
	/* get_status */
	/* play_msf */
	/* play_ti */
	/* pause_resume */
	/* volume_control */

	  /* We have seen a "RRD42(C)DEC     " "4.5d" */
	{ "DEC     ", "RRD42", "",
	  0, 0, 0, 0, 0, 0, 0, 0, rrd42_set_volume },

	  /* We have seen a "CD-ROM DRIVE:84 " "1.0 " */
	{ "NEC     ", "CD-ROM DRIVE:84", "",
	  op_not_supported, nec_eject, nec_subchannel, nec_read_toc,
	  nec_subchannel, nec_play, nec_play, nec_pause_resume,
	  op_not_supported },

	  /* We have seen a "CD-ROM DRIVE:XM " "3232" */
	{ "TOSHIBA ", "CD-ROM DRIVE:XM", "32",
	  op_not_supported, toshiba_eject, toshiba_subchannel, toshiba_read_toc,
	  toshiba_subchannel, toshiba_play, toshiba_play, toshiba_pause_resume,
	  op_not_supported },

	{ "TOSHIBA ", "CD-ROM DRIVE:XM", "33",
	  op_not_supported, toshiba_eject, toshiba_subchannel, toshiba_read_toc,
	  toshiba_subchannel, toshiba_play, toshiba_play, toshiba_pause_resume,
	  op_not_supported },

#if 0
	{ "PIONEER ", "???????DRM-6", "",
	  op_not_supported, pioneer_eject, pioneer_position, pioneer_toc,
	  pioneer_status, pioneer_play, pioneer_play, pioneer_pause_resume,
	  op_not_supported },

	{ "DENON   ", "DRD 25X", "", ...},
	{ "HITACHI ", "CDR 1750S", "", ...},
	{ "HITACHI ", "CDR 1650S", "", ...},
	{ "HITACHI ", "CDR 3650", "", ...},

#endif

	/* Zero terminate this list */
	{ 0, }
};

private void
check_red_list(
	target_info_t		*tgt,
	scsi2_inquiry_data_t	*inq)

{
	red_list_t		*list;

	for (list = &cdrom_exceptions[0]; list->vendor; list++) {

		/*
		 * Prefix-Match all strings 
		 */
		if ((strncmp(list->vendor, (const char *)inq->vendor_id,
				strlen(list->vendor)) == 0) &&
		    (strncmp(list->product, (const char *)inq->product_id,
				strlen(list->product)) == 0) &&
		    (strncmp(list->rev, (const char *)inq->product_rev,
				 strlen(list->rev)) == 0)) {
			/*
			 * One of them..
			 */
			if (tgt->dev_info.cdrom.violates_standards != list) {
			    tgt->dev_info.cdrom.violates_standards = list;
			    curse_the_vendor( list, TRUE );
			}
			return;
		}
	}
}
#endif  /* NSCSI > 0 */
