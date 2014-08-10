/*
 *  linux/drivers/block/ide.c	Version 5.28  Feb 11, 1996
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors (see below)
 */
#define _IDE_C		/* needed by <linux/blk.h> */

/*
 * This is the multiple IDE interface driver, as evolved from hd.c.
 * It supports up to four IDE interfaces, on one or more IRQs (usually 14 & 15).
 * There can be up to two drives per interface, as per the ATA-2 spec.
 *
 * Primary i/f:    ide0: major=3;  (hda)         minor=0; (hdb)         minor=64
 * Secondary i/f:  ide1: major=22; (hdc or hd1a) minor=0; (hdd or hd1b) minor=64
 * Tertiary i/f:   ide2: major=33; (hde)         minor=0; (hdf)         minor=64
 * Quaternary i/f: ide3: major=34; (hdg)         minor=0; (hdh)         minor=64
 *
 * It is easy to extend ide.c to handle more than four interfaces:
 *
 *	Change the MAX_HWIFS constant in ide.h.
 *
 *	Define some new major numbers (in major.h), and insert them into
 *	the ide_hwif_to_major table in ide.c.
 *
 *	Fill in the extra values for the new interfaces into the two tables
 *	inside ide.c:  default_io_base[]  and  default_irqs[].
 *
 *	Create the new request handlers by cloning "do_ide3_request()"
 *	for each new interface, and add them to the switch statement
 *	in the ide_init() function in ide.c.
 *
 *	Recompile, create the new /dev/ entries, and it will probably work.
 *
 *  From hd.c:
 *  |
 *  | It traverses the request-list, using interrupts to jump between functions.
 *  | As nearly all functions can be called within interrupts, we may not sleep.
 *  | Special care is recommended.  Have Fun!
 *  |
 *  | modified by Drew Eckhardt to check nr of hd's from the CMOS.
 *  |
 *  | Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  | in the early extended-partition checks and added DM partitions.
 *  |
 *  | Early work on error handling by Mika Liljeberg (liljeber@cs.Helsinki.FI).
 *  |
 *  | IRQ-unmask, drive-id, multiple-mode, support for ">16 heads",
 *  | and general streamlining by Mark Lord (mlord@bnr.ca).
 *
 *  October, 1994 -- Complete line-by-line overhaul for linux 1.1.x, by:
 *
 *	Mark Lord	(mlord@bnr.ca)			(IDE Perf.Pkg)
 *	Delman Lee	(delman@mipg.upenn.edu)		("Mr. atdisk2")
 *	Petri Mattila	(ptjmatti@kruuna.helsinki.fi)	(EIDE stuff)
 *	Scott Snyder	(snyder@fnald0.fnal.gov)	(ATAPI IDE cd-rom)
 *
 *  Maintained by Mark Lord (mlord@bnr.ca):  ide.c, ide.h, triton.c, hd.c, ..
 *
 *  This was a rewrite of just about everything from hd.c, though some original
 *  code is still sprinkled about.  Think of it as a major evolution, with
 *  inspiration from lots of linux users, esp.  hamish@zot.apana.org.au
 *
 *  Version 1.0 ALPHA	initial code, primary i/f working okay
 *  Version 1.3 BETA	dual i/f on shared irq tested & working!
 *  Version 1.4 BETA	added auto probing for irq(s)
 *  Version 1.5 BETA	added ALPHA (untested) support for IDE cd-roms,
 *  ...
 *  Version 3.5		correct the bios_cyl field if it's too small
 *  (linux 1.1.76)	 (to help fdisk with brain-dead BIOSs)
 *  Version 3.6		cosmetic corrections to comments and stuff
 *  (linux 1.1.77)	reorganise probing code to make it understandable
 *			added halfway retry to probing for drive identification
 *			added "hdx=noprobe" command line option
 *			allow setting multmode even when identification fails
 *  Version 3.7		move set_geometry=1 from do_identify() to ide_init()
 *			increase DRQ_WAIT to eliminate nuisance messages
 *			wait for DRQ_STAT instead of DATA_READY during probing
 *			  (courtesy of Gary Thomas gary@efland.UU.NET)
 *  Version 3.8		fixed byte-swapping for confused Mitsumi cdrom drives
 *			update of ide-cd.c from Scott, allows blocksize=1024
 *			cdrom probe fixes, inspired by jprang@uni-duisburg.de
 *  Version 3.9		don't use LBA if lba_capacity looks funny
 *			correct the drive capacity calculations
 *			fix probing for old Seagates without IDE_ALTSTATUS_REG
 *			fix byte-ordering for some NEC cdrom drives
 *  Version 3.10	disable multiple mode by default; was causing trouble
 *  Version 3.11	fix mis-identification of old WD disks as cdroms
 *  Version 3,12	simplify logic for selecting initial mult_count
 *			  (fixes problems with buggy WD drives)
 *  Version 3.13	remove excess "multiple mode disabled" messages
 *  Version 3.14	fix ide_error() handling of BUSY_STAT
 *			fix byte-swapped cdrom strings (again.. arghh!)
 *			ignore INDEX bit when checking the ALTSTATUS reg
 *  Version 3.15	add SINGLE_THREADED flag for use with dual-CMD i/f
 *			ignore WRERR_STAT for non-write operations
 *			added vlb_sync support for DC-2000A & others,
 *			 (incl. some Promise chips), courtesy of Frank Gockel
 *  Version 3.16	convert vlb_32bit and vlb_sync into runtime flags
 *			add ioctls to get/set VLB flags (HDIO_[SG]ET_CHIPSET)
 *			rename SINGLE_THREADED to SUPPORT_SERIALIZE,
 *			add boot flag to "serialize" operation for CMD i/f
 *			add optional support for DTC2278 interfaces,
 *			 courtesy of andy@cercle.cts.com (Dyan Wile).
 *			add boot flag to enable "dtc2278" probe
 *			add probe to avoid EATA (SCSI) interfaces,
 *			 courtesy of neuffer@goofy.zdv.uni-mainz.de.
 *  Version 4.00	tidy up verify_area() calls - heiko@colossus.escape.de
 *			add flag to ignore WRERR_STAT for some drives
 *			 courtesy of David.H.West@um.cc.umich.edu
 *			assembly syntax tweak to vlb_sync
 *			removeable drive support from scuba@cs.tu-berlin.de
 *			add transparent support for DiskManager-6.0x "Dynamic
 *			 Disk Overlay" (DDO), most of this is in genhd.c
 *			eliminate "multiple mode turned off" message at boot
 *  Version 4.10	fix bug in ioctl for "hdparm -c3"
 *			fix DM6:DDO support -- now works with LILO, fdisk, ...
 *			don't treat some naughty WD drives as removeable
 *  Version 4.11	updated DM6 support using info provided by OnTrack
 *  Version 5.00	major overhaul, multmode setting fixed, vlb_sync fixed
 *			added support for 3rd/4th/alternative IDE ports
 *			created ide.h; ide-cd.c now compiles separate from ide.c
 *			hopefully fixed infinite "unexpected_intr" from cdroms
 *			zillions of other changes and restructuring
 *			somehow reduced overall memory usage by several kB
 *			probably slowed things down slightly, but worth it
 *  Version 5.01	AT LAST!!  Finally understood why "unexpected_intr"
 *			 was happening at various times/places:  whenever the
 *			 ide-interface's ctl_port was used to "mask" the irq,
 *			 it also would trigger an edge in the process of masking
 *			 which would result in a self-inflicted interrupt!!
 *			 (such a stupid way to build a hardware interrupt mask).
 *			 This is now fixed (after a year of head-scratching).
 *  Version 5.02	got rid of need for {enable,disable}_irq_list()
 *  Version 5.03	tune-ups, comments, remove "busy wait" from drive resets
 *			removed PROBE_FOR_IRQS option -- no longer needed
 *			OOOPS!  fixed "bad access" bug for 2nd drive on an i/f
 *  Version 5.04	changed "ira %d" to "irq %d" in DEBUG message
 *			added more comments, cleaned up unexpected_intr()
 *			OOOPS!  fixed null pointer problem in ide reset code
 *			added autodetect for Triton chipset -- no effect yet
 *  Version 5.05	OOOPS!  fixed bug in revalidate_disk()
 *			OOOPS!  fixed bug in ide_do_request()
 *			added ATAPI reset sequence for cdroms
 *  Version 5.10	added Bus-Mastered DMA support for Triton Chipset
 *			some (mostly) cosmetic changes
 *  Version 5.11	added ht6560b support by malafoss@snakemail.hut.fi
 *			reworked PCI scanning code
 *			added automatic RZ1000 detection/support
 *			added automatic PCI CMD640 detection/support
 *			added option for VLB CMD640 support
 *			tweaked probe to find cdrom on hdb with disks on hda,hdc
 *  Version 5.12	some performance tuning
 *			added message to alert user to bad /dev/hd[cd] entries
 *			OOOPS!  fixed bug in atapi reset
 *			driver now forces "serialize" again for all cmd640 chips
 *			noticed REALLY_SLOW_IO had no effect, moved it to ide.c
 *			made do_drive_cmd() into public ide_do_drive_cmd()
 *  Version 5.13	fixed typo ('B'), thanks to houston@boyd.geog.mcgill.ca
 *			fixed ht6560b support
 *  Version 5.13b (sss)	fix problem in calling ide_cdrom_setup()
 *			don't bother invalidating nonexistent partitions
 *  Version 5.14	fixes to cmd640 support.. maybe it works now(?)
 *			added & tested full EZ-DRIVE support -- don't use LILO!
 *			don't enable 2nd CMD640 PCI port during init - conflict
 *  Version 5.15	bug fix in init_cmd640_vlb()
 *			bug fix in interrupt sharing code
 *  Version 5.16	ugh.. fix "serialize" support, broken in 5.15
 *			remove "Huh?" from cmd640 code
 *			added qd6580 interface speed select from Colten Edwards
 *  Version 5.17	kludge around bug in BIOS32 on Intel triton motherboards
 *  Version 5.18	new CMD640 code, moved to cmd640.c, #include'd for now
 *			new UMC8672 code, moved to umc8672.c, #include'd for now
 *			disallow turning on DMA when h/w not capable of DMA
 *  Version 5.19	fix potential infinite timeout on resets
 *			extend reset poll into a general purpose polling scheme
 *			add atapi tape drive support from Gadi Oxman
 *			simplify exit from _intr routines -- no IDE_DO_REQUEST
 *  Version 5.20	leave current rq on blkdev request list during I/O
 *			generalized ide_do_drive_cmd() for tape/cdrom driver use
 *  Version 5.21	fix nasty cdrom/tape bug (ide_preempt was messed up)
 *  Version 5.22	fix ide_xlate_1024() to work with/without drive->id
 *  Version 5.23	miscellaneous touch-ups
 *  Version 5.24	fix #if's for SUPPORT_CMD640
 *  Version 5.25	more touch-ups, fix cdrom resets, ...
 *			cmd640.c now configs/compiles separate from ide.c
 *  Version 5.26	keep_settings now maintains the using_dma flag
 *			fix [EZD] remap message to only output at boot time
 *			fix "bad /dev/ entry" message to say hdc, not hdc0
 *			fix ide_xlate_1024() to respect user specified CHS
 *			use CHS from partn table if it looks translated
 *			re-merged flags chipset,vlb_32bit,vlb_sync into io_32bit
 *			keep track of interface chipset type, when known
 *			add generic PIO mode "tuneproc" mechanism
 *			fix cmd640_vlb option
 *			fix ht6560b support (was completely broken)
 *			umc8672.c now configures/compiles separate from ide.c
 *			move dtc2278 support to dtc2278.c
 *			move ht6560b support to ht6560b.c
 *			move qd6580  support to qd6580.c
 *			add  ali14xx support in ali14xx.c
 * Version 5.27		add [no]autotune parameters to help cmd640
 *			move rz1000  support to rz1000.c
 * Version 5.28		#include "ide_modes.h"
 *			fix disallow_unmask: now per-interface "no_unmask" bit
 *			force io_32bit to be the same on drive pairs of dtc2278
 *			improved IDE tape error handling, and tape DMA support
 *			bugfix in ide_do_drive_cmd() for cdroms + serialize
 *
 *  Some additional driver compile-time options are in ide.h
 *
 *  To do, in likely order of completion:
 *	- add Promise DC4030VL support from peterd@pnd-pc.demon.co.uk
 *	- modify kernel to obtain BIOS geometry for drives on 2nd/3rd/4th i/f
*/

#if defined (MACH) && !defined (LINUX_IDE_DEBUG)
#undef DEBUG
#endif

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <linux/malloc.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/segment.h>
#include <asm/io.h>

#ifdef CONFIG_PCI
#include <linux/bios32.h>
#include <linux/pci.h>
#endif /* CONFIG_PCI */

#include "ide.h"
#include "ide_modes.h"

static ide_hwgroup_t	*irq_to_hwgroup [NR_IRQS];
static const byte	ide_hwif_to_major[MAX_HWIFS] = {IDE0_MAJOR, IDE1_MAJOR, IDE2_MAJOR, IDE3_MAJOR};

static const unsigned short default_io_base[MAX_HWIFS] = {0x1f0, 0x170, 0x1e8, 0x168};
static const byte	default_irqs[MAX_HWIFS]     = {14, 15, 11, 10};

#if (DISK_RECOVERY_TIME > 0)
/*
 * For really screwy hardware (hey, at least it *can* be used with Linux)
 * we can enforce a minimum delay time between successive operations.
 */
static unsigned long read_timer(void)
{
	unsigned long t, flags;
	int i;

	save_flags(flags);
	cli();
	t = jiffies * 11932;
    	outb_p(0, 0x43);
	i = inb_p(0x40);
	i |= inb(0x40) << 8;
	restore_flags(flags);
	return (t - i);
}

static void set_recovery_timer (ide_hwif_t *hwif)
{
	hwif->last_time = read_timer();
}
#define SET_RECOVERY_TIMER(drive) set_recovery_timer (drive)

#else

#define SET_RECOVERY_TIMER(drive)

#endif /* DISK_RECOVERY_TIME */

/*
 * init_ide_data() sets reasonable default values into all fields
 * of all instances of the hwifs and drives, but only on the first call.
 * Subsequent calls have no effect (they don't wipe out anything).
 *
 * This routine is normally called at driver initialization time,
 * but may also be called MUCH earlier during kernel "command-line"
 * parameter processing.  As such, we cannot depend on any other parts
 * of the kernel (such as memory allocation) to be functioning yet.
 *
 * This is too bad, as otherwise we could dynamically allocate the
 * ide_drive_t structs as needed, rather than always consuming memory
 * for the max possible number (MAX_HWIFS * MAX_DRIVES) of them.
 */
#define MAGIC_COOKIE 0x12345678
static void init_ide_data (void)
{
	byte *p;
	unsigned int h, unit;
	static unsigned long magic_cookie = MAGIC_COOKIE;

	if (magic_cookie != MAGIC_COOKIE)
		return;		/* already initialized */
	magic_cookie = 0;

	for (h = 0; h < NR_IRQS; ++h)
		 irq_to_hwgroup[h] = NULL;

	/* bulk initialize hwif & drive info with zeros */
	p = ((byte *) ide_hwifs) + sizeof(ide_hwifs);
	do {
		*--p = 0;
	} while (p > (byte *) ide_hwifs);

	/* fill in any non-zero initial values */
	for (h = 0; h < MAX_HWIFS; ++h) {
		ide_hwif_t *hwif = &ide_hwifs[h];

		hwif->index     = h;
		hwif->noprobe	= (h > 1);
		hwif->io_base	= default_io_base[h];
		hwif->ctl_port	= hwif->io_base ? hwif->io_base+0x206 : 0x000;
#ifdef CONFIG_BLK_DEV_HD
		if (hwif->io_base == HD_DATA)
			hwif->noprobe = 1; /* may be overriden by ide_setup() */
#endif /* CONFIG_BLK_DEV_HD */
		hwif->major	= ide_hwif_to_major[h];
		hwif->name[0]	= 'i';
		hwif->name[1]	= 'd';
		hwif->name[2]	= 'e';
		hwif->name[3]	= '0' + h;
#ifdef CONFIG_BLK_DEV_IDETAPE
		hwif->tape_drive = NULL;
#endif /* CONFIG_BLK_DEV_IDETAPE */
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];

			drive->select.all		= (unit<<4)|0xa0;
			drive->hwif			= hwif;
			drive->ctl			= 0x08;
			drive->ready_stat		= READY_STAT;
			drive->bad_wstat		= BAD_W_STAT;
			drive->special.b.recalibrate	= 1;
			drive->special.b.set_geometry	= 1;
			drive->name[0]			= 'h';
			drive->name[1]			= 'd';
			drive->name[2]			= 'a' + (h * MAX_DRIVES) + unit;
		}
	}
}

#if SUPPORT_VLB_SYNC
/*
 * Some localbus EIDE interfaces require a special access sequence
 * when using 32-bit I/O instructions to transfer data.  We call this
 * the "vlb_sync" sequence, which consists of three successive reads
 * of the sector count register location, with interrupts disabled
 * to ensure that the reads all happen together.
 */
static inline void do_vlb_sync (unsigned short port) {
	(void) inb (port);
	(void) inb (port);
	(void) inb (port);
}
#endif /* SUPPORT_VLB_SYNC */

/*
 * This is used for most PIO data transfers *from* the IDE interface
 */
void ide_input_data (ide_drive_t *drive, void *buffer, unsigned int wcount)
{
	unsigned short io_base  = HWIF(drive)->io_base;
	unsigned short data_reg = io_base+IDE_DATA_OFFSET;
	byte io_32bit = drive->io_32bit;

	if (io_32bit) {
#if SUPPORT_VLB_SYNC
		if (io_32bit & 2) {
			cli();
			do_vlb_sync(io_base+IDE_NSECTOR_OFFSET);
			insl(data_reg, buffer, wcount);
			if (drive->unmask)
				sti();
		} else
#endif /* SUPPORT_VLB_SYNC */
			insl(data_reg, buffer, wcount);
	} else
		insw(data_reg, buffer, wcount<<1);
}

/*
 * This is used for most PIO data transfers *to* the IDE interface
 */
void ide_output_data (ide_drive_t *drive, void *buffer, unsigned int wcount)
{
	unsigned short io_base  = HWIF(drive)->io_base;
	unsigned short data_reg = io_base+IDE_DATA_OFFSET;
	byte io_32bit = drive->io_32bit;

	if (io_32bit) {
#if SUPPORT_VLB_SYNC
		if (io_32bit & 2) {
			cli();
			do_vlb_sync(io_base+IDE_NSECTOR_OFFSET);
			outsl(data_reg, buffer, wcount);
			if (drive->unmask)
				sti();
		} else
#endif /* SUPPORT_VLB_SYNC */
			outsl(data_reg, buffer, wcount);
	} else
		outsw(data_reg, buffer, wcount<<1);
}

/*
 * This should get invoked any time we exit the driver to
 * wait for an interrupt response from a drive.  handler() points
 * at the appropriate code to handle the next interrupt, and a
 * timer is started to prevent us from waiting forever in case
 * something goes wrong (see the timer_expiry() handler later on).
 */
void ide_set_handler (ide_drive_t *drive, ide_handler_t *handler, unsigned int timeout)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
#ifdef DEBUG
	if (hwgroup->handler != NULL) {
		printk("%s: ide_set_handler: handler not null; old=%p, new=%p\n",
			drive->name, hwgroup->handler, handler);
	}
#endif
	hwgroup->handler       = handler;
	hwgroup->timer.expires = jiffies + timeout;
	add_timer(&(hwgroup->timer));
}

/*
 * lba_capacity_is_ok() performs a sanity check on the claimed "lba_capacity"
 * value for this drive (from its reported identification information).
 *
 * Returns:	1 if lba_capacity looks sensible
 *		0 otherwise
 */
static int lba_capacity_is_ok (struct hd_driveid *id)
{
	unsigned long lba_sects   = id->lba_capacity;
	unsigned long chs_sects   = id->cyls * id->heads * id->sectors;
	unsigned long _10_percent = chs_sects / 10;

	/* perform a rough sanity check on lba_sects:  within 10% is "okay" */
	if ((lba_sects - chs_sects) < _10_percent)
		return 1;	/* lba_capacity is good */

	/* some drives have the word order reversed */
	lba_sects = (lba_sects << 16) | (lba_sects >> 16);
	if ((lba_sects - chs_sects) < _10_percent) {
		id->lba_capacity = lba_sects;	/* fix it */
		return 1;	/* lba_capacity is (now) good */
	}
	return 0;	/* lba_capacity value is bad */
}

/*
 * current_capacity() returns the capacity (in sectors) of a drive
 * according to its current geometry/LBA settings.
 */
static unsigned long current_capacity (ide_drive_t  *drive)
{
	struct hd_driveid *id = drive->id;
	unsigned long capacity;

	if (!drive->present)
		return 0;
	if (drive->media != ide_disk)
		return 0x7fffffff;	/* cdrom or tape */
	/* Determine capacity, and use LBA if the drive properly supports it */
	if (id != NULL && (id->capability & 2) && lba_capacity_is_ok(id)) {
		drive->select.b.lba = 1;
		capacity = id->lba_capacity;
	} else {
		drive->select.b.lba = 0;
		capacity = drive->cyl * drive->head * drive->sect;
	}
	return (capacity - drive->sect0);
}

/*
 * ide_geninit() is called exactly *once* for each major, from genhd.c,
 * at the beginning of the initial partition check for the drives.
 */
static void ide_geninit (struct gendisk *gd)
{
	unsigned int unit;
	ide_hwif_t *hwif = gd->real_devices;

	for (unit = 0; unit < gd->nr_real; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
#ifdef CONFIG_BLK_DEV_IDECD
		if (drive->present && drive->media == ide_cdrom)
			ide_cdrom_setup(drive);
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
		if (drive->present && drive->media == ide_tape)
			idetape_setup(drive);
#endif /* CONFIG_BLK_DEV_IDETAPE */
		drive->part[0].nr_sects = current_capacity(drive);
		if (!drive->present || drive->media != ide_disk) {
			drive->part[0].start_sect = -1; /* skip partition check */
		}
	}
	/*
	 * The partition check in genhd.c needs this string to identify
	 * our minor devices by name for display purposes.
	 * Note that doing this will prevent us from working correctly
	 * if ever called a second time for this major (never happens).
	 */
	gd->real_devices = hwif->drives[0].name;  /* name of first drive */
}

/*
 * init_gendisk() (as opposed to ide_geninit) is called for each major device,
 * after probing for drives, to allocate partition tables and other data
 * structures needed for the routines in genhd.c.  ide_geninit() gets called
 * somewhat later, during the partition check.
 */
static void init_gendisk (ide_hwif_t *hwif)
{
	struct gendisk *gd;
	unsigned int unit, units, minors;
	int *bs;

	/* figure out maximum drive number on the interface */
	for (units = MAX_DRIVES; units > 0; --units) {
		if (hwif->drives[units-1].present)
			break;
	}
	minors    = units * (1<<PARTN_BITS);
	gd        = kmalloc (sizeof(struct gendisk), GFP_KERNEL);
	gd->sizes = kmalloc (minors * sizeof(int), GFP_KERNEL);
	gd->part  = kmalloc (minors * sizeof(struct hd_struct), GFP_KERNEL);
	bs        = kmalloc (minors*sizeof(int), GFP_KERNEL);

	/* cdroms and msdos f/s are examples of non-1024 blocksizes */
	blksize_size[hwif->major] = bs;
	for (unit = 0; unit < minors; ++unit)
		*bs++ = BLOCK_SIZE;

	for (unit = 0; unit < units; ++unit)
		hwif->drives[unit].part = &gd->part[unit << PARTN_BITS];

	gd->major	= hwif->major;		/* our major device number */
	gd->major_name	= IDE_MAJOR_NAME;	/* treated special in genhd.c */
	gd->minor_shift	= PARTN_BITS;		/* num bits for partitions */
	gd->max_p	= 1<<PARTN_BITS;	/* 1 + max partitions / drive */
	gd->max_nr	= units;		/* max num real drives */
	gd->nr_real	= units;		/* current num real drives */
	gd->init	= ide_geninit;		/* initialization function */
	gd->real_devices= hwif;			/* ptr to internal data */

	gd->next = gendisk_head;		/* link new major into list */
	hwif->gd = gendisk_head = gd;
}

static void do_reset1 (ide_drive_t *, int);		/* needed below */

#ifdef CONFIG_BLK_DEV_IDEATAPI
/*
 * atapi_reset_pollfunc() gets invoked to poll the interface for completion every 50ms
 * during an atapi drive reset operation. If the drive has not yet responded,
 * and we have not yet hit our maximum waiting time, then the timer is restarted
 * for another 50ms.
 */
static void atapi_reset_pollfunc (ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	byte stat;

	OUT_BYTE (drive->select.all, IDE_SELECT_REG);
	udelay (10);

	if (OK_STAT(stat=GET_STAT(), 0, BUSY_STAT)) {
		printk("%s: ATAPI reset complete\n", drive->name);
	} else {
		if (jiffies < hwgroup->poll_timeout) {
			ide_set_handler (drive, &atapi_reset_pollfunc, HZ/20);
			return;	/* continue polling */
		}
		hwgroup->poll_timeout = 0;	/* end of polling */
		printk("%s: ATAPI reset timed-out, status=0x%02x\n", drive->name, stat);
		do_reset1 (drive, 1);	/* do it the old fashioned way */
	}
	hwgroup->poll_timeout = 0;	/* done polling */
}
#endif /* CONFIG_BLK_DEV_IDEATAPI */

/*
 * reset_pollfunc() gets invoked to poll the interface for completion every 50ms
 * during an ide reset operation. If the drives have not yet responded,
 * and we have not yet hit our maximum waiting time, then the timer is restarted
 * for another 50ms.
 */
static void reset_pollfunc (ide_drive_t *drive)
{
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	ide_hwif_t *hwif = HWIF(drive);
	byte tmp;

	if (!OK_STAT(tmp=GET_STAT(), 0, BUSY_STAT)) {
		if (jiffies < hwgroup->poll_timeout) {
			ide_set_handler (drive, &reset_pollfunc, HZ/20);
			return;	/* continue polling */
		}
		printk("%s: reset timed-out, status=0x%02x\n", hwif->name, tmp);
	} else  {
		printk("%s: reset: ", hwif->name);
		if ((tmp = GET_ERR()) == 1)
			printk("success\n");
		else {
			printk("master: ");
			switch (tmp & 0x7f) {
				case 1: printk("passed");
					break;
				case 2: printk("formatter device error");
					break;
				case 3: printk("sector buffer error");
					break;
				case 4: printk("ECC circuitry error");
					break;
				case 5: printk("controlling MPU error");
					break;
				default:printk("error (0x%02x?)", tmp);
			}
			if (tmp & 0x80)
				printk("; slave: failed");
			printk("\n");
		}
	}
	hwgroup->poll_timeout = 0;	/* done polling */
}

/*
 * do_reset1() attempts to recover a confused drive by resetting it.
 * Unfortunately, resetting a disk drive actually resets all devices on
 * the same interface, so it can really be thought of as resetting the
 * interface rather than resetting the drive.
 *
 * ATAPI devices have their own reset mechanism which allows them to be
 * individually reset without clobbering other devices on the same interface.
 *
 * Unfortunately, the IDE interface does not generate an interrupt to let
 * us know when the reset operation has finished, so we must poll for this.
 * Equally poor, though, is the fact that this may a very long time to complete,
 * (up to 30 seconds worstcase).  So, instead of busy-waiting here for it,
 * we set a timer to poll at 50ms intervals.
 */
static void do_reset1 (ide_drive_t *drive, int  do_not_try_atapi)
{
	unsigned int unit;
	unsigned long flags;
	ide_hwif_t *hwif = HWIF(drive);
	ide_hwgroup_t *hwgroup = HWGROUP(drive);

	save_flags(flags);
	cli();		/* Why ? */

#ifdef CONFIG_BLK_DEV_IDEATAPI
	/* For an ATAPI device, first try an ATAPI SRST. */
	if (drive->media != ide_disk) {
		if (!do_not_try_atapi) {
			if (!drive->keep_settings)
				drive->unmask = 0;
			OUT_BYTE (drive->select.all, IDE_SELECT_REG);
			udelay (20);
			OUT_BYTE (WIN_SRST, IDE_COMMAND_REG);
			hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
			ide_set_handler (drive, &atapi_reset_pollfunc, HZ/20);
			restore_flags (flags);
			return;
		}
	}
#endif /* CONFIG_BLK_DEV_IDEATAPI */

	/*
	 * First, reset any device state data we were maintaining
	 * for any of the drives on this interface.
	 */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *rdrive = &hwif->drives[unit];
		rdrive->special.all = 0;
		rdrive->special.b.set_geometry = 1;
		rdrive->special.b.recalibrate  = 1;
		if (OK_TO_RESET_CONTROLLER)
			rdrive->mult_count = 0;
		if (!rdrive->keep_settings) {
			rdrive->using_dma = 0;
			rdrive->mult_req = 0;
			rdrive->unmask = 0;
		}
		if (rdrive->mult_req != rdrive->mult_count)
			rdrive->special.b.set_multmode = 1;
	}

#if OK_TO_RESET_CONTROLLER
	/*
	 * Note that we also set nIEN while resetting the device,
	 * to mask unwanted interrupts from the interface during the reset.
	 * However, due to the design of PC hardware, this will cause an
	 * immediate interrupt due to the edge transition it produces.
	 * This single interrupt gives us a "fast poll" for drives that
	 * recover from reset very quickly, saving us the first 50ms wait time.
	 */
	OUT_BYTE(drive->ctl|6,IDE_CONTROL_REG);	/* set SRST and nIEN */
	udelay(5);			/* more than enough time */
	OUT_BYTE(drive->ctl|2,IDE_CONTROL_REG);	/* clear SRST, leave nIEN */
	hwgroup->poll_timeout = jiffies + WAIT_WORSTCASE;
	ide_set_handler (drive, &reset_pollfunc, HZ/20);
#endif	/* OK_TO_RESET_CONTROLLER */

	restore_flags (flags);
}

/*
 * ide_do_reset() is the entry point to the drive/interface reset code.
 */
void ide_do_reset (ide_drive_t *drive)
{
	do_reset1 (drive, 0);
#ifdef CONFIG_BLK_DEV_IDETAPE
	if (drive->media == ide_tape)
		drive->tape.reset_issued=1;
#endif /* CONFIG_BLK_DEV_IDETAPE */
}

/*
 * Clean up after success/failure of an explicit drive cmd
 */
void ide_end_drive_cmd (ide_drive_t *drive, byte stat, byte err)
{
	unsigned long flags;
	struct request *rq = HWGROUP(drive)->rq;

	if (rq->cmd == IDE_DRIVE_CMD) {
		byte *args = (byte *) rq->buffer;
		rq->errors = !OK_STAT(stat,READY_STAT,BAD_STAT);
		if (args) {
			args[0] = stat;
			args[1] = err;
			args[2] = IN_BYTE(IDE_NSECTOR_REG);
		}
	}
	save_flags(flags);
	cli();
	blk_dev[MAJOR(rq->rq_dev)].current_request = rq->next;
	HWGROUP(drive)->rq = NULL;
	rq->rq_status = RQ_INACTIVE;
	if (rq->sem != NULL)
		up(rq->sem);
	restore_flags(flags);
}

/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 */
byte ide_dump_status (ide_drive_t *drive, const char *msg, byte stat)
{
	unsigned long flags;
	byte err = 0;

	save_flags (flags);
	sti();
	printk("%s: %s: status=0x%02x", drive->name, msg, stat);
#if FANCY_STATUS_DUMPS
	if (drive->media == ide_disk) {
		printk(" { ");
		if (stat & BUSY_STAT)
			printk("Busy ");
		else {
			if (stat & READY_STAT)	printk("DriveReady ");
			if (stat & WRERR_STAT)	printk("DeviceFault ");
			if (stat & SEEK_STAT)	printk("SeekComplete ");
			if (stat & DRQ_STAT)	printk("DataRequest ");
			if (stat & ECC_STAT)	printk("CorrectedError ");
			if (stat & INDEX_STAT)	printk("Index ");
			if (stat & ERR_STAT)	printk("Error ");
		}
		printk("}");
	}
#endif	/* FANCY_STATUS_DUMPS */
	printk("\n");
	if ((stat & (BUSY_STAT|ERR_STAT)) == ERR_STAT) {
		err = GET_ERR();
		printk("%s: %s: error=0x%02x", drive->name, msg, err);
#if FANCY_STATUS_DUMPS
		if (drive->media == ide_disk) {
			printk(" { ");
			if (err & BBD_ERR)	printk("BadSector ");
			if (err & ECC_ERR)	printk("UncorrectableError ");
			if (err & ID_ERR)	printk("SectorIdNotFound ");
			if (err & ABRT_ERR)	printk("DriveStatusError ");
			if (err & TRK0_ERR)	printk("TrackZeroNotFound ");
			if (err & MARK_ERR)	printk("AddrMarkNotFound ");
			printk("}");
			if (err & (BBD_ERR|ECC_ERR|ID_ERR|MARK_ERR)) {
				byte cur = IN_BYTE(IDE_SELECT_REG);
				if (cur & 0x40) {	/* using LBA? */
					printk(", LBAsect=%ld", (unsigned long)
					 ((cur&0xf)<<24)
					 |(IN_BYTE(IDE_HCYL_REG)<<16)
					 |(IN_BYTE(IDE_LCYL_REG)<<8)
					 | IN_BYTE(IDE_SECTOR_REG));
				} else {
					printk(", CHS=%d/%d/%d",
					 (IN_BYTE(IDE_HCYL_REG)<<8) +
					  IN_BYTE(IDE_LCYL_REG),
					  cur & 0xf,
					  IN_BYTE(IDE_SECTOR_REG));
				}
				if (HWGROUP(drive)->rq)
					printk(", sector=%ld", HWGROUP(drive)->rq->sector);
			}
		}
#endif	/* FANCY_STATUS_DUMPS */
		printk("\n");
	}
	restore_flags (flags);
	return err;
}

/*
 * try_to_flush_leftover_data() is invoked in response to a drive
 * unexpectedly having its DRQ_STAT bit set.  As an alternative to
 * resetting the drive, this routine tries to clear the condition
 * by read a sector's worth of data from the drive.  Of course,
 * this may not help if the drive is *waiting* for data from *us*.
 */
static void try_to_flush_leftover_data (ide_drive_t *drive)
{
	int i = (drive->mult_count ? drive->mult_count : 1) * SECTOR_WORDS;

	while (i > 0) {
		unsigned long buffer[16];
		unsigned int wcount = (i > 16) ? 16 : i;
		i -= wcount;
		ide_input_data (drive, buffer, wcount);
	}
}

/*
 * ide_error() takes action based on the error returned by the controller.
 */
void ide_error (ide_drive_t *drive, const char *msg, byte stat)
{
	struct request *rq;
	byte err;

	err = ide_dump_status(drive, msg, stat);
	if ((rq = HWGROUP(drive)->rq) == NULL || drive == NULL)
		return;
	/* retry only "normal" I/O: */
	if (rq->cmd == IDE_DRIVE_CMD || (rq->cmd != READ && rq->cmd != WRITE && drive->media == ide_disk))
	{
		rq->errors = 1;
		ide_end_drive_cmd(drive, stat, err);
		return;
	}
	if (stat & BUSY_STAT) {		/* other bits are useless when BUSY */
		rq->errors |= ERROR_RESET;
	} else {
		if (drive->media == ide_disk && (stat & ERR_STAT)) {
			/* err has different meaning on cdrom and tape */
			if (err & (BBD_ERR | ECC_ERR))	/* retries won't help these */
				rq->errors = ERROR_MAX;
			else if (err & TRK0_ERR)	/* help it find track zero */
				rq->errors |= ERROR_RECAL;
		}
		if ((stat & DRQ_STAT) && rq->cmd != WRITE)
			try_to_flush_leftover_data(drive);
	}
	if (GET_STAT() & (BUSY_STAT|DRQ_STAT))
		rq->errors |= ERROR_RESET;	/* Mmmm.. timing problem */

	if (rq->errors >= ERROR_MAX) {
#ifdef CONFIG_BLK_DEV_IDETAPE
		if (drive->media == ide_tape) {
			rq->errors = 0;
			idetape_end_request(0, HWGROUP(drive));
		}
		else
#endif /* CONFIG_BLK_DEV_IDETAPE */
 		ide_end_request(0, HWGROUP(drive));
	}
	else {
		if ((rq->errors & ERROR_RESET) == ERROR_RESET) {
			++rq->errors;
			ide_do_reset(drive);
			return;
		} else if ((rq->errors & ERROR_RECAL) == ERROR_RECAL)
			drive->special.b.recalibrate = 1;
		++rq->errors;
	}
}

/*
 * read_intr() is the handler for disk read/multread interrupts
 */
static void read_intr (ide_drive_t *drive)
{
	byte stat;
	int i;
	unsigned int msect, nsect;
	struct request *rq;

	if (!OK_STAT(stat=GET_STAT(),DATA_READY,BAD_R_STAT)) {
		ide_error(drive, "read_intr", stat);
		return;
	}
	msect = drive->mult_count;
read_next:
	rq = HWGROUP(drive)->rq;
	if (msect) {
		if ((nsect = rq->current_nr_sectors) > msect)
			nsect = msect;
		msect -= nsect;
	} else
		nsect = 1;
	ide_input_data(drive, rq->buffer, nsect * SECTOR_WORDS);
#ifdef DEBUG
	printk("%s:  read: sectors(%ld-%ld), buffer=0x%08lx, remaining=%ld\n",
		drive->name, rq->sector, rq->sector+nsect-1,
		(unsigned long) rq->buffer+(nsect<<9), rq->nr_sectors-nsect);
#endif
	rq->sector += nsect;
	rq->buffer += nsect<<9;
	rq->errors = 0;
	i = (rq->nr_sectors -= nsect);
	if ((rq->current_nr_sectors -= nsect) <= 0)
		ide_end_request(1, HWGROUP(drive));
	if (i > 0) {
		if (msect)
			goto read_next;
		ide_set_handler (drive, &read_intr, WAIT_CMD);
	}
}

/*
 * write_intr() is the handler for disk write interrupts
 */
static void write_intr (ide_drive_t *drive)
{
	byte stat;
	int i;
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	struct request *rq = hwgroup->rq;

	if (OK_STAT(stat=GET_STAT(),DRIVE_READY,drive->bad_wstat)) {
#ifdef DEBUG
		printk("%s: write: sector %ld, buffer=0x%08lx, remaining=%ld\n",
			drive->name, rq->sector, (unsigned long) rq->buffer,
			rq->nr_sectors-1);
#endif
		if ((rq->nr_sectors == 1) ^ ((stat & DRQ_STAT) != 0)) {
			rq->sector++;
			rq->buffer += 512;
			rq->errors = 0;
			i = --rq->nr_sectors;
			--rq->current_nr_sectors;
			if (rq->current_nr_sectors <= 0)
				ide_end_request(1, hwgroup);
			if (i > 0) {
				ide_output_data (drive, rq->buffer, SECTOR_WORDS);
				ide_set_handler (drive, &write_intr, WAIT_CMD);
			}
			return;
		}
	}
	ide_error(drive, "write_intr", stat);
}

/*
 * multwrite() transfers a block of one or more sectors of data to a drive
 * as part of a disk multwrite operation.
 */
static void multwrite (ide_drive_t *drive)
{
	struct request *rq = &HWGROUP(drive)->wrq;
	unsigned int mcount = drive->mult_count;

	do {
		unsigned int nsect = rq->current_nr_sectors;
		if (nsect > mcount)
			nsect = mcount;
		mcount -= nsect;

		ide_output_data(drive, rq->buffer, nsect<<7);
#ifdef DEBUG
		printk("%s: multwrite: sector %ld, buffer=0x%08lx, count=%d, remaining=%ld\n",
			drive->name, rq->sector, (unsigned long) rq->buffer,
			nsect, rq->nr_sectors - nsect);
#endif
		if ((rq->nr_sectors -= nsect) <= 0)
			break;
		if ((rq->current_nr_sectors -= nsect) == 0) {
			if ((rq->bh = rq->bh->b_reqnext) != NULL) {
				rq->current_nr_sectors = rq->bh->b_size>>9;
				rq->buffer             = rq->bh->b_data;
			} else {
				panic("%s: buffer list corrupted\n", drive->name);
				break;
			}
		} else {
			rq->buffer += nsect << 9;
		}
	} while (mcount);
}

/*
 * multwrite_intr() is the handler for disk multwrite interrupts
 */
static void multwrite_intr (ide_drive_t *drive)
{
	byte stat;
	int i;
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	struct request *rq = &hwgroup->wrq;

	if (OK_STAT(stat=GET_STAT(),DRIVE_READY,drive->bad_wstat)) {
		if (stat & DRQ_STAT) {
			if (rq->nr_sectors) {
				multwrite(drive);
				ide_set_handler (drive, &multwrite_intr, WAIT_CMD);
				return;
			}
		} else {
			if (!rq->nr_sectors) {	/* all done? */
				rq = hwgroup->rq;
				for (i = rq->nr_sectors; i > 0;){
					i -= rq->current_nr_sectors;
					ide_end_request(1, hwgroup);
				}
				return;
			}
		}
	}
	ide_error(drive, "multwrite_intr", stat);
}

/*
 * Issue a simple drive command
 * The drive must be selected beforehand.
 */
static void ide_cmd(ide_drive_t *drive, byte cmd, byte nsect, ide_handler_t *handler)
{
	ide_set_handler (drive, handler, WAIT_CMD);
	OUT_BYTE(drive->ctl,IDE_CONTROL_REG);
	OUT_BYTE(nsect,IDE_NSECTOR_REG);
	OUT_BYTE(cmd,IDE_COMMAND_REG);
}

/*
 * set_multmode_intr() is invoked on completion of a WIN_SETMULT cmd.
 */
static void set_multmode_intr (ide_drive_t *drive)
{
	byte stat = GET_STAT();

	sti();
	if (OK_STAT(stat,READY_STAT,BAD_STAT)) {
		drive->mult_count = drive->mult_req;
	} else {
		drive->mult_req = drive->mult_count = 0;
		drive->special.b.recalibrate = 1;
		(void) ide_dump_status(drive, "set_multmode", stat);
	}
}

/*
 * set_geometry_intr() is invoked on completion of a WIN_SPECIFY cmd.
 */
static void set_geometry_intr (ide_drive_t *drive)
{
	byte stat = GET_STAT();

	sti();
	if (!OK_STAT(stat,READY_STAT,BAD_STAT))
		ide_error(drive, "set_geometry_intr", stat);
}

/*
 * recal_intr() is invoked on completion of a WIN_RESTORE (recalibrate) cmd.
 */
static void recal_intr (ide_drive_t *drive)
{
	byte stat = GET_STAT();

	sti();
	if (!OK_STAT(stat,READY_STAT,BAD_STAT))
		ide_error(drive, "recal_intr", stat);
}

/*
 * drive_cmd_intr() is invoked on completion of a special DRIVE_CMD.
 */
static void drive_cmd_intr (ide_drive_t *drive)
{
	byte stat = GET_STAT();

	sti();
	if (OK_STAT(stat,READY_STAT,BAD_STAT))
		ide_end_drive_cmd (drive, stat, GET_ERR());
	else
		ide_error(drive, "drive_cmd", stat); /* calls ide_end_drive_cmd */
}

/*
 * do_special() is used to issue WIN_SPECIFY, WIN_RESTORE, and WIN_SETMULT
 * commands to a drive.  It used to do much more, but has been scaled back.
 */
static inline void do_special (ide_drive_t *drive)
{
	special_t *s = &drive->special;
next:
#ifdef DEBUG
	printk("%s: do_special: 0x%02x\n", drive->name, s->all);
#endif
	if (s->b.set_geometry) {
		s->b.set_geometry = 0;
		if (drive->media == ide_disk) {
			OUT_BYTE(drive->sect,IDE_SECTOR_REG);
			OUT_BYTE(drive->cyl,IDE_LCYL_REG);
			OUT_BYTE(drive->cyl>>8,IDE_HCYL_REG);
			OUT_BYTE(((drive->head-1)|drive->select.all)&0xBF,IDE_SELECT_REG);
			ide_cmd(drive, WIN_SPECIFY, drive->sect, &set_geometry_intr);
		}
	} else if (s->b.recalibrate) {
		s->b.recalibrate = 0;
		if (drive->media == ide_disk) {
			ide_cmd(drive, WIN_RESTORE, drive->sect, &recal_intr);
		}
	} else if (s->b.set_pio) {
		ide_tuneproc_t *tuneproc = HWIF(drive)->tuneproc;
		s->b.set_pio = 0;
		if (tuneproc != NULL)
			tuneproc(drive, drive->pio_req);
		goto next;
	} else if (s->b.set_multmode) {
		s->b.set_multmode = 0;
		if (drive->media == ide_disk) {
			if (drive->id && drive->mult_req > drive->id->max_multsect)
				drive->mult_req = drive->id->max_multsect;
			ide_cmd(drive, WIN_SETMULT, drive->mult_req, &set_multmode_intr);
		} else
			drive->mult_req = 0;
	} else if (s->all) {
		s->all = 0;
		printk("%s: bad special flag: 0x%02x\n", drive->name, s->all);
	}
}

/*
 * This routine busy-waits for the drive status to be not "busy".
 * It then checks the status for all of the "good" bits and none
 * of the "bad" bits, and if all is okay it returns 0.  All other
 * cases return 1 after invoking ide_error() -- caller should just return.
 *
 * This routine should get fixed to not hog the cpu during extra long waits..
 * That could be done by busy-waiting for the first jiffy or two, and then
 * setting a timer to wake up at half second intervals thereafter,
 * until timeout is achieved, before timing out.
 */
int ide_wait_stat (ide_drive_t *drive, byte good, byte bad, unsigned long timeout)
{
	byte stat;
	unsigned long flags;

test:
	udelay(1);	/* spec allows drive 400ns to change "BUSY" */
	if (OK_STAT((stat = GET_STAT()), good, bad))
		return 0;	/* fast exit for most frequent case */
	if (!(stat & BUSY_STAT)) {
		ide_error(drive, "status error", stat);
		return 1;
	}

	save_flags(flags);
	sti();
	timeout += jiffies;
	do {
		if (!((stat = GET_STAT()) & BUSY_STAT)) {
			restore_flags(flags);
			goto test;
		}
	} while (jiffies <= timeout);

	restore_flags(flags);
	ide_error(drive, "status timeout", GET_STAT());
	return 1;
}

/*
 * do_rw_disk() issues WIN_{MULT}READ and WIN_{MULT}WRITE commands to a disk,
 * using LBA if supported, or CHS otherwise, to address sectors.  It also takes
 * care of issuing special DRIVE_CMDs.
 */
static inline void do_rw_disk (ide_drive_t *drive, struct request *rq, unsigned long block)
{
	unsigned short io_base = HWIF(drive)->io_base;

	OUT_BYTE(drive->ctl,IDE_CONTROL_REG);
	OUT_BYTE(rq->nr_sectors,io_base+IDE_NSECTOR_OFFSET);
	if (drive->select.b.lba) {
#ifdef DEBUG
		printk("%s: %sing: LBAsect=%ld, sectors=%ld, buffer=0x%08lx\n",
			drive->name, (rq->cmd==READ)?"read":"writ",
			block, rq->nr_sectors, (unsigned long) rq->buffer);
#endif
		OUT_BYTE(block,io_base+IDE_SECTOR_OFFSET);
		OUT_BYTE(block>>=8,io_base+IDE_LCYL_OFFSET);
		OUT_BYTE(block>>=8,io_base+IDE_HCYL_OFFSET);
		OUT_BYTE(((block>>8)&0x0f)|drive->select.all,io_base+IDE_SELECT_OFFSET);
	} else {
		unsigned int sect,head,cyl,track;
		track = block / drive->sect;
		sect  = block % drive->sect + 1;
		OUT_BYTE(sect,io_base+IDE_SECTOR_OFFSET);
		head  = track % drive->head;
		cyl   = track / drive->head;
		OUT_BYTE(cyl,io_base+IDE_LCYL_OFFSET);
		OUT_BYTE(cyl>>8,io_base+IDE_HCYL_OFFSET);
		OUT_BYTE(head|drive->select.all,io_base+IDE_SELECT_OFFSET);
#ifdef DEBUG
		printk("%s: %sing: CHS=%d/%d/%d, sectors=%ld, buffer=0x%08lx\n",
			drive->name, (rq->cmd==READ)?"read":"writ", cyl,
			head, sect, rq->nr_sectors, (unsigned long) rq->buffer);
#endif
	}
	if (rq->cmd == READ) {
#ifdef CONFIG_BLK_DEV_TRITON
		if (drive->using_dma && !(HWIF(drive)->dmaproc(ide_dma_read, drive)))
			return;
#endif /* CONFIG_BLK_DEV_TRITON */
		ide_set_handler(drive, &read_intr, WAIT_CMD);
		OUT_BYTE(drive->mult_count ? WIN_MULTREAD : WIN_READ, io_base+IDE_COMMAND_OFFSET);
		return;
	}
	if (rq->cmd == WRITE) {
#ifdef CONFIG_BLK_DEV_TRITON
		if (drive->using_dma && !(HWIF(drive)->dmaproc(ide_dma_write, drive)))
			return;
#endif /* CONFIG_BLK_DEV_TRITON */
		OUT_BYTE(drive->mult_count ? WIN_MULTWRITE : WIN_WRITE, io_base+IDE_COMMAND_OFFSET);
		if (ide_wait_stat(drive, DATA_READY, drive->bad_wstat, WAIT_DRQ)) {
			printk("%s: no DRQ after issuing %s\n", drive->name,
				drive->mult_count ? "MULTWRITE" : "WRITE");
			return;
		}
		if (!drive->unmask)
			cli();
		if (drive->mult_count) {
			HWGROUP(drive)->wrq = *rq; /* scratchpad */
			ide_set_handler (drive, &multwrite_intr, WAIT_CMD);
			multwrite(drive);
		} else {
			ide_set_handler (drive, &write_intr, WAIT_CMD);
			ide_output_data(drive, rq->buffer, SECTOR_WORDS);
		}
		return;
	}
	if (rq->cmd == IDE_DRIVE_CMD) {
		byte *args = rq->buffer;
		if (args) {
#ifdef DEBUG
			printk("%s: DRIVE_CMD cmd=0x%02x sc=0x%02x fr=0x%02x\n",
			 drive->name, args[0], args[1], args[2]);
#endif
			OUT_BYTE(args[2],io_base+IDE_FEATURE_OFFSET);
			ide_cmd(drive, args[0], args[1], &drive_cmd_intr);
			return;
		} else {
			/*
			 * NULL is actually a valid way of waiting for
			 * all current requests to be flushed from the queue.
			 */
#ifdef DEBUG
			printk("%s: DRIVE_CMD (null)\n", drive->name);
#endif
			ide_end_drive_cmd(drive, GET_STAT(), GET_ERR());
			return;
		}
	}
	printk("%s: bad command: %d\n", drive->name, rq->cmd);
	ide_end_request(0, HWGROUP(drive));
}

/*
 * do_request() initiates handling of a new I/O request
 */
static inline void do_request (ide_hwif_t *hwif, struct request *rq)
{
	unsigned int minor, unit;
	unsigned long block, blockend;
	ide_drive_t *drive;

	sti();
#ifdef DEBUG
	printk("%s: do_request: current=0x%08lx\n", hwif->name, (unsigned long) rq);
#endif
	minor = MINOR(rq->rq_dev);
	unit = minor >> PARTN_BITS;
	if (MAJOR(rq->rq_dev) != hwif->major || unit >= MAX_DRIVES) {
		printk("%s: bad device number: %s\n",
		       hwif->name, kdevname(rq->rq_dev));
		goto kill_rq;
	}
	drive = &hwif->drives[unit];
#ifdef DEBUG
	if (rq->bh && !buffer_locked(rq->bh)) {
		printk("%s: block not locked\n", drive->name);
		goto kill_rq;
	}
#endif
	block    = rq->sector;
	blockend = block + rq->nr_sectors;
	if ((blockend < block) || (blockend > drive->part[minor&PARTN_MASK].nr_sects)) {
		printk("%s%c: bad access: block=%ld, count=%ld\n", drive->name,
		 (minor&PARTN_MASK)?'0'+(minor&PARTN_MASK):' ', block, rq->nr_sectors);
		goto kill_rq;
	}
	block += drive->part[minor&PARTN_MASK].start_sect + drive->sect0;
#if FAKE_FDISK_FOR_EZDRIVE
	if (block == 0 && drive->remap_0_to_1)
		block = 1;  /* redirect MBR access to EZ-Drive partn table */
#endif /* FAKE_FDISK_FOR_EZDRIVE */
	((ide_hwgroup_t *)hwif->hwgroup)->drive = drive;
#ifdef CONFIG_BLK_DEV_HT6560B
	if (hwif->selectproc)
		hwif->selectproc (drive);
#endif /* CONFIG_BLK_DEV_HT6560B */
#if (DISK_RECOVERY_TIME > 0)
	while ((read_timer() - hwif->last_time) < DISK_RECOVERY_TIME);
#endif

#ifdef CONFIG_BLK_DEV_IDETAPE
	POLL_HWIF_TAPE_DRIVE;	/* macro from ide-tape.h */
#endif /* CONFIG_BLK_DEV_IDETAPE */

	OUT_BYTE(drive->select.all,IDE_SELECT_REG);
	if (ide_wait_stat(drive, drive->ready_stat, BUSY_STAT|DRQ_STAT, WAIT_READY)) {
		printk("%s: drive not ready for command\n", drive->name);
		return;
	}
	
	if (!drive->special.all) {
#ifdef CONFIG_BLK_DEV_IDEATAPI
		switch (drive->media) {
			case ide_disk:
				do_rw_disk (drive, rq, block);
				return;
#ifdef CONFIG_BLK_DEV_IDECD
			case ide_cdrom:
				ide_do_rw_cdrom (drive, block);
				return;
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
			case ide_tape:
				if (rq->cmd == IDE_DRIVE_CMD) {
					byte *args = (byte *) rq->buffer;
					OUT_BYTE(args[2],IDE_FEATURE_REG);
					ide_cmd(drive, args[0], args[1], &drive_cmd_intr);
					return;
				}
				idetape_do_request (drive, rq, block);
				return;
#endif /* CONFIG_BLK_DEV_IDETAPE */

			default:
				printk("%s: media type %d not supported\n",
					drive->name, drive->media);
				goto kill_rq;
		}
#else
		do_rw_disk (drive, rq, block); /* simpler and faster */
		return;
#endif /* CONFIG_BLK_DEV_IDEATAPI */;
	}
	do_special(drive);
	return;
kill_rq:
	ide_end_request(0, hwif->hwgroup);
}

/*
 * The driver enables interrupts as much as possible.  In order to do this,
 * (a) the device-interrupt is always masked before entry, and
 * (b) the timeout-interrupt is always disabled before entry.
 *
 * If we enter here from, say irq14, and then start a new request for irq15,
 * (possible with "serialize" option) then we cannot ensure that we exit
 * before the irq15 hits us. So, we must be careful not to let this bother us.
 *
 * Interrupts are still masked (by default) whenever we are exchanging
 * data/cmds with a drive, because some drives seem to have very poor
 * tolerance for latency during I/O.  For devices which don't suffer from
 * this problem (most don't), the unmask flag can be set using the "hdparm"
 * utility, to permit other interrupts during data/cmd transfers.
 */
void ide_do_request (ide_hwgroup_t *hwgroup)
{
	cli();	/* paranoia */
	if (hwgroup->handler != NULL) {
		printk("%s: EEeekk!! handler not NULL in ide_do_request()\n", hwgroup->hwif->name);
		return;
	}
	do {
		ide_hwif_t *hwif = hwgroup->hwif;
		struct request *rq;
		if ((rq = hwgroup->rq) == NULL) {
			do {
				rq = blk_dev[hwif->major].current_request;
				if (rq != NULL && rq->rq_status != RQ_INACTIVE)
					goto got_rq;
			} while ((hwif = hwif->next) != hwgroup->hwif);
			return;		/* no work left for this hwgroup */
		}
	got_rq:	
		do_request(hwgroup->hwif = hwif, hwgroup->rq = rq);
		cli();
	} while (hwgroup->handler == NULL);
}

/*
 * do_hwgroup_request() invokes ide_do_request() after first masking
 * all possible interrupts for the current hwgroup.  This prevents race
 * conditions in the event that an unexpected interrupt occurs while
 * we are in the driver.
 *
 * Note that when an interrupt is used to reenter the driver, the first level
 * handler will already have masked the irq that triggered, but any other ones
 * for the hwgroup will still be unmasked.  The driver tries to be careful
 * about such things.
 */
static void do_hwgroup_request (ide_hwgroup_t *hwgroup)
{
	if (hwgroup->handler == NULL) {
		ide_hwif_t *hgif = hwgroup->hwif;
		ide_hwif_t *hwif = hgif;
		do {
			disable_irq(hwif->irq);
		} while ((hwif = hwif->next) != hgif);
		ide_do_request (hwgroup);
		do {
			enable_irq(hwif->irq);
		} while ((hwif = hwif->next) != hgif);
	}
}

static void do_ide0_request (void)	/* invoked with cli() */
{
	do_hwgroup_request (ide_hwifs[0].hwgroup);
}

static void do_ide1_request (void)	/* invoked with cli() */
{
	do_hwgroup_request (ide_hwifs[1].hwgroup);
}

static void do_ide2_request (void)	/* invoked with cli() */
{
	do_hwgroup_request (ide_hwifs[2].hwgroup);
}

static void do_ide3_request (void)	/* invoked with cli() */
{
	do_hwgroup_request (ide_hwifs[3].hwgroup);
}

static void timer_expiry (unsigned long data)
{
	ide_hwgroup_t *hwgroup = (ide_hwgroup_t *) data;
	ide_drive_t   *drive   = hwgroup->drive;
	unsigned long flags;

	save_flags(flags);
	cli();

	if (hwgroup->poll_timeout != 0) { /* polling in progress? */
		ide_handler_t *handler = hwgroup->handler;
		hwgroup->handler = NULL;
		handler(drive);
	} else if (hwgroup->handler == NULL) {	 /* not waiting for anything? */
		sti(); /* drive must have responded just as the timer expired */
		printk("%s: marginal timeout\n", drive->name);
	} else {
		hwgroup->handler = NULL;	/* abort the operation */
		if (hwgroup->hwif->dmaproc)
			(void) hwgroup->hwif->dmaproc (ide_dma_abort, drive);
		ide_error(drive, "irq timeout", GET_STAT());
	}
	if (hwgroup->handler == NULL)
		do_hwgroup_request (hwgroup);
	restore_flags(flags);
}

/*
 * There's nothing really useful we can do with an unexpected interrupt,
 * other than reading the status register (to clear it), and logging it.
 * There should be no way that an irq can happen before we're ready for it,
 * so we needn't worry much about losing an "important" interrupt here.
 *
 * On laptops (and "green" PCs), an unexpected interrupt occurs whenever the
 * drive enters "idle", "standby", or "sleep" mode, so if the status looks
 * "good", we just ignore the interrupt completely.
 *
 * This routine assumes cli() is in effect when called.
 *
 * If an unexpected interrupt happens on irq15 while we are handling irq14
 * and if the two interfaces are "serialized" (CMD640B), then it looks like
 * we could screw up by interfering with a new request being set up for irq15.
 *
 * In reality, this is a non-issue.  The new command is not sent unless the
 * drive is ready to accept one, in which case we know the drive is not
 * trying to interrupt us.  And ide_set_handler() is always invoked before
 * completing the issuance of any new drive command, so we will not be
 * accidently invoked as a result of any valid command completion interrupt.
 *
 */
static void unexpected_intr (int irq, ide_hwgroup_t *hwgroup)
{
	byte stat;
	unsigned int unit;
	ide_hwif_t *hwif = hwgroup->hwif;

	/*
	 * handle the unexpected interrupt
	 */
	do {
		if (hwif->irq == irq) {
			for (unit = 0; unit < MAX_DRIVES; ++unit) {
				ide_drive_t *drive = &hwif->drives[unit];
				if (!drive->present)
					continue;
#ifdef CONFIG_BLK_DEV_HT6560B
				if (hwif->selectproc)
					hwif->selectproc (drive);
#endif /* CONFIG_BLK_DEV_HT6560B */
				if (!OK_STAT(stat=GET_STAT(), drive->ready_stat, BAD_STAT))
					(void) ide_dump_status(drive, "unexpected_intr", stat);
				if ((stat & DRQ_STAT))
					try_to_flush_leftover_data(drive);
			}
		}
	} while ((hwif = hwif->next) != hwgroup->hwif);
#ifdef CONFIG_BLK_DEV_HT6560B
	if (hwif->selectproc)
		hwif->selectproc (hwgroup->drive);
#endif /* CONFIG_BLK_DEV_HT6560B */
}

/*
 * entry point for all interrupts, caller does cli() for us
 */
static void ide_intr (int irq, struct pt_regs *regs)
{
	ide_hwgroup_t  *hwgroup = irq_to_hwgroup[irq];
	ide_handler_t  *handler;

	if (irq == hwgroup->hwif->irq && (handler = hwgroup->handler) != NULL) {
		ide_drive_t *drive = hwgroup->drive;
		hwgroup->handler = NULL;
		del_timer(&(hwgroup->timer));
		if (drive->unmask)
			sti();
		handler(drive);
		cli();	/* this is necessary, as next rq may be different irq */
		if (hwgroup->handler == NULL) {
			SET_RECOVERY_TIMER(HWIF(drive));
			ide_do_request(hwgroup);
		}
	} else {
		unexpected_intr(irq, hwgroup);
	}
	cli();
}

/*
 * get_info_ptr() returns the (ide_drive_t *) for a given device number.
 * It returns NULL if the given device number does not match any present drives.
 */
static ide_drive_t *get_info_ptr (kdev_t i_rdev)
{
	int		major = MAJOR(i_rdev);
	unsigned int	h;

	for (h = 0; h < MAX_HWIFS; ++h) {
		ide_hwif_t  *hwif = &ide_hwifs[h];
		if (hwif->present && major == hwif->major) {
			unsigned unit = DEVICE_NR(i_rdev);
			if (unit < MAX_DRIVES) {
				ide_drive_t *drive = &hwif->drives[unit];
				if (drive->present)
					return drive;
			} else if (major == IDE0_MAJOR && unit < 4) {
				printk("ide: probable bad entry for /dev/hd%c\n", 'a'+unit);
				printk("ide: to fix it, run:  /usr/src/linux/drivers/block/MAKEDEV.ide\n");
			}
			break;
		}
	}
	return NULL;
}

/*
 * This function is intended to be used prior to invoking ide_do_drive_cmd().
 */
void ide_init_drive_cmd (struct request *rq)
{
	rq->buffer = NULL;
	rq->cmd = IDE_DRIVE_CMD;
	rq->sector = 0;
	rq->nr_sectors = 0;
	rq->current_nr_sectors = 0;
	rq->sem = NULL;
	rq->bh = NULL;
	rq->bhtail = NULL;
	rq->next = NULL;

#if 0	/* these are done each time through ide_do_drive_cmd() */
	rq->errors = 0;
	rq->rq_status = RQ_ACTIVE;
	rq->rq_dev = ????;
#endif
}

/*
 * This function issues a special IDE device request
 * onto the request queue.
 *
 * If action is ide_wait, then then rq is queued at the end of
 * the request queue, and the function sleeps until it has been
 * processed.  This is for use when invoked from an ioctl handler.
 *
 * If action is ide_preempt, then the rq is queued at the head of
 * the request queue, displacing the currently-being-processed
 * request and this function returns immediately without waiting
 * for the new rq to be completed.  This is VERY DANGEROUS, and is
 * intended for careful use by the ATAPI tape/cdrom driver code.
 *
 * If action is ide_next, then the rq is queued immediately after
 * the currently-being-processed-request (if any), and the function
 * returns without waiting for the new rq to be completed.  As above,
 * This is VERY DANGEROUS, and is intended for careful use by the
 * ATAPI tape/cdrom driver code.
 *
 * If action is ide_end, then the rq is queued at the end of the
 * request queue, and the function returns immediately without waiting
 * for the new rq to be completed. This is again intended for careful
 * use by the ATAPI tape/cdrom driver code. (Currently used by ide-tape.c,
 * when operating in the pipelined operation mode).
 */
int ide_do_drive_cmd (ide_drive_t *drive, struct request *rq, ide_action_t action)
{
	unsigned long flags;
	unsigned int major = HWIF(drive)->major;
	struct request *cur_rq;
	struct blk_dev_struct *bdev = &blk_dev[major];
	struct semaphore sem = MUTEX_LOCKED;

	rq->errors = 0;
	rq->rq_status = RQ_ACTIVE;
	rq->rq_dev = MKDEV(major,(drive->select.b.unit)<<PARTN_BITS);
	if (action == ide_wait)
		rq->sem = &sem;

	save_flags(flags);
	cli();
	cur_rq = bdev->current_request;

	if (cur_rq == NULL || action == ide_preempt) {
		rq->next = cur_rq;
		bdev->current_request = rq;
		if (action == ide_preempt) {
			HWGROUP(drive)->rq = NULL;
		} else
		if (HWGROUP(drive)->rq == NULL) {  /* is this necessary (?) */
			bdev->request_fn();
			cli();
		}
	} else {
		if (action == ide_wait || action == ide_end) {
			while (cur_rq->next != NULL)	/* find end of list */
				cur_rq = cur_rq->next;
		}
		rq->next = cur_rq->next;
		cur_rq->next = rq;
	}
	if (action == ide_wait  && rq->rq_status != RQ_INACTIVE)
		down(&sem);	/* wait for it to be serviced */
	restore_flags(flags);
	return rq->errors ? -EIO : 0;	/* return -EIO if errors */
}

static int ide_open(struct inode * inode, struct file * filp)
{
	ide_drive_t *drive;
	unsigned long flags;

	if ((drive = get_info_ptr(inode->i_rdev)) == NULL)
		return -ENODEV;
	save_flags(flags);
	cli();
	while (drive->busy)
		sleep_on(&drive->wqueue);
	drive->usage++;
	restore_flags(flags);
#ifdef CONFIG_BLK_DEV_IDECD
	if (drive->media == ide_cdrom)
		return ide_cdrom_open (inode, filp, drive);
#endif	/* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
	if (drive->media == ide_tape)
		return idetape_blkdev_open (inode, filp, drive);
#endif	/* CONFIG_BLK_DEV_IDETAPE */
	if (drive->removeable) {
		byte door_lock[] = {WIN_DOORLOCK,0,0,0};
		struct request rq;
		check_disk_change(inode->i_rdev);
		ide_init_drive_cmd (&rq);
		rq.buffer = door_lock;
		/*
		 * Ignore the return code from door_lock,
		 * since the open() has already succeeded,
		 * and the door_lock is irrelevant at this point.
		 */
		(void) ide_do_drive_cmd(drive, &rq, ide_wait);
	}
	return 0;
}

/*
 * Releasing a block device means we sync() it, so that it can safely
 * be forgotten about...
 */
static void ide_release(struct inode * inode, struct file * file)
{
	ide_drive_t *drive;

	if ((drive = get_info_ptr(inode->i_rdev)) != NULL) {
		sync_dev(inode->i_rdev);
		drive->usage--;
#ifdef CONFIG_BLK_DEV_IDECD
		if (drive->media == ide_cdrom) {
			ide_cdrom_release (inode, file, drive);
			return;
		}
#endif	/* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
		if (drive->media == ide_tape) {
			idetape_blkdev_release (inode, file, drive);
			return;
		}
#endif	/* CONFIG_BLK_DEV_IDETAPE */
		if (drive->removeable) {
			byte door_unlock[] = {WIN_DOORUNLOCK,0,0,0};
			struct request rq;
			invalidate_buffers(inode->i_rdev);
			ide_init_drive_cmd (&rq);
			rq.buffer = door_unlock;
			(void) ide_do_drive_cmd(drive, &rq, ide_wait);
		}
	}
}

/*
 * This routine is called to flush all partitions and partition tables
 * for a changed disk, and then re-read the new partition table.
 * If we are revalidating a disk because of a media change, then we
 * enter with usage == 0.  If we are using an ioctl, we automatically have
 * usage == 1 (we need an open channel to use an ioctl :-), so this
 * is our limit.
 */
static int revalidate_disk(kdev_t i_rdev)
{
	ide_drive_t *drive;
	unsigned int p, major, minor;
	long flags;

	if ((drive = get_info_ptr(i_rdev)) == NULL)
		return -ENODEV;

	major = MAJOR(i_rdev);
	minor = drive->select.b.unit << PARTN_BITS;
	save_flags(flags);
	cli();
	if (drive->busy || (drive->usage > 1)) {
		restore_flags(flags);
		return -EBUSY;
	};
	drive->busy = 1;
	restore_flags(flags);

	for (p = 0; p < (1<<PARTN_BITS); ++p) {
		if (drive->part[p].nr_sects > 0) {
			kdev_t devp = MKDEV(major, minor+p);
			sync_dev           (devp);
			invalidate_inodes  (devp);
			invalidate_buffers (devp);
		}
		drive->part[p].start_sect = 0;
		drive->part[p].nr_sects   = 0;
	};

	drive->part[0].nr_sects = current_capacity(drive);
	if (drive->media == ide_disk)
		resetup_one_dev(HWIF(drive)->gd, drive->select.b.unit);

	drive->busy = 0;
	wake_up(&drive->wqueue);
	return 0;
}

static int write_fs_long (unsigned long useraddr, long value)
{
	int err;

	if (NULL == (long *)useraddr)
		return -EINVAL;
	if ((err = verify_area(VERIFY_WRITE, (long *)useraddr, sizeof(long))))
		return err;
	put_user((unsigned)value, (long *) useraddr);
	return 0;
}

static int ide_ioctl (struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct hd_geometry *loc = (struct hd_geometry *) arg;
	int err;
	ide_drive_t *drive;
	unsigned long flags;
	struct request rq;

	ide_init_drive_cmd (&rq);
	if (!inode || !(inode->i_rdev))
		return -EINVAL;
	if ((drive = get_info_ptr(inode->i_rdev)) == NULL)
		return -ENODEV;
	switch (cmd) {
		case HDIO_GETGEO:
			if (!loc || drive->media != ide_disk) return -EINVAL;
			err = verify_area(VERIFY_WRITE, loc, sizeof(*loc));
			if (err) return err;
			put_user(drive->bios_head, (byte *) &loc->heads);
			put_user(drive->bios_sect, (byte *) &loc->sectors);
			put_user(drive->bios_cyl, (unsigned short *) &loc->cylinders);
			put_user((unsigned)drive->part[MINOR(inode->i_rdev)&PARTN_MASK].start_sect,
				(unsigned long *) &loc->start);
			return 0;

		case BLKFLSBUF:
			if(!suser()) return -EACCES;
			fsync_dev(inode->i_rdev);
			invalidate_buffers(inode->i_rdev);
			return 0;

		case BLKRASET:
			if(!suser()) return -EACCES;
			if(arg > 0xff) return -EINVAL;
			read_ahead[MAJOR(inode->i_rdev)] = arg;
			return 0;

		case BLKRAGET:
			return write_fs_long(arg, read_ahead[MAJOR(inode->i_rdev)]);

	 	case BLKGETSIZE:   /* Return device size */
			return write_fs_long(arg, drive->part[MINOR(inode->i_rdev)&PARTN_MASK].nr_sects);
		case BLKRRPART: /* Re-read partition tables */
			return revalidate_disk(inode->i_rdev);

		case HDIO_GET_KEEPSETTINGS:
			return write_fs_long(arg, drive->keep_settings);

		case HDIO_GET_UNMASKINTR:
			return write_fs_long(arg, drive->unmask);

		case HDIO_GET_DMA:
			return write_fs_long(arg, drive->using_dma);

		case HDIO_GET_32BIT:
			return write_fs_long(arg, drive->io_32bit);

		case HDIO_GET_MULTCOUNT:
			return write_fs_long(arg, drive->mult_count);

		case HDIO_GET_IDENTITY:
			if (!arg || (MINOR(inode->i_rdev) & PARTN_MASK))
				return -EINVAL;
			if (drive->id == NULL)
				return -ENOMSG;
			err = verify_area(VERIFY_WRITE, (char *)arg, sizeof(*drive->id));
			if (!err)
				memcpy_tofs((char *)arg, (char *)drive->id, sizeof(*drive->id));
			return err;

			case HDIO_GET_NOWERR:
			return write_fs_long(arg, drive->bad_wstat == BAD_R_STAT);

		case HDIO_SET_DMA:
#ifdef CONFIG_BLK_DEV_IDECD
			if (drive->media == ide_cdrom)
				return -EPERM;
#endif /* CONFIG_BLK_DEV_IDECD */
			if (!drive->id || !(drive->id->capability & 1) || !HWIF(drive)->dmaproc)
				return -EPERM;
		case HDIO_SET_KEEPSETTINGS:
		case HDIO_SET_UNMASKINTR:
		case HDIO_SET_NOWERR:
			if (arg > 1)
				return -EINVAL;
		case HDIO_SET_32BIT:
			if (!suser())
				return -EACCES;
			if ((MINOR(inode->i_rdev) & PARTN_MASK))
				return -EINVAL;
			save_flags(flags);
			cli();
			switch (cmd) {
				case HDIO_SET_DMA:
					if (!(HWIF(drive)->dmaproc)) {
						restore_flags(flags);
						return -EPERM;
					}
					drive->using_dma = arg;
					break;
				case HDIO_SET_KEEPSETTINGS:
					drive->keep_settings = arg;
					break;
				case HDIO_SET_UNMASKINTR:
					if (arg && HWIF(drive)->no_unmask) {
						restore_flags(flags);
						return -EPERM;
					}
					drive->unmask = arg;
					break;
				case HDIO_SET_NOWERR:
					drive->bad_wstat = arg ? BAD_R_STAT : BAD_W_STAT;
					break;
				case HDIO_SET_32BIT:
					if (arg > (1 + (SUPPORT_VLB_SYNC<<1)))
						return -EINVAL;
					drive->io_32bit = arg;
#ifdef CONFIG_BLK_DEV_DTC2278
					if (HWIF(drive)->chipset == ide_dtc2278)
						HWIF(drive)->drives[!drive->select.b.unit].io_32bit = arg;
#endif /* CONFIG_BLK_DEV_DTC2278 */
					break;
			}
			restore_flags(flags);
			return 0;

		case HDIO_SET_MULTCOUNT:
			if (!suser())
				return -EACCES;
			if (MINOR(inode->i_rdev) & PARTN_MASK)
				return -EINVAL;
			if (drive->id && arg > drive->id->max_multsect)
				return -EINVAL;
			save_flags(flags);
			cli();
			if (drive->special.b.set_multmode) {
				restore_flags(flags);
				return -EBUSY;
			}
			drive->mult_req = arg;
			drive->special.b.set_multmode = 1;
			restore_flags(flags);
			(void) ide_do_drive_cmd (drive, &rq, ide_wait);
			return (drive->mult_count == arg) ? 0 : -EIO;

		case HDIO_DRIVE_CMD:
		{
			unsigned long args;

			if (NULL == (long *) arg)
				err = ide_do_drive_cmd(drive, &rq, ide_wait);
			else {
				if (!(err = verify_area(VERIFY_READ,(long *)arg,sizeof(long))))
				{
					args = get_user((long *)arg);
					if (!(err = verify_area(VERIFY_WRITE,(long *)arg,sizeof(long)))) {
						rq.buffer = (char *) &args;
						err = ide_do_drive_cmd(drive, &rq, ide_wait);
						put_user(args,(long *)arg);
					}
				}
			}
			return err;
		}
		case HDIO_SET_PIO_MODE:
			if (!suser())
				return -EACCES;
			if (MINOR(inode->i_rdev) & PARTN_MASK)
				return -EINVAL;
			if (!HWIF(drive)->tuneproc)
				return -ENOSYS;
			save_flags(flags);
			cli();
			drive->pio_req = (int) arg;
			drive->special.b.set_pio = 1;
			restore_flags(flags);
			return 0;

		RO_IOCTLS(inode->i_rdev, arg);

		default:
#ifdef CONFIG_BLK_DEV_IDECD
			if (drive->media == ide_cdrom)
				return ide_cdrom_ioctl(drive, inode, file, cmd, arg);
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
			if (drive->media == ide_tape)
				return idetape_blkdev_ioctl(drive, inode, file, cmd, arg);
#endif /* CONFIG_BLK_DEV_IDETAPE */
			return -EPERM;
	}
}

static int ide_check_media_change (kdev_t i_rdev)
{
	ide_drive_t *drive;

	if ((drive = get_info_ptr(i_rdev)) == NULL)
		return -ENODEV;
#ifdef CONFIG_BLK_DEV_IDECD
	if (drive->media == ide_cdrom)
		return ide_cdrom_check_media_change (drive);
#endif	/* CONFIG_BLK_DEV_IDECD */
	if (drive->removeable) /* for disks */
		return 1;	/* always assume it was changed */
	return 0;
}

void ide_fixstring (byte *s, const int bytecount, const int byteswap)
{
	byte *p = s, *end = &s[bytecount & ~1]; /* bytecount must be even */

	if (byteswap) {
		/* convert from big-endian to host byte order */
		for (p = end ; p != s;) {
			unsigned short *pp = (unsigned short *) (p -= 2);
			*pp = ntohs(*pp);
		}
	}

	/* strip leading blanks */
	while (s != end && *s == ' ')
		++s;

	/* compress internal blanks and strip trailing blanks */
	while (s != end && *s) {
		if (*s++ != ' ' || (s != end && *s && *s != ' '))
			*p++ = *(s-1);
	}

	/* wipe out trailing garbage */
	while (p != end)
		*p++ = '\0';
}

static inline void do_identify (ide_drive_t *drive, byte cmd)
{
	int bswap;
	struct hd_driveid *id;
	unsigned long capacity, check;

	id = drive->id = kmalloc (SECTOR_WORDS*4, GFP_KERNEL);
	ide_input_data(drive, id, SECTOR_WORDS);	/* read 512 bytes of id info */
	sti();

	/*
	 * EATA SCSI controllers do a hardware ATA emulation:  ignore them
	 */
	if ((id->model[0] == 'P' && id->model[1] == 'M')
	 || (id->model[0] == 'S' && id->model[1] == 'K')) {
		printk("%s: EATA SCSI HBA %.10s\n", drive->name, id->model);
		drive->present = 0;
		return;
	}

	/*
	 *  WIN_IDENTIFY returns little-endian info,
	 *  WIN_PIDENTIFY *usually* returns little-endian info.
	 */
	bswap = 1;
	if (cmd == WIN_PIDENTIFY) {
		if ((id->model[0] == 'N' && id->model[1] == 'E') /* NEC */
		 || (id->model[0] == 'F' && id->model[1] == 'X') /* Mitsumi */
		 || (id->model[0] == 'P' && id->model[1] == 'i'))/* Pioneer */
			bswap = 0;	/* Vertos drives may still be weird */
	}
	ide_fixstring (id->model,     sizeof(id->model),     bswap);
	ide_fixstring (id->fw_rev,    sizeof(id->fw_rev),    bswap);
	ide_fixstring (id->serial_no, sizeof(id->serial_no), bswap);

	/*
	 * Check for an ATAPI device
	 */

	if (cmd == WIN_PIDENTIFY) {
		byte type = (id->config >> 8) & 0x1f;
		printk("%s: %s, ATAPI ", drive->name, id->model);
		switch (type) {
			case 0:		/* Early cdrom models used zero */
			case 5:
#ifdef CONFIG_BLK_DEV_IDECD
				printk ("CDROM drive\n");
				drive->media = ide_cdrom;
 				drive->present = 1;
				drive->removeable = 1;
				return;
#else
				printk ("CDROM ");
				break;
#endif /* CONFIG_BLK_DEV_IDECD */
			case 1:
#ifdef CONFIG_BLK_DEV_IDETAPE
				printk ("TAPE drive");
				if (idetape_identify_device (drive,id)) {
					drive->media = ide_tape;
					drive->present = 1;
					drive->removeable = 1;
					if (HWIF(drive)->dmaproc != NULL &&
					    !HWIF(drive)->dmaproc(ide_dma_check, drive))
						printk(", DMA");
					printk("\n");
				}
				else {
					drive->present = 0;
					printk ("\nide-tape: the tape is not supported by this version of the driver\n");
				}
				return;
#else
				printk ("TAPE ");
				break;
#endif /* CONFIG_BLK_DEV_IDETAPE */
			default:
				drive->present = 0;
				printk("Type %d - Unknown device\n", type);
				return;
		}
		drive->present = 0;
		printk("- not supported by this kernel\n");
		return;
	}

	/* check for removeable disks (eg. SYQUEST), ignore 'WD' drives */
	if (id->config & (1<<7)) {	/* removeable disk ? */
		if (id->model[0] != 'W' || id->model[1] != 'D')
			drive->removeable = 1;
	}

	drive->media = ide_disk;
	/* Extract geometry if we did not already have one for the drive */
	if (!drive->present) {
		drive->present = 1;
		drive->cyl     = drive->bios_cyl  = id->cyls;
		drive->head    = drive->bios_head = id->heads;
		drive->sect    = drive->bios_sect = id->sectors;
	}
	/* Handle logical geometry translation by the drive */
	if ((id->field_valid & 1) && id->cur_cyls && id->cur_heads
	 && (id->cur_heads <= 16) && id->cur_sectors)
	{
		/*
		 * Extract the physical drive geometry for our use.
		 * Note that we purposely do *not* update the bios info.
		 * This way, programs that use it (like fdisk) will
		 * still have the same logical view as the BIOS does,
		 * which keeps the partition table from being screwed.
		 *
		 * An exception to this is the cylinder count,
		 * which we reexamine later on to correct for 1024 limitations.
		 */
		drive->cyl  = id->cur_cyls;
		drive->head = id->cur_heads;
		drive->sect = id->cur_sectors;

		/* check for word-swapped "capacity" field in id information */
		capacity = drive->cyl * drive->head * drive->sect;
		check = (id->cur_capacity0 << 16) | id->cur_capacity1;
		if (check == capacity) {	/* was it swapped? */
			/* yes, bring it into little-endian order: */
			id->cur_capacity0 = (capacity >>  0) & 0xffff;
			id->cur_capacity1 = (capacity >> 16) & 0xffff;
		}
	}
	/* Use physical geometry if what we have still makes no sense */
	if ((!drive->head || drive->head > 16) && id->heads && id->heads <= 16) {
		drive->cyl  = id->cyls;
		drive->head = id->heads;
		drive->sect = id->sectors;
	}
	/* Correct the number of cyls if the bios value is too small */
	if (drive->sect == drive->bios_sect && drive->head == drive->bios_head) {
		if (drive->cyl > drive->bios_cyl)
			drive->bios_cyl = drive->cyl;
	}

	(void) current_capacity (drive); /* initialize LBA selection */

	printk ("%s: %.40s, %ldMB w/%dKB Cache, %sCHS=%d/%d/%d",
	 drive->name, id->model, current_capacity(drive)/2048L, id->buf_size/2,
	 drive->select.b.lba ? "LBA, " : "",
	 drive->bios_cyl, drive->bios_head, drive->bios_sect);

	drive->mult_count = 0;
	if (id->max_multsect) {
		drive->mult_req = INITIAL_MULT_COUNT;
		if (drive->mult_req > id->max_multsect)
			drive->mult_req = id->max_multsect;
		if (drive->mult_req || ((id->multsect_valid & 1) && id->multsect))
			drive->special.b.set_multmode = 1;
	}
	if (HWIF(drive)->dmaproc != NULL) {	/* hwif supports DMA? */
		if (!(HWIF(drive)->dmaproc(ide_dma_check, drive)))
			printk(", DMA");
	}
	printk("\n");
}

/*
 * Delay for *at least* 10ms.  As we don't know how much time is left
 * until the next tick occurs, we wait an extra tick to be safe.
 * This is used only during the probing/polling for drives at boot time.
 */
static void delay_10ms (void)
{
	unsigned long timer = jiffies + (HZ + 99)/100 + 1;
	while (timer > jiffies);
}

/*
 * try_to_identify() sends an ATA(PI) IDENTIFY request to a drive
 * and waits for a response.  It also monitors irqs while this is
 * happening, in hope of automatically determining which one is
 * being used by the interface.
 *
 * Returns:	0  device was identified
 *		1  device timed-out (no response to identify request)
 *		2  device aborted the command (refused to identify itself)
 */
static int try_to_identify (ide_drive_t *drive, byte cmd)
{
	int hd_status, rc;
	unsigned long timeout;
	int irqs = 0;

	if (!HWIF(drive)->irq) {		/* already got an IRQ? */
		probe_irq_off(probe_irq_on());	/* clear dangling irqs */
		irqs = probe_irq_on();		/* start monitoring irqs */
		OUT_BYTE(drive->ctl,IDE_CONTROL_REG);	/* enable device irq */
	}

	delay_10ms();				/* take a deep breath */
	if ((IN_BYTE(IDE_ALTSTATUS_REG) ^ IN_BYTE(IDE_STATUS_REG)) & ~INDEX_STAT) {
		printk("%s: probing with STATUS instead of ALTSTATUS\n", drive->name);
		hd_status = IDE_STATUS_REG;	/* ancient Seagate drives */
	} else
		hd_status = IDE_ALTSTATUS_REG;	/* use non-intrusive polling */

	OUT_BYTE(cmd,IDE_COMMAND_REG);		/* ask drive for ID */
	timeout = ((cmd == WIN_IDENTIFY) ? WAIT_WORSTCASE : WAIT_PIDENTIFY) / 2;
	timeout += jiffies;
	do {
		if (jiffies > timeout) {
			if (!HWIF(drive)->irq)
				(void) probe_irq_off(irqs);
			return 1;	/* drive timed-out */
		}
		delay_10ms();		/* give drive a breather */
	} while (IN_BYTE(hd_status) & BUSY_STAT);

	delay_10ms();		/* wait for IRQ and DRQ_STAT */
	if (OK_STAT(GET_STAT(),DRQ_STAT,BAD_R_STAT)) {
		cli();			/* some systems need this */
		do_identify(drive, cmd); /* drive returned ID */
		if (drive->present && drive->media != ide_tape) {
			ide_tuneproc_t *tuneproc = HWIF(drive)->tuneproc;
			if (tuneproc != NULL && drive->autotune == 1)
				tuneproc(drive, 255);	/* auto-tune PIO mode */
		}
		rc = 0;			/* drive responded with ID */
	} else
		rc = 2;			/* drive refused ID */
	if (!HWIF(drive)->irq) {
		irqs = probe_irq_off(irqs);	/* get irq number */
		if (irqs > 0)
			HWIF(drive)->irq = irqs;
		else				/* Mmmm.. multiple IRQs */
			printk("%s: IRQ probe failed (%d)\n", drive->name, irqs);
	}
	return rc;
}

/*
 * do_probe() has the difficult job of finding a drive if it exists,
 * without getting hung up if it doesn't exist, without trampling on
 * ethernet cards, and without leaving any IRQs dangling to haunt us later.
 *
 * If a drive is "known" to exist (from CMOS or kernel parameters),
 * but does not respond right away, the probe will "hang in there"
 * for the maximum wait time (about 30 seconds), otherwise it will
 * exit much more quickly.
 *
 * Returns:	0  device was identified
 *		1  device timed-out (no response to identify request)
 *		2  device aborted the command (refused to identify itself)
 *		3  bad status from device (possible for ATAPI drives)
 *		4  probe was not attempted because failure was obvious
 */
static int do_probe (ide_drive_t *drive, byte cmd)
{
	int rc;
#ifdef CONFIG_BLK_DEV_IDEATAPI
	if (drive->present) {	/* avoid waiting for inappropriate probes */
		if ((drive->media != ide_disk) && (cmd == WIN_IDENTIFY))
			return 4;
	}
#endif	/* CONFIG_BLK_DEV_IDEATAPI */
#ifdef DEBUG
	printk("probing for %s: present=%d, media=%d, probetype=%s\n",
		drive->name, drive->present, drive->media,
		(cmd == WIN_IDENTIFY) ? "ATA" : "ATAPI");
#endif
#ifdef CONFIG_BLK_DEV_HT6560B
	if (HWIF(drive)->selectproc)
		HWIF(drive)->selectproc (drive);
#endif /* CONFIG_BLK_DEV_HT6560B */
	OUT_BYTE(drive->select.all,IDE_SELECT_REG);	/* select target drive */
	delay_10ms();				/* wait for BUSY_STAT */
	if (IN_BYTE(IDE_SELECT_REG) != drive->select.all && !drive->present) {
		OUT_BYTE(0xa0,IDE_SELECT_REG);	/* exit with drive0 selected */
		return 3;    /* no i/f present: avoid killing ethernet cards */
	}

	if (OK_STAT(GET_STAT(),READY_STAT,BUSY_STAT)
	 || drive->present || cmd == WIN_PIDENTIFY)
	{
		if ((rc = try_to_identify(drive,cmd)))   /* send cmd and wait */
			rc = try_to_identify(drive,cmd); /* failed: try again */
		if (rc == 1)
			printk("%s: no response (status = 0x%02x)\n", drive->name, GET_STAT());
		(void) GET_STAT();		/* ensure drive irq is clear */
	} else {
		rc = 3;				/* not present or maybe ATAPI */
	}
	if (drive->select.b.unit != 0) {
		OUT_BYTE(0xa0,IDE_SELECT_REG);	/* exit with drive0 selected */
		delay_10ms();
		(void) GET_STAT();		/* ensure drive irq is clear */
	}
	return rc;
}

/*
 * probe_for_drive() tests for existance of a given drive using do_probe().
 *
 * Returns:	0  no device was found
 *		1  device was found (note: drive->present might still be 0)
 */
static inline byte probe_for_drive (ide_drive_t *drive)
{
	if (drive->noprobe)			/* skip probing? */
		return drive->present;
	if (do_probe(drive, WIN_IDENTIFY) >= 2) { /* if !(success||timed-out) */
#ifdef CONFIG_BLK_DEV_IDEATAPI
		(void) do_probe(drive, WIN_PIDENTIFY); /* look for ATAPI device */
#endif	/* CONFIG_BLK_DEV_IDEATAPI */
	}
	if (!drive->present)
		return 0;			/* drive not found */
	if (drive->id == NULL) {		/* identification failed? */
		if (drive->media == ide_disk) {
			printk ("%s: non-IDE drive, CHS=%d/%d/%d\n",
			 drive->name, drive->cyl, drive->head, drive->sect);
		}
#ifdef CONFIG_BLK_DEV_IDECD
		else if (drive->media == ide_cdrom) {
			printk("%s: ATAPI cdrom (?)\n", drive->name);
		}
#endif	/* CONFIG_BLK_DEV_IDECD */
		else {
			drive->present = 0;	/* nuke it */
			return 1;		/* drive was found */
		}
	}
	if (drive->media == ide_disk && !drive->select.b.lba) {
		if (!drive->head || drive->head > 16) {
			printk("%s: INVALID GEOMETRY: %d PHYSICAL HEADS?\n",
			 drive->name, drive->head);
			drive->present = 0;
		}
	}
	return 1;	/* drive was found */
}

/*
 *  This routine only knows how to look for drive units 0 and 1
 *  on an interface, so any setting of MAX_DRIVES > 2 won't work here.
 */
static void probe_for_drives (ide_hwif_t *hwif)
{
	unsigned int unit;

	if (check_region(hwif->io_base,8) || check_region(hwif->ctl_port,1)) {
		int msgout = 0;
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			if (drive->present) {
				drive->present = 0;
				printk("%s: ERROR, PORTS ALREADY IN USE\n", drive->name);
				msgout = 1;
			}
		}
		if (!msgout)
			printk("%s: ports already in use, skipping probe\n", hwif->name);
	} else {
		unsigned long flags;
		save_flags(flags);

#if (MAX_DRIVES > 2)
		printk("%s: probing for first 2 of %d possible drives\n", hwif->name, MAX_DRIVES);
#endif
		sti();	/* needed for jiffies and irq probing */
		/*
		 * Second drive should only exist if first drive was found,
		 * but a lot of cdrom drives seem to be configured as slave-only
		 */
		for (unit = 0; unit < 2; ++unit) { /* note the hardcoded '2' */
			ide_drive_t *drive = &hwif->drives[unit];
			(void) probe_for_drive (drive);
		}
		for (unit = 0; unit < MAX_DRIVES; ++unit) {
			ide_drive_t *drive = &hwif->drives[unit];
			if (drive->present) {
				hwif->present = 1;
				request_region(hwif->io_base,  8, hwif->name);
				request_region(hwif->ctl_port, 1, hwif->name);
				break;
			}
		}
		restore_flags(flags);
	}
}

/*
 * stridx() returns the offset of c within s,
 * or -1 if c is '\0' or not found within s.
 */
static int stridx (const char *s, char c)
{
	char *i = strchr(s, c);
	return (i && c) ? i - s : -1;
}

/*
 * match_parm() does parsing for ide_setup():
 *
 * 1. the first char of s must be '='.
 * 2. if the remainder matches one of the supplied keywords,
 *     the index (1 based) of the keyword is negated and returned.
 * 3. if the remainder is a series of no more than max_vals numbers
 *     separated by commas, the numbers are saved in vals[] and a
 *     count of how many were saved is returned.  Base10 is assumed,
 *     and base16 is allowed when prefixed with "0x".
 * 4. otherwise, zero is returned.
 */
static int match_parm (char *s, const char *keywords[], int vals[], int max_vals)
{
	static const char *decimal = "0123456789";
	static const char *hex = "0123456789abcdef";
	int i, n;

	if (*s++ == '=') {
		/*
		 * Try matching against the supplied keywords,
		 * and return -(index+1) if we match one
		 */
		for (i = 0; *keywords != NULL; ++i) {
			if (!strcmp(s, *keywords++))
				return -(i+1);
		}
		/*
		 * Look for a series of no more than "max_vals"
		 * numeric values separated by commas, in base10,
		 * or base16 when prefixed with "0x".
		 * Return a count of how many were found.
		 */
		for (n = 0; (i = stridx(decimal, *s)) >= 0;) {
			vals[n] = i;
			while ((i = stridx(decimal, *++s)) >= 0)
				vals[n] = (vals[n] * 10) + i;
			if (*s == 'x' && !vals[n]) {
				while ((i = stridx(hex, *++s)) >= 0)
					vals[n] = (vals[n] * 0x10) + i;
			}
			if (++n == max_vals)
				break;
			if (*s == ',')
				++s;
		}
		if (!*s)
			return n;
	}
	return 0;	/* zero = nothing matched */
}

/*
 * ide_setup() gets called VERY EARLY during initialization,
 * to handle kernel "command line" strings beginning with "hdx="
 * or "ide".  Here is the complete set currently supported:
 *
 * "hdx="  is recognized for all "x" from "a" to "h", such as "hdc".
 * "idex=" is recognized for all "x" from "0" to "3", such as "ide1".
 *
 * "hdx=noprobe"	: drive may be present, but do not probe for it
 * "hdx=nowerr"		: ignore the WRERR_STAT bit on this drive
 * "hdx=cdrom"		: drive is present, and is a cdrom drive
 * "hdx=cyl,head,sect"	: disk drive is present, with specified geometry
 * "hdx=autotune"	: driver will attempt to tune interface speed
 *				to the fastest PIO mode supported,
 *				if possible for this drive only.
 *				Not fully supported by all chipset types,
 *				and quite likely to cause trouble with
 *				older/odd IDE drives.
 *
 * "idex=noprobe"	: do not attempt to access/use this interface
 * "idex=base"		: probe for an interface at the addr specified,
 *				where "base" is usually 0x1f0 or 0x170
 *				and "ctl" is assumed to be "base"+0x206
 * "idex=base,ctl"	: specify both base and ctl
 * "idex=base,ctl,irq"	: specify base, ctl, and irq number
 * "idex=autotune"	: driver will attempt to tune interface speed
 *				to the fastest PIO mode supported,
 *				for all drives on this interface.
 *				Not fully supported by all chipset types,
 *				and quite likely to cause trouble with
 *				older/odd IDE drives.
 * "idex=noautotune"	: driver will NOT attempt to tune interface speed
 *				This is the default for most chipsets,
 *				except the cmd640.
 *
 * The following two are valid ONLY on ide0,
 * and the defaults for the base,ctl ports must not be altered.
 *
 * "ide0=serialize"	: do not overlap operations on ide0 and ide1.
 * "ide0=dtc2278"	: probe/support DTC2278 interface
 * "ide0=ht6560b"	: probe/support HT6560B interface
 * "ide0=cmd640_vlb"	: *REQUIRED* for VLB cards with the CMD640 chip
 *			  (not for PCI -- automatically detected)
 * "ide0=qd6580"	: probe/support qd6580 interface
 * "ide0=ali14xx"	: probe/support ali14xx chipsets (ALI M1439, M1443, M1445)
 * "ide0=umc8672"	: probe/support umc8672 chipsets
 */
void ide_setup (char *s)
{
	int i, vals[3];
	ide_hwif_t *hwif;
	ide_drive_t *drive;
	unsigned int hw, unit;
	const char max_drive = 'a' + ((MAX_HWIFS * MAX_DRIVES) - 1);
	const char max_hwif  = '0' + (MAX_HWIFS - 1);

	printk("ide_setup: %s", s);
	init_ide_data ();

	/*
	 * Look for drive options:  "hdx="
	 */
	if (s[0] == 'h' && s[1] == 'd' && s[2] >= 'a' && s[2] <= max_drive) {
		const char *hd_words[] = {"noprobe", "nowerr", "cdrom", "serialize",
						"autotune", "noautotune", NULL};
		unit = s[2] - 'a';
		hw   = unit / MAX_DRIVES;
		unit = unit % MAX_DRIVES;
		hwif = &ide_hwifs[hw];
		drive = &hwif->drives[unit];
		switch (match_parm(&s[3], hd_words, vals, 3)) {
			case -1: /* "noprobe" */
				drive->noprobe = 1;
				goto done;
			case -2: /* "nowerr" */
				drive->bad_wstat = BAD_R_STAT;
				hwif->noprobe = 0;
				goto done;
			case -3: /* "cdrom" */
				drive->present = 1;
				drive->media = ide_cdrom;
				hwif->noprobe = 0;
				goto done;
			case -4: /* "serialize" */
				printk(" -- USE \"ide%c=serialize\" INSTEAD", '0'+hw);
				goto do_serialize;
			case -5: /* "autotune" */
				drive->autotune = 1;
				goto done;
			case -6: /* "noautotune" */
				drive->autotune = 2;
				goto done;
			case 3: /* cyl,head,sect */
				drive->media	= ide_disk;
				drive->cyl	= drive->bios_cyl  = vals[0];
				drive->head	= drive->bios_head = vals[1];
				drive->sect	= drive->bios_sect = vals[2];
				drive->present	= 1;
				drive->forced_geom = 1;
				hwif->noprobe = 0;
				goto done;
			default:
				goto bad_option;
		}
	}
	/*
	 * Look for interface options:  "idex="
	 */
	if (s[0] == 'i' && s[1] == 'd' && s[2] == 'e' && s[3] >= '0' && s[3] <= max_hwif) {
		/*
		 * Be VERY CAREFUL changing this: note hardcoded indexes below
		 */
		const char *ide_words[] = {"noprobe", "serialize", "autotune", "noautotune",
			"qd6580", "ht6560b", "cmd640_vlb", "dtc2278", "umc8672", "ali14xx", NULL};
		hw = s[3] - '0';
		hwif = &ide_hwifs[hw];
		i = match_parm(&s[4], ide_words, vals, 3);

		/*
		 * Cryptic check to ensure chipset not already set for hwif:
		 */
		if (i != -1 && i != -2) {
			if (hwif->chipset != ide_unknown)
				goto bad_option;
			if (i < 0 && ide_hwifs[1].chipset != ide_unknown)
				goto bad_option;
		}
		/*
		 * Interface keywords work only for ide0:
		 */
		if (i <= -6 && hw != 0)
			goto bad_hwif;

		switch (i) {
#ifdef CONFIG_BLK_DEV_ALI14XX
			case -10: /* "ali14xx" */
			{
				extern void init_ali14xx (void);
				init_ali14xx();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_ALI14XX */
#ifdef CONFIG_BLK_DEV_UMC8672
			case -9: /* "umc8672" */
			{
				extern void init_umc8672 (void);
				init_umc8672();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_UMC8672 */
#ifdef CONFIG_BLK_DEV_DTC2278
			case -8: /* "dtc2278" */
			{
				extern void init_dtc2278 (void);
				init_dtc2278();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_DTC2278 */
#ifdef CONFIG_BLK_DEV_CMD640
			case -7: /* "cmd640_vlb" */
			{
				extern int cmd640_vlb; /* flag for cmd640.c */
				cmd640_vlb = 1;
				goto done;
			}
#endif /* CONFIG_BLK_DEV_CMD640 */
#ifdef CONFIG_BLK_DEV_HT6560B
			case -6: /* "ht6560b" */
			{
				extern void init_ht6560b (void);
				init_ht6560b();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_HT6560B */
#if CONFIG_BLK_DEV_QD6580
			case -5: /* "qd6580" (no secondary i/f) */
			{
				extern void init_qd6580 (void);
				init_qd6580();
				goto done;
			}
#endif /* CONFIG_BLK_DEV_QD6580 */
			case -4: /* "noautotune" */
				hwif->drives[0].autotune = 2;
				hwif->drives[1].autotune = 2;
				goto done;
			case -3: /* "autotune" */
				hwif->drives[0].autotune = 1;
				hwif->drives[1].autotune = 1;
				goto done;
			case -2: /* "serialize" */
			do_serialize:
				if (hw > 1) goto bad_hwif;
				ide_hwifs[0].serialized = 1;
				goto done;

			case -1: /* "noprobe" */
				hwif->noprobe = 1;
				goto done;

			case 1:	/* base */
				vals[1] = vals[0] + 0x206; /* default ctl */
			case 2: /* base,ctl */
				vals[2] = 0;	/* default irq = probe for it */
			case 3: /* base,ctl,irq */
				hwif->io_base  = vals[0];
				hwif->ctl_port = vals[1];
				hwif->irq      = vals[2];
				hwif->noprobe  = 0;
				hwif->chipset  = ide_generic;
				goto done;

			case 0: goto bad_option;
			default:
				printk(" -- SUPPORT NOT CONFIGURED IN THIS KERNEL\n");
				return;
		}
	}
bad_option:
	printk(" -- BAD OPTION\n");
	return;
bad_hwif:
	printk("-- NOT SUPPORTED ON ide%d", hw);
done:
	printk("\n");
}

/*
 * This routine is called from the partition-table code in genhd.c
 * to "convert" a drive to a logical geometry with fewer than 1024 cyls.
 *
 * The second parameter, "xparm", determines exactly how the translation 
 * will be handled:
 *		 0 = convert to CHS with fewer than 1024 cyls
 *			using the same method as Ontrack DiskManager.
 *		 1 = same as "0", plus offset everything by 63 sectors.
 *		-1 = similar to "0", plus redirect sector 0 to sector 1.
 *		>1 = convert to a CHS geometry with "xparm" heads.
 *
 * Returns 0 if the translation was not possible, if the device was not 
 * an IDE disk drive, or if a geometry was "forced" on the commandline.
 * Returns 1 if the geometry translation was successful.
 */
int ide_xlate_1024 (kdev_t i_rdev, int xparm, const char *msg)
{
	ide_drive_t *drive;
	static const byte head_vals[] = {4, 8, 16, 32, 64, 128, 255, 0};
	const byte *heads = head_vals;
	unsigned long tracks;

	if ((drive = get_info_ptr(i_rdev)) == NULL || drive->forced_geom)
		return 0;

	if (xparm > 1 && xparm <= drive->bios_head && drive->bios_sect == 63)
		return 0;		/* we already have a translation */

	printk("%s ", msg);

	if (drive->id) {
		drive->cyl  = drive->id->cyls;
		drive->head = drive->id->heads;
		drive->sect = drive->id->sectors;
	}
	drive->bios_cyl  = drive->cyl;
	drive->bios_head = drive->head;
	drive->bios_sect = drive->sect;
	drive->special.b.set_geometry = 1;

	tracks = drive->bios_cyl * drive->bios_head * drive->bios_sect / 63;
	drive->bios_sect = 63;
	if (xparm > 1) {
		drive->bios_head = xparm;
		drive->bios_cyl = tracks / drive->bios_head;
	} else {
		while (drive->bios_cyl >= 1024) {
			drive->bios_head = *heads;
			drive->bios_cyl = tracks / drive->bios_head;
			if (0 == *++heads)
				break;
		}
#if FAKE_FDISK_FOR_EZDRIVE
		if (xparm == -1) {
			drive->remap_0_to_1 = 1;
			msg = "0->1";
		} else
#endif /* FAKE_FDISK_FOR_EZDRIVE */
		if (xparm == 1) {
			drive->sect0 = 63;
			drive->bios_cyl = (tracks - 1) / drive->bios_head;
			msg = "+63";
		}
		printk("[remap %s] ", msg);
	}
	drive->part[0].nr_sects = current_capacity(drive);
	printk("[%d/%d/%d]", drive->bios_cyl, drive->bios_head, drive->bios_sect);
	return 1;
}

/*
 * We query CMOS about hard disks : it could be that we have a SCSI/ESDI/etc
 * controller that is BIOS compatible with ST-506, and thus showing up in our
 * BIOS table, but not register compatible, and therefore not present in CMOS.
 *
 * Furthermore, we will assume that our ST-506 drives <if any> are the primary
 * drives in the system -- the ones reflected as drive 1 or 2.  The first
 * drive is stored in the high nibble of CMOS byte 0x12, the second in the low
 * nibble.  This will be either a 4 bit drive type or 0xf indicating use byte
 * 0x19 for an 8 bit type, drive 1, 0x1a for drive 2 in CMOS.  A non-zero value
 * means we have an AT controller hard disk for that drive.
 *
 * Of course, there is no guarantee that either drive is actually on the
 * "primary" IDE interface, but we don't bother trying to sort that out here.
 * If a drive is not actually on the primary interface, then these parameters
 * will be ignored.  This results in the user having to supply the logical
 * drive geometry as a boot parameter for each drive not on the primary i/f.
 *
 * The only "perfect" way to handle this would be to modify the setup.[cS] code
 * to do BIOS calls Int13h/Fn08h and Int13h/Fn48h to get all of the drive info
 * for us during initialization.  I have the necessary docs -- any takers?  -ml
 */

static void probe_cmos_for_drives (ide_hwif_t *hwif)
{
#ifdef __i386__
	extern struct drive_info_struct drive_info;
	byte cmos_disks, *BIOS = (byte *) &drive_info;
	int unit;

	outb_p(0x12,0x70);		/* specify CMOS address 0x12 */
	cmos_disks = inb_p(0x71);	/* read the data from 0x12 */
	/* Extract drive geometry from CMOS+BIOS if not already setup */
	for (unit = 0; unit < MAX_DRIVES; ++unit) {
		ide_drive_t *drive = &hwif->drives[unit];
		if ((cmos_disks & (0xf0 >> (unit*4))) && !drive->present) {
			drive->cyl   = drive->bios_cyl  = *(unsigned short *)BIOS;
			drive->head  = drive->bios_head = *(BIOS+2);
			drive->sect  = drive->bios_sect = *(BIOS+14);
			drive->ctl   = *(BIOS+8);
			drive->present = 1;
		}
		BIOS += 16;
	}
#endif
}

/*
 * This routine sets up the irq for an ide interface, and creates a new
 * hwgroup for the irq/hwif if none was previously assigned.
 *
 * The SA_INTERRUPT in sa_flags means ide_intr() is always entered with
 * interrupts completely disabled.  This can be bad for interrupt latency,
 * but anything else has led to problems on some machines.  We re-enable
 * interrupts as much as we can safely do in most places.
 */
static int init_irq (ide_hwif_t *hwif)
{
	unsigned long flags;
	int irq = hwif->irq;
	ide_hwgroup_t *hwgroup = irq_to_hwgroup[irq];

	save_flags(flags);
	cli();

	/*
	 * Grab the irq if we don't already have it from a previous hwif
	 */
	if (hwgroup == NULL)  {
		if (request_irq(irq, ide_intr, SA_INTERRUPT|SA_SAMPLE_RANDOM, hwif->name)) {
			restore_flags(flags);
			printk(" -- FAILED!");
			return 1;
		}
	}
	/*
	 * Check for serialization with ide1.
	 * This code depends on us having already taken care of ide1.
	 */
	if (hwif->serialized && hwif->name[3] == '0' && ide_hwifs[1].present)
		hwgroup = ide_hwifs[1].hwgroup;
	/*
	 * If this is the first interface in a group,
	 * then we need to create the hwgroup structure
	 */
	if (hwgroup == NULL) {
		hwgroup = kmalloc (sizeof(ide_hwgroup_t), GFP_KERNEL);
		hwgroup->hwif 	 = hwif->next = hwif;
		hwgroup->rq      = NULL;
		hwgroup->handler = NULL;
		hwgroup->drive   = &hwif->drives[0];
		hwgroup->poll_timeout = 0;
		init_timer(&hwgroup->timer);
		hwgroup->timer.function = &timer_expiry;
		hwgroup->timer.data = (unsigned long) hwgroup;
	} else {
		hwif->next = hwgroup->hwif->next;
		hwgroup->hwif->next = hwif;
	}
	hwif->hwgroup = hwgroup;
	irq_to_hwgroup[irq] = hwgroup;

	restore_flags(flags);	/* safe now that hwif->hwgroup is set up */

	printk("%s at 0x%03x-0x%03x,0x%03x on irq %d", hwif->name,
		hwif->io_base, hwif->io_base+7, hwif->ctl_port, irq);
	if (hwgroup->hwif != hwif)
		printk(" (serialized with %s)", hwgroup->hwif->name);
	printk("\n");
	return 0;
}

static struct file_operations ide_fops = {
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* select */
	ide_ioctl,		/* ioctl */
	NULL,			/* mmap */
	ide_open,		/* open */
	ide_release,		/* release */
	block_fsync		/* fsync */
	,NULL,			/* fasync */
	ide_check_media_change,	/* check_media_change */
	revalidate_disk		/* revalidate */
};

#ifdef CONFIG_PCI
#if defined(CONFIG_BLK_DEV_RZ1000) || defined(CONFIG_BLK_DEV_TRITON)

typedef void (ide_pci_init_proc_t)(byte, byte);

/*
 * ide_probe_pci() scans PCI for a specific vendor/device function,
 * and invokes the supplied init routine for each instance detected.
 */
static void ide_probe_pci (unsigned short vendor, unsigned short device, ide_pci_init_proc_t *init, int func_adj)
{
	unsigned long flags;
	unsigned index;
	byte fn, bus;

	save_flags(flags);
	cli();
	for (index = 0; !pcibios_find_device (vendor, device, index, &bus, &fn); ++index) {
		init (bus, fn + func_adj);
	}
	restore_flags(flags);
}

#endif /* defined(CONFIG_BLK_DEV_RZ1000) || defined(CONFIG_BLK_DEV_TRITON) */
#endif /* CONFIG_PCI */

/*
 * ide_init_pci() finds/initializes "known" PCI IDE interfaces
 *
 * This routine should ideally be using pcibios_find_class() to find
 * all IDE interfaces, but that function causes some systems to "go weird".
 */
static void probe_for_hwifs (void)
{
#ifdef CONFIG_PCI
	/*
	 * Find/initialize PCI IDE interfaces
	 */
	if (pcibios_present()) {
#ifdef CONFIG_BLK_DEV_RZ1000
		ide_pci_init_proc_t init_rz1000;
		ide_probe_pci (PCI_VENDOR_ID_PCTECH, PCI_DEVICE_ID_PCTECH_RZ1000, &init_rz1000, 0);
#endif /* CONFIG_BLK_DEV_RZ1000 */
#ifdef CONFIG_BLK_DEV_TRITON
		/*
		 * Apparently the BIOS32 services on Intel motherboards are
		 * buggy and won't find the PCI_DEVICE_ID_INTEL_82371_1 for us.
		 * So instead, we search for PCI_DEVICE_ID_INTEL_82371_0,
		 * and then add 1.
		 */
		ide_probe_pci (PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371_0, &ide_init_triton, 1);
#endif /* CONFIG_BLK_DEV_TRITON */
	}
#endif /* CONFIG_PCI */
#ifdef CONFIG_BLK_DEV_CMD640
	{
		extern void ide_probe_for_cmd640x (void);
		ide_probe_for_cmd640x();
	}
#endif
}

/*
 * This is gets invoked once during initialization, to set *everything* up
 */
int ide_init (void)
{
	int h;

	init_ide_data ();
	/*
	 * Probe for special "known" interface chipsets
	 */
	probe_for_hwifs ();

	/*
	 * Probe for drives in the usual way.. CMOS/BIOS, then poke at ports
	 */
	for (h = 0; h < MAX_HWIFS; ++h) {
		ide_hwif_t *hwif = &ide_hwifs[h];
		if (!hwif->noprobe) {
			if (hwif->io_base == HD_DATA)
				probe_cmos_for_drives (hwif);
			probe_for_drives (hwif);
		}
		if (hwif->present) {
			if (!hwif->irq) {
				if (!(hwif->irq = default_irqs[h])) {
					printk("%s: DISABLED, NO IRQ\n", hwif->name);
					hwif->present = 0;
					continue;
				}
			}
#ifdef CONFIG_BLK_DEV_HD
			if (hwif->irq == HD_IRQ && hwif->io_base != HD_DATA) {
				printk("%s: CANNOT SHARE IRQ WITH OLD HARDDISK DRIVER (hd.c)\n", hwif->name);
				hwif->present = 0;
			}
#endif /* CONFIG_BLK_DEV_HD */
		}
	}

	/*
	 * Now we try to set up irqs and major devices for what was found
	 */
	for (h = MAX_HWIFS-1; h >= 0; --h) {
		void (*rfn)(void);
		ide_hwif_t *hwif = &ide_hwifs[h];
		if (!hwif->present)
			continue;
		hwif->present = 0; /* we set it back to 1 if all is ok below */
		switch (hwif->major) {
			case IDE0_MAJOR: rfn = &do_ide0_request; break;
			case IDE1_MAJOR: rfn = &do_ide1_request; break;
			case IDE2_MAJOR: rfn = &do_ide2_request; break;
			case IDE3_MAJOR: rfn = &do_ide3_request; break;
			default:
				printk("%s: request_fn NOT DEFINED\n", hwif->name);
				continue;
		}
		if (register_blkdev (hwif->major, hwif->name, &ide_fops)) {
			printk("%s: UNABLE TO GET MAJOR NUMBER %d\n", hwif->name, hwif->major);
		} else if (init_irq (hwif)) {
			printk("%s: UNABLE TO GET IRQ %d\n", hwif->name, hwif->irq);
			(void) unregister_blkdev (hwif->major, hwif->name);
		} else {
			init_gendisk(hwif);
			blk_dev[hwif->major].request_fn = rfn;
			read_ahead[hwif->major] = 8;	/* (4kB) */
			hwif->present = 1;	/* success */
		}
	}

#ifdef CONFIG_BLK_DEV_IDETAPE
	idetape_register_chrdev();	/* Register character device interface to the ide tape */
#endif /* CONFIG_BLK_DEV_IDETAPE */
	
	return 0;
}
