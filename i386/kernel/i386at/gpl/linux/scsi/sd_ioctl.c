/*
 * drivers/scsi/sd_ioctl.c
 *
 * ioctl handling for SCSI disks
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/errno.h>

#include <asm/segment.h>

#include <linux/blk.h>
#include "scsi.h"
#include "scsi_ioctl.h"
#include "hosts.h"
#include "sd.h"

int sd_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
    kdev_t dev = inode->i_rdev;
    int error;
    struct Scsi_Host * host;
    int diskinfo[4];
    struct hd_geometry *loc = (struct hd_geometry *) arg;
    
    switch (cmd) {
    case HDIO_GETGEO:   /* Return BIOS disk parameters */
	if (!loc)  return -EINVAL;
	error = verify_area(VERIFY_WRITE, loc, sizeof(*loc));
	if (error)
	    return error;
	host = rscsi_disks[MINOR(dev) >> 4].device->host;
	diskinfo[0] = 0;
	diskinfo[1] = 0;
	diskinfo[2] = 0;
	if(host->hostt->bios_param != NULL)
	    host->hostt->bios_param(&rscsi_disks[MINOR(dev) >> 4],
				    dev,
				    &diskinfo[0]);
	put_user(diskinfo[0], &loc->heads);
	put_user(diskinfo[1], &loc->sectors);
	put_user(diskinfo[2], &loc->cylinders);
	put_user(sd[MINOR(inode->i_rdev)].start_sect, &loc->start);
	return 0;
    case BLKGETSIZE:   /* Return device size */
	if (!arg)  return -EINVAL;
	error = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
	if (error)
	    return error;
	put_user(sd[MINOR(inode->i_rdev)].nr_sects,
		 (long *) arg);
	return 0;
    case BLKRASET:
	if(!suser())  return -EACCES;
	if(!(inode->i_rdev)) return -EINVAL;
	if(arg > 0xff) return -EINVAL;
	read_ahead[MAJOR(inode->i_rdev)] = arg;
	return 0;
    case BLKFLSBUF:
	if(!suser())  return -EACCES;
	if(!(inode->i_rdev)) return -EINVAL;
	fsync_dev(inode->i_rdev);
	invalidate_buffers(inode->i_rdev);
	return 0;
	
    case BLKRRPART: /* Re-read partition tables */
	return revalidate_scsidisk(dev, 1);
    default:
	return scsi_ioctl(rscsi_disks[MINOR(dev) >> 4].device , cmd, (void *) arg);
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
