/*
 *      sd.c Copyright (C) 1992 Drew Eckhardt 
 *           Copyright (C) 1993, 1994, 1995 Eric Youngdale
 *
 *      Linux scsi disk driver
 *              Initial versions: Drew Eckhardt 
 *              Subsequent revisions: Eric Youngdale
 *
 *      <drew@colorado.edu>
 *
 *       Modified by Eric Youngdale ericy@cais.com to
 *       add scatter-gather, multiple outstanding request, and other
 *       enhancements.
 *
 *       Modified by Eric Youngdale eric@aib.com to support loadable
 *       low-level scsi drivers.
 */

#include <linux/module.h>
#ifdef MODULE
/*
 * This is a variable in scsi.c that is set when we are processing something
 * after boot time.  By definition, this is true when we are a loadable module
 * ourselves.
 */
#define MODULE_FLAG 1
#else
#define MODULE_FLAG scsi_loadable_module_flag
#endif /* MODULE */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <asm/system.h>

#define MAJOR_NR SCSI_DISK_MAJOR
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#include "scsi_ioctl.h"
#include "constants.h"

#include <linux/genhd.h>

/*
 *  static const char RCSid[] = "$Header:";
 */

#define MAX_RETRIES 5

/*
 *  Time out in seconds for disks and Magneto-opticals (which are slower).
 */

#define SD_TIMEOUT (7 * HZ)
#define SD_MOD_TIMEOUT (8 * HZ)

#define CLUSTERABLE_DEVICE(SC) (SC->host->use_clustering && \
				SC->device->type != TYPE_MOD)

struct hd_struct * sd;

Scsi_Disk * rscsi_disks = NULL;
static int * sd_sizes;
static int * sd_blocksizes;
static int * sd_hardsizes;              /* Hardware sector size */

extern int sd_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

static int check_scsidisk_media_change(kdev_t);
static int fop_revalidate_scsidisk(kdev_t);

static sd_init_onedisk(int);

static void requeue_sd_request (Scsi_Cmnd * SCpnt);

static int sd_init(void);
static void sd_finish(void);
static int sd_attach(Scsi_Device *);
static int sd_detect(Scsi_Device *);
static void sd_detach(Scsi_Device *);

struct Scsi_Device_Template sd_template = 
{ NULL, "disk", "sd", NULL, TYPE_DISK, 
      SCSI_DISK_MAJOR, 0, 0, 0, 1,
      sd_detect, sd_init,
      sd_finish, sd_attach, sd_detach
};

static int sd_open(struct inode * inode, struct file * filp)
{
    int target;
    target =  DEVICE_NR(inode->i_rdev);
    
    if(target >= sd_template.dev_max || !rscsi_disks[target].device)
	return -ENXIO;   /* No such device */
    
    /* 
     * Make sure that only one process can do a check_change_disk at one time.
     * This is also used to lock out further access when the partition table 
     * is being re-read. 
     */
    
    while (rscsi_disks[target].device->busy)
    barrier();   
    if(rscsi_disks[target].device->removable) {
	check_disk_change(inode->i_rdev);
	
	/*
	 * If the drive is empty, just let the open fail.
	 */
	if ( !rscsi_disks[target].ready ) {
	    return -ENXIO;
	}

	/*
	 * Similarily, if the device has the write protect tab set,
	 * have the open fail if the user expects to be able to write
	 * to the thing.
	 */
	if ( (rscsi_disks[target].write_prot) && (filp->f_mode & 2) ) { 
	    return -EROFS;
	}

	if(!rscsi_disks[target].device->access_count)
	    sd_ioctl(inode, NULL, SCSI_IOCTL_DOORLOCK, 0);
    };

    /*
     * See if we are requesting a non-existent partition.  Do this
     * after checking for disk change.
     */
    if(sd_sizes[MINOR(inode->i_rdev)] == 0)
	return -ENXIO;
    
    rscsi_disks[target].device->access_count++;
    if (rscsi_disks[target].device->host->hostt->usage_count)
	(*rscsi_disks[target].device->host->hostt->usage_count)++;
    if(sd_template.usage_count) (*sd_template.usage_count)++;
    return 0;
}

static void sd_release(struct inode * inode, struct file * file)
{
    int target;
    sync_dev(inode->i_rdev);
    
    target =  DEVICE_NR(inode->i_rdev);
    
    rscsi_disks[target].device->access_count--;
    if (rscsi_disks[target].device->host->hostt->usage_count)
	(*rscsi_disks[target].device->host->hostt->usage_count)--;
    if(sd_template.usage_count) (*sd_template.usage_count)--;
    
    if(rscsi_disks[target].device->removable) {
	if(!rscsi_disks[target].device->access_count)
	    sd_ioctl(inode, NULL, SCSI_IOCTL_DOORUNLOCK, 0);
    }
}

static void sd_geninit(struct gendisk *);

static struct file_operations sd_fops = {
    NULL,                        /* lseek - default */
    block_read,                  /* read - general block-dev read */
    block_write,                 /* write - general block-dev write */
    NULL,                        /* readdir - bad */
    NULL,                        /* select */
    sd_ioctl,                    /* ioctl */
    NULL,                        /* mmap */
    sd_open,                     /* open code */
    sd_release,                  /* release */
    block_fsync,                 /* fsync */
    NULL,                        /* fasync */
    check_scsidisk_media_change, /* Disk change */
    fop_revalidate_scsidisk      /* revalidate */
};

static struct gendisk sd_gendisk = {
    MAJOR_NR,                    /* Major number */
    "sd",                        /* Major name */
    4,                           /* Bits to shift to get real from partition */
    1 << 4,                      /* Number of partitions per real */
    0,                           /* maximum number of real */
    sd_geninit,                  /* init function */
    NULL,                        /* hd struct */
    NULL,                        /* block sizes */
    0,                           /* number */
    NULL,                        /* internal */
    NULL                         /* next */
};

static void sd_geninit (struct gendisk *ignored)
{
    int i;
    
    for (i = 0; i < sd_template.dev_max; ++i)
	if(rscsi_disks[i].device) 
	    sd[i << 4].nr_sects = rscsi_disks[i].capacity;
#if 0
    /* No longer needed - we keep track of this as we attach/detach */
    sd_gendisk.nr_real = sd_template.dev_max;
#endif
}

/*
 * rw_intr is the interrupt routine for the device driver.  It will
 * be notified on the end of a SCSI read / write, and
 * will take on of several actions based on success or failure.
 */

static void rw_intr (Scsi_Cmnd *SCpnt)
{
    int result = SCpnt->result;
    int this_count = SCpnt->bufflen >> 9;
    
#ifdef DEBUG
    printk("sd%c : rw_intr(%d, %d)\n", 'a' + MINOR(SCpnt->request.rq_dev), 
	   SCpnt->host->host_no, result);
#endif
    
    /*
     * First case : we assume that the command succeeded.  One of two things 
     * will happen here.  Either we will be finished, or there will be more
     * sectors that we were unable to read last time.
     */

    if (!result) {
	
#ifdef DEBUG
	printk("sd%c : %d sectors remain.\n", 'a' + MINOR(SCpnt->request.rq_dev),
	       SCpnt->request.nr_sectors);
	printk("use_sg is %d\n ",SCpnt->use_sg);
#endif
	if (SCpnt->use_sg) {
	    struct scatterlist * sgpnt;
	    int i;
	    sgpnt = (struct scatterlist *) SCpnt->buffer;
	    for(i=0; i<SCpnt->use_sg; i++) {
#ifdef DEBUG
		printk(":%x %x %d\n",sgpnt[i].alt_address, sgpnt[i].address, 
		       sgpnt[i].length);
#endif
		if (sgpnt[i].alt_address) {
		    if (SCpnt->request.cmd == READ)
			memcpy(sgpnt[i].alt_address, sgpnt[i].address, 
			       sgpnt[i].length);
		    scsi_free(sgpnt[i].address, sgpnt[i].length);
		};
	    };

	    /* Free list of scatter-gather pointers */
	    scsi_free(SCpnt->buffer, SCpnt->sglist_len);  
	} else {
	    if (SCpnt->buffer != SCpnt->request.buffer) {
#ifdef DEBUG
		printk("nosg: %x %x %d\n",SCpnt->request.buffer, SCpnt->buffer,
		       SCpnt->bufflen);
#endif  
		if (SCpnt->request.cmd == READ)
		    memcpy(SCpnt->request.buffer, SCpnt->buffer,
			   SCpnt->bufflen);
		scsi_free(SCpnt->buffer, SCpnt->bufflen);
	    };
	};
	/*
	 * If multiple sectors are requested in one buffer, then
	 * they will have been finished off by the first command.
	 * If not, then we have a multi-buffer command.
	 */
	if (SCpnt->request.nr_sectors > this_count)
	{
	    SCpnt->request.errors = 0;
	    
	    if (!SCpnt->request.bh)
	    {
#ifdef DEBUG
		printk("sd%c : handling page request, no buffer\n",
		       'a' + MINOR(SCpnt->request.rq_dev));
#endif
		/*
		 * The SCpnt->request.nr_sectors field is always done in 
		 * 512 byte sectors, even if this really isn't the case.
		 */
		panic("sd.c: linked page request (%lx %x)",
		      SCpnt->request.sector, this_count);
	    }
	}
	SCpnt = end_scsi_request(SCpnt, 1, this_count);
	requeue_sd_request(SCpnt);
	return;
    }
    
    /* Free up any indirection buffers we allocated for DMA purposes. */
    if (SCpnt->use_sg) {
	struct scatterlist * sgpnt;
	int i;
	sgpnt = (struct scatterlist *) SCpnt->buffer;
	for(i=0; i<SCpnt->use_sg; i++) {
#ifdef DEBUG
	    printk("err: %x %x %d\n",SCpnt->request.buffer, SCpnt->buffer,
		   SCpnt->bufflen);
#endif
	    if (sgpnt[i].alt_address) {
		scsi_free(sgpnt[i].address, sgpnt[i].length);
	    };
	};
	scsi_free(SCpnt->buffer, SCpnt->sglist_len);  /* Free list of scatter-gather pointers */
    } else {
#ifdef DEBUG
	printk("nosgerr: %x %x %d\n",SCpnt->request.buffer, SCpnt->buffer,
	       SCpnt->bufflen);
#endif
	if (SCpnt->buffer != SCpnt->request.buffer)
	    scsi_free(SCpnt->buffer, SCpnt->bufflen);
    };
    
    /*
     * Now, if we were good little boys and girls, Santa left us a request
     * sense buffer.  We can extract information from this, so we
     * can choose a block to remap, etc.
     */

    if (driver_byte(result) != 0) {
	if (suggestion(result) == SUGGEST_REMAP) {
#ifdef REMAP
	    /*
	     * Not yet implemented.  A read will fail after being remapped,
	     * a write will call the strategy routine again.
	     */
	    if rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].remap
	    {
		result = 0;
	    }
	    else
#endif
	}
	
	if ((SCpnt->sense_buffer[0] & 0x7f) == 0x70) {
	    if ((SCpnt->sense_buffer[2] & 0xf) == UNIT_ATTENTION) {
		if(rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->removable) {
		    /* detected disc change.  set a bit and quietly refuse
		     * further access.
		     */  
		    rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->changed = 1;
		    SCpnt = end_scsi_request(SCpnt, 0, this_count);
		    requeue_sd_request(SCpnt);
		    return;
		}
                else
                {
                    /*
                     * Must have been a power glitch, or a bus reset.
                     * Could not have been a media change, so we just retry
                     * the request and see what happens.
                     */
                    requeue_sd_request(SCpnt);
                    return;
                }
	    }
	}
	
	
	/* If we had an ILLEGAL REQUEST returned, then we may have
	 * performed an unsupported command.  The only thing this should be 
	 * would be a ten byte read where only a six byte read was supported.
	 * Also, on a system where READ CAPACITY failed, we have have read 
	 * past the end of the disk. 
	 */

	if (SCpnt->sense_buffer[2] == ILLEGAL_REQUEST) {
	    if (rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].ten) {
		rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].ten = 0;
		requeue_sd_request(SCpnt);
		result = 0;
	    } else {
		/* ???? */
	    }
	}
    }  /* driver byte != 0 */
    if (result) {
	printk("SCSI disk error : host %d channel %d id %d lun %d return code = %x\n",
	       rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->host->host_no,
	       rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->channel,
	   rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->id,
	     rscsi_disks[DEVICE_NR(SCpnt->request.rq_dev)].device->lun, result);
	
	if (driver_byte(result) & DRIVER_SENSE)
	    print_sense("sd", SCpnt);
	SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.current_nr_sectors);
	requeue_sd_request(SCpnt);
	return;
    }
}

/*
 * requeue_sd_request() is the request handler function for the sd driver.
 * Its function in life is to take block device requests, and translate
 * them to SCSI commands.
 */

static void do_sd_request (void)
{
    Scsi_Cmnd * SCpnt = NULL;
    Scsi_Device * SDev;
    struct request * req = NULL;
    unsigned long flags;
    int flag = 0;
    
    save_flags(flags);
    while (1==1){
	cli();
	if (CURRENT != NULL && CURRENT->rq_status == RQ_INACTIVE) {
	    restore_flags(flags);
	    return;
	};
	
	INIT_SCSI_REQUEST;
        SDev = rscsi_disks[DEVICE_NR(CURRENT->rq_dev)].device;
        
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
		
	/* We have to be careful here. allocate_device will get a free pointer,
	 * but there is no guarantee that it is queueable.  In normal usage, 
	 * we want to call this, because other types of devices may have the 
	 * host all tied up, and we want to make sure that we have at least 
	 * one request pending for this type of device. We can also come 
	 * through here while servicing an interrupt, because of the need to 
	 * start another command. If we call allocate_device more than once, 
	 * then the system can wedge if the command is not queueable. The 
	 * request_queueable function is safe because it checks to make sure 
	 * that the host is able to take another command before it returns
	 * a pointer.  
	 */

	if (flag++ == 0)
	    SCpnt = allocate_device(&CURRENT,
				    rscsi_disks[DEVICE_NR(CURRENT->rq_dev)].device, 0); 
	else SCpnt = NULL;
	
	/*
	 * The following restore_flags leads to latency problems.  FIXME.
	 * Using a "sti()" gets rid of the latency problems but causes
	 * race conditions and crashes.
	 */
	restore_flags(flags);

	/* This is a performance enhancement. We dig down into the request 
	 * list and try and find a queueable request (i.e. device not busy, 
	 * and host able to accept another command. If we find one, then we 
	 * queue it. This can make a big difference on systems with more than 
	 * one disk drive.  We want to have the interrupts off when monkeying 
	 * with the request list, because otherwise the kernel might try and 
	 * slip in a request in between somewhere. 
	 */

	if (!SCpnt && sd_template.nr_dev > 1){
	    struct request *req1;
	    req1 = NULL;
	    cli();
	    req = CURRENT;
	    while(req){
		SCpnt = request_queueable(req, rscsi_disks[DEVICE_NR(req->rq_dev)].device);
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
	
	if (!SCpnt) return; /* Could not find anything to do */
	
	/* Queue command */
	requeue_sd_request(SCpnt);
    };  /* While */
}    

static void requeue_sd_request (Scsi_Cmnd * SCpnt)
{
    int dev, devm, block, this_count;
    unsigned char cmd[10];
    int bounce_size, contiguous;
    int max_sg;
    struct buffer_head * bh, *bhp;
    char * buff, *bounce_buffer;
    
 repeat:
    
    if(!SCpnt || SCpnt->request.rq_status == RQ_INACTIVE) {
	do_sd_request();
	return;
    }
    
    devm =  MINOR(SCpnt->request.rq_dev);
    dev = DEVICE_NR(SCpnt->request.rq_dev);

    block = SCpnt->request.sector;
    this_count = 0;

#ifdef DEBUG
    printk("Doing sd request, dev = %d, block = %d\n", devm, block);
#endif
    
    if (devm >= (sd_template.dev_max << 4) || 
	!rscsi_disks[dev].device ||
	block + SCpnt->request.nr_sectors > sd[devm].nr_sects)
    {
	SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
	goto repeat;
    }
    
    block += sd[devm].start_sect;
    
    if (rscsi_disks[dev].device->changed)
    {
	/*
	 * quietly refuse to do anything to a changed disc until the changed 
	 * bit has been reset
	 */
	/* printk("SCSI disk has been changed. Prohibiting further I/O.\n"); */
	SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
	goto repeat;
    }
    
#ifdef DEBUG
    printk("sd%c : real dev = /dev/sd%c, block = %d\n", 
	   'a' + devm, dev, block);
#endif
    
    /*
     * If we have a 1K hardware sectorsize, prevent access to single
     * 512 byte sectors.  In theory we could handle this - in fact
     * the scsi cdrom driver must be able to handle this because
     * we typically use 1K blocksizes, and cdroms typically have
     * 2K hardware sectorsizes.  Of course, things are simpler
     * with the cdrom, since it is read-only.  For performance
     * reasons, the filesystems should be able to handle this
     * and not force the scsi disk driver to use bounce buffers
     * for this.
     */
    if (rscsi_disks[dev].sector_size == 1024)
	if((block & 1) || (SCpnt->request.nr_sectors & 1)) {
	    printk("sd.c:Bad block number requested");
	    SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
	    goto repeat;
	}
    
    switch (SCpnt->request.cmd)
    {
    case WRITE :
	if (!rscsi_disks[dev].device->writeable)
	{
	    SCpnt = end_scsi_request(SCpnt, 0, SCpnt->request.nr_sectors);
	    goto repeat;
	}
	cmd[0] = WRITE_6;
	break;
    case READ :
	cmd[0] = READ_6;
	break;
    default :
	panic ("Unknown sd command %d\n", SCpnt->request.cmd);
    }
    
    SCpnt->this_count = 0;
    
    /* If the host adapter can deal with very large scatter-gather
     * requests, it is a waste of time to cluster 
     */
    contiguous = (!CLUSTERABLE_DEVICE(SCpnt) ? 0 :1);
    bounce_buffer = NULL;
    bounce_size = (SCpnt->request.nr_sectors << 9);
    
    /* First see if we need a bounce buffer for this request. If we do, make 
     * sure that we can allocate a buffer. Do not waste space by allocating 
     * a bounce buffer if we are straddling the 16Mb line 
     */ 
    if (contiguous && SCpnt->request.bh &&
	((long) SCpnt->request.bh->b_data) 
	+ (SCpnt->request.nr_sectors << 9) - 1 > ISA_DMA_THRESHOLD 
	&& SCpnt->host->unchecked_isa_dma) {
	if(((long) SCpnt->request.bh->b_data) > ISA_DMA_THRESHOLD)
	    bounce_buffer = (char *) scsi_malloc(bounce_size);
	if(!bounce_buffer) contiguous = 0;
    };
    
    if(contiguous && SCpnt->request.bh && SCpnt->request.bh->b_reqnext)
	for(bh = SCpnt->request.bh, bhp = bh->b_reqnext; bhp; bh = bhp, 
	    bhp = bhp->b_reqnext) {
	    if(!CONTIGUOUS_BUFFERS(bh,bhp)) { 
		if(bounce_buffer) scsi_free(bounce_buffer, bounce_size);
		contiguous = 0;
		break;
	    } 
	};
    if (!SCpnt->request.bh || contiguous) {
	
	/* case of page request (i.e. raw device), or unlinked buffer */
	this_count = SCpnt->request.nr_sectors;
	buff = SCpnt->request.buffer;
	SCpnt->use_sg = 0;
	
    } else if (SCpnt->host->sg_tablesize == 0 ||
	       (need_isa_buffer && dma_free_sectors <= 10)) {
	
	/* Case of host adapter that cannot scatter-gather.  We also
	 * come here if we are running low on DMA buffer memory.  We set
	 * a threshold higher than that we would need for this request so
	 * we leave room for other requests.  Even though we would not need
	 * it all, we need to be conservative, because if we run low enough
	 * we have no choice but to panic. 
	 */
	if (SCpnt->host->sg_tablesize != 0 &&
	    need_isa_buffer && 
	    dma_free_sectors <= 10)
	    printk("Warning: SCSI DMA buffer space running low.  Using non scatter-gather I/O.\n");
	
	this_count = SCpnt->request.current_nr_sectors;
	buff = SCpnt->request.buffer;
	SCpnt->use_sg = 0;
	
    } else {
	
	/* Scatter-gather capable host adapter */
	struct scatterlist * sgpnt;
	int count, this_count_max;
	int counted;
	
	bh = SCpnt->request.bh;
	this_count = 0;
	this_count_max = (rscsi_disks[dev].ten ? 0xffff : 0xff);
	count = 0;
	bhp = NULL;
	while(bh) {
	    if ((this_count + (bh->b_size >> 9)) > this_count_max) break;
	    if(!bhp || !CONTIGUOUS_BUFFERS(bhp,bh) ||
	       !CLUSTERABLE_DEVICE(SCpnt) ||
	       (SCpnt->host->unchecked_isa_dma &&
		((unsigned long) bh->b_data-1) == ISA_DMA_THRESHOLD)) {
		if (count < SCpnt->host->sg_tablesize) count++;
		else break;
	    };
	    this_count += (bh->b_size >> 9);
	    bhp = bh;
	    bh = bh->b_reqnext;
	};
#if 0
	if(SCpnt->host->unchecked_isa_dma &&
	   ((unsigned int) SCpnt->request.bh->b_data-1) == ISA_DMA_THRESHOLD) count--;
#endif
	SCpnt->use_sg = count;  /* Number of chains */
	count = 512;/* scsi_malloc can only allocate in chunks of 512 bytes */
	while( count < (SCpnt->use_sg * sizeof(struct scatterlist))) 
	    count = count << 1;
	SCpnt->sglist_len = count;
	max_sg = count / sizeof(struct scatterlist);
	if(SCpnt->host->sg_tablesize < max_sg) 
	    max_sg = SCpnt->host->sg_tablesize;
	sgpnt = (struct scatterlist * ) scsi_malloc(count);
	if (!sgpnt) {
	    printk("Warning - running *really* short on DMA buffers\n");
	    SCpnt->use_sg = 0;    /* No memory left - bail out */
	    this_count = SCpnt->request.current_nr_sectors;
	    buff = SCpnt->request.buffer;
	} else {
	    memset(sgpnt, 0, count);  /* Zero so it is easy to fill, but only
				       * if memory is available 
				       */
	    buff = (char *) sgpnt;
	    counted = 0;
	    for(count = 0, bh = SCpnt->request.bh, bhp = bh->b_reqnext;
		count < SCpnt->use_sg && bh; 
		count++, bh = bhp) {
		
		bhp = bh->b_reqnext;
		
		if(!sgpnt[count].address) sgpnt[count].address = bh->b_data;
		sgpnt[count].length += bh->b_size;
		counted += bh->b_size >> 9;
		
		if (((long) sgpnt[count].address) + sgpnt[count].length - 1 > 
		    ISA_DMA_THRESHOLD && (SCpnt->host->unchecked_isa_dma) &&
		    !sgpnt[count].alt_address) {
		    sgpnt[count].alt_address = sgpnt[count].address;
		    /* We try and avoid exhausting the DMA pool, since it is 
		     * easier to control usage here. In other places we might 
		     * have a more pressing need, and we would be screwed if 
		     * we ran out */
		    if(dma_free_sectors < (sgpnt[count].length >> 9) + 10) {
			sgpnt[count].address = NULL;
		    } else {
			sgpnt[count].address = 
			    (char *) scsi_malloc(sgpnt[count].length);
		    };
		    /* If we start running low on DMA buffers, we abort the 
		     * scatter-gather operation, and free all of the memory 
		     * we have allocated.  We want to ensure that all scsi 
		     * operations are able to do at least a non-scatter/gather
		     * operation */
		    if(sgpnt[count].address == NULL){ /* Out of dma memory */
#if 0
			printk("Warning: Running low on SCSI DMA buffers");
			/* Try switching back to a non s-g operation. */
			while(--count >= 0){
			    if(sgpnt[count].alt_address) 
				scsi_free(sgpnt[count].address, 
					  sgpnt[count].length);
			};
			this_count = SCpnt->request.current_nr_sectors;
			buff = SCpnt->request.buffer;
			SCpnt->use_sg = 0;
			scsi_free(sgpnt, SCpnt->sglist_len);
#endif
			SCpnt->use_sg = count;
			this_count = counted -= bh->b_size >> 9;
			break;
		    };
		    
		};
		
		/* Only cluster buffers if we know that we can supply DMA 
		 * buffers large enough to satisfy the request. Do not cluster
		 * a new request if this would mean that we suddenly need to 
		 * start using DMA bounce buffers */
		if(bhp && CONTIGUOUS_BUFFERS(bh,bhp) 
		   && CLUSTERABLE_DEVICE(SCpnt)) {
		    char * tmp;
		    
		    if (((long) sgpnt[count].address) + sgpnt[count].length +
			bhp->b_size - 1 > ISA_DMA_THRESHOLD && 
			(SCpnt->host->unchecked_isa_dma) &&
			!sgpnt[count].alt_address) continue;
		    
		    if(!sgpnt[count].alt_address) {count--; continue; }
		    if(dma_free_sectors > 10)
			tmp = (char *) scsi_malloc(sgpnt[count].length 
						   + bhp->b_size);
		    else {
			tmp = NULL;
			max_sg = SCpnt->use_sg;
		    };
		    if(tmp){
			scsi_free(sgpnt[count].address, sgpnt[count].length);
			sgpnt[count].address = tmp;
			count--;
			continue;
		    };
		    
		    /* If we are allowed another sg chain, then increment 
		     * counter so we can insert it.  Otherwise we will end 
		     up truncating */
		    
		    if (SCpnt->use_sg < max_sg) SCpnt->use_sg++;
		};  /* contiguous buffers */
	    }; /* for loop */
	    
	    /* This is actually how many we are going to transfer */
	    this_count = counted; 
	    
	    if(count < SCpnt->use_sg || SCpnt->use_sg 
	       > SCpnt->host->sg_tablesize){
		bh = SCpnt->request.bh;
		printk("Use sg, count %d %x %d\n", 
		       SCpnt->use_sg, count, dma_free_sectors);
		printk("maxsg = %x, counted = %d this_count = %d\n", 
		       max_sg, counted, this_count);
		while(bh){
		    printk("[%p %lx] ", bh->b_data, bh->b_size);
		    bh = bh->b_reqnext;
		};
		if(SCpnt->use_sg < 16)
		    for(count=0; count<SCpnt->use_sg; count++)
			printk("{%d:%p %p %d}  ", count,
			       sgpnt[count].address,
			       sgpnt[count].alt_address,
			       sgpnt[count].length);
		panic("Ooops");
	    };
	    
	    if (SCpnt->request.cmd == WRITE)
		for(count=0; count<SCpnt->use_sg; count++)
		    if(sgpnt[count].alt_address)
			memcpy(sgpnt[count].address, sgpnt[count].alt_address, 
			       sgpnt[count].length);
	};  /* Able to malloc sgpnt */
    };  /* Host adapter capable of scatter-gather */
    
    /* Now handle the possibility of DMA to addresses > 16Mb */
    
    if(SCpnt->use_sg == 0){
	if (((long) buff) + (this_count << 9) - 1 > ISA_DMA_THRESHOLD && 
	    (SCpnt->host->unchecked_isa_dma)) {
	    if(bounce_buffer)
		buff = bounce_buffer;
	    else
		buff = (char *) scsi_malloc(this_count << 9);
	    if(buff == NULL) {  /* Try backing off a bit if we are low on mem*/
		this_count = SCpnt->request.current_nr_sectors;
		buff = (char *) scsi_malloc(this_count << 9);
		if(!buff) panic("Ran out of DMA buffers.");
	    };
	    if (SCpnt->request.cmd == WRITE)
		memcpy(buff, (char *)SCpnt->request.buffer, this_count << 9);
	};
    };
#ifdef DEBUG
    printk("sd%c : %s %d/%d 512 byte blocks.\n", 
	   'a' + devm,
	   (SCpnt->request.cmd == WRITE) ? "writing" : "reading",
	   this_count, SCpnt->request.nr_sectors);
#endif
    
    cmd[1] = (SCpnt->lun << 5) & 0xe0;
    
    if (rscsi_disks[dev].sector_size == 1024){
	if(block & 1) panic("sd.c:Bad block number requested");
	if(this_count & 1) panic("sd.c:Bad block number requested");
	block = block >> 1;
	this_count = this_count >> 1;
    };
    
    if (rscsi_disks[dev].sector_size == 256){
	block = block << 1;
	this_count = this_count << 1;
    };
    
    if (((this_count > 0xff) ||  (block > 0x1fffff)) && rscsi_disks[dev].ten)
    {
	if (this_count > 0xffff)
	    this_count = 0xffff;
	
	cmd[0] += READ_10 - READ_6 ;
	cmd[2] = (unsigned char) (block >> 24) & 0xff;
	cmd[3] = (unsigned char) (block >> 16) & 0xff;
	cmd[4] = (unsigned char) (block >> 8) & 0xff;
	cmd[5] = (unsigned char) block & 0xff;
	cmd[6] = cmd[9] = 0;
	cmd[7] = (unsigned char) (this_count >> 8) & 0xff;
	cmd[8] = (unsigned char) this_count & 0xff;
    }
    else
    {
	if (this_count > 0xff)
	    this_count = 0xff;
	
	cmd[1] |= (unsigned char) ((block >> 16) & 0x1f);
	cmd[2] = (unsigned char) ((block >> 8) & 0xff);
	cmd[3] = (unsigned char) block & 0xff;
	cmd[4] = (unsigned char) this_count;
	cmd[5] = 0;
    }
    
    /*
     * We shouldn't disconnect in the middle of a sector, so with a dumb 
     * host adapter, it's safe to assume that we can at least transfer 
     * this many bytes between each connect / disconnect.  
     */
    
    SCpnt->transfersize = rscsi_disks[dev].sector_size;
    SCpnt->underflow = this_count << 9; 
    scsi_do_cmd (SCpnt, (void *) cmd, buff, 
		 this_count * rscsi_disks[dev].sector_size,
		 rw_intr, 
		 (SCpnt->device->type == TYPE_DISK ? 
		  SD_TIMEOUT : SD_MOD_TIMEOUT),
		 MAX_RETRIES);
}

static int check_scsidisk_media_change(kdev_t full_dev){
    int retval;
    int target;
    struct inode inode;
    int flag = 0;
    
    target =  DEVICE_NR(full_dev);
    
    if (target >= sd_template.dev_max ||
	!rscsi_disks[target].device) {
	printk("SCSI disk request error: invalid device.\n");
	return 0;
    };
    
    if(!rscsi_disks[target].device->removable) return 0;
    
    inode.i_rdev = full_dev;  /* This is all we really need here */
    retval = sd_ioctl(&inode, NULL, SCSI_IOCTL_TEST_UNIT_READY, 0);
    
    if(retval){ /* Unable to test, unit probably not ready.  This usually
		 * means there is no disc in the drive.  Mark as changed,
		 * and we will figure it out later once the drive is
		 * available again.  */
	
	rscsi_disks[target].ready = 0;
	rscsi_disks[target].device->changed = 1;
	return 1; /* This will force a flush, if called from
		   * check_disk_change */
    };
    
    /* 
     * for removable scsi disk ( FLOPTICAL ) we have to recognise the
     * presence of disk in the drive. This is kept in the Scsi_Disk
     * struct and tested at open !  Daniel Roche ( dan@lectra.fr ) 
     */
    
    rscsi_disks[target].ready = 1;	/* FLOPTICAL */

    retval = rscsi_disks[target].device->changed;
    if(!flag) rscsi_disks[target].device->changed = 0;
    return retval;
}

static void sd_init_done (Scsi_Cmnd * SCpnt)
{
    struct request * req;
    
    req = &SCpnt->request;
    req->rq_status = RQ_SCSI_DONE; /* Busy, but indicate request done */
    
    if (req->sem != NULL) {
	up(req->sem);
    }
}

static int sd_init_onedisk(int i)
{
    unsigned char cmd[10];
    unsigned char *buffer;
    unsigned long spintime;
    int the_result, retries;
    Scsi_Cmnd * SCpnt;
    
    /* We need to retry the READ_CAPACITY because a UNIT_ATTENTION is 
     * considered a fatal error, and many devices report such an error 
     * just after a scsi bus reset. 
     */
    
    SCpnt = allocate_device(NULL, rscsi_disks[i].device, 1);
    buffer = (unsigned char *) scsi_malloc(512);
    
    spintime = 0;
    
    /* Spin up drives, as required.  Only do this at boot time */
    if (!MODULE_FLAG){
	do{
	    retries = 0;
	    while(retries < 3)
	    {
		cmd[0] = TEST_UNIT_READY;
		cmd[1] = (rscsi_disks[i].device->lun << 5) & 0xe0;
		memset ((void *) &cmd[2], 0, 8);
		SCpnt->cmd_len = 0;
		SCpnt->sense_buffer[0] = 0;
		SCpnt->sense_buffer[2] = 0;

		{
		    struct semaphore sem = MUTEX_LOCKED;
		    /* Mark as really busy again */
		    SCpnt->request.rq_status = RQ_SCSI_BUSY;
		    SCpnt->request.sem = &sem;
		    scsi_do_cmd (SCpnt,
				 (void *) cmd, (void *) buffer,
				 512, sd_init_done,  SD_TIMEOUT,
				 MAX_RETRIES);
		    down(&sem);
		}

		the_result = SCpnt->result;
		retries++;
		if(   the_result == 0
		   || SCpnt->sense_buffer[2] != UNIT_ATTENTION)
		    break;
	    }
	    
	    /* Look for non-removable devices that return NOT_READY.  
	     * Issue command to spin up drive for these cases. */
	    if(the_result && !rscsi_disks[i].device->removable && 
	       SCpnt->sense_buffer[2] == NOT_READY) {
		int time1;
		if(!spintime){
		    printk( "sd%c: Spinning up disk...", 'a' + i );
		    cmd[0] = START_STOP;
		    cmd[1] = (rscsi_disks[i].device->lun << 5) & 0xe0;
		    cmd[1] |= 1;  /* Return immediately */
		    memset ((void *) &cmd[2], 0, 8);
		    cmd[4] = 1; /* Start spin cycle */
		    SCpnt->cmd_len = 0;
		    SCpnt->sense_buffer[0] = 0;
		    SCpnt->sense_buffer[2] = 0;
		    
		    {
		    	struct semaphore sem = MUTEX_LOCKED;
			/* Mark as really busy again */
			SCpnt->request.rq_status = RQ_SCSI_BUSY; 
		    	SCpnt->request.sem = &sem;
			scsi_do_cmd (SCpnt,
				     (void *) cmd, (void *) buffer,
				     512, sd_init_done,  SD_TIMEOUT,
				     MAX_RETRIES);
			down(&sem);
		    }
		    
		    spintime = jiffies;
		}
		
		time1 = jiffies;
		while(jiffies < time1 + HZ); /* Wait 1 second for next try */
		printk( "." );
	    };
	} while(the_result && spintime && spintime+100*HZ > jiffies);
	if (spintime) {
	    if (the_result)
		printk( "not responding...\n" );
	    else
		printk( "ready\n" );
	}
    };  /* !MODULE_FLAG */
    
    
    retries = 3;
    do {
	cmd[0] = READ_CAPACITY;
	cmd[1] = (rscsi_disks[i].device->lun << 5) & 0xe0;
	memset ((void *) &cmd[2], 0, 8);
	memset ((void *) buffer, 0, 8);
	SCpnt->cmd_len = 0;
	SCpnt->sense_buffer[0] = 0;
	SCpnt->sense_buffer[2] = 0;

	{
	    struct semaphore sem = MUTEX_LOCKED;
	    /* Mark as really busy again */
	    SCpnt->request.rq_status = RQ_SCSI_BUSY;
	    SCpnt->request.sem = &sem;
	    scsi_do_cmd (SCpnt,
			 (void *) cmd, (void *) buffer,
			 8, sd_init_done,  SD_TIMEOUT,
			 MAX_RETRIES);
	    down(&sem);	/* sleep until it is ready */
	}
	
	the_result = SCpnt->result;
	retries--;
	
    } while(the_result && retries);
    
    SCpnt->request.rq_status = RQ_INACTIVE;  /* Mark as not busy */
    
    wake_up(&SCpnt->device->device_wait); 
    
    /* Wake up a process waiting for device */
    
    /*
     * The SCSI standard says: 
     * "READ CAPACITY is necessary for self configuring software"
     *  While not mandatory, support of READ CAPACITY is strongly encouraged.
     *  We used to die if we couldn't successfully do a READ CAPACITY.
     *  But, now we go on about our way.  The side effects of this are
     *
     *  1. We can't know block size with certainty. I have said "512 bytes 
     *     is it" as this is most common.
     *
     *  2. Recovery from when some one attempts to read past the end of the 
     *     raw device will be slower.
     */
    
    if (the_result)
    {
	printk ("sd%c : READ CAPACITY failed.\n"
		"sd%c : status = %x, message = %02x, host = %d, driver = %02x \n",
		'a' + i, 'a' + i,
		status_byte(the_result),
		msg_byte(the_result),
		host_byte(the_result),
		driver_byte(the_result)
		);
	if (driver_byte(the_result)  & DRIVER_SENSE)
	    printk("sd%c : extended sense code = %1x \n", 
		   'a' + i, SCpnt->sense_buffer[2] & 0xf);
	else
	    printk("sd%c : sense not available. \n", 'a' + i);
	
	printk("sd%c : block size assumed to be 512 bytes, disk size 1GB.  \n",
	       'a' + i);
	rscsi_disks[i].capacity = 0x1fffff;
	rscsi_disks[i].sector_size = 512;
	
	/* Set dirty bit for removable devices if not ready - sometimes drives
	 * will not report this properly. */
	if(rscsi_disks[i].device->removable && 
	   SCpnt->sense_buffer[2] == NOT_READY)
	    rscsi_disks[i].device->changed = 1;
	
    }
    else
    {
	/*
	 * FLOPTICAL , if read_capa is ok , drive is assumed to be ready 
	 */
	rscsi_disks[i].ready = 1;

	rscsi_disks[i].capacity = (buffer[0] << 24) |
	    (buffer[1] << 16) |
		(buffer[2] << 8) |
		    buffer[3];
	
	rscsi_disks[i].sector_size = (buffer[4] << 24) |
	    (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];

	if (rscsi_disks[i].sector_size == 0) {
	  rscsi_disks[i].sector_size = 512;
	  printk("sd%c : sector size 0 reported, assuming 512.\n", 'a' + i);
	}
 
	
	if (rscsi_disks[i].sector_size != 512 &&
	    rscsi_disks[i].sector_size != 1024 &&
	    rscsi_disks[i].sector_size != 256)
	{
	    printk ("sd%c : unsupported sector size %d.\n",
		    'a' + i, rscsi_disks[i].sector_size);
	    if(rscsi_disks[i].device->removable){
		rscsi_disks[i].capacity = 0;
	    } else {
		printk ("scsi : deleting disk entry.\n");
		rscsi_disks[i].device = NULL;
		sd_template.nr_dev--;
		return i;
	    };
	}
    {
	/*
	 * The msdos fs need to know the hardware sector size
	 * So I have created this table. See ll_rw_blk.c
	 * Jacques Gelinas (Jacques@solucorp.qc.ca)
	 */
	int m;
	int hard_sector = rscsi_disks[i].sector_size;
	/* There is 16 minor allocated for each devices */
	for (m=i<<4; m<((i+1)<<4); m++){
	    sd_hardsizes[m] = hard_sector;
	}
	printk ("SCSI Hardware sector size is %d bytes on device sd%c\n",
		hard_sector,i+'a');
    }
	if(rscsi_disks[i].sector_size == 1024)
	    rscsi_disks[i].capacity <<= 1;  /* Change into 512 byte sectors */
	if(rscsi_disks[i].sector_size == 256)
	    rscsi_disks[i].capacity >>= 1;  /* Change into 512 byte sectors */
    }
    

    /*
     * Unless otherwise specified, this is not write protected.
     */
    rscsi_disks[i].write_prot = 0;
    if ( rscsi_disks[i].device->removable && rscsi_disks[i].ready ) {
	/* FLOPTICAL */

	/* 
	 *	for removable scsi disk ( FLOPTICAL ) we have to recognise  
	 * the Write Protect Flag. This flag is kept in the Scsi_Disk struct
	 * and tested at open !
	 * Daniel Roche ( dan@lectra.fr )
	 */
	
	memset ((void *) &cmd[0], 0, 8);
	cmd[0] = MODE_SENSE;
	cmd[1] = (rscsi_disks[i].device->lun << 5) & 0xe0;
	cmd[2] = 1;	 /* page code 1 ?? */
	cmd[4] = 12;
	SCpnt->cmd_len = 0;
	SCpnt->sense_buffer[0] = 0;
	SCpnt->sense_buffer[2] = 0;

	/* same code as READCAPA !! */
	{
	    struct semaphore sem = MUTEX_LOCKED;
	    SCpnt->request.rq_status = RQ_SCSI_BUSY;  /* Mark as really busy again */
	    SCpnt->request.sem = &sem;
	    scsi_do_cmd (SCpnt,
			 (void *) cmd, (void *) buffer,
			 512, sd_init_done,  SD_TIMEOUT,
			 MAX_RETRIES);
	    down(&sem);
	}
	
	the_result = SCpnt->result;
	SCpnt->request.rq_status = RQ_INACTIVE;  /* Mark as not busy */
	wake_up(&SCpnt->device->device_wait); 
	
	if ( the_result ) {
	    printk ("sd%c: test WP failed, assume Write Protected\n",i+'a');
	    rscsi_disks[i].write_prot = 1;
	} else {
	    rscsi_disks[i].write_prot = ((buffer[2] & 0x80) != 0);
	    printk ("sd%c: Write Protect is %s\n",i+'a',
	            rscsi_disks[i].write_prot ? "on" : "off");
	}
	
    }	/* check for write protect */
 
    rscsi_disks[i].ten = 1;
    rscsi_disks[i].remap = 1;
    scsi_free(buffer, 512);
    return i;
}

/*
 * The sd_init() function looks at all SCSI drives present, determines
 * their size, and reads partition table entries for them.
 */

static int sd_registered = 0;

static int sd_init()
{
    int i;
    
    if (sd_template.dev_noticed == 0) return 0;
    
    if(!sd_registered) {
	  if (register_blkdev(MAJOR_NR,"sd",&sd_fops)) {
	      printk("Unable to get major %d for SCSI disk\n",MAJOR_NR);
	      return 1;
	  }
	  sd_registered++;
      }
    
    /* We do not support attaching loadable devices yet. */
    if(rscsi_disks) return 0;
    
    sd_template.dev_max = sd_template.dev_noticed + SD_EXTRA_DEVS;
    
    rscsi_disks = (Scsi_Disk *) 
	scsi_init_malloc(sd_template.dev_max * sizeof(Scsi_Disk), GFP_ATOMIC);
    memset(rscsi_disks, 0, sd_template.dev_max * sizeof(Scsi_Disk));
    
    sd_sizes = (int *) scsi_init_malloc((sd_template.dev_max << 4) * 
					sizeof(int), GFP_ATOMIC);
    memset(sd_sizes, 0, (sd_template.dev_max << 4) * sizeof(int));
    
    sd_blocksizes = (int *) scsi_init_malloc((sd_template.dev_max << 4) * 
					     sizeof(int), GFP_ATOMIC);
    
    sd_hardsizes = (int *) scsi_init_malloc((sd_template.dev_max << 4) * 
					    sizeof(int), GFP_ATOMIC);
    
    for(i=0;i<(sd_template.dev_max << 4);i++){
	sd_blocksizes[i] = 1024;
	sd_hardsizes[i] = 512;
    }
    blksize_size[MAJOR_NR] = sd_blocksizes;
    hardsect_size[MAJOR_NR] = sd_hardsizes;
    sd = (struct hd_struct *) scsi_init_malloc((sd_template.dev_max << 4) *
					       sizeof(struct hd_struct),
					       GFP_ATOMIC);
    
    
    sd_gendisk.max_nr = sd_template.dev_max;
    sd_gendisk.part = sd;
    sd_gendisk.sizes = sd_sizes;
    sd_gendisk.real_devices = (void *) rscsi_disks;
    return 0;
}

static void sd_finish()
{
    int i;

    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    
    sd_gendisk.next = gendisk_head;
    gendisk_head = &sd_gendisk;
    
    for (i = 0; i < sd_template.dev_max; ++i)
	if (!rscsi_disks[i].capacity && 
	    rscsi_disks[i].device)
	{
	    if (MODULE_FLAG
		&& !rscsi_disks[i].has_part_table) {
		sd_sizes[i << 4] = rscsi_disks[i].capacity;
		/* revalidate does sd_init_onedisk via MAYBE_REINIT*/
		revalidate_scsidisk(MKDEV(MAJOR_NR, i << 4), 0);
	    }
	    else
	    	i=sd_init_onedisk(i);
	    rscsi_disks[i].has_part_table = 1;
	}
    
    /* If our host adapter is capable of scatter-gather, then we increase
     * the read-ahead to 16 blocks (32 sectors).  If not, we use
     * a two block (4 sector) read ahead. 
     */
    if(rscsi_disks[0].device && rscsi_disks[0].device->host->sg_tablesize)
	read_ahead[MAJOR_NR] = 120;  /* 120 sector read-ahead */
    else
	read_ahead[MAJOR_NR] = 4;  /* 4 sector read-ahead */

    return;
}

static int sd_detect(Scsi_Device * SDp){
    if(SDp->type != TYPE_DISK && SDp->type != TYPE_MOD) return 0;
    
    printk("Detected scsi disk sd%c at scsi%d, channel %d, id %d, lun %d\n", 
	   'a'+ (sd_template.dev_noticed++),
	   SDp->host->host_no, SDp->channel, SDp->id, SDp->lun); 
    
    return 1;
}

static int sd_attach(Scsi_Device * SDp){
    Scsi_Disk * dpnt;
    int i;
    
    if(SDp->type != TYPE_DISK && SDp->type != TYPE_MOD) return 0;
    
    if(sd_template.nr_dev >= sd_template.dev_max) {
	SDp->attached--;
	return 1;
    }
    
    for(dpnt = rscsi_disks, i=0; i<sd_template.dev_max; i++, dpnt++) 
	if(!dpnt->device) break;
    
    if(i >= sd_template.dev_max) panic ("scsi_devices corrupt (sd)");
    
    SDp->scsi_request_fn = do_sd_request;
    rscsi_disks[i].device = SDp;
    rscsi_disks[i].has_part_table = 0;
    sd_template.nr_dev++;
    sd_gendisk.nr_real++;
    return 0;
}

#define DEVICE_BUSY rscsi_disks[target].device->busy
#define USAGE rscsi_disks[target].device->access_count
#define CAPACITY rscsi_disks[target].capacity
#define MAYBE_REINIT  sd_init_onedisk(target)
#define GENDISK_STRUCT sd_gendisk

/* This routine is called to flush all partitions and partition tables
 * for a changed scsi disk, and then re-read the new partition table.
 * If we are revalidating a disk because of a media change, then we
 * enter with usage == 0.  If we are using an ioctl, we automatically have
 * usage == 1 (we need an open channel to use an ioctl :-), so this
 * is our limit.
 */
int revalidate_scsidisk(kdev_t dev, int maxusage){
    int target;
    struct gendisk * gdev;
    unsigned long flags;
    int max_p;
    int start;
    int i;
    
    target =  DEVICE_NR(dev);
    gdev = &GENDISK_STRUCT;
    
    save_flags(flags);
    cli();
    if (DEVICE_BUSY || USAGE > maxusage) {
	restore_flags(flags);
	printk("Device busy for revalidation (usage=%d)\n", USAGE);
	return -EBUSY;
    };
    DEVICE_BUSY = 1;
    restore_flags(flags);
    
    max_p = gdev->max_p;
    start = target << gdev->minor_shift;
    
    for (i=max_p - 1; i >=0 ; i--) {
	int minor = start+i;
	kdev_t devi = MKDEV(MAJOR_NR, minor);
	sync_dev(devi);
	invalidate_inodes(devi);
	invalidate_buffers(devi);
	gdev->part[minor].start_sect = 0;
	gdev->part[minor].nr_sects = 0;
        /*
         * Reset the blocksize for everything so that we can read
         * the partition table.
         */
        blksize_size[MAJOR_NR][minor] = 1024;
    };
    
#ifdef MAYBE_REINIT
    MAYBE_REINIT;
#endif
    
    gdev->part[start].nr_sects = CAPACITY;
    resetup_one_dev(gdev, target);
    
    DEVICE_BUSY = 0;
    return 0;
}

static int fop_revalidate_scsidisk(kdev_t dev){
    return revalidate_scsidisk(dev, 0);
}


static void sd_detach(Scsi_Device * SDp)
{
    Scsi_Disk * dpnt;
    int i;
    int max_p;
    int start;
    
    for(dpnt = rscsi_disks, i=0; i<sd_template.dev_max; i++, dpnt++) 
	if(dpnt->device == SDp) {
	    
	    /* If we are disconnecting a disk driver, sync and invalidate 
	     * everything */
	    max_p = sd_gendisk.max_p;
	    start = i << sd_gendisk.minor_shift;
	    
	    for (i=max_p - 1; i >=0 ; i--) {
		int minor = start+i;
		kdev_t devi = MKDEV(MAJOR_NR, minor);
		sync_dev(devi);
		invalidate_inodes(devi);
		invalidate_buffers(devi);
		sd_gendisk.part[minor].start_sect = 0;
		sd_gendisk.part[minor].nr_sects = 0;
		sd_sizes[minor] = 0;
	    };
	    
	    dpnt->has_part_table = 0;
	    dpnt->device = NULL;
	    dpnt->capacity = 0;
	    SDp->attached--;
	    sd_template.dev_noticed--;
	    sd_template.nr_dev--;
	    sd_gendisk.nr_real--;
	    return;
	}
    return;
}

#ifdef MODULE

int init_module(void) {
    sd_template.usage_count = &mod_use_count_;
    return scsi_register_module(MODULE_SCSI_DEV, &sd_template);
}

void cleanup_module( void) 
{
    struct gendisk * prev_sdgd;
    struct gendisk * sdgd;
    
    scsi_unregister_module(MODULE_SCSI_DEV, &sd_template);
    unregister_blkdev(SCSI_DISK_MAJOR, "sd");
    sd_registered--;
    if( rscsi_disks != NULL )
    {
	scsi_init_free((char *) rscsi_disks,
		       (sd_template.dev_noticed + SD_EXTRA_DEVS) 
		       * sizeof(Scsi_Disk));
	
	scsi_init_free((char *) sd_sizes, sd_template.dev_max * sizeof(int));
	scsi_init_free((char *) sd_blocksizes, sd_template.dev_max * sizeof(int));
	scsi_init_free((char *) sd_hardsizes, sd_template.dev_max * sizeof(int));
	scsi_init_free((char *) sd, 
		       (sd_template.dev_max << 4) * sizeof(struct hd_struct));
	/*
	 * Now remove sd_gendisk from the linked list
	 */
	sdgd = gendisk_head;
	prev_sdgd = NULL;
	while(sdgd != &sd_gendisk)
	{
	    prev_sdgd = sdgd;
	    sdgd = sdgd->next;
	}
	
	if(sdgd != &sd_gendisk)
	    printk("sd_gendisk not in disk chain.\n");
	else {
	    if(prev_sdgd != NULL)
		prev_sdgd->next = sdgd->next;
	    else
		gendisk_head = sdgd->next;
	}
    }
    
    blksize_size[MAJOR_NR] = NULL;
    blk_dev[MAJOR_NR].request_fn = NULL;
    blk_size[MAJOR_NR] = NULL;  
    hardsect_size[MAJOR_NR] = NULL;
    read_ahead[MAJOR_NR] = 0;
    sd_template.dev_max = 0;
}
#endif /* MODULE */

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
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
