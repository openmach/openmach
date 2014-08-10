/* apricot.c: An Apricot 82596 ethernet driver for linux. */
/*
    Apricot
    	Written 1994 by Mark Evans.
	This driver is for the Apricot 82596 bus-master interface

        Modularised 12/94 Mark Evans
    
    Driver skeleton 
	Written 1993 by Donald Becker.
	Copyright 1993 United States Government as represented by the Director,
	National Security Agency.  This software may only be used and distributed
	according to the terms of the GNU Public License as modified by SRC,
	incorporated herein by reference.

	The author may be reached as becker@super.org or
	C/O Supercomputing Research Ctr., 17100 Science Dr., Bowie MD 20715
    

*/

static const char *version = "apricot.c:v0.2 05/12/94\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#ifndef HAVE_PORTRESERVE
#define check_region(addr, size)	0
#define request_region(addr, size,name)	do ; while(0)
#endif

#ifndef HAVE_ALLOC_SKB
#define alloc_skb(size, priority) (struct sk_buff *) kmalloc(size,priority)
#define kfree_skbmem(buff, size) kfree_s(buff,size)
#endif

#define APRICOT_DEBUG 1

#ifdef APRICOT_DEBUG
int i596_debug = APRICOT_DEBUG;
#else
int i596_debug = 1;
#endif

#define APRICOT_TOTAL_SIZE 17

#define I596_NULL -1

#define CMD_EOL		0x8000	/* The last command of the list, stop. */
#define CMD_SUSP	0x4000	/* Suspend after doing cmd. */
#define CMD_INTR	0x2000	/* Interrupt after doing cmd. */

#define CMD_FLEX	0x0008	/* Enable flexible memory model */

enum commands {
	CmdNOp = 0, CmdSASetup = 1, CmdConfigure = 2, CmdMulticastList = 3,
	CmdTx = 4, CmdTDR = 5, CmdDump = 6, CmdDiagnose = 7};

#define STAT_C		0x8000	/* Set to 0 after execution */
#define STAT_B		0x4000	/* Command being executed */
#define STAT_OK		0x2000	/* Command executed ok */
#define STAT_A		0x1000	/* Command aborted */

#define	 CUC_START	0x0100
#define	 CUC_RESUME	0x0200
#define	 CUC_SUSPEND    0x0300
#define	 CUC_ABORT	0x0400
#define	 RX_START	0x0010
#define	 RX_RESUME	0x0020
#define	 RX_SUSPEND	0x0030
#define	 RX_ABORT	0x0040

struct i596_cmd {
    unsigned short status;
    unsigned short command;
    struct i596_cmd *next;
};

#define EOF		0x8000
#define SIZE_MASK	0x3fff

struct i596_tbd {
    unsigned short size;
    unsigned short pad;
    struct i596_tbd *next;
    char *data;
};

struct tx_cmd {
    struct i596_cmd cmd;
    struct i596_tbd *tbd;
    unsigned short size;
    unsigned short pad;
};

struct i596_rfd {
    unsigned short stat;
    unsigned short cmd;
    struct i596_rfd *next;
    long rbd; 
    unsigned short count;
    unsigned short size;
    char data[1532];
};

#define RX_RING_SIZE 8

struct i596_scb {
    unsigned short status;
    unsigned short command;
    struct i596_cmd *cmd;
    struct i596_rfd *rfd;
    unsigned long crc_err;
    unsigned long align_err;
    unsigned long resource_err;
    unsigned long over_err;
    unsigned long rcvdt_err;
    unsigned long short_err;
    unsigned short t_on;
    unsigned short t_off;
};

struct i596_iscp {
    unsigned long stat;
    struct i596_scb *scb;
};

struct i596_scp {
    unsigned long sysbus;
    unsigned long pad;
    struct i596_iscp *iscp;
};

struct i596_private {
    struct i596_scp scp;
    struct i596_iscp iscp;
    struct i596_scb scb;
    struct i596_cmd set_add;
    char eth_addr[8];
    struct i596_cmd set_conf;
    char i596_config[16];
    struct i596_cmd tdr;
    unsigned long stat;
    int last_restart;
    struct i596_rfd *rx_tail;
    struct i596_cmd *cmd_tail;
    struct i596_cmd *cmd_head;
    int cmd_backlog;
    unsigned long last_cmd;
    struct enet_statistics stats;
};

char init_setup[] = {
	0x8E,	/* length, prefetch on */
	0xC8,	/* fifo to 8, monitor off */
	0x80,	/* don't save bad frames */
	0x2E,	/* No source address insertion, 8 byte preamble */
	0x00,	/* priority and backoff defaults */
	0x60,	/* interframe spacing */
	0x00,	/* slot time LSB */
	0xf2,	/* slot time and retries */
	0x00,	/* promiscuous mode */
	0x00,	/* collision detect */
	0x40,	/* minimum frame length */
	0xff,	
	0x00,
	0x7f	/*  *multi IA */ };

static int i596_open(struct device *dev);
static int i596_start_xmit(struct sk_buff *skb, struct device *dev);
static void i596_interrupt(int irq, struct pt_regs *regs);
static int i596_close(struct device *dev);
static struct enet_statistics *i596_get_stats(struct device *dev);
static void i596_add_cmd(struct device *dev, struct i596_cmd *cmd);
static void print_eth(char *);
static void set_multicast_list(struct device *dev);


static inline int
init_rx_bufs(struct device *dev, int num)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int i;
    struct i596_rfd *rfd;

    lp->scb.rfd = (struct i596_rfd *)I596_NULL;

    if (i596_debug > 1) printk ("%s: init_rx_bufs %d.\n", dev->name, num);

    for (i = 0; i < num; i++)
    {
	if (!(rfd = (struct i596_rfd *)kmalloc(sizeof(struct i596_rfd), GFP_KERNEL)))
            break;

	rfd->stat = 0x0000;
	rfd->rbd = I596_NULL;
	rfd->count = 0;
	rfd->size = 1532;
        if (i == 0)
        {
	    rfd->cmd = CMD_EOL;
            lp->rx_tail = rfd;
        }
        else
	    rfd->cmd = 0x0000;

        rfd->next = lp->scb.rfd;
        lp->scb.rfd = rfd;
    }

    if (i != 0)
      lp->rx_tail->next = lp->scb.rfd;

    return (i);
}

static inline void
remove_rx_bufs(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    struct i596_rfd *rfd = lp->scb.rfd;

    lp->rx_tail->next = (struct i596_rfd *)I596_NULL;

    do
    {
        lp->scb.rfd = rfd->next;
        kfree_s(rfd, sizeof(struct i596_rfd));
        rfd = lp->scb.rfd;
    }
    while (rfd != lp->rx_tail);
}

static inline void
init_i596_mem(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    short ioaddr = dev->base_addr;
    int boguscnt = 100;

    /* change the scp address */
    outw(0, ioaddr);
    outw(0, ioaddr);
    outb(4, ioaddr+0xf);
    outw(((((int)&lp->scp) & 0xffff) | 2), ioaddr);
    outw((((int)&lp->scp)>>16) & 0xffff, ioaddr);

    lp->last_cmd = jiffies;

    lp->scp.sysbus = 0x00440000;
    lp->scp.iscp = &(lp->iscp);
    lp->iscp.scb = &(lp->scb);
    lp->iscp.stat = 0x0001;
    lp->cmd_backlog = 0;

    lp->cmd_head = lp->scb.cmd = (struct i596_cmd *) I596_NULL;

    if (i596_debug > 2) printk("%s: starting i82596.\n", dev->name);

    (void) inb (ioaddr+0x10);
    outb(4, ioaddr+0xf);
    outw(0, ioaddr+4);

    while (lp->iscp.stat)
	if (--boguscnt == 0)
	{
	    printk("%s: i82596 initialization timed out with status %4.4x, cmd %4.4x.\n",
		   dev->name, lp->scb.status, lp->scb.command);
	    break;
    	}

    lp->scb.command = 0;

    memcpy (lp->i596_config, init_setup, 14);
    lp->set_conf.command = CmdConfigure;
    i596_add_cmd(dev, &lp->set_conf);

    memcpy (lp->eth_addr, dev->dev_addr, 6);
    lp->set_add.command = CmdSASetup;
    i596_add_cmd(dev, &lp->set_add);

    lp->tdr.command = CmdTDR;
    i596_add_cmd(dev, &lp->tdr);

    boguscnt = 200;
    while (lp->scb.status, lp->scb.command)
	if (--boguscnt == 0)
	{
	    printk("%s: receive unit start timed out with status %4.4x, cmd %4.4x.\n",
		   dev->name, lp->scb.status, lp->scb.command);
	    break;
    	}

    lp->scb.command = RX_START;
    outw(0, ioaddr+4);

    boguscnt = 200;
    while (lp->scb.status, lp->scb.command)
        if (--boguscnt == 0)
	{
	    printk("i82596 init timed out with status %4.4x, cmd %4.4x.\n",
		lp->scb.status, lp->scb.command);
	    break;
	}

    return;
}

static inline int
i596_rx(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int frames = 0;

    if (i596_debug > 3) printk ("i596_rx()\n");

    while ((lp->scb.rfd->stat) & STAT_C)
    {
        if (i596_debug >2) print_eth(lp->scb.rfd->data);

	if ((lp->scb.rfd->stat) & STAT_OK)
	{
	    /* a good frame */
	    int pkt_len = lp->scb.rfd->count & 0x3fff;
	    struct sk_buff *skb = dev_alloc_skb(pkt_len);

	    frames++;

	    if (skb == NULL)
	    {
		printk ("%s: i596_rx Memory squeeze, dropping packet.\n", dev->name);
		lp->stats.rx_dropped++;
		break;
	    }

  	    skb->dev = dev;		
	    memcpy(skb_put(skb,pkt_len), lp->scb.rfd->data, pkt_len);

	    skb->protocol=eth_type_trans(skb,dev);
	    netif_rx(skb);
	    lp->stats.rx_packets++;

	    if (i596_debug > 4) print_eth(skb->data);
	}
	else
	{
	    lp->stats.rx_errors++;
	    if ((lp->scb.rfd->stat) & 0x0001) lp->stats.collisions++;
	    if ((lp->scb.rfd->stat) & 0x0080) lp->stats.rx_length_errors++;
	    if ((lp->scb.rfd->stat) & 0x0100) lp->stats.rx_over_errors++;
	    if ((lp->scb.rfd->stat) & 0x0200) lp->stats.rx_fifo_errors++;
	    if ((lp->scb.rfd->stat) & 0x0400) lp->stats.rx_frame_errors++;
	    if ((lp->scb.rfd->stat) & 0x0800) lp->stats.rx_crc_errors++;
	    if ((lp->scb.rfd->stat) & 0x1000) lp->stats.rx_length_errors++;
	}

	lp->scb.rfd->stat = 0;
	lp->rx_tail->cmd = 0;
	lp->rx_tail = lp->scb.rfd;
	lp->scb.rfd = lp->scb.rfd->next;
	lp->rx_tail->count = 0;
	lp->rx_tail->cmd = CMD_EOL;

    }

    if (i596_debug > 3) printk ("frames %d\n", frames);

    return 0;
}

static inline void
i596_cleanup_cmd(struct i596_private *lp)
{
    struct i596_cmd *ptr;
    int boguscnt = 100;

    if (i596_debug > 4) printk ("i596_cleanup_cmd\n");

    while (lp->cmd_head != (struct i596_cmd *) I596_NULL)
    {
	ptr = lp->cmd_head;

	lp->cmd_head = lp->cmd_head->next;
	lp->cmd_backlog--;

	switch ((ptr->command) & 0x7)
	{
	    case CmdTx:
	    {
		struct tx_cmd *tx_cmd = (struct tx_cmd *) ptr;
		struct sk_buff *skb = ((struct sk_buff *)(tx_cmd->tbd->data)) -1;

		dev_kfree_skb(skb, FREE_WRITE);

		lp->stats.tx_errors++;
		lp->stats.tx_aborted_errors++;

		ptr->next = (struct i596_cmd * ) I596_NULL;
		kfree_s((unsigned char *)tx_cmd, (sizeof (struct tx_cmd) + sizeof (struct i596_tbd)));
		break;
	    }
	    case CmdMulticastList:
	    {
		unsigned short count = *((unsigned short *) (ptr + 1));

		ptr->next = (struct i596_cmd * ) I596_NULL;
		kfree_s((unsigned char *)ptr, (sizeof (struct i596_cmd) + count + 2));
		break;
	    }
	    default:
		ptr->next = (struct i596_cmd * ) I596_NULL;
	}
    }

    while (lp->scb.status, lp->scb.command)
	if (--boguscnt == 0)
	{
	    printk("i596_cleanup_cmd timed out with status %4.4x, cmd %4.4x.\n",
		lp->scb.status, lp->scb.command);
	    break;
    	}

    lp->scb.cmd = lp->cmd_head;
}

static inline void
i596_reset(struct device *dev, struct i596_private *lp, int ioaddr)
{
    int boguscnt = 100;

    if (i596_debug > 4) printk ("i596_reset\n");

    while (lp->scb.status, lp->scb.command)
        if (--boguscnt == 0)
	{
	    printk("i596_reset timed out with status %4.4x, cmd %4.4x.\n",
		lp->scb.status, lp->scb.command);
	    break;
	}

    dev->start = 0;
    dev->tbusy = 1;

    lp->scb.command = CUC_ABORT|RX_ABORT;
    outw(0, ioaddr+4);

    /* wait for shutdown */
    boguscnt = 400;

    while ((lp->scb.status, lp->scb.command) || lp->scb.command)
        if (--boguscnt == 0)
	{
	    printk("i596_reset 2 timed out with status %4.4x, cmd %4.4x.\n",
		lp->scb.status, lp->scb.command);
	    break;
	}

    i596_cleanup_cmd(lp);
    i596_rx(dev);

    dev->start = 1;
    dev->tbusy = 0;
    dev->interrupt = 0;
    init_i596_mem(dev);
}

static void i596_add_cmd(struct device *dev, struct i596_cmd *cmd)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int ioaddr = dev->base_addr;
    unsigned long flags;
    int boguscnt = 100;

    if (i596_debug > 4) printk ("i596_add_cmd\n");

    cmd->status = 0;
    cmd->command |= (CMD_EOL|CMD_INTR);
    cmd->next = (struct i596_cmd *) I596_NULL;

    save_flags(flags);
    cli();
    if (lp->cmd_head != (struct i596_cmd *) I596_NULL)
	lp->cmd_tail->next = cmd;
    else 
    {
	lp->cmd_head = cmd;
	while (lp->scb.status, lp->scb.command)
	    if (--boguscnt == 0)
	    {
		printk("i596_add_cmd timed out with status %4.4x, cmd %4.4x.\n",
		   lp->scb.status, lp->scb.command);
		break;
    	    }

	lp->scb.cmd = cmd;
	lp->scb.command = CUC_START;
        outw (0, ioaddr+4);
    }
    lp->cmd_tail = cmd;
    lp->cmd_backlog++;

    lp->cmd_head = lp->scb.cmd;
    restore_flags(flags);

    if (lp->cmd_backlog > 16) 
    {
	int tickssofar = jiffies - lp->last_cmd;

	if (tickssofar < 25) return;

	printk("%s: command unit timed out, status resetting.\n", dev->name);

	i596_reset(dev, lp, ioaddr);
    }
}

static int
i596_open(struct device *dev)
{
    int i;

    if (i596_debug > 1)
	printk("%s: i596_open() irq %d.\n", dev->name, dev->irq);

    if (request_irq(dev->irq, &i596_interrupt, 0, "apricot"))
	return -EAGAIN;

    irq2dev_map[dev->irq] = dev;

    i = init_rx_bufs(dev, RX_RING_SIZE);

    if ((i = init_rx_bufs(dev, RX_RING_SIZE)) < RX_RING_SIZE)
        printk("%s: only able to allocate %d receive buffers\n", dev->name, i);

    if (i < 4)
    {
        free_irq(dev->irq);
        irq2dev_map[dev->irq] = 0;
        return -EAGAIN;
    }

    dev->tbusy = 0;
    dev->interrupt = 0;
    dev->start = 1;
    MOD_INC_USE_COUNT;

    /* Initialize the 82596 memory */
    init_i596_mem(dev);

    return 0;			/* Always succeed */
}

static int
i596_start_xmit(struct sk_buff *skb, struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int ioaddr = dev->base_addr;
    struct tx_cmd *tx_cmd;

    if (i596_debug > 2) printk ("%s: Apricot start xmit\n", dev->name);

    /* Transmitter timeout, serious problems. */
    if (dev->tbusy) {
	int tickssofar = jiffies - dev->trans_start;
	if (tickssofar < 5)
	    return 1;
	printk("%s: transmit timed out, status resetting.\n",
	       dev->name);
	lp->stats.tx_errors++;
	/* Try to restart the adaptor */
	if (lp->last_restart == lp->stats.tx_packets) {
	    if (i596_debug > 1) printk ("Resetting board.\n");

	    /* Shutdown and restart */
            i596_reset(dev,lp, ioaddr);
	} else {
	    /* Issue a channel attention signal */
	    if (i596_debug > 1) printk ("Kicking board.\n");

	    lp->scb.command = CUC_START|RX_START;
	    outw(0, ioaddr+4);

	    lp->last_restart = lp->stats.tx_packets;
	}
	dev->tbusy = 0;
	dev->trans_start = jiffies;
    }

    /* If some higher level thinks we've misses a tx-done interrupt
       we are passed NULL. n.b. dev_tint handles the cli()/sti()
       itself. */
    if (skb == NULL) {
	dev_tint(dev);
	return 0;
    }

    /* shouldn't happen */
    if (skb->len <= 0) return 0;

    if (i596_debug > 3) printk("%s: i596_start_xmit() called\n", dev->name);

    /* Block a timer-based transmit from overlapping.  This could better be
       done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
    if (set_bit(0, (void*)&dev->tbusy) != 0)
	printk("%s: Transmitter access conflict.\n", dev->name);
    else
    {
	short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	dev->trans_start = jiffies;

	tx_cmd = (struct tx_cmd *) kmalloc ((sizeof (struct tx_cmd) + sizeof (struct i596_tbd)), GFP_ATOMIC);
	if (tx_cmd == NULL)
	{
	    printk ("%s: i596_xmit Memory squeeze, dropping packet.\n", dev->name);
	    lp->stats.tx_dropped++;

	    dev_kfree_skb(skb, FREE_WRITE);
	}
	else
	{
	    tx_cmd->tbd = (struct i596_tbd *) (tx_cmd + 1);
	    tx_cmd->tbd->next = (struct i596_tbd *) I596_NULL;

	    tx_cmd->cmd.command = CMD_FLEX|CmdTx;

	    tx_cmd->pad = 0;
	    tx_cmd->size = 0;
	    tx_cmd->tbd->pad = 0;
	    tx_cmd->tbd->size = EOF | length;

	    tx_cmd->tbd->data = skb->data;

	    if (i596_debug > 3) print_eth(skb->data);

	    i596_add_cmd(dev, (struct i596_cmd *)tx_cmd);

	    lp->stats.tx_packets++;
	}
    }

    dev->tbusy = 0;

    return 0;
}


static void print_eth(char *add)
{
    int i;

    printk ("Dest  ");
    for (i = 0; i < 6; i++)
	printk(" %2.2X", (unsigned char)add[i]);
    printk ("\n");

    printk ("Source");
    for (i = 0; i < 6; i++)
	printk(" %2.2X", (unsigned char)add[i+6]);
    printk ("\n");
    printk ("type %2.2X%2.2X\n", (unsigned char)add[12], (unsigned char)add[13]);
}

int apricot_probe(struct device *dev)
{
    int i;
    struct i596_private *lp;
    int checksum = 0;
    int ioaddr = 0x300;
    char eth_addr[6];
    
    /* this is easy the ethernet interface can only be at 0x300 */
    /* first check nothing is already registered here */

    if (check_region(ioaddr, APRICOT_TOTAL_SIZE))
	return ENODEV;

    for (i = 0; i < 8; i++)
    {
    	eth_addr[i] = inb(ioaddr+8+i);
	checksum += eth_addr[i];
     }

    /* checksum is a multiple of 0x100, got this wrong first time
       some machines have 0x100, some 0x200. The DOS driver doesn't
       even bother with the checksum */

    if (checksum % 0x100) return ENODEV;

    /* Some other boards trip the checksum.. but then appear as ether
       address 0. Trap these - AC */
       
    if(memcmp(eth_addr,"\x00\x00\x49",3)!= 0)
    	return ENODEV;

    request_region(ioaddr, APRICOT_TOTAL_SIZE, "apricot");

    dev->base_addr = ioaddr;
    ether_setup(dev);
    printk("%s: Apricot 82596 at %#3x,", dev->name, ioaddr);

    for (i = 0; i < 6; i++)
	printk(" %2.2X", dev->dev_addr[i] = eth_addr[i]);

    dev->base_addr = ioaddr;
    dev->irq = 10;
    printk(" IRQ %d.\n", dev->irq);

    if (i596_debug > 0) printk(version);

    /* The APRICOT-specific entries in the device structure. */
    dev->open = &i596_open;
    dev->stop = &i596_close;
    dev->hard_start_xmit = &i596_start_xmit;
    dev->get_stats = &i596_get_stats;
    dev->set_multicast_list = &set_multicast_list;

    dev->mem_start = (int)kmalloc(sizeof(struct i596_private)+ 0x0f, GFP_KERNEL);
    /* align for scp */
    dev->priv = (void *)((dev->mem_start + 0xf) & 0xfffffff0);

    lp = (struct i596_private *)dev->priv;
    memset((void *)lp, 0, sizeof(struct i596_private));
    lp->scb.command = 0;
    lp->scb.cmd = (struct i596_cmd *) I596_NULL;
    lp->scb.rfd = (struct i596_rfd *)I596_NULL;

    return 0;
}

static void
i596_interrupt(int irq, struct pt_regs *regs)
{
    struct device *dev = (struct device *)(irq2dev_map[irq]);
    struct i596_private *lp;
    short ioaddr;
    int boguscnt = 200;
    unsigned short status, ack_cmd = 0;

    if (dev == NULL) {
	printk ("i596_interrupt(): irq %d for unknown device.\n", irq);
	return;
    }

    if (i596_debug > 3) printk ("%s: i596_interrupt(): irq %d\n",dev->name, irq);

    if (dev->interrupt)
	printk("%s: Re-entering the interrupt handler.\n", dev->name);

    dev->interrupt = 1;

    ioaddr = dev->base_addr;

    lp = (struct i596_private *)dev->priv;

    while (lp->scb.status, lp->scb.command)
	if (--boguscnt == 0)
	    {
		printk("%s: i596 interrupt, timeout status %4.4x command %4.4x.\n", dev->name, lp->scb.status, lp->scb.command);
		break;
	    }
    status = lp->scb.status;

    if (i596_debug > 4)
	printk("%s: i596 interrupt, status %4.4x.\n", dev->name, status);

    ack_cmd = status & 0xf000;

    if ((status & 0x8000) || (status & 0x2000))
    {
	struct i596_cmd *ptr;

	if ((i596_debug > 4) && (status & 0x8000))
	    printk("%s: i596 interrupt completed command.\n", dev->name);
	if ((i596_debug > 4) && (status & 0x2000))
	    printk("%s: i596 interrupt command unit inactive %x.\n", dev->name, status & 0x0700);

	while ((lp->cmd_head != (struct i596_cmd *) I596_NULL) && (lp->cmd_head->status & STAT_C))
	{
	    ptr = lp->cmd_head;

	    lp->cmd_head = lp->cmd_head->next;
	    lp->cmd_backlog--;

	    switch ((ptr->command) & 0x7)
	    {
		case CmdTx:
		{
		    struct tx_cmd *tx_cmd = (struct tx_cmd *) ptr;
		    struct sk_buff *skb = ((struct sk_buff *)(tx_cmd->tbd->data)) -1;

		    dev_kfree_skb(skb, FREE_WRITE);

		    if ((ptr->status) & STAT_OK)
		    {
	    		if (i596_debug >2) print_eth(skb->data);
		    }
		    else
		    {
			lp->stats.tx_errors++;
			if ((ptr->status) & 0x0020) lp->stats.collisions++;
			if (!((ptr->status) & 0x0040)) lp->stats.tx_heartbeat_errors++;
			if ((ptr->status) & 0x0400) lp->stats.tx_carrier_errors++;
			if ((ptr->status) & 0x0800) lp->stats.collisions++;
			if ((ptr->status) & 0x1000) lp->stats.tx_aborted_errors++;
		    }


		    ptr->next = (struct i596_cmd * ) I596_NULL;
		    kfree_s((unsigned char *)tx_cmd, (sizeof (struct tx_cmd) + sizeof (struct i596_tbd)));
		    break;
		}
		case CmdMulticastList:
		{
		    unsigned short count = *((unsigned short *) (ptr + 1));

		    ptr->next = (struct i596_cmd * ) I596_NULL;
		    kfree_s((unsigned char *)ptr, (sizeof (struct i596_cmd) + count + 2));
		    break;
		}
		case CmdTDR:
		{
		    unsigned long status = *((unsigned long *) (ptr + 1));

		    if (status & 0x8000)
		    {
			if (i596_debug > 3)
	    		    printk("%s: link ok.\n", dev->name);
		    }
		    else
		    {
			if (status & 0x4000)
	    		    printk("%s: Transceiver problem.\n", dev->name);
			if (status & 0x2000)
	    		    printk("%s: Termination problem.\n", dev->name);
			if (status & 0x1000)
	    		    printk("%s: Short circuit.\n", dev->name);

	    		printk("%s: Time %ld.\n", dev->name, status & 0x07ff);
		    }
		}
		default:
		    ptr->next = (struct i596_cmd * ) I596_NULL;

		lp->last_cmd = jiffies;
 	    }
	}

	ptr = lp->cmd_head;
	while ((ptr != (struct i596_cmd *) I596_NULL) && (ptr != lp->cmd_tail))
	{
	    ptr->command &= 0x1fff;
	    ptr = ptr->next;
	}

	if ((lp->cmd_head != (struct i596_cmd *) I596_NULL) && (dev->start)) ack_cmd |= CUC_START;
	lp->scb.cmd = lp->cmd_head;
    }

    if ((status & 0x1000) || (status & 0x4000))
    {
	if ((i596_debug > 4) && (status & 0x4000))
	    printk("%s: i596 interrupt received a frame.\n", dev->name);
	if ((i596_debug > 4) && (status & 0x1000))
	    printk("%s: i596 interrupt receive unit inactive %x.\n", dev->name, status & 0x0070);

	i596_rx(dev);

	if (dev->start) ack_cmd |= RX_START;
    }

    /* acknowledge the interrupt */

/*
    if ((lp->scb.cmd != (struct i596_cmd *) I596_NULL) && (dev->start)) ack_cmd | = CUC_START;
*/
    boguscnt = 100;
    while (lp->scb.status, lp->scb.command)
	if (--boguscnt == 0)
	    {
		printk("%s: i596 interrupt, timeout status %4.4x command %4.4x.\n", dev->name, lp->scb.status, lp->scb.command);
		break;
	    }
    lp->scb.command = ack_cmd;

    (void) inb (ioaddr+0x10);
    outb (4, ioaddr+0xf);
    outw (0, ioaddr+4);

    if (i596_debug > 4)
	printk("%s: exiting interrupt.\n", dev->name);

    dev->interrupt = 0;
    return;
}

static int
i596_close(struct device *dev)
{
    int ioaddr = dev->base_addr;
    struct i596_private *lp = (struct i596_private *)dev->priv;
    int boguscnt = 200;

    dev->start = 0;
    dev->tbusy = 1;

    if (i596_debug > 1)
	printk("%s: Shutting down ethercard, status was %4.4x.\n",
	       dev->name, lp->scb.status);

    lp->scb.command = CUC_ABORT|RX_ABORT;
    outw(0, ioaddr+4);

    i596_cleanup_cmd(lp);

    while (lp->scb.status, lp->scb.command)
	if (--boguscnt == 0)
	{
	    printk("%s: close timed timed out with status %4.4x, cmd %4.4x.\n",
		   dev->name, lp->scb.status, lp->scb.command);
	    break;
    	}
    free_irq(dev->irq);
    irq2dev_map[dev->irq] = 0;
    remove_rx_bufs(dev);
    MOD_DEC_USE_COUNT;

    return 0;
}

static struct enet_statistics *
i596_get_stats(struct device *dev)
{
    struct i596_private *lp = (struct i596_private *)dev->priv;

    return &lp->stats;
}

/*
 *	Set or clear the multicast filter for this adaptor.
 */
 
static void set_multicast_list(struct device *dev)
{
	struct i596_private *lp = (struct i596_private *)dev->priv;
	struct i596_cmd *cmd;

	if (i596_debug > 1)
		printk ("%s: set multicast list %d\n", dev->name, dev->mc_count);

	if (dev->mc_count > 0) 
	{
		struct dev_mc_list *dmi;
		char *cp;
		cmd = (struct i596_cmd *) kmalloc(sizeof(struct i596_cmd)+2+dev->mc_count*6, GFP_ATOMIC);
		if (cmd == NULL)
		{
			printk ("%s: set_multicast Memory squeeze.\n", dev->name);
			return;
		}
		cmd->command = CmdMulticastList;
		*((unsigned short *) (cmd + 1)) = dev->mc_count * 6;
		cp=((char *)(cmd + 1))+2;
		for(dmi=dev->mc_list;dmi!=NULL;dmi=dmi->next)
		{
			memcpy(cp, dmi,6);
			cp+=6;
		}
		print_eth (((char *)(cmd + 1)) + 2);
		i596_add_cmd(dev, cmd);
	}
	else
	{
		if (lp->set_conf.next != (struct i596_cmd * ) I596_NULL) 
			return;
		if (dev->mc_count == 0 && !(dev->flags&(IFF_PROMISC|IFF_ALLMULTI)))
		{
			if(dev->flags&IFF_ALLMULTI)
				dev->flags|=IFF_PROMISC;
			lp->i596_config[8] &= ~0x01;
		}
		else
			lp->i596_config[8] |= 0x01;

		i596_add_cmd(dev, &lp->set_conf);
	}
}

#ifdef HAVE_DEVLIST
static unsigned int apricot_portlist[] = {0x300, 0};
struct netdev_entry apricot_drv = 
{"apricot", apricot_probe, APRICOT_TOTAL_SIZE, apricot_portlist};
#endif

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_apricot = {
  devicename, /* device name inserted by /linux/drivers/net/net_init.c */
  0, 0, 0, 0,
  0x300, 10,
  0, 0, 0, NULL, apricot_probe };

static int io = 0x300;
static int irq = 10;

int
init_module(void)
{
  dev_apricot.base_addr = io;
  dev_apricot.irq       = irq;
  if (register_netdev(&dev_apricot) != 0)
    return -EIO;
  return 0;
}

void
cleanup_module(void)
{
    unregister_netdev(&dev_apricot);
    kfree_s((void *)dev_apricot.mem_start, sizeof(struct i596_private) + 0xf);
    dev_apricot.priv = NULL;

    /* If we don't do this, we can't re-insmod it later. */
    release_region(dev_apricot.base_addr, APRICOT_TOTAL_SIZE);
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c apricot.c"
 * End:
 */
