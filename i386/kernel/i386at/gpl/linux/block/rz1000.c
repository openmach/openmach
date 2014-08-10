/*
 *  linux/drivers/block/rz1000.c	Version 0.02  Feb 08, 1996
 *
 *  Copyright (C) 1995-1996  Linus Torvalds & author (see below)
 */

/*
 *  Principal Author/Maintainer:  mlord@bnr.ca (Mark Lord)
 *
 *  This file provides support for disabling the buggy read-ahead
 *  mode of the RZ1000 IDE chipset, commonly used on Intel motherboards.
 */

#undef REALLY_SLOW_IO		/* most systems can safely undef this */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <asm/io.h>
#include <linux/bios32.h>
#include <linux/pci.h>
#include "ide.h"

static void ide_pci_access_error (int rc)
{
	printk("ide: pcibios access failed - %s\n", pcibios_strerror(rc));
}

void init_rz1000 (byte bus, byte fn)
{
	int rc;
	unsigned short reg;

	printk("ide: buggy RZ1000 interface: ");
	if ((rc = pcibios_read_config_word (bus, fn, PCI_COMMAND, &reg))) {
		ide_pci_access_error (rc);
	} else if (!(reg & 1)) {
		printk("not enabled\n");
	} else {
		if ((rc = pcibios_read_config_word(bus, fn, 0x40, &reg))
		 || (rc =  pcibios_write_config_word(bus, fn, 0x40, reg & 0xdfff)))
		{
			ide_hwifs[0].no_unmask = 1;
			ide_hwifs[1].no_unmask = 1;
			ide_hwifs[0].serialized = 1;
			ide_pci_access_error (rc);
			printk("serialized, disabled unmasking\n");
		} else
			printk("disabled read-ahead\n");
	}
}
