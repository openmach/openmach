/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 *	File: fdi_82077_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Driver for the Intel 82077 Floppy Disk Controller.
 */

#include <fd.h>
#if	NFD > 0

#include <mach/std_types.h>
#include <machine/machspl.h>
#include <chips/busses.h>

#include <chips/fdc_82077.h>
#include <platforms.h>

/* ---- */
#include <device/param.h>
#include <device/io_req.h>
#include <device/device_types.h>
#include <device/disk_status.h>
#define	UNITNO(d)	((d)>>5)
#define	SLAVENO(d)	(((d)>>3)&0x3)
#define	PARAMNO(d)	((d)&0x7)
/* ---- */

#ifdef	MAXINE

/* we can only take one */
#define	MAX_DRIVES		1

#define	my_fdc_type	fdc_82077aa
#define	the_fdc_type	fd->fdc_type
/* later: #define the_fdc_type my_fdc_type */

/* Registers are read/written as words, byte 0 */
/* padding is to x40 boundaries */
typedef struct {
	volatile unsigned int	fd_sra;		/* r:  status register A */
	int pad0[15];
	volatile unsigned int	fd_srb;		/* r:  status register B */
	int pad1[15];
	volatile unsigned int	fd_dor;		/* rw: digital output reg */
	int pad2[15];
	volatile unsigned int	fd_tdr;		/* rw: tape drive register */
	int pad3[15];
	volatile unsigned int	fd_msr;		/* r:  main status register */
/*#define			fd_dsr	fd_msr;	/* w:  data rate select reg */
	int pad4[15];
	volatile unsigned int	fd_data;	/* rw: fifo */
	int pad5[15];
	volatile unsigned int	fd_xxx;		/* --reserved-- */
	int pad6[15];
	volatile unsigned int	fd_dir;		/* r:  digital input reg */
/*#define			fd_ccr	fd_dir;	/* w:  config control reg */
} fd_padded_regmap_t;

#define	machdep_reset_8272a(f,r)

#else	/* MAXINE */

/* Pick your chip and padding */
#define	my_fdc_type		fdc_8272a
#define the_fdc_type		my_fdc_type

#define	fd_padded_regmap_t	fd_8272a_regmap_t

#define	machdep_reset_8272a(f,r)	1

#endif	/* MAXINE */


#ifndef	MAX_DRIVES
#define	MAX_DRIVES	DRIVES_PER_FDC
#endif

/*
 * Autoconf info
 */

static vm_offset_t fd_std[NFD] = { 0 };
static struct bus_device *fd_info[NFD];
static struct bus_ctlr	 *fd_minfo[NFD];
static int fd_probe(), fd_slave(), fd_go();
static void fd_attach();

struct bus_driver fd_driver =
       { fd_probe, fd_slave, fd_attach, fd_go, fd_std, "fd", fd_info,
         "fdc", fd_minfo, /*BUS_INTR_B4_PROBE*/};

/*
 * Externally visible functions
 */
int	fd_intr();					/* kernel */

/*
 * Media table
 *
 *      Cyls,Sec,spc,part,Mtype,RWFpl,FGpl
 */
typedef struct {
	unsigned char	d_cylperunit;
	unsigned char	d_secpercyl;
	unsigned short	d_secperunit;
	unsigned char	d_secpertrk;
	unsigned char	d_gpl;
	unsigned char	d_fgpl;
	unsigned char	d_xfer_rate;
} fd_params_t;

fd_params_t fd_params[8] = {
	{80, 18, 1440,  9, 0x2a, 0x50, FD_DSR_DD_250},	/* [0] 3.50" 720  Kb  */
	{80, 36, 2880, 18, 0x1b, 0x6c, FD_DSR_DD_500},	/* [1] 3.50" 1.44 Meg */
	{40, 18,  720,  9, 0x2a, 0x50, FD_DSR_DD_250},	/* [2] 5.25" 360  Kb  */
	{80, 30, 2400, 15, 0x1b, 0x54, FD_DSR_DD_500},	/* [3] 5.25" 1.20 Meg */
};

/*
 * Software status of chip
 */
struct fd_softc {
	fd_padded_regmap_t	*regs;
	char			fdc_type;
	char			fdc_mode;
	char			messed_up;
	char			slave_active;
	struct slave_t {
		io_req_t	ior;
		decl_simple_lock_data(,slave_lock)

		/* status at end of last command */
		unsigned char	st0;
		unsigned char	st1;
		unsigned char	st2;
		unsigned char	c;
		unsigned char	h;
		unsigned char	r;
		unsigned char	n;
		unsigned char	st3;
		/* ... */
		unsigned char	medium_status;
#		define	ST_MEDIUM_PRESENT	1
#		define	ST_MEDIUM_KNOWN		2
		char		last_command;
		char		bytes_expected;
		fd_params_t	*params;

	} slave_status[DRIVES_PER_FDC];
} fd_softc_data[NFD];

typedef struct fd_softc	*fd_softc_t;

fd_softc_t	fd_softc[NFD];

static char *chip_names[4] = { "8272-A", "82072", "82077-AA", 0 };
static char *mode_names[4] = { "PC AT", "PS/2", "Model 30", 0 };

/*
 * Probe chip to see if it is there
 */
static fd_probe (reg, ctlr)
	vm_offset_t	reg;
	struct bus_ctlr *ctlr;
{
	int		unit = ctlr->unit;
	fd_softc_t	fd;
	fd_padded_regmap_t	*regs;

	/*
	 * See if we are here
	 */
	if (check_memory(reg, 0)) {
		/* no rides today */
		return 0;
	}

	fd = &fd_softc_data[unit];
	fd_softc[unit] = fd;

	regs = (fd_padded_regmap_t *)reg;
	fd->regs = regs;
	fd->fdc_type = my_fdc_type;

	fd_reset(fd);

	if (the_fdc_type == fdc_82077aa) {
		/* See if properly functioning */
		unsigned char	temp = FD_CMD_VERSION;
		if (!fd_go(fd, 0, &temp, 1, 1))
			return 0;	/* total brxage */
		if (!fd_get_result(fd, &temp, 1, FALSE))
			return 0;	/* partial brxage */
		if (temp != FD_VERSION_82077AA)
			printf( "{ %s x%x } ",
				"Accepting non-82077aa version id",
				temp);
	}

	printf("%s%d: %s chip controller",
		ctlr->name, ctlr->unit, chip_names[fd->fdc_type]);
	if (the_fdc_type == fdc_82077aa)
		printf(" in %s mode", mode_names[fd->fdc_mode]);
	printf(".\n");

	return 1;
}

/* See if we like this slave */
static fd_slave(ui, reg)
	struct bus_device	*ui;
	vm_offset_t		reg;
{
	int			slave = ui->slave;
	fd_softc_t		fd;
	unsigned char		sns[2];

	if (slave >= MAX_DRIVES) return 0;

	fd = fd_softc[ui->ctlr];

	sns[0] = FD_CMD_SENSE_DRIVE_STATUS;
	sns[1] = slave & 0x3;
	if (the_fdc_type == fdc_82072)
		sns[1] |= FD_CMD_SDS_NO_MOT;
	if (!fd_go(fd, slave, sns, 2, 1)) return 0;
	if (!fd_get_result(fd, sns, 1, FALSE)) return 0;

	fd->slave_status[slave].st3 = sns[0];

	return 1;
}

static void
fd_attach (ui)
	struct bus_device *ui;
{
	/* Attach a slave */
}

static boolean_t
fd_go(fd, slave, cmd, cmdlen, reply_count)
	fd_softc_t      fd;
	unsigned char	cmd[];
{

	/* XXX check who active, enque ifnot */

	fd->slave_active = slave;
	fd->slave_status[slave].bytes_expected = reply_count;
	fd->slave_status[slave].last_command = *cmd;
	return fd_command(fd, cmd, cmdlen);
}

fd_intr (unit, spllevel)
{
	fd_softc_t      fd;
	fd_padded_regmap_t *regs;
	unsigned char	msr;
	register struct slave_t	*slv;


	splx(spllevel);

	fd = fd_softc[unit];
	regs = fd->regs;

	/* did polling see a media change */
	/* busy bit in msr sez ifasync or not */

	msr = regs->fd_msr;
	if ((msr & (FD_MSR_RQM|FD_MSR_DIO)) == (FD_MSR_RQM|FD_MSR_DIO)) {

		/* result phase */
*(unsigned int *)0xbc040100 &= ~0x00600000;

		slv = &fd->slave_status[fd->slave_active];
		fd_get_result(fd, &slv->st0, slv->bytes_expected, FALSE);
		fd_start(fd, fd->slave_active, TRUE);
		return;
	}
	/* async interrupt, either seek complete or media change */
	while (1) {
		unsigned char   st[2];
		register int	slave, m;

		*st = FD_CMD_SENSE_INT_STATUS;
		fd_command(fd, st, 1);

		fd_get_result(fd, st, 2, FALSE);

		slave = *st & FD_ST0_DS;
		slv = &fd->slave_status[slave];
		slv->c = st[1];

		switch (*st & FD_ST0_IC_MASK) {

		    case FD_ST0_IC_OK:
/* we get an FD_ST0_SE for RECALIBRATE. Wait for it or discard ? */

		    case FD_ST0_IC_AT:

		    case FD_ST0_IC_BAD_CMD:
			return;

		    case FD_ST0_IC_AT_POLL:
			m = slv->medium_status;
			if (m & ST_MEDIUM_PRESENT)
				m &= ~ST_MEDIUM_PRESENT;
			else
				m |= ST_MEDIUM_PRESENT;
			slv->medium_status = m;
		}
	}
}

/*
 * Non-interface functions and utilities
 */

fd_reset(fd)
	fd_softc_t	fd;
{
	register	fd_padded_regmap_t	*regs;

	regs = fd->regs;

	/*
	 * Reset the chip
	 */
	if (the_fdc_type == fdc_82072)
		/* Fix if your box uses an external PLL */
		regs->fd_dsr = FD_DSR_RESET | FD_DSR_EPLL;
	else if (the_fdc_type == fdc_82077aa)
		regs->fd_dor = 0;
	else
		machdep_reset_8272a(fd, regs);

	delay(5);	/* 4usecs in specs */

	/*
	 * Be smart with the smart ones
	 */
	if (the_fdc_type == fdc_82077aa) {

		/*
		 * See in which mood we are (it cannot be changed)
		 */
		int	temp;

		/* Take chip out of hw reset */
		regs->fd_dor = FD_DOR_ENABLE | FD_DOR_DMA_GATE;
		delay(10);

		/* what do we readback from the DIR register as datarate ? */
		regs->fd_ccr = FD_DSR_SD_125;
		delay(10);

		temp = regs->fd_dir;
		if ((temp & 0x7) == FD_DSR_SD_125)
			fd->fdc_mode = mod30_mode;
		else if ((temp & (FD_DIR_ones | FD_DIR_DR_MASK_PS2)) ==
			 ((FD_DSR_SD_125 << FD_DIR_DR_SHIFT_PS2) | FD_DIR_ones))
			fd->fdc_mode = ps2_mode;
		else
			/* this assumes tri-stated bits 1&2 read the same */
			fd->fdc_mode = at_mode;

	}

	/*
	 * Send at least 4 sense interrupt cmds, one per slave
	 */
	{

		unsigned char	sns, st[2];
		int		i, nloops;

		sns = FD_CMD_SENSE_INT_STATUS;
		i   = nloops = 0;

		do {
			nloops++;

			(void) fd_command(fd, &sns, 1);

			st[0] = 0; /* in case bad status */
			(void) fd_get_result(fd, st, 2, TRUE);

			if ((st[0] & FD_ST0_IC_MASK) == FD_ST0_IC_AT_POLL) {
				register int 	slave;

				slave = st[0] & FD_ST0_DS;
				fd->slave_status[slave].st0 = st[0];
				fd->slave_status[slave].c   = st[1];
				i++;
			}
		} while ( (nloops < 30) &&
			  ((i < 4) || (st[0] != FD_ST0_IC_BAD_CMD)) );

		/* sanity check */
		if (nloops == 30) {
			(void) fd_messed_up(fd);
			return;
		}
	}

	/*
	 * Install current parameters
	 */
	if (the_fdc_type != fdc_8272a) {

		unsigned char	cnf[4];

		/* send configure command to turn polling off */
		cnf[0] = FD_CMD_CONFIGURE;
		cnf[1] = 0x60; /* moff 110 */
		cnf[2] = 0x48; /* eis, poll, thr=8 */
		cnf[3] = 0;
		if (!fd_command(fd, cnf, 4))
			return;
		/* no status */
	}

	/*
	 * Send specify to select defaults
	 */
	{
		unsigned char	sfy[3];

		sfy[0] = FD_CMD_SPECIFY;
#if 0
		sfy[1] = (12 << 4) | 7; /* step 4, hut 112us @500 */
		sfy[2] = 2 << 1; /* hlt 29us @500 */
#else
		sfy[1] = (13 << 4) | 15;
		sfy[2] = 1 << 1;
#endif
		(void) fd_command(fd, sfy, 3);
		/* no status */
	}
}

#define	FD_MAX_WAIT	1000

boolean_t
fd_command(fd, cmd, cmd_len)
	fd_softc_t	fd;
	char		*cmd;
{
	register fd_padded_regmap_t	*regs;

	regs = fd->regs;

	while (cmd_len > 0) {
		register int i, s;

		/* there might be long delays, so we pay this price */
		s = splhigh();
		for (i = 0; i < FD_MAX_WAIT; i++)
			if ((regs->fd_msr & (FD_MSR_RQM|FD_MSR_DIO)) ==
			    FD_MSR_RQM)
				break;
			else
				delay(10);
		if (i == FD_MAX_WAIT) {
			splx(s);
			return fd_messed_up(fd);
		}
		regs->fd_data = *cmd++;
		splx(s);
		if (--cmd_len) delay(12);
	}

	return TRUE;
}

boolean_t
fd_get_result(fd, st, st_len, ignore_errors)
	fd_softc_t	fd;
	char		*st;
{
	register fd_padded_regmap_t	*regs;

	regs = fd->regs;

	while (st_len > 0) {
		register int i, s;

		/* there might be long delays, so we pay this price */
		s = splhigh();
		for (i = 0; i < FD_MAX_WAIT; i++)
			if ((regs->fd_msr & (FD_MSR_RQM|FD_MSR_DIO)) ==
			    (FD_MSR_RQM|FD_MSR_DIO))
				break;
			else
				delay(10);
		if (i == FD_MAX_WAIT) {
			splx(s);
			return (ignore_errors) ? FALSE : fd_messed_up(fd);
		}
		*st++ = regs->fd_data;
		splx(s);
		st_len--;
	}

	return TRUE;
}


boolean_t
fd_messed_up(fd)
	fd_softc_t	fd;
{
	fd->messed_up++;
	printf("fd%d: messed up, disabling.\n", fd - fd_softc_data);
	/* here code to 
		ior->error = ..;
		restart
	 */
	return FALSE;
}

/*
 * Debugging aids
 */

fd_state(unit)
{
	fd_softc_t	fd = fd_softc[unit];
	fd_padded_regmap_t	*regs;

	if (!fd || !fd->regs) return 0;
	regs = fd->regs;
	if (the_fdc_type == fdc_8272a)
		printf("msr %x\n", regs->fd_msr);
	else
		printf("sra %x srb %x dor %x tdr %x msr %x dir %x\n",
			regs->fd_sra, regs->fd_srb, regs->fd_dor,
			regs->fd_tdr, regs->fd_msr, regs->fd_dir);
}

#endif

/*   to be moved in separate file, or the above modified to live with scsi */

fd_open(dev, mode, ior)
	int		dev;
	dev_mode_t	mode;
	io_req_t	ior;
{
	unsigned char	cmd[2];
	fd_softc_t      fd;
	int		slave;

	fd = fd_softc[UNITNO(dev)];
	slave = SLAVENO(dev);

	/* XXX find out what medium we have, automagically XXX */
	/* fornow, set params depending on minor */
	fd->slave_status[slave].params = &fd_params[PARAMNO(dev)];

	/* XXXYYYXXXYYY SEND CONFIGURE if params changed */

	/* Turn motor on */
	if (the_fdc_type == fdc_82072) {

		cmd[0] = FD_CMD_MOTOR_ON_OFF | FD_CMD_MOT_ON |
			 ((slave << FD_CMD_MOT_DS_SHIFT) & FD_CMD_MOT_DS);
		(void) fd_go(fd, slave, cmd, 1, 0);
		/* no status */

	} else if (the_fdc_type == fdc_82077aa) {

		fd->regs->fd_dor |= ((1<<slave)<<4);
	}

	/* recalibrate to track 0 */
	cmd[0] = FD_CMD_RECALIBRATE;
	cmd[1] = slave;
	if (!fd_go(fd, slave, cmd, 2, 0))
		return D_DEVICE_DOWN;
	/* will generate a completion interrupt */

	/* if not writeable return D_READ_ONLY ? */

	return D_SUCCESS;
}

fd_close(dev)
	int		dev;
{
        fd_softc_t      fd;
	register int	slave;
	unsigned char	cmd[2];

        slave = SLAVENO(dev);
        fd = fd_softc[UNITNO(dev)];

	/* do not delete media info, do that iff interrupt sez changed */

	/* Turn motor off */
	if (the_fdc_type == fdc_82072) {

		cmd[0] = FD_CMD_MOTOR_ON_OFF |
			 ((slave << FD_CMD_MOT_DS_SHIFT) & FD_CMD_MOT_DS);
		(void) fd_go(fd, 0, cmd, 1, 0);
		/* no status */

	} else if (the_fdc_type == fdc_82077aa) {

		fd->regs->fd_dor &= ~((1<<slave)<<4);
	}
	return D_SUCCESS;
}

fd_strategy(ior)
	io_req_t	ior;
{
#if 0
	if (ior->io_op & IO_READ)
		bzero(ior->io_data, ior->io_count);
	iodone(ior);
#else
	struct slave_t	*slv;
        fd_softc_t      fd;
	unsigned int	i, rec, max, dev;
	fd_params_t	*params;

	/* readonly */

	dev = ior->io_unit;

	/* only one partition */
	fd = fd_softc[UNITNO(dev)];
	slv = &fd->slave_status[SLAVENO(dev)];
	params = slv->params;
	max = params->d_secperunit;
	rec = ior->io_recnum;
	i = btodb(ior->io_count + DEV_BSIZE - 1);
	if (((rec + i) > max) || (ior->io_count < 0)) {
		ior->io_error = D_INVALID_SIZE;
		ior->io_op |= IO_ERROR;
		ior->io_residual = ior->io_count;
		iodone(ior);
		return;
	}

	ior->io_residual = rec / params->d_secpercyl;

	/*
	 * Enqueue operation 
	 */
	i = splbio();
	simple_lock(&slv->slave_lock);
	if (slv->ior) {
		disksort(slv->ior, ior);
		simple_unlock(&slv->slave_lock);
	} else {
		ior->io_next = 0;
		slv->ior = ior;
		simple_unlock(&slv->slave_lock);
		fd_start(fd, SLAVENO(dev), FALSE);
	}
	splx(i);
#endif
}

fd_start(fd, slave, done)
	boolean_t	done;
        fd_softc_t      fd;
{
	register io_req_t	ior;
	struct slave_t	*slv;

	slv = &fd->slave_status[slave];
	if ((ior = slv->ior) == 0)
		return;

	if (done) {
		/* .. errors .. */
		/* .. partial xfers .. */

		/* dequeue next one */
		{
			io_req_t	next;

			simple_lock(&slv->target_lock);
			next = ior->io_next;
			slv->ior = next;
			simple_unlock(&slv->target_lock);

			iodone(ior);
			if (next == 0)
				return;

			ior = next;
		}
	}

#ifdef	no_eis
	if (slv->c != ior->io_residual)	SEEK_it;
#endif

/*	setup dma	*/
#if 1
	if (ior->io_op & IO_READ)	/* like SCSI */
#else
	if ((ior->io_op & IO_READ) == 0)
#endif
	{
		*(unsigned int *)0xbc040100 |= 0x00200000 | 0x00400000;
	} else {
		*(unsigned int *)0xbc040100 &= ~0x00400000;
		*(unsigned int *)0xbc040100 |= 0x00200000;
	}
	*(unsigned int *)0xbc040070 = (((unsigned int)kvtophys(ior->io_data))>>2)<<5;
	*(unsigned int *)0xbc0401a0 = 13;

#ifdef	no_eis
	if (slv->c == ior->io_residual)	{
#else
	{
#endif
		unsigned char	cmd[9];
		unsigned char	head, sec;
		fd_params_t	*params;

		params = slv->params;

		fd->regs->fd_dsr = params->d_xfer_rate;

		sec = ior->io_recnum % params->d_secpercyl;
		head = sec / params->d_secpertrk;
		sec = (sec % params->d_secpertrk);

		cmd[0] = (ior->io_op & IO_READ) ?
				FD_CMD_MT | FD_CMD_MFM | FD_CMD_READ_DATA :
				FD_CMD_MT | FD_CMD_MFM | FD_CMD_WRITE_DATA;
		cmd[1] = (head << 2) | slave;
		cmd[2] = ior->io_residual;
		cmd[3] = head;
		cmd[4] = sec + 1;	/* 0 starts at 1 :-) */
		cmd[5] = 0x2;		/* 512 byte sectors */
		cmd[6] = params->d_secpertrk;
		cmd[7] = params->d_gpl;
		cmd[8] = 0xff;

		fd_go( fd, slave, cmd, 9, 7);

	}
}

extern minphys();

fd_read(dev, ior)
	int		dev;
	io_req_t	ior;
{
	return block_io(fd_strategy, minphys, ior);
}

int fdc_write_enable = 1;

fd_write(dev, ior)
	int		dev;
	io_req_t	ior;
{
/*	check if writeable */

if (fdc_write_enable)
	return block_io(fd_strategy, minphys, ior);
else return D_SUCCESS;
}

fd_set_status(dev, flavor, status, status_count)
	int		dev;
	int		flavor;
	dev_status_t	status;
	unsigned int	*status_count;
{
	printf("fdc_set_status(%x, %x, %x, %x)", dev, flavor, status, status_count);
	return D_SUCCESS;
}

fd_get_status(dev, flavor, status, status_count)
	int		dev;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	printf("fdc_get_status(%x, %x, %x, %x)", dev, flavor, status, status_count);
	return D_SUCCESS;
}

