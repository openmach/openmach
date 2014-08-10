/* at1700.c: A network device driver for  the Allied Telesis AT1700.

	Written 1993-94 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

	This is a device driver for the Allied Telesis AT1700, which is a
	straight-forward Fujitsu MB86965 implementation.

  Sources:
    The Fujitsu MB86965 datasheet.

	After the initial version of this driver was written Gerry Sawkins of
	ATI provided their EEPROM configuration code header file.
    Thanks to NIIBE Yutaka <gniibe@mri.co.jp> for bug fixes.

  Bugs:
	The MB86965 has a design flaw that makes all probes unreliable.  Not
	only is it difficult to detect, it also moves around in I/O space in
	response to inb()s from other device probes!
*/

static const char *version =
	"at1700.c:v1.12 1/18/95  Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

/* This unusual address order is used to verify the CONFIG register. */
static int at1700_probe_list[] =
{0x260, 0x280, 0x2a0, 0x240, 0x340, 0x320, 0x380, 0x300, 0};

/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 1
#endif
static unsigned int net_debug = NET_DEBUG;

typedef unsigned char uchar;

/* Information that need to be kept for each board. */
struct net_local {
	struct enet_statistics stats;
	uint tx_started:1;			/* Number of packet on the Tx queue. */
	uchar tx_queue;				/* Number of packet on the Tx queue. */
	ushort tx_queue_len;		/* Current length of the Tx queue. */
};


/* Offsets from the base address. */
#define STATUS			0
#define TX_STATUS		0
#define RX_STATUS		1
#define TX_INTR			2		/* Bit-mapped interrupt enable registers. */
#define RX_INTR			3
#define TX_MODE			4
#define RX_MODE			5
#define CONFIG_0		6		/* Misc. configuration settings. */
#define CONFIG_1		7
/* Run-time register bank 2 definitions. */
#define DATAPORT		8		/* Word-wide DMA or programmed-I/O dataport. */
#define TX_START		10
#define MODE13			13
#define EEPROM_Ctrl 	16
#define EEPROM_Data 	17
#define IOCONFIG		19
#define RESET			31		/* Write to reset some parts of the chip. */
#define AT1700_IO_EXTENT	32

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x40	/* EEPROM shift clock, in reg. 16. */
#define EE_CS			0x20	/* EEPROM chip select, in reg. 16. */
#define EE_DATA_WRITE	0x80	/* EEPROM chip data in, in reg. 17. */
#define EE_DATA_READ	0x80	/* EEPROM chip data out, in reg. 17. */

/* Delay between EEPROM clock transitions. */
#define eeprom_delay()	do { int _i = 40; while (--_i > 0) { __SLOW_DOWN_IO; }} while (0)

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)


/* Index to functions, as function prototypes. */

extern int at1700_probe(struct device *dev);

static int at1700_probe1(struct device *dev, short ioaddr);
static int read_eeprom(int ioaddr, int location);
static int net_open(struct device *dev);
static int	net_send_packet(struct sk_buff *skb, struct device *dev);
static void net_interrupt(int irq, struct pt_regs *regs);
static void net_rx(struct device *dev);
static int net_close(struct device *dev);
static struct enet_statistics *net_get_stats(struct device *dev);
static void set_multicast_list(struct device *dev);


/* Check for a network adaptor of this type, and return '0' iff one exists.
   If dev->base_addr == 0, probe all likely locations.
   If dev->base_addr == 1, always return failure.
   If dev->base_addr == 2, allocate space for the device and return success
   (detachable devices only).
   */
#ifdef HAVE_DEVLIST
/* Support for a alternate probe manager, which will eliminate the
   boilerplate below. */
struct netdev_entry at1700_drv =
{"at1700", at1700_probe1, AT1700_IO_EXTENT, at1700_probe_list};
#else
int
at1700_probe(struct device *dev)
{
	int i;
	int base_addr = dev ? dev->base_addr : 0;

	if (base_addr > 0x1ff)		/* Check a single specified location. */
		return at1700_probe1(dev, base_addr);
	else if (base_addr != 0)	/* Don't probe at all. */
		return ENXIO;

	for (i = 0; at1700_probe_list[i]; i++) {
		int ioaddr = at1700_probe_list[i];
		if (check_region(ioaddr, AT1700_IO_EXTENT))
			continue;
		if (at1700_probe1(dev, ioaddr) == 0)
			return 0;
	}

	return ENODEV;
}
#endif

/* The Fujitsu datasheet suggests that the NIC be probed for by checking its
   "signature", the default bit pattern after a reset.  This *doesn't* work --
   there is no way to reset the bus interface without a complete power-cycle!

   It turns out that ATI came to the same conclusion I did: the only thing
   that can be done is checking a few bits and then diving right into an
   EEPROM read. */

int at1700_probe1(struct device *dev, short ioaddr)
{
	char irqmap[8] = {3, 4, 5, 9, 10, 11, 14, 15};
	unsigned int i, irq;

	/* Resetting the chip doesn't reset the ISA interface, so don't bother.
	   That means we have to be careful with the register values we probe for.
	   */
#ifdef notdef
	printk("at1700 probe at %#x, eeprom is %4.4x %4.4x %4.4x ctrl %4.4x.\n",
		   ioaddr, read_eeprom(ioaddr, 4), read_eeprom(ioaddr, 5),
		   read_eeprom(ioaddr, 6), inw(ioaddr + EEPROM_Ctrl));
#endif
	if (at1700_probe_list[inb(ioaddr + IOCONFIG) & 0x07] != ioaddr
		|| read_eeprom(ioaddr, 4) != 0x0000
		|| (read_eeprom(ioaddr, 5) & 0xff00) != 0xF400)
		return -ENODEV;

	/* Reset the internal state machines. */
	outb(0, ioaddr + RESET);

	irq = irqmap[(read_eeprom(ioaddr, 12)&0x04)
				 | (read_eeprom(ioaddr, 0)>>14)];

	/* Snarf the interrupt vector now. */
	if (request_irq(irq, &net_interrupt, 0, "at1700")) {
		printk ("AT1700 found at %#3x, but it's unusable due to a conflict on"
				"IRQ %d.\n", ioaddr, irq);
		return EAGAIN;
	}

	/* Allocate a new 'dev' if needed. */
	if (dev == NULL)
		dev = init_etherdev(0, sizeof(struct net_local));

	/* Grab the region so that we can find another board if the IRQ request
	   fails. */
	request_region(ioaddr, AT1700_IO_EXTENT, "at1700");

	printk("%s: AT1700 found at %#3x, IRQ %d, address ", dev->name,
		   ioaddr, irq);

	dev->base_addr = ioaddr;
	dev->irq = irq;
	irq2dev_map[irq] = dev;

	for(i = 0; i < 3; i++) {
		unsigned short eeprom_val = read_eeprom(ioaddr, 4+i);
		printk("%04x", eeprom_val);
		((unsigned short *)dev->dev_addr)[i] = ntohs(eeprom_val);
	}

	/* The EEPROM word 12 bit 0x0400 means use regular 100 ohm 10baseT signals,
	   rather than 150 ohm shielded twisted pair compensation.
	   0x0000 == auto-sense the interface
	   0x0800 == use TP interface
	   0x1800 == use coax interface
	   */
	{
		const char *porttype[] = {"auto-sense", "10baseT", "auto-sense", "10base2"};
		ushort setup_value = read_eeprom(ioaddr, 12);

		dev->if_port = setup_value >> 8;
		printk(" %s interface.\n", porttype[(dev->if_port>>3) & 3]);
	}

	/* Set the station address in bank zero. */
	outb(0xe0, ioaddr + 7);
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + 8 + i);

	/* Switch to bank 1 and set the multicast table to accept none. */
	outb(0xe4, ioaddr + 7);
	for (i = 0; i < 8; i++)
		outb(0x00, ioaddr + 8 + i);

	/* Set the configuration register 0 to 32K 100ns. byte-wide memory, 16 bit
	   bus access, two 4K Tx queues, and disabled Tx and Rx. */
	outb(0xda, ioaddr + CONFIG_0);

	/* Switch to bank 2 and lock our I/O address. */
	outb(0xe8, ioaddr + 7);
	outb(dev->if_port, MODE13);

	/* Power-down the chip.  Aren't we green! */
	outb(0x00, ioaddr + CONFIG_1);

	if (net_debug)
		printk(version);

	/* Initialize the device structure. */
	dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct net_local));

	dev->open		= net_open;
	dev->stop		= net_close;
	dev->hard_start_xmit = net_send_packet;
	dev->get_stats	= net_get_stats;
	dev->set_multicast_list = &set_multicast_list;

	/* Fill in the fields of 'dev' with ethernet-generic values. */
	   
	ether_setup(dev);
	return 0;
}

static int read_eeprom(int ioaddr, int location)
{
	int i;
	unsigned short retval = 0;
	short ee_addr = ioaddr + EEPROM_Ctrl;
	short ee_daddr = ioaddr + EEPROM_Data;
	int read_cmd = location | EE_READ_CMD;
	short ctrl_val = EE_CS;
	
	outb(ctrl_val, ee_addr);
	
	/* Shift the read command bits out. */
	for (i = 9; i >= 0; i--) {
		short dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(dataval, ee_daddr);
		outb(EE_CS | EE_SHIFT_CLK, ee_addr);	/* EEPROM clock tick. */
		eeprom_delay();
		outb(EE_CS, ee_addr);	/* Finish EEPROM a clock tick. */
		eeprom_delay();
	}
	outb(EE_CS, ee_addr);
	
	for (i = 16; i > 0; i--) {
		outb(EE_CS | EE_SHIFT_CLK, ee_addr);
		eeprom_delay();
		retval = (retval << 1) | ((inb(ee_daddr) & EE_DATA_READ) ? 1 : 0);
		outb(EE_CS, ee_addr);
		eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	ctrl_val &= ~EE_CS;
	outb(ctrl_val | EE_SHIFT_CLK, ee_addr);
	eeprom_delay();
	outb(ctrl_val, ee_addr);
	eeprom_delay();
	return retval;
}



static int net_open(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	int i;

	/* Powerup the chip, initialize config register 1, and select bank 0. */
	outb(0xe0, ioaddr + CONFIG_1);

	/* Set the station address in bank zero. */
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + 8 + i);

	/* Switch to bank 1 and set the multicast table to accept none. */
	outb(0xe4, ioaddr + 7);
	for (i = 0; i < 8; i++)
		outb(0x00, ioaddr + 8 + i);

	/* Set the configuration register 0 to 32K 100ns. byte-wide memory, 16 bit
	   bus access, and two 4K Tx queues. */
	outb(0xda, ioaddr + CONFIG_0);

	/* Same config 0, except enable the Rx and Tx. */
	outb(0x5a, ioaddr + CONFIG_0);
	/* Switch to register bank 2 for the run-time registers. */
	outb(0xe8, ioaddr + CONFIG_1);

	lp->tx_started = 0;
	lp->tx_queue = 0;
	lp->tx_queue_len = 0;

	/* Turn on Rx interrupts, leave Tx interrupts off until packet Tx. */
	outb(0x00, ioaddr + TX_INTR);
	outb(0x81, ioaddr + RX_INTR);

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	MOD_INC_USE_COUNT;

	return 0;
}

static int
net_send_packet(struct sk_buff *skb, struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;

	if (dev->tbusy) {
		/* If we get here, some higher level has decided we are broken.
		   There should really be a "kick me" function call instead. */
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < 10)
			return 1;
		printk("%s: transmit timed out with status %04x, %s?\n", dev->name,
			   inw(ioaddr + STATUS), inb(ioaddr + TX_STATUS) & 0x80
			   ? "IRQ conflict" : "network cable problem");
		printk("%s: timeout registers: %04x %04x %04x %04x %04x %04x %04x %04x.\n",
			   dev->name, inw(ioaddr + 0), inw(ioaddr + 2), inw(ioaddr + 4),
			   inw(ioaddr + 6), inw(ioaddr + 8), inw(ioaddr + 10),
			   inw(ioaddr + 12), inw(ioaddr + 14));
		lp->stats.tx_errors++;
		/* ToDo: We should try to restart the adaptor... */
		outw(0xffff, ioaddr + 24);
		outw(0xffff, ioaddr + TX_STATUS);
		outw(0xe85a, ioaddr + CONFIG_0);
		outw(0x8100, ioaddr + TX_INTR);
		dev->tbusy=0;
		dev->trans_start = jiffies;
		lp->tx_started = 0;
		lp->tx_queue = 0;
		lp->tx_queue_len = 0;
	}

	/* If some higher layer thinks we've missed an tx-done interrupt
	   we are passed NULL. Caution: dev_tint() handles the cli()/sti()
	   itself. */
	if (skb == NULL) {
		dev_tint(dev);
		return 0;
	}

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
	if (set_bit(0, (void*)&dev->tbusy) != 0)
		printk("%s: Transmitter access conflict.\n", dev->name);
	else {
		short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
		unsigned char *buf = skb->data;

		/* Turn off the possible Tx interrupts. */
		outb(0x00, ioaddr + TX_INTR);
		
		outw(length, ioaddr + DATAPORT);
		outsw(ioaddr + DATAPORT, buf, (length + 1) >> 1);

		lp->tx_queue++;
		lp->tx_queue_len += length + 2;

		if (lp->tx_started == 0) {
			/* If the Tx is idle, always trigger a transmit. */
			outb(0x80 | lp->tx_queue, ioaddr + TX_START);
			lp->tx_queue = 0;
			lp->tx_queue_len = 0;
			dev->trans_start = jiffies;
			lp->tx_started = 1;
			dev->tbusy = 0;
		} else if (lp->tx_queue_len < 4096 - 1502)
			/* Yes, there is room for one more packet. */
			dev->tbusy = 0;

		/* Turn on Tx interrupts back on. */
		outb(0x82, ioaddr + TX_INTR);
	}
	dev_kfree_skb (skb, FREE_WRITE);

	return 0;
}

/* The typical workload of the driver:
   Handle the network interface interrupts. */
static void
net_interrupt(int irq, struct pt_regs *regs)
{
	struct device *dev = (struct device *)(irq2dev_map[irq]);
	struct net_local *lp;
	int ioaddr, status;

	if (dev == NULL) {
		printk ("at1700_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}
	dev->interrupt = 1;

	ioaddr = dev->base_addr;
	lp = (struct net_local *)dev->priv;
	status = inw(ioaddr + TX_STATUS);
	outw(status, ioaddr + TX_STATUS);

	if (net_debug > 4)
		printk("%s: Interrupt with status %04x.\n", dev->name, status);
	if (status & 0xff00
		||  (inb(ioaddr + RX_MODE) & 0x40) == 0) {			/* Got a packet(s). */
		net_rx(dev);
	}
	if (status & 0x00ff) {
		if (status & 0x80) {
			lp->stats.tx_packets++;
			if (lp->tx_queue) {
				outb(0x80 | lp->tx_queue, ioaddr + TX_START);
				lp->tx_queue = 0;
				lp->tx_queue_len = 0;
				dev->trans_start = jiffies;
				dev->tbusy = 0;
				mark_bh(NET_BH);	/* Inform upper layers. */
			} else {
				lp->tx_started = 0;
				/* Turn on Tx interrupts off. */
				outb(0x00, ioaddr + TX_INTR);
				dev->tbusy = 0;
				mark_bh(NET_BH);	/* Inform upper layers. */
			}
		}
	}

	dev->interrupt = 0;
	return;
}

/* We have a good packet(s), get it/them out of the buffers. */
static void
net_rx(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	int boguscount = 5;

	while ((inb(ioaddr + RX_MODE) & 0x40) == 0) {
		ushort status = inw(ioaddr + DATAPORT);
		ushort pkt_len = inw(ioaddr + DATAPORT);

		if (net_debug > 4)
			printk("%s: Rxing packet mode %02x status %04x.\n",
				   dev->name, inb(ioaddr + RX_MODE), status);
#ifndef final_version
		if (status == 0) {
			outb(0x05, ioaddr + 14);
			break;
		}
#endif

		if ((status & 0xF0) != 0x20) {	/* There was an error. */
			lp->stats.rx_errors++;
			if (status & 0x08) lp->stats.rx_length_errors++;
			if (status & 0x04) lp->stats.rx_frame_errors++;
			if (status & 0x02) lp->stats.rx_crc_errors++;
			if (status & 0x01) lp->stats.rx_over_errors++;
		} else {
			/* Malloc up new buffer. */
			struct sk_buff *skb;

			if (pkt_len > 1550) {
				printk("%s: The AT1700 claimed a very large packet, size %d.\n",
					   dev->name, pkt_len);
				/* Prime the FIFO and then flush the packet. */
				inw(ioaddr + DATAPORT); inw(ioaddr + DATAPORT);
				outb(0x05, ioaddr + 14);
				lp->stats.rx_errors++;
				break;
			}
			skb = dev_alloc_skb(pkt_len+3);
			if (skb == NULL) {
				printk("%s: Memory squeeze, dropping packet (len %d).\n",
					   dev->name, pkt_len);
				/* Prime the FIFO and then flush the packet. */
				inw(ioaddr + DATAPORT); inw(ioaddr + DATAPORT);
				outb(0x05, ioaddr + 14);
				lp->stats.rx_dropped++;
				break;
			}
			skb->dev = dev;
			skb_reserve(skb,2);

			insw(ioaddr + DATAPORT, skb_put(skb,pkt_len), (pkt_len + 1) >> 1);
			skb->protocol=eth_type_trans(skb, dev);
			netif_rx(skb);
			lp->stats.rx_packets++;
		}
		if (--boguscount <= 0)
			break;
	}

	/* If any worth-while packets have been received, dev_rint()
	   has done a mark_bh(NET_BH) for us and will work on them
	   when we get to the bottom-half routine. */
	{
		int i;
		for (i = 0; i < 20; i++) {
			if ((inb(ioaddr + RX_MODE) & 0x40) == 0x40)
				break;
			inw(ioaddr + DATAPORT);				/* dummy status read */
			outb(0x05, ioaddr + 14);
		}

		if (net_debug > 5)
			printk("%s: Exint Rx packet with mode %02x after %d ticks.\n", 
				   dev->name, inb(ioaddr + RX_MODE), i);
	}
	return;
}

/* The inverse routine to net_open(). */
static int net_close(struct device *dev)
{
	int ioaddr = dev->base_addr;

	dev->tbusy = 1;
	dev->start = 0;

	/* Set configuration register 0 to disable Tx and Rx. */
	outb(0xda, ioaddr + CONFIG_0);

	/* Update the statistics -- ToDo. */

	/* Power-down the chip.  Green, green, green! */
	outb(0x00, ioaddr + CONFIG_1);

	MOD_DEC_USE_COUNT;

	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct enet_statistics *
net_get_stats(struct device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	cli();
	/* ToDo: Update the statistics from the device registers. */
	sti();

	return &lp->stats;
}

/* Set or clear the multicast filter for this adaptor.
   num_addrs == -1	Promiscuous mode, receive all packets
   num_addrs == 0	Normal mode, clear multicast list
   num_addrs > 0	Multicast mode, receive normal and MC packets, and do
			best-effort filtering.
 */
static void
set_multicast_list(struct device *dev)
{
	short ioaddr = dev->base_addr;
	if (dev->mc_count || dev->flags&(IFF_PROMISC|IFF_ALLMULTI)) 
	{
		/*
		 *	We must make the kernel realise we had to move
		 *	into promisc mode or we start all out war on
		 *	the cable. - AC
		 */
		dev->flags|=IFF_PROMISC;		
	
		outb(3, ioaddr + RX_MODE);	/* Enable promiscuous mode */
	} 
	else
		outb(2, ioaddr + RX_MODE);	/* Disable promiscuous, use normal mode */
}
#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_at1700 = {
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, at1700_probe };

static int io = 0x260;
static int irq = 0;

int init_module(void)
{
	if (io == 0)
		printk("at1700: You should not use auto-probing with insmod!\n");
	dev_at1700.base_addr = io;
	dev_at1700.irq       = irq;
	if (register_netdev(&dev_at1700) != 0) {
		printk("at1700: register_netdev() returned non-zero.\n");
		return -EIO;
	}
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(&dev_at1700);
	kfree(dev_at1700.priv);
	dev_at1700.priv = NULL;

	/* If we don't do this, we can't re-insmod it later. */
	free_irq(dev_at1700.irq);
	irq2dev_map[dev_at1700.irq] = NULL;
	release_region(dev_at1700.base_addr, AT1700_IO_EXTENT);
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c at1700.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 *  c-indent-level: 4
 * End:
 */
