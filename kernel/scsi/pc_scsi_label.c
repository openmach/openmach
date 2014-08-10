/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989 Carnegie Mellon University
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
/* This goes away as soon as we move it in the Ux server */



#include <mach/std_types.h>
#include <scsi/compat_30.h>
#include <scsi/scsi.h>
#include <scsi/scsi_defs.h>
#include <scsi/rz.h>
#include <scsi/rz_labels.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#if (NSCSI > 0)
#define LABEL_DEBUG(x,y) if (label_flag&x) y

#include <i386at/disk.h>
#include <device/device_types.h>
#include <device/disk_status.h>


int scsi_abs_sec = -1;
int scsi_abs_count = -1;

scsi_rw_abs(dev, data, rw, sec, count)
	dev_t		dev;
{
	io_req_t	ior;
	io_return_t	error;

	io_req_alloc(ior,0);
	ior->io_next = 0;
	ior->io_unit = dev & (~(MAXPARTITIONS-1));	/* sort of */
	ior->io_unit |= PARTITION_ABSOLUTE;
	ior->io_data = (io_buf_ptr_t)data;
	ior->io_count = count;
	ior->io_recnum = sec;
	ior->io_error = 0;
	if (rw == IO_READ)
		ior->io_op = IO_READ;
	else
		ior->io_op = IO_WRITE;
	scdisk_strategy(ior);
	iowait(ior);
	error = ior->io_error;
	io_req_free(ior);
	return(error);
}

io_return_t
scsi_i386_get_status(dev, tgt, flavor, status, status_count)
int		dev;
target_info_t	*tgt;
int		flavor;
dev_status_t	status;
unsigned int	*status_count;
{

	switch (flavor) {
	case V_GETPARMS: {
		struct disklabel *lp = &tgt->dev_info.disk.l;
		struct disk_parms *dp = (struct disk_parms *)status;
		extern struct disklabel default_label;
		int part = rzpartition(dev);

		if (*status_count < sizeof (struct disk_parms)/sizeof(int))
			return (D_INVALID_OPERATION);
		dp->dp_type = DPT_WINI; 
		dp->dp_secsiz = lp->d_secsize;
		if (lp->d_nsectors == default_label.d_nsectors &&
		    lp->d_ntracks == default_label.d_ntracks &&
		    lp->d_ncylinders == default_label.d_ncylinders) {
		    	/* I guess there is nothing there */
			/* Well, then, Adaptec's like ... */
			dp->dp_sectors = 32;
			dp->dp_heads = 64;
			dp->dp_cyls = lp->d_secperunit / 64 / 32 ;
		} else {
			dp->dp_sectors = lp->d_nsectors;
			dp->dp_heads = lp->d_ntracks;
			dp->dp_cyls = lp->d_ncylinders;
		}

		dp->dp_dossectors = 32;
		dp->dp_dosheads = 64;
		dp->dp_doscyls = lp->d_secperunit / 64 / 32;
		dp->dp_ptag = 0;
		dp->dp_pflag = 0;
/* !!! partition changes */
printf("USING PARTIOION TABLE\n");
		dp->dp_pstartsec = lp->d_partitions[part].p_offset;
		dp->dp_pnumsec = lp->d_partitions[part].p_size;
		*status_count = sizeof(struct disk_parms)/sizeof(int);
		break;
	}
	case V_RDABS:
		if (*status_count < DEV_BSIZE/sizeof (int)) {
			printf("RDABS bad size %x", *status_count);
			return (D_INVALID_OPERATION);
		}
		if (scsi_rw_abs(dev, status, IO_READ, scsi_abs_sec, DEV_BSIZE) != D_SUCCESS)
			return(D_INVALID_OPERATION);
		*status_count = DEV_BSIZE/sizeof(int);
		break;
	case V_VERIFY: {
		int count = scsi_abs_count * DEV_BSIZE;
		int sec = scsi_abs_sec;
		char *scsi_verify_buf;
#include "vm/vm_kern.h"

		(void) kmem_alloc(kernel_map, &scsi_verify_buf, PAGE_SIZE);

		*status = 0;
		while (count > 0) {
			int xcount = (count < PAGE_SIZE) ? count : PAGE_SIZE;
			if (scsi_rw_abs(dev, scsi_verify_buf, IO_READ, sec, xcount) != D_SUCCESS) {
				*status = BAD_BLK;
				break;
			} else {
				count -= xcount;
				sec += xcount / DEV_BSIZE;
			}
	        }
		(void) kmem_free(kernel_map, scsi_verify_buf, PAGE_SIZE);
		*status_count = 1;
		break;
	}
	default:
		return(D_INVALID_OPERATION);
	}
	return D_SUCCESS;
}

io_return_t
scsi_i386_set_status(dev, tgt, flavor, status, status_count)
int		dev;
target_info_t	*tgt;
int		flavor;
int 		*status;
unsigned int	status_count;
{
	io_req_t	ior;

	switch (flavor) {
	case V_SETPARMS:
		printf("scsdisk_set_status: invalid flavor V_SETPARMS\n");
		return(D_INVALID_OPERATION);
		break;
	case V_REMOUNT:
		tgt->flags &= ~TGT_ONLINE;
		break;
	case V_ABS:
		scsi_abs_sec = status[0];
		if (status_count == 2)
			scsi_abs_count = status[1];
		break;
	case V_WRABS:
		if (status_count < DEV_BSIZE/sizeof (int)) {
			printf("RDABS bad size %x", status_count);
			return (D_INVALID_OPERATION);
		}
		if (scsi_rw_abs(dev, status, IO_WRITE, scsi_abs_sec, DEV_BSIZE) != D_SUCCESS)
			return(D_INVALID_OPERATION);
		break;
	default:
		return(D_INVALID_OPERATION);
	}
	return D_SUCCESS;
}
#endif  /* NSCSI > 0 */

