/*
 * Linux ethernet device driver for the 3Com Etherlink Plus (3C505)
 * 	By Craig Southeren and Juha Laiho
 *
 * 3c505.c	This module implements an interface to the 3Com
 *		Etherlink Plus (3c505) ethernet card. Linux device 
 *		driver interface reverse engineered from the Linux 3C509
 *		device drivers. Some 3C505 information gleaned from
 *		the Crynwr packet driver. Still this driver would not
 *		be here without 3C505 technical reference provided by
 *		3Com.
 *
 * Version:	@(#)3c505.c	0.8.4	17-Dec-95
 *
 * Authors:	Linux 3c505 device driver by
 *			Craig Southeren, <craigs@ineluki.apana.org.au>
 *              Final debugging by
 *			Andrew Tridgell, <tridge@nimbus.anu.edu.au>
 *		Auto irq/address, tuning, cleanup and v1.1.4+ kernel mods by
 *			Juha Laiho, <jlaiho@ichaos.nullnet.fi>
 *              Linux 3C509 driver by
 *             		Donald Becker, <becker@super.org>
 *		Crynwr packet driver by
 *			Krishnan Gopalan and Gregg Stefancik,
 * 			   Clemson University Engineering Computer Operations.
 *			Portions of the code have been adapted from the 3c505
 *			   driver for NCSA Telnet by Bruce Orchard and later
 *			   modified by Warren Van Houten and krus@diku.dk.
 *              3C505 technical information provided by
 *                      Terry Murphy, of 3Com Network Adapter Division
 *		Linux 1.3.0 changes by
 *			Alan Cox <Alan.Cox@linux.org>
 *                     
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "3c505.h"

/*********************************************************
 *
 *  define debug messages here as common strings to reduce space
 *
 *********************************************************/

static const char * filename = __FILE__;

static const char * null_msg = "*** NULL at %s:%s (line %d) ***\n";
#define CHECK_NULL(p) \
	if (!p) printk(null_msg, filename,__FUNCTION__,__LINE__)

static const char * timeout_msg = "*** timeout at %s:%s (line %d) ***\n";
#define TIMEOUT_MSG(lineno) \
	printk(timeout_msg, filename,__FUNCTION__,(lineno))

static const char * invalid_pcb_msg =
	"*** invalid pcb length %d at %s:%s (line %d) ***\n";
#define INVALID_PCB_MSG(len) \
	printk(invalid_pcb_msg, (len),filename,__FUNCTION__,__LINE__)

static const char * search_msg = "%s: Looking for 3c505 adapter at address %#x...";

static const char * stilllooking_msg = "still looking...";

static const char * found_msg = "found.\n";

static const char * notfound_msg = "not found (reason = %d)\n";

static const char * couldnot_msg = "%s: 3c505 not found\n";

/*********************************************************
 *
 *  various other debug stuff
 *
 *********************************************************/

#ifdef ELP_DEBUG
static int elp_debug = ELP_DEBUG;
#else
static int elp_debug = 0;
#endif

/*
 *  0 = no messages (well, some)
 *  1 = messages when high level commands performed
 *  2 = messages when low level commands performed
 *  3 = messages when interrupts received
 */

#define	ELP_VERSION	"0.8.4"

#ifdef MACH
#define ELP_NEED_HARD_RESET 0
#endif

/*****************************************************************
 *
 * useful macros
 *
 *****************************************************************/

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif


/*****************************************************************
 *
 * List of I/O-addresses we try to auto-sense
 * Last element MUST BE 0!
 *****************************************************************/

const int addr_list[]={0x300,0x280,0x310,0}; 

/*****************************************************************
 *
 * Functions for I/O (note the inline !)
 *
 *****************************************************************/

static inline unsigned char
inb_status (unsigned int base_addr)
{
	return inb(base_addr+PORT_STATUS);
}

static inline unsigned char
inb_control (unsigned int base_addr)
{
	return inb(base_addr+PORT_CONTROL);
}

static inline int
inb_command (unsigned int base_addr)
{
	return inb(base_addr+PORT_COMMAND);
}

static inline void
outb_control (unsigned char val, unsigned int base_addr)
{
	outb(val, base_addr+PORT_CONTROL);
}

static inline void
outb_command (unsigned char val, unsigned int base_addr)
{
	outb(val, base_addr+PORT_COMMAND);
}

static inline unsigned int
inw_data (unsigned int base_addr)
{
	return inw(base_addr+PORT_DATA);
}

static inline void
outw_data (unsigned int val, unsigned int base_addr)
{
	outw(val, base_addr+PORT_DATA);
}


/*****************************************************************
 *
 *  structure to hold context information for adapter
 *
 *****************************************************************/

typedef struct {
	volatile short got[NUM_TRANSMIT_CMDS];	/* flags for command completion */
	pcb_struct tx_pcb;	/* PCB for foreground sending */
	pcb_struct rx_pcb;	/* PCB for foreground receiving */
	pcb_struct itx_pcb;	/* PCB for background sending */
	pcb_struct irx_pcb;	/* PCB for background receiving */
	struct enet_statistics stats;
} elp_device;

static int reset_count=0;

/*****************************************************************
 *
 *  useful functions for accessing the adapter
 *
 *****************************************************************/

/*
 * use this routine when accessing the ASF bits as they are
 * changed asynchronously by the adapter
 */

/* get adapter PCB status */
#define	GET_ASF(addr) \
	(get_status(addr)&ASF_PCB_MASK)

static inline int
get_status (unsigned int base_addr)
{
	int timeout = jiffies + 10;
	register int stat1;
	do {
		stat1 = inb_status(base_addr);
	} while (stat1 != inb_status(base_addr) && jiffies < timeout);
	if (jiffies >= timeout)
		TIMEOUT_MSG(__LINE__);
	return stat1;
}

static inline void
set_hsf (unsigned int base_addr, int hsf)
{
	cli();
	outb_control((inb_control(base_addr)&~HSF_PCB_MASK)|hsf, base_addr);
	sti(); 
}

#define WAIT_HCRE(addr,toval) wait_hcre((addr),(toval),__LINE__)
static inline int
wait_hcre (unsigned int base_addr, int toval, int lineno)
{
	int timeout = jiffies + toval;
	while (((inb_status(base_addr)&HCRE)==0) && (jiffies <= timeout))
		;
	if (jiffies >= timeout) {
		TIMEOUT_MSG(lineno);
		return FALSE;
	}
	return TRUE;
}

static inline int
wait_fast_hcre (unsigned int base_addr, int toval, int lineno)
{
	int timeout = 0;
	while (((inb_status(base_addr)&HCRE)==0) && (timeout++ < toval))
		;
	if (timeout >= toval) {
		sti();
		TIMEOUT_MSG(lineno);
		return FALSE;
	}
	return TRUE;
}

static int start_receive (struct device *, pcb_struct *);
static void adapter_hard_reset (struct device *);

inline static void
adapter_reset (struct device * dev)
{
	int timeout;
	unsigned char orig_hcr=inb_control(dev->base_addr);

	elp_device * adapter=dev->priv;

	outb_control(0,dev->base_addr);

	if (inb_status(dev->base_addr)&ACRF) {
		do {
			inb_command(dev->base_addr);
			timeout=jiffies+2;
			while ((jiffies<=timeout) && !(inb_status(dev->base_addr)&ACRF))
				;
		} while (inb_status(dev->base_addr)&ACRF);
		set_hsf(dev->base_addr,HSF_PCB_NAK);
	}

	outb_control(inb_control(dev->base_addr)|ATTN|DIR,dev->base_addr);
	timeout=jiffies+1;
	while (jiffies<=timeout)
		;
	outb_control(inb_control(dev->base_addr)&~ATTN,dev->base_addr);
	timeout=jiffies+1;
	while (jiffies<=timeout)
		;
	outb_control(inb_control(dev->base_addr)|FLSH,dev->base_addr);
	timeout=jiffies+1;
	while (jiffies<=timeout)
		;
	outb_control(inb_control(dev->base_addr)&~FLSH,dev->base_addr);
	timeout=jiffies+1;
	while (jiffies<=timeout)
		;

	outb_control(orig_hcr, dev->base_addr);
	if (!start_receive(dev, &adapter->tx_pcb))
		printk("%s: start receive command failed \n", dev->name);
}

/*****************************************************************
 *
 * send_pcb
 *   Send a PCB to the adapter. 
 *
 *	output byte to command reg  --<--+
 *	wait until HCRE is non zero      |
 *	loop until all bytes sent   -->--+
 *	set HSF1 and HSF2 to 1
 *	output pcb length
 *	wait until ASF give ACK or NAK
 *	set HSF1 and HSF2 to 0
 *
 *****************************************************************/

static int
send_pcb (struct device * dev, pcb_struct * pcb)
{
	int i;
	int timeout;
	int cont;

	/*
	 * load each byte into the command register and
	 * wait for the HCRE bit to indicate the adapter
	 * had read the byte
	 */
	set_hsf(dev->base_addr,0); 
	if ((cont = WAIT_HCRE(dev->base_addr,5))) {
		cli();
		if (pcb->command==CMD_TRANSMIT_PACKET)
			outb_control(inb_control(dev->base_addr)&~DIR,dev->base_addr);
		outb_command(pcb->command, dev->base_addr);
		sti();
		cont = WAIT_HCRE(dev->base_addr,5);
	}

	if (cont) {
		outb_command(pcb->length, dev->base_addr);
		cont = WAIT_HCRE(dev->base_addr,5);
	}

	cli();
	for (i = 0; cont && (i < pcb->length); i++) {
		outb_command(pcb->data.raw[i], dev->base_addr);
		cont = wait_fast_hcre(dev->base_addr,20000,__LINE__);
	} /* if wait_fast_hcre() failed, has already done sti() */

	/* set the host status bits to indicate end of PCB */
	/* send the total packet length as well */
	/* wait for the adapter to indicate that it has read the PCB */
	if (cont) {
		set_hsf(dev->base_addr,HSF_PCB_END);
		outb_command(2+pcb->length, dev->base_addr);
		sti();
		timeout = jiffies + 7;
		while (jiffies < timeout) {
			i = GET_ASF(dev->base_addr);
			if ((i == ASF_PCB_ACK) || (i == ASF_PCB_NAK))
				break;
		}

		if (i == ASF_PCB_ACK) {
			reset_count=0;
			return TRUE;
		}
		else if (i == ASF_PCB_NAK) {
			printk("%s: PCB send was NAKed\n", dev->name);
		} else {
			printk("%s: timeout after sending PCB\n", dev->name);
		}
	} else {
		sti();
		printk("%s: timeout in middle of sending PCB\n", dev->name);
	}

	adapter_reset(dev);
	return FALSE;
}

/*****************************************************************
 *
 * receive_pcb
 *   Read a PCB to the adapter
 *
 *	wait for ACRF to be non-zero        ---<---+
 *	input a byte                               |
 *	if ASF1 and ASF2 were not both one         |
 *		before byte was read, loop      --->---+
 *	set HSF1 and HSF2 for ack
 *
 *****************************************************************/

static int
receive_pcb (struct device * dev, pcb_struct * pcb)
{
	int i, j;
	int total_length;
	int stat;
	int timeout;

	CHECK_NULL(pcb);
	CHECK_NULL(dev);

	set_hsf(dev->base_addr,0);

	/* get the command code */
	timeout = jiffies + 2;
	while (((stat = get_status(dev->base_addr))&ACRF) == 0 && jiffies < timeout)
		;
	if (jiffies >= timeout) {
		TIMEOUT_MSG(__LINE__);
		return FALSE;
	}

	pcb->command = inb_command(dev->base_addr);

	/* read the data length */
	timeout = jiffies + 3;
	while (((stat = get_status(dev->base_addr)) & ACRF) == 0 && jiffies < timeout)
		;
	if (jiffies >= timeout) {
		TIMEOUT_MSG(__LINE__);
		return FALSE;
	}
	pcb->length = inb_command(dev->base_addr);

	if (pcb->length > MAX_PCB_DATA) {
		INVALID_PCB_MSG(pcb->length);
		adapter_reset(dev);
		return FALSE;
	}

	/* read the data */
	cli();
	i = 0;
	do {
		j = 0;
		while (((stat = get_status(dev->base_addr))&ACRF) == 0 && j++ < 20000)
			;
		pcb->data.raw[i++] = inb_command(dev->base_addr);
		if (i > MAX_PCB_DATA)
			INVALID_PCB_MSG(i);
	} while ((stat & ASF_PCB_MASK) != ASF_PCB_END && j < 20000);
	sti();
	if (j >= 20000) {
		TIMEOUT_MSG(__LINE__);
		return FALSE;
	}

	/* woops, the last "data" byte was really the length! */
	total_length = pcb->data.raw[--i];

	/* safety check total length vs data length */
	if (total_length != (pcb->length + 2)) {
		if (elp_debug >= 2)
			printk("%s: mangled PCB received\n", dev->name);
		set_hsf(dev->base_addr,HSF_PCB_NAK);
		return FALSE;
	}

	set_hsf(dev->base_addr,HSF_PCB_ACK);
	reset_count=0;
	return TRUE;
}

static void
adapter_hard_reset (struct device * dev)
{
	int timeout;
	long flags;

	CHECK_NULL(dev);

	save_flags(flags);
	sti();

	if (elp_debug > 0)
		printk("%s: Resetting the adapter, please wait (approx 20 s)\n",
			dev->name);
	/*
	 * take FLSH and ATTN high
	 */
	outb_control(ATTN|FLSH, dev->base_addr);

	/*
	 * wait for a little bit
	 */
	for (timeout = jiffies + 20; jiffies <= timeout; )
		;

	/*
	 * now take them low
	 */
	outb_control(0, dev->base_addr);

	/*
	 * wait for a little bit
	 */
	for (timeout = jiffies + 20; jiffies <= timeout; )
		;

	/*
	 * now hang around until the board gets it's act together
	 */
	for (timeout = jiffies + (100 * 15); jiffies <= timeout; ) 
		if (GET_ASF(dev->base_addr) != ASF_PCB_END)
			break;
	restore_flags(flags);
}

/******************************************************
 *
 *  queue a receive command on the adapter so we will get an
 *  interrupt when a packet is received.
 *
 ******************************************************/

static int
start_receive (struct device * dev, pcb_struct * tx_pcb)
{
	CHECK_NULL(dev);
	CHECK_NULL(tx_pcb);

	if (elp_debug >= 3)
		printk("%s: restarting receiver\n", dev->name);
	tx_pcb->command = CMD_RECEIVE_PACKET;
	tx_pcb->length = sizeof(struct Rcv_pkt);
	tx_pcb->data.rcv_pkt.buf_seg
		= tx_pcb->data.rcv_pkt.buf_ofs = 0; /* Unused */
	tx_pcb->data.rcv_pkt.buf_len = 1600;
	tx_pcb->data.rcv_pkt.timeout = 0;	/* set timeout to zero */
	return send_pcb(dev, tx_pcb); 
}

/******************************************************
 *
 * extract a packet from the adapter
 * this routine is only called from within the interrupt
 * service routine, so no cli/sti calls are needed
 * note that the length is always assumed to be even
 *
 ******************************************************/

static void
receive_packet (struct device * dev, int len)
{
	register int i;
	unsigned short * ptr;
	int timeout;
	int rlen;
	struct sk_buff *skb;
	elp_device * adapter;

	CHECK_NULL(dev);
	adapter=dev->priv;

	if (len <= 0 || ((len & ~1) != len))
		if (elp_debug >= 3) {
			sti();
			printk("*** bad packet len %d at %s(%d)\n",len,filename,__LINE__);
			cli();
		}

	rlen = (len+1) & ~1;

	skb = dev_alloc_skb(rlen+2);

	/*
	 * make sure the data register is going the right way
	 */

	outb_control(inb_control(dev->base_addr)|DIR, dev->base_addr);

	/*
	 * if buffer could not be allocated, swallow it
	 */
	if (skb == NULL) {
		for (i = 0; i < (rlen/2); i++) {
			timeout = 0;
			while ((inb_status(dev->base_addr)&HRDY) == 0 && timeout++ < 20000)
				;
			if (timeout >= 20000) {
				sti();
				TIMEOUT_MSG(__LINE__);
				break;
			}

			inw_data(dev->base_addr);
		}
		adapter->stats.rx_dropped++;

	} else {
		skb_reserve(skb,2);	/* 16 byte alignment */
		skb->dev = dev;

		/*
		 * now read the data from the adapter
		 */
		ptr = (unsigned short *)skb_put(skb,len);
		for (i = 0; i < (rlen/2); i++) { 
			timeout = 0;
			while ((inb_status(dev->base_addr)&HRDY) == 0 && timeout++ < 20000) 
				;
			if (timeout >= 20000) {
				sti();
				printk("*** timeout at %s(%d) reading word %d of %d ***\n",
					filename,__LINE__, i, rlen/2);	
				kfree_skb(skb, FREE_WRITE);
				return;
			}

			*ptr = inw_data(dev->base_addr); 
			ptr++; 
		}

		sti();
		skb->protocol=eth_type_trans(skb,dev);
		netif_rx(skb);
	}

	outb_control(inb_control(dev->base_addr)&~DIR, dev->base_addr);
}


/******************************************************
 *
 * interrupt handler
 *
 ******************************************************/

static void
elp_interrupt (int irq, struct pt_regs *reg_ptr)
{
	int len;
	int dlen;
	struct device *dev;
	elp_device * adapter;
	int timeout;

	if (irq < 0 || irq > 15) {
		printk ("elp_interrupt(): illegal IRQ number found in interrupt routine (%i)\n", irq);
		return;
	}

	dev = irq2dev_map[irq];

	if (dev == NULL) {
		printk ("elp_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	adapter = (elp_device *) dev->priv;

	CHECK_NULL(adapter);

	if (dev->interrupt)
		if (elp_debug >= 2)
			printk("%s: Re-entering the interrupt handler.\n", dev->name);
	dev->interrupt = 1;

	/*
	 * allow interrupts (we need timers!)
	 */
	sti();

	/*
	 * receive a PCB from the adapter
	 */
	timeout = jiffies + 3;
	while ((inb_status(dev->base_addr)&ACRF) != 0 && jiffies < timeout) {

		if (receive_pcb(dev, &adapter->irx_pcb)) {

			switch (adapter->irx_pcb.command) {

				/*
				 * received a packet - this must be handled fast
				 */
				case CMD_RECEIVE_PACKET_COMPLETE:
					/* if the device isn't open, don't pass packets up the stack */
					if (dev->start == 0)
						break;
					cli();
					/* Set direction of adapter FIFO */
					outb_control(inb_control(dev->base_addr)|DIR,
					             dev->base_addr);
					len = adapter->irx_pcb.data.rcv_resp.pkt_len;
					dlen = adapter->irx_pcb.data.rcv_resp.buf_len;
					if (adapter->irx_pcb.data.rcv_resp.timeout != 0) {
						printk("%s: interrupt - packet not received correctly\n", dev->name);
						sti();
					} else {
						if (elp_debug >= 3) {
							sti();
							printk("%s: interrupt - packet received of length %i (%i)\n", dev->name, len, dlen);
							cli();
						}
						receive_packet(dev, dlen);
						sti();
						if (elp_debug >= 3)
							printk("%s: packet received\n", dev->name);
					}
					if (dev->start && !start_receive(dev, &adapter->itx_pcb)) 
						if (elp_debug >= 2)
							printk("%s: interrupt - failed to send receive start PCB\n", dev->name);
					if (elp_debug >= 3)
					printk("%s: receive procedure complete\n", dev->name);

					break;

				/*
				 * 82586 configured correctly
				 */
				case CMD_CONFIGURE_82586_RESPONSE:
					adapter->got[CMD_CONFIGURE_82586] = 1;
					if (elp_debug >= 3)
						printk("%s: interrupt - configure response received\n", dev->name);
					break;

				/*
				 * Adapter memory configuration
				 */
				case CMD_CONFIGURE_ADAPTER_RESPONSE:
					adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 1;
					if (elp_debug >= 3)
						printk("%s: Adapter memory configuration %s.\n",dev->name,
							adapter->irx_pcb.data.failed?"failed":"succeeded");
					break;

				/*
				 * Multicast list loading
				 */
				case CMD_LOAD_MULTICAST_RESPONSE:
					adapter->got[CMD_LOAD_MULTICAST_LIST] = 1;
					if (elp_debug >= 3)
						printk("%s: Multicast address list loading %s.\n",dev->name,
							adapter->irx_pcb.data.failed?"failed":"succeeded");
					break;

				/*
				 * Station address setting
				 */
				case CMD_SET_ADDRESS_RESPONSE:
					adapter->got[CMD_SET_STATION_ADDRESS] = 1;
					if (elp_debug >= 3)
						printk("%s: Ethernet address setting %s.\n",dev->name,
							adapter->irx_pcb.data.failed?"failed":"succeeded");
					break;


				/*
				 * received board statistics
				 */
				case CMD_NETWORK_STATISTICS_RESPONSE:
					adapter->stats.rx_packets += adapter->irx_pcb.data.netstat.tot_recv;
					adapter->stats.tx_packets += adapter->irx_pcb.data.netstat.tot_xmit;
					adapter->stats.rx_crc_errors += adapter->irx_pcb.data.netstat.err_CRC;
					adapter->stats.rx_frame_errors += adapter->irx_pcb.data.netstat.err_align;
					adapter->stats.rx_fifo_errors += adapter->irx_pcb.data.netstat.err_ovrrun;
					adapter->got[CMD_NETWORK_STATISTICS] = 1;
					if (elp_debug >= 3)
						printk("%s: interrupt - statistics response received\n", dev->name);
					break;

				/*
				 * sent a packet
				 */
				case CMD_TRANSMIT_PACKET_COMPLETE:
					if (elp_debug >= 3) 
					printk("%s: interrupt - packet sent\n", dev->name);
					if (dev->start == 0)
						break;
					if (adapter->irx_pcb.data.xmit_resp.c_stat != 0)
						if (elp_debug >= 2)
							printk("%s: interrupt - error sending packet %4.4x\n",
								dev->name, adapter->irx_pcb.data.xmit_resp.c_stat);
					dev->tbusy = 0;
					mark_bh(NET_BH);
					break;

				/*
				 * some unknown PCB
				 */
				default:
					printk("%s: unknown PCB received - %2.2x\n", dev->name, adapter->irx_pcb.command);
					break;
			}
		} else {
			printk("%s: failed to read PCB on interrupt\n", dev->name);
			adapter_reset(dev);
		}
	}

	/*
	 * indicate no longer in interrupt routine
	 */
	dev->interrupt = 0;
}


/******************************************************
 *
 * open the board
 *
 ******************************************************/

static int
elp_open (struct device *dev)
{
	elp_device * adapter;

	CHECK_NULL(dev);

	adapter = dev->priv;

	if (elp_debug >= 3)  
		printk("%s: request to open device\n", dev->name);

	/*
	 * make sure we actually found the device
	 */
	if (adapter == NULL) {
		printk("%s: Opening a non-existent physical device\n", dev->name);
		return -EAGAIN;
	}

	/*
	 * disable interrupts on the board
	 */
	outb_control(0x00, dev->base_addr);

	/*
	 * clear any pending interrupts
	 */
	inb_command(dev->base_addr);
	adapter_reset(dev);

	/*
	 * interrupt routine not entered
	 */
	dev->interrupt = 0;

	/*
	 *  transmitter not busy 
	 */
	dev->tbusy = 0;

	/*
	 * make sure we can find the device header given the interrupt number
	 */
	irq2dev_map[dev->irq] = dev;

	/*
	 * install our interrupt service routine
	 */
	if (request_irq(dev->irq, &elp_interrupt, 0, "3c505")) {
		irq2dev_map[dev->irq] = NULL;
		return -EAGAIN;
	}

	/*
	 * enable interrupts on the board
	 */
	outb_control(CMDE, dev->base_addr);

	/*
	 * device is now officially open!
	 */
	dev->start = 1;

	/*
	 * configure adapter memory: we need 10 multicast addresses, default==0
	 */
	if (elp_debug >= 3)
		printk("%s: sending 3c505 memory configuration command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_ADAPTER_MEMORY;
	adapter->tx_pcb.data.memconf.cmd_q = 10;
	adapter->tx_pcb.data.memconf.rcv_q = 20;
	adapter->tx_pcb.data.memconf.mcast = 10;
	adapter->tx_pcb.data.memconf.frame = 20;
	adapter->tx_pcb.data.memconf.rcv_b = 20;
	adapter->tx_pcb.data.memconf.progs = 0;
	adapter->tx_pcb.length = sizeof(struct Memconf);
	adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		printk("%s: couldn't send memory configuration command\n", dev->name);
	else {
		int timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_CONFIGURE_ADAPTER_MEMORY] == 0 && jiffies < timeout)
			;
		if (jiffies >= timeout)
			TIMEOUT_MSG(__LINE__);
	}


	/*
	 * configure adapter to receive broadcast messages and wait for response
	 */
	if (elp_debug >= 3)
		printk("%s: sending 82586 configure command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_82586;
	adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD;
	adapter->tx_pcb.length  = 2;
	adapter->got[CMD_CONFIGURE_82586] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		printk("%s: couldn't send 82586 configure command\n", dev->name);
	else {
		int timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_CONFIGURE_82586] == 0 && jiffies < timeout)
			;
		if (jiffies >= timeout)
			TIMEOUT_MSG(__LINE__);
	}

	/*
	 * queue receive commands to provide buffering
	 */
	if (!start_receive(dev, &adapter->tx_pcb))
		printk("%s: start receive command failed \n", dev->name);
	if (elp_debug >= 3)
		printk("%s: start receive command sent\n", dev->name);

	MOD_INC_USE_COUNT;

	return 0;			/* Always succeed */
}


/******************************************************
 *
 * send a packet to the adapter
 *
 ******************************************************/

static int
send_packet (struct device * dev, unsigned char * ptr, int len)
{
	int i;
	int timeout = 0;
	elp_device * adapter;

	/*
	 * make sure the length is even and no shorter than 60 bytes
	 */
	unsigned int nlen = (((len < 60) ? 60 : len) + 1) & (~1);

	CHECK_NULL(dev);
	CHECK_NULL(ptr);

	adapter = dev->priv;

	if (nlen < len)
		printk("Warning, bad length nlen=%d len=%d %s(%d)\n",nlen,len,filename,__LINE__);

	/*
	 * send the adapter a transmit packet command. Ignore segment and offset
	 * and make sure the length is even
	 */
	adapter->tx_pcb.command = CMD_TRANSMIT_PACKET;
	adapter->tx_pcb.length = sizeof(struct Xmit_pkt);
	adapter->tx_pcb.data.xmit_pkt.buf_ofs
		= adapter->tx_pcb.data.xmit_pkt.buf_seg = 0; /* Unused */
	adapter->tx_pcb.data.xmit_pkt.pkt_len = nlen;
	if (!send_pcb(dev, &adapter->tx_pcb)) {
		return FALSE;
	}

	/*
	 * write data to the adapter
	 */
	cli();
	for (i = 0; i < (nlen/2);i++) {
		while (((inb_status(dev->base_addr)&HRDY) == 0)
		       && (timeout++ < 20000))
			;
		if (timeout >= 20000) {
			sti();
			printk("%s: timeout at %s(%d) writing word %d of %d ***\n",
				dev->name,filename,__LINE__, i, nlen/2);
			return FALSE;
		}

		outw_data(*(short *)ptr, dev->base_addr);
		ptr +=2;
	}
	sti();

	return TRUE;
}

/******************************************************
 *
 * start the transmitter
 *    return 0 if sent OK, else return 1
 *
 ******************************************************/

static int
elp_start_xmit (struct sk_buff *skb, struct device *dev)
{
	CHECK_NULL(dev);

	/*
	 * not sure what this does, but the 3c509 driver does it, so...
	 */
	if (skb == NULL) {
		dev_tint(dev);
		return 0;
	}

	/*
	 * if we ended up with a munged length, don't send it
	 */
	if (skb->len <= 0)
		return 0;

	if (elp_debug >= 3)
		printk("%s: request to send packet of length %d\n", dev->name, (int)skb->len);

	/*
	 * if the transmitter is still busy, we have a transmit timeout...
	 */
	if (dev->tbusy) {
		int tickssofar = jiffies - dev->trans_start;
		int stat;
		if (tickssofar < 50) /* was 500, AJT */
			return 1;
		printk("%s: transmit timed out, not resetting adapter\n", dev->name);
		if (((stat=inb_status(dev->base_addr))&ACRF) != 0) 
			printk("%s: hmmm...seemed to have missed an interrupt!\n", dev->name);
		printk("%s: status %#02x\n", dev->name, stat);
		dev->trans_start = jiffies;
		dev->tbusy = 0;
	}

	/*
	 * send the packet at skb->data for skb->len
	 */
	if (!send_packet(dev, skb->data, skb->len)) {
		printk("%s: send packet PCB failed\n", dev->name);
		return 1;
	}

	if (elp_debug >= 3)
		printk("%s: packet of length %d sent\n", dev->name, (int)skb->len);


	/*
	 * start the transmit timeout
	 */
	dev->trans_start = jiffies;

	/*
	 * the transmitter is now busy
	 */
	dev->tbusy = 1;

	/*
	 * free the buffer
	 */
	dev_kfree_skb(skb, FREE_WRITE);

	return 0;
}

/******************************************************
 *
 * return statistics on the board
 *
 ******************************************************/

static struct enet_statistics *
elp_get_stats (struct device *dev)
{
	elp_device *adapter = (elp_device *) dev->priv;

	if (elp_debug >= 3)
		printk("%s: request for stats\n", dev->name);

	/* If the device is closed, just return the latest stats we have,
	   - we cannot ask from the adapter without interrupts */
	if (!dev->start)
		return &adapter->stats;

	/* send a get statistics command to the board */
	adapter->tx_pcb.command = CMD_NETWORK_STATISTICS;
	adapter->tx_pcb.length  = 0;
	adapter->got[CMD_NETWORK_STATISTICS] = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		printk("%s: couldn't send get statistics command\n", dev->name);
	else {
		int timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_NETWORK_STATISTICS] == 0 && jiffies < timeout)
			;
		if (jiffies >= timeout) {
			TIMEOUT_MSG(__LINE__);
			return &adapter->stats;
		}
	}

	/* statistics are now up to date */
	return &adapter->stats;
}

/******************************************************
 *
 * close the board
 *
 ******************************************************/

static int
elp_close (struct device *dev)
{
	elp_device * adapter;

	CHECK_NULL(dev);
	adapter = dev->priv;
	CHECK_NULL(adapter);

	if (elp_debug >= 3)
		printk("%s: request to close device\n", dev->name);

	/* Someone may request the device statistic information even when
	 * the interface is closed. The following will update the statistics
	 * structure in the driver, so we'll be able to give current statistics.
	 */
	(void) elp_get_stats(dev);

	/*
	 * disable interrupts on the board
	 */
	outb_control(0x00, dev->base_addr);

	/*
	 *  flag transmitter as busy (i.e. not available)
	 */
	dev->tbusy = 1;

	/*
	 *  indicate device is closed
	 */
	dev->start = 0;

	/*
	 * release the IRQ
	 */
	free_irq(dev->irq);

	/*
	 * and we no longer have to map irq to dev either
	 */
	irq2dev_map[dev->irq] = 0;

	MOD_DEC_USE_COUNT;

	return 0;
}


/************************************************************
 *
 * Set multicast list
 * num_addrs==0: clear mc_list
 * num_addrs==-1: set promiscuous mode
 * num_addrs>0: set mc_list
 *
 ************************************************************/

static void
elp_set_mc_list (struct device *dev)
{
	elp_device *adapter = (elp_device *) dev->priv;
	struct dev_mc_list *dmi=dev->mc_list;
	int i;

	if (elp_debug >= 3)
		printk("%s: request to set multicast list\n", dev->name);

	if (!(dev->flags&(IFF_PROMISC|IFF_ALLMULTI)))
	{
		/* send a "load multicast list" command to the board, max 10 addrs/cmd */
		/* if num_addrs==0 the list will be cleared */
		adapter->tx_pcb.command = CMD_LOAD_MULTICAST_LIST;
		adapter->tx_pcb.length  = 6*dev->mc_count;
		for (i=0;i<dev->mc_count;i++)
		{
			memcpy(adapter->tx_pcb.data.multicast[i], dmi->dmi_addr,6);
			dmi=dmi->next;
		}
		adapter->got[CMD_LOAD_MULTICAST_LIST] = 0;
		if (!send_pcb(dev, &adapter->tx_pcb))
			printk("%s: couldn't send set_multicast command\n", dev->name);
		else {
			int timeout = jiffies + TIMEOUT;
			while (adapter->got[CMD_LOAD_MULTICAST_LIST] == 0 && jiffies < timeout)
				;
			if (jiffies >= timeout) {
				TIMEOUT_MSG(__LINE__);
			}
		}
		if (dev->mc_count)
			adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD | RECV_MULTI;
		else /* num_addrs == 0 */
			adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_BROAD;
	} 
	else
		adapter->tx_pcb.data.configure = NO_LOOPBACK | RECV_PROMISC;
	/*
	 * configure adapter to receive messages (as specified above)
	 * and wait for response
	 */
	if (elp_debug >= 3)
		printk("%s: sending 82586 configure command\n", dev->name);
	adapter->tx_pcb.command = CMD_CONFIGURE_82586;
	adapter->tx_pcb.length  = 2;
	adapter->got[CMD_CONFIGURE_82586]  = 0;
	if (!send_pcb(dev, &adapter->tx_pcb))
		printk("%s: couldn't send 82586 configure command\n", dev->name);
	else {
		int timeout = jiffies + TIMEOUT;
		while (adapter->got[CMD_CONFIGURE_82586] == 0 && jiffies < timeout)
			;
		if (jiffies >= timeout)
			TIMEOUT_MSG(__LINE__);
	}
}

/******************************************************
 *
 * initialise Etherlink Plus board
 *
 ******************************************************/

static void
elp_init (struct device *dev)
{
	elp_device * adapter;

	CHECK_NULL(dev);

	/*
	 * set ptrs to various functions
	 */
	dev->open = elp_open;	/* local */
	dev->stop = elp_close;	/* local */
	dev->get_stats = elp_get_stats;	/* local */
	dev->hard_start_xmit = elp_start_xmit;	/* local */
	dev->set_multicast_list = elp_set_mc_list;	/* local */

	/* Setup the generic properties */
	ether_setup(dev);

	/*
	 * setup ptr to adapter specific information
	 */
	adapter = (elp_device *)(dev->priv = kmalloc(sizeof(elp_device), GFP_KERNEL));
	CHECK_NULL(adapter);
	if (adapter == NULL)
		return;
	memset(&(adapter->stats), 0, sizeof(struct enet_statistics));

	/*
	 * memory information
	 */
	dev->mem_start = dev->mem_end = dev->rmem_end = dev->rmem_start = 0;
}

/************************************************************
 *
 * A couple of tests to see if there's 3C505 or not
 * Called only by elp_autodetect
 ************************************************************/

static int
elp_sense (struct device * dev)
{
	int timeout;
	int addr=dev->base_addr;
	const char *name=dev->name;
	long flags;
	byte orig_HCR, orig_HSR;

	if (check_region(addr, 0xf)) 
	  return -1;  

	orig_HCR=inb_control(addr);
	orig_HSR=inb_status(addr);

	if (elp_debug > 0)
		printk(search_msg, name, addr);

	if (((orig_HCR==0xff) && (orig_HSR==0xff)) ||
	    ((orig_HCR & DIR) != (orig_HSR & DIR))) {
		if (elp_debug > 0)
			printk(notfound_msg, 1);
		return -1; /* It can't be 3c505 if HCR.DIR != HSR.DIR */
	}

	/* Enable interrupts - we need timers! */
	save_flags(flags);
	sti();

	/* Wait for a while; the adapter may still be booting up */
	if (elp_debug > 0)
		printk(stilllooking_msg);
	for (timeout = jiffies + (100 * 15); jiffies <= timeout; ) 
		if (GET_ASF(addr) != ASF_PCB_END)
			break;

	if (orig_HCR & DIR) {
		/* If HCR.DIR is up, we pull it down. HSR.DIR should follow. */
		outb_control(orig_HCR & ~DIR,addr);
		timeout = jiffies+30;
		while (jiffies < timeout)
			;
		restore_flags(flags);
		if (inb_status(addr) & DIR) {
			outb_control(orig_HCR,addr);
			if (elp_debug > 0)
				printk(notfound_msg, 2);
			return -1;
		}
	} else {
		/* If HCR.DIR is down, we pull it up. HSR.DIR should follow. */
		outb_control(orig_HCR | DIR,addr);
		timeout = jiffies+300;
		while (jiffies < timeout)
			;
		restore_flags(flags);
		if (!(inb_status(addr) & DIR)) {
			outb_control(orig_HCR,addr);
			if (elp_debug > 0)
				printk(notfound_msg, 3);
			return -1;
		}
	}
	/*
	 * It certainly looks like a 3c505. If it has DMA enabled, it needs
	 * a hard reset. Also, do a hard reset if selected at the compile time.
	 */
	if (elp_debug > 0)
			printk(found_msg);

	if (((orig_HCR==0x35) && (orig_HSR==0x5b)) || ELP_NEED_HARD_RESET)
		adapter_hard_reset(dev);
	return 0;
}

/*************************************************************
 *
 * Search through addr_list[] and try to find a 3C505
 * Called only by eplus_probe
 *************************************************************/

static int
elp_autodetect (struct device * dev)
{
	int idx=0;

	/* if base address set, then only check that address
	otherwise, run through the table */
	if (dev->base_addr != 0) { /* dev->base_addr == 0 ==> plain autodetect */
		if (elp_sense(dev) == 0)
			return dev->base_addr;
	} else while ( (dev->base_addr=addr_list[idx++]) ) {
		if (elp_sense(dev) == 0)
			return dev->base_addr;
	}

	/* could not find an adapter */
	if (elp_debug > 0)
		printk(couldnot_msg, dev->name);

	return 0; /* Because of this, the layer above will return -ENODEV */
}

/******************************************************
 *
 * probe for an Etherlink Plus board at the specified address
 *
 ******************************************************/

int
elplus_probe (struct device *dev)
{
	elp_device adapter;
	int i;

	CHECK_NULL(dev);

	/*
	 *  setup adapter structure
	 */

	dev->base_addr = elp_autodetect(dev);
	if ( !(dev->base_addr) )
		return -ENODEV;

	/*
	 *  As we enter here from bootup, the adapter should have IRQs enabled,
	 *  but we can as well enable them anyway.
	 */
	outb_control(inb_control(dev->base_addr) | CMDE, dev->base_addr);
	autoirq_setup(0);

	/*
	 * use ethernet address command to probe for board in polled mode
	 * (this also makes us the IRQ that we need for automatic detection)
	 */
	adapter.tx_pcb.command = CMD_STATION_ADDRESS;
	adapter.tx_pcb.length  = 0;
	if (!send_pcb   (dev, &adapter.tx_pcb) ||
	    !receive_pcb(dev, &adapter.rx_pcb) ||
	    (adapter.rx_pcb.command != CMD_ADDRESS_RESPONSE) ||
	    (adapter.rx_pcb.length != 6)) {
		printk("%s: not responding to first PCB\n", dev->name);
		return -ENODEV;
	}

	if (dev->irq) { /* Is there a preset IRQ? */
		if (dev->irq != autoirq_report(0)) {
			printk("%s: Detected IRQ doesn't match user-defined one.\n",dev->name);
			return -ENODEV;
		}
		/* if dev->irq == autoirq_report(0), all is well */
	} else /* No preset IRQ; just use what we can detect */
		dev->irq=autoirq_report(0);
	switch (dev->irq) { /* Legal, sane? */
		case 0:
			printk("%s: No IRQ reported by autoirq_report().\n",dev->name);
			printk("%s: Check the jumpers of your 3c505 board.\n",dev->name);
			return -ENODEV;
		case 1:
		case 6:
		case 8:
		case 13: 
			printk("%s: Impossible IRQ %d reported by autoirq_report().\n",
			       dev->name, dev->irq);
			return -ENODEV;
	}
	/*
	 *  Now we have the IRQ number so we can disable the interrupts from
	 *  the board until the board is opened.
	 */
	outb_control(inb_control(dev->base_addr) & ~CMDE, dev->base_addr);

	/*
	 * copy ethernet address into structure
	 */
	for (i = 0; i < 6; i++) 
		dev->dev_addr[i] = adapter.rx_pcb.data.eth_addr[i];

	/*
	 * print remainder of startup message
	 */
	printk("%s: 3c505 card found at I/O %#lx using IRQ%d"
	       " has address %02x:%02x:%02x:%02x:%02x:%02x\n",
	       dev->name, dev->base_addr, dev->irq,
	       dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
	       dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	/*
	 * and reserve the address region
	 */
	request_region(dev->base_addr, ELP_IO_EXTENT, "3c505");

	/*
	 * initialise the device
	 */
	elp_init(dev);
	return 0;
}

#ifdef MODULE
static char devicename[9] = { 0, };
static struct device dev_3c505 = {
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, elplus_probe };

int io = 0x300;
int irq = 0;

int init_module(void)
{
	if (io == 0)
		printk("3c505: You should not use auto-probing with insmod!\n");
	dev_3c505.base_addr = io;
	dev_3c505.irq       = irq;
	if (register_netdev(&dev_3c505) != 0) {
		printk("3c505: register_netdev() returned non-zero.\n");
		return -EIO;
	}
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(&dev_3c505);
	kfree(dev_3c505.priv);
	dev_3c505.priv = NULL;

	/* If we don't do this, we can't re-insmod it later. */
	release_region(dev_3c505.base_addr, ELP_IO_EXTENT);
}
#endif /* MODULE */
