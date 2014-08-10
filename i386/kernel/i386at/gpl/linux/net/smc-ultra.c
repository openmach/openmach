/* smc-ultra.c: A SMC Ultra ethernet driver for linux. */
/*
	Written 1993,1994,1995 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
		Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

	This is a driver for the SMC Ultra and SMC EtherEZ ethercards.

	This driver uses the cards in the 8390-compatible, shared memory mode.
	Most of the run-time complexity is handled by the generic code in
	8390.c.  The code in this file is responsible for

		ultra_probe()	 	Detecting and initializing the card.
		ultra_probe1()	

		ultra_open()		The card-specific details of starting, stopping
		ultra_reset_8390()	and resetting the 8390 NIC core.
		ultra_close()

		ultra_block_input()		Routines for reading and writing blocks of
		ultra_block_output()	packet buffer memory.

	This driver enables the shared memory only when doing the actual data
	transfers to avoid a bug in early version of the card that corrupted
	data transferred by a AHA1542.

	This driver does not support the programmed-I/O data transfer mode of
	the EtherEZ.  That support (if available) is smc-ez.c.  Nor does it
	use the non-8390-compatible "Altego" mode. (No support currently planned.)

	Changelog:

	Paul Gortmaker	: multiple card support for module users.
*/

static const char *version =
	"smc-ultra.c:v1.12 1/18/95 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";


#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "8390.h"

/* A zero-terminated list of I/O addresses to be probed. */
static unsigned int ultra_portlist[] =
{0x200, 0x220, 0x240, 0x280, 0x300, 0x340, 0x380, 0};

int ultra_probe(struct device *dev);
int ultra_probe1(struct device *dev, int ioaddr);

static int ultra_open(struct device *dev);
static void ultra_reset_8390(struct device *dev);
static void ultra_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr, 
						int ring_page);
static void ultra_block_input(struct device *dev, int count,
						  struct sk_buff *skb, int ring_offset);
static void ultra_block_output(struct device *dev, int count,
							const unsigned char *buf, const start_page);
static int ultra_close_card(struct device *dev);


#define START_PG		0x00	/* First page of TX buffer */

#define ULTRA_CMDREG	0		/* Offset to ASIC command register. */
#define	 ULTRA_RESET	0x80	/* Board reset, in ULTRA_CMDREG. */
#define	 ULTRA_MEMENB	0x40	/* Enable the shared memory. */
#define ULTRA_NIC_OFFSET  16	/* NIC register offset from the base_addr. */
#define ULTRA_IO_EXTENT 32

/*	Probe for the Ultra.  This looks like a 8013 with the station
	address PROM at I/O ports <base>+8 to <base>+13, with a checksum
	following.
*/
#ifdef HAVE_DEVLIST
struct netdev_entry ultra_drv =
{"ultra", ultra_probe1, NETCARD_IO_EXTENT, netcard_portlist};
#else

int ultra_probe(struct device *dev)
{
	int i;
	int base_addr = dev ? dev->base_addr : 0;

	if (base_addr > 0x1ff)		/* Check a single specified location. */
		return ultra_probe1(dev, base_addr);
	else if (base_addr != 0)	/* Don't probe at all. */
		return ENXIO;

	for (i = 0; ultra_portlist[i]; i++) {
		int ioaddr = ultra_portlist[i];
		if (check_region(ioaddr, ULTRA_IO_EXTENT))
			continue;
		if (ultra_probe1(dev, ioaddr) == 0)
			return 0;
	}

	return ENODEV;
}
#endif

int ultra_probe1(struct device *dev, int ioaddr)
{
	int i;
	int checksum = 0;
	const char *model_name;
	unsigned char eeprom_irq = 0;
	static unsigned version_printed = 0;
	/* Values from various config regs. */
	unsigned char num_pages, irqreg, addr;
	unsigned char idreg = inb(ioaddr + 7);
	unsigned char reg4 = inb(ioaddr + 4) & 0x7f;

	/* Check the ID nibble. */
	if ((idreg & 0xF0) != 0x20 			/* SMC Ultra */
		&& (idreg & 0xF0) != 0x40) 		/* SMC EtherEZ */
		return ENODEV;

	/* Select the station address register set. */
	outb(reg4, ioaddr + 4);

	for (i = 0; i < 8; i++)
		checksum += inb(ioaddr + 8 + i);
	if ((checksum & 0xff) != 0xFF)
		return ENODEV;

	/* We should have a "dev" from Space.c or the static module table. */
	if (dev == NULL) {
		printk("smc-ultra.c: Passed a NULL device.\n");
		dev = init_etherdev(0, 0);
	}

	if (ei_debug  &&  version_printed++ == 0)
		printk(version);

	model_name = (idreg & 0xF0) == 0x20 ? "SMC Ultra" : "SMC EtherEZ";

	printk("%s: %s at %#3x,", dev->name, model_name, ioaddr);

	for (i = 0; i < 6; i++)
		printk(" %2.2X", dev->dev_addr[i] = inb(ioaddr + 8 + i));

	/* Switch from the station address to the alternate register set and
	   read the useful registers there. */
	outb(0x80 | reg4, ioaddr + 4);

	/* Enabled FINE16 mode to avoid BIOS ROM width mismatches @ reboot. */
	outb(0x80 | inb(ioaddr + 0x0c), ioaddr + 0x0c);
	irqreg = inb(ioaddr + 0xd);
	addr = inb(ioaddr + 0xb);

	/* Switch back to the station address register set so that the MS-DOS driver
	   can find the card after a warm boot. */
	outb(reg4, ioaddr + 4);

	if (dev->irq < 2) {
		unsigned char irqmap[] = {0, 9, 3, 5, 7, 10, 11, 15};
		int irq;

		/* The IRQ bits are split. */
		irq = irqmap[((irqreg & 0x40) >> 4) + ((irqreg & 0x0c) >> 2)];

		if (irq == 0) {
			printk(", failed to detect IRQ line.\n");
			return -EAGAIN;
		}
		dev->irq = irq;
		eeprom_irq = 1;
	}

	/* Allocate dev->priv and fill in 8390 specific dev fields. */
	if (ethdev_init(dev)) {
		printk (", no memory for dev->priv.\n");
                return -ENOMEM;
        }
 
	/* OK, we are certain this is going to work.  Setup the device. */
	request_region(ioaddr, ULTRA_IO_EXTENT, model_name);

	/* The 8390 isn't at the base address, so fake the offset */
	dev->base_addr = ioaddr+ULTRA_NIC_OFFSET;

	{
		int addr_tbl[4] = {0x0C0000, 0x0E0000, 0xFC0000, 0xFE0000};
		short num_pages_tbl[4] = {0x20, 0x40, 0x80, 0xff};

		dev->mem_start = ((addr & 0x0f) << 13) + addr_tbl[(addr >> 6) & 3] ;
		num_pages = num_pages_tbl[(addr >> 4) & 3];
	}

	ei_status.name = model_name;
	ei_status.word16 = 1;
	ei_status.tx_start_page = START_PG;
	ei_status.rx_start_page = START_PG + TX_PAGES;
	ei_status.stop_page = num_pages;

	dev->rmem_start = dev->mem_start + TX_PAGES*256;
	dev->mem_end = dev->rmem_end
		= dev->mem_start + (ei_status.stop_page - START_PG)*256;

	printk(",%s IRQ %d memory %#lx-%#lx.\n", eeprom_irq ? "" : "assigned ",
		   dev->irq, dev->mem_start, dev->mem_end-1);

	ei_status.reset_8390 = &ultra_reset_8390;
	ei_status.block_input = &ultra_block_input;
	ei_status.block_output = &ultra_block_output;
	ei_status.get_8390_hdr = &ultra_get_8390_hdr;
	dev->open = &ultra_open;
	dev->stop = &ultra_close_card;
	NS8390_init(dev, 0);

	return 0;
}

static int
ultra_open(struct device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET; /* ASIC addr */

	if (request_irq(dev->irq, ei_interrupt, 0, ei_status.name))
		return -EAGAIN;

	outb(ULTRA_MEMENB, ioaddr);	/* Enable memory, 16 bit mode. */
	outb(0x80, ioaddr + 5);
	outb(0x01, ioaddr + 6);		/* Enable interrupts and memory. */
	ei_open(dev);
	MOD_INC_USE_COUNT;
	return 0;
}

static void
ultra_reset_8390(struct device *dev)
{
	int cmd_port = dev->base_addr - ULTRA_NIC_OFFSET; /* ASIC base addr */

	outb(ULTRA_RESET, cmd_port);
	if (ei_debug > 1) printk("resetting Ultra, t=%ld...", jiffies);
	ei_status.txing = 0;

	outb(ULTRA_MEMENB, cmd_port);

	if (ei_debug > 1) printk("reset done\n");
	return;
}

/* Grab the 8390 specific header. Similar to the block_input routine, but
   we don't need to be concerned with ring wrap as the header will be at
   the start of a page, so we optimize accordingly. */

static void
ultra_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{

	unsigned long hdr_start = dev->mem_start + ((ring_page - START_PG)<<8);

	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);	/* shmem on */
#ifdef notdef
	/* Officially this is what we are doing, but the readl() is faster */
	memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
#else
	((unsigned int*)hdr)[0] = readl(hdr_start);
#endif
	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET); /* shmem off */
}

/* Block input and output are easy on shared memory ethercards, the only
   complication is when the ring buffer wraps. */

static void
ultra_block_input(struct device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	unsigned long xfer_start = dev->mem_start + ring_offset - (START_PG<<8);

	/* Enable shared memory. */
	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

	if (xfer_start + count > dev->rmem_end) {
		/* We must wrap the input move. */
		int semi_count = dev->rmem_end - xfer_start;
		memcpy_fromio(skb->data, xfer_start, semi_count);
		count -= semi_count;
		memcpy_fromio(skb->data + semi_count, dev->rmem_start, count);
	} else {
		/* Packet is in one chunk -- we can copy + cksum. */
		eth_io_copy_and_sum(skb, xfer_start, count, 0);
	}

	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET);	/* Disable memory. */
}

static void
ultra_block_output(struct device *dev, int count, const unsigned char *buf,
				int start_page)
{
	unsigned long shmem = dev->mem_start + ((start_page - START_PG)<<8);

	/* Enable shared memory. */
	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

	memcpy_toio(shmem, buf, count);

	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET); /* Disable memory. */
}

static int
ultra_close_card(struct device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET; /* CMDREG */

	dev->start = 0;
	dev->tbusy = 1;

	if (ei_debug > 1)
		printk("%s: Shutting down ethercard.\n", dev->name);

	outb(0x00, ioaddr + 6);		/* Disable interrupts. */
	free_irq(dev->irq);
	irq2dev_map[dev->irq] = 0;

	NS8390_init(dev, 0);

	/* We should someday disable shared memory and change to 8-bit mode
	   "just in case"... */

	MOD_DEC_USE_COUNT;

	return 0;
}


#ifdef MODULE
#define MAX_ULTRA_CARDS	4	/* Max number of Ultra cards per module */
#define NAMELEN		8	/* # of chars for storing dev->name */
static char namelist[NAMELEN * MAX_ULTRA_CARDS] = { 0, };
static struct device dev_ultra[MAX_ULTRA_CARDS] = {
	{
		NULL,		/* assign a chunk of namelist[] below */
		0, 0, 0, 0,
		0, 0,
		0, 0, 0, NULL, NULL
	},
};

static int io[MAX_ULTRA_CARDS] = { 0, };
static int irq[MAX_ULTRA_CARDS]  = { 0, };

/* This is set up so that only a single autoprobe takes place per call.
ISA device autoprobes on a running machine are not recommended. */
int
init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_ULTRA_CARDS; this_dev++) {
		struct device *dev = &dev_ultra[this_dev];
		dev->name = namelist+(NAMELEN*this_dev);
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->init = ultra_probe;
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; /* only autoprobe 1st one */
			printk(KERN_NOTICE "smc-ultra.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		if (register_netdev(dev) != 0) {
			printk(KERN_WARNING "smc-ultra.c: No SMC Ultra card found (i/o = 0x%x).\n", io[this_dev]);
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

	for (this_dev = 0; this_dev < MAX_ULTRA_CARDS; this_dev++) {
		struct device *dev = &dev_ultra[this_dev];
		if (dev->priv != NULL) {
			/* NB: ultra_close_card() does free_irq + irq2dev */
			int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;
			kfree(dev->priv);
			dev->priv = NULL;
			release_region(ioaddr, ULTRA_IO_EXTENT);
			unregister_netdev(dev);
		}
	}
}
#endif /* MODULE */


/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -Wall -O6 -I/usr/src/linux/net/inet -c smc-ultra.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
