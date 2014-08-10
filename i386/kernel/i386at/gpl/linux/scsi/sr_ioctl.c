#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/errno.h>

#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sr.h"
#include "scsi_ioctl.h"

#include <linux/cdrom.h>

#define IOCTL_RETRIES 3
/* The CDROM is fairly slow, so we need a little extra time */
/* In fact, it is very slow if it has to spin up first */
#define IOCTL_TIMEOUT 3000

static void sr_ioctl_done(Scsi_Cmnd * SCpnt)
{
    struct request * req;
    
    req = &SCpnt->request;
    req->rq_status = RQ_SCSI_DONE; /* Busy, but indicate request done */
    
    if (req->sem != NULL) {
	up(req->sem);
    }
}

/* We do our own retries because we want to know what the specific
   error code is.  Normally the UNIT_ATTENTION code will automatically
   clear after one error */

static int do_ioctl(int target, unsigned char * sr_cmd, void * buffer, unsigned buflength)
{
    Scsi_Cmnd * SCpnt;
    int result;

    SCpnt = allocate_device(NULL, scsi_CDs[target].device, 1);
    {
	struct semaphore sem = MUTEX_LOCKED;
	SCpnt->request.sem = &sem;
	scsi_do_cmd(SCpnt,
		    (void *) sr_cmd, buffer, buflength, sr_ioctl_done, 
		    IOCTL_TIMEOUT, IOCTL_RETRIES);
	down(&sem);
    }
    
    result = SCpnt->result;
    
    /* Minimal error checking.  Ignore cases we know about, and report the rest. */
    if(driver_byte(result) != 0)
	switch(SCpnt->sense_buffer[2] & 0xf) {
	case UNIT_ATTENTION:
	    scsi_CDs[target].device->changed = 1;
	    printk("Disc change detected.\n");
	    break;
	case NOT_READY: /* This happens if there is no disc in drive */
	    printk("CDROM not ready.  Make sure there is a disc in the drive.\n");
	    break;
	case ILLEGAL_REQUEST:
	    printk("CDROM (ioctl) reports ILLEGAL REQUEST.\n");
	    break;
	default:
	    printk("SCSI CD error: host %d id %d lun %d return code = %03x\n", 
		   scsi_CDs[target].device->host->host_no, 
		   scsi_CDs[target].device->id,
		   scsi_CDs[target].device->lun,
		   result);
	    printk("\tSense class %x, sense error %x, extended sense %x\n",
		   sense_class(SCpnt->sense_buffer[0]), 
		   sense_error(SCpnt->sense_buffer[0]),
		   SCpnt->sense_buffer[2] & 0xf);
	    
	};
    
    result = SCpnt->result;
    SCpnt->request.rq_status = RQ_INACTIVE; /* Deallocate */
    wake_up(&SCpnt->device->device_wait);
    /* Wake up a process waiting for device*/
    return result;
}

int sr_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
    u_char  sr_cmd[10];
    
    kdev_t dev = inode->i_rdev;
    int result, target, err;
    
    target = MINOR(dev);
    
    if (target >= sr_template.nr_dev ||
	!scsi_CDs[target].device) return -ENXIO;
    
    switch (cmd) 
    {
	/* Sun-compatible */
    case CDROMPAUSE:
	
	sr_cmd[0] = SCMD_PAUSE_RESUME;
	sr_cmd[1] = scsi_CDs[target].device->lun << 5;
	sr_cmd[2] = sr_cmd[3] = sr_cmd[4] = 0;
	sr_cmd[5] = sr_cmd[6] = sr_cmd[7] = 0;
	sr_cmd[8] = 0;
	sr_cmd[9] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	return result;
	
    case CDROMRESUME:
	
	sr_cmd[0] = SCMD_PAUSE_RESUME;
	sr_cmd[1] = scsi_CDs[target].device->lun << 5;
	sr_cmd[2] = sr_cmd[3] = sr_cmd[4] = 0;
	sr_cmd[5] = sr_cmd[6] = sr_cmd[7] = 0;
	sr_cmd[8] = 1;
	sr_cmd[9] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	
	return result;
	
    case CDROMPLAYMSF:
    {
	struct cdrom_msf msf;

        err = verify_area (VERIFY_READ, (void *) arg, sizeof (msf));
        if (err) return err;

	memcpy_fromfs(&msf, (void *) arg, sizeof(msf));
	
	sr_cmd[0] = SCMD_PLAYAUDIO_MSF;
	sr_cmd[1] = scsi_CDs[target].device->lun << 5;
	sr_cmd[2] = 0;
	sr_cmd[3] = msf.cdmsf_min0;
	sr_cmd[4] = msf.cdmsf_sec0;
	sr_cmd[5] = msf.cdmsf_frame0;
	sr_cmd[6] = msf.cdmsf_min1;
	sr_cmd[7] = msf.cdmsf_sec1;
	sr_cmd[8] = msf.cdmsf_frame1;
	sr_cmd[9] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	return result;
    }

    case CDROMPLAYBLK:
    {
	struct cdrom_blk blk;

        err = verify_area (VERIFY_READ, (void *) arg, sizeof (blk));
        if (err) return err;

	memcpy_fromfs(&blk, (void *) arg, sizeof(blk));
	
	sr_cmd[0] = SCMD_PLAYAUDIO10;
	sr_cmd[1] = scsi_CDs[target].device->lun << 5;
	sr_cmd[2] = blk.from >> 24;
	sr_cmd[3] = blk.from >> 16;
	sr_cmd[4] = blk.from >> 8;
	sr_cmd[5] = blk.from;
	sr_cmd[6] = 0;
	sr_cmd[7] = blk.len >> 8;
	sr_cmd[8] = blk.len;
	sr_cmd[9] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	return result;
    }
		
    case CDROMPLAYTRKIND:
    {
	struct cdrom_ti ti;

        err = verify_area (VERIFY_READ, (void *) arg, sizeof (ti));
        if (err) return err;

	memcpy_fromfs(&ti, (void *) arg, sizeof(ti));
	
	sr_cmd[0] = SCMD_PLAYAUDIO_TI;
	sr_cmd[1] = scsi_CDs[target].device->lun << 5;
	sr_cmd[2] = 0;
	sr_cmd[3] = 0;
	sr_cmd[4] = ti.cdti_trk0;
	sr_cmd[5] = ti.cdti_ind0;
	sr_cmd[6] = 0;
	sr_cmd[7] = ti.cdti_trk1;
	sr_cmd[8] = ti.cdti_ind1;
	sr_cmd[9] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	
	return result;
    }
	
    case CDROMREADTOCHDR:
    {
	struct cdrom_tochdr tochdr;
	char * buffer;
	
	sr_cmd[0] = SCMD_READ_TOC;
	sr_cmd[1] = ((scsi_CDs[target].device->lun) << 5) | 0x02;    /* MSF format */
	sr_cmd[2] = sr_cmd[3] = sr_cmd[4] = sr_cmd[5] = 0;
	sr_cmd[6] = 0;
	sr_cmd[7] = 0;              /* MSB of length (12) */
	sr_cmd[8] = 12;             /* LSB of length */
	sr_cmd[9] = 0;
	
	buffer = (unsigned char *) scsi_malloc(512);
	if(!buffer) return -ENOMEM;
	
	result = do_ioctl(target, sr_cmd, buffer, 12);
	
	tochdr.cdth_trk0 = buffer[2];
	tochdr.cdth_trk1 = buffer[3];
	
	scsi_free(buffer, 512);
	
	err = verify_area (VERIFY_WRITE, (void *) arg, sizeof (struct cdrom_tochdr));
	if (err)
	    return err;
	memcpy_tofs ((void *) arg, &tochdr, sizeof (struct cdrom_tochdr));
	
	return result;
    }
	
    case CDROMREADTOCENTRY:
    {
	struct cdrom_tocentry tocentry;
	char * buffer;
	
        err = verify_area (VERIFY_READ, (void *) arg, sizeof (struct cdrom_tocentry));
        if (err) return err;

	memcpy_fromfs (&tocentry, (void *) arg, sizeof (struct cdrom_tocentry));
	
	sr_cmd[0] = SCMD_READ_TOC;
	sr_cmd[1] = ((scsi_CDs[target].device->lun) << 5) | 0x02;    /* MSF format */
	sr_cmd[2] = sr_cmd[3] = sr_cmd[4] = sr_cmd[5] = 0;
	sr_cmd[6] = tocentry.cdte_track;
	sr_cmd[7] = 0;             /* MSB of length (12)  */
	sr_cmd[8] = 12;            /* LSB of length */
	sr_cmd[9] = 0;
	
	buffer = (unsigned char *) scsi_malloc(512);
	if(!buffer) return -ENOMEM;
	
	result = do_ioctl (target, sr_cmd, buffer, 12);
	
	if (tocentry.cdte_format == CDROM_MSF) {
	    tocentry.cdte_addr.msf.minute = buffer[9];
	    tocentry.cdte_addr.msf.second = buffer[10];
	    tocentry.cdte_addr.msf.frame = buffer[11];
	    tocentry.cdte_ctrl = buffer[5] & 0xf;
	}
	else
	    tocentry.cdte_addr.lba = (int) buffer[0];
	
	scsi_free(buffer, 512);
	
	err = verify_area (VERIFY_WRITE, (void *) arg, sizeof (struct cdrom_tocentry));
	if (err)
	    return err;
	memcpy_tofs ((void *) arg, &tocentry, sizeof (struct cdrom_tocentry));
	
	return result;
    }
	
    case CDROMSTOP:
	sr_cmd[0] = START_STOP;
	sr_cmd[1] = ((scsi_CDs[target].device->lun) << 5) | 1;
	sr_cmd[2] = sr_cmd[3] = sr_cmd[5] = 0;
	sr_cmd[4] = 0;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	return result;
	
    case CDROMSTART:
	sr_cmd[0] = START_STOP;
	sr_cmd[1] = ((scsi_CDs[target].device->lun) << 5) | 1;
	sr_cmd[2] = sr_cmd[3] = sr_cmd[5] = 0;
	sr_cmd[4] = 1;
	
	result = do_ioctl(target, sr_cmd, NULL, 255);
	return result;
	
    case CDROMEJECT:
        /*
         * Allow 0 for access count for auto-eject feature.
         */
	if (scsi_CDs[target].device -> access_count > 1)
	    return -EBUSY;
	
	sr_ioctl (inode, NULL, SCSI_IOCTL_DOORUNLOCK, 0);
	sr_cmd[0] = START_STOP;
	sr_cmd[1] = ((scsi_CDs[target].device -> lun) << 5) | 1;
	sr_cmd[2] = sr_cmd[3] = sr_cmd[5] = 0;
	sr_cmd[4] = 0x02;
	
	if (!(result = do_ioctl(target, sr_cmd, NULL, 255)))
	    scsi_CDs[target].device -> changed = 1;
	
	return result;
	
    case CDROMEJECT_SW:
        scsi_CDs[target].auto_eject = !!arg;
        return 0;

    case CDROMVOLCTRL:
    {
	char * buffer, * mask;
	struct cdrom_volctrl volctrl;
	
        err = verify_area (VERIFY_READ, (void *) arg, sizeof (struct cdrom_volctrl));
        if (err) return err;

	memcpy_fromfs (&volctrl, (void *) arg, sizeof (struct cdrom_volctrl));
	
	/* First we get the current params so we can just twiddle the volume */
	
	sr_cmd[0] = MODE_SENSE;
	sr_cmd[1] = (scsi_CDs[target].device -> lun) << 5;
	sr_cmd[2] = 0xe;    /* Want mode page 0xe, CDROM audio params */
	sr_cmd[3] = 0;
	sr_cmd[4] = 28;
	sr_cmd[5] = 0;
	
	buffer = (unsigned char *) scsi_malloc(512);
	if(!buffer) return -ENOMEM;
	
	if ((result = do_ioctl (target, sr_cmd, buffer, 28))) {
	    printk ("Hosed while obtaining audio mode page\n");
	    scsi_free(buffer, 512);
	    return result;
	}
	
	sr_cmd[0] = MODE_SENSE;
	sr_cmd[1] = (scsi_CDs[target].device -> lun) << 5;
	sr_cmd[2] = 0x4e;   /* Want the mask for mode page 0xe */
	sr_cmd[3] = 0;
	sr_cmd[4] = 28;
	sr_cmd[5] = 0;
	
	mask = (unsigned char *) scsi_malloc(512);
	if(!mask) {
	    scsi_free(buffer, 512);
	    return -ENOMEM;
	};

	if ((result = do_ioctl (target, sr_cmd, mask, 28))) {
	    printk ("Hosed while obtaining mask for audio mode page\n");
	    scsi_free(buffer, 512);
	    scsi_free(mask, 512);
	    return result;
	}
	
	/* Now mask and substitute our own volume and reuse the rest */
	buffer[0] = 0;  /* Clear reserved field */
	
	buffer[21] = volctrl.channel0 & mask[21];
	buffer[23] = volctrl.channel1 & mask[23];
	buffer[25] = volctrl.channel2 & mask[25];
	buffer[27] = volctrl.channel3 & mask[27];
	
	sr_cmd[0] = MODE_SELECT;
	sr_cmd[1] = ((scsi_CDs[target].device -> lun) << 5) | 0x10;    /* Params are SCSI-2 */
	sr_cmd[2] = sr_cmd[3] = 0;
	sr_cmd[4] = 28;
	sr_cmd[5] = 0;
	
	result = do_ioctl (target, sr_cmd, buffer, 28);
	scsi_free(buffer, 512);
	scsi_free(mask, 512);
	return result;
    }
	
    case CDROMSUBCHNL:
    {
	struct cdrom_subchnl subchnl;
	char * buffer;
	
	sr_cmd[0] = SCMD_READ_SUBCHANNEL;
	sr_cmd[1] = ((scsi_CDs[target].device->lun) << 5) | 0x02;    /* MSF format */
	sr_cmd[2] = 0x40;    /* I do want the subchannel info */
	sr_cmd[3] = 0x01;    /* Give me current position info */
	sr_cmd[4] = sr_cmd[5] = 0;
	sr_cmd[6] = 0;
	sr_cmd[7] = 0;
	sr_cmd[8] = 16;
	sr_cmd[9] = 0;
	
	buffer = (unsigned char*) scsi_malloc(512);
	if(!buffer) return -ENOMEM;
	
	result = do_ioctl(target, sr_cmd, buffer, 16);
	
	subchnl.cdsc_audiostatus = buffer[1];
	subchnl.cdsc_format = CDROM_MSF;
	subchnl.cdsc_ctrl = buffer[5] & 0xf;
	subchnl.cdsc_trk = buffer[6];
	subchnl.cdsc_ind = buffer[7];
	
	subchnl.cdsc_reladdr.msf.minute = buffer[13];
	subchnl.cdsc_reladdr.msf.second = buffer[14];
	subchnl.cdsc_reladdr.msf.frame = buffer[15];
	subchnl.cdsc_absaddr.msf.minute = buffer[9];
	subchnl.cdsc_absaddr.msf.second = buffer[10];
	subchnl.cdsc_absaddr.msf.frame = buffer[11];
	
	scsi_free(buffer, 512);
	
	err = verify_area (VERIFY_WRITE, (void *) arg, sizeof (struct cdrom_subchnl));
	if (err)
	    return err;
	memcpy_tofs ((void *) arg, &subchnl, sizeof (struct cdrom_subchnl));
	return result;
    }
	
    case CDROMREADMODE2:
	return -EINVAL;
    case CDROMREADMODE1:
	return -EINVAL;
	
	/* block-copy from ../block/sbpcd.c with some adjustments... */
    case CDROMMULTISESSION: /* tell start-of-last-session to user */
    {
	struct cdrom_multisession  ms_info;
	long                       lba;
	
	err = verify_area(VERIFY_READ, (void *) arg,
			  sizeof(struct cdrom_multisession));
	if (err) return (err);
	
	memcpy_fromfs(&ms_info, (void *) arg, sizeof(struct cdrom_multisession));
	
	if (ms_info.addr_format==CDROM_MSF) { /* MSF-bin requested */
	    lba = scsi_CDs[target].mpcd_sector+CD_BLOCK_OFFSET;
	    ms_info.addr.msf.minute = lba / (CD_SECS*CD_FRAMES);
	    lba %= CD_SECS*CD_FRAMES;
	    ms_info.addr.msf.second = lba / CD_FRAMES;
	    ms_info.addr.msf.frame  = lba % CD_FRAMES;
	} else if (ms_info.addr_format==CDROM_LBA) /* lba requested */
	    ms_info.addr.lba=scsi_CDs[target].mpcd_sector;
	else return (-EINVAL);
	
	ms_info.xa_flag=scsi_CDs[target].xa_flags & 0x01;
	
	err=verify_area(VERIFY_WRITE,(void *) arg,
			sizeof(struct cdrom_multisession));
	if (err) return (err);
	
	memcpy_tofs((void *) arg, &ms_info, sizeof(struct cdrom_multisession));
	return (0);
    }
	
    case BLKRASET:
	if(!suser())  return -EACCES;
	if(!(inode->i_rdev)) return -EINVAL;
	if(arg > 0xff) return -EINVAL;
	read_ahead[MAJOR(inode->i_rdev)] = arg;
	return 0;
	RO_IOCTLS(dev,arg);
    default:
	return scsi_ioctl(scsi_CDs[target].device,cmd,(void *) arg);
    }
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
