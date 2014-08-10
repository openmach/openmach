/*  de4x5.c: A DIGITAL DE425/DE434/DE435/DE500 ethernet driver for Linux.

    Copyright 1994, 1995 Digital Equipment Corporation.

    This software may be used and distributed according to the terms of
    the GNU Public License, incorporated herein by reference.

    This driver is written for the Digital Equipment Corporation series
    of EtherWORKS ethernet cards:

	DE425 TP/COAX EISA
	DE434 TP PCI
	DE435 TP/COAX/AUI PCI
	DE500 10/100 PCI Fasternet

    The driver has been tested on a relatively busy network using the DE425,
    DE434, DE435 and DE500 cards and benchmarked with 'ttcp': it transferred
    16M of data to a DECstation 5000/200 as follows:

                TCP           UDP
             TX     RX     TX     RX
    DE425   1030k  997k   1170k  1128k
    DE434   1063k  995k   1170k  1125k
    DE435   1063k  995k   1170k  1125k
    DE500   1063k  998k   1170k  1125k  in 10Mb/s mode

    All  values are typical (in   kBytes/sec) from a  sample  of 4 for  each
    measurement. Their error is +/-20k on a quiet (private) network and also
    depend on what load the CPU has.

    The author may    be  reached as davies@wanton.lkg.dec.com  or   Digital
    Equipment Corporation, 550 King Street, Littleton MA 01460.

    =========================================================================
    This driver has been written  substantially  from scratch, although  its
    inheritance of style and stack interface from 'ewrk3.c' and in turn from
    Donald Becker's 'lance.c' should be obvious.

    Upto 15 EISA cards can be supported under this driver, limited primarily
    by the available IRQ lines.  I have  checked different configurations of
    multiple depca, EtherWORKS 3 cards and de4x5 cards and  have not found a
    problem yet (provided you have at least depca.c v0.38) ...

    PCI support  has been added  to allow the  driver to work with the DE434
    and  DE435 cards. The I/O  accesses  are a  bit of a   kludge due to the
    differences  in the  EISA and PCI    CSR address offsets  from the  base
    address.

    The ability to load  this driver as a loadable  module has been included
    and  used extensively during the  driver development (to save those long
    reboot sequences).  Loadable module support under  PCI has been achieved
    by letting any I/O address less than 0x1000 be assigned as:

                       0xghh

    where g is the bus number (usually 0 until the BIOS's get fixed)
         hh is the device number (max is 32 per bus).

    Essentially, the I/O address and IRQ information  are ignored and filled
    in later by  the PCI BIOS   during the PCI  probe.  Note  that the board
    should be in the system at boot time so that its I/O address and IRQ are
    allocated by the PCI BIOS automatically. The special case of device 0 on
    bus 0  is  not allowed  as  the probe  will think   you're autoprobing a
    module.

    To utilise this ability, you have to do 8 things:

    0) have a copy of the loadable modules code installed on your system.
    1) copy de4x5.c from the  /linux/drivers/net directory to your favourite
    temporary directory.
    2) edit the  source code near  line 2762 to reflect  the I/O address and
    IRQ you're using, or assign these when loading by:

                   insmod de4x5.o irq=x io=y

    3) compile  de4x5.c, but include -DMODULE in  the command line to ensure
    that the correct bits are compiled (see end of source code).
    4) if you are wanting to add a new  card, goto 5. Otherwise, recompile a
    kernel with the de4x5 configuration turned off and reboot.
    5) insmod de4x5.o
    6) run the net startup bits for your new eth?? interface manually 
    (usually /etc/rc.inet[12] at boot time). 
    7) enjoy!

    Note that autoprobing is not allowed in loadable modules - the system is
    already up and running and you're messing with interrupts.

    To unload a module, turn off the associated interface 
    'ifconfig eth?? down' then 'rmmod de4x5'.

    Automedia detection is included so that in  principal you can disconnect
    from, e.g.  TP, reconnect  to BNC  and  things will still work  (after a
    pause whilst the   driver figures out   where its media went).  My tests
    using ping showed that it appears to work....

    A compile time  switch to allow  Znyx  recognition has been  added. This
    "feature" is in no way supported nor tested  in this driver and the user
    may use it at his/her sole discretion.  I have had 2 conflicting reports
    that  my driver  will or   won't  work with   Znyx. Try Donald  Becker's
    'tulip.c' if this driver doesn't work for  you. I will not be supporting
    Znyx cards since I have no information on them  and can't test them in a
    system.

    TO DO:
    ------


    Revision History
    ----------------

    Version   Date        Description
  
      0.1     17-Nov-94   Initial writing. ALPHA code release.
      0.2     13-Jan-95   Added PCI support for DE435's.
      0.21    19-Jan-95   Added auto media detection.
      0.22    10-Feb-95   Fix interrupt handler call <chris@cosy.sbg.ac.at>.
                          Fix recognition bug reported by <bkm@star.rl.ac.uk>.
			  Add request/release_region code.
			  Add loadable modules support for PCI.
			  Clean up loadable modules support.
      0.23    28-Feb-95   Added DC21041 and DC21140 support. 
                          Fix missed frame counter value and initialisation.
			  Fixed EISA probe.
      0.24    11-Apr-95   Change delay routine to use <linux/udelay>.
                          Change TX_BUFFS_AVAIL macro.
			  Change media autodetection to allow manual setting.
			  Completed DE500 (DC21140) support.
      0.241   18-Apr-95   Interim release without DE500 Autosense Algorithm.
      0.242   10-May-95   Minor changes
      0.30    12-Jun-95   Timer fix for DC21140
                          Portability changes.
			  Add ALPHA changes from <jestabro@ant.tay1.dec.com>.
			  Add DE500 semi automatic autosense.
			  Add Link Fail interrupt TP failure detection.
			  Add timer based link change detection.
			  Plugged a memory leak in de4x5_queue_pkt().
      0.31    13-Jun-95   Fixed PCI stuff for 1.3.1
      0.32    26-Jun-95   Added verify_area() calls in de4x5_ioctl() from
                          suggestion by <heiko@colossus.escape.de>

    =========================================================================
*/

static const char *version = "de4x5.c:v0.32 6/26/95 davies@wanton.lkg.dec.com\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/bios32.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/segment.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <linux/time.h>
#include <linux/types.h>
#include <linux/unistd.h>

#include "de4x5.h"

#ifdef DE4X5_DEBUG
static int de4x5_debug = DE4X5_DEBUG;
#else
static int de4x5_debug = 1;
#endif

#ifdef DE4X5_AUTOSENSE              /* Should be done on a per adapter basis */
static int de4x5_autosense = DE4X5_AUTOSENSE;
#else
static int de4x5_autosense = AUTO;  /* Do auto media/mode sensing */
#endif

#ifdef DE4X5_FULL_DUPLEX            /* Should be done on a per adapter basis */
static s32 de4x5_full_duplex = 1;
#else
static s32 de4x5_full_duplex = 0;
#endif

#define DE4X5_NDA 0xffe0            /* No Device (I/O) Address */

/*
** Ethernet PROM defines
*/
#define PROBE_LENGTH    32
#define ETH_PROM_SIG    0xAA5500FFUL

/*
** Ethernet Info
*/
#define PKT_BUF_SZ	1536            /* Buffer size for each Tx/Rx buffer */
#define MAX_PKT_SZ   	1514            /* Maximum ethernet packet length */
#define MAX_DAT_SZ   	1500            /* Maximum ethernet data length */
#define MIN_DAT_SZ   	1               /* Minimum ethernet data length */
#define PKT_HDR_LEN     14              /* Addresses and data length info */
#define FAKE_FRAME_LEN  (MAX_PKT_SZ + 1)
#define QUEUE_PKT_TIMEOUT (3*HZ)        /* 3 second timeout */


#define CRC_POLYNOMIAL_BE 0x04c11db7UL   /* Ethernet CRC, big endian */
#define CRC_POLYNOMIAL_LE 0xedb88320UL   /* Ethernet CRC, little endian */

/*
** EISA bus defines
*/
#define DE4X5_EISA_IO_PORTS   0x0c00     /* I/O port base address, slot 0 */
#define DE4X5_EISA_TOTAL_SIZE 0xfff      /* I/O address extent */

#define MAX_EISA_SLOTS 16
#define EISA_SLOT_INC 0x1000

#define DE4X5_SIGNATURE {"DE425",""}
#define DE4X5_NAME_LENGTH 8

/*
** PCI Bus defines
*/
#define PCI_MAX_BUS_NUM 8
#define DE4X5_PCI_TOTAL_SIZE 0x80        /* I/O address extent */
#define DE4X5_CLASS_CODE     0x00020000  /* Network controller, Ethernet */

/*
** Memory Alignment. Each descriptor is 4 longwords long. To force a
** particular alignment on the TX descriptor, adjust DESC_SKIP_LEN and
** DESC_ALIGN. ALIGN aligns the start address of the private memory area
** and hence the RX descriptor ring's first entry. 
*/
#define ALIGN4      ((u_long)4 - 1)    /* 1 longword align */
#define ALIGN8      ((u_long)8 - 1)    /* 2 longword align */
#define ALIGN16     ((u_long)16 - 1)   /* 4 longword align */
#define ALIGN32     ((u_long)32 - 1)   /* 8 longword align */
#define ALIGN64     ((u_long)64 - 1)   /* 16 longword align */
#define ALIGN128    ((u_long)128 - 1)  /* 32 longword align */

#define ALIGN         ALIGN32          /* Keep the DC21040 happy... */
#define CACHE_ALIGN   CAL_16LONG
#define DESC_SKIP_LEN DSL_0            /* Must agree with DESC_ALIGN */
/*#define DESC_ALIGN    u32 dummy[4]; / * Must agree with DESC_SKIP_LEN */
#define DESC_ALIGN

#ifdef MACH
#define IS_NOT_DEC
#endif

#ifndef IS_NOT_DEC                     /* See README.de4x5 for using this */
static int is_not_dec = 0;
#else
static int is_not_dec = 1;
#endif

/*
** DE4X5 IRQ ENABLE/DISABLE
*/
#define ENABLE_IRQs { \
    imr |= lp->irq_en;\
    outl(imr, DE4X5_IMR);                   /* Enable the IRQs */\
}

#define DISABLE_IRQs {\
    imr = inl(DE4X5_IMR);\
    imr &= ~lp->irq_en;\
    outl(imr, DE4X5_IMR);                   /* Disable the IRQs */\
}

#define UNMASK_IRQs {\
    imr |= lp->irq_mask;\
    outl(imr, DE4X5_IMR);                   /* Unmask the IRQs */\
}

#define MASK_IRQs {\
    imr = inl(DE4X5_IMR);\
    imr &= ~lp->irq_mask;\
    outl(imr, DE4X5_IMR);                   /* Mask the IRQs */\
}

/*
** DE4X5 START/STOP
*/
#define START_DE4X5 {\
    omr = inl(DE4X5_OMR);\
    omr |= OMR_ST | OMR_SR;\
    outl(omr, DE4X5_OMR);                   /* Enable the TX and/or RX */\
}

#define STOP_DE4X5 {\
    omr = inl(DE4X5_OMR);\
    omr &= ~(OMR_ST|OMR_SR);\
    outl(omr, DE4X5_OMR);                   /* Disable the TX and/or RX */ \
}

/*
** DE4X5 SIA RESET
*/
#define RESET_SIA outl(0, DE4X5_SICR);      /* Reset SIA connectivity regs */

/*
** DE500 AUTOSENSE TIMER INTERVAL (MILLISECS)
*/
#define DE4X5_AUTOSENSE_MS  250

/*
** SROM Structure
*/
struct de4x5_srom {
  char reserved[18];
  char version;
  char num_adapters;
  char ieee_addr[6];
  char info[100];
  short chksum;
};

/*
** DE4X5 Descriptors. Make sure that all the RX buffers are contiguous
** and have sizes of both a power of 2 and a multiple of 4.
** A size of 256 bytes for each buffer could be chosen because over 90% of
** all packets in our network are <256 bytes long and 64 longword alignment
** is possible. 1536 showed better 'ttcp' performance. Take your pick. 32 TX
** descriptors are needed for machines with an ALPHA CPU.
*/
#define NUM_RX_DESC 8                        /* Number of RX descriptors */
#define NUM_TX_DESC 32                       /* Number of TX descriptors */
#define BUFF_ALLOC_RETRIES 10                /* In case of memory shortage */
#define RX_BUFF_SZ 1536                      /* Power of 2 for kmalloc and */
                                             /* Multiple of 4 for DC21040 */
struct de4x5_desc {
    volatile s32 status;
    u32 des1;
    u32 buf;
    u32 next;
    DESC_ALIGN
};

/*
** The DE4X5 private structure
*/
#define DE4X5_PKT_STAT_SZ 16
#define DE4X5_PKT_BIN_SZ  128                /* Should be >=100 unless you
                                                increase DE4X5_PKT_STAT_SZ */

struct de4x5_private {
    char adapter_name[80];                   /* Adapter name */
    struct de4x5_desc rx_ring[NUM_RX_DESC];  /* RX descriptor ring */
    struct de4x5_desc tx_ring[NUM_TX_DESC];  /* TX descriptor ring */
    struct sk_buff *skb[NUM_TX_DESC];        /* TX skb for freeing when sent */
    int rx_new, rx_old;                      /* RX descriptor ring pointers */
    int tx_new, tx_old;                      /* TX descriptor ring pointers */
    char setup_frame[SETUP_FRAME_LEN];       /* Holds MCA and PA info. */
    struct enet_statistics stats;            /* Public stats */
    struct {
	u_int bins[DE4X5_PKT_STAT_SZ]; /* Private stats counters */
	u_int unicast;
	u_int multicast;
	u_int broadcast;
	u_int excessive_collisions;
	u_int tx_underruns;
	u_int excessive_underruns;
    } pktStats;
    char rxRingSize;
    char txRingSize;
    int  bus;                                /* EISA or PCI */
    int  bus_num;                            /* PCI Bus number */
    int  chipset;                            /* DC21040, DC21041 or DC21140 */
    s32  irq_mask;                           /* Interrupt Mask (Enable) bits */
    s32  irq_en;                             /* Summary interrupt bits */
    int  media;                              /* Media (eg TP), mode (eg 100B)*/
    int  linkProb;                           /* Possible Link Problem */
    int  autosense;                          /* Allow/disallow autosensing */
    int  tx_enable;                          /* Enable descriptor polling */
    int  lostMedia;                          /* Possibly lost media */
    int  setup_f;                            /* Setup frame filtering type */
};


/*
** The transmit ring full condition is described by the tx_old and tx_new
** pointers by:
**    tx_old            = tx_new    Empty ring
**    tx_old            = tx_new+1  Full ring
**    tx_old+txRingSize = tx_new+1  Full ring  (wrapped condition)
*/
#define TX_BUFFS_AVAIL ((lp->tx_old<=lp->tx_new)?\
			 lp->tx_old+lp->txRingSize-lp->tx_new-1:\
                         lp->tx_old               -lp->tx_new-1)

/*
** Public Functions
*/
static int     de4x5_open(struct device *dev);
static int     de4x5_queue_pkt(struct sk_buff *skb, struct device *dev);
static void    de4x5_interrupt(int irq, struct pt_regs *regs);
static int     de4x5_close(struct device *dev);
static struct  enet_statistics *de4x5_get_stats(struct device *dev);
static void    set_multicast_list(struct device *dev);
static int     de4x5_ioctl(struct device *dev, struct ifreq *rq, int cmd);

/*
** Private functions
*/
static int     de4x5_hw_init(struct device *dev, u_long iobase);
static int     de4x5_init(struct device *dev);
static int     de4x5_rx(struct device *dev);
static int     de4x5_tx(struct device *dev);
static int     de4x5_ast(struct device *dev);

static int     autoconf_media(struct device *dev);
static void    create_packet(struct device *dev, char *frame, int len);
static void    dce_us_delay(u32 usec);
static void    dce_ms_delay(u32 msec);
static void    load_packet(struct device *dev, char *buf, u32 flags, struct sk_buff *skb);
static void    dc21040_autoconf(struct device *dev);
static void    dc21041_autoconf(struct device *dev);
static void    dc21140_autoconf(struct device *dev);
static int     test_media(struct device *dev, s32 irqs, s32 irq_mask, s32 csr13, s32 csr14, s32 csr15, s32 msec);
/*static int     test_sym_link(struct device *dev, u32 msec);*/
static int     ping_media(struct device *dev);
static void    reset_init_sia(struct device *dev, s32 sicr, s32 strr, s32 sigr);
static int     test_ans(struct device *dev, s32 irqs, s32 irq_mask, s32 msec);
static void    load_ms_timer(struct device *dev, u32 msec);
static int     EISA_signature(char *name, s32 eisa_id);
static int     DevicePresent(u_long iobase);
static short   srom_rd(u_long address, u_char offset);
static void    srom_latch(u_int command, u_long address);
static void    srom_command(u_int command, u_long address);
static void    srom_address(u_int command, u_long address, u_char offset);
static short   srom_data(u_int command, u_long address);
/*static void    srom_busy(u_int command, u_long address);*/
static void    sendto_srom(u_int command, u_long addr);
static int     getfrom_srom(u_long addr);
static void    SetMulticastFilter(struct device *dev);
static int     get_hw_addr(struct device *dev);

static void    eisa_probe(struct device *dev, u_long iobase);
static void    pci_probe(struct device *dev, u_long iobase);
static struct  device *alloc_device(struct device *dev, u_long iobase);
static char    *build_setup_frame(struct device *dev, int mode);
static void    disable_ast(struct device *dev);
static void    enable_ast(struct device *dev, u32 time_out);
static void    kick_tx(struct device *dev);

#ifdef MODULE
int  init_module(void);
void cleanup_module(void);
static int autoprobed = 1, loading_module = 1;
# else
static unsigned char de4x5_irq[] = {5,9,10,11};
static int autoprobed = 0, loading_module = 0;
#endif /* MODULE */

static char name[DE4X5_NAME_LENGTH + 1];
static int num_de4x5s = 0, num_eth = 0;

/*
** Kludge to get around the fact that the CSR addresses have different
** offsets in the PCI and EISA boards. Also note that the ethernet address
** PROM is accessed differently.
*/
static struct bus_type {
    int bus;
    int bus_num;
    int device;
    int chipset;
    struct de4x5_srom srom;
    int autosense;
} bus;

/*
** Miscellaneous defines...
*/
#define RESET_DE4X5 {\
    int i;\
    i=inl(DE4X5_BMR);\
    dce_ms_delay(1);\
    outl(i | BMR_SWR, DE4X5_BMR);\
    dce_ms_delay(1);\
    outl(i, DE4X5_BMR);\
    dce_ms_delay(1);\
    for (i=0;i<5;i++) {inl(DE4X5_BMR); dce_ms_delay(1);}\
    dce_ms_delay(1);\
}



int de4x5_probe(struct device *dev)
{
  int tmp = num_de4x5s, status = -ENODEV;
  u_long iobase = dev->base_addr;

  if ((iobase == 0) && loading_module){
    printk("Autoprobing is not supported when loading a module based driver.\n");
    status = -EIO;
  } else {
    eisa_probe(dev, iobase);
    pci_probe(dev, iobase);

    if ((tmp == num_de4x5s) && (iobase != 0) && loading_module) {
      printk("%s: de4x5_probe() cannot find device at 0x%04lx.\n", dev->name, 
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
de4x5_hw_init(struct device *dev, u_long iobase)
{
  struct bus_type *lp = &bus;
  int tmpbus, tmpchs, i, j, status=0;
  char *tmp;

  /* Ensure we're not sleeping */
  if (lp->chipset == DC21041) {
    outl(0, PCI_CFDA);
    dce_ms_delay(10);
  }

  RESET_DE4X5;

  if ((inl(DE4X5_STS) & (STS_TS | STS_RS)) == 0) {
    /* 
    ** Now find out what kind of DC21040/DC21041/DC21140 board we have.
    */
    if (lp->bus == PCI) {
      if (!is_not_dec) {
	if ((lp->chipset == DC21040) || (lp->chipset == DC21041)) {
	  strcpy(name, "DE435");
	} else if (lp->chipset == DC21140) {
	  strcpy(name, "DE500");                /* Must read the SROM here! */
	}
      } else {
	strcpy(name, "UNKNOWN");
      }
    } else {
      EISA_signature(name, EISA_ID0);
    }

    if (*name != '\0') {                         /* found a board signature */
      dev->base_addr = iobase;
      if (lp->bus == EISA) {
	printk("%s: %s at %04lx (EISA slot %ld)", 
	                        dev->name, name, iobase, ((iobase>>12)&0x0f));
      } else {                                   /* PCI port address */
	printk("%s: %s at %04lx (PCI bus %d, device %d)", dev->name, name,
	                                      iobase, lp->bus_num, lp->device);
      }
	
      printk(", h/w address ");
      status = get_hw_addr(dev);
      for (i = 0; i < ETH_ALEN - 1; i++) {       /* get the ethernet addr. */
	printk("%2.2x:", dev->dev_addr[i]);
      }
      printk("%2.2x,\n", dev->dev_addr[i]);
      
      tmpbus = lp->bus;
      tmpchs = lp->chipset;

      if (status == 0) {
	struct de4x5_private *lp;

	/* 
	** Reserve a section of kernel memory for the adapter
	** private area and the TX/RX descriptor rings.
	*/
	dev->priv = (void *) kmalloc(sizeof(struct de4x5_private) + ALIGN, 
				                                   GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	/*
	** Align to a longword boundary
	*/
	dev->priv = (void *)(((u_long)dev->priv + ALIGN) & ~ALIGN);
	lp = (struct de4x5_private *)dev->priv;
	memset(dev->priv, 0, sizeof(struct de4x5_private));
	lp->bus = tmpbus;
	lp->chipset = tmpchs;

	/*
	** Choose autosensing
	*/
	if (de4x5_autosense & AUTO) {
	  lp->autosense = AUTO;
	} else {
	  if (lp->chipset != DC21140) {
	    if ((lp->chipset == DC21040) && (de4x5_autosense & TP_NW)) {
	      de4x5_autosense = TP;
	    }
	    if ((lp->chipset == DC21041) && (de4x5_autosense & BNC_AUI)) {
	      de4x5_autosense = BNC;
	    }
	    lp->autosense = de4x5_autosense & 0x001f;
	  } else {
	    lp->autosense = de4x5_autosense & 0x00c0;
	  }
	}

	sprintf(lp->adapter_name,"%s (%s)", name, dev->name);
	request_region(iobase, (lp->bus == PCI ? DE4X5_PCI_TOTAL_SIZE :
			                         DE4X5_EISA_TOTAL_SIZE), 
		                                 lp->adapter_name);

	/*
	** Allocate contiguous receive buffers, long word aligned. 
	** This could be a possible memory leak if the private area
	** is ever hosed.
	*/
	for (tmp=NULL, j=0; (j<BUFF_ALLOC_RETRIES) && (tmp==NULL); j++) {
	  if ((tmp = (void *)kmalloc(RX_BUFF_SZ * NUM_RX_DESC + ALIGN, 
	       	  		                        GFP_KERNEL)) != NULL) {
	    tmp = (char *)(((u_long) tmp + ALIGN) & ~ALIGN);
	    for (i=0; i<NUM_RX_DESC; i++) {
	      lp->rx_ring[i].status = 0;
	      lp->rx_ring[i].des1 = RX_BUFF_SZ;
	      lp->rx_ring[i].buf = virt_to_bus(tmp + i * RX_BUFF_SZ);
	      lp->rx_ring[i].next = (u32)NULL;
	    }
	    barrier();
	  }
	}

	if (tmp != NULL) {
	  lp->rxRingSize = NUM_RX_DESC;
	  lp->txRingSize = NUM_TX_DESC;
	  
	  /* Write the end of list marker to the descriptor lists */
	  lp->rx_ring[lp->rxRingSize - 1].des1 |= RD_RER;
	  lp->tx_ring[lp->txRingSize - 1].des1 |= TD_TER;

	  /* Tell the adapter where the TX/RX rings are located. */
	  outl(virt_to_bus(lp->rx_ring), DE4X5_RRBA);
	  outl(virt_to_bus(lp->tx_ring), DE4X5_TRBA);

	  /* Initialise the IRQ mask and Enable/Disable */
	  lp->irq_mask = IMR_RIM | IMR_TIM | IMR_TUM ;
	  lp->irq_en   = IMR_NIM | IMR_AIM;

	  lp->tx_enable = TRUE;

	  if (dev->irq < 2) {
#ifndef MODULE
	    unsigned char irqnum;
	    s32 omr;
	    autoirq_setup(0);
	    
	    omr = inl(DE4X5_OMR);
	    outl(IMR_AIM|IMR_RUM, DE4X5_IMR); /* Unmask RUM interrupt */
	    outl(OMR_SR | omr, DE4X5_OMR);    /* Start RX w/no descriptors */

	    irqnum = autoirq_report(1);
	    if (!irqnum) {
	      printk("      and failed to detect IRQ line.\n");
	      status = -ENXIO;
	    } else {
	      for (dev->irq=0,i=0; (i<sizeof(de4x5_irq)) && (!dev->irq); i++) {
		if (irqnum == de4x5_irq[i]) {
		  dev->irq = irqnum;
		  printk("      and uses IRQ%d.\n", dev->irq);
		}
	      }
		  
	      if (!dev->irq) {
		printk("      but incorrect IRQ line detected.\n");
		status = -ENXIO;
	      }
	    }
		
	    outl(0, DE4X5_IMR);               /* Re-mask RUM interrupt */

#endif /* MODULE */
	  } else {
	    printk("      and requires IRQ%d (not probed).\n", dev->irq);
	  }
	} else {
	  printk("%s: Kernel could not allocate RX buffer memory.\n", 
		                                                    dev->name);
	  status = -ENXIO;
	}
	if (status) release_region(iobase, (lp->bus == PCI ? 
					             DE4X5_PCI_TOTAL_SIZE :
			                             DE4X5_EISA_TOTAL_SIZE));
      } else {
	printk("      which has an Ethernet PROM CRC error.\n");
	status = -ENXIO;
      }
    } else {
      status = -ENXIO;
    }
  } else {
    status = -ENXIO;
  }
  
  if (!status) {
    if (de4x5_debug > 0) {
      printk(version);
    }
    
    /* The DE4X5-specific entries in the device structure. */
    dev->open = &de4x5_open;
    dev->hard_start_xmit = &de4x5_queue_pkt;
    dev->stop = &de4x5_close;
    dev->get_stats = &de4x5_get_stats;
    dev->set_multicast_list = &set_multicast_list;
    dev->do_ioctl = &de4x5_ioctl;
    
    dev->mem_start = 0;
    
    /* Fill in the generic field of the device structure. */
    ether_setup(dev);

    /* Let the adapter sleep to save power */
    if (lp->chipset == DC21041) {
      outl(0, DE4X5_SICR);
      outl(CFDA_PSM, PCI_CFDA);
    }
  } else {                            /* Incorrectly initialised hardware */
    struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
    if (lp) {
      kfree_s(bus_to_virt(lp->rx_ring[0].buf),
	                                    RX_BUFF_SZ * NUM_RX_DESC + ALIGN);
    }
    if (dev->priv) {
      kfree_s(dev->priv, sizeof(struct de4x5_private) + ALIGN);
      dev->priv = NULL;
    }
  }

  return status;
}


static int
de4x5_open(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int i, status = 0;
  s32 imr, omr, sts;

  /*
  ** Wake up the adapter
  */
  if (lp->chipset == DC21041) {
    outl(0, PCI_CFDA);
    dce_ms_delay(10);
  }

  if (request_irq(dev->irq, (void *)de4x5_interrupt, 0, lp->adapter_name)) {
    printk("de4x5_open(): Requested IRQ%d is busy\n",dev->irq);
    status = -EAGAIN;
  } else {

    irq2dev_map[dev->irq] = dev;
    /* 
    ** Re-initialize the DE4X5... 
    */
    status = de4x5_init(dev);

    if (de4x5_debug > 1){
      printk("%s: de4x5 open with irq %d\n",dev->name,dev->irq);
      printk("\tphysical address: ");
      for (i=0;i<6;i++){
	printk("%2.2x:",(short)dev->dev_addr[i]);
      }
      printk("\n");
      printk("Descriptor head addresses:\n");
      printk("\t0x%8.8lx  0x%8.8lx\n",(u_long)lp->rx_ring,(u_long)lp->tx_ring);
      printk("Descriptor addresses:\nRX: ");
      for (i=0;i<lp->rxRingSize-1;i++){
	if (i < 3) {
	  printk("0x%8.8lx  ",(u_long)&lp->rx_ring[i].status);
	}
      }
      printk("...0x%8.8lx\n",(u_long)&lp->rx_ring[i].status);
      printk("TX: ");
      for (i=0;i<lp->txRingSize-1;i++){
	if (i < 3) {
	  printk("0x%8.8lx  ", (u_long)&lp->tx_ring[i].status);
	}
      }
      printk("...0x%8.8lx\n", (u_long)&lp->tx_ring[i].status);
      printk("Descriptor buffers:\nRX: ");
      for (i=0;i<lp->rxRingSize-1;i++){
	if (i < 3) {
	  printk("0x%8.8x  ",lp->rx_ring[i].buf);
	}
      }
      printk("...0x%8.8x\n",lp->rx_ring[i].buf);
      printk("TX: ");
      for (i=0;i<lp->txRingSize-1;i++){
	if (i < 3) {
	  printk("0x%8.8x  ", lp->tx_ring[i].buf);
	}
      }
      printk("...0x%8.8x\n", lp->tx_ring[i].buf);
      printk("Ring size: \nRX: %d\nTX: %d\n", 
	     (short)lp->rxRingSize, 
	     (short)lp->txRingSize); 
      printk("\tstatus:  %d\n", status);
    }

    if (!status) {
      dev->tbusy = 0;                         
      dev->start = 1;
      dev->interrupt = UNMASK_INTERRUPTS;
      dev->trans_start = jiffies;

      START_DE4X5;

      /* Unmask and enable DE4X5 board interrupts */
      imr = 0;
      UNMASK_IRQs;

      /* Reset any pending (stale) interrupts */
      sts = inl(DE4X5_STS);
      outl(sts, DE4X5_STS);

      ENABLE_IRQs;
    }
    if (de4x5_debug > 1) {
      printk("\tsts:  0x%08x\n", inl(DE4X5_STS));
      printk("\tbmr:  0x%08x\n", inl(DE4X5_BMR));
      printk("\timr:  0x%08x\n", inl(DE4X5_IMR));
      printk("\tomr:  0x%08x\n", inl(DE4X5_OMR));
      printk("\tsisr: 0x%08x\n", inl(DE4X5_SISR));
      printk("\tsicr: 0x%08x\n", inl(DE4X5_SICR));
      printk("\tstrr: 0x%08x\n", inl(DE4X5_STRR));
      printk("\tsigr: 0x%08x\n", inl(DE4X5_SIGR));
    }
  }

  MOD_INC_USE_COUNT;

  return status;
}

/*
** Initialize the DE4X5 operating conditions. NB: a chip problem with the
** DC21140 requires using perfect filtering mode for that chip. Since I can't
** see why I'd want > 14 multicast addresses, I may change all chips to use
** the perfect filtering mode. Keep the DMA burst length at 8: there seems
** to be data corruption problems if it is larger (UDP errors seen from a
** ttcp source).
*/
static int
de4x5_init(struct device *dev)
{  
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int i, j, status = 0;
  s32 bmr, omr;

  /* Lock out other processes whilst setting up the hardware */
  set_bit(0, (void *)&dev->tbusy);

  RESET_DE4X5;

  bmr = inl(DE4X5_BMR);
  bmr |= PBL_8 | DESC_SKIP_LEN | CACHE_ALIGN;
  outl(bmr, DE4X5_BMR);

  if (lp->chipset != DC21140) {
    omr = TR_96;
    lp->setup_f = HASH_PERF;
  } else {
    omr = OMR_SDP | OMR_SF;
    lp->setup_f = PERFECT;
  }
  outl(virt_to_bus(lp->rx_ring), DE4X5_RRBA);
  outl(virt_to_bus(lp->tx_ring), DE4X5_TRBA);

  lp->rx_new = lp->rx_old = 0;
  lp->tx_new = lp->tx_old = 0;

  for (i = 0; i < lp->rxRingSize; i++) {
    lp->rx_ring[i].status = R_OWN;
  }

  for (i = 0; i < lp->txRingSize; i++) {
    lp->tx_ring[i].status = 0;
  }

  barrier();

  /* Build the setup frame depending on filtering mode */
  SetMulticastFilter(dev);

  if (lp->chipset != DC21140) {
    load_packet(dev, lp->setup_frame, HASH_F|TD_SET|SETUP_FRAME_LEN, NULL);
  } else {
    load_packet(dev, lp->setup_frame, PERFECT_F|TD_SET|SETUP_FRAME_LEN, NULL);
  }
  outl(omr|OMR_ST, DE4X5_OMR);

  /* Poll for completion of setup frame (interrupts are disabled for now) */
  for (j=0, i=jiffies;(i<=jiffies+HZ/100) && (j==0);) {
    if (lp->tx_ring[lp->tx_new].status >= 0) j=1;
  }
  outl(omr, DE4X5_OMR);                        /* Stop everything! */

  if (j == 0) {
    printk("%s: Setup frame timed out, status %08x\n", dev->name, 
	                                                       inl(DE4X5_STS));
    status = -EIO;
  }

  lp->tx_new = (++lp->tx_new) % lp->txRingSize;
  lp->tx_old = lp->tx_new;

  /* Autoconfigure the connected port */
  if (autoconf_media(dev) == 0) {
    status = -EIO;
  }

  return 0;
}

/* 
** Writes a socket buffer address to the next available transmit descriptor
*/
static int
de4x5_queue_pkt(struct sk_buff *skb, struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int i, status = 0;
  s32 imr, omr, sts;

  /*
  ** Clean out the TX ring asynchronously to interrupts - sometimes the
  ** interrupts are lost by delayed descriptor status updates relative to
  ** the irq assertion, especially with a busy PCI bus.
  */
  if (set_bit(0, (void*)&dev->tbusy) == 0) {
    cli();
    de4x5_tx(dev);
    dev->tbusy = 0;
    sti();
  }
  
  /* 
  ** Transmitter timeout, possibly serious problems.
  ** The 'lostMedia' threshold accounts for transient errors that
  ** were noticed when switching media.
  */
  if (dev->tbusy || (lp->lostMedia > LOST_MEDIA_THRESHOLD)) {
    u_long tickssofar = jiffies - dev->trans_start;
    if ((tickssofar < QUEUE_PKT_TIMEOUT) &&
	(lp->lostMedia <= LOST_MEDIA_THRESHOLD)) {
      status = -1;
    } else {
      if (de4x5_debug >= 1) {
	printk("%s: transmit timed out, status %08x, tbusy:%ld, lostMedia:%d tickssofar:%ld, resetting.\n",dev->name, inl(DE4X5_STS), dev->tbusy, lp->lostMedia, tickssofar);
      }

      /* Stop and reset the TX and RX... */
      STOP_DE4X5;

      /* Re-queue any skb's. */
      for (i=lp->tx_old; i!=lp->tx_new; i=(++i)%lp->txRingSize) {
	if (lp->skb[i] != NULL) {
	  if (lp->skb[i]->len != FAKE_FRAME_LEN) {
	    if (lp->tx_ring[i].status == T_OWN) {
	      dev_queue_xmit(lp->skb[i], dev, SOPRI_NORMAL);
	    } else {                               /* already sent */
	      dev_kfree_skb(lp->skb[i], FREE_WRITE);
	    }
	  } else {
	    dev_kfree_skb(lp->skb[i], FREE_WRITE);
	  }
	  lp->skb[i] = NULL;
	}
      }
      if (skb->len != FAKE_FRAME_LEN) {
	dev_queue_xmit(skb, dev, SOPRI_NORMAL);
      } else {
	dev_kfree_skb(skb, FREE_WRITE);
      }

      /* Initialise the hardware */
      status = de4x5_init(dev);

      /* Unmask DE4X5 board interrupts */
      if (!status) {
	/* Start here to clean stale interrupts later */
	dev->interrupt = UNMASK_INTERRUPTS;
	dev->start = 1;
	dev->tbusy = 0;                         
	dev->trans_start = jiffies;
      
	START_DE4X5;

	/* Unmask DE4X5 board interrupts */
	imr = 0;
	UNMASK_IRQs;

	/* Clear any pending (stale) interrupts */
	sts = inl(DE4X5_STS);
	outl(sts, DE4X5_STS);

	ENABLE_IRQs;
      } else {
	printk("%s: hardware initialisation failure, status %08x.\n",
	                                            dev->name, inl(DE4X5_STS));
      }
    }
  } else if (skb == NULL) {
    dev_tint(dev);
  } else if (skb->len == FAKE_FRAME_LEN) {     /* Don't TX a fake frame! */
    dev_kfree_skb(skb, FREE_WRITE);
  } else if (skb->len > 0) {
    /* Enforce 1 process per h/w access */
    if (set_bit(0, (void*)&dev->tbusy) != 0) { 
      printk("%s: Transmitter access conflict.\n", dev->name);
      status = -1;                             /* Re-queue packet */
    } else {
      cli();
      if (TX_BUFFS_AVAIL) {                    /* Fill in a Tx ring entry */
	load_packet(dev, skb->data, TD_IC | TD_LS | TD_FS | skb->len, skb);
	if (lp->tx_enable) {
	  outl(POLL_DEMAND, DE4X5_TPD);        /* Start the TX */
	}

	lp->tx_new = (++lp->tx_new) % lp->txRingSize; /* Ensure a wrap */
	dev->trans_start = jiffies;

	if (TX_BUFFS_AVAIL) {
	  dev->tbusy = 0;                      /* Another pkt may be queued */
	}
      } else {                                 /* Ring full - re-queue */
	status = -1;
      }
      sti();
    }
  }

  return status;
}

/*
** The DE4X5 interrupt handler. 
** 
** I/O Read/Writes through intermediate PCI bridges are never 'posted',
** so that the asserted interrupt always has some real data to work with -
** if these I/O accesses are ever changed to memory accesses, ensure the
** STS write is read immediately to complete the transaction if the adapter
** is not on bus 0. Lost interrupts can still occur when the PCI bus load
** is high and descriptor status bits cannot be set before the associated
** interrupt is asserted and this routine entered.
*/
static void
de4x5_interrupt(int irq, struct pt_regs *regs)
{
    struct device *dev = (struct device *)(irq2dev_map[irq]);
    struct de4x5_private *lp;
    s32 imr, omr, sts;
    u_long iobase;

    if (dev == NULL) {
	printk ("de4x5_interrupt(): irq %d for unknown device.\n", irq);
    } else {
      lp = (struct de4x5_private *)dev->priv;
      iobase = dev->base_addr;

      if (dev->interrupt)
	printk("%s: Re-entering the interrupt handler.\n", dev->name);

      DISABLE_IRQs;                      /* Ensure non re-entrancy */
      dev->interrupt = MASK_INTERRUPTS;

      while ((sts = inl(DE4X5_STS)) & lp->irq_mask) { /* Read IRQ status */
	outl(sts, DE4X5_STS);            /* Reset the board interrupts */

	if (sts & (STS_RI | STS_RU))	 /* Rx interrupt (packet[s] arrived) */
	  de4x5_rx(dev);

	if (sts & (STS_TI | STS_TU))     /* Tx interrupt (packet sent) */
	  de4x5_tx(dev); 

	if (sts & STS_TM)                /* Autosense tick */
	  de4x5_ast(dev);

	if (sts & STS_LNF) {             /* TP Link has failed */
	  lp->lostMedia = LOST_MEDIA_THRESHOLD + 1;
	  lp->irq_mask &= ~IMR_LFM;
	  kick_tx(dev);
	}

	if (sts & STS_SE) {              /* Bus Error */
	  STOP_DE4X5;
	  printk("%s: Fatal bus error occured, sts=%#8x, device stopped.\n",
	                                                      dev->name, sts);
	}
      }

      if (TX_BUFFS_AVAIL && dev->tbusy) {/* Any resources available? */
	dev->tbusy = 0;                  /* Clear TX busy flag */
	mark_bh(NET_BH);
      }

      dev->interrupt = UNMASK_INTERRUPTS;
      ENABLE_IRQs;
    }

    return;
}

static int
de4x5_rx(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  int i, entry;
  s32 status;
  char *buf;

  for (entry = lp->rx_new; lp->rx_ring[entry].status >= 0;entry = lp->rx_new) {
    status = lp->rx_ring[entry].status;

    if (status & RD_FS) {                   /* Remember the start of frame */
      lp->rx_old = entry;
    }

    if (status & RD_LS) {                   /* Valid frame status */
      if (status & RD_ES) {	            /* There was an error. */
	lp->stats.rx_errors++;              /* Update the error stats. */
	if (status & (RD_RF | RD_TL)) lp->stats.rx_frame_errors++;
	if (status & RD_CE)           lp->stats.rx_crc_errors++;
	if (status & RD_OF)           lp->stats.rx_fifo_errors++;
      } else {                              /* A valid frame received */
	struct sk_buff *skb;
	short pkt_len = (short)(lp->rx_ring[entry].status >> 16) - 4;

	if ((skb = dev_alloc_skb(pkt_len+2)) != NULL) {
	  skb->dev = dev;
	
	  skb_reserve(skb,2);		/* Align */
	  if (entry < lp->rx_old) {         /* Wrapped buffer */
	    short len = (lp->rxRingSize - lp->rx_old) * RX_BUFF_SZ;
	    memcpy(skb_put(skb,len), bus_to_virt(lp->rx_ring[lp->rx_old].buf), len);
	    memcpy(skb_put(skb,pkt_len-len), bus_to_virt(lp->rx_ring[0].buf), pkt_len - len);
	  } else {                          /* Linear buffer */
	    memcpy(skb_put(skb,pkt_len), bus_to_virt(lp->rx_ring[lp->rx_old].buf), pkt_len);
	  }

	  /* Push up the protocol stack */
	  skb->protocol=eth_type_trans(skb,dev);
	  netif_rx(skb);

	  /* Update stats */
	  lp->stats.rx_packets++;
	  for (i=1; i<DE4X5_PKT_STAT_SZ-1; i++) {
	    if (pkt_len < (i*DE4X5_PKT_BIN_SZ)) {
	      lp->pktStats.bins[i]++;
	      i = DE4X5_PKT_STAT_SZ;
	    }
	  }
	  buf = skb->data;                  /* Look at the dest addr */
	  if (buf[0] & 0x01) {              /* Multicast/Broadcast */
	    if ((*(s32 *)&buf[0] == -1) && (*(s16 *)&buf[4] == -1)) {
	      lp->pktStats.broadcast++;
	    } else {
	      lp->pktStats.multicast++;
	    }
	  } else if ((*(s32 *)&buf[0] == *(s32 *)&dev->dev_addr[0]) &&
		     (*(s16 *)&buf[4] == *(s16 *)&dev->dev_addr[4])) {
	    lp->pktStats.unicast++;
	  }
	  
	  lp->pktStats.bins[0]++;           /* Duplicates stats.rx_packets */
	  if (lp->pktStats.bins[0] == 0) {  /* Reset counters */
	    memset((char *)&lp->pktStats, 0, sizeof(lp->pktStats));
	  }
	} else {
	  printk("%s: Insufficient memory; nuking packet.\n", dev->name);
	  lp->stats.rx_dropped++;	      /* Really, deferred. */
	  break;
	}
      }

      /* Change buffer ownership for this last frame, back to the adapter */
      for (; lp->rx_old!=entry; lp->rx_old=(++lp->rx_old)%lp->rxRingSize) {
	lp->rx_ring[lp->rx_old].status = R_OWN;
	barrier();
      }
      lp->rx_ring[entry].status = R_OWN;
      barrier();
    }

    /*
    ** Update entry information
    */
    lp->rx_new = (++lp->rx_new) % lp->rxRingSize;
  }

  return 0;
}

/*
** Buffer sent - check for TX buffer errors.
*/
static int
de4x5_tx(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int entry;
  s32 status;

  for (entry = lp->tx_old; entry != lp->tx_new; entry = lp->tx_old) {
    status = lp->tx_ring[entry].status;
    if (status < 0) {                            /* Buffer not sent yet */
      break;
    } else if (status & TD_ES) {                 /* An error happened */
      lp->stats.tx_errors++; 
      if (status & TD_NC)  lp->stats.tx_carrier_errors++;
      if (status & TD_LC)  lp->stats.tx_window_errors++;
      if (status & TD_UF)  lp->stats.tx_fifo_errors++;
      if (status & TD_LC)  lp->stats.collisions++;
      if (status & TD_EC)  lp->pktStats.excessive_collisions++;
      if (status & TD_DE)  lp->stats.tx_aborted_errors++;

      if ((status != 0x7fffffff) &&              /* Not setup frame */
	  (status & (TD_LO | TD_NC | TD_EC | TD_LF))) {
	lp->lostMedia++;
	if (lp->lostMedia > LOST_MEDIA_THRESHOLD) { /* Trip autosense */
	  kick_tx(dev);
	}
      } else {
	outl(POLL_DEMAND, DE4X5_TPD);            /* Restart a stalled TX */
      }
    } else {                                     /* Packet sent */
      lp->stats.tx_packets++;
      lp->lostMedia = 0;                         /* Remove transient problem */
    }
    /* Free the buffer if it's not a setup frame. */
    if (lp->skb[entry] != NULL) {
      dev_kfree_skb(lp->skb[entry], FREE_WRITE);
      lp->skb[entry] = NULL;
    }

    /* Update all the pointers */
    lp->tx_old = (++lp->tx_old) % lp->txRingSize;
  }

  return 0;
}

static int
de4x5_ast(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 gep;

  disable_ast(dev);

  if (lp->chipset == DC21140) {
    gep = inl(DE4X5_GEP);
    if (((lp->media == _100Mb) &&  (gep & GEP_SLNK)) ||
	((lp->media == _10Mb)  &&  (gep & GEP_LNP))  ||
	((lp->media == _10Mb)  && !(gep & GEP_SLNK)) ||
	 (lp->media == NC)) {
      if (lp->linkProb || ((lp->media == NC) && (!(gep & GEP_LNP)))) {
	lp->lostMedia = LOST_MEDIA_THRESHOLD + 1;
	lp->linkProb = 0;
	kick_tx(dev);
      } else {
	switch(lp->media) {
	case NC:
	  lp->linkProb = 0;
	  enable_ast(dev, DE4X5_AUTOSENSE_MS);
	  break;

	case _10Mb:
	  lp->linkProb = 1;                    /* Flag a potential problem */
	  enable_ast(dev, 1500);
	  break;

	case _100Mb:
	  lp->linkProb = 1;                    /* Flag a potential problem */
	  enable_ast(dev, 4000);
	  break;
	}
      }
    } else {
      lp->linkProb = 0;                        /* Link OK */
      enable_ast(dev, DE4X5_AUTOSENSE_MS);
    }
  }

  return 0;
}

static int
de4x5_close(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 imr, omr;

  dev->start = 0;
  dev->tbusy = 1;

  if (de4x5_debug > 1) {
    printk("%s: Shutting down ethercard, status was %8.8x.\n",
	   dev->name, inl(DE4X5_STS));
  }

  /* 
  ** We stop the DE4X5 here... mask interrupts and stop TX & RX
  */
  DISABLE_IRQs;

  STOP_DE4X5;

  /*
  ** Free the associated irq
  */
  free_irq(dev->irq);
  irq2dev_map[dev->irq] = 0;

  MOD_DEC_USE_COUNT;

  /* Put the adapter to sleep to save power */
  if (lp->chipset == DC21041) {
    outl(0, DE4X5_SICR);
    outl(CFDA_PSM, PCI_CFDA);
  }

  return 0;
}

static struct enet_statistics *
de4x5_get_stats(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  lp->stats.rx_missed_errors = (int) (inl(DE4X5_MFC) & (MFC_OVFL | MFC_CNTR));
    
  return &lp->stats;
}

static void load_packet(struct device *dev, char *buf, u32 flags, struct sk_buff *skb)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;

  lp->tx_ring[lp->tx_new].buf = virt_to_bus(buf);
  lp->tx_ring[lp->tx_new].des1 &= TD_TER;
  lp->tx_ring[lp->tx_new].des1 |= flags;
  lp->skb[lp->tx_new] = skb;
  barrier();
  lp->tx_ring[lp->tx_new].status = T_OWN;
  barrier();

  return;
}
/*
** Set or clear the multicast filter for this adaptor.
** num_addrs == -1	Promiscuous mode, receive all packets - now supported.
**                      Can also use the ioctls.
** num_addrs == 0	Normal mode, clear multicast list
** num_addrs > 0	Multicast mode, receive normal and MC packets, and do
** 			best-effort filtering.
** num_addrs == HASH_TABLE_LEN
**	                Set all multicast bits (pass all multicasts).
*/
static void
set_multicast_list(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  /* First, double check that the adapter is open */
  if (irq2dev_map[dev->irq] != NULL) {
    if (dev->flags & IFF_PROMISC) {         /* set promiscuous mode */
      u32 omr;
      omr = inl(DE4X5_OMR);
      omr |= OMR_PR;
      outl(omr, DE4X5_OMR);
    } else { 
      SetMulticastFilter(dev);
      if (lp->setup_f == HASH_PERF) {
	load_packet(dev, lp->setup_frame, TD_IC | HASH_F | TD_SET | 
		                                        SETUP_FRAME_LEN, NULL);
      } else {
	load_packet(dev, lp->setup_frame, TD_IC | PERFECT_F | TD_SET | 
		                                        SETUP_FRAME_LEN, NULL);
      }
      
      lp->tx_new = (++lp->tx_new) % lp->txRingSize;
      outl(POLL_DEMAND, DE4X5_TPD);                /* Start the TX */
      dev->trans_start = jiffies;
    }
  }

  return;
}

/*
** Calculate the hash code and update the logical address filter
** from a list of ethernet multicast addresses.
** Little endian crc one liner from Matt Thomas, DEC.
*/
static void SetMulticastFilter(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  struct dev_mc_list *dmi=dev->mc_list;
  u_long iobase = dev->base_addr;
  int i, j, bit, byte;
  u16 hashcode;
  u32 omr, crc, poly = CRC_POLYNOMIAL_LE;
  char *pa;
  unsigned char *addrs;

  omr = inl(DE4X5_OMR);
  omr &= ~OMR_PR;
  pa = build_setup_frame(dev, ALL);          /* Build the basic frame */

  if ((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 14)) {
    omr |= OMR_PM;                           /* Pass all multicasts */
  } else if (lp->setup_f == HASH_PERF) {
                                             /* Now update the MCA table */
    for (i=0;i<dev->mc_count;i++) {          /* for each address in the list */
      addrs=dmi->dmi_addr;
      dmi=dmi->next;
      if ((*addrs & 0x01) == 1) {            /* multicast address? */ 
	crc = 0xffffffff;                    /* init CRC for each address */
	for (byte=0;byte<ETH_ALEN;byte++) {  /* for each address byte */
	                                     /* process each address bit */ 
	  for (bit = *addrs++,j=0;j<8;j++, bit>>=1) {
	    crc = (crc >> 1) ^ (((crc ^ bit) & 0x01) ? poly : 0);
	  }
	}
	hashcode = crc & HASH_BITS;          /* hashcode is 9 LSb of CRC */
	
	byte = hashcode >> 3;                /* bit[3-8] -> byte in filter */
	bit = 1 << (hashcode & 0x07);        /* bit[0-2] -> bit in byte */
	
	byte <<= 1;                          /* calc offset into setup frame */
	if (byte & 0x02) {
	  byte -= 1;
	}
	lp->setup_frame[byte] |= bit;
      }
    }
  } else {                                   /* Perfect filtering */
    for (j=0; j<dev->mc_count; j++) {
      addrs=dmi->dmi_addr;
      dmi=dmi->next;
      for (i=0; i<ETH_ALEN; i++) { 
	*(pa + (i&1)) = *addrs++;
	if (i & 0x01) pa += 4;
      }
    }
  }
  outl(omr, DE4X5_OMR);

  return;
}

/*
** EISA bus I/O device probe. Probe from slot 1 since slot 0 is usually
** the motherboard. Upto 15 EISA devices are supported.
*/
static void eisa_probe(struct device *dev, u_long ioaddr)
{
  int i, maxSlots, status;
  u_short vendor, device;
  s32 cfid;
  u_long iobase;
  struct bus_type *lp = &bus;
  char name[DE4X5_STRLEN];

  if (!ioaddr && autoprobed) return ;            /* Been here before ! */
  if ((ioaddr < 0x1000) && (ioaddr > 0)) return; /* PCI MODULE special */

  lp->bus = EISA;

  if (ioaddr == 0) {                     /* Autoprobing */
    iobase = EISA_SLOT_INC;              /* Get the first slot address */
    i = 1;
    maxSlots = MAX_EISA_SLOTS;
  } else {                               /* Probe a specific location */
    iobase = ioaddr;
    i = (ioaddr >> 12);
    maxSlots = i + 1;
  }

  for (status = -ENODEV; (i<maxSlots) && (dev!=NULL); i++, iobase+=EISA_SLOT_INC) {
    if (EISA_signature(name, EISA_ID)) {
      cfid = inl(PCI_CFID);
      device = (u_short)(cfid >> 16);
      vendor = (u_short) cfid;

      lp->bus = EISA;
      lp->chipset = device;
      if (DevicePresent(EISA_APROM) == 0) { 
	/* Write the PCI Configuration Registers */
	outl(PCI_COMMAND_IO | PCI_COMMAND_MASTER, PCI_CFCS);
	outl(0x00004000, PCI_CFLT);
	outl(iobase, PCI_CBIO);

	if (check_region(iobase, DE4X5_EISA_TOTAL_SIZE) == 0) {
	  if ((dev = alloc_device(dev, iobase)) != NULL) {
	    if ((status = de4x5_hw_init(dev, iobase)) == 0) {
	      num_de4x5s++;
	    }
	    num_eth++;
	  }
	} else if (autoprobed) {
	  printk("%s: region already allocated at 0x%04lx.\n", dev->name, iobase);
	}
      }
    }
  }

  return;
}

/*
** PCI bus I/O device probe
** NB: PCI I/O accesses and Bus Mastering are enabled by the PCI BIOS, not
** the driver. Some PCI BIOS's, pre V2.1, need the slot + features to be
** enabled by the user first in the set up utility. Hence we just check for
** enabled features and silently ignore the card if they're not.
**
** STOP PRESS: Some BIOS's __require__ the driver to enable the bus mastering
** bit. Here, check for I/O accesses and then set BM. If you put the card in
** a non BM slot, you're on your own (and complain to the PC vendor that your
** PC doesn't conform to the PCI standard)!
*/
#define PCI_DEVICE    (dev_num << 3)
#define PCI_LAST_DEV  32

static void pci_probe(struct device *dev, u_long ioaddr)
{
  u_char irq;
  u_char pb, pbus, dev_num, dnum, dev_fn;
  u_short vendor, device, index, status;
  u_int class = DE4X5_CLASS_CODE;
  u_int iobase;
  struct bus_type *lp = &bus;

  if (!ioaddr && autoprobed) return ;        /* Been here before ! */

  if (pcibios_present()) {
    lp->bus = PCI;

    if (ioaddr < 0x1000) {
      pbus = (u_short)(ioaddr >> 8);
      dnum = (u_short)(ioaddr & 0xff);
    } else {
      pbus = 0;
      dnum = 0;
    }
    
    for (index=0; 
	 (pcibios_find_class(class, index, &pb, &dev_fn)!= PCIBIOS_DEVICE_NOT_FOUND);
	 index++) {
      dev_num = PCI_SLOT(dev_fn);

      if ((!pbus && !dnum) || ((pbus == pb) && (dnum == dev_num))) {
	pcibios_read_config_word(pb, PCI_DEVICE, PCI_VENDOR_ID, &vendor);
	pcibios_read_config_word(pb, PCI_DEVICE, PCI_DEVICE_ID, &device);
	if (is_DC21040 || is_DC21041 || is_DC21140) {
	  /* Set the device number information */
	  lp->device = dev_num;
	  lp->bus_num = pb;

	  /* Set the chipset information */
	  lp->chipset = device;

	  /* Get the board I/O address */
	  pcibios_read_config_dword(pb, PCI_DEVICE, PCI_BASE_ADDRESS_0, &iobase);
	  iobase &= CBIO_MASK;

	  /* Fetch the IRQ to be used */
	  pcibios_read_config_byte(pb, PCI_DEVICE, PCI_INTERRUPT_LINE, &irq);

	  /* Check if I/O accesses and Bus Mastering are enabled */
	  pcibios_read_config_word(pb, PCI_DEVICE, PCI_COMMAND, &status);
	  if (status & PCI_COMMAND_IO) {
	    if (!(status & PCI_COMMAND_MASTER)) {
	      status |= PCI_COMMAND_MASTER;
	      pcibios_write_config_word(pb, PCI_DEVICE, PCI_COMMAND, status);
	      pcibios_read_config_word(pb, PCI_DEVICE, PCI_COMMAND, &status);
	    }
	    if (status & PCI_COMMAND_MASTER) {
	      if ((DevicePresent(DE4X5_APROM) == 0) || is_not_dec) {
		if (check_region(iobase, DE4X5_PCI_TOTAL_SIZE) == 0) {
		  if ((dev = alloc_device(dev, iobase)) != NULL) {
		    dev->irq = irq;
		    if ((status = de4x5_hw_init(dev, iobase)) == 0) {
		      num_de4x5s++;
		    }
		    num_eth++;
		  }
		} else if (autoprobed) {
		  printk("%s: region already allocated at 0x%04x.\n", dev->name, (u_short)iobase);
		}
	      }
	    }
	  }
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
      if ((dev->base_addr == DE4X5_NDA) || (dev->base_addr == 0)) break;
      dev = dev->next;                     /* walk through eth device list */
      num_eth++;                           /* increment eth device number */
    }

    /*
    ** If an autoprobe is requested for another device, we must re-insert
    ** the request later in the list. Remember the current position first.
    */
    if ((dev->base_addr == 0) && (num_de4x5s > 0)) {
      addAutoProbe++;
      tmp = dev->next;                     /* point to the next device */
      init = dev->init;                    /* remember the probe function */
    }

    /*
    ** If at end of list and can't use current entry, malloc one up. 
    ** If memory could not be allocated, print an error message.
    */
    if ((dev->next == NULL) &&  
	!((dev->base_addr == DE4X5_NDA) || (dev->base_addr == 0))){
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
	dev->init = &de4x5_probe;          /* initialisation routine */
	num_de4x5s++;
      }
    }
    ret = dev;                             /* return current struct, or NULL */
  
    /*
    ** Now figure out what to do with the autoprobe that has to be inserted.
    ** Firstly, search the (possibly altered) list for an empty space.
    */
    if (ret != NULL) {
      if (addAutoProbe) {
	for (; (tmp->next!=NULL) && (tmp->base_addr!=DE4X5_NDA); tmp=tmp->next);

	/*
	** If no more device structures and can't use the current one, malloc
	** one up. If memory could not be allocated, print an error message.
	*/
	if ((tmp->next == NULL) && !(tmp->base_addr == DE4X5_NDA)) {
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
** Auto configure the media here rather than setting the port at compile
** time. This routine is called by de4x5_init() when a loss of media is
** detected (excessive collisions, loss of carrier, no carrier or link fail
** [TP]) to check whether the user has been sneaky and changed the port on us.
*/
static int autoconf_media(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  lp->tx_enable = YES;
  if (de4x5_debug > 0 ) {
    if (lp->chipset != DC21140) {
      printk("%s: Searching for media... ",dev->name);
    } else {
      printk("%s: Searching for mode... ",dev->name);
    }
  }

  if (lp->chipset == DC21040) {
    lp->media = (lp->autosense == AUTO ? TP : lp->autosense);
    dc21040_autoconf(dev);
  } else if (lp->chipset == DC21041) {
    lp->media = (lp->autosense == AUTO ? TP_NW : lp->autosense);
    dc21041_autoconf(dev);
  } else if (lp->chipset == DC21140) {
    disable_ast(dev);
    lp->media = (lp->autosense == AUTO ? _10Mb : lp->autosense);
    dc21140_autoconf(dev);
  }

  if (de4x5_debug > 0 ) {
    if (lp->chipset != DC21140) {
      printk("media is %s\n", (lp->media == NC  ? "unconnected!" :
			      (lp->media == TP  ? "TP." :
			      (lp->media == ANS ? "TP/Nway." :
			      (lp->media == BNC ? "BNC." : 
			      (lp->media == AUI ? "AUI." : 
				                  "BNC/AUI."
			      ))))));
    } else {
      printk("mode is %s\n",(lp->media == NC      ? "link down.":
			    (lp->media == _100Mb  ? "100Mb/s." :
			    (lp->media == _10Mb   ? "10Mb/s." :
				                    "\?\?\?"
			    ))));
    }
  }

  if (lp->media) {
    lp->lostMedia = 0;
    inl(DE4X5_MFC);                         /* Zero the lost frames counter */
    if ((lp->media == TP) || (lp->media == ANS)) {
      lp->irq_mask |= IMR_LFM;
    }
  }
  dce_ms_delay(10);

  return (lp->media);
}

static void dc21040_autoconf(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int i, linkBad;
  s32 sisr = 0, t_3s    = 3000;

  switch (lp->media) {
  case TP:
    reset_init_sia(dev, 0x8f01, 0xffff, 0x0000);
    for (linkBad=1,i=0;(i<t_3s) && linkBad && !(sisr & SISR_NCR);i++) {
      if (((sisr = inl(DE4X5_SISR)) & SISR_LKF) == 0) linkBad = 0;
      dce_ms_delay(1);
    }
    if (linkBad && (lp->autosense == AUTO)) {
      lp->media = BNC_AUI;
      dc21040_autoconf(dev);
    }
    break;

  case BNC:
  case AUI:
  case BNC_AUI:
    reset_init_sia(dev, 0x8f09, 0x0705, 0x0006);
    dce_ms_delay(500);
    linkBad = ping_media(dev);
    if (linkBad && (lp->autosense == AUTO)) {
      lp->media = EXT_SIA;
      dc21040_autoconf(dev);
    }
    break;

  case EXT_SIA:
    reset_init_sia(dev, 0x3041, 0x0000, 0x0006);
    dce_ms_delay(500);
    linkBad = ping_media(dev);
    if (linkBad && (lp->autosense == AUTO)) {
      lp->media = NC;
      dc21040_autoconf(dev);
    }
    break;

  case NC:
#ifndef __alpha__
    reset_init_sia(dev, 0x8f01, 0xffff, 0x0000);
    break;
#else
    /* JAE: for Alpha, default to BNC/AUI, *not* TP */
    reset_init_sia(dev, 0x8f09, 0x0705, 0x0006);
#endif  /* i386 */
  }

  return;
}

/*
** Autoconfigure the media when using the DC21041. AUI needs to be tested
** before BNC, because the BNC port will indicate activity if it's not
** terminated correctly. The only way to test for that is to place a loopback
** packet onto the network and watch for errors.
*/
static void dc21041_autoconf(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 sts, irqs, irq_mask, omr;

  switch (lp->media) {
  case TP_NW:
    omr = inl(DE4X5_OMR);        /* Set up full duplex for the autonegotiate */
    outl(omr | OMR_FD, DE4X5_OMR);
    irqs = STS_LNF | STS_LNP;
    irq_mask = IMR_LFM | IMR_LPM;
    sts = test_media(dev, irqs, irq_mask, 0xef01, 0xffff, 0x0008, 2400);
    if (sts & STS_LNP) {
      lp->media = ANS;
    } else {
      lp->media = AUI;
    }
    dc21041_autoconf(dev);
    break;

  case ANS:
    irqs = STS_LNP;
    irq_mask = IMR_LPM;
    sts = test_ans(dev, irqs, irq_mask, 3000);
    if (!(sts & STS_LNP) && (lp->autosense == AUTO)) {
      lp->media = TP;
      dc21041_autoconf(dev);
    }
    break;

  case TP:
    omr = inl(DE4X5_OMR);                      /* Set up half duplex for TP */
    outl(omr & ~OMR_FD, DE4X5_OMR);
    irqs = STS_LNF | STS_LNP;
    irq_mask = IMR_LFM | IMR_LPM;
    sts = test_media(dev, irqs, irq_mask, 0xef01, 0xff3f, 0x0008, 2400);
    if (!(sts & STS_LNP) && (lp->autosense == AUTO)) {
      if (inl(DE4X5_SISR) & SISR_NRA) {    /* Non selected port activity */
	lp->media = AUI;
      } else {
	lp->media = BNC;
      }
      dc21041_autoconf(dev);
    }
    break;

  case AUI:
    omr = inl(DE4X5_OMR);                      /* Set up half duplex for AUI */
    outl(omr & ~OMR_FD, DE4X5_OMR);
    irqs = 0;
    irq_mask = 0;
    sts = test_media(dev, irqs, irq_mask, 0xef09, 0xf7fd, 0x000e, 1000);
    if (!(inl(DE4X5_SISR) & SISR_SRA) && (lp->autosense == AUTO)) {
      lp->media = BNC;
      dc21041_autoconf(dev);
    }
    break;

  case BNC:
    omr = inl(DE4X5_OMR);                      /* Set up half duplex for BNC */
    outl(omr & ~OMR_FD, DE4X5_OMR);
    irqs = 0;
    irq_mask = 0;
    sts = test_media(dev, irqs, irq_mask, 0xef09, 0xf7fd, 0x0006, 1000);
    if (!(inl(DE4X5_SISR) & SISR_SRA) && (lp->autosense == AUTO)) {
      lp->media = NC;
    } else {                                   /* Ensure media connected */
      if (ping_media(dev)) lp->media = NC;
    }
    break;

  case NC:
    omr = inl(DE4X5_OMR);        /* Set up full duplex for the autonegotiate */
    outl(omr | OMR_FD, DE4X5_OMR);
    reset_init_sia(dev, 0xef01, 0xffff, 0x0008);/* Initialise the SIA */
    break;
  }

  return;
}

/*
** Reduced feature version (temporary I hope)
*/
static void dc21140_autoconf(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 omr;

  switch(lp->media) {
  case _100Mb:      /* Set 100Mb/s, MII Port with PCS Function and Scrambler */
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR));
    omr |= (de4x5_full_duplex ? OMR_FD : 0);   /* Set up Full Duplex */
    outl(omr | OMR_PS | OMR_HBD | OMR_PCS | OMR_SCR, DE4X5_OMR);
    outl(GEP_FDXD | GEP_MODE, DE4X5_GEP);
    break;

  case _10Mb:       /* Set conventional 10Mb/s ENDEC interface */
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR));
    omr |= (de4x5_full_duplex ? OMR_FD : 0);   /* Set up Full Duplex */
    outl(omr | OMR_TTM, DE4X5_OMR);
    outl(GEP_FDXD, DE4X5_GEP);
    break;
  }

  return;
}

static int
test_media(struct device *dev, s32 irqs, s32 irq_mask, s32 csr13, s32 csr14, s32 csr15, s32 msec)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 sts, time, csr12;

  reset_init_sia(dev, csr13, csr14, csr15);

  /* Set link_fail_inhibit_timer */
  load_ms_timer(dev, msec);

  /* clear all pending interrupts */
  sts = inl(DE4X5_STS);
  outl(sts, DE4X5_STS);

  /* clear csr12 NRA and SRA bits */
  csr12 = inl(DE4X5_SISR);
  outl(csr12, DE4X5_SISR);

  /* Poll for timeout - timer interrupt doesn't work correctly */
  do {
    time = inl(DE4X5_GPT) & GPT_VAL;
    sts = inl(DE4X5_STS);
  } while ((time != 0) && !(sts & irqs));

  sts = inl(DE4X5_STS);

  return sts;
}
/*
static int test_sym_link(struct device *dev, u32 msec)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  u32 gep, time;

  / * Set link_fail_inhibit_timer * /
  load_ms_timer(dev, msec);

  / * Poll for timeout or SYM_LINK=0 * /
  do {
    time = inl(DE4X5_GPT) & GPT_VAL;
    gep = inl(DE4X5_GEP) & (GEP_SLNK | GEP_LNP);
  } while ((time > 0) && (gep & GEP_SLNK));

  return gep;
}
*/
/*
** Send a packet onto the media and watch for send errors that indicate the
** media is bad or unconnected.
*/
static int ping_media(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  int i, entry, linkBad;
  s32 omr, t_3s = 4000;
  char frame[64];

  create_packet(dev, frame, sizeof(frame));

  entry = lp->tx_new;                        /* Remember the ring position */
  load_packet(dev, frame, TD_LS | TD_FS | sizeof(frame),NULL);

  omr = inl(DE4X5_OMR);
  outl(omr|OMR_ST, DE4X5_OMR);

  lp->tx_new = (++lp->tx_new) % lp->txRingSize;
  lp->tx_old = lp->tx_new;

  /* Poll for completion of frame (interrupts are disabled for now)... */
  for (linkBad=1,i=0;(i<t_3s) && linkBad;i++) {
    if ((inl(DE4X5_SISR) & SISR_NCR) == 1) break;
    if (lp->tx_ring[entry].status >= 0) linkBad=0;
    dce_ms_delay(1);
  }
  outl(omr, DE4X5_OMR); 

  return ((linkBad || (lp->tx_ring[entry].status & TD_ES)) ? 1 : 0);
}

/*
** Check the Auto Negotiation State. Return OK when a link pass interrupt
** is received and the auto-negotiation status is NWAY OK.
*/
static int test_ans(struct device *dev, s32 irqs, s32 irq_mask, s32 msec)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 sts, ans;

  outl(irq_mask, DE4X5_IMR);

  /* Set timeout limit */
  load_ms_timer(dev, msec);

  /* clear all pending interrupts */
  sts = inl(DE4X5_STS);
  outl(sts, DE4X5_STS);

  /* Poll for interrupts */
  do {
    ans = inl(DE4X5_SISR) & SISR_ANS;
    sts = inl(DE4X5_STS);
  } while (!(sts & irqs) && (ans ^ ANS_NWOK) != 0);

  return ((sts & STS_LNP) && ((ans ^ ANS_NWOK) == 0) ? STS_LNP : 0);
}

/*
**
*/
static void reset_init_sia(struct device *dev, s32 sicr, s32 strr, s32 sigr)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  RESET_SIA;
  outl(sigr, DE4X5_SIGR);
  outl(strr, DE4X5_STRR);
  outl(sicr, DE4X5_SICR);

  return;
}

/*
** Load the timer on the DC21041 and 21140. Max time is 13.42 secs.
*/
static void load_ms_timer(struct device *dev, u32 msec)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;
  s32 i = 2048, j;

  if (lp->chipset == DC21140) {
    j = inl(DE4X5_OMR);
    if ((j & OMR_TTM) && (j & OMR_PS)) {          /* 10Mb/s MII */
      i = 8192;
    } else if ((~j & OMR_TTM) && (j & OMR_PS)) {  /* 100Mb/s MII */
      i = 819;
    }
  }

  outl((s32)(msec * 10000)/i, DE4X5_GPT);

  return;
}

/*
** Create an Ethernet packet with an invalid CRC
*/
static void create_packet(struct device *dev, char *frame, int len)
{
  int i;
  char *buf = frame;

  for (i=0; i<ETH_ALEN; i++) {             /* Use this source address */
    *buf++ = dev->dev_addr[i];
  }
  for (i=0; i<ETH_ALEN; i++) {             /* Use this destination address */
    *buf++ = dev->dev_addr[i];
  }

  *buf++ = 0;                              /* Packet length (2 bytes) */
  *buf++ = 1;
  
  return;
}

/*
** Known delay in microseconds
*/
static void dce_us_delay(u32 usec)
{
  udelay(usec);

  return;
}

/*
** Known delay in milliseconds, in millisecond steps.
*/
static void dce_ms_delay(u32 msec)
{
  u_int i;
  
  for (i=0; i<msec; i++) {
    dce_us_delay(1000);
  }

  return;
}


/*
** Look for a particular board name in the EISA configuration space
*/
static int EISA_signature(char *name, s32 eisa_id)
{
  u_int i;
  const char *signatures[] = DE4X5_SIGNATURE;
  char ManCode[DE4X5_STRLEN];
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
  ManCode[3]=((Eisa.Id[2]&0x0f)+0x30);
  ManCode[4]=(((Eisa.Id[3]>>4)&0x0f)+0x30);
  ManCode[5]='\0';

  for (i=0;(*signatures[i] != '\0') && (*name == '\0');i++) {
    if (strstr(ManCode, signatures[i]) != NULL) {
      strcpy(name,ManCode);
      status = 1;
    }
  }

  return status;                           /* return the device name string */
}

/*
** Look for a special sequence in the Ethernet station address PROM that
** is common across all DIGITAL network adapter products.
** 
** Search the Ethernet address ROM for the signature. Since the ROM address
** counter can start at an arbitrary point, the search must include the entire
** probe sequence length plus the (length_of_the_signature - 1).
** Stop the search IMMEDIATELY after the signature is found so that the
** PROM address counter is correctly positioned at the start of the
** ethernet address for later read out.
*/

static int DevicePresent(u_long aprom_addr)
{
  union {
    struct {
      u32 a;
      u32 b;
    } llsig;
    char Sig[sizeof(u32) << 1];
  } dev;
  char data;
  int i, j, tmp, status = 0;
  short sigLength;
  struct bus_type *lp = &bus;

  dev.llsig.a = ETH_PROM_SIG;
  dev.llsig.b = ETH_PROM_SIG;
  sigLength = sizeof(u32) << 1;

  if (lp->chipset == DC21040) {
    for (i=0,j=0;(j<sigLength) && (i<PROBE_LENGTH+sigLength-1);i++) {
      if (lp->bus == PCI) {
	while ((tmp = inl(aprom_addr)) < 0);
	data = (char)tmp;
      } else {
	data = inb(aprom_addr);
      }
      if (dev.Sig[j] == data) {   /* track signature */
	j++;
      } else {                    /* lost signature; begin search again */
	if (data == dev.Sig[0]) {
	  j=1;
	} else {
	  j=0;
	}
      }
    }

    if (j!=sigLength) {
      status = -ENODEV;           /* search failed */
    }

  } else {                        /* use new srom */
    short *p = (short *)&lp->srom;
    for (i=0; i<(sizeof(struct de4x5_srom)>>1); i++) {
      *p++ = srom_rd(aprom_addr, i);
    }
  }

  return status;
}

static int get_hw_addr(struct device *dev)
{
  u_long iobase = dev->base_addr;
  int i, k, tmp, status = 0;
  u_short j,chksum;
  struct bus_type *lp = &bus;

  for (i=0,k=0,j=0;j<3;j++) {
    k <<= 1 ;
    if (k > 0xffff) k-=0xffff;

    if (lp->bus == PCI) {
      if (lp->chipset == DC21040) {
	while ((tmp = inl(DE4X5_APROM)) < 0);
	k += (u_char) tmp;
	dev->dev_addr[i++] = (u_char) tmp;
	while ((tmp = inl(DE4X5_APROM)) < 0);
	k += (u_short) (tmp << 8);
	dev->dev_addr[i++] = (u_char) tmp;
      } else {
	dev->dev_addr[i] = (u_char) lp->srom.ieee_addr[i]; i++;
	dev->dev_addr[i] = (u_char) lp->srom.ieee_addr[i]; i++;
      }
    } else {
      k += (u_char) (tmp = inb(EISA_APROM));
      dev->dev_addr[i++] = (u_char) tmp;
      k += (u_short) ((tmp = inb(EISA_APROM)) << 8);
      dev->dev_addr[i++] = (u_char) tmp;
    }

    if (k > 0xffff) k-=0xffff;
  }
  if (k == 0xffff) k=0;

  if (lp->bus == PCI) {
    if (lp->chipset == DC21040) {
      while ((tmp = inl(DE4X5_APROM)) < 0);
      chksum = (u_char) tmp;
      while ((tmp = inl(DE4X5_APROM)) < 0);
      chksum |= (u_short) (tmp << 8);
      if (k != chksum) status = -1;
    }
  } else {
    chksum = (u_char) inb(EISA_APROM);
    chksum |= (u_short) (inb(EISA_APROM) << 8);
    if (k != chksum) status = -1;
  }


  return status;
}

/*
** SROM Read
*/
static short srom_rd(u_long addr, u_char offset)
{
  sendto_srom(SROM_RD | SROM_SR, addr);

  srom_latch(SROM_RD | SROM_SR | DT_CS, addr);
  srom_command(SROM_RD | SROM_SR | DT_IN | DT_CS, addr);
  srom_address(SROM_RD | SROM_SR | DT_CS, addr, offset);

  return srom_data(SROM_RD | SROM_SR | DT_CS, addr);
}

static void srom_latch(u_int command, u_long addr)
{
  sendto_srom(command, addr);
  sendto_srom(command | DT_CLK, addr);
  sendto_srom(command, addr);

  return;
}

static void srom_command(u_int command, u_long addr)
{
  srom_latch(command, addr);
  srom_latch(command, addr);
  srom_latch((command & 0x0000ff00) | DT_CS, addr);

  return;
}

static void srom_address(u_int command, u_long addr, u_char offset)
{
  int i;
  char a;

  a = (char)(offset << 2);
  for (i=0; i<6; i++, a <<= 1) {
    srom_latch(command | ((a < 0) ? DT_IN : 0), addr);
  }
  dce_us_delay(1);

  i = (getfrom_srom(addr) >> 3) & 0x01;
  if (i != 0) {
    printk("Bad SROM address phase.....\n");
/*    printk(".");*/
  }

  return;
}

static short srom_data(u_int command, u_long addr)
{
  int i;
  short word = 0;
  s32 tmp;

  for (i=0; i<16; i++) {
    sendto_srom(command  | DT_CLK, addr);
    tmp = getfrom_srom(addr);
    sendto_srom(command, addr);

    word = (word << 1) | ((tmp >> 3) & 0x01);
  }

  sendto_srom(command & 0x0000ff00, addr);

  return word;
}

/*
static void srom_busy(u_int command, u_long addr)
{
  sendto_srom((command & 0x0000ff00) | DT_CS, addr);

  while (!((getfrom_srom(addr) >> 3) & 0x01)) {
    dce_ms_delay(1);
  }

  sendto_srom(command & 0x0000ff00, addr);

  return;
}
*/

static void sendto_srom(u_int command, u_long addr)
{
  outl(command, addr);
  dce_us_delay(1);

  return;
}

static int getfrom_srom(u_long addr)
{
  s32 tmp;

  tmp = inl(addr);
  dce_us_delay(1);

  return tmp;
}

static char *build_setup_frame(struct device *dev, int mode)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  int i;
  char *pa = lp->setup_frame;

  /* Initialise the setup frame */
  if (mode == ALL) {
    memset(lp->setup_frame, 0, SETUP_FRAME_LEN);
  }

  if (lp->setup_f == HASH_PERF) {
    for (pa=lp->setup_frame+IMPERF_PA_OFFSET, i=0; i<ETH_ALEN; i++) {
      *(pa + i) = dev->dev_addr[i];                 /* Host address */
      if (i & 0x01) pa += 2;
    }
    *(lp->setup_frame + (HASH_TABLE_LEN >> 3) - 3) = 0x80; /* B'cast address */
  } else {
    for (i=0; i<ETH_ALEN; i++) { /* Host address */
      *(pa + (i&1)) = dev->dev_addr[i];
      if (i & 0x01) pa += 4;
    }
    for (i=0; i<ETH_ALEN; i++) { /* Broadcast address */
      *(pa + (i&1)) = (char) 0xff;
      if (i & 0x01) pa += 4;
    }
  }

  return pa;                     /* Points to the next entry */
}

static void enable_ast(struct device *dev, u32 time_out)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  lp->irq_mask |= IMR_TMM;
  outl(lp->irq_mask, DE4X5_IMR);
  load_ms_timer(dev, time_out);

  return;
}

static void disable_ast(struct device *dev)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  u_long iobase = dev->base_addr;

  lp->irq_mask &= ~IMR_TMM;
  outl(lp->irq_mask, DE4X5_IMR);
  load_ms_timer(dev, 0);

  return;
}

static void kick_tx(struct device *dev)
{
  struct sk_buff *skb;

  if ((skb = alloc_skb(0, GFP_ATOMIC)) != NULL) {
    skb->len= FAKE_FRAME_LEN;
    skb->arp=1;
    skb->dev=dev;
    dev_queue_xmit(skb, dev, SOPRI_NORMAL);
  }

  return;
}

/*
** Perform IOCTL call functions here. Some are privileged operations and the
** effective uid is checked in those cases.
*/
static int de4x5_ioctl(struct device *dev, struct ifreq *rq, int cmd)
{
  struct de4x5_private *lp = (struct de4x5_private *)dev->priv;
  struct de4x5_ioctl *ioc = (struct de4x5_ioctl *) &rq->ifr_data;
  u_long iobase = dev->base_addr;
  int i, j, status = 0;
  s32 omr;
  union {
    u8  addr[(HASH_TABLE_LEN * ETH_ALEN)];
    u16 sval[(HASH_TABLE_LEN * ETH_ALEN) >> 1];
    u32 lval[(HASH_TABLE_LEN * ETH_ALEN) >> 2];
  } tmp;

  switch(ioc->cmd) {
  case DE4X5_GET_HWADDR:             /* Get the hardware address */
    ioc->len = ETH_ALEN;
    status = verify_area(VERIFY_WRITE, (void *)ioc->data, ioc->len);
    if (status)
      break;
    for (i=0; i<ETH_ALEN; i++) {
      tmp.addr[i] = dev->dev_addr[i];
    }
    memcpy_tofs(ioc->data, tmp.addr, ioc->len);
    
    break;
  case DE4X5_SET_HWADDR:             /* Set the hardware address */
    status = verify_area(VERIFY_READ, (void *)ioc->data, ETH_ALEN);
    if (status)
      break;
    status = -EPERM;
    if (!suser())
      break;
    status = 0;
    memcpy_fromfs(tmp.addr, ioc->data, ETH_ALEN);
    for (i=0; i<ETH_ALEN; i++) {
      dev->dev_addr[i] = tmp.addr[i];
    }
    build_setup_frame(dev, PHYS_ADDR_ONLY);
    /* Set up the descriptor and give ownership to the card */
    while (set_bit(0, (void *)&dev->tbusy) != 0);/* Wait for lock to free*/
    if (lp->setup_f == HASH_PERF) {
      load_packet(dev, lp->setup_frame, TD_IC | HASH_F | TD_SET | 
    	                                        SETUP_FRAME_LEN, NULL);
    } else {
      load_packet(dev, lp->setup_frame, TD_IC | PERFECT_F | TD_SET | 
    	                                        SETUP_FRAME_LEN, NULL);
    }
    lp->tx_new = (++lp->tx_new) % lp->txRingSize;
    outl(POLL_DEMAND, DE4X5_TPD);                /* Start the TX */
    dev->tbusy = 0;                              /* Unlock the TX ring */

    break;
  case DE4X5_SET_PROM:               /* Set Promiscuous Mode */
    if (suser()) {
      omr = inl(DE4X5_OMR);
      omr |= OMR_PR;
      outl(omr, DE4X5_OMR);
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_CLR_PROM:               /* Clear Promiscuous Mode */
    if (suser()) {
      omr = inl(DE4X5_OMR);
      omr &= ~OMR_PR;
      outb(omr, DE4X5_OMR);
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_SAY_BOO:                /* Say "Boo!" to the kernel log file */
    printk("%s: Boo!\n", dev->name);

    break;
  case DE4X5_GET_MCA:                /* Get the multicast address table */
    ioc->len = (HASH_TABLE_LEN >> 3);
    status = verify_area(VERIFY_WRITE, ioc->data, ioc->len);
    if (status)
      break;
    memcpy_tofs(ioc->data, lp->setup_frame, ioc->len); 

    break;
  case DE4X5_SET_MCA:                /* Set a multicast address */
    if (suser()) {
      if (ioc->len != HASH_TABLE_LEN) {         /* MCA changes */
	if (!(status = verify_area(VERIFY_READ, (void *)ioc->data, ETH_ALEN * ioc->len))) {
	  memcpy_fromfs(tmp.addr, ioc->data, ETH_ALEN * ioc->len);
	  set_multicast_list(dev);
	}
      } else {
	set_multicast_list(dev);
      }
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_CLR_MCA:                /* Clear all multicast addresses */
    if (suser()) {
      set_multicast_list(dev);
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_MCA_EN:                 /* Enable pass all multicast addressing */
    if (suser()) {
      omr = inl(DE4X5_OMR);
      omr |= OMR_PM;
      outl(omr, DE4X5_OMR);
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_GET_STATS:              /* Get the driver statistics */
    ioc->len = sizeof(lp->pktStats);
    status = verify_area(VERIFY_WRITE, (void *)ioc->data, ioc->len);
    if (status)
      break;

    cli();
    memcpy_tofs(ioc->data, &lp->pktStats, ioc->len); 
    sti();

    break;
  case DE4X5_CLR_STATS:              /* Zero out the driver statistics */
    if (suser()) {
      cli();
      memset(&lp->pktStats, 0, sizeof(lp->pktStats));
      sti();
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_GET_OMR:                /* Get the OMR Register contents */
    tmp.addr[0] = inl(DE4X5_OMR);
    if (!(status = verify_area(VERIFY_WRITE, (void *)ioc->data, 1))) {
      memcpy_tofs(ioc->data, tmp.addr, 1);
    }

    break;
  case DE4X5_SET_OMR:                /* Set the OMR Register contents */
    if (suser()) {
      if (!(status = verify_area(VERIFY_READ, (void *)ioc->data, 1))) {
	memcpy_fromfs(tmp.addr, ioc->data, 1);
	outl(tmp.addr[0], DE4X5_OMR);
      }
    } else {
      status = -EPERM;
    }

    break;
  case DE4X5_GET_REG:                /* Get the DE4X5 Registers */
    j = 0;
    tmp.lval[0] = inl(DE4X5_STS); j+=4;
    tmp.lval[1] = inl(DE4X5_BMR); j+=4;
    tmp.lval[2] = inl(DE4X5_IMR); j+=4;
    tmp.lval[3] = inl(DE4X5_OMR); j+=4;
    tmp.lval[4] = inl(DE4X5_SISR); j+=4;
    tmp.lval[5] = inl(DE4X5_SICR); j+=4;
    tmp.lval[6] = inl(DE4X5_STRR); j+=4;
    tmp.lval[7] = inl(DE4X5_SIGR); j+=4;
    ioc->len = j;
    if (!(status = verify_area(VERIFY_WRITE, (void *)ioc->data, ioc->len))) {
      memcpy_tofs(ioc->data, tmp.addr, ioc->len);
    }
    break;

#define DE4X5_DUMP              0x0f /* Dump the DE4X5 Status */

  case DE4X5_DUMP:
    j = 0;
    tmp.addr[j++] = dev->irq;
    for (i=0; i<ETH_ALEN; i++) {
      tmp.addr[j++] = dev->dev_addr[i];
    }
    tmp.addr[j++] = lp->rxRingSize;
    tmp.lval[j>>2] = (long)lp->rx_ring; j+=4;
    tmp.lval[j>>2] = (long)lp->tx_ring; j+=4;

    for (i=0;i<lp->rxRingSize-1;i++){
      if (i < 3) {
	tmp.lval[j>>2] = (long)&lp->rx_ring[i].status; j+=4;
      }
    }
    tmp.lval[j>>2] = (long)&lp->rx_ring[i].status; j+=4;
    for (i=0;i<lp->txRingSize-1;i++){
      if (i < 3) {
	tmp.lval[j>>2] = (long)&lp->tx_ring[i].status; j+=4;
      }
    }
    tmp.lval[j>>2] = (long)&lp->tx_ring[i].status; j+=4;
      
    for (i=0;i<lp->rxRingSize-1;i++){
      if (i < 3) {
	tmp.lval[j>>2] = (s32)lp->rx_ring[i].buf; j+=4;
      }
    }
    tmp.lval[j>>2] = (s32)lp->rx_ring[i].buf; j+=4;
    for (i=0;i<lp->txRingSize-1;i++){
      if (i < 3) {
	tmp.lval[j>>2] = (s32)lp->tx_ring[i].buf; j+=4;
      }
    }
    tmp.lval[j>>2] = (s32)lp->tx_ring[i].buf; j+=4;
      
    for (i=0;i<lp->rxRingSize;i++){
      tmp.lval[j>>2] = lp->rx_ring[i].status; j+=4;
    }
    for (i=0;i<lp->txRingSize;i++){
      tmp.lval[j>>2] = lp->tx_ring[i].status; j+=4;
    }

    tmp.lval[j>>2] = inl(DE4X5_STS); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_BMR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_IMR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_OMR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_SISR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_SICR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_STRR); j+=4;
    tmp.lval[j>>2] = inl(DE4X5_SIGR); j+=4; 
    
    tmp.addr[j++] = lp->txRingSize;
    tmp.addr[j++] = dev->tbusy;
      
    ioc->len = j;
    if (!(status = verify_area(VERIFY_WRITE, (void *)ioc->data, ioc->len))) {
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
static struct device thisDE4X5 = {
  devicename, /* device name is inserted by linux/drivers/net/net_init.c */
  0, 0, 0, 0,
  0x2000, 10, /* I/O address, IRQ */
  0, 0, 0, NULL, de4x5_probe };
	
static int io=0x000b;	/* EDIT THESE LINES FOR YOUR CONFIGURATION */
static int irq=10;	/* or use the insmod io= irq= options 		*/

int
init_module(void)
{
  thisDE4X5.base_addr=io;
  thisDE4X5.irq=irq;
  if (register_netdev(&thisDE4X5) != 0)
    return -EIO;
  return 0;
}

void
cleanup_module(void)
{
  struct de4x5_private *lp = (struct de4x5_private *) thisDE4X5.priv;

  if (lp) {
    kfree_s(bus_to_virt(lp->rx_ring[0].buf), RX_BUFF_SZ * NUM_RX_DESC + ALIGN);
  }
  kfree_s(thisDE4X5.priv, sizeof(struct de4x5_private) + ALIGN);
  thisDE4X5.priv = NULL;

  release_region(thisDE4X5.base_addr, (lp->bus == PCI ? 
					             DE4X5_PCI_TOTAL_SIZE :
			                             DE4X5_EISA_TOTAL_SIZE));
  unregister_netdev(&thisDE4X5);
}
#endif /* MODULE */


/*
 * Local variables:
 *  kernel-compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -m486 -c de4x5.c"
 *
 *  module-compile-command: "gcc -D__KERNEL__ -DMODULE -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -m486 -c de4x5.c"
 * End:
 */


