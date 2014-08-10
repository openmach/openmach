/* eth16i.c An ICL EtherTeam 16i and 32 EISA ethernet driver for Linux

   Written 1994-95 by Mika Kuoppala

   Copyright (C) 1994, 1995 by Mika Kuoppala
   Based on skeleton.c and at1700.c by Donald Becker

   This software may be used and distributed according to the terms
   of the GNU Public Licence, incorporated herein by reference.

   The author may be reached as miku@elt.icl.fi

   This driver supports following cards :
	- ICL EtherTeam 16i
	- ICL EtherTeam 32 EISA

   Sources:
     - skeleton.c  a sample network driver core for linux,
       written by Donald Becker <becker@CESDIS.gsfc.nasa.gov>
     - at1700.c a driver for Allied Telesis AT1700, written 
       by Donald Becker.
     - e16iSRV.asm a Netware 3.X Server Driver for ICL EtherTeam16i
       written by Markku Viima
     - The Fujitsu MB86965 databook.
   
   Valuable assistance from:
	Markku Viima (ICL) 
	Ari Valve (ICL)
   
   Revision history:

   Version	Date		Description
   
   0.01		15.12-94	Initial version (card detection)
   0.02         23.01-95        Interrupt is now hooked correctly
   0.03         01.02-95        Rewrote initialization part
   0.04         07.02-95        Base skeleton done...
				Made a few changes to signature checking
				to make it a bit reliable.
				- fixed bug in tx_buf mapping
				- fixed bug in initialization (DLC_EN
				  wasn't enabled when initialization
				  was done.)
   0.05		08.02-95	If there were more than one packet to send,
				transmit was jammed due to invalid
				register write...now fixed
   0.06         19.02-95        Rewrote interrupt handling        
   0.07         13.04-95        Wrote EEPROM read routines
                                Card configuration now set according to
				data read from EEPROM
   0.08         23.06-95        Wrote part that tries to probe used interface
                                port if AUTO is selected

   0.09         01.09-95	Added module support
   
   0.10         04.09-95	Fixed receive packet allocation to work		
        			with kernels > 1.3.x
   
   0.20		20.09-95	Added support for EtherTeam32 EISA	

   0.21         17.10-95        Removed the unnecessary extern 
				init_etherdev() declaration. Some
				other cleanups.
   Bugs:
	In some cases the interface autoprobing code doesn't find 
	the correct interface type. In this case you can 
	manually choose the interface type in DOS with E16IC.EXE which is 
	configuration software for EtherTeam16i and EtherTeam32 cards.
	
   To do:
	- Real multicast support
*/

static char *version = 
	"eth16i.c: v0.21 17-10-95 Mika Kuoppala (miku@elt.icl.fi)\n";

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
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/system.h>		  
#include <asm/bitops.h>		  
#include <asm/io.h>		  
#include <asm/dma.h>

/* Few macros */
#define BIT(a)		        ( (1 << (a)) )  
#define BITSET(ioaddr, bnum)   ((outb(((inb(ioaddr)) | (bnum)), ioaddr))) 
#define BITCLR(ioaddr, bnum)   ((outb(((inb(ioaddr)) & (~(bnum))), ioaddr)))

/* This is the I/O address space for Etherteam 16i adapter. */
#define ETH16I_IO_EXTENT 32

/* Ticks before deciding that transmit has timed out */
#define TIMEOUT_TICKS          30

/* Maximum loop count when receiving packets */
#define MAX_RX_LOOP            40

/* Some interrupt masks */
#define ETH16I_INTR_ON	       0x8f82
#define ETH16I_INTR_OFF	       0x0000
	 
/* Buffers header status byte meanings */
#define PKT_GOOD               BIT(5)
#define PKT_GOOD_RMT           BIT(4)
#define PKT_SHORT              BIT(3)
#define PKT_ALIGN_ERR          BIT(2)
#define PKT_CRC_ERR            BIT(1)
#define PKT_RX_BUF_OVERFLOW    BIT(0)

/* Transmit status register (DLCR0) */
#define TX_STATUS_REG          0
#define TX_DONE                BIT(7)
#define NET_BUSY               BIT(6)
#define TX_PKT_RCD             BIT(5)
#define CR_LOST                BIT(4)
#define COLLISION              BIT(2)
#define COLLISIONS_16          BIT(1)

/* Receive status register (DLCR1) */
#define RX_STATUS_REG          1
#define RX_PKT                 BIT(7)  /* Packet received */
#define BUS_RD_ERR             BIT(6)
#define SHORT_PKT_ERR          BIT(3)
#define ALIGN_ERR              BIT(2)
#define CRC_ERR                BIT(1)
#define RX_BUF_OVERFLOW        BIT(0)
              
/* Transmit Interrupt Enable Register (DLCR2) */
#define TX_INTR_REG            2
#define TX_INTR_DONE           BIT(7)
#define TX_INTR_COL            BIT(2)
#define TX_INTR_16_COL         BIT(1)

/* Receive Interrupt Enable Register (DLCR3) */
#define RX_INTR_REG            3
#define RX_INTR_RECEIVE        BIT(7)
#define RX_INTR_SHORT_PKT      BIT(3)
#define RX_INTR_CRC_ERR        BIT(1)
#define RX_INTR_BUF_OVERFLOW   BIT(0)

/* Transmit Mode Register (DLCR4) */
#define TRANSMIT_MODE_REG      4
#define LOOPBACK_CONTROL       BIT(1)
#define CONTROL_OUTPUT         BIT(2)

/* Receive Mode Register (DLCR5) */
#define RECEIVE_MODE_REG       5
#define RX_BUFFER_EMPTY        BIT(6)
#define ACCEPT_BAD_PACKETS     BIT(5)
#define RECEIVE_SHORT_ADDR     BIT(4)
#define ACCEPT_SHORT_PACKETS   BIT(3)
#define REMOTE_RESET           BIT(2)

#define ADDRESS_FILTER_MODE    BIT(1) | BIT(0)
#define REJECT_ALL             0
#define ACCEPT_ALL             3
#define MODE_1                 1            /* NODE ID, BC, MC, 2-24th bit */
#define MODE_2                 2            /* NODE ID, BC, MC, Hash Table */

/* Configuration Register 0 (DLCR6) */
#define CONFIG_REG_0           6
#define DLC_EN                 BIT(7)
#define SRAM_CYCLE_TIME_100NS  BIT(6)
#define SYSTEM_BUS_WIDTH_8     BIT(5)       /* 1 = 8bit, 0 = 16bit */
#define BUFFER_WIDTH_8         BIT(4)       /* 1 = 8bit, 0 = 16bit */
#define TBS1                   BIT(3)       
#define TBS0                   BIT(2)
#define BS1                    BIT(1)       /* 00=8kb,  01=16kb  */
#define BS0                    BIT(0)       /* 10=32kb, 11=64kb  */

#ifndef ETH16I_TX_BUF_SIZE                   /* 0 = 2kb, 1 = 4kb  */ 
#define ETH16I_TX_BUF_SIZE     2             /* 2 = 8kb, 3 = 16kb */
#endif                                      
#define TX_BUF_1x2048            0
#define TX_BUF_2x2048            1
#define TX_BUF_2x4098            2
#define TX_BUF_2x8192            3

/* Configuration Register 1 (DLCR7) */
#define CONFIG_REG_1           7
#define POWERUP                BIT(5)

/* Transmit start register */
#define TRANSMIT_START_REG     10
#define TRANSMIT_START_RB      2
#define TX_START               BIT(7)       /* Rest of register bit indicate*/
                                            /* number of packets in tx buffer*/
/* Node ID registers (DLCR8-13) */
#define NODE_ID_0              8
#define NODE_ID_RB             0

/* Hash Table registers (HT8-15) */
#define HASH_TABLE_0           8
#define HASH_TABLE_RB          1

/* Buffer memory ports */
#define BUFFER_MEM_PORT_LB    8
#define DATAPORT              BUFFER_MEM_PORT_LB
#define BUFFER_MEM_PORT_HB    9

/* 16 Collision control register (BMPR11) */
#define COL_16_REG             11
#define HALT_ON_16             0x00
#define RETRANS_AND_HALT_ON_16 0x02

/* DMA Burst and Transceiver Mode Register (BMPR13) */
#define TRANSCEIVER_MODE_REG   13
#define TRANSCEIVER_MODE_RB    2         
#define IO_BASE_UNLOCK	       BIT(7)
#define LOWER_SQUELCH_TRESH    BIT(6)
#define LINK_TEST_DISABLE      BIT(5)
#define AUI_SELECT             BIT(4)
#define DIS_AUTO_PORT_SEL      BIT(3)

/* Filter Self Receive Register (BMPR14)  */
#define FILTER_SELF_RX_REG     14
#define SKIP_RECEIVE_PACKET    BIT(2)
#define FILTER_SELF_RECEIVE    BIT(0)
#define RX_BUF_SKIP_PACKET     SKIP_RECEIVE_PACKET | FILTER_SELF_RECEIVE

/* EEPROM Control Register (BMPR 16) */
#define EEPROM_CTRL_REG        16

/* EEPROM Data Register (BMPR 17) */
#define EEPROM_DATA_REG        17

/* NMC93CSx6 EEPROM Control Bits */
#define CS_0                   0x00
#define CS_1                   0x20
#define SK_0                   0x00
#define SK_1                   0x40
#define DI_0                   0x00
#define DI_1                   0x80

/* NMC93CSx6 EEPROM Instructions */
#define EEPROM_READ            0x80

/* NMC93CSx6 EEPROM Addresses */
#define E_NODEID_0                     0x02
#define E_NODEID_1                     0x03
#define E_NODEID_2                     0x04
#define E_PORT_SELECT                  0x14
  #define E_PORT_BNC                   0
  #define E_PORT_DIX                   1
  #define E_PORT_TP                    2
  #define E_PORT_AUTO                  3
#define E_PRODUCT_CFG                  0x30
 

/* Macro to slow down io between EEPROM clock transitions */
#define eeprom_slow_io() do { int _i = 40; while(--_i > 0) { __SLOW_DOWN_IO; }}while(0)

/* Jumperless Configuration Register (BMPR19) */
#define JUMPERLESS_CONFIG      19

/* ID ROM registers, writing to them also resets some parts of chip */
#define ID_ROM_0               24
#define ID_ROM_7               31
#define RESET                  ID_ROM_0

/* This is the I/O address list to be probed when seeking the card */
static unsigned int eth16i_portlist[] =
   { 0x260, 0x280, 0x2A0, 0x240, 0x340, 0x320, 0x380, 0x300, 0 };

static unsigned int eth32i_portlist[] =
   { 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000,
     0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000, 0 };

/* This is the Interrupt lookup table for Eth16i card */
static unsigned int eth16i_irqmap[] = { 9, 10, 5, 15 };

/* This is the Interrupt lookup table for Eth32i card */
static unsigned int eth32i_irqmap[] = { 3, 5, 7, 9, 10, 11, 12, 15 };  
#define EISA_IRQ_REG	0xc89

static unsigned int eth16i_tx_buf_map[] = { 2048, 2048, 4096, 8192 };
unsigned int boot = 1;

/* Use 0 for production, 1 for verification, >2 for debug */
#ifndef ETH16I_DEBUG
#define ETH16I_DEBUG 0
#endif
static unsigned int eth16i_debug = ETH16I_DEBUG;

/* Information for each board */
struct eth16i_local {
  struct enet_statistics stats;
  unsigned int tx_started:1;
  unsigned char tx_queue;         /* Number of packets in transmit buffer */
  unsigned short tx_queue_len;         
  unsigned int tx_buf_size;
  unsigned long open_time;
};

/* Function prototypes */

extern int eth16i_probe(struct device *dev);

static int eth16i_probe1(struct device *dev, short ioaddr);
static int eth16i_check_signature(short ioaddr);
static int eth16i_probe_port(short ioaddr);
static void eth16i_set_port(short ioaddr, int porttype);
static int eth16i_send_probe_packet(short ioaddr, unsigned char *b, int l);
static int eth16i_receive_probe_packet(short ioaddr);
static int eth16i_get_irq(short ioaddr);
static int eth16i_read_eeprom(int ioaddr, int offset);
static int eth16i_read_eeprom_word(int ioaddr);
static void eth16i_eeprom_cmd(int ioaddr, unsigned char command);
static int eth16i_open(struct device *dev);
static int eth16i_close(struct device *dev);
static int eth16i_tx(struct sk_buff *skb, struct device *dev);
static void eth16i_rx(struct device *dev);
static void eth16i_interrupt(int irq, struct pt_regs *regs);
static void eth16i_multicast(struct device *dev, int num_addrs, void *addrs); 
static void eth16i_select_regbank(unsigned char regbank, short ioaddr);
static void eth16i_initialize(struct device *dev);
static struct enet_statistics *eth16i_get_stats(struct device *dev);

static char *cardname = "ICL EtherTeam 16i/32";

#ifdef HAVE_DEVLIST 
/* Support for alternate probe manager */
/struct netdev_entry eth16i_drv = 
   {"eth16i", eth16i_probe1, ETH16I_IO_EXTENT, eth16i_probe_list}; 

#else  /* Not HAVE_DEVLIST */
int eth16i_probe(struct device *dev)
{
  int i;
  int ioaddr;
  int base_addr = dev ? dev->base_addr : 0;

  if(eth16i_debug > 4) 
    printk("Probing started for %s\n", cardname);

  if(base_addr > 0x1ff)           /* Check only single location */
    return eth16i_probe1(dev, base_addr);
  else if(base_addr != 0)         /* Don't probe at all */
    return ENXIO;

  /* Seek card from the ISA io address space */
  for(i = 0; (ioaddr = eth16i_portlist[i]) ; i++) {
    if(check_region(ioaddr, ETH16I_IO_EXTENT))
      continue;
    if(eth16i_probe1(dev, ioaddr) == 0)
      return 0;
  }

  /* Seek card from the EISA io address space */
  for(i = 0; (ioaddr = eth32i_portlist[i]) ; i++) {
    if(check_region(ioaddr, ETH16I_IO_EXTENT))
	continue;
    if(eth16i_probe1(dev, ioaddr) == 0)
	return 0;
   }

  return ENODEV;
}
#endif  /* Not HAVE_DEVLIST */

static int eth16i_probe1(struct device *dev, short ioaddr)
{
  static unsigned version_printed = 0;
  unsigned int irq = 0;
  boot = 1;          /* To inform initilization that we are in boot probe */

  /*
     The MB86985 chip has on register which holds information in which 
     io address the chip lies. First read this register and compare
     it to our current io address and if match then this could
     be our chip.
  */
  
  if(ioaddr < 0x1000) {
    if(eth16i_portlist[(inb(ioaddr + JUMPERLESS_CONFIG) & 0x07)] != ioaddr)
      return -ENODEV;
  }

  /* Now we will go a bit deeper and try to find the chip's signature */

  if(eth16i_check_signature(ioaddr) != 0) /* Can we find the signature here */
    return -ENODEV;
  
  /* 
     Now it seems that we have found a ethernet chip in this particular
     ioaddr. The MB86985 chip has this feature, that when you read a 
     certain register it will increase it's io base address to next
     configurable slot. Now when we have found the chip, first thing is
     to make sure that the chip's ioaddr will hold still here.
  */
  
  eth16i_select_regbank(TRANSCEIVER_MODE_RB, ioaddr);
  outb(0x00, ioaddr + TRANSCEIVER_MODE_REG);
  
  outb(0x00, ioaddr + RESET);             /* Will reset some parts of chip */
  BITSET(ioaddr + CONFIG_REG_0, BIT(7));  /* This will disable the data link */
  
  if(dev == NULL)
    dev = init_etherdev(0, sizeof(struct eth16i_local));

  if( (eth16i_debug & version_printed++) == 0)
    printk(version);

  dev->base_addr = ioaddr;
 
  irq = eth16i_get_irq(ioaddr);
  dev->irq = irq;

  /* Try to obtain interrupt vector */
  if(request_irq(dev->irq, &eth16i_interrupt, 0, "eth16i")) {
    printk("%s: %s at %#3x, but is unusable due 
           conflict on IRQ %d.\n", dev->name, cardname, ioaddr, irq);
    return EAGAIN;
  }

  printk("%s: %s at %#3x, IRQ %d, ", 
	 dev->name, cardname, ioaddr, dev->irq);

  /* Let's grab the region */
  request_region(ioaddr, ETH16I_IO_EXTENT, "eth16i");

  /* Now we will have to lock the chip's io address */
  eth16i_select_regbank(TRANSCEIVER_MODE_RB, ioaddr);
  outb(0x38, ioaddr + TRANSCEIVER_MODE_REG); 

  eth16i_initialize(dev);   /* Initialize rest of the chip's registers */
  
  /* Now let's same some energy by shutting down the chip ;) */
  BITCLR(ioaddr + CONFIG_REG_1, POWERUP);
   
  /* Initialize the device structure */
  if(dev->priv == NULL)
    dev->priv = kmalloc(sizeof(struct eth16i_local), GFP_KERNEL);
  memset(dev->priv, 0, sizeof(struct eth16i_local));

  dev->open               = eth16i_open;
  dev->stop               = eth16i_close;
  dev->hard_start_xmit    = eth16i_tx;
  dev->get_stats          = eth16i_get_stats;
  dev->set_multicast_list = &eth16i_multicast;

  /* Fill in the fields of the device structure with ethernet values. */
  ether_setup(dev);

  boot = 0;

  return 0;
}


static void eth16i_initialize(struct device *dev)
{
  short ioaddr = dev->base_addr;
  int i, node_w = 0;
  unsigned char node_byte = 0;

  /* Setup station address */
  eth16i_select_regbank(NODE_ID_RB, ioaddr);
  for(i = 0 ; i < 3 ; i++) {
    unsigned short node_val = eth16i_read_eeprom(ioaddr, E_NODEID_0 + i);
    ((unsigned short *)dev->dev_addr)[i] = ntohs(node_val);
  }

  for(i = 0; i < 6; i++) { 
    outb( ((unsigned char *)dev->dev_addr)[i], ioaddr + NODE_ID_0 + i);
    if(boot) {
      printk("%02x", inb(ioaddr + NODE_ID_0 + i));
      if(i != 5)
	printk(":");
    }
  }

  /* Now we will set multicast addresses to accept none */
  eth16i_select_regbank(HASH_TABLE_RB, ioaddr);
  for(i = 0; i < 8; i++) 
    outb(0x00, ioaddr + HASH_TABLE_0 + i);

  /*
     Now let's disable the transmitter and receiver, set the buffer ram 
     cycle time, bus width and buffer data path width. Also we shall
     set transmit buffer size and total buffer size.
  */

  eth16i_select_regbank(2, ioaddr);

  node_byte = 0;
  node_w = eth16i_read_eeprom(ioaddr, E_PRODUCT_CFG);

  if( (node_w & 0xFF00) == 0x0800)
    node_byte |= BUFFER_WIDTH_8;

  node_byte |= BS1;

  if( (node_w & 0x00FF) == 64)
    node_byte |= BS0;
  
  node_byte |= DLC_EN | SRAM_CYCLE_TIME_100NS | (ETH16I_TX_BUF_SIZE << 2);

  outb(node_byte, ioaddr + CONFIG_REG_0);

  /* We shall halt the transmitting, if 16 collisions are detected */
  outb(RETRANS_AND_HALT_ON_16, ioaddr + COL_16_REG);

  if(boot) /* Now set port type */
  {
    char *porttype[] = {"BNC", "DIX", "TP", "AUTO"};
    
    ushort ptype = eth16i_read_eeprom(ioaddr, E_PORT_SELECT);
    dev->if_port = (ptype & 0x00FF);
    
    printk(" %s interface.\n", porttype[dev->if_port]);

    if(ptype == E_PORT_AUTO)
      ptype = eth16i_probe_port(ioaddr);
    
    eth16i_set_port(ioaddr, ptype);
  }

  /* Set Receive Mode to normal operation */
  outb(MODE_2, ioaddr + RECEIVE_MODE_REG);
}

static int eth16i_probe_port(short ioaddr)
{
  int i;
  int retcode;
  unsigned char dummy_packet[64] = { 0 };

  /* Powerup the chip */
  outb(0xc0 | POWERUP, ioaddr + CONFIG_REG_1);

  BITSET(ioaddr + CONFIG_REG_0, DLC_EN);

  eth16i_select_regbank(NODE_ID_RB, ioaddr);

  for(i = 0; i < 6; i++) {
    dummy_packet[i] = inb(ioaddr + NODE_ID_0 + i);
    dummy_packet[i+6] = inb(ioaddr + NODE_ID_0 + i);
  }

  dummy_packet[12] = 0x00;
  dummy_packet[13] = 0x04;

  eth16i_select_regbank(2, ioaddr);

  for(i = 0; i < 3; i++) {
    BITSET(ioaddr + CONFIG_REG_0, DLC_EN);
    BITCLR(ioaddr + CONFIG_REG_0, DLC_EN);
    eth16i_set_port(ioaddr, i);
   
    if(eth16i_debug > 1)
    	printk("Set port number %d\n", i);

    retcode = eth16i_send_probe_packet(ioaddr, dummy_packet, 64);
    if(retcode == 0) {
      retcode = eth16i_receive_probe_packet(ioaddr);
      if(retcode != -1) {
	if(eth16i_debug > 1)
		printk("Eth16i interface port found at %d\n", i);
	return i;
      }
    }
    else {
      if(eth16i_debug > 1)
      	printk("TRANSMIT_DONE timeout\n");
    }
  }
  
  if( eth16i_debug > 1)
  	printk("Using default port\n");
 
 return E_PORT_BNC;
}

static void eth16i_set_port(short ioaddr, int porttype)
{ 
    unsigned short temp = 0;

    eth16i_select_regbank(TRANSCEIVER_MODE_RB, ioaddr);
    outb(LOOPBACK_CONTROL, ioaddr + TRANSMIT_MODE_REG);

    temp |= DIS_AUTO_PORT_SEL;

    switch(porttype) {

    case E_PORT_BNC :
      temp |= AUI_SELECT;
      break;

    case E_PORT_TP :
      break;
      
    case E_PORT_DIX :
      temp |= AUI_SELECT;
      BITSET(ioaddr + TRANSMIT_MODE_REG, CONTROL_OUTPUT);
      break;
    }  
    outb(temp, ioaddr + TRANSCEIVER_MODE_REG);

    if(eth16i_debug > 1) {
    	printk("TRANSMIT_MODE_REG = %x\n", inb(ioaddr + TRANSMIT_MODE_REG));
    	printk("TRANSCEIVER_MODE_REG = %x\n", inb(ioaddr+TRANSCEIVER_MODE_REG));
    }
}

static int eth16i_send_probe_packet(short ioaddr, unsigned char *b, int l)
{
  int starttime;

  outb(0xff, ioaddr + TX_STATUS_REG);

  outw(l, ioaddr + DATAPORT);
  outsw(ioaddr + DATAPORT, (unsigned short *)b, (l + 1) >> 1);  
  
  starttime = jiffies;
  outb(TX_START | 1, ioaddr + TRANSMIT_START_REG); 

  while( (inb(ioaddr + TX_STATUS_REG) & 0x80) == 0) {
    if( (jiffies - starttime) > TIMEOUT_TICKS) {
      break;
    }
  }
  
  return(0);
}

static int eth16i_receive_probe_packet(short ioaddr)
{
  int starttime;
  
  starttime = jiffies;
  
  while((inb(ioaddr + TX_STATUS_REG) & 0x20) == 0) {
    if( (jiffies - starttime) > TIMEOUT_TICKS) {
      
      if(eth16i_debug > 1)
	printk("Timeout occured waiting transmit packet received\n");
      starttime = jiffies;
      while((inb(ioaddr + RX_STATUS_REG) & 0x80) == 0) {
	if( (jiffies - starttime) > TIMEOUT_TICKS) {
	if(eth16i_debug > 1)
	  printk("Timeout occured waiting receive packet\n");
        return -1;
        }
      }
      
      if(eth16i_debug > 1)
      	printk("RECEIVE_PACKET\n");
      return(0); /* Found receive packet */
    }
  }

  if(eth16i_debug > 1) {
  	printk("TRANSMIT_PACKET_RECEIVED %x\n", inb(ioaddr + TX_STATUS_REG));
  	printk("RX_STATUS_REG = %x\n", inb(ioaddr + RX_STATUS_REG));
  }

  return(0); /* Return success */
}

static int eth16i_get_irq(short ioaddr)
{
  unsigned char cbyte;
  
  if( ioaddr < 0x1000) {
  	cbyte = inb(ioaddr + JUMPERLESS_CONFIG);
	return( eth16i_irqmap[ ((cbyte & 0xC0) >> 6) ] );
  } else {  /* Oh..the card is EISA so method getting IRQ different */
	unsigned short index = 0;
	cbyte = inb(ioaddr + EISA_IRQ_REG);
	while( (cbyte & 0x01) == 0) {
		cbyte = cbyte >> 1;
		index++;
        }
	return( eth32i_irqmap[ index ] );
  }
}

static int eth16i_check_signature(short ioaddr)
{
  int i;
  unsigned char creg[4] = { 0 };
  
  for(i = 0; i < 4 ; i++) {

    creg[i] = inb(ioaddr + TRANSMIT_MODE_REG + i);
  
    if(eth16i_debug > 1)
	printk("eth16i: read signature byte %x at %x\n", creg[i],
	       ioaddr + TRANSMIT_MODE_REG + i);
  }

  creg[0] &= 0x0F;      /* Mask collision cnr */
  creg[2] &= 0x7F;      /* Mask DCLEN bit */

#ifdef 0
/* 
	This was removed because the card was sometimes left to state
  	from which it couldn't be find anymore. If there is need
	to more strict chech still this have to be fixed.
*/
  if( !( (creg[0] == 0x06) && (creg[1] == 0x41)) ) {
    if(creg[1] != 0x42)
      return -1;
  }
#endif

  if( !( (creg[2] == 0x36) && (creg[3] == 0xE0)) ) {
      creg[2] &= 0x42;
      creg[3] &= 0x03;

      if( !( (creg[2] == 0x42) && (creg[3] == 0x00)) )
	return -1;
  }

  if(eth16i_read_eeprom(ioaddr, E_NODEID_0) != 0)
    return -1;
  if((eth16i_read_eeprom(ioaddr, E_NODEID_1) & 0xFF00) != 0x4B00)
    return -1;

  return 0;
}

static int eth16i_read_eeprom(int ioaddr, int offset)
{
  int data = 0;

  eth16i_eeprom_cmd(ioaddr, EEPROM_READ | offset);
  outb(CS_1, ioaddr + EEPROM_CTRL_REG);
  data = eth16i_read_eeprom_word(ioaddr);
  outb(CS_0 | SK_0, ioaddr + EEPROM_CTRL_REG);

  return(data);  
}

static int eth16i_read_eeprom_word(int ioaddr)
{
  int i;
  int data = 0;

  for(i = 16; i > 0; i--) {
    outb(CS_1 | SK_0, ioaddr + EEPROM_CTRL_REG);
    eeprom_slow_io();
    outb(CS_1 | SK_1, ioaddr + EEPROM_CTRL_REG);
    eeprom_slow_io();
    data = (data << 1) | ((inb(ioaddr + EEPROM_DATA_REG) & DI_1) ? 1 : 0);
    eeprom_slow_io();
  }

  return(data);
}

static void eth16i_eeprom_cmd(int ioaddr, unsigned char command)
{
  int i;
  
  outb(CS_0 | SK_0, ioaddr + EEPROM_CTRL_REG);
  outb(DI_0, ioaddr + EEPROM_DATA_REG);
  outb(CS_1 | SK_0, ioaddr + EEPROM_CTRL_REG);
  outb(DI_1, ioaddr + EEPROM_DATA_REG);
  outb(CS_1 | SK_1, ioaddr + EEPROM_CTRL_REG);

  for(i = 7; i >= 0; i--) {
    short cmd = ( (command & (1 << i)) ? DI_1 : DI_0 );
    outb(cmd, ioaddr + EEPROM_DATA_REG);
    outb(CS_1 | SK_0, ioaddr + EEPROM_CTRL_REG);
    eeprom_slow_io();
    outb(CS_1 | SK_1, ioaddr + EEPROM_CTRL_REG);
    eeprom_slow_io();
  } 
}

static int eth16i_open(struct device *dev)
{
  struct eth16i_local *lp = (struct eth16i_local *)dev->priv;
  int ioaddr = dev->base_addr;
  
  irq2dev_map[dev->irq] = dev;

  /* Powerup the chip */
  outb(0xc0 | POWERUP, ioaddr + CONFIG_REG_1);
  
  /* Initialize the chip */
  eth16i_initialize(dev);  
  
  /* Set the transmit buffer size */
  lp->tx_buf_size = eth16i_tx_buf_map[ETH16I_TX_BUF_SIZE & 0x03];
  
  if(eth16i_debug > 3)
    printk("%s: transmit buffer size %d\n", dev->name, lp->tx_buf_size);

  /* Now enable Transmitter and Receiver sections */
  BITCLR(ioaddr + CONFIG_REG_0, DLC_EN);
 
  /* Now switch to register bank 2, for run time operation */
  eth16i_select_regbank(2, ioaddr);

  lp->open_time = jiffies;
  lp->tx_started = 0;
  lp->tx_queue = 0;
  lp->tx_queue_len = 0;

  /* Turn on interrupts*/
  outw(ETH16I_INTR_ON, ioaddr + TX_INTR_REG);  

  dev->tbusy = 0;
  dev->interrupt = 0;
  dev->start = 1;

#ifdef MODULE
  MOD_INC_USE_COUNT;
#endif

  return 0;
}

static int eth16i_close(struct device *dev)
{
  struct eth16i_local *lp = (struct eth16i_local *)dev->priv;
  int ioaddr = dev->base_addr;

  lp->open_time = 0;

  dev->tbusy = 1;
  dev->start = 0;

  /* Disable transmit and receive */
  BITSET(ioaddr + CONFIG_REG_0, DLC_EN);

  /* Reset the chip */
  outb(0xff, ioaddr + RESET);

  /* Save some energy by switching off power */
  BITCLR(ioaddr + CONFIG_REG_1, POWERUP);

#ifdef MODULE
  MOD_DEC_USE_COUNT;
#endif

  return 0;
}

static int eth16i_tx(struct sk_buff *skb, struct device *dev)
{
  struct eth16i_local *lp = (struct eth16i_local *)dev->priv;
  int ioaddr = dev->base_addr;

  if(dev->tbusy) {
    /* 
       If we get here, some higher level has decided that we are broken. 
       There should really be a "kick me" function call instead. 
    */
    
    int tickssofar = jiffies - dev->trans_start;
    if(tickssofar < TIMEOUT_TICKS)  /* Let's not rush with our timeout, */  
      return 1;                     /* wait a couple of ticks first     */

    printk("%s: transmit timed out with status %04x, %s ?\n", dev->name,
	   inw(ioaddr + TX_STATUS_REG), 
	   (inb(ioaddr + TX_STATUS_REG) & TX_DONE) ? 
	   "IRQ conflict" : "network cable problem");

    /* Let's dump all registers */
    if(eth16i_debug > 0) { 
      printk("%s: timeout regs: %02x %02x %02x %02x %02x %02x %02x %02x.\n",
	     dev->name, inb(ioaddr + 0), inb(ioaddr + 1), inb(ioaddr + 2), 
	     inb(ioaddr + 3), inb(ioaddr + 4), inb(ioaddr + 5),
	     inb(ioaddr + 6), inb(ioaddr + 7));


      printk("lp->tx_queue = %d\n", lp->tx_queue);
      printk("lp->tx_queue_len = %d\n", lp->tx_queue_len);
      printk("lp->tx_started = %d\n", lp->tx_started);

    }

    lp->stats.tx_errors++;

    /* Now let's try to restart the adaptor */
    
    BITSET(ioaddr + CONFIG_REG_0, DLC_EN);
    outw(0xffff, ioaddr + RESET);
    eth16i_initialize(dev);
    outw(0xffff, ioaddr + TX_STATUS_REG);
    BITCLR(ioaddr + CONFIG_REG_0, DLC_EN);
            
    lp->tx_started = 0;
    lp->tx_queue = 0;
    lp->tx_queue_len = 0;
    
    outw(ETH16I_INTR_ON, ioaddr + TX_INTR_REG);
    
    dev->tbusy = 0;
    dev->trans_start = jiffies;
  }

  /* 
     If some higher layer thinks we've missed an tx-done interrupt
     we are passed NULL. Caution: dev_tint() handles the cli()/sti()
     itself 
  */
  if(skb == NULL) {
    dev_tint(dev);
    return 0;
  }

  /* Block a timer based transmitter from overlapping. This could better be
     done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */

  /* Turn off TX interrupts */
  outw(ETH16I_INTR_OFF, ioaddr + TX_INTR_REG);

  if(set_bit(0, (void *)&dev->tbusy) != 0)
    printk("%s: Transmitter access conflict.\n", dev->name);
  else {
    short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
    unsigned char *buf = skb->data;
  
    outw(length, ioaddr + DATAPORT);
    
    if( ioaddr < 0x1000 ) 
    	outsw(ioaddr + DATAPORT, buf, (length + 1) >> 1);
    else {
	unsigned char frag = length % 4;

	outsl(ioaddr + DATAPORT, buf, length >> 2);
	
	if( frag != 0 ) {
	  outsw(ioaddr + DATAPORT, (buf + (length & 0xFFFC)), 1);
	  if( frag == 3 ) 
	    outsw(ioaddr + DATAPORT, (buf + (length & 0xFFFC) + 2), 1);
	}
    }

    lp->tx_queue++;
    lp->tx_queue_len += length + 2;

    if(lp->tx_started == 0) {
      /* If the transmitter is idle..always trigger a transmit */
      outb(TX_START | lp->tx_queue, ioaddr + TRANSMIT_START_REG);
      lp->tx_queue = 0;
      lp->tx_queue_len = 0;
      dev->trans_start = jiffies;
      lp->tx_started = 1;
      dev->tbusy = 0;
    }
    else if(lp->tx_queue_len < lp->tx_buf_size - (ETH_FRAME_LEN + 2)) {
      /* There is still more room for one more packet in tx buffer */
      dev->tbusy = 0;
    }
        
    outw(ETH16I_INTR_ON, ioaddr + TX_INTR_REG);

    /* Turn TX interrupts back on */
    /* outb(TX_INTR_DONE | TX_INTR_16_COL, ioaddr + TX_INTR_REG); */
  } 
  dev_kfree_skb(skb, FREE_WRITE);

  return 0;
}

static void eth16i_rx(struct device *dev)
{
  struct eth16i_local *lp = (struct eth16i_local *)dev->priv;
  int ioaddr = dev->base_addr;
  int boguscount = MAX_RX_LOOP;

  /* Loop until all packets have been read */
  while( (inb(ioaddr + RECEIVE_MODE_REG) & RX_BUFFER_EMPTY) == 0) {
    
    /* Read status byte from receive buffer */ 
    ushort status = inw(ioaddr + DATAPORT);

    if(eth16i_debug > 4)
      printk("%s: Receiving packet mode %02x status %04x.\n", 
	     dev->name, inb(ioaddr + RECEIVE_MODE_REG), status);
  
      if( !(status & PKT_GOOD) ) {
	/* Hmm..something went wrong. Let's check what error occured */
	lp->stats.rx_errors++;
	if( status & PKT_SHORT     ) lp->stats.rx_length_errors++;
	if( status & PKT_ALIGN_ERR ) lp->stats.rx_frame_errors++;
	if( status & PKT_CRC_ERR   ) lp->stats.rx_crc_errors++;
	if( status & PKT_RX_BUF_OVERFLOW) lp->stats.rx_over_errors++;
      }
      else {   /* Ok so now we should have a good packet */
	struct sk_buff *skb;

	/* Get the size of the packet from receive buffer */
	ushort pkt_len = inw(ioaddr + DATAPORT);

	if(pkt_len > ETH_FRAME_LEN) {
	  printk("%s: %s claimed a very large packet, size of %d bytes.\n", 
		 dev->name, cardname, pkt_len);
	  outb(RX_BUF_SKIP_PACKET, ioaddr + FILTER_SELF_RX_REG);
	  lp->stats.rx_dropped++;
	  break;
	}

	skb = dev_alloc_skb(pkt_len + 3);
	if( skb == NULL ) {
	  printk("%s: Could'n allocate memory for packet (len %d)\n", 
		 dev->name, pkt_len);
	  outb(RX_BUF_SKIP_PACKET, ioaddr + FILTER_SELF_RX_REG);
	  lp->stats.rx_dropped++;
	  break;
	}
	
	skb->dev = dev;
	skb_reserve(skb,2);
	/* 
	   Now let's get the packet out of buffer.
	   size is (pkt_len + 1) >> 1, cause we are now reading words
	   and it have to be even aligned.
	*/ 

	if( ioaddr < 0x1000) 
	  insw(ioaddr + DATAPORT, skb_put(skb, pkt_len), (pkt_len + 1) >> 1);
	else {	
	  unsigned char *buf = skb_put(skb, pkt_len);
	  unsigned char frag = pkt_len % 4;

	  insl(ioaddr + DATAPORT, buf, pkt_len >> 2);
	
	  if(frag != 0) {
	  	unsigned short rest[2];
		rest[0] = inw( ioaddr + DATAPORT );
		if(frag == 3)
			rest[1] = inw( ioaddr + DATAPORT );

		memcpy(buf + (pkt_len & 0xfffc), (char *)rest, frag);
	  }
	}
	
        skb->protocol=eth_type_trans(skb, dev);
	netif_rx(skb);
	lp->stats.rx_packets++;
         
	if( eth16i_debug > 5 ) {
	  int i;
	  printk("%s: Received packet of length %d.\n", dev->name, pkt_len);
	  for(i = 0; i < 14; i++) 
	    printk(" %02x", skb->data[i]);
	  printk(".\n");
	}
	
      } /* else */

    if(--boguscount <= 0)
      break;

  } /* while */

#if 0
  {
    int i;

    for(i = 0; i < 20; i++) {
      if( (inb(ioaddr+RECEIVE_MODE_REG) & RX_BUFFER_EMPTY) == RX_BUFFER_EMPTY)
	break;
      inw(ioaddr + DATAPORT);
      outb(RX_BUF_SKIP_PACKET, ioaddr + FILTER_SELF_RX_REG);
    }

    if(eth16i_debug > 1)
      printk("%s: Flushed receive buffer.\n", dev->name);
  }
#endif

  return;
}

static void eth16i_interrupt(int irq, struct pt_regs *regs)
{
  struct device *dev = (struct device *)(irq2dev_map[irq]);
  struct eth16i_local *lp;
  int ioaddr = 0,
      status;

  if(dev == NULL) {
    printk("eth16i_interrupt(): irq %d for unknown device. \n", irq);
    return;
  }

  /* Turn off all interrupts from adapter */
  outw(ETH16I_INTR_OFF, ioaddr + TX_INTR_REG); 

  dev->interrupt = 1;

  ioaddr = dev->base_addr;
  lp = (struct eth16i_local *)dev->priv;
  status = inw(ioaddr + TX_STATUS_REG);      /* Get the status */
  outw(status, ioaddr + TX_STATUS_REG);      /* Clear status bits */
  
  if(eth16i_debug > 3)
    printk("%s: Interrupt with status %04x.\n", dev->name, status);

  if( status & 0x00ff ) {          /* Let's check the transmit status reg */
    
    if(status & TX_DONE) {         /* The transmit has been done */
      lp->stats.tx_packets++;

      if(lp->tx_queue) {           /* Is there still packets ? */
	  /* There was packet(s) so start transmitting and write also
	   how many packets there is to be sended */
	outb(TX_START | lp->tx_queue, ioaddr + TRANSMIT_START_REG);
	lp->tx_queue = 0;
	lp->tx_queue_len = 0;
	dev->trans_start = jiffies;
	dev->tbusy = 0;
	mark_bh(NET_BH);
      }
      else {
	lp->tx_started = 0;
	dev->tbusy = 0;
	mark_bh(NET_BH);
      }
    }
  }
    
  if( ( status & 0xff00 ) || 
     ( (inb(ioaddr + RECEIVE_MODE_REG) & RX_BUFFER_EMPTY) == 0) ) {
    eth16i_rx(dev);  /* We have packet in receive buffer */
  }

  dev->interrupt = 0;
  
  /* Turn interrupts back on */
  outw(ETH16I_INTR_ON, ioaddr + TX_INTR_REG);

  return;
}

static void eth16i_multicast(struct device *dev, int num_addrs, void *addrs)
{
  short ioaddr = dev->base_addr;
  
  if(dev->mc_count || dev->flags&(IFF_ALLMULTI|IFF_PROMISC)) 
  {
    dev->flags|=IFF_PROMISC;	/* Must do this */
    outb(3, ioaddr + RECEIVE_MODE_REG);    
  } else {
    outb(2, ioaddr + RECEIVE_MODE_REG);
  }
}

static struct enet_statistics *eth16i_get_stats(struct device *dev)
{
  struct eth16i_local *lp = (struct eth16i_local *)dev->priv;

  return &lp->stats;
}

static void eth16i_select_regbank(unsigned char banknbr, short ioaddr)
{
  unsigned char data;

  data = inb(ioaddr + CONFIG_REG_1);
  outb( ((data & 0xF3) | ( (banknbr & 0x03) << 2)), ioaddr + CONFIG_REG_1); 
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_eth16i = {
	devicename,
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, eth16i_probe };

int io = 0x2a0;
int irq = 0;

int init_module(void)
{
	if(io == 0)
		printk("eth16i: You should not use auto-probing with insmod!\n");
	
	dev_eth16i.base_addr = io;
	dev_eth16i.irq = irq;
	if( register_netdev( &dev_eth16i ) != 0 ) {
		printk("eth16i: register_netdev() returned non-zero.\n");
		return -EIO;
	}

	return 0;
}

void cleanup_module(void)
{
	unregister_netdev( &dev_eth16i );
	free_irq( dev_eth16i.irq );
	irq2dev_map[ dev_eth16i.irq ] = NULL;
	release_region( dev_eth16i.base_addr, ETH16I_IO_EXTENT );
}

#endif /* MODULE */


