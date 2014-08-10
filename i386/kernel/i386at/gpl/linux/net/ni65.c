/*
 * ni6510 (am7990 'lance' chip) driver for Linux-net-3 by MH
 * Alphacode v0.33 (94/08/22) for 1.1.47 (or later)
 *
 * ----------------------------------------------------------
 * WARNING: DOESN'T WORK ON MACHINES WITH MORE THAN 16MB !!!!
 * ----------------------------------------------------------
 *
 * copyright (c) 1994 M.Hipp
 *
 * This is an extension to the Linux operating system, and is covered by the
 * same Gnu Public License that covers the Linux-kernel.
 *
 * comments/bugs/suggestions can be sent to:
 *    Michael Hipp
 *    email: mhipp@student.uni-tuebingen.de
 *
 * sources:
 *  some things are from the 'ni6510-packet-driver for dos by Russ Nelson'
 *  and from the original drivers by D.Becker
 */

/*
 * Nov.18: multicast tweaked (AC).
 *
 * Aug.22: changes in xmit_intr (ack more than one xmitted-packet), ni65_send_packet (p->lock) (MH)
 *
 * July.16: fixed bugs in recv_skb and skb-alloc stuff  (MH)
 */

/*
 * known BUGS: 16MB limit
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "ni65.h"

/************************************
 * skeleton-stuff
 */

#ifndef HAVE_PORTRESERVE
#define check_region(ioaddr, size)              0
#define request_region(ioaddr, size,name)       do ; while (0)
#endif

#ifndef NET_DEBUG
#define NET_DEBUG 2
#endif
/*
static unsigned int net_debug = NET_DEBUG;
*/

#define NI65_TOTAL_SIZE    16

#define SA_ADDR0 0x02
#define SA_ADDR1 0x07
#define SA_ADDR2 0x01
#define CARD_ID0 0x00
#define CARD_ID1 0x55

/*****************************************/

#define PORT dev->base_addr

#define RMDNUM 8
#define RMDNUMMASK 0x6000 /* log2(RMDNUM)<<13 */
#define TMDNUM 4
#define TMDNUMMASK 0x4000 /* log2(TMDNUM)<<13 */

#define R_BUF_SIZE 1518
#define T_BUF_SIZE 1518

#define MEMSIZE 8+RMDNUM*8+TMDNUM*8

#define L_DATAREG 0x00
#define L_ADDRREG 0x02

#define L_RESET   0x04
#define L_CONFIG  0x05
#define L_EBASE   0x08

/* 
 * to access the am7990-regs, you have to write
 * reg-number into L_ADDRREG, then you can access it using L_DATAREG
 */
#define CSR0 0x00
#define CSR1 0x01
#define CSR2 0x02
#define CSR3 0x03

/* if you #define NO_STATIC the driver is faster but you will have (more) problems with >16MB memory */
#undef NO_STATIC

#define writereg(val,reg) {outw(reg,PORT+L_ADDRREG);inw(PORT+L_ADDRREG); \
                           outw(val,PORT+L_DATAREG);inw(PORT+L_DATAREG);}
#define readreg(reg) (outw(reg,PORT+L_ADDRREG),inw(PORT+L_ADDRREG),\
                       inw(PORT+L_DATAREG))
#define writedatareg(val) {outw(val,PORT+L_DATAREG);inw(PORT+L_DATAREG);}

static int   ni65_probe1(struct device *dev,int);
static void  ni65_interrupt(int irq, struct pt_regs *regs);
  static void recv_intr(struct device *dev);
  static void xmit_intr(struct device *dev);
static int   ni65_open(struct device *dev);
   static int am7990_reinit(struct device *dev);
static int   ni65_send_packet(struct sk_buff *skb, struct device *dev);
static int   ni65_close(struct device *dev);
static struct enet_statistics *ni65_get_stats(struct device *);

static void set_multicast_list(struct device *dev);

struct priv 
{
  struct init_block ib; 
  void *memptr;
  struct rmd *rmdhead;
  struct tmd *tmdhead;
  int rmdnum;
  int tmdnum,tmdlast;
  struct sk_buff *recv_skb[RMDNUM];
  void *tmdbufs[TMDNUM];
  int lock,xmit_queued;
  struct enet_statistics stats;
}; 

int irqtab[] = { 9,12,15,5 }; /* irq config-translate */
int dmatab[] = { 0,3,5,6 };   /* dma config-translate */

/*
 * open (most done by init) 
 */

static int ni65_open(struct device *dev)
{
  if(am7990_reinit(dev))
  {
    dev->tbusy     = 0;
    dev->interrupt = 0;
    dev->start     = 1;
    return 0;
  }
  else
  {
    dev->start = 0;
    return -EAGAIN;
  }
}

static int ni65_close(struct device *dev)
{
  outw(0,PORT+L_RESET); /* that's the hard way */
  dev->tbusy = 1;
  dev->start = 0;
  return 0; 
}

/* 
 * Probe The Card (not the lance-chip) 
 * and set hardaddress
 */ 

int ni65_probe(struct device *dev)
{
  int *port, ports[] = {0x300,0x320,0x340,0x360, 0};
  int base_addr = dev->base_addr;

  if (base_addr > 0x1ff)          /* Check a single specified location. */
     return ni65_probe1(dev, base_addr);
  else if (base_addr > 0)         /* Don't probe at all. */
     return ENXIO;

  for (port = ports; *port; port++) 
  {
    int ioaddr = *port;
    if (check_region(ioaddr, NI65_TOTAL_SIZE))
       continue;
    if( !(inb(ioaddr+L_EBASE+6) == CARD_ID0) || 
        !(inb(ioaddr+L_EBASE+7) == CARD_ID1) )
       continue;
    dev->base_addr = ioaddr;
    if (ni65_probe1(dev, ioaddr) == 0)
       return 0;
  }

  dev->base_addr = base_addr;
  return ENODEV;
}


static int ni65_probe1(struct device *dev,int ioaddr)
{
  int i;
  unsigned char station_addr[6];
  struct priv *p; 

  for(i=0;i<6;i++)
    station_addr[i] = dev->dev_addr[i] = inb(PORT+L_EBASE+i);

  if(station_addr[0] != SA_ADDR0 || station_addr[1] != SA_ADDR1)
  {
    printk("%s: wrong Hardaddress \n",dev->name);
    return ENODEV;
  }

  if(dev->irq == 0) 
    dev->irq = irqtab[(inw(PORT+L_CONFIG)>>2)&3];
  if(dev->dma == 0)  
    dev->dma = dmatab[inw(PORT+L_CONFIG)&3];

  printk("%s: %s found at %#3lx, IRQ %d DMA %d.\n", dev->name,
           "network card", dev->base_addr, dev->irq,dev->dma);

  {        
    int irqval = request_irq(dev->irq, &ni65_interrupt,0,"ni65");
    if (irqval) {
      printk ("%s: unable to get IRQ %d (irqval=%d).\n", 
                dev->name,dev->irq, irqval);
      return EAGAIN;
    }
    if(request_dma(dev->dma, "ni65") != 0)
    {
      printk("%s: Can't request dma-channel %d\n",dev->name,(int) dev->dma);
      free_irq(dev->irq);
      return EAGAIN;
    }
  }
  irq2dev_map[dev->irq] = dev;

  /* Grab the region so we can find another board if autoIRQ fails. */
        request_region(ioaddr,NI65_TOTAL_SIZE,"ni65");

  p = dev->priv = (void *) kmalloc(sizeof(struct priv),GFP_KERNEL);
  if (p == NULL)
   	return -ENOMEM;
  memset((char *) dev->priv,0,sizeof(struct priv));

  dev->open               = ni65_open;
  dev->stop               = ni65_close;
  dev->hard_start_xmit    = ni65_send_packet;
  dev->get_stats          = ni65_get_stats;
  dev->set_multicast_list = set_multicast_list;

  ether_setup(dev);

  dev->flags 	     &= ~IFF_MULTICAST;
  dev->interrupt      = 0;
  dev->tbusy          = 0;
  dev->start          = 0;

  if( (p->memptr = kmalloc(MEMSIZE,GFP_KERNEL)) == NULL) {
    printk("%s: Can't alloc TMD/RMD-buffer.\n",dev->name);
    return EAGAIN;
  }
  if( (unsigned long) (p->memptr + MEMSIZE) & 0xff000000) {
    printk("%s: Can't alloc TMD/RMD buffer in lower 16MB!\n",dev->name);
    return EAGAIN;
  }
  p->tmdhead = (struct tmd *) ((( (unsigned long)p->memptr ) + 8) & 0xfffffff8);
  p->rmdhead = (struct rmd *) (p->tmdhead + TMDNUM);   

#ifndef NO_STATIC
   for(i=0;i<TMDNUM;i++)
   {
     if( (p->tmdbufs[i] = kmalloc(T_BUF_SIZE,GFP_ATOMIC)) == NULL) {
       printk("%s: Can't alloc Xmit-Mem.\n",dev->name);
       return EAGAIN;
     }
     if( (unsigned long) (p->tmdbufs[i]+T_BUF_SIZE) & 0xff000000) {
       printk("%s: Can't alloc Xmit-Mem in lower 16MB!\n",dev->name);
       return EAGAIN;
     }
   }
#endif

   for(i=0;i<RMDNUM;i++)
   {
     if( (p->recv_skb[i] = dev_alloc_skb(R_BUF_SIZE)) == NULL) {
       printk("%s: unable to alloc recv-mem\n",dev->name);
       return EAGAIN;
     }
     if( (unsigned long) (p->recv_skb[i]->data + R_BUF_SIZE) & 0xff000000) {
       printk("%s: unable to alloc receive-memory in lower 16MB!\n",dev->name);
       return EAGAIN;
     }
   }

  return 0; /* we've found everything */
}

/* 
 * init lance (write init-values .. init-buffers) (open-helper)
 */

static int am7990_reinit(struct device *dev)
{
   int i,j;
   struct tmd *tmdp;
   struct rmd *rmdp;
   struct priv *p = (struct priv *) dev->priv;

   p->lock = 0;
   p->xmit_queued = 0;

   disable_dma(dev->dma); /* I've never worked with dma, but we do it like the packetdriver */
   set_dma_mode(dev->dma,DMA_MODE_CASCADE);
   enable_dma(dev->dma); 

   outw(0,PORT+L_RESET); /* first: reset the card */
   if(inw(PORT+L_DATAREG) != 0x4)
   {
     printk("%s: can't RESET ni6510 card: %04x\n",dev->name,(int) inw(PORT+L_DATAREG));
     disable_dma(dev->dma);
     free_dma(dev->dma);
     free_irq(dev->irq);
     return 0;
   }

   /* here: memset all buffs to zero */

   memset(p->memptr,0,MEMSIZE);

   p->tmdnum = 0; p->tmdlast = 0;
   for(i=0;i<TMDNUM;i++)
   {
     tmdp = p->tmdhead + i;
#ifndef NO_STATIC
     tmdp->u.buffer = (unsigned long) p->tmdbufs[i];     
#endif
     tmdp->u.s.status = XMIT_START | XMIT_END;
   }

   p->rmdnum = 0;
   for(i=0;i<RMDNUM;i++)
   {
     rmdp = p->rmdhead + i;
     rmdp->u.buffer = (unsigned long) p->recv_skb[i]->data;
     rmdp->u.s.status = RCV_OWN;
     rmdp->blen = -R_BUF_SIZE;
     rmdp->mlen = 0;
   }
   
   for(i=0;i<6;i++)
   {
     p->ib.eaddr[i] = dev->dev_addr[i];
   }
   p->ib.mode = 0;
   for(i=0;i<8;i++) 
     p->ib.filter[i] = 0;
   p->ib.trplow = (unsigned short) (( (unsigned long) p->tmdhead ) & 0xffff);
   p->ib.trphigh = (unsigned short) ((( (unsigned long) p->tmdhead )>>16) & 0x00ff) | TMDNUMMASK; 
   p->ib.rrplow = (unsigned short) (( (unsigned long) p->rmdhead ) & 0xffff);
   p->ib.rrphigh = (unsigned short) ((( (unsigned long) p->rmdhead )>>16) & 0x00ff) | RMDNUMMASK;

   writereg(0,CSR3);  /* busmaster/no word-swap */
   writereg((unsigned short) (((unsigned long) &(p->ib)) & 0xffff),CSR1);
   writereg((unsigned short) (((unsigned long) &(p->ib))>>16),CSR2);
   
   writereg(CSR0_INIT,CSR0); /* this changes L_ADDRREG to CSR0 */

  /*
   * NOW, WE NEVER WILL CHANGE THE L_ADDRREG, CSR0 IS ALWAYS SELECTED 
   */

    for(i=0;i<5;i++)
    {
      for(j=0;j<2000000;j++); /* wait a while */
      if(inw(PORT+L_DATAREG) & CSR0_IDON) break; /* init ok ? */
    }
    if(i == 5) 
    {
      printk("%s: can't init am7990, status: %04x\n",dev->name,(int) inw(PORT+L_DATAREG));
      disable_dma(dev->dma);
      free_dma(dev->dma);
      free_irq(dev->irq);
      return 0; /* false */
    } 

    writedatareg(CSR0_CLRALL | CSR0_INEA | CSR0_STRT); /* start lance , enable interrupts */

    return 1; /* OK */
}
 
/* 
 * interrupt handler  
 */

static void ni65_interrupt(int irq, struct pt_regs * regs)
{
  int csr0;
  struct device *dev = (struct device *) irq2dev_map[irq];

  if (dev == NULL) {
    printk ("net_interrupt(): irq %d for unknown device.\n", irq);
    return;
  }

  csr0 = inw(PORT+L_DATAREG);
  writedatareg(csr0 & CSR0_CLRALL); /* ack interrupts, disable int. */

  dev->interrupt = 1;

  if(csr0 & CSR0_ERR)
  {
     struct priv *p = (struct priv *) dev->priv;

     if(csr0 & CSR0_BABL)
       p->stats.tx_errors++;
     if(csr0 & CSR0_MISS)
       p->stats.rx_errors++;
  }

  if(csr0 & CSR0_RINT) /* RECV-int? */
  { 
    recv_intr(dev);
  }
  if(csr0 & CSR0_TINT) /* XMIT-int? */
  {  
    xmit_intr(dev);
  }

  writedatareg(CSR0_INEA);  /* reenable inter. */
  dev->interrupt = 0;

  return;
}

/*
 * We have received an Xmit-Interrupt ..
 * send a new packet if necessary
 */

static void xmit_intr(struct device *dev)
{
  int tmdstat;
  struct tmd *tmdp;
  struct priv *p = (struct priv *) dev->priv;

#ifdef NO_STATIC
  struct sk_buff *skb;
#endif

  while(p->xmit_queued)
  {
    tmdp = p->tmdhead + p->tmdlast;
    tmdstat = tmdp->u.s.status;
    if(tmdstat & XMIT_OWN)
      break;
#ifdef NO_STATIC
    skb = (struct sk_buff *) p->tmdbufs[p->tmdlast];
    dev_kfree_skb(skb,FREE_WRITE); 
#endif

    if(tmdstat & XMIT_ERR)
    {
      printk("%s: xmit-error: %04x %04x\n",dev->name,(int) tmdstat,(int) tmdp->status2);
      if(tmdp->status2 & XMIT_TDRMASK) 
        printk("%s: tdr-problems (e.g. no resistor)\n",dev->name);

     /* checking some errors */
      if(tmdp->status2 & XMIT_RTRY) 
        p->stats.tx_aborted_errors++;
      if(tmdp->status2 & XMIT_LCAR) 
        p->stats.tx_carrier_errors++;
      p->stats.tx_errors++;
      tmdp->status2 = 0;
    }
    else
      p->stats.tx_packets++;

    p->tmdlast = (p->tmdlast + 1) & (TMDNUM-1);
    if(p->tmdlast == p->tmdnum)
      p->xmit_queued = 0;
  }

  dev->tbusy = 0;
  mark_bh(NET_BH);
}

/*
 * We have received a packet
 */

static void recv_intr(struct device *dev)
{
  struct rmd *rmdp; 
  int rmdstat,len;
  struct sk_buff *skb,*skb1;
  struct priv *p = (struct priv *) dev->priv;

  rmdp = p->rmdhead + p->rmdnum;
  while(!( (rmdstat = rmdp->u.s.status) & RCV_OWN))
  {
    if( (rmdstat & (RCV_START | RCV_END)) != (RCV_START | RCV_END) ) /* is packet start & end? */ 
    {
      if(rmdstat & RCV_START)
      {
        p->stats.rx_errors++;
        p->stats.rx_length_errors++;
        printk("%s: packet too long\n",dev->name);
      }
      rmdp->u.s.status = RCV_OWN; /* change owner */
    }
    else if(rmdstat & RCV_ERR)
    {
      printk("%s: receive-error: %04x\n",dev->name,(int) rmdstat );
      p->stats.rx_errors++;
      if(rmdstat & RCV_FRAM) p->stats.rx_frame_errors++;
      if(rmdstat & RCV_OFLO) p->stats.rx_over_errors++;
      if(rmdstat & RCV_CRC)  p->stats.rx_crc_errors++;
      rmdp->u.s.status = RCV_OWN;
      printk("%s: lance-status: %04x\n",dev->name,(int) inw(PORT+L_DATAREG));
    }
    else
    {
      len = (rmdp->mlen & 0x0fff) - 4; /* -4: ignore FCS */
      skb = dev_alloc_skb(R_BUF_SIZE);
      if(skb != NULL)
      {
        if( (unsigned long) (skb->data + R_BUF_SIZE) & 0xff000000) {
          memcpy(skb_put(skb,len),p->recv_skb[p->rmdnum]->data,len);
	  skb1 = skb;
        }
        else {
          skb1 = p->recv_skb[p->rmdnum];
          p->recv_skb[p->rmdnum] = skb;
          rmdp->u.buffer = (unsigned long) skb_put(skb1,len);
        }
        rmdp->u.s.status = RCV_OWN;
        rmdp->mlen = 0;   /* not necc ???? */
        skb1->dev = dev;
        p->stats.rx_packets++;
        skb1->protocol=eth_type_trans(skb1,dev);
        netif_rx(skb1);
      }
      else
      {
        rmdp->u.s.status = RCV_OWN;
        printk("%s: can't alloc new sk_buff\n",dev->name);
        p->stats.rx_dropped++;
      }
    }
    p->rmdnum++; p->rmdnum &= RMDNUM-1;
    rmdp = p->rmdhead + p->rmdnum;
  }
}

/*
 * kick xmitter ..
 */

static int ni65_send_packet(struct sk_buff *skb, struct device *dev)
{
  struct priv *p = (struct priv *) dev->priv;
  struct tmd *tmdp;

  if(dev->tbusy)
  {
    int tickssofar = jiffies - dev->trans_start;
    if (tickssofar < 25)
      return 1;

    printk("%s: xmitter timed out, try to restart!\n",dev->name);
    am7990_reinit(dev);
    dev->tbusy=0;
    dev->trans_start = jiffies;
  }

  if(skb == NULL)
  {
    dev_tint(dev);
    return 0;
  }

  if (skb->len <= 0)
    return 0;

  if (set_bit(0, (void*)&dev->tbusy) != 0)
  {
     printk("%s: Transmitter access conflict.\n", dev->name);
     return 1;
  }
  if(set_bit(0,(void*) &p->lock) != 0)
  {
    printk("%s: Queue was locked!\n",dev->name);
    return 1;
  }

  {
    short len = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;

    tmdp = p->tmdhead + p->tmdnum;

#ifdef NO_STATIC
    tmdp->u.buffer = (unsigned long) (skb->data);
    p->tmdbufs[p->tmdnum] = skb;
#else
    memcpy((char *) (tmdp->u.buffer & 0x00ffffff),(char *)skb->data,skb->len);
    dev_kfree_skb (skb, FREE_WRITE);
#endif
    tmdp->blen = -len;
    tmdp->u.s.status = XMIT_OWN | XMIT_START | XMIT_END;

    cli();
    p->xmit_queued = 1;
    writedatareg(CSR0_TDMD | CSR0_INEA); /* enable xmit & interrupt */
    p->tmdnum++; p->tmdnum &= TMDNUM-1;
 
    if( !((p->tmdhead + p->tmdnum)->u.s.status & XMIT_OWN) ) 
      dev->tbusy = 0;
    p->lock = 0;
    sti();

    dev->trans_start = jiffies;

  }

  return 0;
}

static struct enet_statistics *ni65_get_stats(struct device *dev)
{
  return &((struct priv *) dev->priv)->stats;
}

static void set_multicast_list(struct device *dev)
{
}

/*
 * END of ni65.c 
 */

