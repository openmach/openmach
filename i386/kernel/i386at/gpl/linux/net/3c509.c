/* 3c509.c: A 3c509 EtherLink3 ethernet driver for linux. */
/*
	Written 1993,1994 by Donald Becker.

	Copyright 1994 by Donald Becker.
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.	 This software may be used and
	distributed according to the terms of the GNU Public License,
	incorporated herein by reference.

	This driver is for the 3Com EtherLinkIII series.

	The author may be reached as becker@cesdis.gsfc.nasa.gov or
	C/O Center of Excellence in Space Data and Information Sciences
		Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

	Known limitations:
	Because of the way 3c509 ISA detection works it's difficult to predict
	a priori which of several ISA-mode cards will be detected first.

	This driver does not use predictive interrupt mode, resulting in higher
	packet latency but lower overhead.  If interrupts are disabled for an
	unusually long time it could also result in missed packets, but in
	practice this rarely happens.
*/

static const  char *version = "3c509.c:1.03 10/8/94 becker@cesdis.gsfc.nasa.gov\n";

#include <linux/module.h>

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/config.h>	/* for CONFIG_MCA */

#include <asm/bitops.h>
#include <asm/io.h>


#ifdef EL3_DEBUG
int el3_debug = EL3_DEBUG;
#else
int el3_debug = 2;
#endif

/* To minimize the size of the driver source I only define operating
   constants if they are used several times.  You'll need the manual
   if you want to understand driver details. */
/* Offsets from base I/O address. */
#define EL3_DATA 0x00
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e
#define ID_PORT 0x100
#define	 EEPROM_READ 0x80

#define EL3_IO_EXTENT	16

#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)


/* The top five bits written to EL3_CMD are a command, the lower
   11 bits are the parameter, if applicable. */
enum c509cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11, RxDiscard = 8<<11,
	TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrMask = 14<<11,
	SetReadZero = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11,};

/* The SetRxFilter command accepts the following classes: */
enum RxFilter {
	RxStation = 1, RxMulticast = 2, RxBroadcast = 4, RxProm = 8 };

/* Register window 1 offsets, the window used in normal operation. */
#define TX_FIFO		0x00
#define RX_FIFO		0x00
#define RX_STATUS 	0x08
#define TX_STATUS 	0x0B
#define TX_FREE		0x0C		/* Remaining free bytes in Tx buffer. */

#define WN0_IRQ		0x08		/* Window 0: Set IRQ line in bits 12-15. */
#define WN4_MEDIA	0x0A		/* Window 4: Various transcvr/media bits. */
#define  MEDIA_TP	0x00C0		/* Enable link beat and jabber for 10baseT. */

struct el3_private {
	struct enet_statistics stats;
};

static ushort id_read_eeprom(int index);
static ushort read_eeprom(short ioaddr, int index);
static int el3_open(struct device *dev);
static int el3_start_xmit(struct sk_buff *skb, struct device *dev);
static void el3_interrupt(int irq, struct pt_regs *regs);
static void update_stats(int addr, struct device *dev);
static struct enet_statistics *el3_get_stats(struct device *dev);
static int el3_rx(struct device *dev);
static int el3_close(struct device *dev);
static void set_multicast_list(struct device *dev);



int el3_probe(struct device *dev)
{
	short lrs_state = 0xff, i;
	ushort ioaddr, irq, if_port;
	short *phys_addr = (short *)dev->dev_addr;
	static int current_tag = 0;

	/* First check all slots of the EISA bus.  The next slot address to
	   probe is kept in 'eisa_addr' to support multiple probe() calls. */
	if (EISA_bus) {
		static int eisa_addr = 0x1000;
		while (eisa_addr < 0x9000) {
			ioaddr = eisa_addr;
			eisa_addr += 0x1000;

			/* Check the standard EISA ID register for an encoded '3Com'. */
			if (inw(ioaddr + 0xC80) != 0x6d50)
				continue;

			/* Change the register set to the configuration window 0. */
			outw(SelectWindow | 0, ioaddr + 0xC80 + EL3_CMD);

			irq = inw(ioaddr + WN0_IRQ) >> 12;
			if_port = inw(ioaddr + 6)>>14;
			for (i = 0; i < 3; i++)
				phys_addr[i] = htons(read_eeprom(ioaddr, i));

			/* Restore the "Product ID" to the EEPROM read register. */
			read_eeprom(ioaddr, 3);

			/* Was the EISA code an add-on hack?  Nahhhhh... */
			goto found;
		}
	}

#ifdef CONFIG_MCA
	if (MCA_bus) {
		mca_adaptor_select_mode(1);
		for (i = 0; i < 8; i++)
			if ((mca_adaptor_id(i) | 1) == 0x627c) {
				ioaddr = mca_pos_base_addr(i);
				irq = inw(ioaddr + WN0_IRQ) >> 12;
				if_port = inw(ioaddr + 6)>>14;
				for (i = 0; i < 3; i++)
					phys_addr[i] = htons(read_eeprom(ioaddr, i));

				mca_adaptor_select_mode(0);
				goto found;
			}
		mca_adaptor_select_mode(0);

	}
#endif

	/* Next check for all ISA bus boards by sending the ID sequence to the
	   ID_PORT.  We find cards past the first by setting the 'current_tag'
	   on cards as they are found.  Cards with their tag set will not
	   respond to subsequent ID sequences. */

	if (check_region(ID_PORT,1)) {
	  static int once = 1;
	  if (once) printk("3c509: Somebody has reserved 0x%x, can't do ID_PORT lookup, nor card auto-probing\n",ID_PORT);
	  once = 0;
	  return -ENODEV;
	}

	outb(0x00, ID_PORT);
	outb(0x00, ID_PORT);
	for(i = 0; i < 255; i++) {
		outb(lrs_state, ID_PORT);
		lrs_state <<= 1;
		lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
	}

	/* For the first probe, clear all board's tag registers. */
	if (current_tag == 0)
		outb(0xd0, ID_PORT);
	else				/* Otherwise kill off already-found boards. */
		outb(0xd8, ID_PORT);

	if (id_read_eeprom(7) != 0x6d50) {
		return -ENODEV;
	}

	/* Read in EEPROM data, which does contention-select.
	   Only the lowest address board will stay "on-line".
	   3Com got the byte order backwards. */
	for (i = 0; i < 3; i++) {
		phys_addr[i] = htons(id_read_eeprom(i));
	}

	{
		unsigned short iobase = id_read_eeprom(8);
		if_port = iobase >> 14;
		ioaddr = 0x200 + ((iobase & 0x1f) << 4);
	}
	irq = id_read_eeprom(9) >> 12;

	if (dev->base_addr != 0
		&&	dev->base_addr != (unsigned short)ioaddr) {
		return -ENODEV;
	}

	/* Set the adaptor tag so that the next card can be found. */
	outb(0xd0 + ++current_tag, ID_PORT);

	/* Activate the adaptor at the EEPROM location. */
	outb(0xff, ID_PORT);

	EL3WINDOW(0);
	if (inw(ioaddr) != 0x6d50)
		return -ENODEV;

	/* Free the interrupt so that some other card can use it. */
	outw(0x0f00, ioaddr + WN0_IRQ);
 found:
	dev->base_addr = ioaddr;
	dev->irq = irq;
	dev->if_port = if_port;
	request_region(dev->base_addr, EL3_IO_EXTENT, "3c509");

	{
		const char *if_names[] = {"10baseT", "AUI", "undefined", "BNC"};
		printk("%s: 3c509 at %#3.3lx tag %d, %s port, address ",
			   dev->name, dev->base_addr, current_tag, if_names[dev->if_port]);
	}

	/* Read in the station address. */
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i]);
	printk(", IRQ %d.\n", dev->irq);

	/* Make up a EL3-specific-data structure. */
	dev->priv = kmalloc(sizeof(struct el3_private), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct el3_private));

	if (el3_debug > 0)
		printk(version);

	/* The EL3-specific entries in the device structure. */
	dev->open = &el3_open;
	dev->hard_start_xmit = &el3_start_xmit;
	dev->stop = &el3_close;
	dev->get_stats = &el3_get_stats;
	dev->set_multicast_list = &set_multicast_list;

	/* Fill in the generic fields of the device structure. */
	ether_setup(dev);
	return 0;
}

/* Read a word from the EEPROM using the regular EEPROM access register.
   Assume that we are in register window zero.
 */
static ushort read_eeprom(short ioaddr, int index)
{
	int timer;

	outw(EEPROM_READ + index, ioaddr + 10);
	/* Pause for at least 162 us. for the read to take place. */
	for (timer = 0; timer < 162*4 + 400; timer++)
		SLOW_DOWN_IO;
	return inw(ioaddr + 12);
}

/* Read a word from the EEPROM when in the ISA ID probe state. */
static ushort id_read_eeprom(int index)
{
	int timer, bit, word = 0;

	/* Issue read command, and pause for at least 162 us. for it to complete.
	   Assume extra-fast 16Mhz bus. */
	outb(EEPROM_READ + index, ID_PORT);

	/* This should really be done by looking at one of the timer channels. */
	for (timer = 0; timer < 162*4 + 400; timer++)
		SLOW_DOWN_IO;

	for (bit = 15; bit >= 0; bit--)
		word = (word << 1) + (inb(ID_PORT) & 0x01);

	if (el3_debug > 3)
		printk("  3c509 EEPROM word %d %#4.4x.\n", index, word);

	return word;
}



static int
el3_open(struct device *dev)
{
	int ioaddr = dev->base_addr;
	int i;

	outw(TxReset, ioaddr + EL3_CMD);
	outw(RxReset, ioaddr + EL3_CMD);
	outw(SetReadZero | 0x00, ioaddr + EL3_CMD);

	if (request_irq(dev->irq, &el3_interrupt, 0, "3c509")) {
		return -EAGAIN;
	}

	EL3WINDOW(0);
	if (el3_debug > 3)
		printk("%s: Opening, IRQ %d	 status@%x %4.4x.\n", dev->name,
			   dev->irq, ioaddr + EL3_STATUS, inw(ioaddr + EL3_STATUS));

	/* Activate board: this is probably unnecessary. */
	outw(0x0001, ioaddr + 4);

	irq2dev_map[dev->irq] = dev;

	/* Set the IRQ line. */
	outw((dev->irq << 12) | 0x0f00, ioaddr + WN0_IRQ);

	/* Set the station address in window 2 each time opened. */
	EL3WINDOW(2);

	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + i);

	if (dev->if_port == 3)
		/* Start the thinnet transceiver. We should really wait 50ms...*/
		outw(StartCoax, ioaddr + EL3_CMD);
	else if (dev->if_port == 0) {
		/* 10baseT interface, enabled link beat and jabber check. */
		EL3WINDOW(4);
		outw(inw(ioaddr + WN4_MEDIA) | MEDIA_TP, ioaddr + WN4_MEDIA);
	}

	/* Switch to the stats window, and clear all stats by reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	EL3WINDOW(6);
	for (i = 0; i < 9; i++)
		inb(ioaddr + i);
	inb(ioaddr + 10);
	inb(ioaddr + 12);

	/* Switch to register set 1 for normal use. */
	EL3WINDOW(1);

	/* Accept b-case and phys addr only. */
	outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
	outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

	dev->interrupt = 0;
	dev->tbusy = 0;
	dev->start = 1;

	outw(RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
	outw(TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */
	/* Allow status bits to be seen. */
	outw(SetReadZero | 0xff, ioaddr + EL3_CMD);
	outw(AckIntr | 0x69, ioaddr + EL3_CMD); /* Ack IRQ */
	outw(SetIntrMask | 0x98, ioaddr + EL3_CMD); /* Set interrupt mask. */

	if (el3_debug > 3)
		printk("%s: Opened 3c509  IRQ %d  status %4.4x.\n",
			   dev->name, dev->irq, inw(ioaddr + EL3_STATUS));

	MOD_INC_USE_COUNT;
	return 0;					/* Always succeed */
}

static int
el3_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;

	/* Transmitter timeout, serious problems. */
	if (dev->tbusy) {
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < 10)
			return 1;
		printk("%s: transmit timed out, tx_status %2.2x status %4.4x.\n",
			   dev->name, inb(ioaddr + TX_STATUS), inw(ioaddr + EL3_STATUS));
		dev->trans_start = jiffies;
		/* Issue TX_RESET and TX_START commands. */
		outw(TxReset, ioaddr + EL3_CMD);
		outw(TxEnable, ioaddr + EL3_CMD);
		dev->tbusy = 0;
	}

	if (skb == NULL) {
		dev_tint(dev);
		return 0;
	}

	if (skb->len <= 0)
		return 0;

	if (el3_debug > 4) {
		printk("%s: el3_start_xmit(length = %ld) called, status %4.4x.\n",
			   dev->name, skb->len, inw(ioaddr + EL3_STATUS));
	}
#ifndef final_version
	{	/* Error-checking code, delete for 1.30. */
		ushort status = inw(ioaddr + EL3_STATUS);
		if (status & 0x0001 		/* IRQ line active, missed one. */
			&& inw(ioaddr + EL3_STATUS) & 1) { 			/* Make sure. */
			printk("%s: Missed interrupt, status then %04x now %04x"
				   "  Tx %2.2x Rx %4.4x.\n", dev->name, status,
				   inw(ioaddr + EL3_STATUS), inb(ioaddr + TX_STATUS),
				   inw(ioaddr + RX_STATUS));
			/* Fake interrupt trigger by masking, acknowledge interrupts. */
			outw(SetReadZero | 0x00, ioaddr + EL3_CMD);
			outw(AckIntr | 0x69, ioaddr + EL3_CMD); /* Ack IRQ */
			outw(SetReadZero | 0xff, ioaddr + EL3_CMD);
		}
	}
#endif

	/* Avoid timer-based retransmission conflicts. */
	if (set_bit(0, (void*)&dev->tbusy) != 0)
		printk("%s: Transmitter access conflict.\n", dev->name);
	else {
		/* Put out the doubleword header... */
		outw(skb->len, ioaddr + TX_FIFO);
		outw(0x00, ioaddr + TX_FIFO);
		/* ... and the packet rounded to a doubleword. */
		outsl(ioaddr + TX_FIFO, skb->data, (skb->len + 3) >> 2);

		dev->trans_start = jiffies;
		if (inw(ioaddr + TX_FREE) > 1536) {
			dev->tbusy = 0;
		} else
			/* Interrupt us when the FIFO has room for max-sized packet. */
			outw(SetTxThreshold + 1536, ioaddr + EL3_CMD);
	}

	dev_kfree_skb (skb, FREE_WRITE);

	/* Clear the Tx status stack. */
	{
		short tx_status;
		int i = 4;

		while (--i > 0	&&	(tx_status = inb(ioaddr + TX_STATUS)) > 0) {
			if (tx_status & 0x38) lp->stats.tx_aborted_errors++;
			if (tx_status & 0x30) outw(TxReset, ioaddr + EL3_CMD);
			if (tx_status & 0x3C) outw(TxEnable, ioaddr + EL3_CMD);
			outb(0x00, ioaddr + TX_STATUS); /* Pop the status stack. */
		}
	}
	return 0;
}

/* The EL3 interrupt handler. */
static void
el3_interrupt(int irq, struct pt_regs *regs)
{
	struct device *dev = (struct device *)(irq2dev_map[irq]);
	int ioaddr, status;
	int i = 0;

	if (dev == NULL) {
		printk ("el3_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);
	dev->interrupt = 1;

	ioaddr = dev->base_addr;
	status = inw(ioaddr + EL3_STATUS);

	if (el3_debug > 4)
		printk("%s: interrupt, status %4.4x.\n", dev->name, status);

	while ((status = inw(ioaddr + EL3_STATUS)) & 0x91) {

		if (status & 0x10)
			el3_rx(dev);

		if (status & 0x08) {
			if (el3_debug > 5)
				printk("	TX room bit was handled.\n");
			/* There's room in the FIFO for a full-sized packet. */
			outw(AckIntr | 0x08, ioaddr + EL3_CMD);
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}
		if (status & 0x80)				/* Statistics full. */
			update_stats(ioaddr, dev);

		if (++i > 10) {
			printk("%s: Infinite loop in interrupt, status %4.4x.\n",
				   dev->name, status);
			/* Clear all interrupts. */
			outw(AckIntr | 0xFF, ioaddr + EL3_CMD);
			break;
		}
		/* Acknowledge the IRQ. */
		outw(AckIntr | 0x41, ioaddr + EL3_CMD); /* Ack IRQ */

	}

	if (el3_debug > 4) {
		printk("%s: exiting interrupt, status %4.4x.\n", dev->name,
			   inw(ioaddr + EL3_STATUS));
	}

	dev->interrupt = 0;
	return;
}


static struct enet_statistics *
el3_get_stats(struct device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	unsigned long flags;

	save_flags(flags);
	cli();
	update_stats(dev->base_addr, dev);
	restore_flags(flags);
	return &lp->stats;
}

/*  Update statistics.  We change to register window 6, so this should be run
	single-threaded if the device is active. This is expected to be a rare
	operation, and it's simpler for the rest of the driver to assume that
	window 1 is always valid rather than use a special window-state variable.
	*/
static void update_stats(int ioaddr, struct device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;

	if (el3_debug > 5)
		printk("   Updating the statistics.\n");
	/* Turn off statistics updates while reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	/* Switch to the stats window, and read everything. */
	EL3WINDOW(6);
	lp->stats.tx_carrier_errors 	+= inb(ioaddr + 0);
	lp->stats.tx_heartbeat_errors	+= inb(ioaddr + 1);
	/* Multiple collisions. */	   	inb(ioaddr + 2);
	lp->stats.collisions			+= inb(ioaddr + 3);
	lp->stats.tx_window_errors		+= inb(ioaddr + 4);
	lp->stats.rx_fifo_errors		+= inb(ioaddr + 5);
	lp->stats.tx_packets			+= inb(ioaddr + 6);
	/* Rx packets	*/				inb(ioaddr + 7);
	/* Tx deferrals */				inb(ioaddr + 8);
	inw(ioaddr + 10);	/* Total Rx and Tx octets. */
	inw(ioaddr + 12);

	/* Back to window 1, and turn statistics back on. */
	EL3WINDOW(1);
	outw(StatsEnable, ioaddr + EL3_CMD);
	return;
}

static int
el3_rx(struct device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;
	short rx_status;

	if (el3_debug > 5)
		printk("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
			   inw(ioaddr+EL3_STATUS), inw(ioaddr+RX_STATUS));
	while ((rx_status = inw(ioaddr + RX_STATUS)) > 0) {
		if (rx_status & 0x4000) { /* Error, update stats. */
			short error = rx_status & 0x3800;
			lp->stats.rx_errors++;
			switch (error) {
			case 0x0000:		lp->stats.rx_over_errors++; break;
			case 0x0800:		lp->stats.rx_length_errors++; break;
			case 0x1000:		lp->stats.rx_frame_errors++; break;
			case 0x1800:		lp->stats.rx_length_errors++; break;
			case 0x2000:		lp->stats.rx_frame_errors++; break;
			case 0x2800:		lp->stats.rx_crc_errors++; break;
			}
		} else {
			short pkt_len = rx_status & 0x7ff;
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len+5);
			if (el3_debug > 4)
				printk("Receiving packet size %d status %4.4x.\n",
					   pkt_len, rx_status);
			if (skb != NULL) {
				skb->dev = dev;
				skb_reserve(skb,2);	/* Align IP on 16 byte boundaries */

				/* 'skb->data' points to the start of sk_buff data area. */
				insl(ioaddr+RX_FIFO, skb_put(skb,pkt_len),
							(pkt_len + 3) >> 2);

				skb->protocol=eth_type_trans(skb,dev);
				netif_rx(skb);
				outw(RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */
				lp->stats.rx_packets++;
				continue;
			} else if (el3_debug)
				printk("%s: Couldn't allocate a sk_buff of size %d.\n",
					   dev->name, pkt_len);
		}
		lp->stats.rx_dropped++;
		outw(RxDiscard, ioaddr + EL3_CMD);
		while (inw(ioaddr + EL3_STATUS) & 0x1000)
			printk("	Waiting for 3c509 to discard packet, status %x.\n",
				   inw(ioaddr + EL3_STATUS) );
	}

	return 0;
}

/* 
 *	Set or clear the multicast filter for this adaptor.
 */
 
static void set_multicast_list(struct device *dev)
{
	short ioaddr = dev->base_addr;
	if (el3_debug > 1) {
		static int old = 0;
		if (old != dev->mc_count) {
			old = dev->mc_count;
			printk("%s: Setting Rx mode to %d addresses.\n", dev->name, dev->mc_count);
		}
	}
	if (dev->flags&IFF_PROMISC) 
	{
		outw(SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm,
			 ioaddr + EL3_CMD);
	}
	else if (dev->mc_count || (dev->flags&IFF_ALLMULTI)) 
	{
		outw(SetRxFilter|RxStation|RxMulticast|RxBroadcast, ioaddr + EL3_CMD);
	} 
	else
		outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
}

static int
el3_close(struct device *dev)
{
	int ioaddr = dev->base_addr;

	if (el3_debug > 2)
		printk("%s: Shutting down ethercard.\n", dev->name);

	dev->tbusy = 1;
	dev->start = 0;

	/* Turn off statistics ASAP.  We update lp->stats below. */
	outw(StatsDisable, ioaddr + EL3_CMD);

	/* Disable the receiver and transmitter. */
	outw(RxDisable, ioaddr + EL3_CMD);
	outw(TxDisable, ioaddr + EL3_CMD);

	if (dev->if_port == 3)
		/* Turn off thinnet power.  Green! */
		outw(StopCoax, ioaddr + EL3_CMD);
	else if (dev->if_port == 0) {
		/* Disable link beat and jabber, if_port may change ere next open(). */
		EL3WINDOW(4);
		outw(inw(ioaddr + WN4_MEDIA) & ~MEDIA_TP, ioaddr + WN4_MEDIA);
	}

	free_irq(dev->irq);
	/* Switching back to window 0 disables the IRQ. */
	EL3WINDOW(0);
	/* But we explicitly zero the IRQ line select anyway. */
	outw(0x0f00, ioaddr + WN0_IRQ);


	irq2dev_map[dev->irq] = 0;

	update_stats(ioaddr, dev);
	MOD_DEC_USE_COUNT;
	return 0;
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_3c509 = {
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, el3_probe };

static int io = 0;
static int irq = 0;

int
init_module(void)
{
	dev_3c509.base_addr = io;
	dev_3c509.irq       = irq;
	if (!EISA_bus) {
		printk("3c509: WARNING! Module load-time probing works reliably only for EISA-bus!\n");
	}
	if (register_netdev(&dev_3c509) != 0)
		return -EIO;
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(&dev_3c509);
	kfree_s(dev_3c509.priv,sizeof(struct el3_private));
	dev_3c509.priv=NULL;
	/* If we don't do this, we can't re-insmod it later. */
	release_region(dev_3c509.base_addr, EL3_IO_EXTENT);
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c 3c509.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 * End:
 */
