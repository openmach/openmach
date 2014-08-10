/* 
 * net-3-driver for the NI5210 card (i82586 Ethernet chip)
 *
 * This is an extension to the Linux operating system, and is covered by the
 * same Gnu Public License that covers that work.
 * 
 * Alphacode 0.62 (95/01/19) for Linux 1.1.82 (or later)
 * Copyrights (c) 1994,1995 by M.Hipp (Michael.Hipp@student.uni-tuebingen.de)
 *    [feel free to mail ....]
 *
 * CAN YOU PLEASE REPORT ME YOUR PERFORMANCE EXPERIENCES !!.
 * 
 * If you find a bug, please report me:
 *   The kernel panic output and any kmsg from the ni52 driver
 *   the ni5210-driver-version and the linux-kernel version 
 *   how many shared memory (memsize) on the netcard, 
 *   bootprom: yes/no, base_addr, mem_start
 *   maybe the ni5210-card revision and the i82586 version
 *
 * autoprobe for: base_addr: 0x300,0x280,0x360,0x320,0x340
 *                mem_start: 0xc8000,0xd0000,0xd4000,0xd8000 (8K and 16K)
 *
 * sources:
 *   skeleton.c from Donald Becker
 *
 * I have also done a look in the following sources: (mail me if you need them)
 *   crynwr-packet-driver by Russ Nelson
 *   Garret A. Wollman's (fourth) i82586-driver for BSD
 *   (before getting an i82596 (yes 596 not 586) manual, the existing drivers helped
 *    me a lot to understand this tricky chip.)
 *
 * Known Problems:
 *   The internal sysbus seems to be slow. So we often lose packets because of
 *   overruns while receiving from a fast remote host. 
 *   This can slow down TCP connections. Maybe the newer ni5210 cards are better.
 * 
 * IMPORTANT NOTE:
 *   On fast networks, it's a (very) good idea to have 16K shared memory. With
 *   8K, we can store only 4 receive frames, so it can (easily) happen that a remote 
 *   machine 'overruns' our system.
 *
 * Known i82586 bugs (I'm sure, there are many more!):
 *   Running the NOP-mode, the i82586 sometimes seems to forget to report
 *   every xmit-interrupt until we restart the CU.
 *   Another MAJOR bug is, that the RU sometimes seems to ignore the EL-Bit 
 *   in the RBD-Struct which indicates an end of the RBD queue. 
 *   Instead, the RU fetches another (randomly selected and 
 *   usually used) RBD and begins to fill it. (Maybe, this happens only if 
 *   the last buffer from the previous RFD fits exact into the queue and
 *   the next RFD can't fetch an initial RBD. Anyone knows more? )
 */

/*
 * 18.Nov.95: Mcast changes (AC).
 *
 * 19.Jan.95: verified (MH)
 *
 * 19.Sep.94: Added Multicast support (not tested yet) (MH)
 * 
 * 18.Sep.94: Workaround for 'EL-Bug'. Removed flexible RBD-handling. 
 *            Now, every RFD has exact one RBD. (MH)
 *
 * 14.Sep.94: added promiscuous mode, a few cleanups (MH)
 *
 * 19.Aug.94: changed request_irq() parameter (MH)
 * 
 * 20.July.94: removed cleanup bugs, removed a 16K-mem-probe-bug (MH)
 *
 * 19.July.94: lotsa cleanups .. (MH)
 *
 * 17.July.94: some patches ... verified to run with 1.1.29 (MH)
 *
 * 4.July.94: patches for Linux 1.1.24  (MH)
 *
 * 26.March.94: patches for Linux 1.0 and iomem-auto-probe (MH)
 *
 * 30.Sep.93: Added nop-chain .. driver now runs with only one Xmit-Buff, too (MH)
 *
 * < 30.Sep.93: first versions 
 */
 
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "ni52.h"

#define DEBUG       /* debug on */
#define SYSBUSVAL 1 /* 8 Bit */

#define ni_attn586()  {outb(0,dev->base_addr+NI52_ATTENTION);}
#define ni_reset586() {outb(0,dev->base_addr+NI52_RESET);}

#define make32(ptr16) (p->memtop + (short) (ptr16) )
#define make24(ptr32) ((char *) (ptr32) - p->base)
#define make16(ptr32) ((unsigned short) ((unsigned long) (ptr32) - (unsigned long) p->memtop ))

/******************* how to calculate the buffers *****************************

  * IMPORTANT NOTE: if you configure only one NUM_XMIT_BUFFS, the driver works
  * --------------- in a different (more stable?) mode. Only in this mode it's
  *                 possible to configure the driver with 'NO_NOPCOMMANDS'

sizeof(scp)=12; sizeof(scb)=16; sizeof(iscp)=8;
sizeof(scp)+sizeof(iscp)+sizeof(scb) = 36 = INIT
sizeof(rfd) = 24; sizeof(rbd) = 12; 
sizeof(tbd) = 8; sizeof(transmit_cmd) = 16;
sizeof(nop_cmd) = 8; 

  * if you don't know the driver, better do not change this values: */

#define RECV_BUFF_SIZE 1524 /* slightly oversized */
#define XMIT_BUFF_SIZE 1524 /* slightly oversized */
#define NUM_XMIT_BUFFS 1    /* config for both, 8K and 16K shmem */
#define NUM_RECV_BUFFS_8  4 /* config for 8K shared mem */
#define NUM_RECV_BUFFS_16 9 /* config for 16K shared mem */
#define NO_NOPCOMMANDS      /* only possible with NUM_XMIT_BUFFS=1 */

/**************************************************************************/

#define DELAY(x) {int i=jiffies; \
                  if(loops_per_sec == 1) \
                     while(i+(x)>jiffies); \
                  else \
                     __delay((loops_per_sec>>5)*x); \
                 }

/* a much shorter delay: */
#define DELAY_16(); { __delay( (loops_per_sec>>16)+1 ); }

/* wait for command with timeout: */
#define WAIT_4_SCB_CMD() { int i; \
  for(i=0;i<1024;i++) { \
    if(!p->scb->cmd) break; \
    DELAY_16(); \
    if(i == 1023) { \
      printk("%s: scb_cmd timed out .. resetting i82586\n",dev->name); \
      ni_reset586(); } } }


#define NI52_TOTAL_SIZE 16
#define NI52_ADDR0 0x02
#define NI52_ADDR1 0x07
#define NI52_ADDR2 0x01

#ifndef HAVE_PORTRESERVE
#define check_region(ioaddr, size)              0
#define request_region(ioaddr, size,name)    do ; while (0)
#endif

static int     ni52_probe1(struct device *dev,int ioaddr);
static void    ni52_interrupt(int irq,struct pt_regs *reg_ptr);
static int     ni52_open(struct device *dev);
static int     ni52_close(struct device *dev);
static int     ni52_send_packet(struct sk_buff *,struct device *);
static struct  enet_statistics *ni52_get_stats(struct device *dev);
static void    set_multicast_list(struct device *dev);

/* helper-functions */
static int     init586(struct device *dev);
static int     check586(struct device *dev,char *where,unsigned size);
static void    alloc586(struct device *dev);
static void    startrecv586(struct device *dev);
static void   *alloc_rfa(struct device *dev,void *ptr);
static void    ni52_rcv_int(struct device *dev);
static void    ni52_xmt_int(struct device *dev);
static void    ni52_rnr_int(struct device *dev);

struct priv
{
  struct enet_statistics stats;
  unsigned long base;
  char *memtop;
  volatile struct rfd_struct  *rfd_last,*rfd_top,*rfd_first;
  volatile struct scp_struct  *scp;  /* volatile is important */
  volatile struct iscp_struct *iscp; /* volatile is important */
  volatile struct scb_struct  *scb;  /* volatile is important */
  volatile struct tbd_struct  *xmit_buffs[NUM_XMIT_BUFFS];
  volatile struct transmit_cmd_struct *xmit_cmds[NUM_XMIT_BUFFS];
#if (NUM_XMIT_BUFFS == 1)
  volatile struct nop_cmd_struct *nop_cmds[2];
#else
  volatile struct nop_cmd_struct *nop_cmds[NUM_XMIT_BUFFS];
#endif
  volatile int    nop_point,num_recv_buffs;
  volatile char  *xmit_cbuffs[NUM_XMIT_BUFFS];
  volatile int    xmit_count,xmit_last;
};


/**********************************************
 * close device 
 */

static int ni52_close(struct device *dev)
{
  free_irq(dev->irq);
  irq2dev_map[dev->irq] = 0;

  ni_reset586(); /* the hard way to stop the receiver */

  dev->start = 0;
  dev->tbusy = 0;

  return 0;
}

/**********************************************
 * open device 
 */

static int ni52_open(struct device *dev)
{
  alloc586(dev);
  init586(dev);  
  startrecv586(dev);

  if(request_irq(dev->irq, &ni52_interrupt,0,"ni52")) 
  {    
    ni_reset586();
    return -EAGAIN;
  }  
  irq2dev_map[dev->irq] = dev;

  dev->interrupt = 0;
  dev->tbusy = 0;
  dev->start = 1;

  return 0; /* most done by init */
}

/**********************************************
 * Check to see if there's an 82586 out there. 
 */

static int check586(struct device *dev,char *where,unsigned size)
{
  struct priv *p = (struct priv *) dev->priv;
  char *iscp_addrs[2];
  int i;

  p->base = (unsigned long) where + size - 0x01000000;
  p->memtop = where + size;
  p->scp = (struct scp_struct *)(p->base + SCP_DEFAULT_ADDRESS);
  memset((char *)p->scp,0, sizeof(struct scp_struct));
  p->scp->sysbus = SYSBUSVAL;        /* 1 = 8Bit-Bus, 0 = 16 Bit */
  
  iscp_addrs[0] = where;
  iscp_addrs[1]= (char *) p->scp - sizeof(struct iscp_struct);

  for(i=0;i<2;i++)
  {
    p->iscp = (struct iscp_struct *) iscp_addrs[i];
    memset((char *)p->iscp,0, sizeof(struct iscp_struct));

    p->scp->iscp = make24(p->iscp);
    p->iscp->busy = 1;

    ni_reset586();
    ni_attn586();
    DELAY(2);	/* wait a while... */

    if(p->iscp->busy) /* i82586 clears 'busy' after successful init */
      return 0;
  }
  return 1;
}

/******************************************************************
 * set iscp at the right place, called by ni52_probe1 and open586. 
 */

void alloc586(struct device *dev)
{
  struct priv *p =  (struct priv *) dev->priv; 

  ni_reset586();
  DELAY(2);

  p->scp  = (struct scp_struct *)  (p->base + SCP_DEFAULT_ADDRESS);
  p->scb  = (struct scb_struct *)  (dev->mem_start);
  p->iscp = (struct iscp_struct *) ((char *)p->scp - sizeof(struct iscp_struct));

  memset((char *) p->iscp,0,sizeof(struct iscp_struct));
  memset((char *) p->scp ,0,sizeof(struct scp_struct));

  p->scp->iscp = make24(p->iscp);
  p->scp->sysbus = SYSBUSVAL;
  p->iscp->scb_offset = make16(p->scb);

  p->iscp->busy = 1;
  ni_reset586();
  ni_attn586();

  DELAY(2); 

  if(p->iscp->busy)
    printk("%s: Init-Problems (alloc).\n",dev->name);

  memset((char *)p->scb,0,sizeof(struct scb_struct));
}

/**********************************************
 * probe the ni5210-card
 */

int ni52_probe(struct device *dev)
{
  int *port, ports[] = {0x300, 0x280, 0x360 , 0x320 , 0x340, 0};
  int base_addr = dev->base_addr;

  if (base_addr > 0x1ff)		/* Check a single specified location. */
    if( (inb(base_addr+NI52_MAGIC1) == NI52_MAGICVAL1) &&
        (inb(base_addr+NI52_MAGIC2) == NI52_MAGICVAL2))
      return ni52_probe1(dev, base_addr);
  else if (base_addr > 0)		/* Don't probe at all. */
    return ENXIO;

  for (port = ports; *port; port++) {
    int ioaddr = *port;
    if (check_region(ioaddr, NI52_TOTAL_SIZE))
      continue;
    if( !(inb(ioaddr+NI52_MAGIC1) == NI52_MAGICVAL1) || 
        !(inb(ioaddr+NI52_MAGIC2) == NI52_MAGICVAL2))
      continue;

    dev->base_addr = ioaddr;
    if (ni52_probe1(dev, ioaddr) == 0)
      return 0;
  }

  dev->base_addr = base_addr;
  return ENODEV;
}

static int ni52_probe1(struct device *dev,int ioaddr)
{
  long memaddrs[] = { 0xd0000,0xd2000,0xc8000,0xca000,0xd4000,0xd6000,0xd8000, 0 };
  int i,size;

  for(i=0;i<ETH_ALEN;i++)
    dev->dev_addr[i] = inb(dev->base_addr+i);

  if(dev->dev_addr[0] != NI52_ADDR0 || dev->dev_addr[1] != NI52_ADDR1
                                    || dev->dev_addr[2] != NI52_ADDR2)
    return ENODEV;

  printk("%s: Ni52 found at %#3lx, ",dev->name,dev->base_addr);

  request_region(ioaddr,NI52_TOTAL_SIZE,"ni52");

  dev->priv = (void *) kmalloc(sizeof(struct priv),GFP_KERNEL); 
                                  /* warning: we don't free it on errors */
  if (dev->priv == NULL)
     return -ENOMEM;
  memset((char *) dev->priv,0,sizeof(struct priv));

  /* 
   * check (or search) IO-Memory, 8K and 16K
   */
  if(dev->mem_start != 0) /* no auto-mem-probe */
  {
    size = 0x4000; /* check for 16K mem */
    if(!check586(dev,(char *) dev->mem_start,size)) {
      size = 0x2000; /* check for 8K mem */
      if(!check586(dev,(char *) dev->mem_start,size)) {
        printk("?memprobe, Can't find memory at 0x%lx!\n",dev->mem_start);
        return ENODEV;
      }
    }
  }
  else  
  {
    for(i=0;;i++)
    {
      if(!memaddrs[i]) {
        printk("?memprobe, Can't find io-memory!\n");
        return ENODEV;
      }
      dev->mem_start = memaddrs[i];
      size = 0x2000; /* check for 8K mem */
      if(check586(dev,(char *)dev->mem_start,size)) /* 8K-check */
        break;
      size = 0x4000; /* check for 16K mem */
      if(check586(dev,(char *)dev->mem_start,size)) /* 16K-check */
        break;
    }
  }
  dev->mem_end = dev->mem_start + size; /* set mem_end showed by 'ifconfig' */
  
  ((struct priv *) (dev->priv))->base =  dev->mem_start + size - 0x01000000;
  alloc586(dev);

  /* set number of receive-buffs according to memsize */
  if(size == 0x2000)
    ((struct priv *) dev->priv)->num_recv_buffs = NUM_RECV_BUFFS_8;
  else
    ((struct priv *) dev->priv)->num_recv_buffs = NUM_RECV_BUFFS_16;

  printk("Memaddr: 0x%lx, Memsize: %d, ",dev->mem_start,size);

  if(dev->irq < 2)
  {
    autoirq_setup(0);
    ni_reset586();
    ni_attn586();
    if(!(dev->irq = autoirq_report(2)))
    {
      printk("?autoirq, Failed to detect IRQ line!\n"); 
      return 1;
    }
  }
  else if(dev->irq == 2) 
    dev->irq = 9;

  printk("IRQ %d.\n",dev->irq);

  dev->open            = &ni52_open;
  dev->stop            = &ni52_close;
  dev->get_stats       = &ni52_get_stats;
  dev->hard_start_xmit = &ni52_send_packet;
  dev->set_multicast_list = &set_multicast_list;

  dev->if_port 	       = 0;

  ether_setup(dev);

  dev->tbusy = 0;
  dev->interrupt = 0;
  dev->start = 0;
  
  return 0;
}

/********************************************** 
 * init the chip (ni52-interrupt should be disabled?!)
 * needs a correct 'allocated' memory
 */

static int init586(struct device *dev)
{
  void *ptr;
  unsigned long s;
  int i,result=0;
  struct priv *p = (struct priv *) dev->priv;
  volatile struct configure_cmd_struct  *cfg_cmd;
  volatile struct iasetup_cmd_struct *ias_cmd;
  volatile struct tdr_cmd_struct *tdr_cmd;
  volatile struct mcsetup_cmd_struct *mc_cmd;
  struct dev_mc_list *dmi=dev->mc_list;
  int num_addrs=dev->mc_count;

  ptr = (void *) ((char *)p->scb + sizeof(struct scb_struct));

  cfg_cmd = (struct configure_cmd_struct *)ptr; /* configure-command */
  cfg_cmd->cmd_status = 0;
  cfg_cmd->cmd_cmd    = CMD_CONFIGURE | CMD_LAST;
  cfg_cmd->cmd_link   = 0xffff;

  cfg_cmd->byte_cnt   = 0x0a; /* number of cfg bytes */
  cfg_cmd->fifo       = 0x08; /* fifo-limit (8=tx:32/rx:64) */
  cfg_cmd->sav_bf     = 0x40; /* hold or discard bad recv frames (bit 7) */
  cfg_cmd->adr_len    = 0x2e; /* addr_len |!src_insert |pre-len |loopback */
  cfg_cmd->priority   = 0x00;
  cfg_cmd->ifs        = 0x60;
  cfg_cmd->time_low   = 0x00;
  cfg_cmd->time_high  = 0xf2;
  cfg_cmd->promisc    = 0;
  if(dev->flags&(IFF_ALLMULTI|IFF_PROMISC))
  {
	cfg_cmd->promisc=1;
	dev->flags|=IFF_PROMISC;
  }
  cfg_cmd->carr_coll  = 0x00;
 
  p->scb->cbl_offset = make16(cfg_cmd);

  p->scb->cmd = CUC_START; /* cmd.-unit start */
  ni_attn586();
 
  s = jiffies; /* warning: only active with interrupts on !! */
  while(!(cfg_cmd->cmd_status & STAT_COMPL)) 
    if(jiffies-s > 30) break;

  if((cfg_cmd->cmd_status & (STAT_OK|STAT_COMPL)) != (STAT_COMPL|STAT_OK))
  {
    printk("%s (ni52): configure command failed: %x\n",dev->name,cfg_cmd->cmd_status);
    return 1; 
  }

    /*
     * individual address setup
     */
  ias_cmd = (struct iasetup_cmd_struct *)ptr;

  ias_cmd->cmd_status = 0;
  ias_cmd->cmd_cmd    = CMD_IASETUP | CMD_LAST;
  ias_cmd->cmd_link   = 0xffff;

  memcpy((char *)&ias_cmd->iaddr,(char *) dev->dev_addr,ETH_ALEN);

  p->scb->cbl_offset = make16(ias_cmd);

  p->scb->cmd = CUC_START; /* cmd.-unit start */
  ni_attn586();

  s = jiffies;
  while(!(ias_cmd->cmd_status & STAT_COMPL)) 
    if(jiffies-s > 30) break;

  if((ias_cmd->cmd_status & (STAT_OK|STAT_COMPL)) != (STAT_OK|STAT_COMPL)) {
    printk("%s (ni52): individual address setup command failed: %04x\n",dev->name,ias_cmd->cmd_status);
    return 1; 
  }

   /* 
    * TDR, wire check .. e.g. no resistor e.t.c 
    */
  tdr_cmd = (struct tdr_cmd_struct *)ptr;

  tdr_cmd->cmd_status  = 0;
  tdr_cmd->cmd_cmd     = CMD_TDR | CMD_LAST;
  tdr_cmd->cmd_link    = 0xffff;
  tdr_cmd->status      = 0;

  p->scb->cbl_offset = make16(tdr_cmd);

  p->scb->cmd = CUC_START; /* cmd.-unit start */
  ni_attn586();

  s = jiffies; 
  while(!(tdr_cmd->cmd_status & STAT_COMPL))
    if(jiffies - s > 30) {
      printk("%s: Problems while running the TDR.\n",dev->name);
      result = 1;
    }

  if(!result)
  {
    DELAY(2); /* wait for result */
    result = tdr_cmd->status;

    p->scb->cmd = p->scb->status & STAT_MASK;
    ni_attn586(); /* ack the interrupts */

    if(result & TDR_LNK_OK) ;
    else if(result & TDR_XCVR_PRB)
      printk("%s: TDR: Transceiver problem!\n",dev->name);
    else if(result & TDR_ET_OPN)
      printk("%s: TDR: No correct termination %d clocks away.\n",dev->name,result & TDR_TIMEMASK);
    else if(result & TDR_ET_SRT) 
    {
      if (result & TDR_TIMEMASK) /* time == 0 -> strange :-) */
        printk("%s: TDR: Detected a short circuit %d clocks away.\n",dev->name,result & TDR_TIMEMASK);
    }
    else
      printk("%s: TDR: Unknown status %04x\n",dev->name,result);
  }
 
   /* 
    * ack interrupts 
    */
  p->scb->cmd = p->scb->status & STAT_MASK;
  ni_attn586();

   /*
    * alloc nop/xmit-cmds
    */
#if (NUM_XMIT_BUFFS == 1)
  for(i=0;i<2;i++)
  {
    p->nop_cmds[i] = (struct nop_cmd_struct *)ptr;
    p->nop_cmds[i]->cmd_cmd    = CMD_NOP;
    p->nop_cmds[i]->cmd_status = 0;
    p->nop_cmds[i]->cmd_link   = make16((p->nop_cmds[i]));
    ptr = (char *) ptr + sizeof(struct nop_cmd_struct);
  }
  p->xmit_cmds[0] = (struct transmit_cmd_struct *)ptr; /* transmit cmd/buff 0 */
  ptr = (char *) ptr + sizeof(struct transmit_cmd_struct);
#else
  for(i=0;i<NUM_XMIT_BUFFS;i++)
  {
    p->nop_cmds[i] = (struct nop_cmd_struct *)ptr;
    p->nop_cmds[i]->cmd_cmd    = CMD_NOP;
    p->nop_cmds[i]->cmd_status = 0;
    p->nop_cmds[i]->cmd_link   = make16((p->nop_cmds[i]));
    ptr = (char *) ptr + sizeof(struct nop_cmd_struct);
    p->xmit_cmds[i] = (struct transmit_cmd_struct *)ptr; /*transmit cmd/buff 0*/
    ptr = (char *) ptr + sizeof(struct transmit_cmd_struct);
  }
#endif

  ptr = alloc_rfa(dev,(void *)ptr); /* init receive-frame-area */ 

  /* 
   * Multicast setup
   */
  
  if(dev->mc_count)
  { /* I don't understand this: do we really need memory after the init? */
    int len = ((char *) p->iscp - (char *) ptr - 8) / 6;
    if(len <= 0)
    {
      printk("%s: Ooooops, no memory for MC-Setup!\n",dev->name);
    }
    else
    {
      if(len < num_addrs)
      {
      	/* BUG - should go ALLMULTI in this case */
        num_addrs = len;
        printk("%s: Sorry, can only apply %d MC-Address(es).\n",dev->name,num_addrs);
      }
      mc_cmd = (struct mcsetup_cmd_struct *) ptr;
      mc_cmd->cmd_status = 0;
      mc_cmd->cmd_cmd = CMD_MCSETUP | CMD_LAST;
      mc_cmd->cmd_link = 0xffff;
      mc_cmd->mc_cnt = num_addrs * 6;
      for(i=0;i<num_addrs;i++)
      {
		memcpy((char *) mc_cmd->mc_list[i], dmi->dmi_addr,6);
		dmi=dmi->next;
      }
      p->scb->cbl_offset = make16(mc_cmd);
      p->scb->cmd = CUC_START;
      ni_attn586();
      s = jiffies;
      while(!(mc_cmd->cmd_status & STAT_COMPL))
        if(jiffies - s > 30)
          break;
      if(!(mc_cmd->cmd_status & STAT_COMPL))
        printk("%s: Can't apply multicast-address-list.\n",dev->name);
    }
  }

  /*
   * alloc xmit-buffs / init xmit_cmds
   */
  for(i=0;i<NUM_XMIT_BUFFS;i++)
  {
    p->xmit_cbuffs[i] = (char *)ptr; /* char-buffs */
    ptr = (char *) ptr + XMIT_BUFF_SIZE;
    p->xmit_buffs[i] = (struct tbd_struct *)ptr; /* TBD */
    ptr = (char *) ptr + sizeof(struct tbd_struct);
    if((void *)ptr > (void *)p->iscp) 
    {
      printk("%s: not enough shared-mem for your configuration!\n",dev->name);
      return 1;
    }   
    memset((char *)(p->xmit_cmds[i]) ,0, sizeof(struct transmit_cmd_struct));
    memset((char *)(p->xmit_buffs[i]),0, sizeof(struct tbd_struct));
    p->xmit_cmds[i]->cmd_status = STAT_COMPL;
    p->xmit_cmds[i]->cmd_cmd = CMD_XMIT | CMD_INT;
    p->xmit_cmds[i]->tbd_offset = make16((p->xmit_buffs[i]));
    p->xmit_buffs[i]->next = 0xffff;
    p->xmit_buffs[i]->buffer = make24((p->xmit_cbuffs[i]));
  }

  p->xmit_count = 0; 
  p->xmit_last  = 0;
#ifndef NO_NOPCOMMANDS
  p->nop_point  = 0;
#endif

   /*
    * 'start transmitter' (nop-loop)
    */
#ifndef NO_NOPCOMMANDS
  p->scb->cbl_offset = make16(p->nop_cmds[0]);
  p->scb->cmd = CUC_START;
  ni_attn586();
  WAIT_4_SCB_CMD();
#else
  p->xmit_cmds[0]->cmd_link = 0xffff;
  p->xmit_cmds[0]->cmd_cmd  = CMD_XMIT | CMD_LAST | CMD_INT;
#endif

  return 0;
}

/******************************************************
 * This is a helper routine for ni52_rnr_int() and init586(). 
 * It sets up the Receive Frame Area (RFA).
 */

static void *alloc_rfa(struct device *dev,void *ptr) 
{
  volatile struct rfd_struct *rfd = (struct rfd_struct *)ptr;
  volatile struct rbd_struct *rbd;
  int i;
  struct priv *p = (struct priv *) dev->priv;

  memset((char *) rfd,0,sizeof(struct rfd_struct)*p->num_recv_buffs);
  p->rfd_first = rfd;

  for(i = 0; i < p->num_recv_buffs; i++)
    rfd[i].next = make16(rfd + (i+1) % p->num_recv_buffs);
  rfd[p->num_recv_buffs-1].last = RFD_SUSP;   /* RU suspend */

  ptr = (void *) (rfd + p->num_recv_buffs);

  rbd = (struct rbd_struct *) ptr;
  ptr = (void *) (rbd + p->num_recv_buffs);

   /* clr descriptors */
  memset((char *) rbd,0,sizeof(struct rbd_struct)*p->num_recv_buffs);

  for(i=0;i<p->num_recv_buffs;i++)
  {
    rbd[i].next = make16((rbd + (i+1) % p->num_recv_buffs));
    rbd[i].size = RECV_BUFF_SIZE;
    rbd[i].buffer = make24(ptr);
    ptr = (char *) ptr + RECV_BUFF_SIZE;
  }

  p->rfd_top  = p->rfd_first;
  p->rfd_last = p->rfd_first + p->num_recv_buffs - 1;

  p->scb->rfa_offset		= make16(p->rfd_first);
  p->rfd_first->rbd_offset	= make16(rbd);

  return ptr;
}


/**************************************************
 * Interrupt Handler ...
 */

static void ni52_interrupt(int irq,struct pt_regs *reg_ptr)
{
  struct device *dev = (struct device *) irq2dev_map[irq];
  unsigned short stat;
  struct priv *p;

  if (dev == NULL) {
    printk ("ni52-interrupt: irq %d for unknown device.\n",(int) -(((struct pt_regs *)reg_ptr)->orig_eax+2));
    return;
  }
  p = (struct priv *) dev->priv;

  dev->interrupt = 1;

  while((stat=p->scb->status & STAT_MASK))
  {
    p->scb->cmd = stat;
    ni_attn586(); /* ack inter. */

   if(stat & STAT_CX)    /* command with I-bit set complete */
      ni52_xmt_int(dev);

    if(stat & STAT_FR)   /* received a frame */
      ni52_rcv_int(dev);

#ifndef NO_NOPCOMMANDS
    if(stat & STAT_CNA)  /* CU went 'not ready' */
    {
      if(dev->start)
        printk("%s: oops! CU has left active state. stat: %04x/%04x.\n",dev->name,(int) stat,(int) p->scb->status);
    }
#endif

    if(stat & STAT_RNR) /* RU went 'not ready' */
    {
      if(p->scb->status & RU_SUSPEND) /* special case: RU_SUSPEND */
      {
        WAIT_4_SCB_CMD();
        p->scb->cmd = RUC_RESUME;
        ni_attn586();
      }
      else
      {
        printk("%s: Receiver-Unit went 'NOT READY': %04x/%04x.\n",dev->name,(int) stat,(int) p->scb->status);
        ni52_rnr_int(dev); 
      }
    }
    WAIT_4_SCB_CMD(); /* wait for ack. (ni52_xmt_int can be faster than ack!!) */
    if(p->scb->cmd)   /* timed out? */
      break;
  }

  dev->interrupt = 0;
}

/*******************************************************
 * receive-interrupt
 */

static void ni52_rcv_int(struct device *dev)
{
  int status;
  unsigned short totlen;
  struct sk_buff *skb;
  struct rbd_struct *rbd;
  struct priv *p = (struct priv *) dev->priv;

  for(;(status = p->rfd_top->status) & STAT_COMPL;)
  {
      rbd = (struct rbd_struct *) make32(p->rfd_top->rbd_offset);

      if(status & STAT_OK) /* frame received without error? */
      {
        if( (totlen = rbd->status) & RBD_LAST) /* the first and the last buffer? */
        {
          totlen &= RBD_MASK; /* length of this frame */
          rbd->status = 0;
          skb = (struct sk_buff *) dev_alloc_skb(totlen+2);
          if(skb != NULL)
          {
            skb->dev = dev;
            skb_reserve(skb,2);		/* 16 byte alignment */
            memcpy(skb_put(skb,totlen),(char *) p->base+(unsigned long) rbd->buffer, totlen);
            skb->protocol=eth_type_trans(skb,dev);
            netif_rx(skb);
            p->stats.rx_packets++;
          }
          else
            p->stats.rx_dropped++;
        }
        else
        {
          printk("%s: received oversized frame.\n",dev->name);
          p->stats.rx_dropped++;
        }
      }
      else /* frame !(ok), only with 'save-bad-frames' */
      {
        printk("%s: oops! rfd-error-status: %04x\n",dev->name,status);
        p->stats.rx_errors++;
      }
      p->rfd_top->status = 0;
      p->rfd_top->last = RFD_SUSP;
      p->rfd_last->last = 0;        /* delete RU_SUSP  */
      p->rfd_last = p->rfd_top;
      p->rfd_top = (struct rfd_struct *) make32(p->rfd_top->next); /* step to next RFD */
  }
}

/**********************************************************
 * handle 'Receiver went not ready'. 
 */

static void ni52_rnr_int(struct device *dev)
{
  struct priv *p = (struct priv *) dev->priv;

  p->stats.rx_errors++;

  WAIT_4_SCB_CMD();    /* wait for the last cmd */
  p->scb->cmd = RUC_ABORT; /* usually the RU is in the 'no resource'-state .. abort it now. */
  ni_attn586(); 
  WAIT_4_SCB_CMD();    /* wait for accept cmd. */

  alloc_rfa(dev,(char *)p->rfd_first);
  startrecv586(dev); /* restart RU */

  printk("%s: Receive-Unit restarted. Status: %04x\n",dev->name,p->scb->status);

}

/**********************************************************
 * handle xmit - interrupt
 */

static void ni52_xmt_int(struct device *dev)
{
  int status;
  struct priv *p = (struct priv *) dev->priv;

  status = p->xmit_cmds[p->xmit_last]->cmd_status;
  if(!(status & STAT_COMPL))
    printk("%s: strange .. xmit-int without a 'COMPLETE'\n",dev->name);

  if(status & STAT_OK)
  {
    p->stats.tx_packets++;
    p->stats.collisions += (status & TCMD_MAXCOLLMASK);
  }
  else 
  {
    p->stats.tx_errors++;
    if(status & TCMD_LATECOLL) {
      printk("%s: late collision detected.\n",dev->name);
      p->stats.collisions++;
    } 
    else if(status & TCMD_NOCARRIER) {
      p->stats.tx_carrier_errors++;
      printk("%s: no carrier detected.\n",dev->name);
    } 
    else if(status & TCMD_LOSTCTS) 
      printk("%s: loss of CTS detected.\n",dev->name);
    else if(status & TCMD_UNDERRUN) {
      p->stats.tx_fifo_errors++;
      printk("%s: DMA underrun detected.\n",dev->name);
    }
    else if(status & TCMD_MAXCOLL) {
      printk("%s: Max. collisions exceeded.\n",dev->name);
      p->stats.collisions += 16;
    } 
  }

#if (NUM_XMIT_BUFFS != 1)
  if( (++p->xmit_last) == NUM_XMIT_BUFFS) 
    p->xmit_last = 0;
#endif

  dev->tbusy = 0;
  mark_bh(NET_BH);
}

/***********************************************************
 * (re)start the receiver
 */ 

static void startrecv586(struct device *dev)
{
  struct priv *p = (struct priv *) dev->priv;

  p->scb->rfa_offset = make16(p->rfd_first);
  p->scb->cmd = RUC_START;
  ni_attn586();		/* start cmd. */
  WAIT_4_SCB_CMD();	/* wait for accept cmd. (no timeout!!) */
}

/******************************************************
 * send frame 
 */

static int ni52_send_packet(struct sk_buff *skb, struct device *dev)
{
  int len,i;
#ifndef NO_NOPCOMMANDS
  int next_nop;
#endif
  struct priv *p = (struct priv *) dev->priv;

  if(dev->tbusy)
  {
    int tickssofar = jiffies - dev->trans_start;
    if (tickssofar < 5)
      return 1;

    if(p->scb->status & CU_ACTIVE) /* COMMAND-UNIT active? */
    {
      dev->tbusy = 0;
#ifdef DEBUG
      printk("%s: strange ... timeout with CU active?!?\n",dev->name);
      printk("%s: X0: %04x N0: %04x N1: %04x %d\n",dev->name,(int)p->xmit_cmds[0]->cmd_status,(int)p->nop_cmds[0]->cmd_status,(int)p->nop_cmds[1]->cmd_status,(int)p->nop_point);
#endif
      p->scb->cmd = CUC_ABORT;
      ni_attn586();
      WAIT_4_SCB_CMD();
      p->scb->cbl_offset = make16(p->nop_cmds[p->nop_point]);
      p->scb->cmd = CUC_START;
      ni_attn586();
      WAIT_4_SCB_CMD();
      dev->trans_start = jiffies;
      return 0;
    }
    else
    {
#ifdef DEBUG
      printk("%s: xmitter timed out, try to restart! stat: %04x\n",dev->name,p->scb->status);
      printk("%s: command-stats: %04x %04x\n",dev->name,p->xmit_cmds[0]->cmd_status,p->xmit_cmds[1]->cmd_status);
#endif
      ni52_close(dev);
      ni52_open(dev);
    }
    dev->trans_start = jiffies;
    return 0;
  }

  if(skb == NULL)
  {
    dev_tint(dev);
    return 0;
  }

  if (skb->len <= 0)
    return 0;
  if(skb->len > XMIT_BUFF_SIZE)
  {
    printk("%s: Sorry, max. framelength is %d bytes. The length of your frame is %ld bytes.\n",dev->name,XMIT_BUFF_SIZE,skb->len);
    return 0;
  }

  if (set_bit(0, (void*)&dev->tbusy) != 0)
     printk("%s: Transmitter access conflict.\n", dev->name);
  else
  {
    memcpy((char *)p->xmit_cbuffs[p->xmit_count],(char *)(skb->data),skb->len);
    len = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;

#if (NUM_XMIT_BUFFS == 1)
#  ifdef NO_NOPCOMMANDS
    p->xmit_buffs[0]->size = TBD_LAST | len;
    for(i=0;i<16;i++)
    {
      p->scb->cbl_offset = make16(p->xmit_cmds[0]);
      p->scb->cmd = CUC_START;
      p->xmit_cmds[0]->cmd_status = 0;

      ni_attn586();
      dev->trans_start = jiffies;
      if(!i)
        dev_kfree_skb(skb,FREE_WRITE);
      WAIT_4_SCB_CMD();
      if( (p->scb->status & CU_ACTIVE)) /* test it, because CU sometimes doesn't start immediately */
        break;
      if(p->xmit_cmds[0]->cmd_status)
        break;
      if(i==15)
        printk("%s: Can't start transmit-command.\n",dev->name);
    }
#  else
    next_nop = (p->nop_point + 1) & 0x1;
    p->xmit_buffs[0]->size = TBD_LAST | len;

    p->xmit_cmds[0]->cmd_link   = p->nop_cmds[next_nop]->cmd_link 
                                = make16((p->nop_cmds[next_nop]));
    p->xmit_cmds[0]->cmd_status = p->nop_cmds[next_nop]->cmd_status = 0;

    p->nop_cmds[p->nop_point]->cmd_link = make16((p->xmit_cmds[0]));
    dev->trans_start = jiffies;
    p->nop_point = next_nop;
    dev_kfree_skb(skb,FREE_WRITE);
#  endif
#else
    p->xmit_buffs[p->xmit_count]->size = TBD_LAST | len;
    if( (next_nop = p->xmit_count + 1) == NUM_XMIT_BUFFS ) 
      next_nop = 0;

    p->xmit_cmds[p->xmit_count]->cmd_status  = 0;
    p->xmit_cmds[p->xmit_count]->cmd_link = p->nop_cmds[next_nop]->cmd_link 
                                          = make16((p->nop_cmds[next_nop]));
    p->nop_cmds[next_nop]->cmd_status = 0;

    p->nop_cmds[p->xmit_count]->cmd_link = make16((p->xmit_cmds[p->xmit_count]));
    dev->trans_start = jiffies;
    p->xmit_count = next_nop;
  
    cli();
    if(p->xmit_count != p->xmit_last)
      dev->tbusy = 0;
    sti();
    dev_kfree_skb(skb,FREE_WRITE);
#endif
  }
  return 0;
}

/*******************************************
 * Someone wanna have the statistics 
 */

static struct enet_statistics *ni52_get_stats(struct device *dev)
{
  struct priv *p = (struct priv *) dev->priv;
  unsigned short crc,aln,rsc,ovrn;

  crc = p->scb->crc_errs; /* get error-statistic from the ni82586 */
  p->scb->crc_errs -= crc;
  aln = p->scb->aln_errs;
  p->scb->aln_errs -= aln;
  rsc = p->scb->rsc_errs;
  p->scb->rsc_errs -= rsc;
  ovrn = p->scb->ovrn_errs;
  p->scb->ovrn_errs -= ovrn;

  p->stats.rx_crc_errors += crc;
  p->stats.rx_fifo_errors += ovrn;
  p->stats.rx_frame_errors += aln;
  p->stats.rx_dropped += rsc;

  return &p->stats;
}

/********************************************************
 * Set MC list ..  
 */

static void set_multicast_list(struct device *dev)
{
  if(!dev->start)
  {
    printk("%s: Can't apply promiscuous/multicastmode to a not running interface.\n",dev->name);
    return;
  }

  dev->start = 0;
  alloc586(dev);
  init586(dev);  
  startrecv586(dev);
  dev->start = 1;
}

/*
 * END: linux/drivers/net/ni52.c 
 */
