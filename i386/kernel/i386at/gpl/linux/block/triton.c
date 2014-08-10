/*
 *  linux/drivers/block/triton.c	Version 1.06  Feb 6, 1996
 *
 *  Copyright (c) 1995-1996  Mark Lord
 *  May be copied or modified under the terms of the GNU General Public License
 */

/*
 * This module provides support for the Bus Master IDE DMA function
 * of the Intel PCI Triton chipset (82371FB).
 *
 * DMA is currently supported only for hard disk drives (not cdroms).
 *
 * Support for cdroms will likely be added at a later date,
 * after broader experience has been obtained with hard disks.
 *
 * Up to four drives may be enabled for DMA, and the Triton chipset will
 * (hopefully) arbitrate the PCI bus among them.  Note that the 82371FB chip
 * provides a single "line buffer" for the BM IDE function, so performance of
 * multiple (two) drives doing DMA simultaneously will suffer somewhat,
 * as they contest for that resource bottleneck.  This is handled transparently
 * inside the 82371FB chip.
 *
 * By default, DMA support is prepared for use, but is currently enabled only
 * for drives which support multi-word DMA mode2 (mword2), or which are
 * recognized as "good" (see table below).  Drives with only mode0 or mode1
 * (single or multi) DMA should also work with this chipset/driver (eg. MC2112A)
 * but are not enabled by default.  Use "hdparm -i" to view modes supported
 * by a given drive.
 *
 * The hdparm-2.4 (or later) utility can be used for manually enabling/disabling
 * DMA support, but must be (re-)compiled against this kernel version or later.
 *
 * To enable DMA, use "hdparm -d1 /dev/hd?" on a per-drive basis after booting.
 * If problems arise, ide.c will disable DMA operation after a few retries.
 * This error recovery mechanism works and has been extremely well exercised.
 *
 * IDE drives, depending on their vintage, may support several different modes
 * of DMA operation.  The boot-time modes are indicated with a "*" in
 * the "hdparm -i" listing, and can be changed with *knowledgeable* use of
 * the "hdparm -X" feature.  There is seldom a need to do this, as drives
 * normally power-up with their "best" PIO/DMA modes enabled.
 *
 * Testing was done with an ASUS P55TP4XE/100 system and the following drives:
 *
 *   Quantum Fireball 1080A (1Gig w/83kB buffer), DMA mode2, PIO mode4.
 *	- DMA mode2 works well (7.4MB/sec), despite the tiny on-drive buffer.
 *	- This drive also does PIO mode4, at about the same speed as DMA mode2.
 *	  An awesome drive for the price!
 *
 *   Fujitsu M1606TA (1Gig w/256kB buffer), DMA mode2, PIO mode4.
 *	- DMA mode2 gives horrible performance (1.6MB/sec), despite the good
 *	  size of the on-drive buffer and a boasted 10ms average access time.
 *	- PIO mode4 was better, but peaked at a mere 4.5MB/sec.
 *
 *   Micropolis MC2112A (1Gig w/508kB buffer), drive pre-dates EIDE and ATA2.
 *	- DMA works fine (2.2MB/sec), probably due to the large on-drive buffer.
 *	- This older drive can also be tweaked for fastPIO (3.7MB/sec) by using
 *	  maximum clock settings (5,4) and setting all flags except prefetch.
 *
 *   Western Digital AC31000H (1Gig w/128kB buffer), DMA mode1, PIO mode3.
 *	- DMA does not work reliably.  The drive appears to be somewhat tardy
 *	  in deasserting DMARQ at the end of a sector.  This is evident in
 *	  the observation that WRITEs work most of the time, depending on
 *	  cache-buffer occupancy, but multi-sector reads seldom work.
 *
 * Testing was done with a Gigabyte GA-586 ATE system and the following drive:
 * (Uwe Bonnes - bon@elektron.ikp.physik.th-darmstadt.de)
 *
 *   Western Digital AC31600H (1.6Gig w/128kB buffer), DMA mode2, PIO mode4.
 *	- much better than its 1Gig cousin, this drive is reported to work
 *	  very well with DMA (7.3MB/sec).
 *
 * Other drives:
 *
 *   Maxtor 7540AV (515Meg w/32kB buffer), DMA modes mword0/sword2, PIO mode3.
 *	- a budget drive, with budget performance, around 3MB/sec.
 *
 *   Western Digital AC2850F (814Meg w/64kB buffer), DMA mode1, PIO mode3.
 *	- another "caviar" drive, similar to the AC31000, except that this one
 *	  worked with DMA in at least one system.  Throughput is about 3.8MB/sec
 *	  for both DMA and PIO.
 *
 *   Conner CFS850A (812Meg w/64kB buffer), DMA mode2, PIO mode4.
 *	- like most Conner models, this drive proves that even a fast interface
 *	  cannot improve slow media.  Both DMA and PIO peak around 3.5MB/sec.
 *
 * If you have any drive models to add, email your results to:  mlord@bnr.ca
 * Keep an eye on /var/adm/messages for "DMA disabled" messages.
 *
 * Some people have reported trouble with Intel Zappa motherboards.
 * This can be fixed by upgrading the AMI BIOS to version 1.00.04.BS0,
 * available from ftp://ftp.intel.com/pub/bios/10004bs0.exe
 * (thanks to Glen Morrell <glen@spin.Stanford.edu> for researching this).
 *
 * And, yes, Intel Zappa boards really *do* use the Triton IDE ports.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/pci.h>
#include <linux/bios32.h>

#include <asm/io.h>
#include <asm/dma.h>

#include "ide.h"

/*
 * good_dma_drives() lists the model names (from "hdparm -i")
 * of drives which do not support mword2 DMA but which are
 * known to work fine with this interface under Linux.
 */
const char *good_dma_drives[] = {"Micropolis 2112A",
				 "CONNER CTMA 4000"};

/*
 * Our Physical Region Descriptor (PRD) table should be large enough
 * to handle the biggest I/O request we are likely to see.  Since requests
 * can have no more than 256 sectors, and since the typical blocksize is
 * two sectors, we could get by with a limit of 128 entries here for the
 * usual worst case.  Most requests seem to include some contiguous blocks,
 * further reducing the number of table entries required.
 *
 * The driver reverts to PIO mode for individual requests that exceed
 * this limit (possible with 512 byte blocksizes, eg. MSDOS f/s), so handling
 * 100% of all crazy scenarios here is not necessary.
 *
 * As it turns out though, we must allocate a full 4KB page for this,
 * so the two PRD tables (ide0 & ide1) will each get half of that,
 * allowing each to have about 256 entries (8 bytes each) from this.
 */
#define PRD_BYTES	8
#define PRD_ENTRIES	(PAGE_SIZE / (2 * PRD_BYTES))

/*
 * dma_intr() is the handler for disk read/write DMA interrupts
 */
static void dma_intr (ide_drive_t *drive)
{
	byte stat, dma_stat;
	int i;
	struct request *rq = HWGROUP(drive)->rq;
	unsigned short dma_base = HWIF(drive)->dma_base;

	dma_stat = inb(dma_base+2);		/* get DMA status */
	outb(inb(dma_base)&~1, dma_base);	/* stop DMA operation */
	stat = GET_STAT();			/* get drive status */
	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		if ((dma_stat & 7) == 4) {	/* verify good DMA status */
			rq = HWGROUP(drive)->rq;
			for (i = rq->nr_sectors; i > 0;) {
				i -= rq->current_nr_sectors;
				ide_end_request(1, HWGROUP(drive));
			}
			return;
		}
		printk("%s: bad DMA status: 0x%02x\n", drive->name, dma_stat);
	}
	sti();
	ide_error(drive, "dma_intr", stat);
}

/*
 * build_dmatable() prepares a dma request.
 * Returns 0 if all went okay, returns 1 otherwise.
 */
static int build_dmatable (ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	struct buffer_head *bh = rq->bh;
	unsigned long size, addr, *table = HWIF(drive)->dmatable;
	unsigned int count = 0;

	do {
		/*
		 * Determine addr and size of next buffer area.  We assume that
		 * individual virtual buffers are always composed linearly in
		 * physical memory.  For example, we assume that any 8kB buffer
		 * is always composed of two adjacent physical 4kB pages rather
		 * than two possibly non-adjacent physical 4kB pages.
		 */
		if (bh == NULL) {  /* paging and tape requests have (rq->bh == NULL) */
			addr = virt_to_bus (rq->buffer);
#ifdef CONFIG_BLK_DEV_IDETAPE
			if (drive->media == ide_tape)
				size = drive->tape.pc->request_transfer;
			else
#endif /* CONFIG_BLK_DEV_IDETAPE */	
			size = rq->nr_sectors << 9;
		} else {
			/* group sequential buffers into one large buffer */
			addr = virt_to_bus (bh->b_data);
			size = bh->b_size;
			while ((bh = bh->b_reqnext) != NULL) {
				if ((addr + size) != virt_to_bus (bh->b_data))
					break;
				size += bh->b_size;
			}
		}

		/*
		 * Fill in the dma table, without crossing any 64kB boundaries.
		 * We assume 16-bit alignment of all blocks.
		 */
		while (size) {
			if (++count >= PRD_ENTRIES) {
				printk("%s: DMA table too small\n", drive->name);
				return 1; /* revert to PIO for this request */
			} else {
				unsigned long bcount = 0x10000 - (addr & 0xffff);
				if (bcount > size)
					bcount = size;
				*table++ = addr;
				*table++ = bcount;
				addr += bcount;
				size -= bcount;
			}
		}
	} while (bh != NULL);
	if (count) {
		*--table |= 0x80000000;	/* set End-Of-Table (EOT) bit */
		return 0;
	}
	printk("%s: empty DMA table?\n", drive->name);
	return 1;	/* let the PIO routines handle this weirdness */
}

static int config_drive_for_dma (ide_drive_t *drive)
{
	const char **list;

	struct hd_driveid *id = drive->id;
	if (id && (id->capability & 1)) {
		/* Enable DMA on any drive that supports mword2 DMA */
		if ((id->field_valid & 2) && (id->dma_mword & 0x404) == 0x404) {
			drive->using_dma = 1;
			return 0;		/* DMA enabled */
		}
		/* Consult the list of known "good" drives */
		list = good_dma_drives;
		while (*list) {
			if (!strcmp(*list++,id->model)) {
				drive->using_dma = 1;
				return 0;	/* DMA enabled */
			}
		}
	}
	return 1;	/* DMA not enabled */
}

/*
 * triton_dmaproc() initiates/aborts DMA read/write operations on a drive.
 *
 * The caller is assumed to have selected the drive and programmed the drive's
 * sector address using CHS or LBA.  All that remains is to prepare for DMA
 * and then issue the actual read/write DMA/PIO command to the drive.
 *
 * For ATAPI devices, we just prepare for DMA and return. The caller should
 * then issue the packet command to the drive and call us again with
 * ide_dma_begin afterwards.
 *
 * Returns 0 if all went well.
 * Returns 1 if DMA read/write could not be started, in which case
 * the caller should revert to PIO for the current request.
 */
static int triton_dmaproc (ide_dma_action_t func, ide_drive_t *drive)
{
	unsigned long dma_base = HWIF(drive)->dma_base;
	unsigned int reading = (1 << 3);

	switch (func) {
		case ide_dma_abort:
			outb(inb(dma_base)&~1, dma_base);	/* stop DMA */
			return 0;
		case ide_dma_check:
			return config_drive_for_dma (drive);
		case ide_dma_write:
			reading = 0;
		case ide_dma_read:
			break;
		case ide_dma_status_bad:
			return ((inb(dma_base+2) & 7) != 4);	/* verify good DMA status */
		case ide_dma_transferred:
#if 0
			return (number of bytes actually transferred);
#else
			return (0);
#endif
		case ide_dma_begin:
			outb(inb(dma_base)|1, dma_base);	/* begin DMA */
			return 0;
		default:
			printk("triton_dmaproc: unsupported func: %d\n", func);
			return 1;
	}
	if (build_dmatable (drive))
		return 1;
	outl(virt_to_bus (HWIF(drive)->dmatable), dma_base + 4); /* PRD table */
	outb(reading, dma_base);			/* specify r/w */
	outb(0x26, dma_base+2);				/* clear status bits */
#ifdef CONFIG_BLK_DEV_IDEATAPI
	if (drive->media != ide_disk)
		return 0;
#endif /* CONFIG_BLK_DEV_IDEATAPI */	
	ide_set_handler(drive, &dma_intr, WAIT_CMD);	/* issue cmd to drive */
	OUT_BYTE(reading ? WIN_READDMA : WIN_WRITEDMA, IDE_COMMAND_REG);
	outb(inb(dma_base)|1, dma_base);		/* begin DMA */
	return 0;
}

/*
 * print_triton_drive_flags() displays the currently programmed options
 * in the Triton chipset for a given drive.
 *
 *	If fastDMA  is "no", then slow ISA timings are used for DMA data xfers.
 *	If fastPIO  is "no", then slow ISA timings are used for PIO data xfers.
 *	If IORDY    is "no", then IORDY is assumed to always be asserted.
 *	If PreFetch is "no", then data pre-fetch/post are not used.
 *
 * When "fastPIO" and/or "fastDMA" are "yes", then faster PCI timings and
 * back-to-back 16-bit data transfers are enabled, using the sample_CLKs
 * and recovery_CLKs (PCI clock cycles) timing parameters for that interface.
 */
static void print_triton_drive_flags (unsigned int unit, byte flags)
{
	printk("         %s ", unit ? "slave :" : "master:");
	printk( "fastDMA=%s",	(flags&9)	? "on " : "off");
	printk(" PreFetch=%s",	(flags&4)	? "on " : "off");
	printk(" IORDY=%s",	(flags&2)	? "on " : "off");
	printk(" fastPIO=%s\n",	((flags&9)==1)	? "on " : "off");
}

static void init_triton_dma (ide_hwif_t *hwif, unsigned short base)
{
	static unsigned long dmatable = 0;

	printk("    %s: BusMaster DMA at 0x%04x-0x%04x", hwif->name, base, base+7);
	if (check_region(base, 8)) {
		printk(" -- ERROR, PORTS ALREADY IN USE");
	} else {
		request_region(base, 8, "triton DMA");
		hwif->dma_base = base;
		if (!dmatable) {
			/*
			 * Since we know we are on a PCI bus, we could
			 * actually use __get_free_pages() here instead
			 * of __get_dma_pages() -- no ISA limitations.
			 */
			dmatable = __get_dma_pages(GFP_KERNEL, 0);
		}
		if (dmatable) {
			hwif->dmatable = (unsigned long *) dmatable;
			dmatable += (PRD_ENTRIES * PRD_BYTES);
			outl(virt_to_bus(hwif->dmatable), base + 4);
			hwif->dmaproc  = &triton_dmaproc;
		}
	}
	printk("\n");
}

/*
 * calc_mode() returns the ATA PIO mode number, based on the number
 * of cycle clks passed in.  Assumes 33Mhz bus operation (30ns per clk).
 */
byte calc_mode (byte clks)
{
	if (clks == 3)	return 5;
	if (clks == 4)	return 4;
	if (clks <  6)	return 3;
	if (clks <  8)	return 2;
	if (clks < 13)	return 1;
	return 0;
}

/*
 * ide_init_triton() prepares the IDE driver for DMA operation.
 * This routine is called once, from ide.c during driver initialization,
 * for each triton chipset which is found (unlikely to be more than one).
 */
void ide_init_triton (byte bus, byte fn)
{
	int rc = 0, h;
	int dma_enabled = 0;
	unsigned short bmiba, pcicmd;
	unsigned int timings;

	printk("ide: Triton BM-IDE on PCI bus %d function %d\n", bus, fn);
	/*
	 * See if IDE and BM-DMA features are enabled:
	 */
	if ((rc = pcibios_read_config_word(bus, fn, 0x04, &pcicmd)))
		goto quit;
	if ((pcicmd & 1) == 0)  {
		printk("ide: Triton IDE ports are not enabled\n");
		goto quit;
	}
	if ((pcicmd & 4) == 0) {
		printk("ide: Triton BM-DMA feature is not enabled -- upgrade your BIOS\n");
	} else {
		/*
		 * Get the bmiba base address
		 */
		if ((rc = pcibios_read_config_word(bus, fn, 0x20, &bmiba)))
			goto quit;
		bmiba &= 0xfff0;	/* extract port base address */
		dma_enabled = 1;
	}

	/*
	 * See if ide port(s) are enabled
	 */
	if ((rc = pcibios_read_config_dword(bus, fn, 0x40, &timings)))
		goto quit;
	if (!(timings & 0x80008000)) {
		printk("ide: neither Triton IDE port is enabled\n");
		goto quit;
	}

	/*
	 * Save the dma_base port addr for each interface
	 */
	for (h = 0; h < MAX_HWIFS; ++h) {
		byte s_clks, r_clks;
		ide_hwif_t *hwif = &ide_hwifs[h];
		unsigned short time;
		if (hwif->io_base == 0x1f0) {
			time = timings & 0xffff;
			if ((timings & 0x8000) == 0)	/* interface enabled? */
				continue;
			hwif->chipset = ide_triton;
			if (dma_enabled)
				init_triton_dma(hwif, bmiba);
		} else if (hwif->io_base == 0x170) {
			time = timings >> 16;
			if ((timings & 0x8000) == 0)	/* interface enabled? */
				continue;
			hwif->chipset = ide_triton;
			if (dma_enabled)
				init_triton_dma(hwif, bmiba + 8);
		} else
			continue;
		s_clks = ((~time >> 12) & 3) + 2;
		r_clks = ((~time >>  8) & 3) + 1;
		printk("    %s timing: (0x%04x) sample_CLKs=%d, recovery_CLKs=%d (PIO mode%d)\n",
		 hwif->name, time, s_clks, r_clks, calc_mode(s_clks+r_clks));
		print_triton_drive_flags (0, time & 0xf);
		print_triton_drive_flags (1, (time >> 4) & 0xf);
	}

quit: if (rc) printk("ide: pcibios access failed - %s\n", pcibios_strerror(rc));
}

