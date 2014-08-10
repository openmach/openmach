/* eepro.c: Intel EtherExpress Pro/10 device driver for Linux. */
/*
	Written 1994, 1995 by Bao C. Ha.

	Copyright (C) 1994, 1995 by Bao C. Ha.

	This software may be used and distributed
	according to the terms of the GNU Public License,
	incorporated herein by reference.

	The author may be reached at bao@saigon.async.com 
	or 418 Hastings Place, Martinez, GA 30907.

	Things remaining to do:
	Better record keeping of errors.
	Eliminate transmit interrupt to reduce overhead.
	Implement "concurrent processing". I won't be doing it!
	Allow changes to the partition of the transmit and receive
	buffers, currently the ratio is 3:1 of receive to transmit
	buffer ratio.  

	Bugs:

	If you have a problem of not detecting the 82595 during a
	reboot (warm reset), disable the FLASH memory should fix it.
	This is a compatibility hardware problem.
	
	Versions:

	0.07a	Fix a stat report which counts every packet as a
		heart-beat failure. (BCH, 6/3/95)

	0.07	Modified to support all other 82595-based lan cards.  
		The IRQ vector of the EtherExpress Pro will be set
		according to the value saved in the EEPROM.  For other
		cards, I will do autoirq_request() to grab the next
		available interrupt vector. (BCH, 3/17/95)

	0.06a,b	Interim released.  Minor changes in the comments and
		print out format. (BCH, 3/9/95 and 3/14/95)

	0.06	First stable release that I am comfortable with. (BCH,
		3/2/95)	

	0.05	Complete testing of multicast. (BCH, 2/23/95)	

	0.04	Adding multicast support. (BCH, 2/14/95)	

	0.03	First widely alpha release for public testing. 
		(BCH, 2/14/95)	

*/

static const char *version =
	"eepro.c: v0.07a 6/5/95 Bao C. Ha (bao@saigon.async.com)\n";

#include <linux/module.h>

/*
  Sources:

	This driver wouldn't have been written without the availability 
	of the Crynwr's Lan595 driver source code.  It helps me to 
	familiarize with the 82595 chipset while waiting for the Intel 
	documentation.  I also learned how to detect the 82595 using 
	the packet driver's technique.

	This driver is written by cutting and pasting the skeleton.c driver
	provided by Donald Becker.  I also borrowed the EEPROM routine from
	Donald Becker's 82586 driver.

	Datasheet for the Intel 82595. It provides just enough info that 
	the casual reader might think that it documents the i82595.

	The User Manual for the 82595.  It provides a lot of the missing
	information.

*/

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


/* First, a few definitions that the brave might change. */
/* A zero-terminated list of I/O addresses to be probed. */
static unsigned int eepro_portlist[] =
   { 0x200, 0x240, 0x280, 0x2C0, 0x300, 0x320, 0x340, 0x360, 0};

/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 2
#endif
static unsigned int net_debug = NET_DEBUG;

/* The number of low I/O ports used by the ethercard. */
#define EEPRO_IO_EXTENT	16

/* Information that need to be kept for each board. */
struct eepro_local {
	struct enet_statistics stats;
	unsigned rx_start;
	unsigned tx_start; /* start of the transmit chain */
	int tx_last;  /* pointer to last packet in the transmit chain */
	unsigned tx_end;   /* end of the transmit chain (plus 1) */
	int eepro;	/* a flag, TRUE=1 for the EtherExpress Pro/10,
			   FALSE = 0 for other 82595-based lan cards. */
};

/* The station (ethernet) address prefix, used for IDing the board. */
#define SA_ADDR0 0x00
#define SA_ADDR1 0xaa
#define SA_ADDR2 0x00

/* Index to functions, as function prototypes. */

extern int eepro_probe(struct device *dev);	

static int	eepro_probe1(struct device *dev, short ioaddr);
static int	eepro_open(struct device *dev);
static int	eepro_send_packet(struct sk_buff *skb, struct device *dev);
static void	eepro_interrupt(int irq, struct pt_regs *regs);
static void 	eepro_rx(struct device *dev);
static void 	eepro_transmit_interrupt(struct device *dev);
static int	eepro_close(struct device *dev);
static struct enet_statistics *eepro_get_stats(struct device *dev);
static void set_multicast_list(struct device *dev);

static int read_eeprom(int ioaddr, int location);
static void hardware_send_packet(struct device *dev, void *buf, short length);
static int	eepro_grab_irq(struct device *dev);

/*
  			Details of the i82595.

You will need either the datasheet or the user manual to understand what
is going on here.  The 82595 is very different from the 82586, 82593.

The receive algorithm in eepro_rx() is just an implementation of the
RCV ring structure that the Intel 82595 imposes at the hardware level.
The receive buffer is set at 24K, and the transmit buffer is 8K.  I
am assuming that the total buffer memory is 32K, which is true for the
Intel EtherExpress Pro/10.  If it is less than that on a generic card,
the driver will be broken.

The transmit algorithm in the hardware_send_packet() is similar to the
one in the eepro_rx().  The transmit buffer is a ring linked list.
I just queue the next available packet to the end of the list.  In my
system, the 82595 is so fast that the list seems to always contain a
single packet.  In other systems with faster computers and more congested
network traffics, the ring linked list should improve performance by
allowing up to 8K worth of packets to be queued.

*/
#define	RAM_SIZE	0x8000
#define	RCV_HEADER	8
#define	RCV_RAM		0x6000	/* 24KB for RCV buffer */
#define	RCV_LOWER_LIMIT	0x00	/* 0x0000 */
#define	RCV_UPPER_LIMIT	((RCV_RAM - 2) >> 8)	/* 0x5ffe */
#define	XMT_RAM		(RAM_SIZE - RCV_RAM)	/* 8KB for XMT buffer */
#define	XMT_LOWER_LIMIT	(RCV_RAM >> 8)	/* 0x6000 */
#define	XMT_UPPER_LIMIT	((RAM_SIZE - 2) >> 8)	/* 0x7ffe */
#define	XMT_HEADER	8

#define	RCV_DONE	0x0008
#define	RX_OK		0x2000
#define	RX_ERROR	0x0d81

#define	TX_DONE_BIT	0x0080
#define	CHAIN_BIT	0x8000
#define	XMT_STATUS	0x02
#define	XMT_CHAIN	0x04
#define	XMT_COUNT	0x06

#define	BANK0_SELECT	0x00		
#define	BANK1_SELECT	0x40		
#define	BANK2_SELECT	0x80		

/* Bank 0 registers */
#define	COMMAND_REG	0x00	/* Register 0 */
#define	MC_SETUP	0x03
#define	XMT_CMD		0x04
#define	DIAGNOSE_CMD	0x07
#define	RCV_ENABLE_CMD	0x08
#define	RCV_DISABLE_CMD	0x0a
#define	STOP_RCV_CMD	0x0b
#define	RESET_CMD	0x0e
#define	POWER_DOWN_CMD	0x18
#define	RESUME_XMT_CMD	0x1c
#define	SEL_RESET_CMD	0x1e
#define	STATUS_REG	0x01	/* Register 1 */
#define	RX_INT		0x02
#define	TX_INT		0x04
#define	EXEC_STATUS	0x30
#define	ID_REG		0x02	/* Register 2	*/
#define	R_ROBIN_BITS	0xc0	/* round robin counter */
#define	ID_REG_MASK	0x2c
#define	ID_REG_SIG	0x24
#define	AUTO_ENABLE	0x10
#define	INT_MASK_REG	0x03	/* Register 3	*/
#define	RX_STOP_MASK	0x01
#define	RX_MASK		0x02
#define	TX_MASK		0x04
#define	EXEC_MASK	0x08
#define	ALL_MASK	0x0f
#define	RCV_BAR		0x04	/* The following are word (16-bit) registers */
#define	RCV_STOP	0x06
#define	XMT_BAR		0x0a
#define	HOST_ADDRESS_REG	0x0c
#define	IO_PORT		0x0e

/* Bank 1 registers */
#define	REG1	0x01
#define	WORD_WIDTH	0x02
#define	INT_ENABLE	0x80
#define INT_NO_REG	0x02
#define	RCV_LOWER_LIMIT_REG	0x08
#define	RCV_UPPER_LIMIT_REG	0x09
#define	XMT_LOWER_LIMIT_REG	0x0a
#define	XMT_UPPER_LIMIT_REG	0x0b

/* Bank 2 registers */
#define	XMT_Chain_Int	0x20	/* Interrupt at the end of the transmit chain */
#define	XMT_Chain_ErrStop	0x40 /* Interrupt at the end of the chain even if there are errors */
#define	RCV_Discard_BadFrame	0x80 /* Throw bad frames away, and continue to receive others */
#define	REG2		0x02
#define	PRMSC_Mode	0x01
#define	Multi_IA	0x20
#define	REG3		0x03
#define	TPE_BIT		0x04
#define	BNC_BIT		0x20
	
#define	I_ADD_REG0	0x04
#define	I_ADD_REG1	0x05
#define	I_ADD_REG2	0x06
#define	I_ADD_REG3	0x07
#define	I_ADD_REG4	0x08
#define	I_ADD_REG5	0x09

#define EEPROM_REG 0x0a
#define EESK 0x01
#define EECS 0x02
#define EEDI 0x04
#define EEDO 0x08


/* Check for a network adaptor of this type, and return '0' iff one exists.
   If dev->base_addr == 0, probe all likely locations.
   If dev->base_addr == 1, always return failure.
   If dev->base_addr == 2, allocate space for the device and return success
   (detachable devices only).
   */
#ifdef HAVE_DEVLIST
/* Support for a alternate probe manager, which will eliminate the
   boilerplate below. */
struct netdev_entry netcard_drv =
{"eepro", eepro_probe1, EEPRO_IO_EXTENT, eepro_portlist};
#else
int
eepro_probe(struct device *dev)
{
	int i;
	int base_addr = dev ? dev->base_addr : 0;

	if (base_addr > 0x1ff)		/* Check a single specified location. */
		return eepro_probe1(dev, base_addr);
	else if (base_addr != 0)	/* Don't probe at all. */
		return ENXIO;

	for (i = 0; eepro_portlist[i]; i++) {
		int ioaddr = eepro_portlist[i];
		if (check_region(ioaddr, EEPRO_IO_EXTENT))
			continue;
		if (eepro_probe1(dev, ioaddr) == 0)
			return 0;
	}

	return ENODEV;
}
#endif

/* This is the real probe routine.  Linux has a history of friendly device
   probes on the ISA bus.  A good device probes avoids doing writes, and
   verifies that the correct device exists and functions.  */

int eepro_probe1(struct device *dev, short ioaddr)
{
	unsigned short station_addr[6], id, counter;
	int i;
	int eepro;	/* a flag, TRUE=1 for the EtherExpress Pro/10,
			   FALSE = 0 for other 82595-based lan cards. */
	const char *ifmap[] = {"AUI", "10Base2", "10BaseT"};
	enum iftype { AUI=0, BNC=1, TPE=2 };

	/* Now, we are going to check for the signature of the
	   ID_REG (register 2 of bank 0) */

	if (((id=inb(ioaddr + ID_REG)) & ID_REG_MASK) == ID_REG_SIG) {

		/* We seem to have the 82595 signature, let's
		   play with its counter (last 2 bits of
		   register 2 of bank 0) to be sure. */
	
		counter = (id & R_ROBIN_BITS);	
		if (((id=inb(ioaddr+ID_REG)) & R_ROBIN_BITS) == 
			(counter + 0x40)) {

			/* Yes, the 82595 has been found */

			/* Now, get the ethernet hardware address from
			   the EEPROM */

			station_addr[0] = read_eeprom(ioaddr, 2);
			station_addr[1] = read_eeprom(ioaddr, 3);
			station_addr[2] = read_eeprom(ioaddr, 4);

			/* Check the station address for the manufacturer's code */

			if (station_addr[2] != 0x00aa || (station_addr[1] & 0xff00) != 0x0000) {
				eepro = 0;
				printk("%s: Intel 82595-based lan card at %#x,", 
					dev->name, ioaddr);
			}
			else {
				eepro = 1;
				printk("%s: Intel EtherExpress Pro/10 at %#x,", 
					dev->name, ioaddr);
			}

   			/* Fill in the 'dev' fields. */
			dev->base_addr = ioaddr;
			
			for (i=0; i < 6; i++) {
				dev->dev_addr[i] = ((unsigned char *) station_addr)[5-i];
				printk("%c%02x", i ? ':' : ' ', dev->dev_addr[i]);
			}
				
			outb(BANK2_SELECT, ioaddr); /* be CAREFUL, BANK 2 now */
			id = inb(ioaddr + REG3);
			if (id & TPE_BIT)
				dev->if_port = TPE;
			else dev->if_port = BNC;

			if (dev->irq < 2 && eepro) {
				i = read_eeprom(ioaddr, 1);
				switch (i & 0x07) {
					case 0:	dev->irq = 9; break;
					case 1:	dev->irq = 3; break;
					case 2:	dev->irq = 5; break;
					case 3:	dev->irq = 10; break;
					case 4:	dev->irq = 11; break;
					default: /* should never get here !!!!! */
						printk(" illegal interrupt vector stored in EEPROM.\n");
						return ENODEV;
					}
				}
			else if (dev->irq == 2)
				dev->irq = 9;

			if (dev->irq > 2) {
				printk(", IRQ %d, %s.\n", dev->irq,
						ifmap[dev->if_port]);
				if (request_irq(dev->irq, &eepro_interrupt, 0, "eepro")) {
					printk("%s: unable to get IRQ %d.\n", dev->name, dev->irq);
					return -EAGAIN;
				}
			}
			else printk(", %s.\n", ifmap[dev->if_port]);
			
			if ((dev->mem_start & 0xf) > 0)
				net_debug = dev->mem_start & 7;

			if (net_debug > 3) {
				i = read_eeprom(ioaddr, 5);
				if (i & 0x2000) /* bit 13 of EEPROM word 5 */
					printk("%s: Concurrent Processing is enabled but not used!\n",
						dev->name);
			}

			if (net_debug) 
				printk(version);

			/* Grab the region so we can find another board if autoIRQ fails. */
			request_region(ioaddr, EEPRO_IO_EXTENT, "eepro");

			/* Initialize the device structure */
			dev->priv = kmalloc(sizeof(struct eepro_local), GFP_KERNEL);
			if (dev->priv == NULL)
				return -ENOMEM;
			memset(dev->priv, 0, sizeof(struct eepro_local));

			dev->open = eepro_open;
			dev->stop = eepro_close;
			dev->hard_start_xmit = eepro_send_packet;
			dev->get_stats = eepro_get_stats;
			dev->set_multicast_list = &set_multicast_list;

			/* Fill in the fields of the device structure with
			   ethernet generic values */

			ether_setup(dev);

			outb(RESET_CMD, ioaddr); /* RESET the 82595 */

			return 0;
			}
		else return ENODEV;
		}
	else if (net_debug > 3)
		printk ("EtherExpress Pro probed failed!\n");
	return ENODEV;
}

/* Open/initialize the board.  This is called (in the current kernel)
   sometime after booting when the 'ifconfig' program is run.

   This routine should set everything up anew at each open, even
   registers that "should" only need to be set once at boot, so that
   there is non-reboot way to recover if something goes wrong.
   */

static char irqrmap[] = {-1,-1,0,1,-1,2,-1,-1,-1,0,3,4,-1,-1,-1,-1};
static int	eepro_grab_irq(struct device *dev)
{
	int irqlist[] = { 5, 9, 10, 11, 4, 3, 0};	
	int *irqp = irqlist, temp_reg, ioaddr = dev->base_addr;

	outb(BANK1_SELECT, ioaddr); /* be CAREFUL, BANK 1 now */

	/* Enable the interrupt line. */
	temp_reg = inb(ioaddr + REG1);
	outb(temp_reg | INT_ENABLE, ioaddr + REG1); 
	
	outb(BANK0_SELECT, ioaddr); /* be CAREFUL, BANK 0 now */

	/* clear all interrupts */
	outb(ALL_MASK, ioaddr + STATUS_REG); 
	/* Let EXEC event to interrupt */
	outb(ALL_MASK & ~(EXEC_MASK), ioaddr + INT_MASK_REG); 

	do {
		outb(BANK1_SELECT, ioaddr); /* be CAREFUL, BANK 1 now */

		temp_reg = inb(ioaddr + INT_NO_REG);
		outb((temp_reg & 0xf8) | irqrmap[*irqp], ioaddr + INT_NO_REG); 

		outb(BANK0_SELECT, ioaddr); /* Switch back to Bank 0 */

		if (request_irq (*irqp, NULL, 0, "bogus") != EBUSY) {
			/* Twinkle the interrupt, and check if it's seen */
			autoirq_setup(0);

			outb(DIAGNOSE_CMD, ioaddr); /* RESET the 82595 */
				
			if (*irqp == autoirq_report(2) &&  /* It's a good IRQ line */
				(request_irq(dev->irq = *irqp, &eepro_interrupt, 0, "eepro") == 0)) 
					break;

			/* clear all interrupts */
			outb(ALL_MASK, ioaddr + STATUS_REG); 
		}
	} while (*++irqp);

	outb(BANK1_SELECT, ioaddr); /* Switch back to Bank 1 */

	/* Disable the physical interrupt line. */
	temp_reg = inb(ioaddr + REG1);
	outb(temp_reg & 0x7f, ioaddr + REG1); 

	outb(BANK0_SELECT, ioaddr); /* Switch back to Bank 0 */

	/* Mask all the interrupts. */
	outb(ALL_MASK, ioaddr + INT_MASK_REG); 

	/* clear all interrupts */
	outb(ALL_MASK, ioaddr + STATUS_REG); 

	return dev->irq;
}

static int
eepro_open(struct device *dev)
{
	unsigned short temp_reg;
	int i, ioaddr = dev->base_addr;
	struct eepro_local *lp = (struct eepro_local *)dev->priv;

	if (net_debug > 3)
		printk("eepro: entering eepro_open routine.\n");

	if (dev->dev_addr[0] == SA_ADDR0 &&
			dev->dev_addr[1] == SA_ADDR1 &&
			dev->dev_addr[2] == SA_ADDR2)
		lp->eepro = 1; /* Yes, an Intel EtherExpress Pro/10 */
	else lp->eepro = 0; /* No, it is a generic 82585 lan card */

	/* Get the interrupt vector for the 82595 */	
	if (dev->irq < 2 && eepro_grab_irq(dev) == 0) {
		printk("%s: unable to get IRQ %d.\n", dev->name, dev->irq);
		return -EAGAIN;
	}
				
	if (irq2dev_map[dev->irq] != 0
		|| (irq2dev_map[dev->irq] = dev) == 0)
		return -EAGAIN;

	/* Initialize the 82595. */

	outb(BANK2_SELECT, ioaddr); /* be CAREFUL, BANK 2 now */
	temp_reg = inb(ioaddr + EEPROM_REG);
	if (temp_reg & 0x10) /* Check the TurnOff Enable bit */
		outb(temp_reg & 0xef, ioaddr + EEPROM_REG);
	for (i=0; i < 6; i++) 
		outb(dev->dev_addr[i] , ioaddr + I_ADD_REG0 + i); 
			
	temp_reg = inb(ioaddr + REG1);    /* Setup Transmit Chaining */
	outb(temp_reg | XMT_Chain_Int | XMT_Chain_ErrStop /* and discard bad RCV frames */
		| RCV_Discard_BadFrame, ioaddr + REG1);  

	temp_reg = inb(ioaddr + REG2); /* Match broadcast */
	outb(temp_reg | 0x14, ioaddr + REG2);

	temp_reg = inb(ioaddr + REG3);
	outb(temp_reg & 0x3f, ioaddr + REG3); /* clear test mode */

	/* Set the receiving mode */
	outb(BANK1_SELECT, ioaddr); /* be CAREFUL, BANK 1 now */
	
	temp_reg = inb(ioaddr + INT_NO_REG);
	outb((temp_reg & 0xf8) | irqrmap[dev->irq], ioaddr + INT_NO_REG); 

	/* Initialize the RCV and XMT upper and lower limits */
	outb(RCV_LOWER_LIMIT, ioaddr + RCV_LOWER_LIMIT_REG); 
	outb(RCV_UPPER_LIMIT, ioaddr + RCV_UPPER_LIMIT_REG); 
	outb(XMT_LOWER_LIMIT, ioaddr + XMT_LOWER_LIMIT_REG); 
	outb(XMT_UPPER_LIMIT, ioaddr + XMT_UPPER_LIMIT_REG); 

	/* Enable the interrupt line. */
	temp_reg = inb(ioaddr + REG1);
	outb(temp_reg | INT_ENABLE, ioaddr + REG1); 

	outb(BANK0_SELECT, ioaddr); /* Switch back to Bank 0 */

	/* Let RX and TX events to interrupt */
	outb(ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG); 
	/* clear all interrupts */
	outb(ALL_MASK, ioaddr + STATUS_REG); 

	/* Initialize RCV */
	outw(RCV_LOWER_LIMIT << 8, ioaddr + RCV_BAR); 
	lp->rx_start = (RCV_LOWER_LIMIT << 8) ;
	outw((RCV_UPPER_LIMIT << 8) | 0xfe, ioaddr + RCV_STOP); 

	/* Initialize XMT */
	outw(XMT_LOWER_LIMIT << 8, ioaddr + XMT_BAR); 
	
	outb(SEL_RESET_CMD, ioaddr);
	/* We are supposed to wait for 2 us after a SEL_RESET */
	SLOW_DOWN_IO;
	SLOW_DOWN_IO;	

	lp->tx_start = lp->tx_end = XMT_LOWER_LIMIT << 8; /* or = RCV_RAM */
	lp->tx_last = 0;  
	
	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	if (net_debug > 3)
		printk("eepro: exiting eepro_open routine.\n");

	outb(RCV_ENABLE_CMD, ioaddr);

	MOD_INC_USE_COUNT;
	return 0;
}

static int
eepro_send_packet(struct sk_buff *skb, struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	int ioaddr = dev->base_addr;

	if (net_debug > 5)
		printk("eepro: entering eepro_send_packet routine.\n");
	
	if (dev->tbusy) {
		/* If we get here, some higher level has decided we are broken.
		   There should really be a "kick me" function call instead. */
		int tickssofar = jiffies - dev->trans_start;
		if (tickssofar < 5)
			return 1;
		if (net_debug > 1)
			printk("%s: transmit timed out, %s?\n", dev->name,
				   "network cable problem");
		lp->stats.tx_errors++;
		/* Try to restart the adaptor. */
		outb(SEL_RESET_CMD, ioaddr); 
		/* We are supposed to wait for 2 us after a SEL_RESET */
		SLOW_DOWN_IO;
		SLOW_DOWN_IO;

		/* Do I also need to flush the transmit buffers here? YES? */
		lp->tx_start = lp->tx_end = RCV_RAM; 
		lp->tx_last = 0;
	
		dev->tbusy=0;
		dev->trans_start = jiffies;

		outb(RCV_ENABLE_CMD, ioaddr);

	}

	/* If some higher layer thinks we've missed an tx-done interrupt
	   we are passed NULL. Caution: dev_tint() handles the cli()/sti()
	   itself. */
	if (skb == NULL) {
		dev_tint(dev);
		return 0;
	}

	/* Block a timer-based transmit from overlapping. */
	if (set_bit(0, (void*)&dev->tbusy) != 0)
		printk("%s: Transmitter access conflict.\n", dev->name);
	else {
		short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
		unsigned char *buf = skb->data;

		hardware_send_packet(dev, buf, length);
		dev->trans_start = jiffies;
	}

	dev_kfree_skb (skb, FREE_WRITE);

	/* You might need to clean up and record Tx statistics here. */
	/* lp->stats.tx_aborted_errors++; */

	if (net_debug > 5)
		printk("eepro: exiting eepro_send_packet routine.\n");
	
	return 0;
}


/*	The typical workload of the driver:
	Handle the network interface interrupts. */
static void
eepro_interrupt(int irq, struct pt_regs * regs)
{
	struct device *dev = (struct device *)(irq2dev_map[irq]);
	int ioaddr, status, boguscount = 0;

	if (net_debug > 5)
		printk("eepro: entering eepro_interrupt routine.\n");
	
	if (dev == NULL) {
		printk ("eepro_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}
	dev->interrupt = 1;
	
	ioaddr = dev->base_addr;

	do { 
		status = inb(ioaddr + STATUS_REG);

		if (status & RX_INT) {
			if (net_debug > 4)
				printk("eepro: packet received interrupt.\n");

			/* Acknowledge the RX_INT */
			outb(RX_INT, ioaddr + STATUS_REG); 

			/* Get the received packets */
			eepro_rx(dev);
		}
		else if (status & TX_INT) {
			if (net_debug > 4)
				printk("eepro: packet transmit interrupt.\n");

			/* Acknowledge the TX_INT */
			outb(TX_INT, ioaddr + STATUS_REG); 

			/* Process the status of transmitted packets */
			eepro_transmit_interrupt(dev);
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}		
	} while ((++boguscount < 10) && (status & 0x06));

	dev->interrupt = 0;
	if (net_debug > 5)
		printk("eepro: exiting eepro_interrupt routine.\n");
	
	return;
}

static int
eepro_close(struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	int ioaddr = dev->base_addr;
	short temp_reg;

	dev->tbusy = 1;
	dev->start = 0;

	outb(BANK1_SELECT, ioaddr); /* Switch back to Bank 1 */

	/* Disable the physical interrupt line. */
	temp_reg = inb(ioaddr + REG1);
	outb(temp_reg & 0x7f, ioaddr + REG1); 

	outb(BANK0_SELECT, ioaddr); /* Switch back to Bank 0 */

	/* Flush the Tx and disable Rx. */
	outb(STOP_RCV_CMD, ioaddr); 
	lp->tx_start = lp->tx_end = RCV_RAM ;
	lp->tx_last = 0;  

	/* Mask all the interrupts. */
	outb(ALL_MASK, ioaddr + INT_MASK_REG); 

	/* clear all interrupts */
	outb(ALL_MASK, ioaddr + STATUS_REG); 

	/* Reset the 82595 */
	outb(RESET_CMD, ioaddr); 

	/* release the interrupt */
	free_irq(dev->irq);

	irq2dev_map[dev->irq] = 0;

	/* Update the statistics here. What statistics? */

	/* We are supposed to wait for 200 us after a RESET */
	SLOW_DOWN_IO;
	SLOW_DOWN_IO; /* May not be enough? */

	MOD_DEC_USE_COUNT;
	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct enet_statistics *
eepro_get_stats(struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;

	return &lp->stats;
}

/* Set or clear the multicast filter for this adaptor.
 */
static void
set_multicast_list(struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	short ioaddr = dev->base_addr;
	unsigned short mode;
	struct dev_mc_list *dmi=dev->mc_list;

	if (dev->flags&(IFF_ALLMULTI|IFF_PROMISC) || dev->mc_count > 63) 
	{
		/*
		 *	We must make the kernel realise we had to move
		 *	into promisc mode or we start all out war on
		 *	the cable. If it was a promisc rewquest the
		 *	flag is already set. If not we assert it.
		 */
		dev->flags|=IFF_PROMISC;		

		outb(BANK2_SELECT, ioaddr); /* be CAREFUL, BANK 2 now */
		mode = inb(ioaddr + REG2);
		outb(mode | PRMSC_Mode, ioaddr + REG2);	
		mode = inb(ioaddr + REG3);
		outb(mode, ioaddr + REG3); /* writing reg. 3 to complete the update */
		outb(BANK0_SELECT, ioaddr); /* Return to BANK 0 now */
		printk("%s: promiscuous mode enabled.\n", dev->name);
	} 
	else if (dev->mc_count==0 ) 
	{
		outb(BANK2_SELECT, ioaddr); /* be CAREFUL, BANK 2 now */
		mode = inb(ioaddr + REG2);
		outb(mode & 0xd6, ioaddr + REG2); /* Turn off Multi-IA and PRMSC_Mode bits */
		mode = inb(ioaddr + REG3);
		outb(mode, ioaddr + REG3); /* writing reg. 3 to complete the update */
		outb(BANK0_SELECT, ioaddr); /* Return to BANK 0 now */
	}
	else 
	{
		unsigned short status, *eaddrs;
		int i, boguscount = 0;
		
		/* Disable RX and TX interrupts.  Neccessary to avoid
		   corruption of the HOST_ADDRESS_REG by interrupt
		   service routines. */
		outb(ALL_MASK, ioaddr + INT_MASK_REG);

		outb(BANK2_SELECT, ioaddr); /* be CAREFUL, BANK 2 now */
		mode = inb(ioaddr + REG2);
		outb(mode | Multi_IA, ioaddr + REG2);	
		mode = inb(ioaddr + REG3);
		outb(mode, ioaddr + REG3); /* writing reg. 3 to complete the update */
		outb(BANK0_SELECT, ioaddr); /* Return to BANK 0 now */
		outw(lp->tx_end, ioaddr + HOST_ADDRESS_REG);
		outw(MC_SETUP, ioaddr + IO_PORT);
		outw(0, ioaddr + IO_PORT);
		outw(0, ioaddr + IO_PORT);
		outw(6*(dev->mc_count + 1), ioaddr + IO_PORT);
		for (i = 0; i < dev->mc_count; i++) 
		{
			eaddrs=(unsigned short *)dmi->dmi_addr;
			dmi=dmi->next;
			outw(*eaddrs++, ioaddr + IO_PORT);
			outw(*eaddrs++, ioaddr + IO_PORT);
			outw(*eaddrs++, ioaddr + IO_PORT);
		}
		eaddrs = (unsigned short *) dev->dev_addr;
		outw(eaddrs[0], ioaddr + IO_PORT);
		outw(eaddrs[1], ioaddr + IO_PORT);
		outw(eaddrs[2], ioaddr + IO_PORT);
		outw(lp->tx_end, ioaddr + XMT_BAR);
		outb(MC_SETUP, ioaddr);

		/* Update the transmit queue */
		i = lp->tx_end + XMT_HEADER + 6*(dev->mc_count + 1);
		if (lp->tx_start != lp->tx_end) 
		{ 
			/* update the next address and the chain bit in the 
			   last packet */
			outw(lp->tx_last + XMT_CHAIN, ioaddr + HOST_ADDRESS_REG);
			outw(i, ioaddr + IO_PORT);
			outw(lp->tx_last + XMT_COUNT, ioaddr + HOST_ADDRESS_REG);
			status = inw(ioaddr + IO_PORT);
			outw(status | CHAIN_BIT, ioaddr + IO_PORT);
			lp->tx_end = i ;
		}
		else lp->tx_start = lp->tx_end = i ;

		/* Acknowledge that the MC setup is done */
		do { /* We should be doing this in the eepro_interrupt()! */
			SLOW_DOWN_IO;
			SLOW_DOWN_IO;
			if (inb(ioaddr + STATUS_REG) & 0x08) 
			{
				i = inb(ioaddr);
				outb(0x08, ioaddr + STATUS_REG);
				if (i & 0x20) { /* command ABORTed */
					printk("%s: multicast setup failed.\n", 
						dev->name);
					break;
				} else if ((i & 0x0f) == 0x03)	{ /* MC-Done */
					printk("%s: set Rx mode to %d addresses.\n", 
						dev->name, dev->mc_count);
					break;
				}
			}
		} while (++boguscount < 100);

		/* Re-enable RX and TX interrupts */
		outb(ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG); 
	
	}
	outb(RCV_ENABLE_CMD, ioaddr);
}

/* The horrible routine to read a word from the serial EEPROM. */
/* IMPORTANT - the 82595 will be set to Bank 0 after the eeprom is read */

/* The delay between EEPROM clock transitions. */
#define eeprom_delay()	{ int _i = 40; while (--_i > 0) { __SLOW_DOWN_IO; }}
#define EE_READ_CMD (6 << 6)

int
read_eeprom(int ioaddr, int location)
{
	int i;
	unsigned short retval = 0;
	short ee_addr = ioaddr + EEPROM_REG;
	int read_cmd = location | EE_READ_CMD;
	short ctrl_val = EECS ;
	
	outb(BANK2_SELECT, ioaddr);
	outb(ctrl_val, ee_addr);
	
	/* Shift the read command bits out. */
	for (i = 8; i >= 0; i--) {
		short outval = (read_cmd & (1 << i)) ? ctrl_val | EEDI
			: ctrl_val;
		outb(outval, ee_addr);
		outb(outval | EESK, ee_addr);	/* EEPROM clock tick. */
		eeprom_delay();
		outb(outval, ee_addr);	/* Finish EEPROM a clock tick. */
		eeprom_delay();
	}
	outb(ctrl_val, ee_addr);
	
	for (i = 16; i > 0; i--) {
		outb(ctrl_val | EESK, ee_addr);	 eeprom_delay();
		retval = (retval << 1) | ((inb(ee_addr) & EEDO) ? 1 : 0);
		outb(ctrl_val, ee_addr);  eeprom_delay();
	}

	/* Terminate the EEPROM access. */
	ctrl_val &= ~EECS;
	outb(ctrl_val | EESK, ee_addr);
	eeprom_delay();
	outb(ctrl_val, ee_addr);
	eeprom_delay();
	outb(BANK0_SELECT, ioaddr);
	return retval;
}

static void
hardware_send_packet(struct device *dev, void *buf, short length)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	short ioaddr = dev->base_addr;
	unsigned status, tx_available, last, end, boguscount = 10;

	if (net_debug > 5)
		printk("eepro: entering hardware_send_packet routine.\n");

	while (boguscount-- > 0) {

		/* determine how much of the transmit buffer space is available */
		if (lp->tx_end > lp->tx_start)
			tx_available = XMT_RAM - (lp->tx_end - lp->tx_start);
		else if (lp->tx_end < lp->tx_start)
			tx_available = lp->tx_start - lp->tx_end;
		else tx_available = XMT_RAM;

		/* Disable RX and TX interrupts.  Neccessary to avoid
		   corruption of the HOST_ADDRESS_REG by interrupt
		   service routines. */
		outb(ALL_MASK, ioaddr + INT_MASK_REG);

		if (((((length + 1) >> 1) << 1) + 2*XMT_HEADER) 
			>= tx_available)   /* No space available ??? */
			continue;

		last = lp->tx_end;
		end = last + (((length + 1) >> 1) << 1) + XMT_HEADER;

		if (end >= RAM_SIZE) { /* the transmit buffer is wrapped around */
			if ((RAM_SIZE - last) <= XMT_HEADER) {	
			/* Arrrr!!!, must keep the xmt header together,
			  several days were lost to chase this one down. */
				last = RCV_RAM;
				end = last + (((length + 1) >> 1) << 1) + XMT_HEADER;
			}	
			else end = RCV_RAM + (end - RAM_SIZE);
		}

		outw(last, ioaddr + HOST_ADDRESS_REG);
		outw(XMT_CMD, ioaddr + IO_PORT);
		outw(0, ioaddr + IO_PORT);
		outw(end, ioaddr + IO_PORT);
		outw(length, ioaddr + IO_PORT);
		outsw(ioaddr + IO_PORT, buf, (length + 1) >> 1);

		if (lp->tx_start != lp->tx_end) { 
			/* update the next address and the chain bit in the 
			   last packet */
			if (lp->tx_end != last) {
				outw(lp->tx_last + XMT_CHAIN, ioaddr + HOST_ADDRESS_REG);
				outw(last, ioaddr + IO_PORT);
			}
			outw(lp->tx_last + XMT_COUNT, ioaddr + HOST_ADDRESS_REG);
			status = inw(ioaddr + IO_PORT);
			outw(status | CHAIN_BIT, ioaddr + IO_PORT);
		}

		/* A dummy read to flush the DRAM write pipeline */
		status = inw(ioaddr + IO_PORT); 

		/* Enable RX and TX interrupts */
		outb(ALL_MASK & ~(RX_MASK | TX_MASK), ioaddr + INT_MASK_REG); 
	
		if (lp->tx_start == lp->tx_end) {
			outw(last, ioaddr + XMT_BAR);
			outb(XMT_CMD, ioaddr);
			lp->tx_start = last;   /* I don't like to change tx_start here */
		}
		else	outb(RESUME_XMT_CMD, ioaddr);

		lp->tx_last = last;
		lp->tx_end = end;

		if (dev->tbusy) {
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}

		if (net_debug > 5)
			printk("eepro: exiting hardware_send_packet routine.\n");
		return;
	}
	dev->tbusy = 1;
	if (net_debug > 5)
		printk("eepro: exiting hardware_send_packet routine.\n");
}

static void
eepro_rx(struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	short ioaddr = dev->base_addr;
	short boguscount = 20;
	short rcv_car = lp->rx_start;
	unsigned rcv_event, rcv_status, rcv_next_frame, rcv_size;

	if (net_debug > 5)
		printk("eepro: entering eepro_rx routine.\n");
	
	/* Set the read pointer to the start of the RCV */
	outw(rcv_car, ioaddr + HOST_ADDRESS_REG);
	rcv_event = inw(ioaddr + IO_PORT);

	while (rcv_event == RCV_DONE) {
		rcv_status = inw(ioaddr + IO_PORT);
		rcv_next_frame = inw(ioaddr + IO_PORT);
		rcv_size = inw(ioaddr + IO_PORT);

		if ((rcv_status & (RX_OK | RX_ERROR)) == RX_OK) {
			/* Malloc up new buffer. */
			struct sk_buff *skb;

			rcv_size &= 0x3fff;
			skb = dev_alloc_skb(rcv_size+2);
			if (skb == NULL) {
				printk("%s: Memory squeeze, dropping packet.\n", dev->name);
				lp->stats.rx_dropped++;
				break;
			}
			skb->dev = dev;
			skb_reserve(skb,2);

			insw(ioaddr+IO_PORT, skb_put(skb,rcv_size), (rcv_size + 1) >> 1);
	
			skb->protocol = eth_type_trans(skb,dev);	
			netif_rx(skb);
			lp->stats.rx_packets++;
		}
		else { /* Not sure will ever reach here, 
			  I set the 595 to discard bad received frames */
			lp->stats.rx_errors++;
			if (rcv_status & 0x0100)
				lp->stats.rx_over_errors++;
			else if (rcv_status & 0x0400)
				lp->stats.rx_frame_errors++;
			else if (rcv_status & 0x0800)
				lp->stats.rx_crc_errors++;
			printk("%s: event = %#x, status = %#x, next = %#x, size = %#x\n", 
				dev->name, rcv_event, rcv_status, rcv_next_frame, rcv_size);
		}
		if (rcv_status & 0x1000)
			lp->stats.rx_length_errors++;
		if (--boguscount == 0)
			break;

		rcv_car = lp->rx_start + RCV_HEADER + rcv_size;
		lp->rx_start = rcv_next_frame;
		outw(rcv_next_frame, ioaddr + HOST_ADDRESS_REG);
		rcv_event = inw(ioaddr + IO_PORT);

	} 
	if (rcv_car == 0)
		rcv_car = (RCV_UPPER_LIMIT << 8) | 0xff;
	outw(rcv_car - 1, ioaddr + RCV_STOP);

	if (net_debug > 5)
		printk("eepro: exiting eepro_rx routine.\n");
}

static void
eepro_transmit_interrupt(struct device *dev)
{
	struct eepro_local *lp = (struct eepro_local *)dev->priv;
	short ioaddr = dev->base_addr;
	short boguscount = 10; 
	short xmt_status;

	while (lp->tx_start != lp->tx_end) { 

		outw(lp->tx_start, ioaddr + HOST_ADDRESS_REG);
		xmt_status = inw(ioaddr+IO_PORT);
		if ((xmt_status & TX_DONE_BIT) == 0) break;
		xmt_status = inw(ioaddr+IO_PORT);
		lp->tx_start = inw(ioaddr+IO_PORT);
	
		if (dev->tbusy) {
			dev->tbusy = 0;
			mark_bh(NET_BH);
		}

		if (xmt_status & 0x2000)
			lp->stats.tx_packets++;
		else {
			lp->stats.tx_errors++;
			if (xmt_status & 0x0400)
				lp->stats.tx_carrier_errors++;
			printk("%s: XMT status = %#x\n",
				dev->name, xmt_status);
		}
		if (xmt_status & 0x000f)
			lp->stats.collisions += (xmt_status & 0x000f);
		if ((xmt_status & 0x0040) == 0x0)
			lp->stats.tx_heartbeat_errors++;

		if (--boguscount == 0)
			break;  
	}
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_eepro = {
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, eepro_probe };

static int io = 0x200;
static int irq = 0;

int
init_module(void)
{
	if (io == 0)
		printk("eepro: You should not use auto-probing with insmod!\n");
	dev_eepro.base_addr = io;
	dev_eepro.irq       = irq;

	if (register_netdev(&dev_eepro) != 0)
		return -EIO;
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(&dev_eepro);
	kfree_s(dev_eepro.priv,sizeof(struct eepro_local));
	dev_eepro.priv=NULL;

	/* If we don't do this, we can't re-insmod it later. */
	release_region(dev_eepro.base_addr, EEPRO_IO_EXTENT);
}
#endif /* MODULE */
