/*  depca.c: A DIGITAL DEPCA  & EtherWORKS ethernet driver for linux.

    Written 1994, 1995 by David C. Davies.


                      Copyright 1994 David C. Davies
		                   and 
			 United States Government
	 (as represented by the Director, National Security Agency).  

               Copyright 1995  Digital Equipment Corporation.


    This software may be used and distributed according to the terms of
    the GNU Public License, incorporated herein by reference.

    This driver is written for the Digital Equipment Corporation series
    of DEPCA and EtherWORKS ethernet cards:

        DEPCA       (the original)
    	DE100
    	DE101
	DE200 Turbo
	DE201 Turbo
	DE202 Turbo (TP BNC)
	DE210
	DE422       (EISA)

    The  driver has been tested on DE100, DE200 and DE202 cards  in  a
    relatively busy network. The DE422 has been tested a little.

    This  driver will NOT work   for the DE203,  DE204  and DE205 series  of
    cards,  since they have  a  new custom ASIC in   place of the AMD  LANCE
    chip.  See the 'ewrk3.c'   driver in the  Linux  source tree for running
    those cards.

    I have benchmarked the driver with a  DE100 at 595kB/s to (542kB/s from)
    a DECstation 5000/200.

    The   author   may    be   reached  at    davies@wanton.lkg.dec.com   or
    davies@maniac.ultranet.com  or Digital  Equipment Corporation, 550  King
    Street, Littleton MA 01460.

    =========================================================================

    The  driver was originally based  on   the 'lance.c' driver from  Donald
    Becker   which  is included with  the  standard  driver distribution for
    linux.  V0.4  is  a complete  re-write  with only  the kernel  interface
    remaining from the original code.

    1) Lance.c code in /linux/drivers/net/
    2) "Ethernet/IEEE 802.3 Family. 1992 World Network Data Book/Handbook",
       AMD, 1992 [(800) 222-9323].
    3) "Am79C90 CMOS Local Area Network Controller for Ethernet (C-LANCE)",
       AMD, Pub. #17881, May 1993.
    4) "Am79C960 PCnet-ISA(tm), Single-Chip Ethernet Controller for ISA",
       AMD, Pub. #16907, May 1992
    5) "DEC EtherWORKS LC Ethernet Controller Owners Manual",
       Digital Equipment corporation, 1990, Pub. #EK-DE100-OM.003
    6) "DEC EtherWORKS Turbo Ethernet Controller Owners Manual",
       Digital Equipment corporation, 1990, Pub. #EK-DE200-OM.003
    7) "DEPCA Hardware Reference Manual", Pub. #EK-DEPCA-PR
       Digital Equipment Corporation, 1989
    8) "DEC EtherWORKS Turbo_(TP BNC) Ethernet Controller Owners Manual",
       Digital Equipment corporation, 1991, Pub. #EK-DE202-OM.001
    

    Peter Bauer's depca.c (V0.5) was referred to when debugging V0.1 of this
    driver.

    The original DEPCA  card requires that the  ethernet ROM address counter
    be enabled to count and has an 8 bit NICSR.  The ROM counter enabling is
    only  done when a  0x08 is read as the  first address octet (to minimise
    the chances  of writing over some  other hardware's  I/O register).  The
    NICSR accesses   have been changed  to  byte accesses  for all the cards
    supported by this driver, since there is only one  useful bit in the MSB
    (remote boot timeout) and it  is not used.  Also, there  is a maximum of
    only 48kB network  RAM for this  card.  My thanks  to Torbjorn Lindh for
    help debugging all this (and holding my feet to  the fire until I got it
    right).

    The DE200  series  boards have  on-board 64kB  RAM for  use  as a shared
    memory network  buffer. Only the DE100  cards make use  of a  2kB buffer
    mode which has not  been implemented in  this driver (only the 32kB  and
    64kB modes are supported [16kB/48kB for the original DEPCA]).

    At the most only 2 DEPCA cards can  be supported on  the ISA bus because
    there is only provision  for two I/O base addresses  on each card (0x300
    and 0x200). The I/O address is detected by searching for a byte sequence
    in the Ethernet station address PROM at the expected I/O address for the
    Ethernet  PROM.   The shared memory  base   address  is 'autoprobed'  by
    looking  for the self  test PROM  and detecting the  card name.   When a
    second  DEPCA is  detected,  information  is   placed in the   base_addr
    variable of the  next device structure (which  is created if necessary),
    thus  enabling ethif_probe  initialization  for the device.  More than 2
    EISA cards can  be  supported, but  care will  be  needed assigning  the
    shared memory to ensure that each slot has the  correct IRQ, I/O address
    and shared memory address assigned.

    ************************************************************************

    NOTE: If you are using two  ISA DEPCAs, it is  important that you assign
    the base memory addresses correctly.   The  driver autoprobes I/O  0x300
    then 0x200.  The  base memory address for  the first device must be less
    than that of the second so that the auto probe will correctly assign the
    I/O and memory addresses on the same card.  I can't think of a way to do
    this unambiguously at the moment, since there is nothing on the cards to
    tie I/O and memory information together.

    I am unable  to  test  2 cards   together for now,    so this  code   is
    unchecked. All reports, good or bad, are welcome.

    ************************************************************************

    The board IRQ   setting must be  at an  unused IRQ which  is auto-probed
    using Donald Becker's autoprobe routines. DEPCA and DE100 board IRQs are
    {2,3,4,5,7}, whereas the  DE200 is at {5,9,10,11,15}.  Note that IRQ2 is
    really IRQ9 in machines with 16 IRQ lines.

    No 16MB memory  limitation should exist with this  driver as DMA is  not
    used and the common memory area is in low memory on the network card (my
    current system has 20MB and I've not had problems yet).

    The ability to load this driver as a loadable module has been added. To
    utilise this ability, you have to do <8 things:

    0) have a copy of the loadable modules code installed on your system.
    1) copy depca.c from the  /linux/drivers/net directory to your favourite
    temporary directory.
    2) if you wish, edit the  source code near  line 1530 to reflect the I/O
    address and IRQ you're using (see also 5).
    3) compile  depca.c, but include -DMODULE in  the command line to ensure
    that the correct bits are compiled (see end of source code).
    4) if you are wanting to add a new  card, goto 5. Otherwise, recompile a
    kernel with the depca configuration turned off and reboot.
    5) insmod depca.o [irq=7] [io=0x200] [mem=0xd0000] [adapter_name=DE100]
       [Alan Cox: Changed the code to allow command line irq/io assignments]
       [Dave Davies: Changed the code to allow command line mem/name
                                                                assignments]
    6) run the net startup bits for your eth?? interface manually 
    (usually /etc/rc.inet[12] at boot time). 
    7) enjoy!

    Note that autoprobing is not allowed in loadable modules - the system is
    already up and running and you're messing with interrupts.

    To unload a module, turn off the associated interface 
    'ifconfig eth?? down' then 'rmmod depca'.

    To assign a base memory address for the shared memory  when running as a
    loadable module, see 5 above.  To include the adapter  name (if you have
    no PROM  but know the card name)  also see 5  above. Note that this last
    option  will not work  with kernel  built-in  depca's. 

    The shared memory assignment for a loadable module  makes sense to avoid
    the 'memory autoprobe' picking the wrong shared memory  (for the case of
    2 depca's in a PC).


    TO DO:
    ------


    Revision History
    ----------------

    Version   Date        Description
  
      0.1     25-jan-94   Initial writing.
      0.2     27-jan-94   Added LANCE TX hardware buffer chaining.
      0.3      1-feb-94   Added multiple DEPCA support.
      0.31     4-feb-94   Added DE202 recognition.
      0.32    19-feb-94   Tidy up. Improve multi-DEPCA support.
      0.33    25-feb-94   Fix DEPCA ethernet ROM counter enable.
                          Add jabber packet fix from murf@perftech.com
			  and becker@super.org
      0.34     7-mar-94   Fix DEPCA max network memory RAM & NICSR access.
      0.35     8-mar-94   Added DE201 recognition. Tidied up.
      0.351   30-apr-94   Added EISA support. Added DE422 recognition.
      0.36    16-may-94   DE422 fix released.
      0.37    22-jul-94   Added MODULE support
      0.38    15-aug-94   Added DBR ROM switch in depca_close(). 
                          Multi DEPCA bug fix.
      0.38axp 15-sep-94   Special version for Alpha AXP Linux V1.0.
      0.381   12-dec-94   Added DE101 recognition, fix multicast bug.
      0.382    9-feb-95   Fix recognition bug reported by <bkm@star.rl.ac.uk>.
      0.383   22-feb-95   Fix for conflict with VESA SCSI reported by
                          <stromain@alf.dec.com>
      0.384   17-mar-95   Fix a ring full bug reported by <bkm@star.rl.ac.uk>
      0.385    3-apr-95   Fix a recognition bug reported by 
                                                <ryan.niemi@lastfrontier.com>
      0.386   21-apr-95   Fix the last fix...sorry, must be galloping senility
      0.40    25-May-95   Rewrite for portability & updated.
                          ALPHA support from <jestabro@amt.tay1.dec.com>
      0.41    26-Jun-95   Added verify_area() calls in depca_ioctl() from
                          suggestion by <heiko@colossus.escape.de>
      0.42    27-Dec-95   Add 'mem' shared memory assigment for loadable 
                          modules.
                          Add 'adapter_name' for loadable modules when no PROM.
			  Both above from a suggestion by 
			  <pchen@woodruffs121.residence.gatech.edu>.
			  Add new multicasting code.

    =========================================================================
*/

static const char *version = "depca.c:v0.42 95/12/27 davies@wanton.lkg.dec.com\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <linux/time.h>
#include <linux/types.h>
#include <linux/unistd.h>

#include "depca.h"

#ifdef DEPCA_DEBUG
static int depca_debug = DEPCA_DEBUG;
#else
static int depca_debug = 1;
#endif

#define DEPCA_NDA 0xffe0            /* No Device Address */

/*
** Ethernet PROM defines
*/
#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

/*
** Set the number of Tx and Rx buffers. Ensure that the memory requested
** here is <= to the amount of shared memory set up by the board switches.
** The number of descriptors MUST BE A POWER OF 2.
**
** total_memory = NUM_RX_DESC*(8+RX_BUFF_SZ) + NUM_TX_DESC*(8+TX_BUFF_SZ)
*/
#define NUM_RX_DESC     8               /* Number of RX descriptors */
#define NUM_TX_DESC     8               /* Number of TX descriptors */
#define RX_BUFF_SZ	1536            /* Buffer size for each Rx buffer */
#define TX_BUFF_SZ	1536            /* Buffer size for each Tx buffer */

#define CRC_POLYNOMIAL_BE 0x04c11db7UL  /* Ethernet CRC, big endian */
#define CRC_POLYNOMIAL_LE 0xedb88320UL  /* Ethernet CRC, little endian */

/*
** EISA bus defines
*/
#define DEPCA_EISA_IO_PORTS 0x0c00       /* I/O port base address, slot 0 */
#define MAX_EISA_SLOTS 16
#define EISA_SLOT_INC 0x1000

/*
** ISA Bus defines
*/
#define DEPCA_RAM_BASE_ADDRESSES {0xc0000,0xd0000,0xe0000,0x00000}
#define DEPCA_IO_PORTS {0x300, 0x200, 0}
#define DEPCA_TOTAL_SIZE 0x10
static short mem_chkd = 0;

/*
** Name <-> Adapter mapping
*/
#define DEPCA_SIGNATURE {"DEPCA",\
			 "DE100","DE101",\
                         "DE200","DE201","DE202",\
			 "DE210",\
                         "DE422",\
                         ""}
static enum {DEPCA, de100, de101, de200, de201, de202, de210, de422, unknown} adapter;

/*
** Miscellaneous info...
*/
#define DEPCA_STRLEN 16
#define MAX_NUM_DEPCAS 2

/*
** Memory Alignment. Each descriptor is 4 longwords long. To force a
** particular alignment on the TX descriptor, adjust DESC_SKIP_LEN and
** DESC_ALIGN. ALIGN aligns the start address of the private memory area
** and hence the RX descriptor ring's first entry. 
*/
#define ALIGN4      ((u_long)4 - 1)       /* 1 longword align */
#define ALIGN8      ((u_long)8 - 1)       /* 2 longword (quadword) align */
#define ALIGN         ALIGN8              /* Keep the LANCE happy... */

/*
** The DEPCA Rx and Tx ring descriptors. 
*/
struct depca_rx_desc {
    volatile s32 base;
    s16 buf_length;		/* This length is negative 2's complement! */
    s16 msg_length;		/* This length is "normal". */
};

struct depca_tx_desc {
    volatile s32 base;
    s16 length;		        /* This length is negative 2's complement! */
    s16 misc;                   /* Errors and TDR info */
};

#define LA_MASK 0x0000ffff      /* LANCE address mask for mapping network RAM
				   to LANCE memory address space */

/*
** The Lance initialization block, described in databook, in common memory.
*/
struct depca_init {
    u16 mode;	                /* Mode register */
    u8  phys_addr[ETH_ALEN];	/* Physical ethernet address */
    u8  mcast_table[8];	        /* Multicast Hash Table. */
    u32 rx_ring;     	        /* Rx ring base pointer & ring length */
    u32 tx_ring;	        /* Tx ring base pointer & ring length */
};

#define DEPCA_PKT_STAT_SZ 16
#define DEPCA_PKT_BIN_SZ  128                /* Should be >=100 unless you
                                                increase DEPCA_PKT_STAT_SZ */
struct depca_private {
    char devname[DEPCA_STRLEN];    /* Device Product String                  */
    char adapter_name[DEPCA_STRLEN];/* /proc/ioports string                  */
    char adapter;                  /* Adapter type                           */
    struct depca_rx_desc *rx_ring; /* Pointer to start of RX descriptor ring */
    struct depca_tx_desc *tx_ring; /* Pointer to start of TX descriptor ring */
    struct depca_init	init_block;/* Shadow Initialization block            */
    char *rx_memcpy[NUM_RX_DESC];  /* CPU virt address of sh'd memory buffs  */
    char *tx_memcpy[NUM_TX_DESC];  /* CPU virt address of sh'd memory buffs  */
    u_long bus_offset;             /* (E)ISA bus address offset vs LANCE     */
    u_long sh_mem;  		   /* Physical start addr of shared mem area */
    u_long dma_buffs;		   /* LANCE Rx and Tx buffers start address. */
    int	rx_new, tx_new;		   /* The next free ring entry               */
    int rx_old, tx_old;	           /* The ring entries to be free()ed.       */
    struct enet_statistics stats;
    struct {                       /* Private stats counters                 */
	u32 bins[DEPCA_PKT_STAT_SZ];
	u32 unicast;
	u32 multicast;
	u32 broadcast;
	u32 excessive_collisions;
	u32 tx_underruns;
	u32 excessive_underruns;
    } pktStats;
    int txRingMask;                /* TX ring mask                           */
    int rxRingMask;                /* RX ring mask                           */
    s32 rx_rlen;                   /* log2(rxRingMask+1) for the descriptors */
    s32 tx_rlen;                   /* log2(txRingMask+1) for the descriptors */
};

/*
** The transmit ring full condition is described by the tx_old and tx_new
** pointers by:
**    tx_old            = tx_new    Empty ring
**    tx_old            = tx_new+1  Full ring
**    tx_old+txRingMask = tx_new    Full ring  (wrapped condition)
*/
#define TX_BUFFS_AVAIL ((lp->tx_old<=lp->tx_new)?\
			 lp->tx_old+lp->txRingMask-lp->tx_new:\
                         lp->tx_old               -lp->tx_new-1)

/*
** Public Functions
*/
static int    depca_open(struct device *dev);
static int    depca_start_xmit(struct sk_buff *skb, struct device *dev);
static void   depca_interrupt(int irq, struct pt_regs * regs);
static int    depca_close(struct device *dev);
static int    depca_ioctl(struct device *dev, struct ifreq *rq, int cmd);
static struct enet_statistics *depca_get_stats(struct device *dev);
static void   set_multicast_list(struct device *dev);

/*
** Private functions
*/
static int    depca_hw_init(struct device *dev, u_long ioaddr);
static void   depca_init_ring(struct device *dev);
static int    depca_rx(struct device *dev);
static int    depca_tx(struct device *dev);

static void   LoadCSRs(struct device *dev);
static int    InitRestartDepca(struct device *dev);
static void   DepcaSignature(char *name, u_long paddr);
static int    DevicePresent(u_long ioaddr);
static int    get_hw_addr(struct device *dev);
static int    EISA_signature(char *name, s32 eisa_id);
static void   SetMulticastFilter(struct device *dev);
static void   isa_probe(struct device *dev, u_long iobase);
static void   eisa_probe(struct device *dev, u_long iobase);
static struct device *alloc_device(struct device *dev, u_long iobase);
static int    load_packet(struct device *dev, struct sk_buff *skb);
static void   depca_dbg_open(struct device *dev);

#ifdef MODULE
int           init_module(void);
void          cleanup_module(void);
static int    autoprobed = 1, loading_module = 1;
# else
static u_char de1xx_irq[] = {2,3,4,5,7,0};
static u_char de2xx_irq[] = {5,9,10,11,15,0};
static u_char de422_irq[] = {5,9,10,11,0};
static u_char *depca_irq;
static int    autoprobed = 0, loading_module = 0;
#endif /* MODULE */

static char   name[DEPCA_STRLEN];
static int    num_depcas = 0, num_eth = 0;
static int    mem=0;                       /* For loadable module assignment
                                              use insmod mem=0x????? .... */
static char   *adapter_name = '\0';        /* If no PROM when loadable module
					      use insmod adapter_name=DE??? ...
					   */
/*
** Miscellaneous defines...
*/
#define STOP_DEPCA \
    outw(CSR0, DEPCA_ADDR);\
    outw(STOP, DEPCA_DATA)



int depca_probe(struct device *dev)
{
  int tmp = num_depcas, status = -ENODEV;
  u_long iobase = dev->base_addr;

  if ((iobase == 0) && loading_module){
    printk("Autoprobing is not supported when loading a module based driver.\n");
    status = -EIO;
  } else {
    isa_probe(dev, iobase);
    eisa_probe(dev, iobase);

    if ((tmp == num_depcas) && (iobase != 0) && loading_module) {
      printk("%s: depca_probe() cannot find device at 0x%04lx.\n", dev->name, 
	                                                               iobase);
    }

    /*
    ** Walk the device list to check that at least one device
    ** initialised OK
    */
    for (; (dev->priv == NULL) && (dev->next != NULL); dev = dev->next);

    if (dev->priv) status = 0;
    if (iobase == 0) autoprobed = 1;
  }

  return status;
}

static int
depca_hw_init(struct device *dev, u_long ioaddr)
{
  struct depca_private *lp;
  int i, j, offset, netRAM, mem_len, status=0;
  s16 nicsr;
  u_long mem_start=0, mem_base[] = DEPCA_RAM_BASE_ADDRESSES;

  STOP_DEPCA;

  nicsr = inb(DEPCA_NICSR);
  nicsr = ((nicsr & ~SHE & ~RBE & ~IEN) | IM);
  outb(nicsr, DEPCA_NICSR);

  if (inw(DEPCA_DATA) == STOP) {
    if (mem == 0) {
      for (; mem_base[mem_chkd]; mem_chkd++) {
	mem_start = mem_base[mem_chkd];
	DepcaSignature(name, mem_start);
	if (*name != '\0') break;
      }
    } else {
      mem_start = mem;
      if (adapter_name) {
	strcpy(name, adapter_name);
      } else{
	DepcaSignature(name, mem_start);
      }
    }

    if ((*name != '\0') && mem_start) {           /* found a DEPCA device */
      dev->base_addr = ioaddr;

      if ((ioaddr&0x0fff)==DEPCA_EISA_IO_PORTS) {/* EISA slot address */
	printk("%s: %s at 0x%04lx (EISA slot %d)", 
	                    dev->name, name, ioaddr, (int)((ioaddr>>12)&0x0f));
      } else {                             /* ISA port address */
	printk("%s: %s at 0x%04lx", dev->name, name, ioaddr);
      }

      printk(", h/w address ");
      status = get_hw_addr(dev);
      for (i=0; i<ETH_ALEN - 1; i++) { /* get the ethernet address */
	printk("%2.2x:", dev->dev_addr[i]);
      }
      printk("%2.2x", dev->dev_addr[i]);

      if (status == 0) {
	/* Set up the maximum amount of network RAM(kB) */
	netRAM = ((adapter != DEPCA) ? 64 : 48);
	if ((nicsr & _128KB) && (adapter == de422)) netRAM = 128;
	offset = 0x0000;

	/* Shared Memory Base Address */ 
	if (nicsr & BUF) {
	  offset = 0x8000;              /* 32kbyte RAM offset*/
	  nicsr &= ~BS;                 /* DEPCA RAM in top 32k */
	  netRAM -= 32;
	}
	mem_start += offset;            /* (E)ISA start address */
	if ((mem_len = (NUM_RX_DESC*(sizeof(struct depca_rx_desc)+RX_BUFF_SZ) +
			NUM_TX_DESC*(sizeof(struct depca_tx_desc)+TX_BUFF_SZ) +
			sizeof(struct depca_init))) <=
	    (netRAM<<10)) {
	  printk(",\n      has %dkB RAM at 0x%.5lx", netRAM, mem_start);

	  /* Enable the shadow RAM. */
	  if (adapter != DEPCA) {
	    nicsr |= SHE;
	    outb(nicsr, DEPCA_NICSR);
	  }
 
	  /* Define the device private memory */
	  dev->priv = (void *) kmalloc(sizeof(struct depca_private), GFP_KERNEL);
	  if (dev->priv == NULL)
	    return -ENOMEM;
	  lp = (struct depca_private *)dev->priv;
	  memset((char *)dev->priv, 0, sizeof(struct depca_private));
	  lp->adapter = adapter;
	  sprintf(lp->adapter_name,"%s (%s)", name, dev->name);
	  request_region(ioaddr, DEPCA_TOTAL_SIZE, lp->adapter_name);

	  /* Initialisation Block */
	  lp->sh_mem = mem_start;
	  mem_start += sizeof(struct depca_init);

	  /* Tx & Rx descriptors (aligned to a quadword boundary) */
	  mem_start = (mem_start + ALIGN) & ~ALIGN;
	  lp->rx_ring = (struct depca_rx_desc *)mem_start;

	  mem_start += (sizeof(struct depca_rx_desc) * NUM_RX_DESC);
	  lp->tx_ring = (struct depca_tx_desc *)mem_start;

	  mem_start += (sizeof(struct depca_tx_desc) * NUM_TX_DESC);
	  lp->bus_offset = mem_start & 0x00ff0000;
	  mem_start &= LA_MASK;           /* LANCE re-mapped start address */

	  lp->dma_buffs = mem_start;

	  /* Finish initialising the ring information. */
	  lp->rxRingMask = NUM_RX_DESC - 1;
	  lp->txRingMask = NUM_TX_DESC - 1;

	  /* Calculate Tx/Rx RLEN size for the descriptors. */
	  for (i=0, j = lp->rxRingMask; j>0; i++) {
	    j >>= 1;
	  }
	  lp->rx_rlen = (s32)(i << 29);
	  for (i=0, j = lp->txRingMask; j>0; i++) {
	    j >>= 1;
	  }
	  lp->tx_rlen = (s32)(i << 29);

	  /* Load the initialisation block */
	  depca_init_ring(dev);

	  /* Initialise the control and status registers */
	  LoadCSRs(dev);

	  /* Enable DEPCA board interrupts for autoprobing */
	  nicsr = ((nicsr & ~IM)|IEN);
	  outb(nicsr, DEPCA_NICSR);

	  /* To auto-IRQ we enable the initialization-done and DMA err,
	     interrupts. For now we will always get a DMA error. */
	  if (dev->irq < 2) {
#ifndef MODULE
	    unsigned char irqnum;
	    autoirq_setup(0);
	    
	    /* Assign the correct irq list */
	    switch (lp->adapter) {
	    case DEPCA:
	    case de100:
	    case de101:
	      depca_irq = de1xx_irq;
	      break;
	    case de200:
	    case de201:
	    case de202:
	    case de210:
	      depca_irq = de2xx_irq;
	      break;
	    case de422:
	      depca_irq = de422_irq;
	      break;
	    }

	    /* Trigger an initialization just for the interrupt. */
	    outw(INEA | INIT, DEPCA_DATA);
	  
	    irqnum = autoirq_report(1);
	    if (!irqnum) {
	      printk(" and failed to detect IRQ line.\n");
	      status = -ENXIO;
	    } else {
	      for (dev->irq=0,i=0; (depca_irq[i]) && (!dev->irq); i++) {
		if (irqnum == depca_irq[i]) {
		  dev->irq = irqnum;
		  printk(" and uses IRQ%d.\n", dev->irq);
		}
	      }
	      
	      if (!dev->irq) {
		printk(" but incorrect IRQ line detected.\n");
		status = -ENXIO;
	      }
	    }
#endif /* MODULE */
	  } else {
	    printk(" and assigned IRQ%d.\n", dev->irq);
	  }
	  if (status) release_region(ioaddr, DEPCA_TOTAL_SIZE);
	} else {
	  printk(",\n      requests %dkB RAM: only %dkB is available!\n", 
	         	                                (mem_len>>10), netRAM);
	  status = -ENXIO;
	}
      } else {
	printk("      which has an Ethernet PROM CRC error.\n");
	status = -ENXIO;
      }
    }
    if (!status) {
      if (depca_debug > 0) {
	printk(version);
      }

      /* The DEPCA-specific entries in the device structure. */
      dev->open = &depca_open;
      dev->hard_start_xmit = &depca_start_xmit;
      dev->stop = &depca_close;
      dev->get_stats = &depca_get_stats;
      dev->set_multicast_list = &set_multicast_list;
      dev->do_ioctl = &depca_ioctl;

      dev->mem_start = 0;
	
      /* Fill in the generic field of the device structure. */
      ether_setup(dev);
    } else {                           /* Incorrectly initialised hardware */
      if (dev->priv) {
	kfree_s(dev->priv, sizeof(struct depca_private));
	dev->priv = NULL;
      }
    }
  } else {
    status = -ENXIO;
  }

  return status;
}


static int
depca_open(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;
  s16 nicsr;
  int status = 0;

  irq2dev_map[dev->irq] = dev;
  STOP_DEPCA;
  nicsr = inb(DEPCA_NICSR);

  /* Make sure the shadow RAM is enabled */
  if (adapter != DEPCA) {
    nicsr |= SHE;
    outb(nicsr, DEPCA_NICSR);
  }

  /* Re-initialize the DEPCA... */
  depca_init_ring(dev);
  LoadCSRs(dev);

  depca_dbg_open(dev);

  if (request_irq(dev->irq, &depca_interrupt, 0, lp->adapter_name)) {
    printk("depca_open(): Requested IRQ%d is busy\n",dev->irq);
    status = -EAGAIN;
  } else {

    /* Enable DEPCA board interrupts and turn off LED */
    nicsr = ((nicsr & ~IM & ~LED)|IEN);
    outb(nicsr, DEPCA_NICSR);
    outw(CSR0,DEPCA_ADDR);
    
    dev->tbusy = 0;                         
    dev->interrupt = 0;
    dev->start = 1;
    
    status = InitRestartDepca(dev);

    if (depca_debug > 1){
      printk("CSR0: 0x%4.4x\n",inw(DEPCA_DATA));
      printk("nicsr: 0x%02x\n",inb(DEPCA_NICSR));
    }
  }

  MOD_INC_USE_COUNT;
  
  return status;
}

/* Initialize the lance Rx and Tx descriptor rings. */
static void
depca_init_ring(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_int i;
  u_long p;

  /* Lock out other processes whilst setting up the hardware */
  set_bit(0, (void *)&dev->tbusy);

  lp->rx_new = lp->tx_new = 0;
  lp->rx_old = lp->tx_old = 0;

  /* Initialize the base addresses and length of each buffer in the ring */
  for (i = 0; i <= lp->rxRingMask; i++) {
    writel((p=lp->dma_buffs+i*RX_BUFF_SZ) | R_OWN, &lp->rx_ring[i].base);
    writew(-RX_BUFF_SZ, &lp->rx_ring[i].buf_length);
    lp->rx_memcpy[i]=(char *)(p+lp->bus_offset);
  }
  for (i = 0; i <= lp->txRingMask; i++) {
    writel((p=lp->dma_buffs+(i+lp->txRingMask+1)*TX_BUFF_SZ) & 0x00ffffff,
	                                                 &lp->tx_ring[i].base);
    lp->tx_memcpy[i]=(char *)(p+lp->bus_offset);
  }

  /* Set up the initialization block */
  lp->init_block.rx_ring = ((u32)((u_long)lp->rx_ring)&LA_MASK) | lp->rx_rlen;
  lp->init_block.tx_ring = ((u32)((u_long)lp->tx_ring)&LA_MASK) | lp->tx_rlen;

  SetMulticastFilter(dev);

  for (i = 0; i < ETH_ALEN; i++) {
    lp->init_block.phys_addr[i] = dev->dev_addr[i];
  }

  lp->init_block.mode = 0x0000;            /* Enable the Tx and Rx */

  return;
}

/* 
** Writes a socket buffer to TX descriptor ring and starts transmission 
*/
static int
depca_start_xmit(struct sk_buff *skb, struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;
  int status = 0;

  /* Transmitter timeout, serious problems. */
  if (dev->tbusy) {
    int tickssofar = jiffies - dev->trans_start;
    if (tickssofar < 1*HZ) {
      status = -1;
    } else {
      printk("%s: transmit timed out, status %04x, resetting.\n",
	     dev->name, inw(DEPCA_DATA));
	
      STOP_DEPCA;
      depca_init_ring(dev);
      LoadCSRs(dev);
      dev->interrupt = UNMASK_INTERRUPTS;
      dev->start = 1;
      dev->tbusy=0;
      dev->trans_start = jiffies;
      InitRestartDepca(dev);
    }
    return status;
  } else if (skb == NULL) {
    dev_tint(dev);
  } else if (skb->len > 0) {
    /* Enforce 1 process per h/w access */
    if (set_bit(0, (void*)&dev->tbusy) != 0) {
      printk("%s: Transmitter access conflict.\n", dev->name);
      status = -1;
    } else {
      if (TX_BUFFS_AVAIL) {                    /* Fill in a Tx ring entry */
	status = load_packet(dev, skb);

	if (!status) {
	  /* Trigger an immediate send demand. */
	  outw(CSR0, DEPCA_ADDR);
	  outw(INEA | TDMD, DEPCA_DATA);
	  
	  dev->trans_start = jiffies;
	  dev_kfree_skb(skb, FREE_WRITE);
	}
	if (TX_BUFFS_AVAIL) {
	  dev->tbusy=0;
	}  
      } else {
	status = -1;
      }
    }
  }
  
  return status;
}

/*
** The DEPCA interrupt handler. 
*/
static void
depca_interrupt(int irq, struct pt_regs * regs)
{
  struct device *dev = (struct device *)(irq2dev_map[irq]);
  struct depca_private *lp;
  s16 csr0, nicsr;
  u_long ioaddr;

  if (dev == NULL) {
    printk ("depca_interrupt(): irq %d for unknown device.\n", irq);
  } else {
    lp = (struct depca_private *)dev->priv;
    ioaddr = dev->base_addr;
    
    if (dev->interrupt)
      printk("%s: Re-entering the interrupt handler.\n", dev->name);

    dev->interrupt = MASK_INTERRUPTS;

    /* mask the DEPCA board interrupts and turn on the LED */
    nicsr = inb(DEPCA_NICSR);
    nicsr |= (IM|LED);
    outb(nicsr, DEPCA_NICSR);

    outw(CSR0, DEPCA_ADDR);
    csr0 = inw(DEPCA_DATA);

    /* Acknowledge all of the current interrupt sources ASAP. */
    outw(csr0 & INTE, DEPCA_DATA);

    if (csr0 & RINT)		       /* Rx interrupt (packet arrived) */
      depca_rx(dev);

    if (csr0 & TINT) 	               /* Tx interrupt (packet sent) */
      depca_tx(dev);

    if ((TX_BUFFS_AVAIL >= 0) && dev->tbusy) { /* any resources available? */
      dev->tbusy = 0;                  /* clear TX busy flag */
      mark_bh(NET_BH);
    }

    /* Unmask the DEPCA board interrupts and turn off the LED */
    nicsr = (nicsr & ~IM & ~LED);
    outb(nicsr, DEPCA_NICSR);

    dev->interrupt = UNMASK_INTERRUPTS;
  }

  return;
}

static int
depca_rx(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  int i, entry;
  s32 status;

  for (entry=lp->rx_new; 
       !(readl(&lp->rx_ring[entry].base) & R_OWN);
       entry=lp->rx_new){
    status = readl(&lp->rx_ring[entry].base) >> 16 ;
    if (status & R_STP) {                      /* Remember start of frame */
      lp->rx_old = entry;
    }
    if (status & R_ENP) {                      /* Valid frame status */
      if (status & R_ERR) {	               /* There was an error. */
	lp->stats.rx_errors++;                 /* Update the error stats. */
	if (status & R_FRAM) lp->stats.rx_frame_errors++;
	if (status & R_OFLO) lp->stats.rx_over_errors++;
	if (status & R_CRC)  lp->stats.rx_crc_errors++;
	if (status & R_BUFF) lp->stats.rx_fifo_errors++;
      } else {	
	short len, pkt_len = readw(&lp->rx_ring[entry].msg_length);
	struct sk_buff *skb;

	skb = dev_alloc_skb(pkt_len+2);
	if (skb != NULL) {
	  unsigned char *buf;
	  skb_reserve(skb,2);               /* 16 byte align the IP header */
	  buf = skb_put(skb,pkt_len);
	  skb->dev = dev;
	  if (entry < lp->rx_old) {         /* Wrapped buffer */
	    len = (lp->rxRingMask - lp->rx_old + 1) * RX_BUFF_SZ;
	    memcpy_fromio(buf, lp->rx_memcpy[lp->rx_old], len);
	    memcpy_fromio(buf + len, lp->rx_memcpy[0], pkt_len-len);
	  } else {                          /* Linear buffer */
	    memcpy_fromio(buf, lp->rx_memcpy[lp->rx_old], pkt_len);
	  }

	  /* 
	  ** Notify the upper protocol layers that there is another 
	  ** packet to handle
	  */
	  skb->protocol=eth_type_trans(skb,dev);
	  netif_rx(skb);
 
	  /*
	  ** Update stats
	  */
	  lp->stats.rx_packets++;
	  for (i=1; i<DEPCA_PKT_STAT_SZ-1; i++) {
	    if (pkt_len < (i*DEPCA_PKT_BIN_SZ)) {
	      lp->pktStats.bins[i]++;
	      i = DEPCA_PKT_STAT_SZ;
	    }
	  }
	  if (buf[0] & 0x01) {              /* Multicast/Broadcast */
	    if ((*(s16 *)&buf[0] == -1) &&
		(*(s16 *)&buf[2] == -1) &&
		(*(s16 *)&buf[4] == -1)) {
	      lp->pktStats.broadcast++;
	    } else {
	      lp->pktStats.multicast++;
	    }
	  } else if ((*(s16 *)&buf[0] == *(s16 *)&dev->dev_addr[0]) &&
		     (*(s16 *)&buf[2] == *(s16 *)&dev->dev_addr[2]) &&
		     (*(s16 *)&buf[4] == *(s16 *)&dev->dev_addr[4])) {
	    lp->pktStats.unicast++;
	  }
	  
	  lp->pktStats.bins[0]++;           /* Duplicates stats.rx_packets */
	  if (lp->pktStats.bins[0] == 0) {  /* Reset counters */
	    memset((char *)&lp->pktStats, 0, sizeof(lp->pktStats));
	  }
	} else {
	  printk("%s: Memory squeeze, deferring packet.\n", dev->name);
	  lp->stats.rx_dropped++;	/* Really, deferred. */
	  break;
	}
      }
      /* Change buffer ownership for this last frame, back to the adapter */
      for (; lp->rx_old!=entry; lp->rx_old=(++lp->rx_old)&lp->rxRingMask) {
	writel(readl(&lp->rx_ring[lp->rx_old].base) | R_OWN, 
	                                        &lp->rx_ring[lp->rx_old].base);
      }
      writel(readl(&lp->rx_ring[entry].base) | R_OWN, &lp->rx_ring[entry].base);
    }

    /*
    ** Update entry information
    */
    lp->rx_new = (++lp->rx_new) & lp->rxRingMask;
    }

    return 0;
}

/*
** Buffer sent - check for buffer errors.
*/
static int
depca_tx(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  int entry;
  s32 status;
  u_long ioaddr = dev->base_addr;

  for (entry = lp->tx_old; entry != lp->tx_new; entry = lp->tx_old) {
    status = readl(&lp->tx_ring[entry].base) >> 16 ;

    if (status < 0) {                          /* Packet not yet sent! */
      break;
    } else if (status & T_ERR) {               /* An error occured. */
      status = readl(&lp->tx_ring[entry].misc);
      lp->stats.tx_errors++;
      if (status & TMD3_RTRY) lp->stats.tx_aborted_errors++;
      if (status & TMD3_LCAR) lp->stats.tx_carrier_errors++;
      if (status & TMD3_LCOL) lp->stats.tx_window_errors++;
      if (status & TMD3_UFLO) lp->stats.tx_fifo_errors++;
      if (status & (TMD3_BUFF | TMD3_UFLO)) {
	/* Trigger an immediate send demand. */
	outw(CSR0, DEPCA_ADDR);
	outw(INEA | TDMD, DEPCA_DATA);
      }
    } else if (status & (T_MORE | T_ONE)) {
      lp->stats.collisions++;
    } else {
      lp->stats.tx_packets++;
    }

    /* Update all the pointers */
    lp->tx_old = (++lp->tx_old) & lp->txRingMask;
  }

  return 0;
}

static int
depca_close(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  s16 nicsr;
  u_long ioaddr = dev->base_addr;

  dev->start = 0;
  dev->tbusy = 1;

  outw(CSR0, DEPCA_ADDR);

  if (depca_debug > 1) {
    printk("%s: Shutting down ethercard, status was %2.2x.\n",
	   dev->name, inw(DEPCA_DATA));
  }

  /* 
  ** We stop the DEPCA here -- it occasionally polls
  ** memory if we don't. 
  */
  outw(STOP, DEPCA_DATA);

  /*
  ** Give back the ROM in case the user wants to go to DOS
  */
  if (lp->adapter != DEPCA) {
    nicsr = inb(DEPCA_NICSR);
    nicsr &= ~SHE;
    outb(nicsr, DEPCA_NICSR);
  }

  /*
  ** Free the associated irq
  */
  free_irq(dev->irq);
  irq2dev_map[dev->irq] = NULL;

  MOD_DEC_USE_COUNT;

  return 0;
}

static void LoadCSRs(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;

  outw(CSR1, DEPCA_ADDR);                /* initialisation block address LSW */
  outw((u16)(lp->sh_mem & LA_MASK), DEPCA_DATA);
  outw(CSR2, DEPCA_ADDR);                /* initialisation block address MSW */
  outw((u16)((lp->sh_mem & LA_MASK) >> 16), DEPCA_DATA);
  outw(CSR3, DEPCA_ADDR);                /* ALE control */
  outw(ACON, DEPCA_DATA);

  outw(CSR0, DEPCA_ADDR);                /* Point back to CSR0 */

  return;
}

static int InitRestartDepca(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;
  int i, status=0;

  /* Copy the shadow init_block to shared memory */
  memcpy_toio((char *)lp->sh_mem, &lp->init_block, sizeof(struct depca_init));

  outw(CSR0, DEPCA_ADDR);                /* point back to CSR0 */
  outw(INIT, DEPCA_DATA);                /* initialize DEPCA */

  /* wait for lance to complete initialisation */
  for (i=0;(i<100) && !(inw(DEPCA_DATA) & IDON); i++); 

  if (i!=100) {
    /* clear IDON by writing a "1", enable interrupts and start lance */
    outw(IDON | INEA | STRT, DEPCA_DATA);
    if (depca_debug > 2) {
      printk("%s: DEPCA open after %d ticks, init block 0x%08lx csr0 %4.4x.\n",
	     dev->name, i, lp->sh_mem, inw(DEPCA_DATA));
    }
  } else {
    printk("%s: DEPCA unopen after %d ticks, init block 0x%08lx csr0 %4.4x.\n",
	     dev->name, i, lp->sh_mem, inw(DEPCA_DATA));
    status = -1;
  }

  return status;
}

static struct enet_statistics *
depca_get_stats(struct device *dev)
{
    struct depca_private *lp = (struct depca_private *)dev->priv;

    /* Null body since there is no framing error counter */

    return &lp->stats;
}

/*
** Set or clear the multicast filter for this adaptor.
*/
static void
set_multicast_list(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;
  
  if (irq2dev_map[dev->irq] != NULL) {
    while(dev->tbusy);                /* Stop ring access */
    set_bit(0, (void*)&dev->tbusy);
    while(lp->tx_old != lp->tx_new);  /* Wait for the ring to empty */

    STOP_DEPCA;                       /* Temporarily stop the depca.  */
    depca_init_ring(dev);             /* Initialize the descriptor rings */

    if (dev->flags & IFF_PROMISC) {   /* Set promiscuous mode */
      lp->init_block.mode |= PROM;
    } else {
      SetMulticastFilter(dev);
      lp->init_block.mode &= ~PROM;   /* Unset promiscuous mode */
    }

    LoadCSRs(dev);                    /* Reload CSR3 */
    InitRestartDepca(dev);            /* Resume normal operation. */
    dev->tbusy = 0;                   /* Unlock the TX ring */
  }
}

/*
** Calculate the hash code and update the logical address filter
** from a list of ethernet multicast addresses.
** Big endian crc one liner is mine, all mine, ha ha ha ha!
** LANCE calculates its hash codes big endian.
*/
static void SetMulticastFilter(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  struct dev_mc_list *dmi=dev->mc_list;
  char *addrs;
  int i, j, bit, byte;
  u16 hashcode;
  s32 crc, poly = CRC_POLYNOMIAL_BE;

  if (dev->flags & IFF_ALLMULTI) {         /* Set all multicast bits */
    for (i=0; i<(HASH_TABLE_LEN>>3); i++) {
      lp->init_block.mcast_table[i] = (char)0xff;
    }
  } else {
    for (i=0; i<(HASH_TABLE_LEN>>3); i++){ /* Clear the multicast table */
      lp->init_block.mcast_table[i]=0;
    }
                                           /* Add multicast addresses */
    for (i=0;i<dev->mc_count;i++) {        /* for each address in the list */
      addrs=dmi->dmi_addr;
      dmi=dmi->next;
      if ((*addrs & 0x01) == 1) {          /* multicast address? */ 
	crc = 0xffffffff;                  /* init CRC for each address */
	for (byte=0;byte<ETH_ALEN;byte++) {/* for each address byte */
	                                   /* process each address bit */ 
	  for (bit = *addrs++,j=0;j<8;j++, bit>>=1) {
	    crc = (crc << 1) ^ ((((crc<0?1:0) ^ bit) & 0x01) ? poly : 0);
	  }
	}
	hashcode = (crc & 1);              /* hashcode is 6 LSb of CRC ... */
	for (j=0;j<5;j++) {                /* ... in reverse order. */
	  hashcode = (hashcode << 1) | ((crc>>=1) & 1);
	}                                      
	
	
	byte = hashcode >> 3;              /* bit[3-5] -> byte in filter */
	bit = 1 << (hashcode & 0x07);      /* bit[0-2] -> bit in byte */
	lp->init_block.mcast_table[byte] |= bit;
      }
    }
  }

  return;
}

/*
** ISA bus I/O device probe
*/
static void isa_probe(struct device *dev, u_long ioaddr)
{
  int i = num_depcas, maxSlots;
  s32 ports[] = DEPCA_IO_PORTS;

  if (!ioaddr && autoprobed) return ;          /* Been here before ! */
  if (ioaddr > 0x400) return;                  /* EISA Address */
  if (i >= MAX_NUM_DEPCAS) return;             /* Too many ISA adapters */

  if (ioaddr == 0) {                           /* Autoprobing */
    maxSlots = MAX_NUM_DEPCAS;
  } else {                                     /* Probe a specific location */
    ports[i] = ioaddr;
    maxSlots = i + 1;
  }

  for (; (i<maxSlots) && (dev!=NULL) && ports[i]; i++) {
    if (DevicePresent(ports[i]) == 0) { 
      if (check_region(ports[i], DEPCA_TOTAL_SIZE) == 0) {
	if ((dev = alloc_device(dev, ports[i])) != NULL) {
	  if (depca_hw_init(dev, ports[i]) == 0) {
	    num_depcas++;
	  }
	  num_eth++;
	}
      } else if (autoprobed) {
	printk("%s: region already allocated at 0x%04x.\n", dev->name,ports[i]);
      }
    }
  }

  return;
}

/*
** EISA bus I/O device probe. Probe from slot 1 since slot 0 is usually
** the motherboard. Upto 15 EISA devices are supported.
*/
static void eisa_probe(struct device *dev, u_long ioaddr)
{
  int i, maxSlots;
  u_long iobase;
  char name[DEPCA_STRLEN];

  if (!ioaddr && autoprobed) return ;            /* Been here before ! */
  if ((ioaddr < 0x400) && (ioaddr > 0)) return;  /* ISA Address */

  if (ioaddr == 0) {                           /* Autoprobing */
    iobase = EISA_SLOT_INC;                    /* Get the first slot address */
    i = 1;
    maxSlots = MAX_EISA_SLOTS;
  } else {                                     /* Probe a specific location */
    iobase = ioaddr;
    i = (ioaddr >> 12);
    maxSlots = i + 1;
  }
  if ((iobase & 0x0fff) == 0) iobase += DEPCA_EISA_IO_PORTS;

  for (; (i<maxSlots) && (dev!=NULL); i++, iobase+=EISA_SLOT_INC) {
    if (EISA_signature(name, EISA_ID)) {
      if (DevicePresent(iobase) == 0) { 
	if (check_region(iobase, DEPCA_TOTAL_SIZE) == 0) {
	  if ((dev = alloc_device(dev, iobase)) != NULL) {
	    if (depca_hw_init(dev, iobase) == 0) {
	      num_depcas++;
	    }
	    num_eth++;
	  }
	} else if (autoprobed) {
	  printk("%s: region already allocated at 0x%04lx.\n",dev->name,iobase);
	}
      }
    }
  }

  return;
}

/*
** Allocate the device by pointing to the next available space in the
** device structure. Should one not be available, it is created.
*/
static struct device *alloc_device(struct device *dev, u_long iobase)
{
  int addAutoProbe = 0;
  struct device *tmp = NULL, *ret;
  int (*init)(struct device *) = NULL;

  /*
  ** Check the device structures for an end of list or unused device
  */
  if (!loading_module) {
    while (dev->next != NULL) {
      if ((dev->base_addr == DEPCA_NDA) || (dev->base_addr == 0)) break;
      dev = dev->next;                     /* walk through eth device list */
      num_eth++;                           /* increment eth device number */
    }

    /*
    ** If an autoprobe is requested for another device, we must re-insert
    ** the request later in the list. Remember the current information.
    */
    if ((dev->base_addr == 0) && (num_depcas > 0)) {
      addAutoProbe++;
      tmp = dev->next;                     /* point to the next device */
      init = dev->init;                    /* remember the probe function */
    }

    /*
    ** If at end of list and can't use current entry, malloc one up. 
    ** If memory could not be allocated, print an error message.
    */
    if ((dev->next == NULL) &&  
	!((dev->base_addr == DEPCA_NDA) || (dev->base_addr == 0))){
      dev->next = (struct device *)kmalloc(sizeof(struct device) + 8,
					   GFP_KERNEL);

      dev = dev->next;                     /* point to the new device */
      if (dev == NULL) {
	printk("eth%d: Device not initialised, insufficient memory\n",
	       num_eth);
      } else {
	/*
	** If the memory was allocated, point to the new memory area
	** and initialize it (name, I/O address, next device (NULL) and
	** initialisation probe routine).
	*/
	dev->name = (char *)(dev + sizeof(struct device));
	if (num_eth > 9999) {
	  sprintf(dev->name,"eth????");    /* New device name */
	} else {
	  sprintf(dev->name,"eth%d", num_eth);/* New device name */
	}
	dev->base_addr = iobase;           /* assign the io address */
	dev->next = NULL;                  /* mark the end of list */
	dev->init = &depca_probe;          /* initialisation routine */
	num_depcas++;
      }
    }
    ret = dev;                             /* return current struct, or NULL */
  
    /*
    ** Now figure out what to do with the autoprobe that has to be inserted.
    ** Firstly, search the (possibly altered) list for an empty space.
    */
    if (ret != NULL) {
      if (addAutoProbe) {
	for (;(tmp->next!=NULL) && (tmp->base_addr!=DEPCA_NDA); tmp=tmp->next);

	/*
	** If no more device structures and can't use the current one, malloc
	** one up. If memory could not be allocated, print an error message.
	*/
	if ((tmp->next == NULL) && !(tmp->base_addr == DEPCA_NDA)) {
	  tmp->next = (struct device *)kmalloc(sizeof(struct device) + 8,
					       GFP_KERNEL);
	  tmp = tmp->next;                     /* point to the new device */
	  if (tmp == NULL) {
	    printk("%s: Insufficient memory to extend the device list.\n", 
		   dev->name);
	  } else {
	    /*
	    ** If the memory was allocated, point to the new memory area
	    ** and initialize it (name, I/O address, next device (NULL) and
	    ** initialisation probe routine).
	    */
	    tmp->name = (char *)(tmp + sizeof(struct device));
	    if (num_eth > 9999) {
	      sprintf(tmp->name,"eth????");       /* New device name */
	    } else {
	      sprintf(tmp->name,"eth%d", num_eth);/* New device name */
	    }
	    tmp->base_addr = 0;                /* re-insert the io address */
	    tmp->next = NULL;                  /* mark the end of list */
	    tmp->init = init;                  /* initialisation routine */
	  }
	} else {                               /* structure already exists */
	  tmp->base_addr = 0;                  /* re-insert the io address */
	}
      }
    }
  } else {
    ret = dev;
  }

  return ret;
}

/*
** Look for a particular board name in the on-board Remote Diagnostics
** and Boot (readb) ROM. This will also give us a clue to the network RAM
** base address.
*/
static void DepcaSignature(char *name, u_long paddr)
{
  u_int i,j,k;
  const char *signatures[] = DEPCA_SIGNATURE;
  char tmpstr[16];

  for (i=0;i<16;i++) {                  /* copy the first 16 bytes of ROM to */
    tmpstr[i] = readb(paddr+0xc000+i);  /* a temporary string */
  }

  strcpy(name,"");
  for (i=0;*signatures[i]!='\0' && *name=='\0';i++) {
    for (j=0,k=0;j<16 && k<strlen(signatures[i]);j++) {
      if (signatures[i][k] == tmpstr[j]) {              /* track signature */
	k++;
      } else {                     /* lost signature; begin search again */
	k=0;
      }
    }
    if (k == strlen(signatures[i])) {
      strcpy(name,signatures[i]);
    }
  }

  adapter = i - 1;

  return;
}

/*
** Look for a special sequence in the Ethernet station address PROM that
** is common across all DEPCA products. Note that the original DEPCA needs
** its ROM address counter to be initialized and enabled. Only enable
** if the first address octet is a 0x08 - this minimises the chances of
** messing around with some other hardware, but it assumes that this DEPCA
** card initialized itself correctly.
** 
** Search the Ethernet address ROM for the signature. Since the ROM address
** counter can start at an arbitrary point, the search must include the entire
** probe sequence length plus the (length_of_the_signature - 1).
** Stop the search IMMEDIATELY after the signature is found so that the
** PROM address counter is correctly positioned at the start of the
** ethernet address for later read out.
*/
static int DevicePresent(u_long ioaddr)
{
  union {
    struct {
      u32 a;
      u32 b;
    } llsig;
    char Sig[sizeof(u32) << 1];
  } dev;
  short sigLength=0;
  s8 data;
  s16 nicsr;
  int i, j, status = 0;

  data = inb(DEPCA_PROM);                /* clear counter on DEPCA */
  data = inb(DEPCA_PROM);                /* read data */

  if (data == 0x08) {                    /* Enable counter on DEPCA */
    nicsr = inb(DEPCA_NICSR);
    nicsr |= AAC;
    outb(nicsr, DEPCA_NICSR);
  }
  
  dev.llsig.a = ETH_PROM_SIG;
  dev.llsig.b = ETH_PROM_SIG;
  sigLength = sizeof(u32) << 1;

  for (i=0,j=0;j<sigLength && i<PROBE_LENGTH+sigLength-1;i++) {
    data = inb(DEPCA_PROM);
    if (dev.Sig[j] == data) {    /* track signature */
      j++;
    } else {                     /* lost signature; begin search again */
      if (data == dev.Sig[0]) {  /* rare case.... */
	j=1;
      } else {
	j=0;
      }
    }
  }

  if (j!=sigLength) {
    status = -ENODEV;           /* search failed */
  }

  return status;
}

/*
** The DE100 and DE101 PROM accesses were made non-standard for some bizarre
** reason: access the upper half of the PROM with x=0; access the lower half
** with x=1.
*/
static int get_hw_addr(struct device *dev)
{
  u_long ioaddr = dev->base_addr;
  int i, k, tmp, status = 0;
  u_short j, x, chksum;

  x = (((adapter == de100) || (adapter == de101)) ? 1 : 0);

  for (i=0,k=0,j=0;j<3;j++) {
    k <<= 1 ;
    if (k > 0xffff) k-=0xffff;

    k += (u_char) (tmp = inb(DEPCA_PROM + x));
    dev->dev_addr[i++] = (u_char) tmp;
    k += (u_short) ((tmp = inb(DEPCA_PROM + x)) << 8);
    dev->dev_addr[i++] = (u_char) tmp;

    if (k > 0xffff) k-=0xffff;
  }
  if (k == 0xffff) k=0;

  chksum = (u_char) inb(DEPCA_PROM + x);
  chksum |= (u_short) (inb(DEPCA_PROM + x) << 8);
  if (k != chksum) status = -1;

  return status;
}

/*
** Load a packet into the shared memory
*/
static int load_packet(struct device *dev, struct sk_buff *skb)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  int i, entry, end, len, status = 0;

  entry = lp->tx_new;  		               /* Ring around buffer number. */
  end = (entry + (skb->len - 1) / TX_BUFF_SZ) & lp->txRingMask;
  if (!(readl(&lp->tx_ring[end].base) & T_OWN)) {/* Enough room? */
    /* 
    ** Caution: the write order is important here... don't set up the
    ** ownership rights until all the other information is in place.
    */
    if (end < entry) {                         /* wrapped buffer */
      len = (lp->txRingMask - entry + 1) * TX_BUFF_SZ;
      memcpy_toio(lp->tx_memcpy[entry], skb->data, len);
      memcpy_toio(lp->tx_memcpy[0], skb->data + len, skb->len - len);
    } else {                                   /* linear buffer */
      memcpy_toio(lp->tx_memcpy[entry], skb->data, skb->len);
    }

    /* set up the buffer descriptors */
    len = (skb->len < ETH_ZLEN) ? ETH_ZLEN : skb->len;
    for (i = entry; i != end; i = (++i) & lp->txRingMask) {
                                               /* clean out flags */
      writel(readl(&lp->tx_ring[i].base) & ~T_FLAGS, &lp->tx_ring[i].base);
      writew(0x0000, &lp->tx_ring[i].misc);    /* clears other error flags */
      writew(-TX_BUFF_SZ, &lp->tx_ring[i].length);/* packet length in buffer */
      len -= TX_BUFF_SZ;
    }
                                               /* clean out flags */
    writel(readl(&lp->tx_ring[end].base) & ~T_FLAGS, &lp->tx_ring[end].base);
    writew(0x0000, &lp->tx_ring[end].misc);    /* clears other error flags */
    writew(-len, &lp->tx_ring[end].length);    /* packet length in last buff */

                                               /* start of packet */
    writel(readl(&lp->tx_ring[entry].base) | T_STP, &lp->tx_ring[entry].base);
                                               /* end of packet */
    writel(readl(&lp->tx_ring[end].base) | T_ENP, &lp->tx_ring[end].base);

    for (i=end; i!=entry; --i) {
                                               /* ownership of packet */
      writel(readl(&lp->tx_ring[i].base) | T_OWN, &lp->tx_ring[i].base);
      if (i == 0) i=lp->txRingMask+1;
    }   
    writel(readl(&lp->tx_ring[entry].base) | T_OWN, &lp->tx_ring[entry].base);
 
    lp->tx_new = (++end) & lp->txRingMask;     /* update current pointers */
  } else {
    status = -1;
  }

  return status;
}

/*
** Look for a particular board name in the EISA configuration space
*/
static int EISA_signature(char *name, s32 eisa_id)
{
  u_int i;
  const char *signatures[] = DEPCA_SIGNATURE;
  char ManCode[DEPCA_STRLEN];
  union {
    s32 ID;
    char Id[4];
  } Eisa;
  int status = 0;

  *name = '\0';
  Eisa.ID = inl(eisa_id);

  ManCode[0]=(((Eisa.Id[0]>>2)&0x1f)+0x40);
  ManCode[1]=(((Eisa.Id[1]&0xe0)>>5)+((Eisa.Id[0]&0x03)<<3)+0x40);
  ManCode[2]=(((Eisa.Id[2]>>4)&0x0f)+0x30);
  ManCode[3]=(( Eisa.Id[2]&0x0f)+0x30);
  ManCode[4]=(((Eisa.Id[3]>>4)&0x0f)+0x30);
  ManCode[5]='\0';

  for (i=0;(*signatures[i] != '\0') && (*name == '\0');i++) {
    if (strstr(ManCode, signatures[i]) != NULL) {
      strcpy(name,ManCode);
      status = 1;
    }
  }

  return status;
}

static void depca_dbg_open(struct device *dev)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  u_long ioaddr = dev->base_addr;
  struct depca_init *p = (struct depca_init *)lp->sh_mem;
  int i; 

  if (depca_debug > 1){
    /* Copy the shadow init_block to shared memory */
    memcpy_toio((char *)lp->sh_mem,&lp->init_block,sizeof(struct depca_init));

    printk("%s: depca open with irq %d\n",dev->name,dev->irq);
    printk("Descriptor head addresses:\n");
    printk("\t0x%lx  0x%lx\n",(u_long)lp->rx_ring, (u_long)lp->tx_ring);
    printk("Descriptor addresses:\nRX: ");
    for (i=0;i<lp->rxRingMask;i++){
      if (i < 3) {
	printk("0x%8.8lx ", (long) &lp->rx_ring[i].base);
      }
    }
    printk("...0x%8.8lx\n", (long) &lp->rx_ring[i].base);
    printk("TX: ");
    for (i=0;i<lp->txRingMask;i++){
      if (i < 3) {
	printk("0x%8.8lx ", (long) &lp->tx_ring[i].base);
      }
    }
    printk("...0x%8.8lx\n", (long) &lp->tx_ring[i].base);
    printk("\nDescriptor buffers:\nRX: ");
    for (i=0;i<lp->rxRingMask;i++){
      if (i < 3) {
	printk("0x%8.8x  ", (u32) readl(&lp->rx_ring[i].base));
      }
    }
    printk("...0x%8.8x\n", (u32) readl(&lp->rx_ring[i].base));
    printk("TX: ");
    for (i=0;i<lp->txRingMask;i++){
      if (i < 3) {
	printk("0x%8.8x  ", (u32) readl(&lp->tx_ring[i].base));
      }
    }
    printk("...0x%8.8x\n", (u32) readl(&lp->tx_ring[i].base));
    printk("Initialisation block at 0x%8.8lx\n",lp->sh_mem);
    printk("\tmode: 0x%4.4x\n", (u16) readw(&p->mode));
    printk("\tphysical address: ");
    for (i=0;i<ETH_ALEN-1;i++){
      printk("%2.2x:",(u_char)readb(&p->phys_addr[i]));
    }
    printk("%2.2x\n",(u_char)readb(&p->phys_addr[i]));
    printk("\tmulticast hash table: ");
    for (i=0;i<(HASH_TABLE_LEN >> 3)-1;i++){
      printk("%2.2x:",(u_char)readb(&p->mcast_table[i]));
    }
    printk("%2.2x\n",(u_char)readb(&p->mcast_table[i]));
    printk("\trx_ring at: 0x%8.8x\n", (u32) readl(&p->rx_ring));
    printk("\ttx_ring at: 0x%8.8x\n", (u32) readl(&p->tx_ring));
    printk("dma_buffs: 0x%8.8lx\n",lp->dma_buffs);
    printk("Ring size:\nRX: %d  Log2(rxRingMask): 0x%8.8x\n", 
	   (int)lp->rxRingMask + 1, 
	   lp->rx_rlen);
    printk("TX: %d  Log2(txRingMask): 0x%8.8x\n", 
	   (int)lp->txRingMask + 1, 
	   lp->tx_rlen);
    outw(CSR2,DEPCA_ADDR);
    printk("CSR2&1: 0x%4.4x",inw(DEPCA_DATA));
    outw(CSR1,DEPCA_ADDR);
    printk("%4.4x\n",inw(DEPCA_DATA));
    outw(CSR3,DEPCA_ADDR);
    printk("CSR3: 0x%4.4x\n",inw(DEPCA_DATA));
  }

  return;
}

/*
** Perform IOCTL call functions here. Some are privileged operations and the
** effective uid is checked in those cases.
** All MCA IOCTLs will not work here and are for testing purposes only.
*/
static int depca_ioctl(struct device *dev, struct ifreq *rq, int cmd)
{
  struct depca_private *lp = (struct depca_private *)dev->priv;
  struct depca_ioctl *ioc = (struct depca_ioctl *) &rq->ifr_data;
  int i, status = 0;
  u_long ioaddr = dev->base_addr;
  union {
    u8  addr[(HASH_TABLE_LEN * ETH_ALEN)];
    u16 sval[(HASH_TABLE_LEN * ETH_ALEN) >> 1];
    u32 lval[(HASH_TABLE_LEN * ETH_ALEN) >> 2];
  } tmp;

  switch(ioc->cmd) {
  case DEPCA_GET_HWADDR:             /* Get the hardware address */
    for (i=0; i<ETH_ALEN; i++) {
      tmp.addr[i] = dev->dev_addr[i];
    }
    ioc->len = ETH_ALEN;
    if (!(status = verify_area(VERIFY_WRITE, (void *)ioc->data, ioc->len))) {
      memcpy_tofs(ioc->data, tmp.addr, ioc->len);
    }

    break;
  case DEPCA_SET_HWADDR:             /* Set the hardware address */
    if (suser()) {
      if (!(status = verify_area(VERIFY_READ, (void *)ioc->data, ETH_ALEN))) {
	memcpy_fromfs(tmp.addr,ioc->data,ETH_ALEN);
	for (i=0; i<ETH_ALEN; i++) {
	  dev->dev_addr[i] = tmp.addr[i];
	}
	while(dev->tbusy);              /* Stop ring access */
	set_bit(0, (void*)&dev->tbusy);
	while(lp->tx_old != lp->tx_new);/* Wait for the ring to empty */

	STOP_DEPCA;                     /* Temporarily stop the depca.  */
	depca_init_ring(dev);           /* Initialize the descriptor rings */
	LoadCSRs(dev);                  /* Reload CSR3 */
	InitRestartDepca(dev);          /* Resume normal operation. */
	dev->tbusy = 0;                 /* Unlock the TX ring */
      }
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_SET_PROM:               /* Set Promiscuous Mode */
    if (suser()) {
      while(dev->tbusy);                /* Stop ring access */
      set_bit(0, (void*)&dev->tbusy);
      while(lp->tx_old != lp->tx_new);  /* Wait for the ring to empty */

      STOP_DEPCA;                       /* Temporarily stop the depca.  */
      depca_init_ring(dev);             /* Initialize the descriptor rings */
      lp->init_block.mode |= PROM;      /* Set promiscuous mode */

      LoadCSRs(dev);                    /* Reload CSR3 */
      InitRestartDepca(dev);            /* Resume normal operation. */
      dev->tbusy = 0;                   /* Unlock the TX ring */
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_CLR_PROM:               /* Clear Promiscuous Mode */
    if (suser()) {
      while(dev->tbusy);                /* Stop ring access */
      set_bit(0, (void*)&dev->tbusy);
      while(lp->tx_old != lp->tx_new);  /* Wait for the ring to empty */

      STOP_DEPCA;                       /* Temporarily stop the depca.  */
      depca_init_ring(dev);             /* Initialize the descriptor rings */
      lp->init_block.mode &= ~PROM;     /* Clear promiscuous mode */

      LoadCSRs(dev);                    /* Reload CSR3 */
      InitRestartDepca(dev);            /* Resume normal operation. */
      dev->tbusy = 0;                   /* Unlock the TX ring */
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_SAY_BOO:                /* Say "Boo!" to the kernel log file */
    printk("%s: Boo!\n", dev->name);

    break;
  case DEPCA_GET_MCA:                /* Get the multicast address table */
    ioc->len = (HASH_TABLE_LEN >> 3);
    if (!(status = verify_area(VERIFY_WRITE, ioc->data, ioc->len))) {
      memcpy_tofs(ioc->data, lp->init_block.mcast_table, ioc->len); 
    }

    break;
  case DEPCA_SET_MCA:                /* Set a multicast address */
    if (suser()) {
      if (!(status=verify_area(VERIFY_READ, ioc->data, ETH_ALEN*ioc->len))) {
	memcpy_fromfs(tmp.addr, ioc->data, ETH_ALEN * ioc->len);
	set_multicast_list(dev);
      }
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_CLR_MCA:                /* Clear all multicast addresses */
    if (suser()) {
      set_multicast_list(dev);
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_MCA_EN:                 /* Enable pass all multicast addressing */
    if (suser()) {
      set_multicast_list(dev);
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_GET_STATS:              /* Get the driver statistics */
    cli();
    ioc->len = sizeof(lp->pktStats);
    if (!(status=verify_area(VERIFY_WRITE, ioc->data, ioc->len))) {
      memcpy_tofs(ioc->data, &lp->pktStats, ioc->len); 
    }
    sti();

    break;
  case DEPCA_CLR_STATS:              /* Zero out the driver statistics */
    if (suser()) {
      cli();
      memset(&lp->pktStats, 0, sizeof(lp->pktStats));
      sti();
    } else {
      status = -EPERM;
    }

    break;
  case DEPCA_GET_REG:                /* Get the DEPCA Registers */
    i=0;
    tmp.sval[i++] = inw(DEPCA_NICSR);
    outw(CSR0, DEPCA_ADDR);              /* status register */
    tmp.sval[i++] = inw(DEPCA_DATA);
    memcpy(&tmp.sval[i], &lp->init_block, sizeof(struct depca_init));
    ioc->len = i+sizeof(struct depca_init);
    if (!(status=verify_area(VERIFY_WRITE, ioc->data, ioc->len))) {
      memcpy_tofs(ioc->data, tmp.addr, ioc->len);
    }

    break;
  default:
    status = -EOPNOTSUPP;
  }

  return status;
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device thisDepca = {
  devicename,  /* device name is inserted by /linux/drivers/net/net_init.c */
  0, 0, 0, 0,
  0x200, 7,    /* I/O address, IRQ */
  0, 0, 0, NULL, depca_probe };

static int irq=7;	/* EDIT THESE LINE FOR YOUR CONFIGURATION */
static int io=0x200;    /* Or use the irq= io= options to insmod */

/* See depca_probe() for autoprobe messages when a module */	
int
init_module(void)
{
  thisDepca.irq=irq;
  thisDepca.base_addr=io;

  if (register_netdev(&thisDepca) != 0)
    return -EIO;

  return 0;
}

void
cleanup_module(void)
{
  release_region(thisDepca.base_addr, DEPCA_TOTAL_SIZE);
  if (thisDepca.priv) {
    kfree(thisDepca.priv);
    thisDepca.priv = NULL;
  }
  thisDepca.irq=0;

  unregister_netdev(&thisDepca);
}
#endif /* MODULE */


/*
 * Local variables:
 *  kernel-compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -m486 -c depca.c"
 *
 *  module-compile-command: "gcc -D__KERNEL__ -DMODULE -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -m486 -c depca.c"
 * End:
 */
