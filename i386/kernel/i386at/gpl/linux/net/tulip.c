/* tulip.c: A DEC 21040 ethernet driver for linux. */
/*
   NOTICE: this version works with kernels 1.1.82 and later only!
	Written 1994,1995 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	This driver is for the SMC EtherPower PCI ethernet adapter.
	It should work with most other DEC 21*40-based ethercards.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
*/

static const char *version = "tulip.c:v0.05 1/20/95 becker@cesdis.gsfc.nasa.gov\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/bios32.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

/* The total size is unusually large: The 21040 aligns each of its 16
   longword-wide registers on a quadword boundary. */
#define TULIP_TOTAL_SIZE 0x80

#ifdef HAVE_DEVLIST
struct netdev_entry tulip_drv =
{"Tulip", tulip_pci_probe, TULIP_TOTAL_SIZE, NULL};
#endif

#define TULIP_DEBUG 1
#ifdef TULIP_DEBUG
int tulip_debug = TULIP_DEBUG;
#else
int tulip_debug = 1;
#endif

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the DECchip 21040 "Tulip", Digital's
single-chip ethernet controller for PCI, as used on the SMC EtherPower
ethernet adapter.

II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS should be set to assign the
PCI INTA signal to an otherwise unused system IRQ line.  While it's
physically possible to shared PCI interrupt lines, the kernel doesn't
support it. 

III. Driver operation

IIIa. Ring buffers
The Tulip can use either ring buffers or lists of Tx and Rx descriptors.
The current driver uses a statically allocated Rx ring of descriptors and
buffers, and a list of the Tx buffers.

IIIC. Synchronization
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

The send packet thread has partial control over the Tx ring and 'dev->tbusy'
flag.  It sets the tbusy flag whenever it's queuing a Tx packet. If the next
queue slot is empty, it clears the tbusy flag when finished otherwise it sets
the 'tp->tx_full' flag.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  (The Tx-done interrupt can't be selectively turned off, so
we can't avoid the interrupt overhead by having the Tx routine reap the Tx
stats.)	 After reaping the stats, it marks the queue entry as empty by setting
the 'base' to zero.	 Iff the 'tp->tx_full' flag is set, it clears both the
tx_full and tbusy flags.

IV. Notes

Thanks to Duke Kamstra of SMC for providing an EtherPower board.

The DEC databook doesn't document which Rx filter settings accept broadcast
packets.  Nor does it document how to configure the part to configure the
serial subsystem for normal (vs. loopback) operation or how to have it
autoswitch between internal 10baseT, SIA and AUI transceivers.

The databook claims that CSR13, CSR14, and CSR15 should each be the last
register of the set CSR12-15 written.   Hmmm, now how is that possible?
*/

#define DEC_VENDOR_ID	0x1011		/* Hex 'D' :-> */
#define DEC_21040_ID	0x0002		/* Change for 21140. */

/* Keep the ring sizes a power of two for efficiency. */
#define TX_RING_SIZE	4
#define RX_RING_SIZE	4
#define PKT_BUF_SZ		1536			/* Size of each temporary Rx buffer.*/

/* Offsets to the Command and Status Registers, "CSRs".  All accesses
   must be longword instructions and quadword aligned. */
enum tulip_offsets {
	CSR0=0,    CSR1=0x08, CSR2=0x10, CSR3=0x18, CSR4=0x20, CSR5=0x28,
	CSR6=0x30, CSR7=0x38, CSR8=0x40, CSR9=0x48, CSR10=0x50, CSR11=0x58,
	CSR12=0x60, CSR13=0x68, CSR14=0x70, CSR15=0x78 };

/* The Tulip Rx and Tx buffer descriptors. */
struct tulip_rx_desc {
	int status;
	int length;
	char *buffer1, *buffer2;			/* We use only buffer 1.  */
};

struct tulip_tx_desc {
	int status;
	int length;
	char *buffer1, *buffer2;			/* We use only buffer 1.  */
};

struct tulip_private {
	char devname[8];			/* Used only for kernel debugging. */
	struct tulip_rx_desc rx_ring[RX_RING_SIZE];
	struct tulip_tx_desc tx_ring[TX_RING_SIZE];
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	long rx_buffs;				/* Address of temporary Rx buffers. */
	struct enet_statistics stats;
	int setup_frame[48];		/* Pseudo-Tx frame to init address table. */
	unsigned int cur_rx, cur_tx;		/* The next free ring entry */
	unsigned int dirty_rx, dirty_tx;	/* The ring entries to be free()ed. */
	unsigned int tx_full:1;
	int pad0, pad1;						/* Used for 8-byte alignment */
};

static void tulip_probe1(int ioaddr, int irq);
static int tulip_open(struct device *dev);
static void tulip_init_ring(struct device *dev);
static int tulip_start_xmit(struct sk_buff *skb, struct device *dev);
static int tulip_rx(struct device *dev);
static void tulip_interrupt(int irq, struct pt_regs *regs);
static int tulip_close(struct device *dev);
static struct enet_statistics *tulip_get_stats(struct device *dev);
static void set_multicast_list(struct device *dev);
static int set_mac_address(struct device *dev, void *addr);



#ifndef MODULE
/* This 21040 probe is unlike most other board probes.  We can use memory
   efficiently by allocating a large contiguous region and dividing it
   ourselves.  This is done by having the initialization occur before
   the 'kmalloc()' memory management system is started. */

int dec21040_init(void)
{

    if (pcibios_present()) {
	    int pci_index;
		for (pci_index = 0; pci_index < 8; pci_index++) {
			unsigned char pci_bus, pci_device_fn, pci_irq_line;
			unsigned long pci_ioaddr;
		
			if (pcibios_find_device (DEC_VENDOR_ID, DEC_21040_ID, pci_index,
									 &pci_bus, &pci_device_fn) != 0)
				break;
			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_INTERRUPT_LINE, &pci_irq_line);
			pcibios_read_config_dword(pci_bus, pci_device_fn,
									  PCI_BASE_ADDRESS_0, &pci_ioaddr);
			/* Remove I/O space marker in bit 0. */
			pci_ioaddr &= ~3;
			if (tulip_debug > 2)
				printk("Found DEC PCI Tulip at I/O %#lx, IRQ %d.\n",
					   pci_ioaddr, pci_irq_line);
			tulip_probe1(pci_ioaddr, pci_irq_line);
		}
	}

	return 0;
}
#endif
#ifdef MODULE
static int tulip_probe(struct device *dev)
{
	printk("tulip: This driver does not yet install properly from module!\n");
	return -1;
}
#endif

static void tulip_probe1(int ioaddr, int irq)
{
	static int did_version = 0;			/* Already printed version info. */
	struct device *dev;
	struct tulip_private *tp;
	int i;

	if (tulip_debug > 0  &&  did_version++ == 0)
		printk(version);

	dev = init_etherdev(0, 0);

	printk("%s: DEC 21040 Tulip at %#3x,", dev->name, ioaddr);

	/* Stop the chip's Tx and Rx processes. */
	outl(inl(ioaddr + CSR6) & ~0x2002, ioaddr + CSR6);
	/* Clear the missed-packet counter. */
	inl(ioaddr + CSR8) & 0xffff;

	/* The station address ROM is read byte serially.  The register must
	   be polled, waiting for the value to be read bit serially from the
	   EEPROM.
	   */
	outl(0, ioaddr + CSR9);		/* Reset the pointer with a dummy write. */
	for (i = 0; i < 6; i++) {
		int value, boguscnt = 100000;
		do
			value = inl(ioaddr + CSR9);
		while (value < 0  && --boguscnt > 0);
		printk(" %2.2x", dev->dev_addr[i] = value);
	}
	printk(", IRQ %d\n", irq);

	/* We do a request_region() only to register /proc/ioports info. */
	request_region(ioaddr, TULIP_TOTAL_SIZE, "DEC Tulip Ethernet");

	dev->base_addr = ioaddr;
	dev->irq = irq;

	/* Make certain the data structures are quadword aligned. */
	tp = kmalloc(sizeof(*tp), GFP_KERNEL | GFP_DMA);
	dev->priv = tp;
	tp->rx_buffs = kmalloc(PKT_BUF_SZ*RX_RING_SIZE, GFP_KERNEL | GFP_DMA);

	/* The Tulip-specific entries in the device structure. */
	dev->open = &tulip_open;
	dev->hard_start_xmit = &tulip_start_xmit;
	dev->stop = &tulip_close;
	dev->get_stats = &tulip_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->set_mac_address = &set_mac_address;

	return;
}


static int
tulip_open(struct device *dev)
{
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	int ioaddr = dev->base_addr;

	/* Reset the chip, holding bit 0 set at least 10 PCI cycles. */
	outl(0xfff80001, ioaddr + CSR0);
	SLOW_DOWN_IO;
	/* Deassert reset.  Set 8 longword cache alignment, 8 longword burst.
	   Cache alignment bits 15:14	     Burst length 13:8
    	0000	No alignment  0x00000000 unlimited		0800 8 longwords
		4000	8  longwords		0100 1 longword		1000 16 longwords
		8000	16 longwords		0200 2 longwords	2000 32 longwords
		C000	32  longwords		0400 4 longwords
	   Wait the specified 50 PCI cycles after a reset by initializing
	   Tx and Rx queues and the address filter list. */
	outl(0xfff84800, ioaddr + CSR0);

	if (irq2dev_map[dev->irq] != NULL
		|| (irq2dev_map[dev->irq] = dev) == NULL
		|| dev->irq == 0
		|| request_irq(dev->irq, &tulip_interrupt, 0, "DEC 21040 Tulip")) {
		return -EAGAIN;
	}

	if (tulip_debug > 1)
		printk("%s: tulip_open() irq %d.\n", dev->name, dev->irq);

	tulip_init_ring(dev);

	/* Fill the whole address filter table with our physical address. */
	{ 
		unsigned short *eaddrs = (unsigned short *)dev->dev_addr;
		int *setup_frm = tp->setup_frame, i;

		/* You must add the broadcast address when doing perfect filtering! */
		*setup_frm++ = 0xffff;
		*setup_frm++ = 0xffff;
		*setup_frm++ = 0xffff;
		/* Fill the rest of the accept table with our physical address. */
		for (i = 1; i < 16; i++) {
			*setup_frm++ = eaddrs[0];
			*setup_frm++ = eaddrs[1];
			*setup_frm++ = eaddrs[2];
		}
		/* Put the setup frame on the Tx list. */
		tp->tx_ring[0].length = 0x08000000 | 192;
		tp->tx_ring[0].buffer1 = (char *)tp->setup_frame;
		tp->tx_ring[0].buffer2 = 0;
		tp->tx_ring[0].status = 0x80000000;

		tp->cur_tx++, tp->dirty_tx++;
	}

	outl((int)tp->rx_ring, ioaddr + CSR3);
	outl((int)tp->tx_ring, ioaddr + CSR4);

	/* Turn on the xcvr interface. */
	outl(0x00000000, ioaddr + CSR13);
	outl(0x00000004, ioaddr + CSR13);

	/* Start the chip's Tx and Rx processes. */
	outl(0xfffe2002, ioaddr + CSR6);

	/* Trigger an immediate transmit demand to process the setup frame. */
	outl(0, ioaddr + CSR1);

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	/* Enable interrupts by setting the interrupt mask. */
	outl(0xFFFFFFFF, ioaddr + CSR7);

	if (tulip_debug > 2) {
		printk("%s: Done tulip_open(), CSR0 %8.8x, CSR13 %8.8x.\n",
			   dev->name, inl(ioaddr + CSR0), inl(ioaddr + CSR13));
	}
	MOD_INC_USE_COUNT;
	return 0;
}

/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void
tulip_init_ring(struct device *dev)
{
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	int i;

	tp->tx_full = 0;
	tp->cur_rx = tp->cur_tx = 0;
	tp->dirty_rx = tp->dirty_tx = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		tp->rx_ring[i].status = 0x80000000;	/* Owned by Tulip chip */
		tp->rx_ring[i].length = PKT_BUF_SZ;
		tp->rx_ring[i].buffer1 = (char *)(tp->rx_buffs + i*PKT_BUF_SZ);
		tp->rx_ring[i].buffer2 = (char *)&tp->rx_ring[i+1];
	}
	/* Mark the last entry as wrapping the ring. */ 
	tp->rx_ring[i-1].length = PKT_BUF_SZ | 0x02000000;
	tp->rx_ring[i-1].buffer2 = (char *)&tp->rx_ring[0];

	/* The Tx buffer descriptor is filled in as needed, but we
	   do need to clear the ownership bit. */
	for (i = 0; i < TX_RING_SIZE; i++) {
		tp->tx_ring[i].status = 0x00000000;
	}
}

static int
tulip_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	int ioaddr = dev->base_addr;
	int entry;

	/* Transmitter timeout, serious problems. */
	if (dev->tbusy) {
		int tickssofar = jiffies - dev->trans_start;
		int i;
		if (tickssofar < 20)
			return 1;
		printk("%s: transmit timed out, status %8.8x, SIA %8.8x %8.8x %8.8x %8.8x, resetting...\n",
			   dev->name, inl(ioaddr + CSR5), inl(ioaddr + CSR12),
			   inl(ioaddr + CSR13), inl(ioaddr + CSR14), inl(ioaddr + CSR15));
		printk("  Rx ring %8.8x: ", (int)tp->rx_ring);
		for (i = 0; i < RX_RING_SIZE; i++)
			printk(" %8.8x", (unsigned int)tp->rx_ring[i].status);
		printk("\n  Tx ring %8.8x: ", (int)tp->tx_ring);
		for (i = 0; i < TX_RING_SIZE; i++)
			printk(" %8.8x", (unsigned int)tp->tx_ring[i].status);
		printk("\n");

		tp->stats.tx_errors++;
		/* We should reinitialize the hardware here. */
		dev->tbusy=0;
		dev->trans_start = jiffies;
		return 0;
	}

	if (skb == NULL || skb->len <= 0) {
		printk("%s: Obsolete driver layer request made: skbuff==NULL.\n",
			   dev->name);
		dev_tint(dev);
		return 0;
	}

	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well.
	   If this ever occurs the queue layer is doing something evil! */
	if (set_bit(0, (void*)&dev->tbusy) != 0) {
		printk("%s: Transmitter access conflict.\n", dev->name);
		return 1;
	}

	/* Caution: the write order is important here, set the base address
	   with the "ownership" bits last. */

	/* Calculate the next Tx descriptor entry. */
	entry = tp->cur_tx % TX_RING_SIZE;

	tp->tx_full = 1;
	tp->tx_skbuff[entry] = skb;
	tp->tx_ring[entry].length = skb->len |
		(entry == TX_RING_SIZE-1 ? 0xe2000000 : 0xe0000000);
	tp->tx_ring[entry].buffer1 = skb->data;
	tp->tx_ring[entry].buffer2 = 0;
	tp->tx_ring[entry].status = 0x80000000;	/* Pass ownership to the chip. */

	tp->cur_tx++;

	/* Trigger an immediate transmit demand. */
	outl(0, ioaddr + CSR1);

	dev->trans_start = jiffies;

	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void tulip_interrupt(int irq, struct pt_regs *regs)
{
	struct device *dev = (struct device *)(irq2dev_map[irq]);
	struct tulip_private *lp;
	int csr5, ioaddr, boguscnt=10;

	if (dev == NULL) {
		printk ("tulip_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	ioaddr = dev->base_addr;
	lp = (struct tulip_private *)dev->priv;
	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);

	dev->interrupt = 1;

	do {
		csr5 = inl(ioaddr + CSR5);
		/* Acknowledge all of the current interrupt sources ASAP. */
		outl(csr5 & 0x0001ffff, ioaddr + CSR5);

		if (tulip_debug > 4)
			printk("%s: interrupt  csr5=%#8.8x new csr5=%#8.8x.\n",
				   dev->name, csr5, inl(dev->base_addr + CSR5));

		if ((csr5 & 0x00018000) == 0)
			break;

		if (csr5 & 0x0040)			/* Rx interrupt */
			tulip_rx(dev);

		if (csr5 & 0x0001) {		/* Tx-done interrupt */
			int dirty_tx = lp->dirty_tx;

			while (dirty_tx < lp->cur_tx) {
				int entry = dirty_tx % TX_RING_SIZE;
				int status = lp->tx_ring[entry].status;

				if (status < 0)
					break;			/* It still hasn't been Txed */

				if (status & 0x8000) {
					/* There was an major error, log it. */
					lp->stats.tx_errors++;
					if (status & 0x4104) lp->stats.tx_aborted_errors++;
					if (status & 0x0C00) lp->stats.tx_carrier_errors++;
					if (status & 0x0200) lp->stats.tx_window_errors++;
					if (status & 0x0002) lp->stats.tx_fifo_errors++;
					if (status & 0x0080) lp->stats.tx_heartbeat_errors++;
#ifdef ETHER_STATS
					if (status & 0x0100) lp->stats.collisions16++;
#endif
				} else {
#ifdef ETHER_STATS
					if (status & 0x0001) lp->stats.tx_deferred++;
#endif
					lp->stats.collisions += (status >> 3) & 15;
					lp->stats.tx_packets++;
				}

				/* Free the original skb. */
				dev_kfree_skb(lp->tx_skbuff[entry], FREE_WRITE);
				dirty_tx++;
			}

#ifndef final_version
			if (lp->cur_tx - dirty_tx >= TX_RING_SIZE) {
				printk("out-of-sync dirty pointer, %d vs. %d, full=%d.\n",
					   dirty_tx, lp->cur_tx, lp->tx_full);
				dirty_tx += TX_RING_SIZE;
			}
#endif

			if (lp->tx_full && dev->tbusy
				&& dirty_tx > lp->cur_tx - TX_RING_SIZE + 2) {
				/* The ring is no longer full, clear tbusy. */
				lp->tx_full = 0;
				dev->tbusy = 0;
				mark_bh(NET_BH);
			}

			lp->dirty_tx = dirty_tx;
		}

		/* Log errors. */
		if (csr5 & 0x8000) {	/* Abnormal error summary bit. */
			if (csr5 & 0x0008) lp->stats.tx_errors++; /* Tx babble. */
			if (csr5 & 0x0100) { 		/* Missed a Rx frame. */
				lp->stats.rx_errors++;
				lp->stats.rx_missed_errors += inl(ioaddr + CSR8) & 0xffff;
			}
			if (csr5 & 0x0800) {
				printk("%s: Something Wicked happened! %8.8x.\n",
					   dev->name, csr5);
				/* Hmmmmm, it's not clear what to do here. */
			}
		}
		if (--boguscnt < 0) {
			printk("%s: Too much work at interrupt, csr5=0x%8.8x.\n",
				   dev->name, csr5);
			/* Clear all interrupt sources. */
			outl(0x0001ffff, ioaddr + CSR5);
			break;
		}
	} while (1);

	if (tulip_debug > 3)
		printk("%s: exiting interrupt, csr5=%#4.4x.\n",
			   dev->name, inl(ioaddr + CSR5));

	/* Special code for testing *only*. */
	{
		static int stopit = 10;
		if (dev->start == 0  &&  --stopit < 0) {
			printk("%s: Emergency stop, looping startup interrupt.\n",
				   dev->name);
			free_irq(irq);
		}
	}

	dev->interrupt = 0;
	return;
}

static int
tulip_rx(struct device *dev)
{
	struct tulip_private *lp = (struct tulip_private *)dev->priv;
	int entry = lp->cur_rx % RX_RING_SIZE;
	int i;
		
	if (tulip_debug > 4)
		printk(" In tulip_rx().\n");
	/* If we own the next entry, it's a new packet. Send it up. */
	while (lp->rx_ring[entry].status >= 0) {
		int status = lp->rx_ring[entry].status;

		if (tulip_debug > 4)
			printk("  tulip_rx() status was %8.8x.\n", status);
		if ((status & 0x0300) != 0x0300) {
			printk("%s: Ethernet frame spanned multiple buffers, status %8.8x!\n",
				   dev->name, status);
		} else if (status & 0x8000) {
			/* There was a fatal error. */
			lp->stats.rx_errors++; /* end of a packet.*/
			if (status & 0x0890) lp->stats.rx_length_errors++;
			if (status & 0x0004) lp->stats.rx_frame_errors++;
			if (status & 0x0002) lp->stats.rx_crc_errors++;
			if (status & 0x0001) lp->stats.rx_fifo_errors++;
		} else {
			/* Malloc up new buffer, compatible with net-2e. */
			short pkt_len = lp->rx_ring[entry].status >> 16;
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len+2);
			if (skb == NULL) {
				printk("%s: Memory squeeze, deferring packet.\n", dev->name);
				/* Check that at least two ring entries are free.
				   If not, free one and mark stats->rx_dropped++. */
				for (i=0; i < RX_RING_SIZE; i++)
					if (lp->rx_ring[(entry+i) % RX_RING_SIZE].status < 0)
						break;

				if (i > RX_RING_SIZE -2) {
					lp->stats.rx_dropped++;
					lp->rx_ring[entry].status = 0x80000000;
					lp->cur_rx++;
				}
				break;
			}
			skb->dev = dev;
			skb_reserve(skb,2);	/* 16 byte align the data fields */
			memcpy(skb_put(skb,pkt_len), lp->rx_ring[entry].buffer1, pkt_len);
			skb->protocol=eth_type_trans(skb,dev);
			netif_rx(skb);
			lp->stats.rx_packets++;
		}

		lp->rx_ring[entry].status = 0x80000000;
		entry = (++lp->cur_rx) % RX_RING_SIZE;
	}

	return 0;
}

static int
tulip_close(struct device *dev)
{
	int ioaddr = dev->base_addr;
	struct tulip_private *tp = (struct tulip_private *)dev->priv;

	dev->start = 0;
	dev->tbusy = 1;

	if (tulip_debug > 1)
		printk("%s: Shutting down ethercard, status was %2.2x.\n",
			   dev->name, inl(ioaddr + CSR5));

	/* Disable interrupts by clearing the interrupt mask. */
	outl(0x00000000, ioaddr + CSR7);
	/* Stop the chip's Tx and Rx processes. */
	outl(inl(ioaddr + CSR6) & ~0x2002, ioaddr + CSR6);

	tp->stats.rx_missed_errors += inl(ioaddr + CSR8) & 0xffff;

	free_irq(dev->irq);
	irq2dev_map[dev->irq] = 0;

	MOD_DEC_USE_COUNT;
	return 0;
}

static struct enet_statistics *
tulip_get_stats(struct device *dev)
{
	struct tulip_private *tp = (struct tulip_private *)dev->priv;
	short ioaddr = dev->base_addr;

	tp->stats.rx_missed_errors += inl(ioaddr + CSR8) & 0xffff;

	return &tp->stats;
}

/*
 *	Set or clear the multicast filter for this adaptor.
 */

static void set_multicast_list(struct device *dev)
{
	short ioaddr = dev->base_addr;
	int csr6 = inl(ioaddr + CSR6) & ~0x00D5;

	if (dev->flags&IFF_PROMISC) 
	{			/* Set promiscuous. */
		outl(csr6 | 0x00C0, ioaddr + CSR6);
		/* Log any net taps. */
		printk("%s: Promiscuous mode enabled.\n", dev->name);
	}
	else if (dev->mc_count > 15 || (dev->flags&IFF_ALLMULTI)) 
	{
		/* Too many to filter perfectly -- accept all multicasts. */
		outl(csr6 | 0x0080, ioaddr + CSR6);
	}
	else
	{
		struct tulip_private *tp = (struct tulip_private *)dev->priv;
		struct dev_mc_list *dmi=dev->mc_list;
		int *setup_frm = tp->setup_frame;
		unsigned short *eaddrs;
		int i;

		/* We have <= 15 addresses that we can use the wonderful
		   16 address perfect filtering of the Tulip.  Note that only
		   the low shortword of setup_frame[] is valid. */
		outl(csr6 | 0x0000, ioaddr + CSR6);
		i=0;
		while(dmi) 
		{
			eaddrs=(unsigned short *)dmi->dmi_addr;
			dmi=dmi->next;
			i++;
			*setup_frm++ = *eaddrs++;
			*setup_frm++ = *eaddrs++;
			*setup_frm++ = *eaddrs++;
		}
		/* Fill the rest of the table with our physical address. */
		eaddrs = (unsigned short *)dev->dev_addr;
		do {
			*setup_frm++ = eaddrs[0];
			*setup_frm++ = eaddrs[1];
			*setup_frm++ = eaddrs[2];
		} while (++i < 16);

		/* Now add this frame to the Tx list. */
	}
}

static int
set_mac_address(struct device *dev, void *addr)
{
	int i;
	struct sockaddr *sa=(struct sockaddr *)addr;
	if (dev->start)
		return -EBUSY;
	printk("%s: Setting MAC address to ", dev->name);
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = sa->sa_data[i]);
	printk(".\n");
	return 0;
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_tulip = {
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, tulip_probe
};

static int io = 0;
static int irq = 0;

int init_module(void)
{
	printk("tulip: Sorry, modularization is not completed\n");
	return -EIO;
#if 0
	if (io == 0)
	  printk("tulip: You should not use auto-probing with insmod!\n");
	dev_tulip.base_addr = io;
	dev_tulip.irq       = irq;
	if (register_netdev(&dev_tulip) != 0) {
		printk("tulip: register_netdev() returned non-zero.\n");
		return -EIO;
	}
	return 0;
#endif
}

void
cleanup_module(void)
{
	unregister_netdev(&dev_tulip);
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c tulip.c"
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
