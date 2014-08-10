/* 3c503.c: A shared-memory NS8390 ethernet driver for linux. */
/*
    Written 1992-94 by Donald Becker.

    Copyright 1993 United States Government as represented by the
    Director, National Security Agency.  This software may be used and
    distributed according to the terms of the GNU Public License,
    incorporated herein by reference.

    The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
    Center of Excellence in Space Data and Information Sciences
       Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

    This driver should work with the 3c503 and 3c503/16.  It should be used
    in shared memory mode for best performance, although it may also work
    in programmed-I/O mode.

    Sources:
    EtherLink II Technical Reference Manual,
    EtherLink II/16 Technical Reference Manual Supplement,
    3Com Corporation, 5400 Bayfront Plaza, Santa Clara CA 95052-8145
    
    The Crynwr 3c503 packet driver.

    Changelog:

    Paul Gortmaker	: add support for the 2nd 8kB of RAM on 16 bit cards.
    Paul Gortmaker	: multiple card support for module users.

*/

static const char *version =
    "3c503.c:v1.10 9/23/93  Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <asm/io.h>
#include <asm/system.h>

#include "8390.h"
#include "3c503.h"


int el2_probe(struct device *dev);
int el2_pio_probe(struct device *dev);
int el2_probe1(struct device *dev, int ioaddr);

/* A zero-terminated list of I/O addresses to be probed in PIO mode. */
static unsigned int netcard_portlist[] =
	{ 0x300,0x310,0x330,0x350,0x250,0x280,0x2a0,0x2e0,0};

#define EL2_IO_EXTENT	16

#ifdef HAVE_DEVLIST
/* The 3c503 uses two entries, one for the safe memory-mapped probe and
   the other for the typical I/O probe. */
struct netdev_entry el2_drv =
{"3c503", el2_probe, EL1_IO_EXTENT, 0};
struct netdev_entry el2pio_drv =
{"3c503pio", el2_pioprobe1, EL1_IO_EXTENT, netcard_portlist};
#endif

static int el2_open(struct device *dev);
static int el2_close(struct device *dev);
static void el2_reset_8390(struct device *dev);
static void el2_init_card(struct device *dev);
static void el2_block_output(struct device *dev, int count,
			     const unsigned char *buf, const start_page);
static void el2_block_input(struct device *dev, int count, struct sk_buff *skb,
			   int ring_offset);
static void el2_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr,
			 int ring_page);


/* This routine probes for a memory-mapped 3c503 board by looking for
   the "location register" at the end of the jumpered boot PROM space.
   This works even if a PROM isn't there.

   If the ethercard isn't found there is an optional probe for
   ethercard jumpered to programmed-I/O mode.
   */
int
el2_probe(struct device *dev)
{
    int *addr, addrs[] = { 0xddffe, 0xd9ffe, 0xcdffe, 0xc9ffe, 0};
    int base_addr = dev->base_addr;

    if (base_addr > 0x1ff)	/* Check a single specified location. */
	return el2_probe1(dev, base_addr);
    else if (base_addr != 0)		/* Don't probe at all. */
	return ENXIO;

    for (addr = addrs; *addr; addr++) {
	int i;
	unsigned int base_bits = readb(*addr);
	/* Find first set bit. */
	for(i = 7; i >= 0; i--, base_bits >>= 1)
	    if (base_bits & 0x1)
		break;
	if (base_bits != 1)
	    continue;
	if (check_region(netcard_portlist[i], EL2_IO_EXTENT))
	    continue;
	if (el2_probe1(dev, netcard_portlist[i]) == 0)
	    return 0;
    }
#if ! defined(no_probe_nonshared_memory) && ! defined (HAVE_DEVLIST)
    return el2_pio_probe(dev);
#else
    return ENODEV;
#endif
}

#ifndef HAVE_DEVLIST
/*  Try all of the locations that aren't obviously empty.  This touches
    a lot of locations, and is much riskier than the code above. */
int
el2_pio_probe(struct device *dev)
{
    int i;
    int base_addr = dev ? dev->base_addr : 0;

    if (base_addr > 0x1ff)	/* Check a single specified location. */
	return el2_probe1(dev, base_addr);
    else if (base_addr != 0)	/* Don't probe at all. */
	return ENXIO;

    for (i = 0; netcard_portlist[i]; i++) {
	int ioaddr = netcard_portlist[i];
	if (check_region(ioaddr, EL2_IO_EXTENT))
	    continue;
	if (el2_probe1(dev, ioaddr) == 0)
	    return 0;
    }

    return ENODEV;
}
#endif

/* Probe for the Etherlink II card at I/O port base IOADDR,
   returning non-zero on success.  If found, set the station
   address and memory parameters in DEVICE. */
int
el2_probe1(struct device *dev, int ioaddr)
{
    int i, iobase_reg, membase_reg, saved_406, wordlength;
    static unsigned version_printed = 0;
    unsigned long vendor_id;

    /* Reset and/or avoid any lurking NE2000 */
    if (inb(ioaddr + 0x408) == 0xff) {
    	udelay(1000);
	return ENODEV;
    }

    /* We verify that it's a 3C503 board by checking the first three octets
       of its ethernet address. */
    iobase_reg = inb(ioaddr+0x403);
    membase_reg = inb(ioaddr+0x404);
    /* ASIC location registers should be 0 or have only a single bit set. */
    if (   (iobase_reg  & (iobase_reg - 1))
	|| (membase_reg & (membase_reg - 1))) {
	return ENODEV;
    }
    saved_406 = inb_p(ioaddr + 0x406);
    outb_p(ECNTRL_RESET|ECNTRL_THIN, ioaddr + 0x406); /* Reset it... */
    outb_p(ECNTRL_THIN, ioaddr + 0x406);
    /* Map the station addr PROM into the lower I/O ports. We now check
       for both the old and new 3Com prefix */
    outb(ECNTRL_SAPROM|ECNTRL_THIN, ioaddr + 0x406);
    vendor_id = inb(ioaddr)*0x10000 + inb(ioaddr + 1)*0x100 + inb(ioaddr + 2);
    if ((vendor_id != OLD_3COM_ID) && (vendor_id != NEW_3COM_ID)) {
	/* Restore the register we frobbed. */
	outb(saved_406, ioaddr + 0x406);
	return ENODEV;
    }

    /* We should have a "dev" from Space.c or the static module table. */
    if (dev == NULL) {
	printk("3c503.c: Passed a NULL device.\n");
	dev = init_etherdev(0, 0);
    }

    if (ei_debug  &&  version_printed++ == 0)
	printk(version);

    dev->base_addr = ioaddr;

    /* Allocate dev->priv and fill in 8390 specific dev fields. */
    if (ethdev_init(dev)) {
	printk ("3c503: unable to allocate memory for dev->priv.\n");
	return -ENOMEM;
     }

    printk("%s: 3c503 at i/o base %#3x, node ", dev->name, ioaddr);

    /* Retrieve and print the ethernet address. */
    for (i = 0; i < 6; i++)
	printk(" %2.2x", dev->dev_addr[i] = inb(ioaddr + i));

    /* Map the 8390 back into the window. */
    outb(ECNTRL_THIN, ioaddr + 0x406);

    /* Check for EL2/16 as described in tech. man. */
    outb_p(E8390_PAGE0, ioaddr + E8390_CMD);
    outb_p(0, ioaddr + EN0_DCFG);
    outb_p(E8390_PAGE2, ioaddr + E8390_CMD);
    wordlength = inb_p(ioaddr + EN0_DCFG) & ENDCFG_WTS;
    outb_p(E8390_PAGE0, ioaddr + E8390_CMD);

    /* Probe for, turn on and clear the board's shared memory. */
    if (ei_debug > 2) printk(" memory jumpers %2.2x ", membase_reg);
    outb(EGACFR_NORM, ioaddr + 0x405);	/* Enable RAM */

    /* This should be probed for (or set via an ioctl()) at run-time.
       Right now we use a sleazy hack to pass in the interface number
       at boot-time via the low bits of the mem_end field.  That value is
       unused, and the low bits would be discarded even if it was used. */
#if defined(EI8390_THICK) || defined(EL2_AUI)
    ei_status.interface_num = 1;
#else
    ei_status.interface_num = dev->mem_end & 0xf;
#endif
    printk(", using %sternal xcvr.\n", ei_status.interface_num == 0 ? "in" : "ex");

    if ((membase_reg & 0xf0) == 0) {
	dev->mem_start = 0;
	ei_status.name = "3c503-PIO";
    } else {
	dev->mem_start = ((membase_reg & 0xc0) ? 0xD8000 : 0xC8000) +
	    ((membase_reg & 0xA0) ? 0x4000 : 0);

#define EL2_MEMSIZE (EL2_MB1_STOP_PG - EL2_MB1_START_PG)*256
#ifdef EL2MEMTEST
	/* This has never found an error, but someone might care.
	   Note that it only tests the 2nd 8kB on 16kB 3c503/16
	   cards between card addr. 0x2000 and 0x3fff. */
	{			/* Check the card's memory. */
	    unsigned long mem_base = dev->mem_start;
	    unsigned int test_val = 0xbbadf00d;
	    writel(0xba5eba5e, mem_base);
	    for (i = sizeof(test_val); i < EL2_MEMSIZE; i+=sizeof(test_val)) {
		writel(test_val, mem_base + i);
		if (readl(mem_base) != 0xba5eba5e
		    || readl(mem_base + i) != test_val) {
		    printk("3c503.c: memory failure or memory address conflict.\n");
		    dev->mem_start = 0;
		    ei_status.name = "3c503-PIO";
		    break;
		}
		test_val += 0x55555555;
		writel(0, mem_base + i);
	    }
	}
#endif  /* EL2MEMTEST */

	dev->mem_end = dev->rmem_end = dev->mem_start + EL2_MEMSIZE;

	if (wordlength) {	/* No Tx pages to skip over to get to Rx */
		dev->rmem_start = dev->mem_start;
		ei_status.name = "3c503/16";
	} else {
		dev->rmem_start = TX_PAGES*256 + dev->mem_start;
		ei_status.name = "3c503";
	}
    }

    /*
	Divide up the memory on the card. This is the same regardless of
	whether shared-mem or PIO is used. For 16 bit cards (16kB RAM),
	we use the entire 8k of bank1 for an Rx ring. We only use 3k 
	of the bank0 for 2 full size Tx packet slots. For 8 bit cards,
	(8kB RAM) we use 3kB of bank1 for two Tx slots, and the remaining 
	5kB for an Rx ring.  */

    if (wordlength) {
	ei_status.tx_start_page = EL2_MB0_START_PG;
	ei_status.rx_start_page = EL2_MB1_START_PG;
    } else {
	ei_status.tx_start_page = EL2_MB1_START_PG;
	ei_status.rx_start_page = EL2_MB1_START_PG + TX_PAGES;
    }

    /* Finish setting the board's parameters. */
    ei_status.stop_page = EL2_MB1_STOP_PG;
    ei_status.word16 = wordlength;
    ei_status.reset_8390 = &el2_reset_8390;
    ei_status.get_8390_hdr = &el2_get_8390_hdr;
    ei_status.block_input = &el2_block_input;
    ei_status.block_output = &el2_block_output;

    request_region(ioaddr, EL2_IO_EXTENT, ei_status.name);

    if (dev->irq == 2)
	dev->irq = 9;
    else if (dev->irq > 5 && dev->irq != 9) {
	printk("3c503: configured interrupt %d invalid, will use autoIRQ.\n",
	       dev->irq);
	dev->irq = 0;
    }

    ei_status.saved_irq = dev->irq;

    dev->start = 0;
    dev->open = &el2_open;
    dev->stop = &el2_close;

    if (dev->mem_start)
	printk("%s: %s - %dkB RAM, 8kB shared mem window at %#6lx-%#6lx.\n",
		dev->name, ei_status.name, (wordlength+1)<<3,
		dev->mem_start, dev->mem_end-1);

    else
	printk("\n%s: %s, %dkB RAM, using programmed I/O (REJUMPER for SHARED MEMORY).\n",
	       dev->name, ei_status.name, (wordlength+1)<<3);

    return 0;
}

static int
el2_open(struct device *dev)
{

    if (dev->irq < 2) {
	int irqlist[] = {5, 9, 3, 4, 0};
	int *irqp = irqlist;

	outb(EGACFR_NORM, E33G_GACFR);	/* Enable RAM and interrupts. */
	do {
	    if (request_irq (*irqp, NULL, 0, "bogus") != -EBUSY) {
		/* Twinkle the interrupt, and check if it's seen. */
		autoirq_setup(0);
		outb_p(0x04 << ((*irqp == 9) ? 2 : *irqp), E33G_IDCFR);
		outb_p(0x00, E33G_IDCFR);
		if (*irqp == autoirq_report(0)	 /* It's a good IRQ line! */
		    && request_irq (dev->irq = *irqp, &ei_interrupt, 0, ei_status.name) == 0)
		    break;
	    }
	} while (*++irqp);
	if (*irqp == 0) {
	    outb(EGACFR_IRQOFF, E33G_GACFR);	/* disable interrupts. */
	    return -EAGAIN;
	}
    } else {
	if (request_irq(dev->irq, &ei_interrupt, 0, ei_status.name)) {
	    return -EAGAIN;
	}
    }

    el2_init_card(dev);
    ei_open(dev);
    MOD_INC_USE_COUNT;
    return 0;
}

static int
el2_close(struct device *dev)
{
    free_irq(dev->irq);
    dev->irq = ei_status.saved_irq;
    irq2dev_map[dev->irq] = NULL;
    outb(EGACFR_IRQOFF, E33G_GACFR);	/* disable interrupts. */

    ei_close(dev);
    MOD_DEC_USE_COUNT;
    return 0;
}

/* This is called whenever we have a unrecoverable failure:
       transmit timeout
       Bad ring buffer packet header
 */
static void
el2_reset_8390(struct device *dev)
{
    if (ei_debug > 1) {
	printk("%s: Resetting the 3c503 board...", dev->name);
	printk("%#lx=%#02x %#lx=%#02x %#lx=%#02x...", E33G_IDCFR, inb(E33G_IDCFR),
	       E33G_CNTRL, inb(E33G_CNTRL), E33G_GACFR, inb(E33G_GACFR));
    }
    outb_p(ECNTRL_RESET|ECNTRL_THIN, E33G_CNTRL);
    ei_status.txing = 0;
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
    el2_init_card(dev);
    if (ei_debug > 1) printk("done\n");
}

/* Initialize the 3c503 GA registers after a reset. */
static void
el2_init_card(struct device *dev)
{
    /* Unmap the station PROM and select the DIX or BNC connector. */
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);

    /* Set ASIC copy of rx's first and last+1 buffer pages */
    /* These must be the same as in the 8390. */
    outb(ei_status.rx_start_page, E33G_STARTPG);
    outb(ei_status.stop_page,  E33G_STOPPG);

    /* Point the vector pointer registers somewhere ?harmless?. */
    outb(0xff, E33G_VP2);	/* Point at the ROM restart location 0xffff0 */
    outb(0xff, E33G_VP1);
    outb(0x00, E33G_VP0);
    /* Turn off all interrupts until we're opened. */
    outb_p(0x00,  dev->base_addr + EN0_IMR);
    /* Enable IRQs iff started. */
    outb(EGACFR_NORM, E33G_GACFR);

    /* Set the interrupt line. */
    outb_p((0x04 << (dev->irq == 9 ? 2 : dev->irq)), E33G_IDCFR);
    outb_p(8, E33G_DRQCNT);		/* Set burst size to 8 */
    outb_p(0x20, E33G_DMAAH);	/* Put a valid addr in the GA DMA */
    outb_p(0x00, E33G_DMAAL);
    return;			/* We always succeed */
}

/* Either use the shared memory (if enabled on the board) or put the packet
   out through the ASIC FIFO.  The latter is probably much slower. */
static void
el2_block_output(struct device *dev, int count,
		 const unsigned char *buf, const start_page)
{
    int i;				/* Buffer index */
    int boguscount = 0;		/* timeout counter */

    if (ei_status.word16)      /* Tx packets go into bank 0 on EL2/16 card */
	outb(EGACFR_RSEL|EGACFR_TCM, E33G_GACFR);
    else 
	outb(EGACFR_NORM, E33G_GACFR);

    if (dev->mem_start) {	/* Shared memory transfer */
	unsigned long dest_addr = dev->mem_start +
	    ((start_page - ei_status.tx_start_page) << 8);
	memcpy_toio(dest_addr, buf, count);
	outb(EGACFR_NORM, E33G_GACFR);	/* Back to bank1 in case on bank0 */
	return;
    }
    /* No shared memory, put the packet out the slow way. */
    /* Set up then start the internal memory transfer to Tx Start Page */
    outb(0x00, E33G_DMAAL);
    outb_p(start_page, E33G_DMAAH);
    outb_p((ei_status.interface_num ? ECNTRL_AUI : ECNTRL_THIN ) | ECNTRL_OUTPUT
	   | ECNTRL_START, E33G_CNTRL);

    /* This is the byte copy loop: it should probably be tuned for
       speed once everything is working.  I think it is possible
       to output 8 bytes between each check of the status bit. */
    for(i = 0; i < count; i++) {
	if (i % 8 == 0)
	    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
		if (++boguscount > (i<<3) + 32) {
		    printk("%s: FIFO blocked in el2_block_output (at %d of %d, bc=%d).\n",
			   dev->name, i, count, boguscount);
		    outb(EGACFR_NORM, E33G_GACFR);	/* To MB1 for EL2/16 */
		    return;
		}
	outb(buf[i], E33G_FIFOH);
    }
    outb_p(ei_status.interface_num==0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
    outb(EGACFR_NORM, E33G_GACFR);	/* Back to bank1 in case on bank0 */
    return;
}

/* Read the 4 byte, page aligned 8390 specific header. */
static void
el2_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
    unsigned int i;
    unsigned long hdr_start = dev->mem_start + ((ring_page - EL2_MB1_START_PG)<<8);
    unsigned long fifo_watchdog;

    if (dev->mem_start) {       /* Use the shared memory. */
#ifdef notdef
	/* Officially this is what we are doing, but the readl() is faster */
	memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
#else
	((unsigned int*)hdr)[0] = readl(hdr_start);
#endif
	return;
    }

    /* No shared memory, use programmed I/O. Ugh. */
    outb(0, E33G_DMAAL);
    outb_p(ring_page & 0xff, E33G_DMAAH);
    outb_p((ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI) | ECNTRL_INPUT
	   | ECNTRL_START, E33G_CNTRL);

    /* Header is < 8 bytes, so only check the FIFO at the beginning. */
    fifo_watchdog = jiffies;
    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0) {
	if (jiffies - fifo_watchdog > 2*HZ/100) {
		printk("%s: FIFO blocked in el2_get_8390_hdr.\n", dev->name);
		break;
	}
    }

    for(i = 0; i < sizeof(struct e8390_pkt_hdr); i++)
	((char *)(hdr))[i] = inb_p(E33G_FIFOH);

    outb_p(ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
}

/* Returns the new ring pointer. */
static void
el2_block_input(struct device *dev, int count, struct sk_buff *skb, int ring_offset)
{
    int boguscount = 0;
    int end_of_ring = dev->rmem_end;
    unsigned int i;

    /* Maybe enable shared memory just be to be safe... nahh.*/
    if (dev->mem_start) {	/* Use the shared memory. */
	ring_offset -= (EL2_MB1_START_PG<<8);
	if (dev->mem_start + ring_offset + count > end_of_ring) {
	    /* We must wrap the input move. */
	    int semi_count = end_of_ring - (dev->mem_start + ring_offset);
	    memcpy_fromio(skb->data, dev->mem_start + ring_offset, semi_count);
	    count -= semi_count;
	    memcpy_fromio(skb->data + semi_count, dev->rmem_start, count);
	} else {
		/* Packet is in one chunk -- we can copy + cksum. */
		eth_io_copy_and_sum(skb, dev->mem_start + ring_offset, count, 0);
	}
	return;
    }
    /* No shared memory, use programmed I/O. */
    outb(ring_offset & 0xff, E33G_DMAAL);
    outb_p((ring_offset >> 8) & 0xff, E33G_DMAAH);
    outb_p((ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI) | ECNTRL_INPUT
	   | ECNTRL_START, E33G_CNTRL);

    /* This is the byte copy loop: it should probably be tuned for
       speed once everything is working. */
    for(i = 0; i < count; i++) {
	if (i % 8 == 0)
	    while ((inb(E33G_STATUS) & ESTAT_DPRDY) == 0)
		if (++boguscount > (i<<3) + 32) {
		    printk("%s: FIFO blocked in el2_block_input() (at %d of %d, bc=%d).\n",
			   dev->name, i, count, boguscount);
		    boguscount = 0;
		    break;
		}
	(skb->data)[i] = inb_p(E33G_FIFOH);
    }
    outb_p(ei_status.interface_num == 0 ? ECNTRL_THIN : ECNTRL_AUI, E33G_CNTRL);
}


#ifdef MODULE
#define MAX_EL2_CARDS	4	/* Max number of EL2 cards per module */
#define NAMELEN		8	/* # of chars for storing dev->name */
static char namelist[NAMELEN * MAX_EL2_CARDS] = { 0, };
static struct device dev_el2[MAX_EL2_CARDS] = {
	{
		NULL,		/* assign a chunk of namelist[] below */
		0, 0, 0, 0,
		0, 0,
		0, 0, 0, NULL, NULL
	},
};

static int io[MAX_EL2_CARDS] = { 0, };
static int irq[MAX_EL2_CARDS]  = { 0, };
static int xcvr[MAX_EL2_CARDS] = { 0, };	/* choose int. or ext. xcvr */

/* This is set up so that only a single autoprobe takes place per call.
ISA device autoprobes on a running machine are not recommended. */
int
init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_EL2_CARDS; this_dev++) {
		struct device *dev = &dev_el2[this_dev];
		dev->name = namelist+(NAMELEN*this_dev);
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->mem_end = xcvr[this_dev];	/* low 4bits = xcvr sel. */
		dev->init = el2_probe;
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; /* only autoprobe 1st one */
			printk(KERN_NOTICE "3c503.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		if (register_netdev(dev) != 0) {
			printk(KERN_WARNING "3c503.c: No 3c503 card found (i/o = 0x%x).\n", io[this_dev]);
			if (found != 0) return 0;	/* Got at least one. */
			return -ENXIO;
		}
		found++;
	}

	return 0;
}

void
cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_EL2_CARDS; this_dev++) {
		struct device *dev = &dev_el2[this_dev];
		if (dev->priv != NULL) {
			/* NB: el2_close() handles free_irq + irq2dev map */
			kfree(dev->priv);
			dev->priv = NULL;
			release_region(dev->base_addr, EL2_IO_EXTENT);
			unregister_netdev(dev);
		}
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 * End:
 */
