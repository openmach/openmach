/* ne.c: A general non-shared-memory NS8390 ethernet driver for linux. */
/*
    Written 1992-94 by Donald Becker.

    Copyright 1993 United States Government as represented by the
    Director, National Security Agency.

    This software may be used and distributed according to the terms
    of the GNU Public License, incorporated herein by reference.

    The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
    Center of Excellence in Space Data and Information Sciences
        Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

    This driver should work with many programmed-I/O 8390-based ethernet
    boards.  Currently it supports the NE1000, NE2000, many clones,
    and some Cabletron products.

    Changelog:

    Paul Gortmaker	: use ENISR_RDC to monitor Tx PIO uploads, made
			  sanity checks and bad clone support optional.
    Paul Gortmaker	: new reset code, reset card after probe at boot.
    Paul Gortmaker	: multiple card support for module users.
    Paul Gortmaker	: Support for PCI ne2k clones, similar to lance.c

*/

/* Routines for the NatSemi-based designs (NE[12]000). */

static const char *version =
    "ne.c:v1.10 9/23/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";


#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/bios32.h>
#include <asm/system.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "8390.h"

/* Some defines that people can play with if so inclined. */

/* Do we support clones that don't adhere to 14,15 of the SAprom ? */
#define SUPPORT_NE_BAD_CLONES

/* Do we perform extra sanity checks on stuff ? */
/* #define NE_SANITY_CHECK */

/* Do we implement the read before write bugfix ? */
/* #define NE_RW_BUGFIX */

/* Do we have a non std. amount of memory? (in units of 256 byte pages) */
/* #define PACKETBUF_MEMSIZE	0x40 */

/* ---- No user-serviceable parts below ---- */

/* A zero-terminated list of I/O addresses to be probed. */
static unsigned int netcard_portlist[] =
{ 0x300, 0x280, 0x320, 0x340, 0x360, 0};

#ifdef SUPPORT_NE_BAD_CLONES
/* A list of bad clones that we none-the-less recognize. */
static struct { const char *name8, *name16; unsigned char SAprefix[4];}
bad_clone_list[] = {
    {"DE100", "DE200", {0x00, 0xDE, 0x01,}},
    {"DE120", "DE220", {0x00, 0x80, 0xc8,}},
    {"DFI1000", "DFI2000", {'D', 'F', 'I',}}, /* Original, eh?  */
    {"EtherNext UTP8", "EtherNext UTP16", {0x00, 0x00, 0x79}},
    {"NE1000","NE2000-invalid", {0x00, 0x00, 0xd8}}, /* Ancient real NE1000. */
    {"NN1000", "NN2000",  {0x08, 0x03, 0x08}}, /* Outlaw no-name clone. */
    {"4-DIM8","4-DIM16", {0x00,0x00,0x4d,}},  /* Outlaw 4-Dimension cards. */
    {"Con-Intl_8", "Con-Intl_16", {0x00, 0x00, 0x24}}, /* Connect Int'nl */
    {0,}
};
#endif

#define NE_BASE	 (dev->base_addr)
#define NE_CMD	 	0x00
#define NE_DATAPORT	0x10	/* NatSemi-defined port window offset. */
#define NE_RESET	0x1f	/* Issue a read to reset, a write to clear. */
#define NE_IO_EXTENT	0x20

#define NE1SM_START_PG	0x20	/* First page of TX buffer */
#define NE1SM_STOP_PG 	0x40	/* Last page +1 of RX ring */
#define NESM_START_PG	0x40	/* First page of TX buffer */
#define NESM_STOP_PG	0x80	/* Last page +1 of RX ring */

/* Non-zero only if the current card is a PCI with BIOS-set IRQ. */
static unsigned char pci_irq_line = 0;

int ne_probe(struct device *dev);
static int ne_probe1(struct device *dev, int ioaddr);

static int ne_open(struct device *dev);
static int ne_close(struct device *dev);

static void ne_reset_8390(struct device *dev);
static void ne_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr,
			  int ring_page);
static void ne_block_input(struct device *dev, int count,
			  struct sk_buff *skb, int ring_offset);
static void ne_block_output(struct device *dev, const int count,
		const unsigned char *buf, const int start_page);


/*  Probe for various non-shared-memory ethercards.

   NEx000-clone boards have a Station Address PROM (SAPROM) in the packet
   buffer memory space.  NE2000 clones have 0x57,0x57 in bytes 0x0e,0x0f of
   the SAPROM, while other supposed NE2000 clones must be detected by their
   SA prefix.

   Reading the SAPROM from a word-wide card with the 8390 set in byte-wide
   mode results in doubled values, which can be detected and compensated for.

   The probe is also responsible for initializing the card and filling
   in the 'dev' and 'ei_status' structures.

   We use the minimum memory size for some ethercard product lines, iff we can't
   distinguish models.  You can increase the packet buffer size by setting
   PACKETBUF_MEMSIZE.  Reported Cabletron packet buffer locations are:
	E1010   starts at 0x100 and ends at 0x2000.
	E1010-x starts at 0x100 and ends at 0x8000. ("-x" means "more memory")
	E2010	 starts at 0x100 and ends at 0x4000.
	E2010-x starts at 0x100 and ends at 0xffff.  */

#ifdef HAVE_DEVLIST
struct netdev_entry netcard_drv =
{"ne", ne_probe1, NE_IO_EXTENT, netcard_portlist};
#else

/*  Note that this probe only picks up one card at a time, even for multiple
    PCI ne2k cards. Use "ether=0,0,eth1" if you have a second PCI ne2k card.
    This keeps things consistent regardless of the bus type of the card. */

int ne_probe(struct device *dev)
{
    int i;
    int base_addr = dev ? dev->base_addr : 0;

    /* First check any supplied i/o locations. User knows best. <cough> */
    if (base_addr > 0x1ff)	/* Check a single specified location. */
	return ne_probe1(dev, base_addr);
    else if (base_addr != 0)	/* Don't probe at all. */
	return ENXIO;

    /* Then look for any installed PCI clones */
#if defined(CONFIG_PCI)
    if (pcibios_present()) {
	int pci_index;
	for (pci_index = 0; pci_index < 8; pci_index++) {
		unsigned char pci_bus, pci_device_fn;
		unsigned int pci_ioaddr;

		/* Currently only Realtek are making PCI ne2k clones. */
		if (pcibios_find_device (PCI_VENDOR_ID_REALTEK,
				PCI_DEVICE_ID_REALTEK_8029, pci_index,
				&pci_bus, &pci_device_fn) != 0)
			break;	/* OK, now try to probe for std. ISA card */
		pcibios_read_config_byte(pci_bus, pci_device_fn,
				PCI_INTERRUPT_LINE, &pci_irq_line);
		pcibios_read_config_dword(pci_bus, pci_device_fn,
				PCI_BASE_ADDRESS_0, &pci_ioaddr);
		/* Strip the I/O address out of the returned value */
		pci_ioaddr &= PCI_BASE_ADDRESS_IO_MASK;
		/* Avoid already found cards from previous ne_probe() calls */
		if (check_region(pci_ioaddr, NE_IO_EXTENT))
			continue;
		printk("ne.c: PCI BIOS reports ne2000 clone at i/o %#x, irq %d.\n",
				pci_ioaddr, pci_irq_line);
		if (ne_probe1(dev, pci_ioaddr) != 0) {	/* Shouldn't happen. */
			printk(KERN_ERR "ne.c: Probe of PCI card at %#x failed.\n", pci_ioaddr);
			break;	/* Hrmm, try to probe for ISA card... */
		}
		pci_irq_line = 0;
		return 0;
	}
    }
#endif  /* defined(CONFIG_PCI) */

    /* Last resort. The semi-risky ISA auto-probe. */
    for (i = 0; netcard_portlist[i]; i++) {
	int ioaddr = netcard_portlist[i];
	if (check_region(ioaddr, NE_IO_EXTENT))
	    continue;
	if (ne_probe1(dev, ioaddr) == 0)
	    return 0;
    }

    return ENODEV;
}
#endif

static int ne_probe1(struct device *dev, int ioaddr)
{
    int i;
    unsigned char SA_prom[32];
    int wordlength = 2;
    const char *name = NULL;
    int start_page, stop_page;
    int neX000, ctron;
    int reg0 = inb_p(ioaddr);
    static unsigned version_printed = 0;

    if (reg0 == 0xFF)
	return ENODEV;

    /* Do a preliminary verification that we have a 8390. */
    {	int regd;
	outb_p(E8390_NODMA+E8390_PAGE1+E8390_STOP, ioaddr + E8390_CMD);
	regd = inb_p(ioaddr + 0x0d);
	outb_p(0xff, ioaddr + 0x0d);
	outb_p(E8390_NODMA+E8390_PAGE0, ioaddr + E8390_CMD);
	inb_p(ioaddr + EN0_COUNTER0); /* Clear the counter by reading. */
	if (inb_p(ioaddr + EN0_COUNTER0) != 0) {
	    outb_p(reg0, ioaddr);
	    outb_p(regd, ioaddr + 0x0d);	/* Restore the old values. */
	    return ENODEV;
	}
    }

    if (ei_debug  &&  version_printed++ == 0)
	printk(version);

    printk("NE*000 ethercard probe at %#3x:", ioaddr);

    /* Reset card. Who knows what dain-bramaged state it was left in. */
    {	unsigned long reset_start_time = jiffies;

	/* DON'T change these to inb_p/outb_p or reset will fail on clones. */
	outb(inb(ioaddr + NE_RESET), ioaddr + NE_RESET);

	while ((inb_p(ioaddr + EN0_ISR) & ENISR_RESET) == 0)
		if (jiffies - reset_start_time > 2*HZ/100) {
			printk(" not found (no reset ack).\n");
			return ENODEV;
		}

	outb_p(0xff, ioaddr + EN0_ISR);		/* Ack all intr. */
    }

    /* Read the 16 bytes of station address PROM.
       We must first initialize registers, similar to NS8390_init(eifdev, 0).
       We can't reliably read the SAPROM address without this.
       (I learned the hard way!). */
    {
	struct {unsigned char value, offset; } program_seq[] = {
	    {E8390_NODMA+E8390_PAGE0+E8390_STOP, E8390_CMD}, /* Select page 0*/
	    {0x48,	EN0_DCFG},	/* Set byte-wide (0x48) access. */
	    {0x00,	EN0_RCNTLO},	/* Clear the count regs. */
	    {0x00,	EN0_RCNTHI},
	    {0x00,	EN0_IMR},	/* Mask completion irq. */
	    {0xFF,	EN0_ISR},
	    {E8390_RXOFF, EN0_RXCR},	/* 0x20  Set to monitor */
	    {E8390_TXOFF, EN0_TXCR},	/* 0x02  and loopback mode. */
	    {32,	EN0_RCNTLO},
	    {0x00,	EN0_RCNTHI},
	    {0x00,	EN0_RSARLO},	/* DMA starting at 0x0000. */
	    {0x00,	EN0_RSARHI},
	    {E8390_RREAD+E8390_START, E8390_CMD},
	};
	for (i = 0; i < sizeof(program_seq)/sizeof(program_seq[0]); i++)
	    outb_p(program_seq[i].value, ioaddr + program_seq[i].offset);

    }
    for(i = 0; i < 32 /*sizeof(SA_prom)*/; i+=2) {
	SA_prom[i] = inb(ioaddr + NE_DATAPORT);
	SA_prom[i+1] = inb(ioaddr + NE_DATAPORT);
	if (SA_prom[i] != SA_prom[i+1])
	    wordlength = 1;
    }

    if (wordlength == 2) {
	/* We must set the 8390 for word mode. */
	outb_p(0x49, ioaddr + EN0_DCFG);
	/* We used to reset the ethercard here, but it doesn't seem
	   to be necessary. */
	/* Un-double the SA_prom values. */
	for (i = 0; i < 16; i++)
	    SA_prom[i] = SA_prom[i+i];
	start_page = NESM_START_PG;
	stop_page = NESM_STOP_PG;
    } else {
	start_page = NE1SM_START_PG;
	stop_page = NE1SM_STOP_PG;
    }

    neX000 = (SA_prom[14] == 0x57  &&  SA_prom[15] == 0x57);
    ctron =  (SA_prom[0] == 0x00 && SA_prom[1] == 0x00 && SA_prom[2] == 0x1d);

    /* Set up the rest of the parameters. */
    if (neX000) {
	name = (wordlength == 2) ? "NE2000" : "NE1000";
    } else if (ctron) {
	name = (wordlength == 2) ? "Ctron-8" : "Ctron-16";
	start_page = 0x01;
	stop_page = (wordlength == 2) ? 0x40 : 0x20;
    } else {
#ifdef SUPPORT_NE_BAD_CLONES
	/* Ack!  Well, there might be a *bad* NE*000 clone there.
	   Check for total bogus addresses. */
	for (i = 0; bad_clone_list[i].name8; i++) {
	    if (SA_prom[0] == bad_clone_list[i].SAprefix[0] &&
		SA_prom[1] == bad_clone_list[i].SAprefix[1] &&
		SA_prom[2] == bad_clone_list[i].SAprefix[2]) {
		if (wordlength == 2) {
		    name = bad_clone_list[i].name16;
		} else {
		    name = bad_clone_list[i].name8;
		}
		break;
	    }
	}
	if (bad_clone_list[i].name8 == NULL) {
	    printk(" not found (invalid signature %2.2x %2.2x).\n",
		   SA_prom[14], SA_prom[15]);
	    return ENXIO;
	}
#else
	printk(" not found.\n");
	return ENXIO;
#endif

    }

    /* We should have a "dev" from Space.c or the static module table. */
    if (dev == NULL) {
	printk("ne.c: Passed a NULL device.\n");
	dev = init_etherdev(0, 0);
    }

    if (pci_irq_line) {
	dev->irq = pci_irq_line;
    }

    if (dev->irq < 2) {
	autoirq_setup(0);
	outb_p(0x50, ioaddr + EN0_IMR);	/* Enable one interrupt. */
	outb_p(0x00, ioaddr + EN0_RCNTLO);
	outb_p(0x00, ioaddr + EN0_RCNTHI);
	outb_p(E8390_RREAD+E8390_START, ioaddr); /* Trigger it... */
	outb_p(0x00, ioaddr + EN0_IMR); 		/* Mask it again. */
	dev->irq = autoirq_report(0);
	if (ei_debug > 2)
	    printk(" autoirq is %d\n", dev->irq);
    } else if (dev->irq == 2)
	/* Fixup for users that don't know that IRQ 2 is really IRQ 9,
	   or don't know which one to set. */
	dev->irq = 9;

    if (! dev->irq) {
	printk(" failed to detect IRQ line.\n");
	return EAGAIN;
    }
    
    /* Snarf the interrupt now.  There's no point in waiting since we cannot
       share and the board will usually be enabled. */
    {
	int irqval = request_irq(dev->irq, ei_interrupt, 0, name);
	if (irqval) {
	    printk (" unable to get IRQ %d (irqval=%d).\n", dev->irq, irqval);
	    return EAGAIN;
	}
    }

    dev->base_addr = ioaddr;

    /* Allocate dev->priv and fill in 8390 specific dev fields. */
    if (ethdev_init(dev)) {
	printk (" unable to get memory for dev->priv.\n");
	free_irq(dev->irq);
	return -ENOMEM;
    }
 
    request_region(ioaddr, NE_IO_EXTENT, name);

    for(i = 0; i < ETHER_ADDR_LEN; i++) {
	printk(" %2.2x", SA_prom[i]);
	dev->dev_addr[i] = SA_prom[i];
    }

    printk("\n%s: %s found at %#x, using IRQ %d.\n",
	   dev->name, name, ioaddr, dev->irq);

    ei_status.name = name;
    ei_status.tx_start_page = start_page;
    ei_status.stop_page = stop_page;
    ei_status.word16 = (wordlength == 2);

    ei_status.rx_start_page = start_page + TX_PAGES;
#ifdef PACKETBUF_MEMSIZE
    /* Allow the packet buffer size to be overridden by know-it-alls. */
    ei_status.stop_page = ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

    ei_status.reset_8390 = &ne_reset_8390;
    ei_status.block_input = &ne_block_input;
    ei_status.block_output = &ne_block_output;
    ei_status.get_8390_hdr = &ne_get_8390_hdr;
    dev->open = &ne_open;
    dev->stop = &ne_close;
    NS8390_init(dev, 0);
    return 0;
}

static int
ne_open(struct device *dev)
{
    ei_open(dev);
    MOD_INC_USE_COUNT;
    return 0;
}

static int
ne_close(struct device *dev)
{
    if (ei_debug > 1)
	printk("%s: Shutting down ethercard.\n", dev->name);
    ei_close(dev);
    MOD_DEC_USE_COUNT;
    return 0;
}

/* Hard reset the card.  This used to pause for the same period that a
   8390 reset command required, but that shouldn't be necessary. */
static void
ne_reset_8390(struct device *dev)
{
    unsigned long reset_start_time = jiffies;

    if (ei_debug > 1) printk("resetting the 8390 t=%ld...", jiffies);

    /* DON'T change these to inb_p/outb_p or reset will fail on clones. */
    outb(inb(NE_BASE + NE_RESET), NE_BASE + NE_RESET);

    ei_status.txing = 0;
    ei_status.dmaing = 0;

    /* This check _should_not_ be necessary, omit eventually. */
    while ((inb_p(NE_BASE+EN0_ISR) & ENISR_RESET) == 0)
	if (jiffies - reset_start_time > 2*HZ/100) {
	    printk("%s: ne_reset_8390() did not complete.\n", dev->name);
	    break;
	}
    outb_p(ENISR_RESET, NE_BASE + EN0_ISR);	/* Ack intr. */
}

/* Grab the 8390 specific header. Similar to the block_input routine, but
   we don't need to be concerned with ring wrap as the header will be at
   the start of a page, so we optimize accordingly. */

static void
ne_get_8390_hdr(struct device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{

    int nic_base = dev->base_addr;

    /* This *shouldn't* happen. If it does, it's the last thing you'll see */
    if (ei_status.dmaing) {
	printk("%s: DMAing conflict in ne_get_8390_hdr "
	   "[DMAstat:%d][irqlock:%d][intr:%d].\n",
	   dev->name, ei_status.dmaing, ei_status.irqlock,
	   dev->interrupt);
	return;
    }

    ei_status.dmaing |= 0x01;
    outb_p(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base+ NE_CMD);
    outb_p(sizeof(struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);
    outb_p(0, nic_base + EN0_RCNTHI);
    outb_p(0, nic_base + EN0_RSARLO);		/* On page boundary */
    outb_p(ring_page, nic_base + EN0_RSARHI);
    outb_p(E8390_RREAD+E8390_START, nic_base + NE_CMD);

    if (ei_status.word16)
	insw(NE_BASE + NE_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr)>>1);
    else
	insb(NE_BASE + NE_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr));

    outb_p(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
    ei_status.dmaing &= ~0x01;
}

/* Block input and output, similar to the Crynwr packet driver.  If you
   are porting to a new ethercard, look at the packet driver source for hints.
   The NEx000 doesn't share the on-board packet memory -- you have to put
   the packet out through the "remote DMA" dataport using outb. */

static void
ne_block_input(struct device *dev, int count, struct sk_buff *skb, int ring_offset)
{
#ifdef NE_SANITY_CHECK
    int xfer_count = count;
#endif
    int nic_base = dev->base_addr;
    char *buf = skb->data;

    /* This *shouldn't* happen. If it does, it's the last thing you'll see */
    if (ei_status.dmaing) {
	printk("%s: DMAing conflict in ne_block_input "
	   "[DMAstat:%d][irqlock:%d][intr:%d].\n",
	   dev->name, ei_status.dmaing, ei_status.irqlock,
	   dev->interrupt);
	return;
    }
    ei_status.dmaing |= 0x01;
    outb_p(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base+ NE_CMD);
    outb_p(count & 0xff, nic_base + EN0_RCNTLO);
    outb_p(count >> 8, nic_base + EN0_RCNTHI);
    outb_p(ring_offset & 0xff, nic_base + EN0_RSARLO);
    outb_p(ring_offset >> 8, nic_base + EN0_RSARHI);
    outb_p(E8390_RREAD+E8390_START, nic_base + NE_CMD);
    if (ei_status.word16) {
      insw(NE_BASE + NE_DATAPORT,buf,count>>1);
      if (count & 0x01) {
	buf[count-1] = inb(NE_BASE + NE_DATAPORT);
#ifdef NE_SANITY_CHECK
	xfer_count++;
#endif
      }
    } else {
	insb(NE_BASE + NE_DATAPORT, buf, count);
    }

#ifdef NE_SANITY_CHECK
    /* This was for the ALPHA version only, but enough people have
       been encountering problems so it is still here.  If you see
       this message you either 1) have a slightly incompatible clone
       or 2) have noise/speed problems with your bus. */
    if (ei_debug > 1) {		/* DMA termination address check... */
	int addr, tries = 20;
	do {
	    /* DON'T check for 'inb_p(EN0_ISR) & ENISR_RDC' here
	       -- it's broken for Rx on some cards! */
	    int high = inb_p(nic_base + EN0_RSARHI);
	    int low = inb_p(nic_base + EN0_RSARLO);
	    addr = (high << 8) + low;
	    if (((ring_offset + xfer_count) & 0xff) == low)
		break;
	} while (--tries > 0);
	if (tries <= 0)
	    printk("%s: RX transfer address mismatch,"
		   "%#4.4x (expected) vs. %#4.4x (actual).\n",
		   dev->name, ring_offset + xfer_count, addr);
    }
#endif
    outb_p(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
    ei_status.dmaing &= ~0x01;
}

static void
ne_block_output(struct device *dev, int count,
		const unsigned char *buf, const int start_page)
{
    int nic_base = NE_BASE;
    unsigned long dma_start;
#ifdef NE_SANITY_CHECK
    int retries = 0;
#endif

    /* Round the count up for word writes.  Do we need to do this?
       What effect will an odd byte count have on the 8390?
       I should check someday. */
    if (ei_status.word16 && (count & 0x01))
      count++;

    /* This *shouldn't* happen. If it does, it's the last thing you'll see */
    if (ei_status.dmaing) {
	printk("%s: DMAing conflict in ne_block_output."
	   "[DMAstat:%d][irqlock:%d][intr:%d]\n",
	   dev->name, ei_status.dmaing, ei_status.irqlock,
	   dev->interrupt);
	return;
    }
    ei_status.dmaing |= 0x01;
    /* We should already be in page 0, but to be safe... */
    outb_p(E8390_PAGE0+E8390_START+E8390_NODMA, nic_base + NE_CMD);

#ifdef NE_SANITY_CHECK
 retry:
#endif

#ifdef NE8390_RW_BUGFIX
    /* Handle the read-before-write bug the same way as the
       Crynwr packet driver -- the NatSemi method doesn't work.
       Actually this doesn't always work either, but if you have
       problems with your NEx000 this is better than nothing! */
    outb_p(0x42, nic_base + EN0_RCNTLO);
    outb_p(0x00,   nic_base + EN0_RCNTHI);
    outb_p(0x42, nic_base + EN0_RSARLO);
    outb_p(0x00, nic_base + EN0_RSARHI);
    outb_p(E8390_RREAD+E8390_START, nic_base + NE_CMD);
    /* Make certain that the dummy read has occurred. */
    SLOW_DOWN_IO;
    SLOW_DOWN_IO;
    SLOW_DOWN_IO;
#endif

    outb_p(ENISR_RDC, nic_base + EN0_ISR);

   /* Now the normal output. */
    outb_p(count & 0xff, nic_base + EN0_RCNTLO);
    outb_p(count >> 8,   nic_base + EN0_RCNTHI);
    outb_p(0x00, nic_base + EN0_RSARLO);
    outb_p(start_page, nic_base + EN0_RSARHI);

    outb_p(E8390_RWRITE+E8390_START, nic_base + NE_CMD);
    if (ei_status.word16) {
	outsw(NE_BASE + NE_DATAPORT, buf, count>>1);
    } else {
	outsb(NE_BASE + NE_DATAPORT, buf, count);
    }

    dma_start = jiffies;

#ifdef NE_SANITY_CHECK
    /* This was for the ALPHA version only, but enough people have
       been encountering problems so it is still here. */
    if (ei_debug > 1) {		/* DMA termination address check... */
	int addr, tries = 20;
	do {
	    int high = inb_p(nic_base + EN0_RSARHI);
	    int low = inb_p(nic_base + EN0_RSARLO);
	    addr = (high << 8) + low;
	    if ((start_page << 8) + count == addr)
		break;
	} while (--tries > 0);
	if (tries <= 0) {
	    printk("%s: Tx packet transfer address mismatch,"
		   "%#4.4x (expected) vs. %#4.4x (actual).\n",
		   dev->name, (start_page << 8) + count, addr);
	    if (retries++ == 0)
		goto retry;
	}
    }
#endif

    while ((inb_p(nic_base + EN0_ISR) & ENISR_RDC) == 0)
	if (jiffies - dma_start > 2*HZ/100) {		/* 20ms */
		printk("%s: timeout waiting for Tx RDC.\n", dev->name);
		ne_reset_8390(dev);
		NS8390_init(dev,1);
		break;
	}

    outb_p(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
    ei_status.dmaing &= ~0x01;
    return;
}


#ifdef MODULE
#define MAX_NE_CARDS	4	/* Max number of NE cards per module */
#define NAMELEN		8	/* # of chars for storing dev->name */
static char namelist[NAMELEN * MAX_NE_CARDS] = { 0, };
static struct device dev_ne[MAX_NE_CARDS] = {
	{
		NULL,		/* assign a chunk of namelist[] below */
		0, 0, 0, 0,
		0, 0,
		0, 0, 0, NULL, NULL
	},
};

static int io[MAX_NE_CARDS] = { 0, };
static int irq[MAX_NE_CARDS]  = { 0, };

/* This is set up so that no autoprobe takes place. We can't guarantee
that the ne2k probe is the last 8390 based probe to take place (as it
is at boot) and so the probe will get confused by any other 8390 cards.
ISA device autoprobes on a running machine are not recommended anyway. */

int
init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_NE_CARDS; this_dev++) {
		struct device *dev = &dev_ne[this_dev];
		dev->name = namelist+(NAMELEN*this_dev);
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->init = ne_probe;
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break; /* only complain once */
			printk(KERN_NOTICE "ne.c: Module autoprobing not allowed. Append \"io=0xNNN\" value(s).\n");
			return -EPERM;
		}
		if (register_netdev(dev) != 0) {
			printk(KERN_WARNING "ne.c: No NE*000 card found (i/o = 0x%x).\n", io[this_dev]);
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

	for (this_dev = 0; this_dev < MAX_NE_CARDS; this_dev++) {
		struct device *dev = &dev_ne[this_dev];
		if (dev->priv != NULL) {
			kfree(dev->priv);
			dev->priv = NULL;
			free_irq(dev->irq);
			irq2dev_map[dev->irq] = NULL;
			release_region(dev->base_addr, NE_IO_EXTENT);
			unregister_netdev(dev);
		}
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -DKERNEL -Wall -O6 -fomit-frame-pointer -I/usr/src/linux/net/tcp -c ne.c"
 *  version-control: t
 *  kept-new-versions: 5
 * End:
 */
