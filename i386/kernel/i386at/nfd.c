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
 */

#include <fd.h>
#if NFD > 0
/*
 * Floppy disk driver.
 *
 * Supports:
 * 1 controller and 2 drives.
 * Media change and automatic media detection.
 * Arbitrarily sized read/write requests.
 * Misaligned requests
 * DMA above 16 Meg
 *
 * TODO:
 * 1) Real probe routines for controller and drives.
 * 2) Support for multiple controllers.  The driver does
 * not assume a single controller since all functions
 * take the controller and/or device structure as an
 * argument, however the probe routines limit the
 * number of controllers and drives to 1 and 2 respectively.
 * 3) V_VERIFY ioctl.
 * 4) User defined diskette parameters.
 * 5) Detect Intel 82077 or compatible and use its FIFO mode.
 *
 * Shantanu Goel (goel@cs.columbia.edu)
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include "vm_param.h"
#include <kern/time_out.h>
#include <vm/pmap.h>
#include <device/param.h>
#include <device/buf.h>
#include <device/errno.h>
#include <chips/busses.h>
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/cram.h>
#include <i386at/disk.h>
#include <i386at/nfdreg.h>

/*
 * Number of drives supported by an FDC.
 * The controller is actually capable of
 * supporting 4 drives, however, most (all?)
 * board implementations only support 2.
 */
#define NDRIVES_PER_FDC	2
#define NFDC		((NFD + NDRIVES_PER_FDC - 1) / NDRIVES_PER_FDC)

#define fdunit(dev)	(((int)(dev) >> 6) & 3)
#define fdmedia(dev)	((int)(dev) & 3)

#define b_cylin		b_resid
#define B_FORMAT	B_MD1

#define SECSIZE		512

#define DMABSIZE	(18*1024)	/* size of DMA bounce buffer */

#define OP_TIMEOUT	5		/* time to wait (secs) for an
					   operation before giving up */
#define MOTOR_TIMEOUT	5		/* time to wait (secs) before turning
					   off an idle drive motor */
#define MAX_RETRIES	48		/* number of times to try
					   an I/O operation */

#define SRTHUT		0xdf		/* step rate/head unload time */
#define HLTND		0x02		/* head load time/dma mode */

/*
 * DMA controller.
 *
 * XXX: There should be a generic <i386/dma.h> file.
 */

/*
 * Ports
 */
#define DMA2_PAGE	0x81	/* channel 2, page register */
#define DMA2_ADDR	0x04	/* channel 2, addr register */
#define DMA2_COUNT	0x05	/* channel 2, count register */
#define DMA_STATUS	0x08	/* status register */
#define DMA_COMMAND	0x08	/* command register */
#define DMA_WREQ	0x09	/* request register */
#define DMA_SINGLEMSK	0x0a	/* single mask register */
#define DMA_MODE	0x0b	/* mode register */
#define DMA_FLIPFLOP	0x0c	/* pointer flip/flop */
#define DMA_TEMP	0x0d	/* temporary register */
#define DMA_MASTERCLR	0x0d	/* master clear */
#define DMA_CLRMASK	0x0e	/* clear mask register */
#define DMA_ALLMASK	0x0f	/* all mask register */

/*
 * Commands
 */
#define DMA_WRITE	0x46	/* write on channel 2 */
#define DMA_READ	0x4a	/* read on channel 2 */

/*
 * Autoconfiguration stuff.
 */
struct	bus_ctlr *fdminfo[NFDC];
struct	bus_device *fddinfo[NFD];
int	fdstd[] = { 0 };
int	fdprobe(), fdslave(), fdintr();
void	fdattach();
struct	bus_driver fddriver = {
	fdprobe, fdslave, fdattach, 0, fdstd, "fd", fddinfo, "fdc", fdminfo
};

/*
 * Per-controller state.
 */
struct fdcsoftc {
	int	sc_flags;
#define FDF_WANT	0x01	/* someone needs direct controller access */
#define FDF_RESET	0x02	/* controller needs reset */
#define FDF_LIMIT	0x04	/* limit transfer to a single sector */
#define FDF_BOUNCE	0x08	/* using bounce buffer */
	int	sc_state;	/* transfer fsm */
	caddr_t	sc_addr;	/* buffer address */
	int	sc_resid;	/* amount left to transfer */
	int	sc_amt;		/* amount currently being transferred */
	int	sc_op;		/* operation being performed */
	int	sc_mode;	/* DMA mode */
	int	sc_sn;		/* sector number */
	int	sc_tn;		/* track number */
	int	sc_cn;		/* cylinder number */
	int	sc_recalerr;	/* # recalibration errors */
	int	sc_seekerr;	/* # seek errors */
	int	sc_ioerr;	/* # i/o errors */
	int	sc_dor;		/* copy of digital output register */
	int	sc_rate;	/* copy of transfer rate register */
	int	sc_wticks;	/* watchdog */
	u_int	sc_buf;		/* buffer for transfers > 16 Meg */
	u_char	sc_cmd[9];	/* command buffer */
	u_char	sc_results[7];	/* operation results */
} fdcsoftc[NFDC];

#define sc_st0	sc_results[0]
#define sc_st3	sc_results[0]
#define sc_st1	sc_results[1]
#define sc_pcn	sc_results[1]
#define sc_st2	sc_results[2]
#define sc_c	sc_results[3]
#define sc_h	sc_results[4]
#define sc_r	sc_results[5]
#define sc_n	sc_results[6]

/*
 * Transfer states.
 */
#define IDLE		0	/* controller is idle */
#define RESET		1	/* reset controller */
#define RESETDONE	2	/* reset completion interrupt */
#define RECAL		3	/* recalibrate drive */
#define RECALDONE	4	/* recalibration complete interrupt */
#define SEEK		5	/* perform seek on drive */
#define SEEKDONE	6	/* seek completion interrupt */
#define TRANSFER	7	/* perform transfer on drive */
#define TRANSFERDONE	8	/* transfer completion interrupt */

/*
 * Per-drive state.
 */
struct fdsoftc {
	int	sc_flags;
#define FDF_RECAL	0x02	/* drive needs recalibration */
#define FDF_SEEK	0x04	/* force seek during auto-detection */
#define FDF_AUTO	0x08	/* performing auto-density */
#define FDF_AUTOFORCE	0x10	/* force auto-density */
#define FDF_INIT	0x20	/* drive is being initialized */
	int	sc_type;	/* drive type */
	struct	fddk *sc_dk;	/* diskette type */
	int	sc_cyl;		/* current head position */
	int	sc_mticks;	/* motor timeout */
} fdsoftc[NFD];

struct	buf fdtab[NFDC];	/* controller queues */
struct	buf fdutab[NFD];	/* drive queues */

/*
 * Floppy drive type names.
 */
char	*fdnames[] = { "360K", "1.2 Meg", "720K", "1.44 Meg" };
#define NTYPES	(sizeof(fdnames) / sizeof(fdnames[0]))

/*
 * Floppy diskette parameters.
 */
struct fddk {
	int	dk_nspu;	/* sectors/unit */
	int	dk_nspc;	/* sectors/cylinder */
	int	dk_ncyl;	/* cylinders/unit */
	int	dk_nspt;	/* sectors/track */
	int	dk_step;	/* !=0 means double track steps */
	int	dk_gap;		/* read/write gap length */
	int	dk_fgap;	/* format gap length */
	int	dk_rate;	/* transfer rate */
	int	dk_drives;	/* bit mask of drives that accept diskette */
	char	*dk_name;	/* type name */
} fddk[] = {
	/*
	 * NOTE: largest density for each drive type must be first so
	 * fdauto() tries it before any lower ones.
	 */
	{ 2880, 36, 80, 18, 0, 0x1b, 0x6c, 0x00, 0x08, "1.44 Meg" },
	{ 2400, 30, 80, 15, 0, 0x1b, 0x54, 0x00, 0x02,  "1.2 Meg" },
	{ 1440, 18, 80,  9, 0, 0x2a, 0x50, 0x02, 0x0c,     "720K" },
	{  720, 18, 40,  9, 1, 0x23, 0x50, 0x01, 0x02,     "360K" },
	{  720, 18, 40,  9, 0, 0x2a, 0x50, 0x02, 0x01,  "360K PC" }
};
#define NDKTYPES	(sizeof(fddk) / sizeof(fddk[0]))

/*
 * For compatibility with old driver.
 * This array is indexed by the old floppy type codes
 * and points to the corresponding entry for that
 * type in fddk[] above.
 */
struct	fddk *fdcompat[NDKTYPES];

int	fdwstart = 0;
int	fdstrategy(), fdformat();
char	*fderrmsg();
void	fdwatch(), fdminphys(), fdspinup(), wakeup();

#define FDDEBUG
#ifdef FDDEBUG
int fddebug = 0;
#define DEBUGF(n, stmt)	{ if (fddebug >= (n)) stmt; }
#else
#define DEBUGF(n, stmt)
#endif

/*
 * Probe for a controller.
 */
int
fdprobe(xxx, um)
	int xxx;
	struct bus_ctlr *um;
{
	struct fdcsoftc *fdc;

	if (um->unit >= NFDC) {
		printf("fdc%d: not configured\n", um->unit);
		return (0);
	}
	if (um->unit > 0)	/* XXX: only 1 controller */
		return (0);

	/*
	 * XXX: need real probe
	 */
	take_ctlr_irq(um);
	printf("%s%d: port 0x%x, spl %d, pic %d.\n",
	       um->name, um->unit, um->address, um->sysdep, um->sysdep1);

	/*
	 * Set up compatibility array.
	 */
	fdcompat[0] = &fddk[2];
	fdcompat[1] = &fddk[0];
	fdcompat[2] = &fddk[3];
	fdcompat[3] = &fddk[1];

	fdc = &fdcsoftc[um->unit];
	fdc->sc_rate = -1;
	if (!fdc->sc_buf) {
		fdc->sc_buf = alloc_dma_mem(DMABSIZE, 64*1024);
		if (fdc->sc_buf == 0)
			panic("fd: alloc_dma_mem() failed");
	}
	fdc->sc_dor = DOR_RSTCLR | DOR_IENABLE;
	outb(FD_DOR(um->address), fdc->sc_dor);
	return (1);
}

/*
 * Probe for a drive.
 */
int
fdslave(ui)
	struct bus_device *ui;
{
	struct fdsoftc *sc;

	if (ui->unit >= NFD) {
		printf("fd%d: not configured\n", ui->unit);
		return (0);
	}
	if (ui->unit > 1)	/* XXX: only 2 drives */
		return (0);

	/*
	 * Find out from CMOS if drive exists.
	 */
	sc = &fdsoftc[ui->unit];
	outb(CMOS_ADDR, 0x10);
	sc->sc_type = inb(CMOS_DATA);
	if (ui->unit == 0)
		sc->sc_type >>= 4;
	sc->sc_type &= 0x0f;
	return (sc->sc_type);
}

/*
 * Attach a drive to the system.
 */
void
fdattach(ui)
	struct bus_device *ui;
{
	struct fdsoftc *sc;

	sc = &fdsoftc[ui->unit];
	if (--sc->sc_type >= NTYPES) {
		printf(": unknown drive type %d", sc->sc_type);
		ui->alive = 0;
		return;
	}
	printf(": %s", fdnames[sc->sc_type]);
	sc->sc_flags = FDF_RECAL | FDF_SEEK | FDF_AUTOFORCE;
}

int
fdopen(dev, mode)
	dev_t dev;
	int mode;
{
	int unit = fdunit(dev), error;
	struct bus_device *ui;
	struct fdsoftc *sc;

	if (unit >= NFD || (ui = fddinfo[unit]) == 0 || ui->alive == 0)
		return (ENXIO);

	/*
	 * Start watchdog.
	 */
	if (!fdwstart) {
		fdwstart++;
		timeout(fdwatch, 0, hz);
	}
	/*
	 * Do media detection if drive is being opened for the
	 * first time or diskette has been changed since the last open.
	 */
	sc = &fdsoftc[unit];
	if ((sc->sc_flags & FDF_AUTOFORCE) || fddskchg(ui)) {
		if (error = fdauto(dev))
			return (error);
		sc->sc_flags &= ~FDF_AUTOFORCE;
	}
	return (0);
}

int
fdclose(dev)
	dev_t dev;
{
	int s, unit = fdunit(dev);
	struct fdsoftc *sc = &fdsoftc[unit];

	/*
	 * Wait for pending operations to complete.
	 */
	s = splbio();
	while (fdutab[unit].b_active) {
		sc->sc_flags |= FDF_WANT;
		assert_wait((event_t)sc, FALSE);
		thread_block((void (*)())0);
	}
	splx(s);
	return (0);
}

int
fdread(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	return (block_io(fdstrategy, fdminphys, ior));
}

int
fdwrite(dev, ior)
	dev_t dev;
	io_req_t ior;
{
	return (block_io(fdstrategy, fdminphys, ior));
}

int
fdgetstat(dev, flavor, status, status_count)
	dev_t dev;
	dev_flavor_t flavor;
	dev_status_t status;
	mach_msg_type_number_t *status_count;
{
	switch (flavor) {

	case DEV_GET_SIZE:
	{
		int *info;
		io_return_t error;
		struct disk_parms dp;

		if (error = fdgetparms(dev, &dp))
			return (error);
		info = (int *)status;
		info[DEV_GET_SIZE_DEVICE_SIZE] = dp.dp_pnumsec * SECSIZE;
		info[DEV_GET_SIZE_RECORD_SIZE] = SECSIZE;
		*status_count = DEV_GET_SIZE_COUNT;
		return (D_SUCCESS);
	}
	case V_GETPARMS:
		if (*status_count < (sizeof(struct disk_parms) / sizeof(int)))
			return (D_INVALID_OPERATION);
		*status_count = sizeof(struct disk_parms) / sizeof(int);
		return (fdgetparms(dev, (struct disk_parms *)status));

	default:
		return (D_INVALID_OPERATION);
	}
}

int
fdsetstat(dev, flavor, status, status_count)
	dev_t dev;
	dev_flavor_t flavor;
	dev_status_t status;
	mach_msg_type_number_t status_count;
{
	switch (flavor) {

	case V_SETPARMS:
		return (fdsetparms(dev, *(int *)status));

	case V_FORMAT:
		return (fdformat(dev, (union io_arg *)status));

	case V_VERIFY:
		/*
		 * XXX: needs to be implemented
		 */
		return (D_SUCCESS);

	default:
		return (D_INVALID_OPERATION);
	}
}

int
fddevinfo(dev, flavor, info)
	dev_t dev;
	int flavor;
	char *info;
{
	switch (flavor) {

	case D_INFO_BLOCK_SIZE:
		*(int *)info = SECSIZE;
		return (D_SUCCESS);

	default:
		return (D_INVALID_OPERATION);
	}
}

/*
 * Allow arbitrary transfers.  Standard minphys restricts
 * transfers to a maximum of 256K preventing us from reading
 * an entire diskette in a single system call.
 */
void
fdminphys(ior)
	io_req_t ior;
{
}

/*
 * Return current media parameters.
 */
int
fdgetparms(dev, dp)
	dev_t dev;
	struct disk_parms *dp;
{
	struct fddk *dk = fdsoftc[fdunit(dev)].sc_dk;

	dp->dp_type = DPT_FLOPPY;
	dp->dp_heads = 2;
	dp->dp_sectors = dk->dk_nspt;
	dp->dp_pstartsec = 0;
	dp->dp_cyls = dk->dk_ncyl;
	dp->dp_pnumsec = dk->dk_nspu;
	return (0);
}

/*
 * Set media parameters.
 */
int
fdsetparms(dev, type)
	dev_t dev;
	int type;
{
	struct fdsoftc *sc;
	struct fddk *dk;

	if (type < 0 || type >= NDKTYPES)
		return (EINVAL);
	dk = fdcompat[type];
	sc = &fdsoftc[fdunit(dev)];
	if ((dk->dk_drives & (1 << sc->sc_type)) == 0)
		return (EINVAL);
	sc->sc_dk = dk;
	return (D_SUCCESS);
}

/*
 * Format a floppy.
 */
int
fdformat(dev, arg)
	dev_t dev;
	union io_arg *arg;
{
	int i, j, sect, error = 0;
	unsigned track, num_trks;
	struct buf *bp;
	struct fddk *dk;
	struct format_info *fmt;

	dk = fdsoftc[fdunit(dev)].sc_dk;
	num_trks = arg->ia_fmt.num_trks;
	track = arg->ia_fmt.start_trk;
	if (num_trks == 0 || track + num_trks > (dk->dk_ncyl << 1)
	    || arg->ia_fmt.intlv >= dk->dk_nspt)
		return (EINVAL);

	bp = (struct buf *)geteblk(SECSIZE);
	bp->b_dev = dev;
	bp->b_bcount = dk->dk_nspt * sizeof(struct format_info);
	bp->b_blkno = track * dk->dk_nspt;

	while (num_trks-- > 0) {
		/*
		 * Set up format information.
		 */
		fmt = (struct format_info *)bp->b_un.b_addr;
		for (i = 0; i < dk->dk_nspt; i++)
			fmt[i].sector = 0;
		for (i = 0, j = 0, sect = 1; i < dk->dk_nspt; i++) {
			fmt[j].cyl = track >> 1;
			fmt[j].head = track & 1;
			fmt[j].sector = sect++;
			fmt[j].secsize = 2;
			if ((j += arg->ia_fmt.intlv) < dk->dk_nspt)
				continue;
			for (j -= dk->dk_nspt; j < dk->dk_nspt; j++)
				if (fmt[j].sector == 0)
					break;
		}
		bp->b_flags = B_FORMAT;
		fdstrategy(bp);
		biowait(bp);
		if (bp->b_flags & B_ERROR) {
			error = bp->b_error;
			break;
		}
		bp->b_blkno += dk->dk_nspt;
		track++;
	}
	bp->b_flags &= ~B_FORMAT;
	brelse(bp);
	return (error);
}

/*
 * Strategy routine.
 * Enqueue a request on drive queue.
 */
int
fdstrategy(bp)
	struct buf *bp;
{
	int unit = fdunit(bp->b_dev), s;
	int bn, sz, maxsz;
	struct buf *dp;
	struct bus_device *ui = fddinfo[unit];
	struct fddk *dk = fdsoftc[unit].sc_dk;

	bn = bp->b_blkno;
	sz = (bp->b_bcount + SECSIZE - 1) / SECSIZE;
	maxsz = dk->dk_nspu;
	if (bn < 0 || bn + sz > maxsz) {
		if (bn == maxsz) {
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		sz = maxsz - bn;
		if (sz <= 0) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto done;
		}
		bp->b_bcount = sz * SECSIZE;
	}
	bp->b_cylin = bn / dk->dk_nspc;
	dp = &fdutab[unit];
	s = splbio();
	disksort(dp, bp);
	if (!dp->b_active) {
		fdustart(ui);
		if (!fdtab[ui->mi->unit].b_active)
			fdstart(ui->mi);
	}
	splx(s);
	return;
 done:
	biodone(bp);
	return;
}

/*
 * Unit start routine.
 * Move request from drive to controller queue.
 */
int
fdustart(ui)
	struct bus_device *ui;
{
	struct buf *bp;
	struct buf *dp;

	bp = &fdutab[ui->unit];
	if (bp->b_actf == 0)
		return;
	dp = &fdtab[ui->mi->unit];
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
fdstart(um)
	struct bus_ctlr *um;
{
	struct buf *bp;
	struct buf *dp;
	struct fdsoftc *sc;
	struct fdcsoftc *fdc;
	struct bus_device *ui;
	struct fddk *dk;

	/*
	 * Pull a request from the controller queue.
	 */
	dp = &fdtab[um->unit];
	if ((bp = dp->b_actf) == 0)
		return;
	bp = bp->b_actf;

	fdc = &fdcsoftc[um->unit];
	ui = fddinfo[fdunit(bp->b_dev)];
	sc = &fdsoftc[ui->unit];
	dk = sc->sc_dk;

	/*
	 * Mark controller busy.
	 */
	dp->b_active++;

	/*
	 * Figure out where this request is going.
	 */
	fdc->sc_cn = bp->b_cylin;
	fdc->sc_sn = bp->b_blkno % dk->dk_nspc;
	fdc->sc_tn = fdc->sc_sn / dk->dk_nspt;
	fdc->sc_sn %= dk->dk_nspt;

	/*
	 * Set up for multi-sector transfer.
	 */
	fdc->sc_op = ((bp->b_flags & B_FORMAT) ? CMD_FORMAT
		      : ((bp->b_flags & B_READ) ? CMD_READ : CMD_WRITE));
	fdc->sc_mode = (bp->b_flags & B_READ) ? DMA_WRITE : DMA_READ;
	fdc->sc_addr = bp->b_un.b_addr;
	fdc->sc_resid = bp->b_bcount;
	fdc->sc_wticks = 0;
	fdc->sc_recalerr = 0;
	fdc->sc_seekerr = 0;
	fdc->sc_ioerr = 0;

	/*
	 * Set initial transfer state.
	 */
	if (fdc->sc_flags & FDF_RESET)
		fdc->sc_state = RESET;
	else if (sc->sc_flags & FDF_RECAL)
		fdc->sc_state = RECAL;
	else if (sc->sc_cyl != fdc->sc_cn)
		fdc->sc_state = SEEK;
	else
		fdc->sc_state = TRANSFER;

	/*
	 * Set transfer rate.
	 */
	if (fdc->sc_rate != dk->dk_rate) {
		fdc->sc_rate = dk->dk_rate;
		outb(FD_RATE(um->address), fdc->sc_rate);
	}
	/*
	 * Turn on drive motor.
	 * Don't start I/O if drive is spinning up.
	 */
	if (fdmotoron(ui)) {
		timeout(fdspinup, (void *)um, hz / 2);
		return;
	}
	/*
	 * Call transfer state routine to do the actual I/O.
	 */
	fdstate(um);
}

/*
 * Interrupt routine.
 */
int
fdintr(ctlr)
	int ctlr;
{
	int timedout;
	u_char results[7];
	struct buf *bp;
	struct bus_device *ui;
	struct fdsoftc *sc;
	struct buf *dp = &fdtab[ctlr];
	struct fdcsoftc *fdc = &fdcsoftc[ctlr];
	struct bus_ctlr *um = fdminfo[ctlr];

	if (!dp->b_active) {
		printf("fdc%d: stray interrupt\n", ctlr);
		return;
	}
	timedout = fdc->sc_wticks >= OP_TIMEOUT;
	fdc->sc_wticks = 0;
	bp = dp->b_actf->b_actf;
	ui = fddinfo[fdunit(bp->b_dev)];
	sc = &fdsoftc[ui->unit];

	/*
	 * Operation timed out, terminate request.
	 */
	if (timedout) {
		fderror("timed out", ui);
		fdmotoroff(ui);
		sc->sc_flags |= FDF_RECAL;
		bp->b_flags |= B_ERROR;
		bp->b_error = ENXIO;
		fddone(ui, bp);
		return;
	}
	/*
	 * Read results from FDC.
	 * For transfer completion they can be read immediately.
	 * For anything else, we must issue a Sense Interrupt
	 * Status Command.  We keep issuing this command till
	 * FDC returns invalid command status.  The Controller Busy
	 * bit in the status register indicates completion of a
	 * read/write/format operation.
	 */
	if (inb(FD_STATUS(um->address)) & ST_CB) {
		if (!fdresults(um, fdc->sc_results))
			return;
	} else {
		while (1) {
			fdc->sc_cmd[0] = CMD_SENSEI;
			if (!fdcmd(um, 1)) {
				DEBUGF(2, printf(2, "fd%d: SENSEI failed\n"));
				return;
			}
			if (!fdresults(um, results))
				return;
			if ((results[0] & ST0_IC) == 0x80)
				break;
			if ((results[0] & ST0_US) == ui->slave) {
				fdc->sc_results[0] = results[0];
				fdc->sc_results[1] = results[1];
			}
		}
	}
	/*
	 * Let transfer state routine handle the rest.
	 */
	fdstate(um);
}

/*
 * Transfer finite state machine driver.
 */
int
fdstate(um)
	struct bus_ctlr *um;
{
	int unit, max, pa, s;
	struct buf *bp;
	struct fdsoftc *sc;
	struct bus_device *ui;
	struct fddk *dk;
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	bp = fdtab[um->unit].b_actf->b_actf;
	ui = fddinfo[fdunit(bp->b_dev)];
	sc = &fdsoftc[ui->unit];
	dk = sc->sc_dk;

	while (1) switch (fdc->sc_state) {

	case RESET:
		/*
		 * Reset the controller.
		 */
		fdreset(um);
		return;

	case RESETDONE:
		/*
		 * Reset complete.
		 * Mark all drives as needing recalibration
		 * and issue specify command.
		 */
		for (unit = 0; unit < NFD; unit++)
			if (fddinfo[unit] && fddinfo[unit]->alive
			    && fddinfo[unit]->mi == um)
				fdsoftc[unit].sc_flags |= FDF_RECAL;
		fdc->sc_cmd[0] = CMD_SPECIFY;
		fdc->sc_cmd[1] = SRTHUT;
		fdc->sc_cmd[2] = HLTND;
		if (!fdcmd(um, 3))
			return;
		fdc->sc_flags &= ~FDF_RESET;
		fdc->sc_state = RECAL;
		break;

	case RECAL:
		/*
		 * Recalibrate drive.
		 */
		fdc->sc_state = RECALDONE;
		fdc->sc_cmd[0] = CMD_RECAL;
		fdc->sc_cmd[1] = ui->slave;
		fdcmd(um, 2);
		return;

	case RECALDONE:
		/*
		 * Recalibration complete.
		 */
		if ((fdc->sc_st0 & ST0_IC) || (fdc->sc_st0 & ST0_EC)) {
			if (++fdc->sc_recalerr == 2) {
				fderror("recalibrate failed", ui);
				goto bad;
			}
			fdc->sc_state = RESET;
			break;
		}
		sc->sc_flags &= ~FDF_RECAL;
		fdc->sc_recalerr = 0;
		sc->sc_cyl = -1;
		fdc->sc_state = SEEK;
		break;

	case SEEK:
		/*
		 * Perform seek operation.
		 */
		fdc->sc_state = SEEKDONE;
		fdc->sc_cmd[0] = CMD_SEEK;
		fdc->sc_cmd[1] = (fdc->sc_tn << 2) | ui->slave;
		fdc->sc_cmd[2] = fdc->sc_cn;
		if (dk->dk_step)
			fdc->sc_cmd[2] <<= 1;
		fdcmd(um, 3);
		return;

	case SEEKDONE:
		/*
		 * Seek complete.
		 */
		if (dk->dk_step)
			fdc->sc_pcn >>= 1;
		if ((fdc->sc_st0 & ST0_IC) || (fdc->sc_st0 & ST0_SE) == 0
		    || fdc->sc_pcn != fdc->sc_cn) {
			if (++fdc->sc_seekerr == 2) {
				fderror("seek failed", ui);
				goto bad;
			}
			fdc->sc_state = RESET;
			break;
		}
		fdc->sc_seekerr = 0;
		sc->sc_cyl = fdc->sc_pcn;
		fdc->sc_state = TRANSFER;
		break;

	case TRANSFER:
		/*
		 * Perform I/O transfer.
		 */
		fdc->sc_flags &= ~FDF_BOUNCE;
		pa = pmap_extract(kernel_pmap, fdc->sc_addr);
		if (fdc->sc_op == CMD_FORMAT) {
			max = sizeof(struct format_info) * dk->dk_nspt;
		} else if (fdc->sc_flags & FDF_LIMIT) {
			fdc->sc_flags &= ~FDF_LIMIT;
			max = SECSIZE;
		} else {
			max = (dk->dk_nspc - dk->dk_nspt * fdc->sc_tn
			       - fdc->sc_sn) * SECSIZE;
		}
		if (max > fdc->sc_resid)
			max = fdc->sc_resid;
		if (pa >= 16*1024*1024) {
			fdc->sc_flags |= FDF_BOUNCE;
			pa = fdc->sc_buf;
			if (max < DMABSIZE)
				fdc->sc_amt = max;
			else
				fdc->sc_amt = DMABSIZE;
		} else {
			int prevpa, curpa, omax;
			vm_offset_t va;

			omax = max;
			if (max > 65536 - (pa & 0xffff))
				max = 65536 - (pa & 0xffff);
			fdc->sc_amt = I386_PGBYTES - (pa & (I386_PGBYTES - 1));
			va = (vm_offset_t)fdc->sc_addr + fdc->sc_amt;
			prevpa = pa & ~(I386_PGBYTES - 1);
			while (fdc->sc_amt < max) {
				curpa = pmap_extract(kernel_pmap, va);
				if (curpa >= 16*1024*1024
				    || curpa != prevpa + I386_PGBYTES)
					break;
				fdc->sc_amt += I386_PGBYTES;
				va += I386_PGBYTES;
				prevpa = curpa;
			}
			if (fdc->sc_amt > max)
				fdc->sc_amt = max;
			if (fdc->sc_op == CMD_FORMAT) {
				if (fdc->sc_amt != omax) {
					fdc->sc_flags |= FDF_BOUNCE;
					pa = fdc->sc_buf;
					fdc->sc_amt = omax;
				}
			} else if (fdc->sc_amt != fdc->sc_resid) {
				if (fdc->sc_amt < SECSIZE) {
					fdc->sc_flags |= FDF_BOUNCE;
					pa = fdc->sc_buf;
					if (omax > DMABSIZE)
						fdc->sc_amt = DMABSIZE;
					else
						fdc->sc_amt = omax;
				} else
					fdc->sc_amt &= ~(SECSIZE - 1);
			}
		}

		DEBUGF(2, printf("fd%d: TRANSFER: amt %d cn %d tn %d sn %d\n",
				 ui->unit, fdc->sc_amt, fdc->sc_cn,
				 fdc->sc_tn, fdc->sc_sn + 1));

		if ((fdc->sc_flags & FDF_BOUNCE) && fdc->sc_op != CMD_READ) {
			fdc->sc_flags &= ~FDF_BOUNCE;
			bcopy(fdc->sc_addr, (caddr_t)phystokv(fdc->sc_buf),
			      fdc->sc_amt);
		}
		/*
		 * Set up DMA.
		 */
		s = sploff();
		outb(DMA_SINGLEMSK, 0x04 | 0x02);
		outb(DMA_FLIPFLOP, 0);
		outb(DMA_MODE, fdc->sc_mode);
		outb(DMA2_ADDR, pa);
		outb(DMA2_ADDR, pa >> 8);
		outb(DMA2_PAGE, pa >> 16);
		outb(DMA2_COUNT, fdc->sc_amt - 1);
		outb(DMA2_COUNT, (fdc->sc_amt - 1) >> 8);
		outb(DMA_SINGLEMSK, 0x02);
		splon(s);

		/*
		 * Issue command to FDC.
		 */
		fdc->sc_state = TRANSFERDONE;
		fdc->sc_cmd[0] = fdc->sc_op;
		fdc->sc_cmd[1] = (fdc->sc_tn << 2) | ui->slave;
		if (fdc->sc_op == CMD_FORMAT) {
			fdc->sc_cmd[2] = 0x02;
			fdc->sc_cmd[3] = dk->dk_nspt;
			fdc->sc_cmd[4] = dk->dk_fgap;
			fdc->sc_cmd[5] = 0xda;
			fdcmd(um, 6);
		} else {
			fdc->sc_cmd[2] = fdc->sc_cn;
			fdc->sc_cmd[3] = fdc->sc_tn;
			fdc->sc_cmd[4] = fdc->sc_sn + 1;
			fdc->sc_cmd[5] = 0x02;
			fdc->sc_cmd[6] = dk->dk_nspt;
			fdc->sc_cmd[7] = dk->dk_gap;
			fdc->sc_cmd[8] = 0xff;
			fdcmd(um, 9);
		}
		return;

	case TRANSFERDONE:
		/*
		 * Transfer complete.
		 */
		if (fdc->sc_st0 & ST0_IC) {
			fdc->sc_ioerr++;
			if (sc->sc_flags & FDF_AUTO) {
				/*
				 * Give up on second try if
				 * media detection is in progress.
				 */
				if (fdc->sc_ioerr == 2)
					goto bad;
				fdc->sc_state = RECAL;
				break;
			}
			if (fdc->sc_ioerr == MAX_RETRIES) {
				fderror(fderrmsg(ui), ui);
				goto bad;
			}
			/*
			 * Give up immediately on write-protected diskettes.
			 */
			if (fdc->sc_st1 & ST1_NW) {
				fderror("write-protected diskette", ui);
				goto bad;
			}
			/*
			 * Limit transfer to a single sector.
			 */
			fdc->sc_flags |= FDF_LIMIT;
			/*
			 * Every fourth attempt recalibrate the drive.
			 * Every eight attempt reset the controller.
			 * Also, every eighth attempt inform user
			 * about the error.
			 */
			if (fdc->sc_ioerr & 3)
				fdc->sc_state = TRANSFER;
			else if (fdc->sc_ioerr & 7)
				fdc->sc_state = RECAL;
			else {
				fdc->sc_state = RESET;
				fderror(fderrmsg(ui), ui);
			}
			break;
		}
		/*
		 * Transfer completed successfully.
		 * Advance counters/pointers, and if more
		 * is left, initiate I/O.
		 */
		if (fdc->sc_flags & FDF_BOUNCE) {
			fdc->sc_flags &= ~FDF_BOUNCE;
			bcopy((caddr_t)phystokv(fdc->sc_buf), fdc->sc_addr,
			      fdc->sc_amt);
		}
		if ((fdc->sc_resid -= fdc->sc_amt) == 0) {
			bp->b_resid = 0;
			fddone(ui, bp);
			return;
		}
		fdc->sc_state = TRANSFER;
		fdc->sc_ioerr = 0;
		fdc->sc_addr += fdc->sc_amt;
		if (fdc->sc_op == CMD_FORMAT) {
			fdc->sc_sn = 0;
			if (fdc->sc_tn == 1) {
				fdc->sc_tn = 0;
				fdc->sc_cn++;
				fdc->sc_state = SEEK;
			} else
				fdc->sc_tn = 1;
		} else {
			fdc->sc_sn += fdc->sc_amt / SECSIZE;
			while (fdc->sc_sn >= dk->dk_nspt) {
				fdc->sc_sn -= dk->dk_nspt;
				if (fdc->sc_tn == 1) {
					fdc->sc_tn = 0;
					fdc->sc_cn++;
					fdc->sc_state = SEEK;
				} else
					fdc->sc_tn = 1;
			}
		}
		break;

	default:
		printf("fd%d: invalid state\n", ui->unit);
		panic("fdstate");
		/*NOTREACHED*/
	}
 bad:
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	sc->sc_flags |= FDF_RECAL;
	fddone(ui, bp);
}

/*
 * Terminate current request and start
 * any others that are queued.
 */
int
fddone(ui, bp)
	struct bus_device *ui;
	struct buf *bp;
{
	struct bus_ctlr *um = ui->mi;
	struct fdsoftc *sc = &fdsoftc[ui->unit];
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];
	struct buf *dp = &fdtab[um->unit];

	DEBUGF(1, printf("fd%d: fddone()\n", ui->unit));

	/*
	 * Remove this request from queue.
	 */
	if (bp) {
		fdutab[ui->unit].b_actf = bp->b_actf;
		biodone(bp);
		bp = &fdutab[ui->unit];
		dp->b_actf = bp->b_forw;
	} else
		bp = &fdutab[ui->unit];

	/*
	 * Mark controller and drive idle.
	 */
	dp->b_active = 0;
	bp->b_active = 0;
	fdc->sc_state = IDLE;
	sc->sc_mticks = 0;
	fdc->sc_flags &= ~(FDF_LIMIT|FDF_BOUNCE);

	/*
	 * Start up other requests.
	 */
	fdustart(ui);
	fdstart(um);

	/*
	 * Wakeup anyone waiting for drive or controller.
	 */
	if (sc->sc_flags & FDF_WANT) {
		sc->sc_flags &= ~FDF_WANT;
		wakeup((void *)sc);
	}
	if (fdc->sc_flags & FDF_WANT) {
		fdc->sc_flags &= ~FDF_WANT;
		wakeup((void *)fdc);
	}
}

/*
 * Check if diskette change has occured since the last open.
 */
int
fddskchg(ui)
	struct bus_device *ui;
{
	int s, dir;
	struct fdsoftc *sc = &fdsoftc[ui->unit];
	struct bus_ctlr *um = ui->mi;
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	/*
	 * Get access to controller.
	 */
	s = splbio();	
	while (fdtab[um->unit].b_active) {
		fdc->sc_flags |= FDF_WANT;
		assert_wait((event_t)fdc, FALSE);
		thread_block((void (*)())0);
	}
	fdtab[um->unit].b_active = 1;
	fdutab[ui->unit].b_active = 1;

	/*
	 * Turn on drive motor and read digital input register.
	 */
	if (fdmotoron(ui)) {
		timeout(wakeup, (void *)fdc, hz / 2);
		assert_wait((event_t)fdc, FALSE);
		thread_block((void (*)())0);
	}
	dir = inb(FD_DIR(um->address));
	fddone(ui, NULL);
	splx(s);

	if (dir & DIR_DSKCHG) {
		printf("fd%d: diskette change detected\n", ui->unit);
		sc->sc_flags |= FDF_SEEK;
		return (1);
	}
	return (0);
}

/*
 * Do media detection.
 */
int
fdauto(dev)
	dev_t dev;
{
	int i, error = 0;
	struct buf *bp;
	struct bus_device *ui = fddinfo[fdunit(dev)];
	struct fdsoftc *sc = &fdsoftc[ui->unit];
	struct fddk *dk, *def = 0;

	sc->sc_flags |= FDF_AUTO;
	bp = (struct buf *)geteblk(SECSIZE);
	for (i = 0, dk = fddk; i < NDKTYPES; i++, dk++) {
		if ((dk->dk_drives & (1 << sc->sc_type)) == 0)
			continue;
		if (def == 0)
			def = dk;
		sc->sc_dk = dk;
		bp->b_flags = B_READ;
		bp->b_dev = dev;
		bp->b_bcount = SECSIZE;
		if (sc->sc_flags & FDF_SEEK) {
			sc->sc_flags &= ~FDF_SEEK;
			bp->b_blkno = 100;
		} else
			bp->b_blkno = 0;
		fdstrategy(bp);
		biowait(bp);
		if ((bp->b_flags & B_ERROR) == 0 || bp->b_error == ENXIO)
			break;
	}
	if (i == NDKTYPES) {
		printf("fd%d: couldn't detect type, using %s\n",
		       ui->unit, def->dk_name);
		sc->sc_dk = def;
	} else if ((bp->b_flags & B_ERROR) == 0)
		printf("fd%d: detected %s\n", ui->unit, sc->sc_dk->dk_name);
	else
		error = ENXIO;
	sc->sc_flags &= ~FDF_AUTO;
	brelse(bp);
	return (error);
}

/*
 * Turn on drive motor and select drive.
 */
int
fdmotoron(ui)
	struct bus_device *ui;
{
	int bit;
	struct bus_ctlr *um = ui->mi;
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	bit = 1 << (ui->slave + 4);
	if ((fdc->sc_dor & bit) == 0) {
		fdc->sc_dor &= ~3;
		fdc->sc_dor |= bit | ui->slave;
		outb(FD_DOR(um->address), fdc->sc_dor);
		return (1);
	}
	if ((fdc->sc_dor & 3) != ui->slave) {
		fdc->sc_dor &= ~3;
		fdc->sc_dor |= ui->slave;
		outb(FD_DOR(um->address), fdc->sc_dor);
	}
	return (0);
}

/*
 * Turn off drive motor.
 */
int
fdmotoroff(ui)
	struct bus_device *ui;
{
	struct bus_ctlr *um = ui->mi;
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	fdc->sc_dor &= ~(1 << (ui->slave + 4));
	outb(FD_DOR(um->address), fdc->sc_dor);
}

/*
 * This routine is invoked via timeout() by fdstart()
 * to call fdstate() at splbio.
 */
void
fdspinup(um)
	struct bus_ctlr *um;
{
	int s;

	s = splbio();
	fdstate(um);
	splx(s);
}

/*
 * Watchdog routine.
 * Check for hung operations.
 * Turn off motor of idle drives.
 */
void
fdwatch()
{
	int unit, s;
	struct bus_device *ui;

	timeout(fdwatch, 0, hz);
	s = splbio();
	for (unit = 0; unit < NFDC; unit++)
		if (fdtab[unit].b_active
		    && ++fdcsoftc[unit].sc_wticks == OP_TIMEOUT)
			fdintr(unit);
	for (unit = 0; unit < NFD; unit++) {
		if ((ui = fddinfo[unit]) == 0 || ui->alive == 0)
			continue;
		if (fdutab[unit].b_active == 0
		    && (fdcsoftc[ui->mi->unit].sc_dor & (1 << (ui->slave + 4)))
		    && ++fdsoftc[unit].sc_mticks == MOTOR_TIMEOUT)
			fdmotoroff(ui);
	}
	splx(s);
}

/*
 * Print an error message.
 */
int
fderror(msg, ui)
	char *msg;
	struct bus_device *ui;
{
	struct fdcsoftc *fdc = &fdcsoftc[ui->mi->unit];

	printf("fd%d: %s, %sing cn %d tn %d sn %d\n", ui->unit, msg,
	       (fdc->sc_op == CMD_READ ? "read"
		: (fdc->sc_op == CMD_WRITE ? "writ" : "formatt")),
	       fdc->sc_cn, fdc->sc_tn, fdc->sc_sn + 1);
}

/*
 * Return an error message for an I/O error.
 */
char *
fderrmsg(ui)
	struct bus_device *ui;
{
	struct fdcsoftc *fdc = &fdcsoftc[ui->mi->unit];

	if (fdc->sc_st1 & ST1_EC)
		return ("invalid sector");
	if (fdc->sc_st1 & ST1_DE)
		return ("CRC error");
	if (fdc->sc_st1 & ST1_OR)
		return ("DMA overrun");
	if (fdc->sc_st1 & ST1_ND)
		return ("sector not found");
	if (fdc->sc_st1 & ST1_NW)
		return ("write-protected diskette");
	if (fdc->sc_st1 & ST1_MA)
		return ("missing address mark");
	return ("hard error");
}

/*
 * Output a command to FDC.
 */
int
fdcmd(um, n)
	struct bus_ctlr *um;
	int n;
{
	int i, j;
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	for (i = j = 0; i < 200; i++) {
		if ((inb(FD_STATUS(um->address)) & (ST_RQM|ST_DIO)) != ST_RQM)
			continue;
		outb(FD_DATA(um->address), fdc->sc_cmd[j++]);
		if (--n == 0)
			return (1);
	}
	/*
	 * Controller is not responding, reset it.
	 */
	DEBUGF(1, printf("fdc%d: fdcmd() failed\n", um->unit));
	fdreset(um);
	return (0);
}

/*
 * Read results from FDC.
 */
int
fdresults(um, rp)
	struct bus_ctlr *um;
	u_char *rp;
{
	int i, j, status;

	for (i = j = 0; i < 200; i++) {
		status = inb(FD_STATUS(um->address));
		if ((status & ST_RQM) == 0)
			continue;
		if ((status & ST_DIO) == 0)
			return (j);
		if (j == 7)
			break;
		*rp++ = inb(FD_DATA(um->address));
		j++;
	}
	/*
	 * Controller is not responding, reset it.
	 */
	DEBUGF(1, printf("fdc%d: fdresults() failed\n", um->unit));
	fdreset(um);
	return (0);
}

/*
 * Reset controller.
 */
int
fdreset(um)
	struct bus_ctlr *um;
{
	struct fdcsoftc *fdc = &fdcsoftc[um->unit];

	outb(FD_DOR(um->address), fdc->sc_dor & ~(DOR_RSTCLR|DOR_IENABLE));
	fdc->sc_state = RESETDONE;
	fdc->sc_flags |= FDF_RESET;
	outb(FD_DOR(um->address), fdc->sc_dor);
}

#endif /* NFD > 0 */
