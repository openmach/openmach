/*
 *  sr.c Copyright (C) 1992 David Giller
 *	     Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *
 *  adapted from:
 *	sd.c Copyright (C) 1992 Drew Eckhardt 
 *	Linux scsi disk driver by
 *		Drew Eckhardt <drew@colorado.edu>
 *
 *      Modified by Eric Youngdale ericy@cais.com to
 *      add scatter-gather, multiple outstanding request, and other
 *      enhancements.
 *
 *	    Modified by Eric Youngdale eric@aib.com to support loadable
 *	    low-level scsi drivers.
 *
 *	 Modified by Thomas Quinot thomas@melchior.cuivre.fdn.fr to
 *	 provide auto-eject.
 *
 */

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/cdrom.h>
#include <asm/system.h>

#define MAJOR_NR SCSI_CDROM_MAJOR
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sr.h"
#include "scsi_ioctl.h"   /* For the door lock/unlock commands */
#include "constants.h"

#define MAX_RETRIES 3
#define SR_TIMEOUT (150 * HZ)

static int sr_init(void);
static void sr_finish(void);
static int sr_attach(Scsi_Device *);
static int sr_detect(Scsi_Device *);
static void sr_detach(Scsi_Device *);

struct Scsi_Device_Template sr_template = {NULL, "cdrom", "sr", NULL, TYPE_ROM, 
					       SCSI_CDROM_MAJOR, 0, 0, 0, 1,
					       sr_detect, sr_init,
					       sr_finish, sr_attach, sr_detach};

Scsi_CD * scsi_CDs = NULL;
static int * sr_sizes;

static int * sr_blocksizes;

static int sr_open(struct inode *, struct file *);
static void get_sectorsize(int);

extern int sr_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

void requeue_sr_request (Scsi_Cmnd * SCpnt);
static int check_cdrom_media_change(kdev_t);

static void sr_release(struct inode * inode, struct file * file)
{
	sync_dev(inode->i_rdev);
	if(! --scsi_CDs[MINOR(inode->i_rdev)].device->access_count)
	{
	    sr_ioctl(inode, NULL, SCSI_IOCTL_DOORUNLOCK, 0);
	    if (scsi_CDs[MINOR(inode->i_rdev)].auto_eject)
		sr_ioctl(inode, NULL, CDROMEJECT, 0);
	}
	if (scsi_CDs[MINOR(inode->i_rdev)].device->host->hostt->usage_count)
	    (*scsi_CDs[MINOR(inode->i_rdev)].device->host->hostt->usage_count)--;
	if(sr_template.usage_count) (*sr_template.usage_count)--;
}

static struct file_operations sr_fops = 
{
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* select */
	sr_ioctl,		/* ioctl */
	NULL,			/* mmap */
	sr_open,       	/* special open code */
	sr_release,		/* release */
	NULL,			/* fsync */
	NULL,			/* fasync */
	check_cdrom_media_change,  /* Disk change */
	NULL			/* revalidate */
};

/*
 * This function checks to see if the media has been changed in the
 * CDROM drive.  It is possible that we have already sensed a change,
 * or the drive may have sensed one and not yet reported it.  We must
 * be ready for either case. This function always reports the current
 * value of the changed bit.  If flag is 0, then the changed bit is reset.
 * This function could be done as an ioctl, but we would need to have
 * an inode for that to work, and we do not always have one.
 */

int check_cdrom_media_change(kdev_t full_dev){
	int retval, target;
	struct inode inode;
	int flag = 0;
    
	target =  MINOR(full_dev);
    
	if (target >= sr_template.nr_dev) {
		printk("CD-ROM request error: invalid device.\n");
		return 0;
	};
    
	inode.i_rdev = full_dev;  /* This is all we really need here */
	retval = sr_ioctl(&inode, NULL, SCSI_IOCTL_TEST_UNIT_READY, 0);
    
	if(retval){ /* Unable to test, unit probably not ready.  This usually
		 * means there is no disc in the drive.  Mark as changed,
		 * and we will figure it out later once the drive is
		 * available again.  */
	
	scsi_CDs[target].device->changed = 1;
	return 1; /* This will force a flush, if called from
		   * check_disk_change */
	};
    
	retval = scsi_CDs[target].device->changed;
	if(!flag) {
	scsi_CDs[target].device->changed = 0;
	/* If the disk changed, the capacity will now be different,
	 * so we force a re-read of this information */
	if (retval) scsi_CDs[target].needs_sector_size = 1;
	};
	return retval;
}

/*
 * rw_intr is the interrupt routine for the device driver.  It will be notified on the 
 * end of a SCSI read / write, and will take on of several actions based on success or failure.
 */

static void rw_intr (Scsi_Cmnd * SCpnt)
{
	int result = SCpnt->result;
	int this_count = SCpnt->this_count;
    
#ifdef DEBUG
	printk("sr.c done: %x %x\n",result, SCpnt->request.bh->b_data);
#endif
	if (!result)
    { /* No error */
	if (SCpnt->use_sg == 0) {
		    if (SCpnt->buffer != SCpnt->request.buffer)
	    {
		int offset;
		offset = (SCpnt->request.sector % 4) << 9;
		memcpy((char *)SCpnt->request.buffer, 
		       (char *)SCpnt->buffer + offset, 
		       this_count << 9);
		/* Even though we are not using scatter-gather, we look
		 * ahead and see if there is a linked request for the
		 * other half of this buffer.  If there is, then satisfy
		 * it. */
		if((offset == 0) && this_count == 2 &&
		   SCpnt->request.nr_sectors > this_count && 
		   SCpnt->request.bh &&
		   SCpnt->request.bh->b_reqnext &&
		   SCpnt->request.bh->b_reqnext->b_size == 1024) {
		    memcpy((char *)SCpnt->request.bh->b_reqnext->b_data, 
			   (char *)SCpnt->buffer + 1024, 
			   1024);
		    this_count += 2;
		};
		
		scsi_free(SCpnt->buffer, 2048);
	    }
	} else {
		    struct scatterlist * sgpnt;
		    int i;
		    sgpnt = (struct scatterlist *) SCpnt->buffer;
		    for(i=0; i<SCpnt->use_sg; i++) {
		if (sgpnt[i].alt_address) {
		    if (sgpnt[i].alt_address != sgpnt[i].address) {
			memcpy(sgpnt[i].alt_address, sgpnt[i].address, sgpnt[i].length);
		    };
		    scsi_free(sgpnt[i].address, sgpnt[i].length);
		};
		    };
		    scsi_free(SCpnt->buffer, SCpnt->sglist_len);  /* Free list of scatter-gather pointers */
		    if(SCpnt->request.sector % 4) this_count -= 2;
	    /* See   if there is a padding record at the end that needs to be removed */
		    if(this_count > SCpnt->request.nr_sectors)
		this_count -= 2;
	};
	
#ifdef DEBUG
		printk("(%x %x %x) ",SCpnt->request.bh, SCpnt->request.nr_sectors, 
		       this_count);
#endif
		if (SCpnt->request.nr_sectors > this_count)
	{	 
			SCpnt->request.errors = 0;
			if (!SCpnt->request.bh)
			    panic("sr.c: linked page request (%lx %x)",
		      SCpnt->request.sector, this_count);
	}
	
	SCpnt = end_scsi_request(SCpnt, 1, this_count);  /* All done */
	requeue_sr_request(SCpnt);
	return;
    } /* Normal completion */
    
	/* We only come through here if we have an error of some kind */
    
    /* Free up any indirection buffers we allocated for DMA purposes. */
	if (SCpnt->use_sg) {
	struct scatterlist * sgpnt;
	int i;
	sgpnt = (struct scatterlist *) SCpnt->buffer;
	for(i=0; i<SCpnt->use_sg; i++) {
	    if (sgpnt[i].alt_address) {
		scsi_free(sgpnt[i].address, sgpnt[i].length);
	    };
	};
	scsi_free(SCpnt->buffer, SCpnt->sglist_len);  /* Free list of scatter-gather pointers */
	} else {
	if (SCpnt->buffer != SCpnt->request.buffer)
	    scsi_free(SCpnt->buffer, SCpnt->bufflen);
	};
    
	if (driver_byte(result) != 0) {
		if ((SCpnt->sense_buffer[0] & 0x7f) == 0x70) {
			if ((SCpnt->sense_buffer[2] & 0xf) == UNIT_ATTENTION) {
				/* detected disc change.  set a bit and quietly refuse 
				 * further access.	*/
		
				scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].device->changed = 1;
				SCpnt = end_scsi_request(SCpnt, 0, this_count);
				requeue_sr_request(SCpnt);
				return;
			}
		}
	    
		if (SCpnt->sense_buffer[2] == ILLEGAL_REQUEST) {
			printk("CD-ROM error: ");
			print_sense("sr", SCpnt);
			printk("command was: ");
			print_command(SCpnt->cmnd);
			if (scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].ten) {
				scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].ten = 0;
				requeue_sr_request(SCpnt);
				result = 0;
				return;
			} else {
				SCpnt = end_scsi_request(SCpnt, 0, this_count);
				requeue_sr_request(SCpnt); /* Do next request */
				return;
			}
	    
		}
	
		if (SCpnt->sense_buffer[2] == NOT_READY) {
			printk("CDROM not ready.  Make sure you have a disc in the drive.\n");
			SCpnt = end_scsi_request(SCpnt, 0, this_count);
			requeue_sr_request(SCpnt); /* Do next request */
			return;
		};
    }
	
	/* We only get this far if we have an error we have not recognized */
	if(result) {
	printk("SCSI CD error : host %d id %d lun %d return code = %03x\n", 
	       scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].device->host->host_no, 
	       scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].device->id,
	       scsi_CDs[DEVICE_NR(SCpnt->request.rq_dev)].device->lun,
	       result);
	    
	if (status_byte(result) == CHECK_CONDITION)
	    print_sense("sr", SCpnt);
	
	SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.current_nr_sectors);
	requeue_sr_request(SCpnt);
    }
}

/*
 * Here I tried to implement better support for PhotoCD's.
 * 
 * Much of this has do be done with vendor-specific SCSI-commands.
 * So I have to complete it step by step. Useful information is welcome.
 *
 * Actually works:
 *   - NEC:     Detection and support of multisession CD's. Special handling
 *              for XA-disks is not necessary.
 *     
 *   - TOSHIBA: setting density is done here now, mounting PhotoCD's should
 *              work now without running the program "set_density"
 *              Multisession CD's are supported too.
 *
 *   kraxel@cs.tu-berlin.de (Gerd Knorr)
 */
/*
 * 19950704 operator@melchior.cuivre.fdn.fr (Thomas Quinot)
 *
 *   - SONY:	Same as Nec.
 *
 *   - PIONEER: works with SONY code
 */

static void sr_photocd(struct inode *inode)
{
    unsigned long   sector,min,sec,frame;
    unsigned char   buf[40];    /* the buffer for the ioctl */
    unsigned char   *cmd;       /* the scsi-command */
    unsigned char   *send;      /* the data we send to the drive ... */
    unsigned char   *rec;       /* ... and get back */
    int             rc,is_xa,no_multi;
    
    if (scsi_CDs[MINOR(inode->i_rdev)].xa_flags & 0x02) {
#ifdef DEBUG
	printk("sr_photocd: CDROM and/or the driver does not support multisession CD's");
#endif
	return;
    }
    
    if (!suser()) {
	/* I'm not the superuser, so SCSI_IOCTL_SEND_COMMAND isn't allowed for me.
	 * That's why mpcd_sector will be initialized with zero, because I'm not
	 * able to get the right value. Necessary only if access_count is 1, else
	 * no disk change happened since the last call of this function and we can
	 * keep the old value.
	 */
	if (1 == scsi_CDs[MINOR(inode->i_rdev)].device->access_count) {
	    scsi_CDs[MINOR(inode->i_rdev)].mpcd_sector = 0;
	    scsi_CDs[MINOR(inode->i_rdev)].xa_flags &= ~0x01;
	}
	return;
    }
    
    sector   = 0;
    is_xa    = 0;
    no_multi = 0;
    cmd = rec = &buf[8];
    
    switch(scsi_CDs[MINOR(inode->i_rdev)].device->manufacturer) {
	
    case SCSI_MAN_NEC:
#ifdef DEBUG
	printk("sr_photocd: use NEC code\n");
#endif
	memset(buf,0,40);
	*((unsigned long*)buf)   = 0x0;   /* we send nothing...     */
	*((unsigned long*)buf+1) = 0x16;  /* and receive 0x16 bytes */
	cmd[0] = 0xde;
	cmd[1] = 0x03;
	cmd[2] = 0xb0;
	rc = kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
			   SCSI_IOCTL_SEND_COMMAND, buf);
	if (rc != 0) {
	    printk("sr_photocd: ioctl error (NEC): 0x%x\n",rc);
	    break;
	}
	if (rec[14] != 0 && rec[14] != 0xb0) {
	    printk("sr_photocd: (NEC) Hmm, seems the CDROM doesn't support multisession CD's\n");
	    no_multi = 1;
	    break;
	}
	min   = (unsigned long) rec[15]/16*10 + (unsigned long) rec[15]%16;
	sec   = (unsigned long) rec[16]/16*10 + (unsigned long) rec[16]%16;
	frame = (unsigned long) rec[17]/16*10 + (unsigned long) rec[17]%16;
	sector = min*CD_SECS*CD_FRAMES + sec*CD_FRAMES + frame;
	is_xa  = (rec[14] == 0xb0);
#ifdef DEBUG
	if (sector) {
	    printk("sr_photocd: multisession CD detected. start: %lu\n",sector);
	}
#endif
	break;
	
    case SCSI_MAN_TOSHIBA:
#ifdef DEBUG
	printk("sr_photocd: use TOSHIBA code\n");
#endif
	
	/* we request some disc information (is it a XA-CD ?,
	 * where starts the last session ?) */
	memset(buf,0,40);
	*((unsigned long*)buf)   = 0;
	*((unsigned long*)buf+1) = 4;  /* we receive 4 bytes from the drive */
	cmd[0] = 0xc7;
	cmd[1] = 3;
	rc = kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
			       SCSI_IOCTL_SEND_COMMAND, buf);
	if (rc != 0) {
	    if (rc == 0x28000002) {
		/* Got a "not ready" - error. No chance to find out if this is
		 * because there is no CD in the drive or because the drive
		 * don't knows multisession CD's. So I need to do an extra check... */
		if (kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
				      SCSI_IOCTL_TEST_UNIT_READY, NULL)) {
		    printk("sr_photocd: drive not ready\n");
		} else {
		    printk("sr_photocd: (TOSHIBA) Hmm, seems the CDROM doesn't support multisession CD's\n");
		    no_multi = 1;
		}
	    } else
		printk("sr_photocd: ioctl error (TOSHIBA #1): 0x%x\n",rc);
	    break; /* if the first ioctl fails, we don't call the second one */
	}
	is_xa  = (rec[0] == 0x20);
	min    = (unsigned long) rec[1]/16*10 + (unsigned long) rec[1]%16;
	sec    = (unsigned long) rec[2]/16*10 + (unsigned long) rec[2]%16;
	frame  = (unsigned long) rec[3]/16*10 + (unsigned long) rec[3]%16;
	sector = min*CD_SECS*CD_FRAMES + sec*CD_FRAMES + frame;
	if (sector) {
	    sector -= CD_BLOCK_OFFSET;
#ifdef DEBUG
	    printk("sr_photocd: multisession CD detected: start: %lu\n",sector);
#endif
	}
	
	/* now we do a get_density... */
	memset(buf,0,40);
	*((unsigned long*)buf)   = 0;
	*((unsigned long*)buf+1) = 12;
	cmd[0] = 0x1a;
	cmd[2] = 1;
	cmd[4] = 12;
	rc = kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
			       SCSI_IOCTL_SEND_COMMAND, buf);
	if (rc != 0) {
	    printk("sr_photocd: ioctl error (TOSHIBA #2): 0x%x\n",rc);
	    break;
	}
#ifdef DEBUG
	printk("sr_photocd: get_density: 0x%x\n",rec[4]);
#endif
	
	/* ...and only if necessary a set_density */
	if ((rec[4] != 0x81 && is_xa) || (rec[4] != 0 && !is_xa)) {
#ifdef DEBUG
	    printk("sr_photocd: doing set_density\n");
#endif
	    memset(buf,0,40);
	    *((unsigned long*)buf)   = 12;  /* sending 12 bytes... */
	    *((unsigned long*)buf+1) = 0;
	    cmd[0] = 0x15;
	    cmd[1] = (1 << 4);
	    cmd[4] = 12;
	    send = &cmd[6];                 /* this is a 6-Byte command          */
	    send[ 3] = 0x08;                /* the data for the command          */
	    send[ 4] = (is_xa) ? 0x81 : 0;  /* density 0x81 for XA-CD's, 0 else  */
	    send[10] = 0x08;
	    rc = kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
				   SCSI_IOCTL_SEND_COMMAND, buf);
	    if (rc != 0) {
		printk("sr_photocd: ioctl error (TOSHIBA #3): 0x%x\n",rc);
	    }
	    /* The set_density command may have changed the sector size or capacity. */
	    scsi_CDs[MINOR(inode->i_rdev)].needs_sector_size = 1;
	}
	break;

    case SCSI_MAN_SONY: /* Thomas QUINOT <thomas@melchior.cuivre.fdn.fr> */
    case SCSI_MAN_PIONEER:
#ifdef DEBUG
	printk("sr_photocd: use SONY/PIONEER code\n");
#endif
	memset(buf,0,40);
	*((unsigned long*)buf)   = 0x0;   /* we send nothing...     */
	*((unsigned long*)buf+1) = 0x0c;  /* and receive 0x0c bytes */
	cmd[0] = 0x43; /* Read TOC */
	cmd[8] = 0x0c;
	cmd[9] = 0x40;
	rc = kernel_scsi_ioctl(scsi_CDs[MINOR(inode->i_rdev)].device,
			       SCSI_IOCTL_SEND_COMMAND, buf);
	
	if (rc != 0) {
	    printk("sr_photocd: ioctl error (SONY): 0x%x\n",rc);
	    break;
	}
	if ((rec[0] << 8) + rec[1] != 0x0a) {
	    printk("sr_photocd: (SONY) Hmm, seems the CDROM doesn't support multisession CD's\n");
	    no_multi = 1;
	    break;
	}
	sector = rec[11] + (rec[10] << 8) + (rec[9] << 16) + (rec[8] << 24);
	is_xa = !!sector;
#ifdef DEBUG
	if (sector)
	    printk ("sr_photocd: multisession CD detected. start: %lu\n",sector);
#endif
	break;
		
    case SCSI_MAN_NEC_OLDCDR:
    case SCSI_MAN_UNKNOWN:
    default:
	sector = 0;
	no_multi = 1;
	break; }
    
    scsi_CDs[MINOR(inode->i_rdev)].mpcd_sector = sector;
    if (is_xa)
	scsi_CDs[MINOR(inode->i_rdev)].xa_flags |= 0x01;
    else
	scsi_CDs[MINOR(inode->i_rdev)].xa_flags &= ~0x01;
    if (no_multi)
	scsi_CDs[MINOR(inode->i_rdev)].xa_flags |= 0x02;
    return;
}

static int sr_open(struct inode * inode, struct file * filp)
{
	if(MINOR(inode->i_rdev) >= sr_template.nr_dev || 
	   !scsi_CDs[MINOR(inode->i_rdev)].device) return -ENXIO;   /* No such device */
    
	if (filp->f_mode & 2)  
	    return -EROFS;
    
    check_disk_change(inode->i_rdev);
    
	if(!scsi_CDs[MINOR(inode->i_rdev)].device->access_count++)
	sr_ioctl(inode, NULL, SCSI_IOCTL_DOORLOCK, 0);
	if (scsi_CDs[MINOR(inode->i_rdev)].device->host->hostt->usage_count)
	(*scsi_CDs[MINOR(inode->i_rdev)].device->host->hostt->usage_count)++;
	if(sr_template.usage_count) (*sr_template.usage_count)++;
    
	sr_photocd(inode);
    
	/* If this device did not have media in the drive at boot time, then
	 * we would have been unable to get the sector size.  Check to see if
	 * this is the case, and try again.
	 */
    
	if(scsi_CDs[MINOR(inode->i_rdev)].needs_sector_size)
	get_sectorsize(MINOR(inode->i_rdev));
    
	return 0;
}


/*
 * do_sr_request() is the request handler function for the sr driver.
 * Its function in life is to take block device requests, and
 * translate them to SCSI commands.  
 */

static void do_sr_request (void)
{
    Scsi_Cmnd * SCpnt = NULL;
    struct request * req = NULL;
    Scsi_Device * SDev;
    unsigned long flags;
    int flag = 0;
    
    while (1==1){
	save_flags(flags);
	cli();
	if (CURRENT != NULL && CURRENT->rq_status == RQ_INACTIVE) {
	    restore_flags(flags);
	    return;
	};
	
	INIT_SCSI_REQUEST;
 
	SDev = scsi_CDs[DEVICE_NR(CURRENT->rq_dev)].device;
	
	/*
	 * I am not sure where the best place to do this is.  We need
	 * to hook in a place where we are likely to come if in user
	 * space.
	 */
	if( SDev->was_reset )
	{
 	    /*
 	     * We need to relock the door, but we might
 	     * be in an interrupt handler.  Only do this
 	     * from user space, since we do not want to
 	     * sleep from an interrupt.
 	     */
 	    if( SDev->removable && !intr_count )
 	    {
		scsi_ioctl(SDev, SCSI_IOCTL_DOORLOCK, 0);
 	    }
 	    SDev->was_reset = 0;
	}
	
	if (flag++ == 0)
	    SCpnt = allocate_device(&CURRENT,
				    scsi_CDs[DEVICE_NR(CURRENT->rq_dev)].device, 0); 
	else SCpnt = NULL;
	restore_flags(flags);
	
	/* This is a performance enhancement.  We dig down into the request list and
	 * try and find a queueable request (i.e. device not busy, and host able to
	 * accept another command.  If we find one, then we queue it. This can
	 * make a big difference on systems with more than one disk drive.  We want
	 * to have the interrupts off when monkeying with the request list, because
	 * otherwise the kernel might try and slip in a request in between somewhere. */
	
	if (!SCpnt && sr_template.nr_dev > 1){
	    struct request *req1;
	    req1 = NULL;
	    save_flags(flags);
	    cli();
	    req = CURRENT;
	    while(req){
		SCpnt = request_queueable(req,
					  scsi_CDs[DEVICE_NR(req->rq_dev)].device);
		if(SCpnt) break;
		req1 = req;
		req = req->next;
	    };
	    if (SCpnt && req->rq_status == RQ_INACTIVE) {
		if (req == CURRENT) 
		    CURRENT = CURRENT->next;
		else
		    req1->next = req->next;
	    };
	    restore_flags(flags);
	};
	
	if (!SCpnt)
	    return; /* Could not find anything to do */
	
	wake_up(&wait_for_request);

	/* Queue command */
	requeue_sr_request(SCpnt);
    };  /* While */
}    

void requeue_sr_request (Scsi_Cmnd * SCpnt)
{
	unsigned int dev, block, realcount;
	unsigned char cmd[10], *buffer, tries;
	int this_count, start, end_rec;
    
	tries = 2;
    
 repeat:
	if(!SCpnt || SCpnt->request.rq_status == RQ_INACTIVE) {
		do_sr_request();
		return;
	}
    
	dev =  MINOR(SCpnt->request.rq_dev);
	block = SCpnt->request.sector;	
	buffer = NULL;
	this_count = 0;
    
	if (dev >= sr_template.nr_dev) {
		/* printk("CD-ROM request error: invalid device.\n");			*/
		SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
		tries = 2;
		goto repeat;
	}
    
	if (!scsi_CDs[dev].use) {
		/* printk("CD-ROM request error: device marked not in use.\n");		*/
		SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
		tries = 2;
		goto repeat;
	}
    
	if (scsi_CDs[dev].device->changed) {
	/* 
	 * quietly refuse to do anything to a changed disc 
	 * until the changed bit has been reset
	 */
		/* printk("CD-ROM has been changed.  Prohibiting further I/O.\n");	*/
		SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
		tries = 2;
		goto repeat;
	}
	
	switch (SCpnt->request.cmd)
    {
    case WRITE: 		
	SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
	goto repeat;
	break;
    case READ : 
	cmd[0] = READ_6;
	break;
    default : 
	panic ("Unknown sr command %d\n", SCpnt->request.cmd);
    }
	
	cmd[1] = (SCpnt->lun << 5) & 0xe0;
    
    /*
     * Now do the grungy work of figuring out which sectors we need, and
     * where in memory we are going to put them.
     * 
     * The variables we need are:
     * 
     * this_count= number of 512 byte sectors being read 
     * block     = starting cdrom sector to read.
     * realcount = # of cdrom sectors to read
     * 
     * The major difference between a scsi disk and a scsi cdrom
     * is that we will always use scatter-gather if we can, because we can
     * work around the fact that the buffer cache has a block size of 1024,
     * and we have 2048 byte sectors.  This code should work for buffers that
     * are any multiple of 512 bytes long.
     */
    
	SCpnt->use_sg = 0;
    
	if (SCpnt->host->sg_tablesize > 0 &&
	    (!need_isa_buffer ||
	 dma_free_sectors >= 10)) {
	struct buffer_head * bh;
	struct scatterlist * sgpnt;
	int count, this_count_max;
	bh = SCpnt->request.bh;
	this_count = 0;
	count = 0;
	this_count_max = (scsi_CDs[dev].ten ? 0xffff : 0xff) << 4;
	/* Calculate how many links we can use.  First see if we need
	 * a padding record at the start */
	this_count = SCpnt->request.sector % 4;
	if(this_count) count++;
	while(bh && count < SCpnt->host->sg_tablesize) {
	    if ((this_count + (bh->b_size >> 9)) > this_count_max) break;
	    this_count += (bh->b_size >> 9);
	    count++;
	    bh = bh->b_reqnext;
	};
	/* Fix up in case of an odd record at the end */
	end_rec = 0;
	if(this_count % 4) {
	    if (count < SCpnt->host->sg_tablesize) {
		count++;
		end_rec = (4 - (this_count % 4)) << 9;
		this_count += 4 - (this_count % 4);
	    } else {
		count--;
		this_count -= (this_count % 4);
	    };
	};
	SCpnt->use_sg = count;  /* Number of chains */
	count = 512;/* scsi_malloc can only allocate in chunks of 512 bytes*/
	while( count < (SCpnt->use_sg * sizeof(struct scatterlist))) 
	    count = count << 1;
	SCpnt->sglist_len = count;
	sgpnt = (struct scatterlist * ) scsi_malloc(count);
	if (!sgpnt) {
	    printk("Warning - running *really* short on DMA buffers\n");
	    SCpnt->use_sg = 0;  /* No memory left - bail out */
	} else {
	    buffer = (unsigned char *) sgpnt;
	    count = 0;
	    bh = SCpnt->request.bh;
	    if(SCpnt->request.sector % 4) {
		sgpnt[count].length = (SCpnt->request.sector % 4) << 9;
		sgpnt[count].address = (char *) scsi_malloc(sgpnt[count].length);
		if(!sgpnt[count].address) panic("SCSI DMA pool exhausted.");
		sgpnt[count].alt_address = sgpnt[count].address; /* Flag to delete
								    if needed */
		count++;
	    };
	    for(bh = SCpnt->request.bh; count < SCpnt->use_sg; 
		count++, bh = bh->b_reqnext) {
		if (bh) { /* Need a placeholder at the end of the record? */
		    sgpnt[count].address = bh->b_data;
		    sgpnt[count].length = bh->b_size;
		    sgpnt[count].alt_address = NULL;
		} else {
		    sgpnt[count].address = (char *) scsi_malloc(end_rec);
		    if(!sgpnt[count].address) panic("SCSI DMA pool exhausted.");
		    sgpnt[count].length = end_rec;
		    sgpnt[count].alt_address = sgpnt[count].address;
		    if (count+1 != SCpnt->use_sg) panic("Bad sr request list");
		    break;
		};
		if (((long) sgpnt[count].address) + sgpnt[count].length > ISA_DMA_THRESHOLD &&
		  SCpnt->host->unchecked_isa_dma) {
		    sgpnt[count].alt_address = sgpnt[count].address;
		    /* We try and avoid exhausting the DMA pool, since it is easier
		     * to control usage here.  In other places we might have a more
		     * pressing need, and we would be screwed if we ran out */
		    if(dma_free_sectors < (sgpnt[count].length >> 9) + 5) {
			sgpnt[count].address = NULL;
		    } else {
			sgpnt[count].address = (char *) scsi_malloc(sgpnt[count].length);
		    };
		    /* If we start running low on DMA buffers, we abort the scatter-gather
		     * operation, and free all of the memory we have allocated.  We want to
		     * ensure that all scsi operations are able to do at least a non-scatter/gather
		     * operation */
		    if(sgpnt[count].address == NULL){ /* Out of dma memory */
			printk("Warning: Running low on SCSI DMA buffers");
			/* Try switching back to a non scatter-gather operation. */
			while(--count >= 0){
			    if(sgpnt[count].alt_address) 
				scsi_free(sgpnt[count].address, sgpnt[count].length);
			};
			SCpnt->use_sg = 0;
			scsi_free(buffer, SCpnt->sglist_len);
			break;
		    }; /* if address == NULL */
		};  /* if need DMA fixup */
	    };  /* for loop to fill list */
#ifdef DEBUG
	    printk("SR: %d %d %d %d %d *** ",SCpnt->use_sg, SCpnt->request.sector,
		   this_count, 
		   SCpnt->request.current_nr_sectors,
		   SCpnt->request.nr_sectors);
	    for(count=0; count<SCpnt->use_sg; count++)
		printk("SGlist: %d %x %x %x\n", count,
		       sgpnt[count].address, 
		       sgpnt[count].alt_address, 
		       sgpnt[count].length);
#endif
	};  /* Able to allocate scatter-gather list */
	};
	
	if (SCpnt->use_sg == 0){
	/* We cannot use scatter-gather.  Do this the old fashion way */
	if (!SCpnt->request.bh)  	
	    this_count = SCpnt->request.nr_sectors;
	else
	    this_count = (SCpnt->request.bh->b_size >> 9);
	
	start = block % 4;
	if (start)
	    {				  
	    this_count = ((this_count > 4 - start) ? 
			  (4 - start) : (this_count));
	    buffer = (unsigned char *) scsi_malloc(2048);
	    } 
	else if (this_count < 4)
	    {
	    buffer = (unsigned char *) scsi_malloc(2048);
	    }
	else
	    {
	    this_count -= this_count % 4;
	    buffer = (unsigned char *) SCpnt->request.buffer;
	    if (((long) buffer) + (this_count << 9) > ISA_DMA_THRESHOLD &&
		SCpnt->host->unchecked_isa_dma)
		buffer = (unsigned char *) scsi_malloc(this_count << 9);
	    }
	};
    
	if (scsi_CDs[dev].sector_size == 2048)
	block = block >> 2; /* These are the sectors that the cdrom uses */
	else
	block = block & 0xfffffffc;
    
	realcount = (this_count + 3) / 4;
    
	if (scsi_CDs[dev].sector_size == 512) realcount = realcount << 2;
    
	if (((realcount > 0xff) || (block > 0x1fffff)) && scsi_CDs[dev].ten) 
    {
		if (realcount > 0xffff)
	{
			realcount = 0xffff;
			this_count = realcount * (scsi_CDs[dev].sector_size >> 9);
	}
	
		cmd[0] += READ_10 - READ_6 ;
		cmd[2] = (unsigned char) (block >> 24) & 0xff;
		cmd[3] = (unsigned char) (block >> 16) & 0xff;
		cmd[4] = (unsigned char) (block >> 8) & 0xff;
		cmd[5] = (unsigned char) block & 0xff;
		cmd[6] = cmd[9] = 0;
		cmd[7] = (unsigned char) (realcount >> 8) & 0xff;
		cmd[8] = (unsigned char) realcount & 0xff;
    }
	else
    {
	if (realcount > 0xff)
	{
	    realcount = 0xff;
	    this_count = realcount * (scsi_CDs[dev].sector_size >> 9);
	}
	
	cmd[1] |= (unsigned char) ((block >> 16) & 0x1f);
	cmd[2] = (unsigned char) ((block >> 8) & 0xff);
	cmd[3] = (unsigned char) block & 0xff;
	cmd[4] = (unsigned char) realcount;
	cmd[5] = 0;
    }   
    
#ifdef DEBUG
    { 
	int i;
	printk("ReadCD: %d %d %d %d\n",block, realcount, buffer, this_count);
	printk("Use sg: %d\n", SCpnt->use_sg);
	printk("Dumping command: ");
	for(i=0; i<12; i++) printk("%2.2x ", cmd[i]);
	printk("\n");
    };
#endif
    
    /* Some dumb host adapters can speed transfers by knowing the
     * minimum transfersize in advance.
     *
     * We shouldn't disconnect in the middle of a sector, but the cdrom
     * sector size can be larger than the size of a buffer and the
     * transfer may be split to the size of a buffer.  So it's safe to
     * assume that we can at least transfer the minimum of the buffer
     * size (1024) and the sector size between each connect / disconnect.
     */
    
    SCpnt->transfersize = (scsi_CDs[dev].sector_size > 1024) ?
	1024 : scsi_CDs[dev].sector_size;
    
	SCpnt->this_count = this_count;
	scsi_do_cmd (SCpnt, (void *) cmd, buffer, 
		 realcount * scsi_CDs[dev].sector_size, 
		 rw_intr, SR_TIMEOUT, MAX_RETRIES);
}

static int sr_detect(Scsi_Device * SDp){
    
    if(SDp->type != TYPE_ROM && SDp->type != TYPE_WORM) return 0;
    
    printk("Detected scsi CD-ROM sr%d at scsi%d, channel %d, id %d, lun %d\n", 
	   sr_template.dev_noticed++,
	   SDp->host->host_no, SDp->channel, SDp->id, SDp->lun); 
    
    return 1;
}

static int sr_attach(Scsi_Device * SDp){
    Scsi_CD * cpnt;
    int i;
    
    if(SDp->type != TYPE_ROM && SDp->type != TYPE_WORM) return 1;
    
    if (sr_template.nr_dev >= sr_template.dev_max)
    {
	SDp->attached--;
	return 1;
    }
    
    for(cpnt = scsi_CDs, i=0; i<sr_template.dev_max; i++, cpnt++) 
	if(!cpnt->device) break;
    
    if(i >= sr_template.dev_max) panic ("scsi_devices corrupt (sr)");
    
    SDp->scsi_request_fn = do_sr_request;
    scsi_CDs[i].device = SDp;
    sr_template.nr_dev++;
    if(sr_template.nr_dev > sr_template.dev_max)
	panic ("scsi_devices corrupt (sr)");
    return 0;
}


static void sr_init_done (Scsi_Cmnd * SCpnt)
{
    struct request * req;
    
    req = &SCpnt->request;
    req->rq_status = RQ_SCSI_DONE; /* Busy, but indicate request done */
    
    if (req->sem != NULL) {
	up(req->sem);
    }
}

static void get_sectorsize(int i){
    unsigned char cmd[10];
    unsigned char *buffer;
    int the_result, retries;
    Scsi_Cmnd * SCpnt;
    
    buffer = (unsigned char *) scsi_malloc(512);
    SCpnt = allocate_device(NULL, scsi_CDs[i].device, 1);
    
    retries = 3;
    do {
	cmd[0] = READ_CAPACITY;
	cmd[1] = (scsi_CDs[i].device->lun << 5) & 0xe0;
	memset ((void *) &cmd[2], 0, 8);
	SCpnt->request.rq_status = RQ_SCSI_BUSY;  /* Mark as really busy */
	SCpnt->cmd_len = 0;
	
	memset(buffer, 0, 8);

	/* Do the command and wait.. */
	{
	    struct semaphore sem = MUTEX_LOCKED;
	    SCpnt->request.sem = &sem;
	    scsi_do_cmd (SCpnt,
			 (void *) cmd, (void *) buffer,
			 512, sr_init_done,  SR_TIMEOUT,
			 MAX_RETRIES);
	    down(&sem);
	}
	
	the_result = SCpnt->result;
	retries--;
	
    } while(the_result && retries);
    
    SCpnt->request.rq_status = RQ_INACTIVE;  /* Mark as not busy */
    
    wake_up(&SCpnt->device->device_wait); 
    
    if (the_result) {
	scsi_CDs[i].capacity = 0x1fffff;
	scsi_CDs[i].sector_size = 2048;  /* A guess, just in case */
	scsi_CDs[i].needs_sector_size = 1;
    } else {
	scsi_CDs[i].capacity = (buffer[0] << 24) |
	    (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	scsi_CDs[i].sector_size = (buffer[4] << 24) |
	    (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
	if(scsi_CDs[i].sector_size == 0) scsi_CDs[i].sector_size = 2048;
	/* Work around bug/feature in HP 4020i CD-Recorder... */
	if(scsi_CDs[i].sector_size == 2340) scsi_CDs[i].sector_size = 2048;
	if(scsi_CDs[i].sector_size != 2048 && 
	   scsi_CDs[i].sector_size != 512) {
	    printk ("scd%d : unsupported sector size %d.\n",
		    i, scsi_CDs[i].sector_size);
	    scsi_CDs[i].capacity = 0;
	    scsi_CDs[i].needs_sector_size = 1;
	};
	if(scsi_CDs[i].sector_size == 2048)
	    scsi_CDs[i].capacity *= 4;
	scsi_CDs[i].needs_sector_size = 0;
	sr_sizes[i] = scsi_CDs[i].capacity;
    };
    scsi_free(buffer, 512);
}

static int sr_registered = 0;

static int sr_init()
{
	int i;
    
	if(sr_template.dev_noticed == 0) return 0;
    
	if(!sr_registered) {
	if (register_blkdev(MAJOR_NR,"sr",&sr_fops)) {
	    printk("Unable to get major %d for SCSI-CD\n",MAJOR_NR);
	    return 1;
	}
	sr_registered++;
	}
    
	
	if (scsi_CDs) return 0;
	sr_template.dev_max = sr_template.dev_noticed + SR_EXTRA_DEVS;
	scsi_CDs = (Scsi_CD *) scsi_init_malloc(sr_template.dev_max * sizeof(Scsi_CD), GFP_ATOMIC);
	memset(scsi_CDs, 0, sr_template.dev_max * sizeof(Scsi_CD));
    
	sr_sizes = (int *) scsi_init_malloc(sr_template.dev_max * sizeof(int), GFP_ATOMIC);
	memset(sr_sizes, 0, sr_template.dev_max * sizeof(int));
    
	sr_blocksizes = (int *) scsi_init_malloc(sr_template.dev_max * 
					     sizeof(int), GFP_ATOMIC);
	for(i=0;i<sr_template.dev_max;i++) sr_blocksizes[i] = 2048;
	blksize_size[MAJOR_NR] = sr_blocksizes;
	return 0;
}

void sr_finish()
{
    int i;
    
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	blk_size[MAJOR_NR] = sr_sizes;	
    
	for (i = 0; i < sr_template.nr_dev; ++i)
    {
	/* If we have already seen this, then skip it.  Comes up
	 * with loadable modules. */
	if (scsi_CDs[i].capacity) continue;
	scsi_CDs[i].capacity = 0x1fffff;
	scsi_CDs[i].sector_size = 2048;  /* A guess, just in case */
	scsi_CDs[i].needs_sector_size = 1;
#if 0
	/* seems better to leave this for later */
	get_sectorsize(i);
	printk("Scd sectorsize = %d bytes.\n", scsi_CDs[i].sector_size);
#endif
	scsi_CDs[i].use = 1;
	scsi_CDs[i].ten = 1;
	scsi_CDs[i].remap = 1;
	scsi_CDs[i].auto_eject = 0; /* Default is not to eject upon unmount. */
	sr_sizes[i] = scsi_CDs[i].capacity;
    }
    
    
	/* If our host adapter is capable of scatter-gather, then we increase
	 * the read-ahead to 16 blocks (32 sectors).  If not, we use
	 * a two block (4 sector) read ahead. */
	if(scsi_CDs[0].device && scsi_CDs[0].device->host->sg_tablesize)
	read_ahead[MAJOR_NR] = 32;  /* 32 sector read-ahead.  Always removable. */
	else
	read_ahead[MAJOR_NR] = 4;  /* 4 sector read-ahead */
    
	return;
}	

static void sr_detach(Scsi_Device * SDp)
{
    Scsi_CD * cpnt;
    int i;
    
    for(cpnt = scsi_CDs, i=0; i<sr_template.dev_max; i++, cpnt++) 
	if(cpnt->device == SDp) {
	    kdev_t devi = MKDEV(MAJOR_NR, i);

	    /*
	     * Since the cdrom is read-only, no need to sync the device.
	     * We should be kind to our buffer cache, however.
	     */
	    invalidate_inodes(devi);
	    invalidate_buffers(devi);
	    
	    /*
	     * Reset things back to a sane state so that one can re-load a new
	     * driver (perhaps the same one).
	     */
	    cpnt->device = NULL;
	    cpnt->capacity = 0;
	    SDp->attached--;
	    sr_template.nr_dev--;
	    sr_template.dev_noticed--;
	    sr_sizes[i] = 0;
	    return;
	}
    return;
}


#ifdef MODULE

int init_module(void) {
    sr_template.usage_count = &mod_use_count_;
    return scsi_register_module(MODULE_SCSI_DEV, &sr_template);
}

void cleanup_module( void) 
{
    scsi_unregister_module(MODULE_SCSI_DEV, &sr_template);
    unregister_blkdev(SCSI_CDROM_MAJOR, "sr");
    sr_registered--;
    if(scsi_CDs != NULL) {
	scsi_init_free((char *) scsi_CDs,
		       (sr_template.dev_noticed + SR_EXTRA_DEVS) 
		       * sizeof(Scsi_CD));
	
	scsi_init_free((char *) sr_sizes, sr_template.dev_max * sizeof(int));
	scsi_init_free((char *) sr_blocksizes, sr_template.dev_max * sizeof(int));
    }
    
    blksize_size[MAJOR_NR] = NULL;
    blk_dev[MAJOR_NR].request_fn = NULL;
    blk_size[MAJOR_NR] = NULL;	
    read_ahead[MAJOR_NR] = 0;
    
    sr_template.dev_max = 0;
}
#endif /* MODULE */

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
