/* 
 * Copyright (c) 1994 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * MODIFIED BY KEVIN T. VAN MAREN, University of Utah, CSL
 * Copyright (c) 1996, University of Utah, CSL
 *
 * Uses a 'unified' partition code with the SCSI driver.
 * Reading/Writing disklabels through the kernel is NOT recommended.
 * (The preferred method is through the raw device (wd0), with no
 * open partitions).  setdisklabel() should work for the in-core
 * fudged disklabel, but will not change the partitioning. The driver
 * *never* sees the disklabel on the disk.
 *
 */


#include <hd.h>
#if NHD > 0 && !defined(LINUX_DEV)
/*
 * Hard disk driver.
 *
 * Supports:
 * 1 controller and 2 drives.
 * Arbitrarily sized read/write requests.
 * Misaligned requests.
 * Multiple sector transfer mode (not tested extensively).
 *
 * TODO:
 * 1) Real probe routines for controller and drives.
 * 2) Support for multiple controllers.  The driver does
 * not assume a single controller since all functions
 * take the controller and/or device structure as an
 * argument, however the probe routines limit the
 * number of controllers and drives to 1 and 2 respectively.
 *
 * Shantanu Goel (goel@cs.columbia.edu)
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include "vm_param.h"
#include <kern/time_out.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <device/param.h>
#include <device/buf.h>
#include <device/errno.h>
#include <device/device_types.h>
#include <device/disk_status.h>
#include <chips/busses.h>
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/cram.h>
#include <i386at/disk.h>
#include <i386at/nhdreg.h>

#include <scsi/rz_labels.h>


/* this is for the partition code */
typedef struct ide_driver_info {
	dev_t dev;
/*	struct buf *bp; */
	int sectorsize;
} ide_driver_info;

#define MAX_IDE_PARTS 32   /* max partitions per drive */
static char *drive_name[4]={"wd0: ","wd1: ","xxx ","yyy "};

/*
 * XXX: This will have to be fixed for controllers that
 * can support upto 4 drives.
 */
#define NDRIVES_PER_HDC	2
#define NHDC		((NHD + NDRIVES_PER_HDC - 1) / NDRIVES_PER_HDC)

#define b_cylin		b_resid

#define B_ABS		B_MD1
#define B_IDENTIFY	(B_MD1 << 1)

/* shift right SLICE_BITS + PARTITION_BITS. Note: 2^10 = 1024 sub-parts */
#define hdunit(dev)	(((dev) >> 10) & 3)
#define hdpart(dev)	((dev) & 0x3ff)

#define MAX_RETRIES	12	/* maximum number of retries */
#define OP_TIMEOUT	7	/* time to wait (secs) for an operation */

/*
 * Autoconfiguration stuff.
 */
struct	bus_ctlr *hdminfo[NHDC];
struct	bus_device *hddinfo[NHD];
int	hdstd[] = { 0 };
int	hdprobe(), hdslave(), hdstrategy();
void	hdattach();
struct	bus_driver hddriver = {
	hdprobe, hdslave, hdattach, 0, hdstd, "hd", hddinfo, "hdc", hdminfo, 0
};

/*
 * BIOS geometry.
 */
struct hdbios {
	int	bg_ncyl;	/* cylinders/unit */
	int	bg_ntrk;	/* tracks/cylinder */
	int	bg_precomp;	/* write precomp cylinder */
	int	bg_nsect;	/* sectors/track */
} hdbios[NHD];

/*
 * Controller state.
 */
struct hdcsoftc {
	int	sc_state;	/* transfer fsm */
	caddr_t	sc_addr;	/* buffer address */
	int	sc_resid;	/* amount left to transfer */
	int	sc_amt;		/* amount currently being transferred */
	int	sc_cnt;		/* amount transferred per interrupt */
	int	sc_sn;		/* sector number */
	int	sc_tn;		/* track number */
	int	sc_cn;		/* cylinder number */
	int	sc_recalerr;	/* # recalibration errors */
	int	sc_ioerr;	/* # i/o errors */
	int	sc_wticks;	/* watchdog */
	caddr_t	sc_buf;		/* buffer for unaligned requests */
} hdcsoftc[NHDC];

/*
 * Transfer states.
 */
#define IDLE		0	/* controller is idle */
#define SETPARAM	1	/* set disk parameters */
#define SETPARAMDONE	2	/* set parameters done */
#define RESTORE		3	/* recalibrate drive */
#define RESTOREDONE	4	/* recalibrate done */
#define TRANSFER	5	/* perform I/O transfer */
#define TRANSFERDONE	6	/* transfer done */
#define IDENTIFY	7	/* get drive info */
#define IDENTIFYDONE	8	/* get drive info done */
#define SETMULTI	9	/* set multiple mode count */
#define SETMULTIDONE	10	/* set multiple mode count done */

/*
 * Drive state.
 */
struct hdsoftc {
	int	sc_flags;
#define HDF_SETPARAM	0x001	/* set drive parameters before I/O operation */
#define HDF_RESTORE	0x002	/* drive needs recalibration */
#define HDF_WANT	0x004	/* some one is waiting for drive */
#define HDF_UNALIGNED	0x008	/* request is not a multiple of sector size */
#define HDF_SETMULTI	0x010	/* set multiple count before I/O operation */
#define HDF_MULTIDONE	0x020	/* multicnt field is valid */
#define HDF_IDENTDONE	0x040	/* identify command done */
#define HDF_LBA		0x080	/* use LBA mode */
	int	sc_multicnt;	/* current multiple count */
	int	sc_abssn;	/* absolute sector number (for {RD,WR}ABS) */
	int	sc_abscnt;	/* absolute sector count */
	int	sc_openpart;	/* bit mask of open partitions */
	struct	hdident sc_id;	/* info returned by identify */
} hdsoftc[NHD];

struct	buf hdtab[NHDC];	/* controller queues */
struct	buf hdutab[NHD];	/* drive queues */
struct	disklabel hdlabel[NHD];	/* disklabels -- incorrect info! */
struct  diskpart array[NHD*MAX_IDE_PARTS]; /* partition info */

/*
 * To enable multiple mode,
 * set this, recompile, and reboot the machine.
 */
int	hdenmulti = 0;

char	*hderrchk();
struct	buf *geteblk();
int	hdwstart = 0;
void	hdwatch();

/*
 * Probe for a controller.
 */
int
hdprobe(xxx, um)
	int xxx;
	struct bus_ctlr *um;
{
	struct hdcsoftc *hdc;

	if (um->unit >= NHDC) {
		printf("hdc%d: not configured\n", um->unit);
		return (0);
	}
	if (um->unit > 0) {	/* XXX: only 1 controller */

                printf("nhd:probe for 2+ controllers -- not implemented\n");
                return (0);
        }

	/*
	 * XXX: need real probe
	*/
	hdc = &hdcsoftc[um->unit];
	if (!hdc->sc_buf)
		kmem_alloc(kernel_map,
			   (vm_offset_t *)&hdc->sc_buf, I386_PGBYTES);
	take_ctlr_irq(um);
	return (1);
}

/*
 * Probe for a drive.
 */
int
hdslave(ui)
	struct bus_device *ui;
{
	int type;

	if (ui->unit >= NHD) {
		printf("hd%d: not configured\n", ui->unit);
		return (0);
	}
	if (ui->unit > 1)	/* XXX: only 2 drives */
		return (0);

	/*
	 * Find out if drive exists by reading CMOS.
	 */
	outb(CMOS_ADDR, 0x12);
	type = inb(CMOS_DATA);
	if (ui->unit == 0)
		type >>= 4;
	type &= 0x0f;
	return (type);
}

/*
 * Attach a drive to the system.
 */
void
hdattach(ui)
	struct bus_device *ui;
{
	char *tbl;
	unsigned n;
	/* struct hdsoftc *sc = &hdsoftc[ui->unit]; */
	struct disklabel *lp = &hdlabel[ui->unit];
	struct hdbios *bg = &hdbios[ui->unit];

	/*
	 * Set up a temporary disklabel from BIOS parameters.
	 * The actual partition table will be read during open.
	 */
	n = *(unsigned *)phystokv(ui->address);
	tbl = (unsigned char *)phystokv((n & 0xffff) + ((n >> 12) & 0xffff0));
	bg->bg_ncyl = *(unsigned short *)tbl;
	bg->bg_ntrk = *(unsigned char *)(tbl + 2);
	bg->bg_precomp = *(unsigned short *)(tbl + 5);
	bg->bg_nsect = *(unsigned char *)(tbl + 14);
	fudge_bsd_label(lp, DTYPE_ESDI, bg->bg_ncyl*bg->bg_ntrk*bg->bg_nsect,
			bg->bg_ntrk, bg->bg_nsect, SECSIZE, 3);  

	/* FORCE sector size to 512... */

	printf(": ntrak(heads) %d, ncyl %d, nsec %d, size %u MB",
	       lp->d_ntracks, lp->d_ncylinders, lp->d_nsectors,
	       lp->d_secperunit * lp->d_secsize / (1024*1024));
}

int
hdopen(dev, mode)
	dev_t dev;
	int mode;
{
	int unit = hdunit(dev), part = hdpart(dev) /*, error */;
	struct bus_device *ui;
	struct hdsoftc *sc;
	struct diskpart *label;

	if (unit >= NHD || (ui = hddinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);
	if (!hdwstart) {
		hdwstart++;
		timeout(hdwatch, 0, hz);
	}
	sc = &hdsoftc[unit];
	/* should this be changed so only gets called once, even if all 
	   partitions are closed and re-opened? */
	if (sc->sc_openpart == 0) {
		hdinit(dev);
		if (sc->sc_flags & HDF_LBA)
			printf("hd%d: Using LBA mode\n", ui->unit);
	}

/* Note: should set a bit in the label structure to ensure that
   aliasing prevents multiple instances to be opened. */
#if 0
	if (part >= MAXPARTITIONS || lp->d_partitions[part].p_size == 0)
		return (ENXIO);
#endif 0

	label=lookup_part(&array[MAX_IDE_PARTS*unit], hdpart(dev));
	if (!label)
		return (ENXIO);


	sc->sc_openpart |= 1 << part;
	return (0);
}

int
hdclose(dev)
	dev_t dev;
{
	int unit = hdunit(dev), s;
	struct hdsoftc *sc = &hdsoftc[unit];

	sc->sc_openpart &= ~(1 << hdpart(dev));
	if (sc->sc_openpart == 0) {
		s = splbio();
		while (hdutab[unit].b_active) {
			sc->sc_flags |= HDF_WANT;
			assert_wait((event_t)sc, FALSE);
			thread_block((void (*)())0);
		}
		splx(s);
	}
	return (0);
}

int
hdread(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	return (block_io(hdstrategy, minphys, ior));
}

int
hdwrite(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	return (block_io(hdstrategy, minphys, ior));
}

int
hdgetstat(dev, flavor, data, count)
	dev_t dev;
	dev_flavor_t flavor;
	dev_status_t data;
	mach_msg_type_number_t *count;
{
	int unit = hdunit(dev), part = hdpart(dev);
	struct hdsoftc *sc = &hdsoftc[unit];
	struct disklabel *lp = &hdlabel[unit];
	struct buf *bp;
	struct diskpart *label;

	label=lookup_part(&array[MAX_IDE_PARTS*unit], hdpart(dev));
	switch (flavor) {

	case DEV_GET_SIZE:
		if (label) {
			data[DEV_GET_SIZE_DEVICE_SIZE] = (label->size * lp->d_secsize);
			data[DEV_GET_SIZE_RECORD_SIZE] = lp->d_secsize;
			*count = DEV_GET_SIZE_COUNT;
		} else {  /* Kevin: added checking here */
			data[DEV_GET_SIZE_DEVICE_SIZE] = 0;
			data[DEV_GET_SIZE_RECORD_SIZE] = 0;
			*count = 0;
		}
		break;

	case DIOCGDINFO:
	case DIOCGDINFO - (0x10 << 16):
		dkgetlabel(lp, flavor, data, count);
		break;

	case V_GETPARMS:
	{
		struct disk_parms *dp;
		struct hdbios *bg = &hdbios[unit];

		if (*count < (sizeof(struct disk_parms) / sizeof(int)))
			return (D_INVALID_OPERATION);
		dp = (struct disk_parms *)data;
		dp->dp_type = DPT_WINI;
		dp->dp_heads = lp->d_ntracks;
		dp->dp_cyls = lp->d_ncylinders;
		dp->dp_sectors = lp->d_nsectors;
		dp->dp_dosheads = bg->bg_ntrk;
		dp->dp_doscyls = bg->bg_ncyl;
		dp->dp_dossectors = bg->bg_nsect;
		dp->dp_secsiz = lp->d_secsize;
		dp->dp_ptag = 0;
		dp->dp_pflag = 0;
		if (label) {
			dp->dp_pstartsec = label->start;
			dp->dp_pnumsec = label->size; 
		} else { /* added by Kevin */
			dp->dp_pstartsec = -1;
			dp->dp_pnumsec = -1;
		}

		*count = sizeof(struct disk_parms) / sizeof(int);
		break;
	}
	case V_RDABS:
		if (*count < lp->d_secsize / sizeof(int)) {
			printf("hd%d: RDABS, bad size %d\n", unit, *count);
			return (EINVAL);
		}
		bp = geteblk(lp->d_secsize);
		bp->b_flags = B_READ | B_ABS;
		bp->b_blkno = sc->sc_abssn;
		bp->b_dev = dev;
		bp->b_bcount = lp->d_secsize;
		hdstrategy(bp);
		biowait(bp);
		if (bp->b_flags & B_ERROR) {
			printf("hd%d: RDABS failed\n", unit);
			brelse(bp);
			return (EIO);
		}
		bcopy(bp->b_un.b_addr, (caddr_t)data, lp->d_secsize);
		brelse(bp);
		*count = lp->d_secsize / sizeof(int);
		break;

	case V_VERIFY:
	{
		int i, amt, n, error = 0;

		bp = geteblk(I386_PGBYTES);
		bp->b_blkno = sc->sc_abssn;
		bp->b_dev = dev;
		amt = sc->sc_abscnt;
		n = I386_PGBYTES / lp->d_secsize;
		while (amt > 0) {
			i = (amt > n) ? n : amt;
			bp->b_bcount = i * lp->d_secsize;
			bp->b_flags = B_READ | B_ABS;
			hdstrategy(bp);
			biowait(bp);
			if (bp->b_flags & B_ERROR) {
				error = BAD_BLK;
				break;
			}
			amt -= bp->b_bcount;
			bp->b_blkno += i;
		}
		brelse(bp);
		data[0] = error;
		*count = 1;
		break;
	}
	default:
		return (D_INVALID_OPERATION);
	}
	return (0);
}

int
hdsetstat(dev, flavor, data, count)
	dev_t dev;
	dev_flavor_t flavor;
	dev_status_t data;
	mach_msg_type_number_t count;
{
	int unit = hdunit(dev); /* , part = hdpart(dev); */
	int error = 0 /*, s */;
	struct hdsoftc *sc = &hdsoftc[unit];
	struct disklabel *lp = &hdlabel[unit];
	struct buf *bp;

	switch (flavor) {

	case DIOCWLABEL:
	case DIOCWLABEL - (0x10 << 16):
		break;

	case DIOCSDINFO:
	case DIOCSDINFO - (0x10 << 16):
		if (count != (sizeof(struct disklabel) / sizeof(int)))
			return (D_INVALID_SIZE);
		error = setdisklabel(lp, (struct disklabel *)data);
		if (error == 0 && (sc->sc_flags & HDF_LBA) == 0)
			sc->sc_flags |= HDF_SETPARAM;
		break;

	case DIOCWDINFO:
	case DIOCWDINFO - (0x10 << 16):
		if (count != (sizeof(struct disklabel) / sizeof(int)))
			return (D_INVALID_SIZE);
		error = setdisklabel(lp, (struct disklabel *)data);
		if (error == 0) {
			if ((sc->sc_flags & HDF_LBA) == 0)
				sc->sc_flags |= HDF_SETPARAM;
			error = hdwritelabel(dev);
		}
		break;

	case V_REMOUNT:
		hdinit(dev);
		break;

	case V_ABS:
		if (count != 1 && count != 2)
			return (D_INVALID_OPERATION);
		sc->sc_abssn = *(int *)data;
		if (sc->sc_abssn < 0 || sc->sc_abssn >= lp->d_secperunit)
			return (D_INVALID_OPERATION);
		if (count == 2)
			sc->sc_abscnt = *((int *)data + 1);
		else
			sc->sc_abscnt = 1;
		if (sc->sc_abscnt <= 0
		    || sc->sc_abssn + sc->sc_abscnt > lp->d_secperunit)
			return (D_INVALID_OPERATION);
		break;

	case V_WRABS:
		if (count < (lp->d_secsize / sizeof(int))) {
			printf("hd%d: WRABS, bad size %d\n", unit, count);
			return (D_INVALID_OPERATION);
		}
		bp = geteblk(lp->d_secsize);
		bcopy((caddr_t)data, bp->b_un.b_addr, lp->d_secsize);
		bp->b_flags = B_WRITE | B_ABS;
		bp->b_blkno = sc->sc_abssn;
		bp->b_bcount = lp->d_secsize;
		bp->b_dev = dev;
		hdstrategy(bp);
		biowait(bp);
		if (bp->b_flags & B_ERROR) {
			printf("hd%d: WRABS failed\n", unit);
			error = EIO;
		}
		brelse(bp);
		break;

	default:
		return (D_INVALID_OPERATION);
	}
	return (error);
}

int
hddevinfo(dev, flavor, info)
	dev_t dev;
	int flavor;
	char *info;
{
	switch (flavor) {

	case D_INFO_BLOCK_SIZE:
		*((int *)info) = SECSIZE;  /* #defined to 512 */
		break;

	default:
		return (D_INVALID_OPERATION);
	}
	return (0);
}




/* Kevin T. Van Maren: Added this low-level routine for the unified 
   partition code. A pointer to this routine is passed, along with param* */
int 
ide_read_fun(struct ide_driver_info *param, int sectornum, char *buff) 
{
	struct buf *bp;

	bp = geteblk(param->sectorsize);
	bp->b_flags =  B_READ | B_ABS;

        bp->b_bcount = param->sectorsize;
        bp->b_blkno = sectornum;

	/* WARNING: DEPENDS ON NUMBER OF BITS FOR PARTITIONS */
	bp->b_dev = param->dev & ~0x3ff;
	hdstrategy(bp);
	biowait(bp);
	if ((bp->b_flags & B_ERROR) == 0) 
		bcopy((char *)bp->b_un.b_addr, buff, param->sectorsize);
	else {
		printf("ERROR!\n");
		return(B_ERROR);
	}

	brelse(bp);
	return(0);
}



/*
 * Initialize drive.
 */
int
hdinit(dev)
	dev_t dev;
{
	int unit = hdunit(dev);
	struct hdsoftc *sc = &hdsoftc[unit];
	struct disklabel *lp = &hdlabel[unit], *dlp;
	struct buf *bp = 0;
	int numpart;

        struct ide_driver_info ide_param = { dev, /* bp, */ lp->d_secsize };
	int ret;

	/*
	 * Issue identify command.
	 */
	if ((sc->sc_flags & HDF_IDENTDONE) == 0) {
		sc->sc_flags |= HDF_IDENTDONE;
		bp = geteblk(lp->d_secsize);
		/* sector size #defined to 512 */
		bp->b_flags = B_IDENTIFY;
		bp->b_dev = dev;
		hdstrategy(bp);
		biowait(bp);
		if ((bp->b_flags & B_ERROR) == 0) {
			bcopy((char *)bp->b_un.b_addr,
			      (char *)&sc->sc_id, sizeof(struct hdident));

			/*
			 * Check if drive supports LBA mode.
			 */
			if (sc->sc_id.id_capability & 2)
				sc->sc_flags |= HDF_LBA;
		}
	}

	/*
	 * Check if drive supports multiple read/write mode.
	 */
	hdmulti(dev);

	/* Note: label was fudged during attach! */

	/* ensure the 'raw disk' can be accessed reliably */
        array[MAX_IDE_PARTS*unit].start=0;
        array[MAX_IDE_PARTS*unit].size=lp->d_secperunit;  /* fill in root for MY reads */
#if 0
        array[MAX_IDE_PARTS*unit].subs=0;
        array[MAX_IDE_PARTS*unit].nsubs=0; 
        array[MAX_IDE_PARTS*unit].type=0;
        array[MAX_IDE_PARTS*unit].fsys=0;
#endif 0

	numpart=get_only_partition(&ide_param, (*ide_read_fun),
		&array[MAX_IDE_PARTS*unit],MAX_IDE_PARTS,lp->d_secperunit, 
		drive_name[unit]);

	printf("%s %d partitions found\n",drive_name[unit],numpart);

        if ((sc->sc_flags & HDF_LBA) == 0)
                sc->sc_flags |= HDF_SETPARAM;

        brelse(bp);
	return(ret);
}




/*
 * Check if drive supports multiple read/write mode.
 */
int
hdmulti(dev)
	dev_t dev;
{
	int unit = hdunit(dev);
	struct hdsoftc *sc = &hdsoftc[unit];
	struct buf *bp;
	struct hdident *id;

	if (sc->sc_flags & HDF_MULTIDONE)
		return(0);

	sc->sc_flags |= HDF_MULTIDONE;

	if (hdenmulti == 0)
		return(0);

	/*
	 * Get drive information by issuing IDENTIFY command.
	 */
	bp = geteblk(DEV_BSIZE);
	bp->b_flags = B_IDENTIFY;
	bp->b_dev = dev;
	hdstrategy(bp);
	biowait(bp);
	id = (struct hdident *)bp->b_un.b_addr;

	/*
	 * If controller does not recognise IDENTIFY command,
	 * or does not support multiple mode, clear count.
	 */
	if ((bp->b_flags & B_ERROR) || !id->id_multisize)
		sc->sc_multicnt = 0;
	else {
		sc->sc_multicnt = id->id_multisize;
		printf("hd%d: max multiple size %u", unit, sc->sc_multicnt);
		/*
		 * Use 4096 since it is the minimum block size in FFS.
		 */
		if (sc->sc_multicnt > 4096 / 512)
			sc->sc_multicnt = 4096 / 512;
		printf(", using %u\n", sc->sc_multicnt);
		sc->sc_flags |= HDF_SETMULTI;
	}
	brelse(bp);
}

/*
 * Write label to disk.
 */
int
hdwritelabel(dev)
	dev_t dev;
{
	int unit = hdunit(dev), error = 0;
	long labelsect;
	struct buf *bp;
	struct disklabel *lp = &hdlabel[unit];

	printf("hdwritelabel: no longer implemented\n");

#if 0
	bp = geteblk(lp->d_secsize);
	bp->b_flags = B_READ | B_ABS;
	bp->b_blkno = LBLLOC + lp->d_partitions[PART_DISK].p_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_dev = dev;
	hdstrategy(bp);
	biowait(bp);
	if (bp->b_flags & B_ERROR) {
		printf("hd%d: hdwritelabel(), error reading disklabel\n",unit);
		error = EIO;
		goto out;
	}
	*(struct disklabel *)bp->b_un.b_addr = *lp;  /* copy disk label */
	bp->b_flags = B_WRITE | B_ABS;
	hdstrategy(bp);
	biowait(bp);
	if (bp->b_flags & B_ERROR) {
		printf("hd%d: hdwritelabel(), error writing disklabel\n",unit);
		error = EIO;
	}
 out:
	brelse(bp);
#endif 0

	return (error);
}

/*
 * Strategy routine.
 * Enqueue request on drive.
 */
int
hdstrategy(bp)
	struct buf *bp;
{
	int unit = hdunit(bp->b_dev), part = hdpart(bp->b_dev), s;
	long bn, sz, maxsz;
	struct buf *dp;
	struct hdsoftc *sc = &hdsoftc[unit];
	struct bus_device *ui = hddinfo[unit];
	struct disklabel *lp = &hdlabel[unit];
	struct diskpart *label;

	if (bp->b_flags & B_IDENTIFY) {
		bp->b_cylin = 0;
		goto q;
	}
	bn = bp->b_blkno;
	if (bp->b_flags & B_ABS)
		goto q1;
	sz = (bp->b_bcount + lp->d_secsize - 1) / lp->d_secsize;
	label=lookup_part(&array[MAX_IDE_PARTS*unit], hdpart(bp->b_dev));
	if (label) {
		maxsz = label->size;
	} else {
		bp->b_flags |= B_ERROR;
		bp->b_error = EINVAL;
		goto done;
	}

	if (bn < 0 || bn + sz > maxsz) {
		if (bn == maxsz) {
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		sz = maxsz - bn;
		if (sz <= 0) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EINVAL;
			goto done;
		}
		bp->b_bcount = sz * lp->d_secsize;
	}
	bn += lp->d_partitions[part].p_offset;
	bn += label->start;

 q1:
	bp->b_cylin = (sc->sc_flags & HDF_LBA) ? bn : bn / lp->d_secpercyl;
 q:
	dp = &hdutab[unit];
	s = splbio();
	disksort(dp, bp);
	if (!dp->b_active) {
		hdustart(ui);
		if (!hdtab[ui->mi->unit].b_active)
			hdstart(ui->mi);
	}
	splx(s);
	return(0);
 done:
	biodone(bp);
	return(0);
}

/*
 * Unit start routine.
 * Move request from drive to controller queue.
 */
int
hdustart(ui)
	struct bus_device *ui;
{
	struct buf *bp;
	struct buf *dp;

	bp = &hdutab[ui->unit];
	if (bp->b_actf == 0)
		return(0);
	dp = &hdtab[ui->mi->unit];
	if (dp->b_actf == 0)
		dp->b_actf = bp;
	else
		dp->b_actl->b_forw = bp;
	bp->b_forw = 0;
	dp->b_actl = bp;
	bp->b_active++;
}

/*
 * Start output on controller.
 */
int
hdstart(um)
	struct bus_ctlr *um;
{
	long bn;
	struct buf *bp;
	struct buf *dp;
	struct hdsoftc *sc;
	struct hdcsoftc *hdc;
	struct bus_device *ui;
	struct disklabel *lp;
	struct diskpart *label;

	/*
	 * Pull a request from the controller queue.
	 */
	dp = &hdtab[um->unit];
	if ((bp = dp->b_actf) == 0)
		return(0);
	bp = bp->b_actf;

	hdc = &hdcsoftc[um->unit];
	ui = hddinfo[hdunit(bp->b_dev)];
	sc = &hdsoftc[ui->unit];
	lp = &hdlabel[ui->unit];

	label = lookup_part(&array[MAX_IDE_PARTS*hdunit(bp->b_dev)], hdpart(bp->b_dev));

	/*
	 * Mark controller busy.
	 */
	dp->b_active++;

	if (bp->b_flags & B_IDENTIFY) {
		hdc->sc_state = IDENTIFY;
		goto doit;
	}

	/*
	 * Figure out where this request is going.
	 */
	if (sc->sc_flags & HDF_LBA)
		hdc->sc_cn = bp->b_cylin;
	else {
		bn = bp->b_blkno;
		if ((bp->b_flags & B_ABS) == 0) {
			bn += label->start; /* partition must be valid */
		}
		hdc->sc_cn = bp->b_cylin;
		hdc->sc_sn = bn % lp->d_secpercyl;
		hdc->sc_tn = hdc->sc_sn / lp->d_nsectors;
		hdc->sc_sn %= lp->d_nsectors;
	}

	/*
	 * Set up for multi-sector transfer.
	 */
	hdc->sc_addr = bp->b_un.b_addr;
	hdc->sc_resid = bp->b_bcount;
	hdc->sc_wticks = 0;
	hdc->sc_recalerr = 0;
	hdc->sc_ioerr = 0;

	/*
	 * Set initial transfer state.
	 */
	if (sc->sc_flags & HDF_SETPARAM)
		hdc->sc_state = SETPARAM;
	else if (sc->sc_flags & HDF_RESTORE)
		hdc->sc_state = RESTORE;
	else if (sc->sc_flags & HDF_SETMULTI)
		hdc->sc_state = SETMULTI;
	else
		hdc->sc_state = TRANSFER;

 doit:
	/*
	 * Call transfer state routine to do the actual I/O.
	 */
	hdstate(um);
}

/*
 * Interrupt routine.
 */
int
hdintr(ctlr)
	int ctlr;
{
	int timedout;
	struct bus_ctlr *um = hdminfo[ctlr];
	struct bus_device *ui;
	struct buf *bp;
	struct buf *dp = &hdtab[ctlr];
	struct hdcsoftc *hdc = &hdcsoftc[ctlr];

	if (!dp->b_active) {
		(void) inb(HD_STATUS(um->address));
		printf("hdc%d: stray interrupt\n", ctlr);
		return(0);
	}
	timedout = hdc->sc_wticks >= OP_TIMEOUT;
	hdc->sc_wticks = 0;

	/*
	 * Operation timed out, terminate request.
	 */
	if (timedout) {
		bp = dp->b_actf->b_actf;
		ui = hddinfo[hdunit(bp->b_dev)];
		hderror("timed out", ui);
		hdsoftc[ui->unit].sc_flags |= HDF_RESTORE;
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		hddone(ui, bp);
		return(0);
	}

	/*
	 * Let transfer state routine handle the rest.
	 */
	hdstate(um);
}

/*
 * Transfer finite state machine driver.
 */
int
hdstate(um)
	struct bus_ctlr *um;
{
	char *msg;
	int op;
	struct buf *bp;
	struct hdsoftc *sc;
	struct bus_device *ui;
	struct disklabel *lp;
	struct hdcsoftc *hdc = &hdcsoftc[um->unit];
	struct hdbios *bg;

	bp = hdtab[um->unit].b_actf->b_actf;
	ui = hddinfo[hdunit(bp->b_dev)];
	lp = &hdlabel[ui->unit];
	sc = &hdsoftc[ui->unit];
	bg = &hdbios[ui->unit];

	/*
	 * Ensure controller is not busy.
	 */
	if (!hdwait(um))
		goto ctlr_err;

	while (1) switch (hdc->sc_state) {

	case SETPARAM:
		/*
		 * Set drive parameters.
		 */
		outb(HD_DRVHD(um->address),
		     0xa0 | (ui->slave << 4) | (lp->d_ntracks - 1));
		outb(HD_SECTCNT(um->address), lp->d_nsectors);
		outb(HD_CMD(um->address), CMD_SETPARAM);
		hdc->sc_state = SETPARAMDONE;
		return(0);

	case SETPARAMDONE:
		/*
		 * Set parameters complete.
		 */
		if (msg = hderrchk(um))
			goto bad;
		sc->sc_flags &= ~HDF_SETPARAM;
		hdc->sc_state = RESTORE;
		break;

	case RESTORE:
		/*
		 * Recalibrate drive.
		 */
		outb(HD_DRVHD(um->address), 0xa0 | (ui->slave << 4));
		outb(HD_CMD(um->address), CMD_RESTORE);
		hdc->sc_state = RESTOREDONE;
		return(0);

	case RESTOREDONE:
		/*
		 * Recalibration complete.
		 */
		if (msg = hderrchk(um)) {
			if (++hdc->sc_recalerr == 2)
				goto bad;
			hdc->sc_state = RESTORE;
			break;
		}
		sc->sc_flags &= ~HDF_RESTORE;
		hdc->sc_recalerr = 0;
		if (sc->sc_flags & HDF_SETMULTI)
			hdc->sc_state = SETMULTI;
		else
			hdc->sc_state = TRANSFER;
		break;

	case TRANSFER:
		/*
		 * Perform I/O transfer.
		 */
		sc->sc_flags &= ~HDF_UNALIGNED;
		hdc->sc_state = TRANSFERDONE;
		hdc->sc_amt = hdc->sc_resid / lp->d_secsize;
		if (hdc->sc_amt == 0) {
			sc->sc_flags |= HDF_UNALIGNED;
			hdc->sc_amt = 1;
		} else if (hdc->sc_amt > 256)
			hdc->sc_amt = 256;
		if (sc->sc_multicnt > 1 && hdc->sc_amt >= sc->sc_multicnt) {
			hdc->sc_cnt = sc->sc_multicnt;
			hdc->sc_amt -= hdc->sc_amt % hdc->sc_cnt;
			if (bp->b_flags & B_READ)
				op = CMD_READMULTI;
			else
				op = CMD_WRITEMULTI;
		} else {
			hdc->sc_cnt = 1;
			if (bp->b_flags & B_READ)
				op = CMD_READ;
			else
				op = CMD_WRITE;
		}
		if (sc->sc_flags & HDF_LBA) {
			outb(HD_DRVHD(um->address),
			     (0xe0 | (ui->slave << 4)
			      | ((hdc->sc_cn >> 24) & 0x0f)));
			outb(HD_SECT(um->address), hdc->sc_cn);
			outb(HD_CYLLO(um->address), hdc->sc_cn >> 8);
			outb(HD_CYLHI(um->address), hdc->sc_cn >> 16);
		} else {
			outb(HD_DRVHD(um->address),
			     0xa0 | (ui->slave << 4) | hdc->sc_tn);
			outb(HD_SECT(um->address), hdc->sc_sn + 1);
			outb(HD_CYLLO(um->address), hdc->sc_cn);
			outb(HD_CYLHI(um->address), hdc->sc_cn >> 8);
		}
		outb(HD_SECTCNT(um->address), hdc->sc_amt & 0xff);
		outb(HD_PRECOMP(um->address), bg->bg_precomp / 4);
		outb(HD_CMD(um->address), op);
		if ((bp->b_flags & B_READ) == 0) {
			int i;
			caddr_t buf;

			if (sc->sc_flags & HDF_UNALIGNED) {
				buf = hdc->sc_buf;
				bcopy(hdc->sc_addr, buf, hdc->sc_resid);
				bzero(buf + hdc->sc_resid,
				      lp->d_secsize - hdc->sc_resid);
			} else
				buf = hdc->sc_addr;
			for (i = 0; i < 1000000; i++)
				if (inb(HD_STATUS(um->address)) & ST_DREQ) {
					loutw(HD_DATA(um->address), buf,
					      hdc->sc_cnt * lp->d_secsize / 2);
					return(0);
				}
			goto ctlr_err;
		}
		return(0);

	case TRANSFERDONE:
		/*
		 * Transfer complete.
		 */
		if (msg = hderrchk(um)) {
			if (++hdc->sc_ioerr == MAX_RETRIES)
				goto bad;
			/*
			 * Every fourth attempt print a message
			 * and recalibrate the drive.
			 */
			if (hdc->sc_ioerr & 3)
				hdc->sc_state = TRANSFER;
			else {
				hderror(msg, ui);
				hdc->sc_state = RESTORE;
			}
			break;
		}
		if (bp->b_flags & B_READ) {
			if (sc->sc_flags & HDF_UNALIGNED) {
				linw(HD_DATA(um->address), hdc->sc_buf,
				     lp->d_secsize / 2);
				bcopy(hdc->sc_buf, hdc->sc_addr,
				      hdc->sc_resid);
			} else
				linw(HD_DATA(um->address), hdc->sc_addr,
				     hdc->sc_cnt * lp->d_secsize / 2);
		}
		hdc->sc_resid -= hdc->sc_cnt * lp->d_secsize;
		if (hdc->sc_resid <= 0) {
			bp->b_resid = 0;
			hddone(ui, bp);
			return(0);
		}
		if (sc->sc_flags & HDF_LBA)
			hdc->sc_cn += hdc->sc_cnt;
		else {
			hdc->sc_sn += hdc->sc_cnt;
			while (hdc->sc_sn >= lp->d_nsectors) {
				hdc->sc_sn -= lp->d_nsectors;
				if (++hdc->sc_tn == lp->d_ntracks) {
					hdc->sc_tn = 0;
					hdc->sc_cn++;
				}
			}
		}
		hdc->sc_ioerr = 0;
		hdc->sc_addr += hdc->sc_cnt * lp->d_secsize;
		hdc->sc_amt -= hdc->sc_cnt;
		if (hdc->sc_amt == 0) {
			hdc->sc_state = TRANSFER;
			break;
		}
		if ((bp->b_flags & B_READ) == 0) {
			int i;

			for (i = 0; i < 1000000; i++)
				if (inb(HD_STATUS(um->address)) & ST_DREQ) {
					loutw(HD_DATA(um->address),
					      hdc->sc_addr,
					      hdc->sc_cnt * lp->d_secsize / 2);
					return(0);
				}
			goto ctlr_err;
		}
		return(0);

	case IDENTIFY:
		/*
		 * Get drive info.
		 */
		hdc->sc_state = IDENTIFYDONE;
		outb(HD_DRVHD(um->address), 0xa0 | (ui->slave << 4));
		outb(HD_CMD(um->address), CMD_IDENTIFY);
		return(0);

	case IDENTIFYDONE:
		/*
		 * Get drive info complete.
		 */
		if (msg = hderrchk(um))
			goto bad;
		linw(HD_DATA(um->address), (u_short *)bp->b_un.b_addr, 256);
		hddone(ui, bp);
		return(0);

	case SETMULTI:
		/*
		 * Set multiple mode count.
		 */
		hdc->sc_state = SETMULTIDONE;
		outb(HD_DRVHD(um->address), 0xa0 | (ui->slave << 4));
		outb(HD_SECTCNT(um->address), sc->sc_multicnt);
		outb(HD_CMD(um->address), CMD_SETMULTI);
		return(0);

	case SETMULTIDONE:
		/*
		 * Set multiple mode count complete.
		 */
		sc->sc_flags &= ~HDF_SETMULTI;
		if (msg = hderrchk(um)) {
			sc->sc_multicnt = 0;
			goto bad;
		}
		hdc->sc_state = TRANSFER;
		break;

	default:
		printf("hd%d: invalid state\n", ui->unit);
		panic("hdstate");
		/*NOTREACHED*/
	}

 ctlr_err:
	msg = "controller error";

 bad:
	hderror(msg, ui);
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	sc->sc_flags |= HDF_RESTORE;
	hddone(ui, bp);
}

/*
 * Terminate current request and start
 * any others that are queued.
 */
int
hddone(ui, bp)
	struct bus_device *ui;
	struct buf *bp;
{
	struct bus_ctlr *um = ui->mi;
	struct hdsoftc *sc = &hdsoftc[ui->unit];
	struct hdcsoftc *hdc = &hdcsoftc[um->unit];
	struct buf *dp = &hdtab[um->unit];

	sc->sc_flags &= ~HDF_UNALIGNED;

	/*
	 * Remove this request from queue.
	 */
	hdutab[ui->unit].b_actf = bp->b_actf;
	biodone(bp);
	bp = &hdutab[ui->unit];
	dp->b_actf = bp->b_forw;

	/*
	 * Mark controller and drive idle.
	 */
	dp->b_active = 0;
	bp->b_active = 0;
	hdc->sc_state = IDLE;

	/*
	 * Start up other requests.
	 */
	hdustart(ui);
	hdstart(um);

	/*
	 * Wakeup anyone waiting for drive.
	 */
	if (sc->sc_flags & HDF_WANT) {
		sc->sc_flags &= ~HDF_WANT;
		wakeup((caddr_t)sc);
	}
}

/*
 * Wait for controller to be idle.
 */
int
hdwait(um)
	struct bus_ctlr *um;
{
	int i, status;

	for (i = 0; i < 1000000; i++) {
		status = inb(HD_STATUS(um->address));
		if ((status & ST_BUSY) == 0 && (status & ST_READY))
			return (status);
	}
	return (0);
}

/*
 * Check for errors on completion of an operation.
 */
char *
hderrchk(um)
	struct bus_ctlr *um;
{
	int status;

	status = inb(HD_STATUS(um->address));
	if (status & ST_WRTFLT)
		return ("write fault");
	if (status & ST_ERROR) {
		status = inb(HD_ERROR(um->address));
		if (status & ERR_DAM)
			return ("data address mark not found");
		if (status & ERR_TR0)
			return ("track 0 not found");
		if (status & ERR_ID)
			return ("sector not found");
		if (status & ERR_ECC)
			return ("uncorrectable ECC error");
		if (status & ERR_BADBLK)
			return ("bad block detected");
		if (status & ERR_ABORT)
			return ("command aborted");
		return ("hard error");
	}
	return (NULL);
}

/*
 * Print an error message.
 */
hderror(msg, ui)
	char *msg;
	struct bus_device *ui;
{
	char *op;
	int prn_sn = 0;
	struct hdcsoftc *hdc = &hdcsoftc[ui->mi->unit];

	switch (hdc->sc_state) {

	case SETPARAM:
	case SETPARAMDONE:
		op = "SETPARAM: ";
		break;

	case RESTORE:
	case RESTOREDONE:
		op = "RESTORE: ";
		break;

	case TRANSFER:
	case TRANSFERDONE:
		if (hdutab[ui->unit].b_actf->b_flags & B_READ)
			op = "READ: ";
		else
			op = "WRITE: ";
		prn_sn = 1;
		break;

	case IDENTIFY:
	case IDENTIFYDONE:
		op = "IDENTIFY: ";
		break;

	case SETMULTI:
	case SETMULTIDONE:
		op = "SETMULTI: ";
		break;

	default:
		op = "";
		break;
	}
	printf("hd%d: %s%s", ui->unit, op, msg);
	if (prn_sn) {
		if (hdsoftc[ui->unit].sc_flags & HDF_LBA)
			printf(", bn %d", hdc->sc_cn);
		else
			printf(", cn %d tn %d sn %d",
			       hdc->sc_cn, hdc->sc_tn, hdc->sc_sn + 1);
	}
	printf("\n");
}

/*
 * Watchdog routine.
 * Check for any hung operations.
 */
void
hdwatch()
{
	int unit, s;

	timeout(hdwatch, 0, hz);
	s = splbio();
	for (unit = 0; unit < NHDC; unit++)
		if (hdtab[unit].b_active
		    && ++hdcsoftc[unit].sc_wticks >= OP_TIMEOUT)
			hdintr(unit);
	splx(s);
}

#endif /* NHD > 0 && !LINUX_DEV */
