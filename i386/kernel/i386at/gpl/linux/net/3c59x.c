/* 3c59x.c: A 3Com 3c590/3c595 "Vortex" ethernet driver for linux. */
/*
	Written 1995 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	This driver is for the 3Com "Vortex" series ethercards.  Members of
	the series include the 3c590 PCI EtherLink III and 3c595-Tx PCI Fast
	EtherLink.  It also works with the 10Mbs-only 3c590 PCI EtherLink III.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
*/

static char *version = "3c59x.c:v0.13 2/13/96 becker@cesdis.gsfc.nasa.gov\n";

/* "Knobs" that turn on special features. */
/* Allow the use of bus master transfers instead of programmed-I/O for the
   Tx process.  Bus master transfers are always disabled by default, but
   iff this is set they may be turned on using 'options'. */
#define VORTEX_BUS_MASTER

/* Put out somewhat more debugging messages. (0 - no msg, 1 minimal msgs). */
#define VORTEX_DEBUG 1

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/bios32.h>
#include <linux/timer.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#ifdef HAVE_SHARED_IRQ
#define USE_SHARED_IRQ
#include <linux/shared_irq.h>
#endif

/* The total size is twice that of the original EtherLinkIII series: the
   runtime register window, window 1, is now always mapped in. */
#define VORTEX_TOTAL_SIZE 0x20

#ifdef HAVE_DEVLIST
struct netdev_entry tc59x_drv =
{"Vortex", vortex_pci_probe, VORTEX_TOTAL_SIZE, NULL};
#endif

#ifdef VORTEX_DEBUG
int vortex_debug = VORTEX_DEBUG;
#else
int vortex_debug = 1;
#endif

static int product_ids[] = {0x5900, 0x5950, 0x5951, 0x5952, 0, 0};
static const char *product_names[] = {
	"3c590 Vortex 10Mbps",
	"3c595 Vortex 100baseTX",
	"3c595 Vortex 100baseT4",
	"3c595 Vortex 100base-MII",
	"EISA Vortex 3c597",
};
#define DEMON_INDEX 5			/* Caution!  Must be consistent with above! */

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the 3Com FastEtherLink, 3Com's PCI to
10/100baseT adapter.  It also works with the 3c590, a similar product
with only a 10Mbs interface.

II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS should be set to assign the
PCI INTA signal to an otherwise unused system IRQ line.  While it's
physically possible to shared PCI interrupt lines, the 1.2.0 kernel doesn't
support it.

III. Driver operation

The 3c59x series use an interface that's very similar to the previous 3c5x9
series.  The primary interface is two programmed-I/O FIFOs, with an
alternate single-contiguous-region bus-master transfer (see next).

One extension that is advertised in a very large font is that the adapters
are capable of being bus masters.  Unfortunately this capability is only for
a single contiguous region making it less useful than the list of transfer
regions available with the DEC Tulip or AMD PCnet.  Given the significant
performance impact of taking an extra interrupt for each transfer, using
DMA transfers is a win only with large blocks.

IIIC. Synchronization
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

IV. Notes

Thanks to Cameron Spitzer and Terry Murphy of 3Com for providing both
3c590 and 3c595 boards.
The name "Vortex" is the internal 3Com project name for the PCI ASIC, and
the not-yet-released (3/95) EISA version is called "Demon".  According to
Terry these names come from rides at the local amusement park.

The new chips support both ethernet (1.5K) and FDDI (4.5K) packet sizes!
This driver only supports ethernet packets because of the skbuff allocation
limit of 4K.
*/

#define TCOM_VENDOR_ID	0x10B7		/* 3Com's manufacturer's ID. */

/* Operational defintions.
   These are not used by other compilation units and thus are not
   exported in a ".h" file.

   First the windows.  There are eight register windows, with the command
   and status registers available in each.
   */
#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e

/* The top five bits written to EL3_CMD are a command, the lower
   11 bits are the parameter, if applicable.
   Note that 11 parameters bits was fine for ethernet, but the new chip
   can handle FDDI lenght frames (~4500 octets) and now parameters count
   32-bit 'Dwords' rather than octets. */

enum vortex_cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11, RxDiscard = 8<<11,
	TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11,
	StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11,};

/* The SetRxFilter command accepts the following classes: */
enum RxFilter {
	RxStation = 1, RxMulticast = 2, RxBroadcast = 4, RxProm = 8 };

/* Bits in the general status register. */
enum vortex_status {
	IntLatch = 0x0001, AdapterFailure = 0x0002, TxComplete = 0x0004,
	TxAvailable = 0x0008, RxComplete = 0x0010, RxEarly = 0x0020,
	IntReq = 0x0040, StatsFull = 0x0080, DMADone = 1<<8,
	DMAInProgress = 1<<11,			/* DMA controller is still busy.*/
	CmdInProgress = 1<<12,			/* EL3_CMD is still busy.*/
};

/* Register window 1 offsets, the window used in normal operation.
   On the Vortex this window is always mapped at offsets 0x10-0x1f. */
enum Window1 {
	TX_FIFO = 0x10,  RX_FIFO = 0x10,  RxErrors = 0x14,
	RxStatus = 0x18,  Timer=0x1A, TxStatus = 0x1B,
	TxFree = 0x1C, /* Remaining free bytes in Tx buffer. */
};
enum Window0 {
	Wn0EepromCmd = 10,		/* Window 0: EEPROM command register. */
};
enum Win0_EEPROM_bits {
	EEPROM_Read = 0x80, EEPROM_WRITE = 0x40, EEPROM_ERASE = 0xC0,
	EEPROM_EWENB = 0x30,		/* Enable erasing/writing for 10 msec. */
	EEPROM_EWDIS = 0x00,		/* Disable EWENB before 10 msec timeout. */
};
/* EEPROM locations. */
enum eeprom_offset {
	PhysAddr01=0, PhysAddr23=1, PhysAddr45=2, ModelID=3,
	EtherLink3ID=7, IFXcvrIO=8, IRQLine=9,
	NodeAddr01=10, NodeAddr23=11, NodeAddr45=12,
	DriverTune=13, Checksum=15};

enum Window3 {			/* Window 3: MAC/config bits. */
	Wn3_Config=0, Wn3_MAC_Ctrl=6, Wn3_Options=8,
};
union wn3_config {
	int i;
	struct w3_config_fields {
		unsigned int ram_size:3, ram_width:1, ram_speed:2, rom_size:2;
		int pad8:8;
		unsigned int ram_split:2, pad18:2, xcvr:3, pad21:1, autoselect:1;
		int pad24:8;
	} u;
};

enum Window4 {
	Wn4_Media = 0x0A,		/* Window 4: Various transcvr/media bits. */
};
enum Win4_Media_bits {
	Media_TP = 0x00C0,		/* Enable link beat and jabber for 10baseT. */
};
enum Window7 {					/* Window 7: Bus Master control. */
	Wn7_MasterAddr = 0, Wn7_MasterLen = 6, Wn7_MasterStatus = 12,
};

struct vortex_private {
	char devname[8];			/* "ethN" string, also for kernel debug. */
	const char *product_name;
	struct device *next_module;
	struct enet_statistics stats;
#ifdef VORTEX_BUS_MASTER
	struct sk_buff *tx_skb;		/* Packet being eaten by bus master ctrl.  */
#endif
	struct timer_list timer;	/* Media selection timer. */
	int options;				/* User-settable driver options (none yet). */
	unsigned int media_override:3, full_duplex:1, bus_master:1, autoselect:1;
};

static char *if_names[] = {
	"10baseT", "10Mbs AUI", "undefined", "10base2",
	"100baseTX", "100baseFX", "MII", "undefined"};

static int vortex_scan(struct device *dev);
static int vortex_found_device(struct device *dev, int ioaddr, int irq,
							   int product_index, int options);
static int vortex_probe1(struct device *dev);
static int vortex_open(struct device *dev);
static void vortex_timer(unsigned long arg);
static int vortex_start_xmit(struct sk_buff *skb, struct device *dev);
static int vortex_rx(struct device *dev);
static void vortex_interrupt(int irq, struct pt_regs *regs);
static int vortex_close(struct device *dev);
static void update_stats(int addr, struct device *dev);
static struct enet_statistics *vortex_get_stats(struct device *dev);
static void set_multicast_list(struct device *dev);


/* Unlike the other PCI cards the 59x cards don't need a large contiguous
   memory region, so making the driver a loadable module is feasible.

   Unfortuneately maximizing the shared code between the integrated and
   module version of the driver results in a complicated set of initialization
   procedures.
   init_module() -- modules /  tc59x_init()  -- built-in
		The wrappers for vortex_scan()
   vortex_scan()  		 The common routine that scans for PCI and EISA cards
   vortex_found_device() Allocate a device structure when we find a card.
					Different versions exist for modules and built-in.
   vortex_probe1()		Fill in the device structure -- this is seperated
					so that the modules code can put it in dev->init.
*/
/* This driver uses 'options' to pass the media type, full-duplex flag, etc. */
/* Note: this is the only limit on the number of cards supported!! */
int options[8] = { -1, -1, -1, -1, -1, -1, -1, -1,};

#ifdef MODULE
static int debug = -1;
/* A list of all installed Vortex devices, for removing the driver module. */
static struct device *root_vortex_dev = NULL;

int
init_module(void)
{
	int cards_found;

	if (debug >= 0)
		vortex_debug = debug;
	if (vortex_debug)
		printk(version);

	root_vortex_dev = NULL;
	cards_found = vortex_scan(0);
	return cards_found < 0 ? cards_found : 0;
}

#else
unsigned long tc59x_probe(struct device *dev)
{
	int cards_found = 0;

	cards_found = vortex_scan(dev);

	if (vortex_debug > 0  &&  cards_found)
		printk(version);

	return cards_found ? 0 : -ENODEV;
}
#endif  /* not MODULE */

static int vortex_scan(struct device *dev)
{
	int cards_found = 0;

	if (pcibios_present()) {
		static int pci_index = 0;
		for (; pci_index < 8; pci_index++) {
			unsigned char pci_bus, pci_device_fn, pci_irq_line, pci_latency;
			unsigned int pci_ioaddr;
			unsigned short pci_command;
			int index;

			for (index = 0; product_ids[index]; index++) {
				if ( ! pcibios_find_device(TCOM_VENDOR_ID, product_ids[index],
										   pci_index, &pci_bus,
										   &pci_device_fn))
					break;
			}
			if ( ! product_ids[index])
				break;

			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_INTERRUPT_LINE, &pci_irq_line);
			pcibios_read_config_dword(pci_bus, pci_device_fn,
									  PCI_BASE_ADDRESS_0, &pci_ioaddr);
			/* Remove I/O space marker in bit 0. */
			pci_ioaddr &= ~3;

#ifdef VORTEX_BUS_MASTER
			/* Get and check the bus-master and latency values.
			   Some PCI BIOSes fail to set the master-enable bit, and
			   the latency timer must be set to the maximum value to avoid
			   data corruption that occurs when the timer expires during
			   a transfer.  Yes, it's a bug. */
			pcibios_read_config_word(pci_bus, pci_device_fn,
									 PCI_COMMAND, &pci_command);
			if ( ! (pci_command & PCI_COMMAND_MASTER)) {
				printk("  PCI Master Bit has not been set! Setting...\n");
				pci_command |= PCI_COMMAND_MASTER;
				pcibios_write_config_word(pci_bus, pci_device_fn,
										  PCI_COMMAND, pci_command);
			}
			pcibios_read_config_byte(pci_bus, pci_device_fn,
										 PCI_LATENCY_TIMER, &pci_latency);
			if (pci_latency != 255) {
				printk("  Overriding PCI latency timer (CFLT) setting of %d, new value is 255.\n", pci_latency);
				pcibios_write_config_byte(pci_bus, pci_device_fn,
										  PCI_LATENCY_TIMER, 255);
			}
#endif  /* VORTEX_BUS_MASTER */
			vortex_found_device(dev, pci_ioaddr, pci_irq_line, index,
								dev && dev->mem_start ? dev->mem_start
								: options[cards_found]);
			dev = 0;
			cards_found++;
		}
	}

	/* Now check all slots of the EISA bus. */
	if (EISA_bus) {
		static int ioaddr = 0x1000;
		for (ioaddr = 0x1000; ioaddr < 0x9000; ioaddr += 0x1000) {
			/* Check the standard EISA ID register for an encoded '3Com'. */
			if (inw(ioaddr + 0xC80) != 0x6d50)
				continue;
			/* Check for a product that we support. */
			if ((inw(ioaddr + 0xC82) & 0xFFF0) != 0x5970
				&& (inw(ioaddr + 0xC82) & 0xFFF0) != 0x5920)
				continue;
			vortex_found_device(dev, ioaddr, inw(ioaddr + 0xC88) >> 12,
								DEMON_INDEX,  dev && dev->mem_start
								? dev->mem_start : options[cards_found]);
			dev = 0;
			cards_found++;
		}
	}

	return cards_found;
}

static int vortex_found_device(struct device *dev, int ioaddr, int irq,
							   int product_index, int options)
{
	struct vortex_private *vp;

#ifdef MODULE
	/* Allocate and fill new device structure. */
	int dev_size = sizeof(struct device) +
		sizeof(struct vortex_private);
	
	dev = (struct device *) kmalloc(dev_size, GFP_KERNEL);
	memset(dev, 0, dev_size);
	dev->priv = ((void *)dev) + sizeof(struct device);
	vp = (struct vortex_private *)dev->priv;
	dev->name = vp->devname; /* An empty string. */
	dev->base_addr = ioaddr;
	dev->irq = irq;
	dev->init = vortex_probe1;
	vp->product_name = product_names[product_index];
	vp->options = options;
	if (options >= 0) {
		vp->media_override = ((options & 7) == 2)  ?  0  :  options & 7;
		vp->full_duplex = (options & 8) ? 1 : 0;
		vp->bus_master = (options & 16) ? 1 : 0;
	} else {
		vp->media_override = 7;
		vp->full_duplex = 0;
		vp->bus_master = 0;
	}
	ether_setup(dev);
	vp->next_module = root_vortex_dev;
	root_vortex_dev = dev;
	if (register_netdev(dev) != 0)
		return -EIO;
#else  /* not a MODULE */
	if (dev) {
		dev->priv = kmalloc(sizeof (struct vortex_private), GFP_KERNEL);
		memset(dev->priv, 0, sizeof (struct vortex_private));
	}
	dev = init_etherdev(dev, sizeof(struct vortex_private));
	dev->base_addr = ioaddr;
	dev->irq = irq;
	vp  = (struct vortex_private *)dev->priv;
	vp->product_name = product_names[product_index];
	vp->options = options;
	if (options >= 0) {
		vp->media_override = ((options & 7) == 2)  ?  0  :  options & 7;
		vp->full_duplex = (options & 8) ? 1 : 0;
		vp->bus_master = (options & 16) ? 1 : 0;
	} else {
		vp->media_override = 7;
		vp->full_duplex = 0;
		vp->bus_master = 0;
	}

	vortex_probe1(dev);
#endif /* MODULE */
	return 0;
}

static int vortex_probe1(struct device *dev)
{
	int ioaddr = dev->base_addr;
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	int i;

	printk("%s: 3Com %s at %#3x,", dev->name,
		   vp->product_name, ioaddr);

	/* Read the station address from the EEPROM. */
	EL3WINDOW(0);
	for (i = 0; i < 3; i++) {
		short *phys_addr = (short *)dev->dev_addr;
		int timer;
		outw(EEPROM_Read + PhysAddr01 + i, ioaddr + Wn0EepromCmd);
		/* Pause for at least 162 us. for the read to take place. */
		for (timer = 0; timer < 162*4 + 400; timer++) {
			SLOW_DOWN_IO;
			if ((inw(ioaddr + Wn0EepromCmd) & 0x8000) == 0)
				break;
		}
		phys_addr[i] = htons(inw(ioaddr + 12));
	}
	for (i = 0; i < 6; i++)
		printk("%c%2.2x", i ? ':' : ' ', dev->dev_addr[i]);
	printk(", IRQ %d\n", dev->irq);
	/* Tell them about an invalid IRQ. */
	if (vortex_debug && (dev->irq <= 0 || dev->irq > 15))
		printk(" *** Warning: this IRQ is unlikely to work!\n");

	{
		char *ram_split[] = {"5:3", "3:1", "1:1", "invalid"};
		union wn3_config config;
		EL3WINDOW(3);
		config.i = inl(ioaddr + Wn3_Config);
		if (vortex_debug > 1)
			printk("  Internal config register is %4.4x, transceivers %#x.\n",
				   config.i, inw(ioaddr + Wn3_Options));
		printk("  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
			   8 << config.u.ram_size,
			   config.u.ram_width ? "word" : "byte",
			   ram_split[config.u.ram_split],
			   config.u.autoselect ? "autoselect/" : "",
			   if_names[config.u.xcvr]);
		dev->if_port = config.u.xcvr;
		vp->autoselect = config.u.autoselect;
	}

	/* We do a request_region() only to register /proc/ioports info. */
	request_region(ioaddr, VORTEX_TOTAL_SIZE, vp->product_name);

	/* The 3c59x-specific entries in the device structure. */
	dev->open = &vortex_open;
	dev->hard_start_xmit = &vortex_start_xmit;
	dev->stop = &vortex_close;
	dev->get_stats = &vortex_get_stats;
	dev->set_multicast_list = &set_multicast_list;
#if defined (HAVE_SET_MAC_ADDR) && 0
	dev->set_mac_address = &set_mac_address;
#endif

	return 0;
}


static int
vortex_open(struct device *dev)
{
	int ioaddr = dev->base_addr;
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	union wn3_config config;
	int i;

	/* Before initializing select the active media port. */
	EL3WINDOW(3);
	if (vp->full_duplex)
		outb(0x20, ioaddr + Wn3_MAC_Ctrl); /* Set the full-duplex bit. */
	config.i = inl(ioaddr + Wn3_Config);

	if (vp->media_override != 7) {
		if (vortex_debug > 1)
			printk("%s: Media override to transceiver %d (%s).\n",
				   dev->name, vp->media_override, if_names[vp->media_override]);
		config.u.xcvr = vp->media_override;
		dev->if_port = vp->media_override;
		outl(config.i, ioaddr + Wn3_Config);
	}

	if (vortex_debug > 1) {
		printk("%s: vortex_open() InternalConfig %8.8x.\n",
			dev->name, config.i);
	}

	outw(TxReset, ioaddr + EL3_CMD);
	for (i = 20; i >= 0 ; i--)
		if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
			break;

	outw(RxReset, ioaddr + EL3_CMD);
	/* Wait a few ticks for the RxReset command to complete. */
	for (i = 20; i >= 0 ; i--)
		if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
			break;

	outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD);

#ifdef USE_SHARED_IRQ
	i = request_shared_irq(dev->irq, &vortex_interrupt, dev, vp->product_name);
	if (i)						/* Error */
		return i;
#else
	if (dev->irq == 0  ||  irq2dev_map[dev->irq] != NULL)
		return -EAGAIN;
	irq2dev_map[dev->irq] = dev;
	if (request_irq(dev->irq, &vortex_interrupt, 0, vp->product_name)) {
		irq2dev_map[dev->irq] = NULL;
		return -EAGAIN;
	}
#endif

	if (vortex_debug > 1) {
		EL3WINDOW(4);
		printk("%s: vortex_open() irq %d media status %4.4x.\n",
			   dev->name, dev->irq, inw(ioaddr + Wn4_Media));
	}

	/* Set the station address and mask in window 2 each time opened. */
	EL3WINDOW(2);
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + i);
	for (; i < 12; i+=2)
		outw(0, ioaddr + i);

	if (dev->if_port == 3)
		/* Start the thinnet transceiver. We should really wait 50ms...*/
		outw(StartCoax, ioaddr + EL3_CMD);
	else if (dev->if_port == 0) {
		/* 10baseT interface, enabled link beat and jabber check. */
		EL3WINDOW(4);
		outw(inw(ioaddr + Wn4_Media) | Media_TP, ioaddr + Wn4_Media);
	}

	/* Switch to the stats window, and clear all stats by reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	EL3WINDOW(6);
	for (i = 0; i < 10; i++)	
		inb(ioaddr + i);
	inw(ioaddr + 10);
	inw(ioaddr + 12);
	/* New: On the Vortex we must also clear the BadSSD counter. */
	EL3WINDOW(4);
	inb(ioaddr + 12);

	/* Switch to register set 7 for normal use. */
	EL3WINDOW(7);

	/* Accept b-case and phys addr only. */
	outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
	outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	outw(RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
	outw(TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */
	/* Allow status bits to be seen. */
	outw(SetStatusEnb | 0xff, ioaddr + EL3_CMD);
	/* Ack all pending events, and set active indicator mask. */
	outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
		 ioaddr + EL3_CMD);
	outw(SetIntrEnb | IntLatch | TxAvailable | RxComplete | StatsFull
		 | DMADone, ioaddr + EL3_CMD);

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	if (vp->autoselect) {
		init_timer(&vp->timer);
		vp->timer.expires = (14*HZ)/10; 			/* 1.4 sec. */
		vp->timer.data = (unsigned long)dev;
		vp->timer.function = &vortex_timer;    /* timer handler */
		add_timer(&vp->timer);
	}
	return 0;
}

static void vortex_timer(unsigned long data)
{
	struct device *dev = (struct device *)data;
	if (vortex_debug > 2)
		printk("%s: Media selection timer tick happened.\n", dev->name);
	/* ToDo: active media selection here! */
}

static int
vortex_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	int ioaddr = dev->base_addr;

	/* Transmitter timeout, serious problems. */
	if (dev->tbusy) {
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < 40)
			return 1;
		printk("%s: transmit timed out, tx_status %2.2x status %4.4x.\n",
			   dev->name, inb(ioaddr + TxStatus), inw(ioaddr + EL3_STATUS));
		vp->stats.tx_errors++;
		/* Issue TX_RESET and TX_START commands. */
		outw(TxReset, ioaddr + EL3_CMD);
		{
			int i;
			for (i = 20; i >= 0 ; i--)
				if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
					break;
		}
		outw(TxEnable, ioaddr + EL3_CMD);
		dev->trans_start = jiffies;
		dev->tbusy = 0;
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

	/* Put out the doubleword header... */
	outl(skb->len, ioaddr + TX_FIFO);
#ifdef VORTEX_BUS_MASTER
	if (vp->bus_master) {
		/* Set the bus-master controller to transfer the packet. */
		outl((int)(skb->data), ioaddr + Wn7_MasterAddr);
		outw((skb->len + 3) & ~3, ioaddr + Wn7_MasterLen);
		vp->tx_skb = skb;
		outw(StartDMADown, ioaddr + EL3_CMD);
	} else {
		/* ... and the packet rounded to a doubleword. */
		outsl(ioaddr + TX_FIFO, skb->data, (skb->len + 3) >> 2);
		dev_kfree_skb (skb, FREE_WRITE);
		if (inw(ioaddr + TxFree) > 1536) {
			dev->tbusy = 0;
		} else
			/* Interrupt us when the FIFO has room for max-sized packet. */
			outw(SetTxThreshold + 1536, ioaddr + EL3_CMD);
	}
#else
	/* ... and the packet rounded to a doubleword. */
	outsl(ioaddr + TX_FIFO, skb->data, (skb->len + 3) >> 2);
	dev_kfree_skb (skb, FREE_WRITE);
	if (inw(ioaddr + TxFree) > 1536) {
		dev->tbusy = 0;
	} else
		/* Interrupt us when the FIFO has room for max-sized packet. */
		outw(SetTxThreshold + 1536, ioaddr + EL3_CMD);
#endif  /* bus master */

	dev->trans_start = jiffies;

	/* Clear the Tx status stack. */
	{
		short tx_status;
		int i = 4;

		while (--i > 0	&&	(tx_status = inb(ioaddr + TxStatus)) > 0) {
			if (tx_status & 0x3C) {		/* A Tx-disabling error occured.  */
				if (vortex_debug > 2)
				  printk("%s: Tx error, status %2.2x.\n",
						 dev->name, tx_status);
				if (tx_status & 0x04) vp->stats.tx_fifo_errors++;
				if (tx_status & 0x38) vp->stats.tx_aborted_errors++;
				if (tx_status & 0x30) {
					int j;
					outw(TxReset, ioaddr + EL3_CMD);
					for (j = 20; j >= 0 ; j--)
						if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
							break;
				}
				outw(TxEnable, ioaddr + EL3_CMD);
			}
			outb(0x00, ioaddr + TxStatus); /* Pop the status stack. */
		}
	}
	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void vortex_interrupt(int irq, struct pt_regs *regs)
{
#ifdef USE_SHARED_IRQ
	struct device *dev = (struct device *)(irq == 0 ? regs : irq2dev_map[irq]);
#else
	struct device *dev = (struct device *)(irq2dev_map[irq]);
#endif
	struct vortex_private *lp;
	int ioaddr, status;
	int latency;
	int i = 0;

	if (dev == NULL) {
		printk ("vortex_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	if (dev->interrupt)
		printk("%s: Re-entering the interrupt handler.\n", dev->name);
	dev->interrupt = 1;

	ioaddr = dev->base_addr;
	latency = inb(ioaddr + Timer);
	lp = (struct vortex_private *)dev->priv;

	status = inw(ioaddr + EL3_STATUS);

	if (vortex_debug > 4)
		printk("%s: interrupt, status %4.4x, timer %d.\n", dev->name,
			   status, latency);
	if ((status & 0xE000) != 0xE000) {
		static int donedidthis=0;
		/* Some interrupt controllers store a bogus interrupt from boot-time.
		   Ignore a single early interrupt, but don't hang the machine for
		   other interrupt problems. */
		if (donedidthis++ > 1) {
			printk("%s: Bogus interrupt, bailing. Status %4.4x, start=%d.\n",
				   dev->name, status, dev->start);
			free_irq(dev->irq);
		}
	}

	do {
		if (vortex_debug > 5)
				printk("%s: In interrupt loop, status %4.4x.\n",
					   dev->name, status);
		if (status & RxComplete)
			vortex_rx(dev);

		if (status & TxAvailable) {
			if (vortex_debug > 5)
				printk("	TX room bit was handled.\n");
			/* There's room in the FIFO for a full-sized packet. */
			outw(AckIntr | TxAvailable, ioaddr + EL3_CMD);
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}
#ifdef VORTEX_BUS_MASTER
		if (status & DMADone) {
			outw(0x1000, ioaddr + Wn7_MasterStatus); /* Ack the event. */
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}
#endif
		if (status & (AdapterFailure | RxEarly | StatsFull)) {
			/* Handle all uncommon interrupts at once. */
			if (status & RxEarly) {				/* Rx early is unused. */
				vortex_rx(dev);
				outw(AckIntr | RxEarly, ioaddr + EL3_CMD);
			}
			if (status & StatsFull) { 	/* Empty statistics. */
				static int DoneDidThat = 0;
				if (vortex_debug > 4)
					printk("%s: Updating stats.\n", dev->name);
				update_stats(ioaddr, dev);
				/* DEBUG HACK: Disable statistics as an interrupt source. */
				/* This occurs when we have the wrong media type! */
				if (DoneDidThat == 0  &&
					inw(ioaddr + EL3_STATUS) & StatsFull) {
					int win, reg;
					printk("%s: Updating stats failed, disabling stats as an"
						   " interrupt source.\n", dev->name);
					for (win = 0; win < 8; win++) {
						EL3WINDOW(win);
						printk("\n Vortex window %d:", win);
						for (reg = 0; reg < 16; reg++)
							printk(" %2.2x", inb(ioaddr+reg));
					}
					EL3WINDOW(7);
					outw(SetIntrEnb | 0x18, ioaddr + EL3_CMD);
					DoneDidThat++;
				}
			}
			if (status & AdapterFailure) {
				/* Adapter failure requires Rx reset and reinit. */
				outw(RxReset, ioaddr + EL3_CMD);
				/* Set the Rx filter to the current state. */
				outw(SetRxFilter | RxStation | RxBroadcast
					 | (dev->flags & IFF_ALLMULTI ? RxMulticast : 0)
					 | (dev->flags & IFF_PROMISC ? RxProm : 0),
					 ioaddr + EL3_CMD);
				outw(RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver. */
				outw(AckIntr | AdapterFailure, ioaddr + EL3_CMD);
			}
		}

		if (++i > 10) {
			printk("%s: Infinite loop in interrupt, status %4.4x.  "
				   "Disabling functions (%4.4x).\n",
				   dev->name, status, SetStatusEnb | ((~status) & 0xFE));
			/* Disable all pending interrupts. */
			outw(SetStatusEnb | ((~status) & 0xFE), ioaddr + EL3_CMD);
			outw(AckIntr | 0xFF, ioaddr + EL3_CMD);
			break;
		}
		/* Acknowledge the IRQ. */
		outw(AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);

	} while ((status = inw(ioaddr + EL3_STATUS)) & (IntLatch | RxComplete));

	if (vortex_debug > 4)
		printk("%s: exiting interrupt, status %4.4x.\n", dev->name, status);

	dev->interrupt = 0;
	return;
}

static int
vortex_rx(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	int ioaddr = dev->base_addr;
	int i;
	short rx_status;

	if (vortex_debug > 5)
		printk("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
			   inw(ioaddr+EL3_STATUS), inw(ioaddr+RxStatus));
	while ((rx_status = inw(ioaddr + RxStatus)) > 0) {
		if (rx_status & 0x4000) { /* Error, update stats. */
			unsigned char rx_error = inb(ioaddr + RxErrors);
			if (vortex_debug > 4)
				printk(" Rx error: status %2.2x.\n", rx_error);
			vp->stats.rx_errors++;
			if (rx_error & 0x01)  vp->stats.rx_over_errors++;
			if (rx_error & 0x02)  vp->stats.rx_length_errors++;
			if (rx_error & 0x04)  vp->stats.rx_frame_errors++;
			if (rx_error & 0x08)  vp->stats.rx_crc_errors++;
			if (rx_error & 0x10)  vp->stats.rx_length_errors++;
		} else {
			/* The packet length: up to 4.5K!. */
			short pkt_len = rx_status & 0x1fff;
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len + 5);
			if (vortex_debug > 4)
				printk("Receiving packet size %d status %4.4x.\n",
					   pkt_len, rx_status);
			if (skb != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
				/* 'skb_put()' points to the start of sk_buff data area. */
				insl(ioaddr + RX_FIFO, skb_put(skb, pkt_len),
					 (pkt_len + 3) >> 2);
				skb->protocol = eth_type_trans(skb, dev);
				netif_rx(skb);
				outw(RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */
				/* Wait a limited time to go to next packet. */
				for (i = 200; i >= 0; i--)
					if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
						break;
				vp->stats.rx_packets++;
				continue;
			} else if (vortex_debug)
				printk("%s: Couldn't allocate a sk_buff of size %d.\n",
					   dev->name, pkt_len);
		}
		vp->stats.rx_dropped++;
		outw(RxDiscard, ioaddr + EL3_CMD);
		/* Wait a limited time to skip this packet. */
		for (i = 200; i >= 0; i--)
			if ( ! inw(ioaddr + EL3_STATUS) & CmdInProgress)
				break;
	}

	return 0;
}

static int
vortex_close(struct device *dev)
{
	int ioaddr = dev->base_addr;

	dev->start = 0;
	dev->tbusy = 1;

	if (vortex_debug > 1)
		printk("%s: vortex_close() status %4.4x, Tx status %2.2x.\n",
			   dev->name, inw(ioaddr + EL3_STATUS), inb(ioaddr + TxStatus));

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
		outw(inw(ioaddr + Wn4_Media) & ~Media_TP, ioaddr + Wn4_Media);
	}

#ifdef USE_SHARED_IRQ
	free_shared_irq(dev->irq, dev);
#else
	free_irq(dev->irq);
	/* Mmmm, we should diable all interrupt sources here. */
	irq2dev_map[dev->irq] = 0;
#endif

	update_stats(ioaddr, dev);
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif

	return 0;
}

static struct enet_statistics *
vortex_get_stats(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	unsigned long flags;

	save_flags(flags);
	cli();
	update_stats(dev->base_addr, dev);
	restore_flags(flags);
	return &vp->stats;
}

/*  Update statistics.
	Unlike with the EL3 we need not worry about interrupts changing
	the window setting from underneath us, but we must still guard
	against a race condition with a StatsUpdate interrupt updating the
	table.  This is done by checking that the ASM (!) code generated uses
	atomic updates with '+='.
	*/
static void update_stats(int ioaddr, struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;

	/* Unlike the 3c5x9 we need not turn off stats updates while reading. */
	/* Switch to the stats window, and read everything. */
	EL3WINDOW(6);
	vp->stats.tx_carrier_errors		+= inb(ioaddr + 0);
	vp->stats.tx_heartbeat_errors	+= inb(ioaddr + 1);
	/* Multiple collisions. */		inb(ioaddr + 2);
	vp->stats.collisions			+= inb(ioaddr + 3);
	vp->stats.tx_window_errors		+= inb(ioaddr + 4);
	vp->stats.rx_fifo_errors		+= inb(ioaddr + 5);
	vp->stats.tx_packets			+= inb(ioaddr + 6);
	vp->stats.tx_packets			+= (inb(ioaddr + 9)&0x30) << 4;
	/* Rx packets	*/				inb(ioaddr + 7);   /* Must read to clear */
	/* Tx deferrals */				inb(ioaddr + 8);
	/* Don't bother with register 9, an extention of registers 6&7.
	   If we do use the 6&7 values the atomic update assumption above
	   is invalid. */
	inw(ioaddr + 10);	/* Total Rx and Tx octets. */
	inw(ioaddr + 12);
	/* New: On the Vortex we must also clear the BadSSD counter. */
	EL3WINDOW(4);
	inb(ioaddr + 12);

	/* We change back to window 7 (not 1) with the Vortex. */
	EL3WINDOW(7);
	return;
}

/* There are two version of set_multicast_list() to support both v1.2 and
   v1.4 kernels. */
static void
set_multicast_list(struct device *dev)
{
	short ioaddr = dev->base_addr;

	if ((dev->mc_list)  ||  (dev->flags & IFF_ALLMULTI)) {
		outw(SetRxFilter|RxStation|RxMulticast|RxBroadcast, ioaddr + EL3_CMD);
		if (vortex_debug > 3) {
			printk("%s: Setting Rx multicast mode, %d addresses.\n",
				   dev->name, dev->mc_count);
		}
	} else if (dev->flags & IFF_PROMISC) {
		outw(SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm,
			 ioaddr + EL3_CMD);
	} else
		outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
}


#ifdef MODULE
void
cleanup_module(void)
{
	struct device *next_dev;

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (root_vortex_dev) {
		next_dev = ((struct vortex_private *)root_vortex_dev->priv)->next_module;
		unregister_netdev(root_vortex_dev);
		release_region(root_vortex_dev->base_addr, VORTEX_TOTAL_SIZE);
		kfree(root_vortex_dev);
		root_vortex_dev = next_dev;
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c 3c59x.c -o 3c59x.o"
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
