/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/* 
 *	Olivetti PC586 Mach Ethernet driver v1.0
 *	Copyright Ing. C. Olivetti & C. S.p.A. 1988, 1989
 *	All rights reserved.
 *
 */ 

/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/*
 * NOTE:
 *		by rvb:
 *  1.	The best book on the 82586 is:
 *		LAN Components User's Manual by Intel
 *	The copy I found was dated 1984.  This really tells you
 *	what the state machines are doing
 *  2.	In the current design, we only do one write at a time,
 *	though the hardware is capable of chaining and possibly
 *	even batching.  The problem is that we only make one
 *	transmit buffer available in sram space.
 *  3.	
 *  n.	Board Memory Map
	RFA/FD	0   -   227	0x228 bytes
		 226 = 0x19 * 0x16 bytes
	RBD	 228 - 3813	0x35ec bytes
		35e8 = 0x19 *	0x228 bytes
				== 0x0a bytes (bd) + 2 bytes + 21c bytes
	CU	3814 - 3913	0x100 bytes
	TBD	3914 - 39a3	0x90 bytes 
		  90 = No 18 * 0x08 bytes 
	TBUF	39a4 - 3fdd	0x63a bytes (= 1594(10))
	SCB	3fde - 3fed	0x10 bytes
	ISCP	3fee - 3ff5	0x08 bytes
	SCP	3ff6 - 3fff	0x0a bytes
 *		
 */

/*
 * NOTE:
 *
 *	Currently this driver doesn't support trailer protocols for
 *	packets.  Once that is added, please remove this comment.
 *
 *	Also, some lacking material includes the DLI code.  If you
 *	are compiling this driver with DLI set, lookout, that code
 * 	has not been looked at.
 *
 */

#define DEBUG
#define IF_CNTRS	MACH
#define	NDLI	0

#include	<pc586.h>

#ifdef	MACH_KERNEL
#include	<kern/time_out.h>
#include	<device/device_types.h>
#include	<device/errno.h>
#include	<device/io_req.h>
#include	<device/if_hdr.h>
#include	<device/if_ether.h>
#include	<device/net_status.h>
#include	<device/net_io.h>
#else	MACH_KERNEL
#include	<sys/param.h>
#include	<mach/machine/vm_param.h>
#include	<sys/systm.h>
#include	<sys/mbuf.h>
#include	<sys/buf.h>
#include	<sys/protosw.h>
#include	<sys/socket.h>
#include	<sys/vmmac.h>
#include	<sys/ioctl.h>
#include	<sys/errno.h>
#include	<sys/syslog.h>

#include	<net/if.h>
#include	<net/netisr.h>
#include	<net/route.h>

#ifdef	INET
#include	<netinet/in.h>
#include	<netinet/in_systm.h>
#include	<netinet/in_var.h>
#include	<netinet/ip.h>
#include	<netinet/if_ether.h>
#endif

#ifdef	NS
#include	<netns/ns.h>
#include	<netns/ns_if.h>
#endif

#if	DLI
#include	<net/dli_var.h>
struct	dli_var	de_dlv[NDE];
#endif	DLI
#endif	MACH_KERNEL

#include	<i386/ipl.h>
#include	<mach/vm_param.h>
#include	<vm/vm_kern.h>
#include	<chips/busses.h>
#include	<i386at/if_pc586.h>

#define	SPLNET	spl6
#if	__STDC__
#define CMD(x, y, unit) *(u_short *)(pc_softc[unit].prom + OFFSET_ ## x) = (u_short) (y)
#else	__STDC__
#define CMD(x, y, unit) *(u_short *)(pc_softc[unit].prom + OFFSET_/**/x) = (u_short) (y)
#endif	__STDC__

#define pc586chatt(unit)  CMD(CHANATT, 0x0001, unit)
#define pc586inton(unit)  CMD(INTENAB, CMD_1,  unit)
#define pc586intoff(unit) CMD(INTENAB, CMD_0,  unit)

int	pc586probe();
void	pc586attach();
int	pc586intr(), pc586init(), pc586output(), pc586ioctl(), pc586reset();
int	pc586watch(), pc586rcv(), pc586xmt(), pc586bldcu();
int	pc586diag(), pc586config();
char	*pc586bldru();
char	*ram_to_ptr();
u_short	ptr_to_ram();

static vm_offset_t pc586_std[NPC586] = { 0 };
static struct bus_device *pc586_info[NPC586];
struct	bus_driver	pcdriver = 
	{pc586probe, 0, pc586attach, 0, pc586_std, "pc", pc586_info, 0, 0, 0};

char	t_packet[ETHERMTU + sizeof(struct ether_header) + sizeof(long)];
int	xmt_watch = 0;

typedef struct { 
#ifdef	MACH_KERNEL
	struct	ifnet	ds_if;		/* generic interface header */
	u_char	ds_addr[6];		/* Ethernet hardware address */
#else	MACH_KERNEL
	struct	arpcom	pc586_ac;
#define	ds_if	pc586_ac.ac_if
#define	ds_addr	pc586_ac.ac_enaddr
#endif	MACH_KERNEL
	int	flags;
        int     seated;
        int     timer;
        int     open;
        fd_t    *begin_fd;
	fd_t    *end_fd;
	rbd_t   *end_rbd;
	char    *prom;
	char	*sram;
	int     tbusy;
	short	mode;
} pc_softc_t;
pc_softc_t	pc_softc[NPC586];

struct pc586_cntrs {
	struct {
		u_int xmt, xmti;
		u_int defer;
		u_int busy;
		u_int sleaze, intrinsic, intrinsic_count;
		u_int chain;
	} xmt; 
	struct {
		u_int rcv;
		u_int ovw;
		u_int crc;
		u_int frame;
		u_int rscerrs, ovrnerrs;
		u_int partial, bad_chain, fill;
	} rcv;
	u_int watch;
} pc586_cntrs[NPC586];


#ifdef	IF_CNTRS
int pc586_narp = 1, pc586_arp = 0;
int pc586_ein[32], pc586_eout[32]; 
int pc586_lin[128/8], pc586_lout[128/8]; 
static
log_2(no)
unsigned long no;
{
	return ({ unsigned long _temp__;
		asm("bsr %1, %0; jne 0f; xorl %0, %0; 0:" :
		    "=r" (_temp__) : "a" (no));
		_temp__;});
}
#endif	IF_CNTRS

/*
 * pc586probe:
 *
 *	This function "probes" or checks for the pc586 board on the bus to see
 *	if it is there.  As far as I can tell, the best break between this
 *	routine and the attach code is to simply determine whether the board
 *	is configured in properly.  Currently my approach to this is to write
 *	and read a word from the SRAM on the board being probed.  If the word
 *	comes back properly then we assume the board is there.  The config
 *	code expects to see a successful return from the probe routine before
 *	attach will be called.
 *
 * input	: address device is mapped to, and unit # being checked
 * output	: a '1' is returned if the board exists, and a 0 otherwise
 *
 */
pc586probe(port, dev)
struct bus_device	*dev;
{
	caddr_t		addr = (caddr_t)dev->address;
	int		unit = dev->unit;
	int		len = round_page(0x4000);
	int		sram_len = round_page(0x4000);
	extern		vm_offset_t phys_last_addr;
	int		i;
	volatile char	*b_prom;
	volatile char	*b_sram;
	volatile u_short*t_ps;

	if ((unit < 0) || (unit > NPC586)) {
		printf("pc%d: board out of range [0..%d]\n",
			unit, NPC586);
		return(0);
	}
	if ((addr > (caddr_t)0x100000) && (addr < (caddr_t)phys_last_addr))
		return 0;

	if (kmem_alloc_pageable(kernel_map, (vm_offset_t *) &b_prom, len)
							!= KERN_SUCCESS) {
		printf("pc%d: can not allocate memory for prom.\n", unit);
		return 0;
	}
	if (kmem_alloc_pageable(kernel_map, (vm_offset_t *) &b_sram, sram_len)
							!= KERN_SUCCESS) {
		printf("pc%d: can not allocate memory for sram.\n", unit);
		return 0;
	}
	(void)pmap_map(b_prom, (vm_offset_t)addr, 
			(vm_offset_t)addr+len, 
			VM_PROT_READ | VM_PROT_WRITE);
	if ((int)addr > 0x100000)			/* stupid hardware */
		addr += EXTENDED_ADDR;
	addr += 0x4000;					/* sram space */
	(void)pmap_map(b_sram, (vm_offset_t)addr, 
			(vm_offset_t)addr+sram_len, 
			VM_PROT_READ | VM_PROT_WRITE);

	*(b_prom + OFFSET_RESET) = 1;
	{ int i; for (i = 0; i < 1000; i++);	/* 4 clocks at 6Mhz */}
	*(b_prom + OFFSET_RESET) = 0;
	t_ps = (u_short *)(b_sram + OFFSET_SCB);
	*(t_ps) = (u_short)0x5a5a;
	if (*(t_ps) != (u_short)0x5a5a) {
		kmem_free(kernel_map, b_prom, len);
		kmem_free(kernel_map, b_sram, sram_len);
		return(0);
	}
        t_ps = (u_short *)(b_prom +  + OFFSET_PROM);
#define ETHER0 0x00
#define ETHER1 0xaa
#define ETHER2 0x00
	if ((t_ps[0]&0xff) == ETHER0 &&
	    (t_ps[1]&0xff) == ETHER1 &&
	    (t_ps[2]&0xff) == ETHER2)
	    	pc_softc[unit].seated = TRUE;
#undef	ETHER0
#undef	ETHER1
#undef	ETHER2
#define ETHER0 0x00
#define ETHER1 0x00
#define ETHER2 0x1c
	if ((t_ps[0]&0xff) == ETHER0 ||
	    (t_ps[1]&0xff) == ETHER1 ||
	    (t_ps[2]&0xff) == ETHER2)
	    	pc_softc[unit].seated = TRUE;
#undef	ETHER0
#undef	ETHER1
#undef	ETHER2
	if (pc_softc[unit].seated != TRUE) {
		kmem_free(kernel_map, b_prom, len);
		kmem_free(kernel_map, b_sram, sram_len);
		return(0);
	}
	(volatile char *)pc_softc[unit].prom = (volatile char *)b_prom;
	(volatile char *)pc_softc[unit].sram = (volatile char *)b_sram;
	return(1);
}

/*
 * pc586attach:
 *
 *	This function attaches a PC586 board to the "system".  The rest of
 *	runtime structures are initialized here (this routine is called after
 *	a successful probe of the board).  Once the ethernet address is read
 *	and stored, the board's ifnet structure is attached and readied.
 *
 * input	: bus_device structure setup in autoconfig
 * output	: board structs and ifnet is setup
 *
 */
void pc586attach(dev)
	struct bus_device	*dev;
{
	struct	ifnet	*ifp;
	u_char		*addr_p;
	u_short		*b_addr;
	u_char		unit = (u_char)dev->unit;	
	pc_softc_t	*sp = &pc_softc[unit];
	volatile scb_t	*scb_p;

	take_dev_irq(dev);
	printf(", port = %x, spl = %d, pic = %d. ",
		dev->address, dev->sysdep, dev->sysdep1);

	sp->timer = -1;
	sp->flags = 0;
	sp->mode = 0;
	sp->open = 0;
	CMD(RESET, CMD_1, unit);
	{ int i; for (i = 0; i < 1000; i++);	/* 4 clocks at 6Mhz */}
	CMD(RESET, CMD_0, unit);
	b_addr = (u_short *)(sp->prom + OFFSET_PROM);
	addr_p = (u_char *)sp->ds_addr;
	addr_p[0] = b_addr[0];
	addr_p[1] = b_addr[1];
	addr_p[2] = b_addr[2];
	addr_p[3] = b_addr[3];
	addr_p[4] = b_addr[4];
	addr_p[5] = b_addr[5];
	printf("ethernet id [%x:%x:%x:%x:%x:%x]",
		addr_p[0], addr_p[1], addr_p[2],
		addr_p[3], addr_p[4], addr_p[5]);

	scb_p = (volatile scb_t *)(sp->sram + OFFSET_SCB);
	scb_p->scb_crcerrs = 0;			/* initialize counters */
	scb_p->scb_alnerrs = 0;
	scb_p->scb_rscerrs = 0;
	scb_p->scb_ovrnerrs = 0;

	ifp = &(sp->ds_if);
	ifp->if_unit = unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST;
#ifdef	MACH_KERNEL
	ifp->if_header_size = sizeof(struct ether_header);
	ifp->if_header_format = HDR_ETHERNET;
	ifp->if_address_size = 6;
	ifp->if_address = (char *)&sp->ds_addr[0];
	if_init_queues(ifp);
#else	MACH_KERNEL
	ifp->if_name = "pc";
	ifp->if_init = pc586init;
	ifp->if_output = pc586output;
	ifp->if_ioctl = pc586ioctl;
	ifp->if_reset = pc586reset;
	ifp->if_next = NULL;
	if_attach(ifp);
#endif	MACH_KERNEL
}

/*
 * pc586reset:
 *
 *	This routine is in part an entry point for the "if" code.  Since most 
 *	of the actual initialization has already (we hope already) been done
 *	by calling pc586attach().
 *
 * input	: unit number or board number to reset
 * output	: board is reset
 *
 */
pc586reset(unit)
int	unit;
{
	pc_softc[unit].ds_if.if_flags &= ~IFF_RUNNING;
	pc_softc[unit].flags &= ~(DSF_LOCK|DSF_RUNNING);
	return(pc586init(unit));

}

/*
 * pc586init:
 *
 *	Another routine that interfaces the "if" layer to this driver.  
 *	Simply resets the structures that are used by "upper layers".  
 *	As well as calling pc586hwrst that does reset the pc586 board.
 *
 * input	: board number
 * output	: structures (if structs) and board are reset
 *
 */	
pc586init(unit)
int	unit;
{
	struct	ifnet	*ifp;
	int		stat;
	spl_t		oldpri;

	ifp = &(pc_softc[unit].ds_if);
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	if (ifp->if_addrlist == (struct ifaddr *)0) {
		return;
	}
#endif	MACH_KERNEL
	oldpri = SPLNET();
	if ((stat = pc586hwrst(unit)) == TRUE) {
#ifdef	MACH_KERNEL
#undef	HZ
#define HZ hz
#endif	MACH_KERNEL
		timeout(pc586watch, &(ifp->if_unit), 5*HZ);
		pc_softc[unit].timer = 5;

		pc_softc[unit].ds_if.if_flags |= IFF_RUNNING;
		pc_softc[unit].flags |= DSF_RUNNING;
		pc_softc[unit].tbusy = 0;
		pc586start(unit);
#if	DLI
		dli_init();
#endif	DLI
	} else
		printf("pc%d init(): trouble resetting board.\n", unit);
	splx(oldpri);
	return(stat);
}

#ifdef	MACH_KERNEL
/*ARGSUSED*/
pc586open(dev, flag)
	dev_t	dev;
	int	flag;
{
	register int	unit;
	pc_softc_t	*sp;

	unit = minor(dev);	/* XXX */
	if (unit < 0 || unit >= NPC586 || !pc_softc[unit].seated)
	    return (ENXIO);

	pc_softc[unit].ds_if.if_flags |= IFF_UP;
	pc586init(unit);
	return (0);
}
#endif	MACH_KERNEL

/*
 * pc586start:
 *
 *	This is yet another interface routine that simply tries to output a
 *	in an mbuf after a reset.
 *
 * input	: board number
 * output	: stuff sent to board if any there
 *
 */
pc586start(unit)
int	unit;
{
#ifdef	MACH_KERNEL
	io_req_t	m;
#else	MACH_KERNEL
	struct	mbuf		*m;
#endif	MACH_KERNEL
	struct	ifnet		*ifp;
	register pc_softc_t	*is = &pc_softc[unit];
	volatile scb_t		*scb_p = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);

	if (is->tbusy) {
		if (!(scb_p->scb_status & 0x0700)) { /* ! IDLE */
			is->tbusy = 0;
			pc586_cntrs[unit].xmt.busy++;
			/*
			 * This is probably just a race.  The xmt'r is just
			 * became idle but WE have masked interrupts so ...
			 */
			if (xmt_watch) printf("!!");
		} else
			return;
	}

	ifp = &(pc_softc[unit].ds_if);
	IF_DEQUEUE(&ifp->if_snd, m);
#ifdef	MACH_KERNEL
	if (m != 0)
#else	MACH_KERNEL
	if (m != (struct mbuf *)0) 
#endif	MACH_KERNEL
	{
		is->tbusy++;
		pc586_cntrs[unit].xmt.xmt++;
		pc586xmt(unit, m);
	}
	return;
}

/*
 * pc586read:
 *
 *	This routine does the actual copy of data (including ethernet header
 *	structure) from the pc586 to an mbuf chain that will be passed up
 *	to the "if" (network interface) layer.  NOTE:  we currently
 *	don't handle trailer protocols, so if that is needed, it will
 *	(at least in part) be added here.  For simplicities sake, this
 *	routine copies the receive buffers from the board into a local (stack)
 *	buffer until the frame has been copied from the board.  Once in
 *	the local buffer, the contents are copied to an mbuf chain that
 *	is then enqueued onto the appropriate "if" queue.
 *
 * input	: board number, and an frame descriptor pointer
 * output	: the packet is put into an mbuf chain, and passed up
 * assumes	: if any errors occur, packet is "dropped on the floor"
 *
 */
pc586read(unit, fd_p)
int	unit;
fd_t	*fd_p;
{
	register pc_softc_t	*is = &pc_softc[unit];
	register struct ifnet	*ifp = &is->ds_if;
	struct	ether_header	eh;
#ifdef	MACH_KERNEL
	ipc_kmsg_t	new_kmsg;
	struct ether_header *ehp;
	struct packet_header *pkt;
	char 	*dp;
#else	MACH_KERNEL
    	struct	mbuf		*m, *tm;
#endif	MACH_KERNEL
	rbd_t			*rbd_p;
	u_char			*buffer_p;
	u_char			*mb_p;
	u_short			mlen, len, clen;
	u_short			bytes_in_msg, bytes_in_mbuf, bytes;


	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		printf("pc%d read(): board is not running.\n", ifp->if_unit);
		pc586intoff(ifp->if_unit);
	}
	pc586_cntrs[unit].rcv.rcv++;
#ifdef	MACH_KERNEL
	new_kmsg = net_kmsg_get();
	if (new_kmsg == IKM_NULL) {
	    /*
	     * Drop the received packet.
	     */
	    is->ds_if.if_rcvdrops++;

	    /*
	     * not only do we want to return, we need to drop the packet on
	     * the floor to clear the interrupt.
	     */
	    return 1;
	}
	ehp = (struct ether_header *) (&net_kmsg(new_kmsg)->header[0]);
	pkt = (struct packet_header *)(&net_kmsg(new_kmsg)->packet[0]);

	/*
	 * Get ether header.
	 */
	ehp->ether_type = fd_p->length;
	len = sizeof(struct ether_header);
	bcopy16(fd_p->source, ehp->ether_shost, ETHER_ADD_SIZE);
	bcopy16(fd_p->destination, ehp->ether_dhost, ETHER_ADD_SIZE);

	/*
	 * Get packet body.
	 */
	dp = (char *)(pkt + 1);

	rbd_p = (rbd_t *)ram_to_ptr(fd_p->rbd_offset, unit);
	if (rbd_p == 0) {
	    printf("pc%d read(): Invalid buffer\n", unit);
	    if (pc586hwrst(unit) != TRUE) {
		printf("pc%d read(): hwrst trouble.\n", unit);
	    }
	    net_kmsg_put(new_kmsg);
	    return 0;
	}

	do {
	    buffer_p = (u_char *)(pc_softc[unit].sram + rbd_p->buffer_addr);
	    bytes_in_msg = rbd_p->status & RBD_SW_COUNT;
	    bcopy16((u_short *)buffer_p,
		       (u_short *)dp,
		       (bytes_in_msg + 1) & ~1);	/* but we know it's even */
	    len += bytes_in_msg;
	    dp += bytes_in_msg;
	    if (rbd_p->status & RBD_SW_EOF)
		break;
	    rbd_p = (rbd_t *)ram_to_ptr(rbd_p->next_rbd_offset, unit);
	} while ((int) rbd_p);

	pkt->type = ehp->ether_type;
	pkt->length =
		len - sizeof(struct ether_header)
		    + sizeof(struct packet_header);

	/*
	 * Send the packet to the network module.
	 */
	net_packet(ifp, new_kmsg, pkt->length, ethernet_priority(new_kmsg));
	return 1;
#else	MACH_KERNEL
	eh.ether_type = ntohs(fd_p->length);
	bcopy16(fd_p->source, eh.ether_shost, ETHER_ADD_SIZE);
	bcopy16(fd_p->destination, eh.ether_dhost, ETHER_ADD_SIZE);

	if ((rbd_p =(rbd_t *)ram_to_ptr(fd_p->rbd_offset, unit))== (rbd_t *)NULL) {
		printf("pc%d read(): Invalid buffer\n", unit);
		if (pc586hwrst(unit) != TRUE) {
			printf("pc%d read(): hwrst trouble.\n", unit);
		}
		return 0;
	}

	bytes_in_msg = rbd_p->status & RBD_SW_COUNT;
	buffer_p = (u_char *)(pc_softc[unit].sram + rbd_p->buffer_addr);
	MGET(m, M_DONTWAIT, MT_DATA);
	tm = m;
	if (m == (struct mbuf *)0) {
		/*
		 * not only do we want to return, we need to drop the packet on
		 * the floor to clear the interrupt.
 		 *
		 */
		printf("pc%d read(): No mbuf 1st\n", unit);
		if (pc586hwrst(unit) != TRUE) {
			pc586intoff(unit);
			printf("pc%d read(): hwrst trouble.\n", unit);
			pc_softc[unit].timer = 0;
		}
		return 0;
	}
m->m_next = (struct mbuf *) 0;
	m->m_len = MLEN;
	if (bytes_in_msg > 2 * MLEN - sizeof (struct ifnet **)) {
		MCLGET(m);
	}
	/*
	 * first mbuf in the packet must contain a pointer to the
	 * ifnet structure.  other mbufs that follow and make up
	 * the packet do not need this pointer in the mbuf.
 	 *
 	 */
	*(mtod(tm, struct ifnet **)) = ifp;
	mlen = sizeof (struct ifnet **);
	clen = mlen;
	bytes_in_mbuf = m->m_len - sizeof(struct ifnet **);
	mb_p = mtod(tm, u_char *) + sizeof (struct ifnet **);
	bytes = min(bytes_in_mbuf, bytes_in_msg);
	do {
		if (bytes & 1)
			len = bytes + 1;
		else
			len = bytes;
		bcopy16(buffer_p, mb_p, len);
		clen += bytes;
		mlen += bytes;

		if (!(bytes_in_mbuf -= bytes)) {
			MGET(tm->m_next, M_DONTWAIT, MT_DATA);
			tm = tm->m_next;
			if (tm == (struct mbuf *)0) {
				m_freem(m);
				printf("pc%d read(): No mbuf nth\n", unit);
				if (pc586hwrst(unit) != TRUE) {
					pc586intoff(unit);
					printf("pc%d read(): hwrst trouble.\n", unit);
					pc_softc[unit].timer = 0;
				}
				return 0;
			}
			mlen = 0;
			tm->m_len = MLEN;
			bytes_in_mbuf = MLEN;
			mb_p = mtod(tm, u_char *);
		} else
			mb_p += bytes;

		if (!(bytes_in_msg  -= bytes)) {
			if (rbd_p->status & RBD_SW_EOF ||
			    (rbd_p = (rbd_t *)ram_to_ptr(rbd_p->next_rbd_offset, unit)) ==
			     NULL) {
				tm->m_len = mlen;
				break;
			} else {
				bytes_in_msg = rbd_p->status & RBD_SW_COUNT;
				buffer_p = (u_char *)(pc_softc[unit].sram + rbd_p->buffer_addr);
			}
		} else
			buffer_p += bytes;

		bytes = min(bytes_in_mbuf, bytes_in_msg);
	} while(1);
#ifdef	IF_CNTRS
/*	clen -= (sizeof (struct ifnet **)
	clen += 4 /* crc */;
	clen += sizeof (struct ether_header);
	pc586_ein[log_2(clen)]++;
	if (clen < 128) pc586_lin[clen>>3]++;

	if (eh.ether_type == ETHERTYPE_ARP) {
		pc586_arp++;
		if (pc586_narp) {
			pc586_ein[log_2(clen)]--;
			if (clen < 128) pc586_lin[clen>>3]--;
		}
	}
#endif	IF_CNTRS
	/*
	 * received packet is now in a chain of mbuf's.  next step is
	 * to pass the packet upwards.
	 *
	 */
	pc586send_packet_up(m, &eh, is);
	return 1;
#endif	MACH_KERNEL
}

/*
 * Send a packet composed of an mbuf chain to the higher levels
 *
 */
#ifndef	MACH_KERNEL
pc586send_packet_up(m, eh, is)
struct mbuf *m;
struct ether_header *eh;
pc_softc_t *is;
{
	register struct ifqueue	*inq;
	spl_t      		opri;

	switch (eh->ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;
	case ETHERTYPE_ARP:
		arpinput(&is->pc586_ac, m);
		return;
#endif
#ifdef NS
	case ETHERTYPE_NS:
		schednetisr(NETISR_NS);
		inq = &nsintrq;
		break;
#endif
	default:
#if	DLI
		{
			eh.ether_type = htons(eh.ether_type);
			dli_input(m,eh.ether_type,&eh.ether_shost[0],
		  	&de_dlv[ds->ds_if.if_unit], &eh);
		}
#else	DLI
		m_freem(m);
#endif	DLI
		return;
	}
	opri = SPLNET();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		splx(opri);
		m_freem(m);
		return;
	}
	IF_ENQUEUE(inq, m);
	splx(opri);
	return;
}
#endif	MACH_KERNEL

#ifdef	MACH_KERNEL
pc586output(dev, ior)
	dev_t	dev;
	io_req_t ior;
{
	register int	unit;

	unit = minor(dev);	/* XXX */
	if (unit < 0 || unit >= NPC586 || !pc_softc[unit].seated)
	    return (ENXIO);

	return (net_write(&pc_softc[unit].ds_if, pc586start, ior));
}

pc586setinput(dev, receive_port, priority, filter, filter_count)
	dev_t		dev;
	mach_port_t	receive_port;
	int		priority;
	filter_t	filter[];
	unsigned int	filter_count;
{
	register int unit = minor(dev);
	if (unit < 0 || unit >= NPC586 || !pc_softc[unit].seated)
	    return (ENXIO);

	return (net_set_filter(&pc_softc[unit].ds_if,
			receive_port, priority,
			filter, filter_count));
}
#else	MACH_KERNEL
/*
 * pc586output:
 *
 *	This routine is called by the "if" layer to output a packet to
 *	the network.  This code resolves the local ethernet address, and
 *	puts it into the mbuf if there is room.  If not, then a new mbuf
 *	is allocated with the header information and precedes the data
 *	to be transmitted.  The routines that actually transmit the
 *	data (pc586xmt()) expect the ethernet structure to precede
 *	the data in the mbuf.  This information is required by the
 *	82586's transfer command segment, and thus mbuf's cannot
 *	be simply "slammed" out onto the network.
 *
 * input:	ifnet structure pointer, an mbuf with data, and address
 *		to be resolved
 * output:	mbuf is updated to hold enet address, or a new mbuf
 *	  	with the address is added
 *
 */
pc586output(ifp, m0, dst)
struct ifnet	*ifp;
struct mbuf	*m0;
struct sockaddr *dst;
{
	register pc_softc_t		*is = &pc_softc[ifp->if_unit];
	register struct mbuf		*m = m0;
	int 				type, error;
	spl_t				opri;
 	u_char				edst[6];
	struct in_addr			idst;
	register struct ether_header	*eh;
	register int			off;
	int				usetrailers;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		printf("pc%d output(): board is not running.\n", ifp->if_unit);
		pc586intoff(ifp->if_unit);
		error = ENETDOWN;
		goto bad;
	}
	switch (dst->sa_family) {

#ifdef INET
	case AF_INET:
		idst = ((struct sockaddr_in *)dst)->sin_addr;
 		if (!arpresolve(&is->pc586_ac, m, &idst, edst, &usetrailers)){
			return (0);	/* if not yet resolved */
		}
		off = ntohs((u_short)mtod(m, struct ip *)->ip_len) - m->m_len;

		if (usetrailers && off > 0 && (off & 0x1ff) == 0 &&
		    m->m_off >= MMINOFF + 2 * sizeof (u_short)) {
			type = ETHERTYPE_TRAIL + (off>>9);
			m->m_off -= 2 * sizeof (u_short);
			m->m_len += 2 * sizeof (u_short);
			*mtod(m, u_short *) = htons((u_short)ETHERTYPE_IP);
			*(mtod(m, u_short *) + 1) = htons((u_short)m->m_len);
			goto gottrailertype;
		}
		type = ETHERTYPE_IP;
		off = 0;
		goto gottype;
#endif
#ifdef NS
	case AF_NS:
		type = ETHERTYPE_NS;
 		bcopy((caddr_t)&(((struct sockaddr_ns *)dst)->sns_addr.x_host),
		(caddr_t)edst, sizeof (edst));
		off = 0;
		goto gottype;
#endif

#if	DLI
	case AF_DLI:
		if (m->m_len < sizeof(struct ether_header))
		{
			error = EMSGSIZE;
			goto bad;
		}
		eh = mtod(m, struct ether_header *);
 		bcopy(dst->sa_data, (caddr_t)eh->ether_dhost, 
						sizeof (eh->ether_dhost));
		goto gotheader;
#endif	DLI

	case AF_UNSPEC:
		eh = (struct ether_header *)dst->sa_data;
 		bcopy((caddr_t)eh->ether_dhost, (caddr_t)edst, sizeof (edst));
		type = eh->ether_type;
		goto gottype;

	default:
		printf("pc%d output(): can't handle af%d\n",
			ifp->if_unit, dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

gottrailertype:
	/*
	 * Packet to be sent as trailer: move first packet
	 * (control information) to end of chain.
	 */
	while (m->m_next)
		m = m->m_next;
	m->m_next = m0;
	m = m0->m_next;
	m0->m_next = 0;
	m0 = m;

gottype:
	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	if (m->m_off > MMAXOFF ||
	    MMINOFF + sizeof (struct ether_header) > m->m_off) {
		m = m_get(M_DONTWAIT, MT_HEADER);
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
		m->m_next = m0;
		m->m_off = MMINOFF;
		m->m_len = sizeof (struct ether_header);
	} else {
		m->m_off -= sizeof (struct ether_header);
		m->m_len += sizeof (struct ether_header);
	}
	eh = mtod(m, struct ether_header *);
	eh->ether_type = htons((u_short)type);
 	bcopy((caddr_t)edst, (caddr_t)eh->ether_dhost, sizeof (edst));
 	bcopy((caddr_t)is->ds_addr,(caddr_t)eh->ether_shost, sizeof(edst));
#if	DLI
gotheader:
#endif	DLI

	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	opri = SPLNET();
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		splx(opri);
		m_freem(m);
		return (ENOBUFS);
	}
	IF_ENQUEUE(&ifp->if_snd, m);
	/*
 	 * Some action needs to be added here for checking whether the
	 * board is already transmitting.  If it is, we don't want to
 	 * start it up (ie call pc586start()).  We will attempt to send
 	 * packets that are queued up after an interrupt occurs.  Some
 	 * flag checking action has to happen here and/or in the start
 	 * routine.  This note is here to remind me that some thought
 	 * is needed and there is a potential problem here.
	 *
	 */
	pc586start(ifp->if_unit);
	splx(opri);
	return (0);
bad:
	m_freem(m0);
	return (error);
}
#endif	MACH_KERNEL

#ifdef	MACH_KERNEL
pc586getstat(dev, flavor, status, count)
	dev_t	dev;
	int	flavor;
	dev_status_t	status;		/* pointer to OUT array */
	unsigned int	*count;		/* out */
{
	register int	unit = minor(dev);
	register pc_softc_t	*sp;

	if (unit < 0 || unit >= NPC586 || !pc_softc[unit].seated)
	    return (ENXIO);

	sp = &pc_softc[unit];
	return (net_getstat(&sp->ds_if, flavor, status, count));
}

pc586setstat(dev, flavor, status, count)
	dev_t	dev;
	int	flavor;
	dev_status_t	status;
	unsigned int	count;
{
	register int	unit = minor(dev);
	register pc_softc_t	*sp;

	if (unit < 0 || unit >= NPC586 || !pc_softc[unit].seated)
	    return (ENXIO);

	sp = &pc_softc[unit];

	switch (flavor) {
	    case NET_STATUS:
	    {
		/*
		 * All we can change are flags, and not many of those.
		 */
		register struct net_status *ns = (struct net_status *)status;
		int	mode = 0;

		if (count < NET_STATUS_COUNT)
		    return (D_INVALID_OPERATION);

		if (ns->flags & IFF_ALLMULTI)
		    mode |= MOD_ENAL;
		if (ns->flags & IFF_PROMISC)
		    mode |= MOD_PROM;

		/*
		 * Force a complete reset if the receive mode changes
		 * so that these take effect immediately.
		 */
		if (sp->mode != mode) {
		    sp->mode = mode;
		    if (sp->flags & DSF_RUNNING) {
			sp->flags &= ~(DSF_LOCK|DSF_RUNNING);
			pc586init(unit);
		    }
		}
		break;
	    }

	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);

}
#else	MACH_KERNEL
/*
 * pc586ioctl:
 *
 *	This routine processes an ioctl request from the "if" layer
 *	above.
 *
 * input	: pointer the appropriate "if" struct, command, and data
 * output	: based on command appropriate action is taken on the
 *	 	  pc586 board(s) or related structures
 * return	: error is returned containing exit conditions
 *
 */
pc586ioctl(ifp, cmd, data)
struct ifnet	*ifp;
int	cmd;
caddr_t	data;
{
	register struct ifaddr	*ifa = (struct ifaddr *)data;
	int			unit = ifp->if_unit;
	register pc_softc_t	*is = &pc_softc[unit];
	short			mode = 0;
	int			error = 0;
	spl_t			opri;

 	opri = SPLNET();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		pc586init(unit);
		switch (ifa->ifa_addr.sa_family) {
#ifdef INET
		case AF_INET:
			((struct arpcom *)ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		case AF_NS:
    			{
			register struct ns_addr *ina = 
			&(IA_SNS(ifa)->sns_addr);
			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)(ds->ds_addr);
			else
				pc586setaddr(ina->x_host.c_host, unit);
			break;
    			}
#endif
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_ALLMULTI)
			mode |= MOD_ENAL;
		if (ifp->if_flags & IFF_PROMISC)
			mode |= MOD_PROM;
		/*
		 * force a complete reset if the receive multicast/
		 * promiscuous mode changes so that these take 
		 * effect immediately.
		 *
		 */
		if (is->mode != mode) {
			is->mode = mode;
			if (is->flags & DSF_RUNNING) {
		    		is->flags &= ~(DSF_LOCK|DSF_RUNNING);
				pc586init(unit);
			}
		}
		if ((ifp->if_flags & IFF_UP) == 0 && is->flags & DSF_RUNNING) {
			printf("pc%d ioctl(): board is not running\n", unit);
			is->flags &= ~(DSF_LOCK | DSF_RUNNING);
			is->timer = -1;
			pc586intoff(unit);
		} else if (ifp->if_flags & IFF_UP && (is->flags & DSF_RUNNING) == 0) {
			pc586init(unit);
		}
		break;
#ifdef	IF_CNTRS
	case SIOCCIFCNTRS:
		if (!suser()) {
			error = EPERM;
			break;
		}
		bzero((caddr_t)pc586_ein, sizeof (pc586_ein));
		bzero((caddr_t)pc586_eout, sizeof (pc586_eout));
		bzero((caddr_t)pc586_lin, sizeof (pc586_lin));
		bzero((caddr_t)pc586_lout, sizeof (pc586_lout));
		bzero((caddr_t)&pc586_arp, sizeof (int));
		bzero((caddr_t)&pc586_cntrs, sizeof (pc586_cntrs));
		break;
#endif	IF_CNTRS
	default:
		error = EINVAL;
	}
	splx(opri);
	return (error);
}
#endif	MACH_KERNEL

/*
 * pc586hwrst:
 *
 *	This routine resets the pc586 board that corresponds to the 
 *	board number passed in.
 *
 * input	: board number to do a hardware reset
 * output	: board is reset
 *
 */
pc586hwrst(unit)
int unit;
{
	CMD(CHANATT, CMD_0, unit);
	CMD(RESET, CMD_1, unit);
	{ int i; for (i = 0; i < 1000; i++);	/* 4 clocks at 6Mhz */}
	CMD(RESET,CMD_0, unit);

/*
 *	for (i = 0; i < 1000000; i++); 
 *	with this loop above and with the reset toggle also looping to
 *	1000000.  We don't see the reset behaving as advertised.  DOES
 *	IT HAPPEN AT ALL.  In particular, NORMODE, ENABLE, and XFER
 *	should all be zero and they have not changed at all.
 */
	CMD(INTENAB, CMD_0, unit);
	CMD(NORMMODE, CMD_0, unit);
	CMD(XFERMODE, CMD_1, unit);

	pc586bldcu(unit);

	if (pc586diag(unit) == FALSE)
		return(FALSE);

	if (pc586config(unit) == FALSE)
		return(FALSE);
	/* 
	 * insert code for loopback test here
	 *
	 */
	pc586rustrt(unit);

	pc586inton(unit);
	CMD(NORMMODE, CMD_1, unit);
	return(TRUE);
}

/*
 * pc586watch():
 *
 *	This routine is the watchdog timer routine for the pc586 chip.  If
 *	chip wedges, this routine will fire and cause a board reset and
 *	begin again.
 *
 * input	: which board is timing out
 * output	: potential board reset if wedged
 *
 */
int watch_dead = 0;
pc586watch(b_ptr)
caddr_t	b_ptr;
{
	spl_t	opri;
	int	unit = *b_ptr;

	if ((pc_softc[unit].ds_if.if_flags & IFF_UP) == 0)  {
		return;
	}
	if (pc_softc[unit].timer == -1) {
		timeout(pc586watch, b_ptr, 5*HZ);
		return;
	}
	if (--pc_softc[unit].timer != -1) {
		timeout(pc586watch, b_ptr, 1*HZ);
		return;
	}

	opri = SPLNET();
#ifdef	notdef
	printf("pc%d watch(): 6sec timeout no %d\n", unit, ++watch_dead);
#endif	notdef
	pc586_cntrs[unit].watch++;
	if (pc586hwrst(unit) != TRUE) {
		printf("pc%d watch(): hwrst trouble.\n", unit);
		pc_softc[unit].timer = 0;
	} else {
		timeout(pc586watch, b_ptr, 1*HZ);
		pc_softc[unit].timer = 5;
	}
	splx(opri);
}

/*
 * pc586intr:
 *
 *	This function is the interrupt handler for the pc586 ethernet
 *	board.  This routine will be called whenever either a packet
 *	is received, or a packet has successfully been transfered and
 *	the unit is ready to transmit another packet.
 *
 * input	: board number that interrupted
 * output	: either a packet is received, or a packet is transfered
 *
 */
pc586intr(unit)
int unit;
{
	volatile scb_t	*scb_p = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	volatile ac_t	*cb_p  = (volatile ac_t *)(pc_softc[unit].sram + OFFSET_CU);
	int		next, x;
	int		i;
	u_short		int_type;

	if (pc_softc[unit].seated == FALSE) { 
		printf("pc%d intr(): board not seated\n", unit);
		return(-1);
	}

	while ((int_type = (scb_p->scb_status & SCB_SW_INT)) != 0) {
		pc586ack(unit);
		if (int_type & SCB_SW_FR) {
			pc586rcv(unit);
			watch_dead=0;
		}
		if (int_type & SCB_SW_RNR) {
			pc586_cntrs[unit].rcv.ovw++;
#ifdef	notdef
			printf("pc%d intr(): receiver overrun! begin_fd = %x\n",
				unit, pc_softc[unit].begin_fd);
#endif	notdef
			pc586rustrt(unit);
		}
		if (int_type & SCB_SW_CNA) {
			/*
			 * At present, we don't care about CNA's.  We
			 * believe they are a side effect of XMT.
			 */
		}
		if (int_type & SCB_SW_CX) {
			/*
			 * At present, we only request Interrupt for
			 * XMT.
			 */
			if ((!(cb_p->ac_status & AC_SW_OK)) ||
			    (cb_p->ac_status & (0xfff^TC_SQE))) {
				if (cb_p->ac_status & TC_DEFER) {
					if (xmt_watch) printf("DF");
					pc586_cntrs[unit].xmt.defer++;
				} else if (cb_p->ac_status & (TC_COLLISION|0xf)) {
					if (xmt_watch) printf("%x",cb_p->ac_status & 0xf);
				} else if (xmt_watch) 
					printf("pc%d XMT: %x %x\n",
						unit, cb_p->ac_status, cb_p->ac_command);
			}
			pc586_cntrs[unit].xmt.xmti++;
			pc_softc[unit].tbusy = 0;
			pc586start(unit);
		}
		pc_softc[unit].timer = 5;
	}
	return(0);
}

/*
 * pc586rcv:
 *
 *	This routine is called by the interrupt handler to initiate a
 *	packet transfer from the board to the "if" layer above this
 *	driver.  This routine checks if a buffer has been successfully
 *	received by the pc586.  If so, the routine pc586read is called
 *	to do the actual transfer of the board data (including the
 *	ethernet header) into a packet (consisting of an mbuf chain).
 *
 * input	: number of the board to check
 * output	: if a packet is available, it is "sent up"
 *
 */
pc586rcv(unit)
int	unit;
{
	fd_t	*fd_p;

	for (fd_p = pc_softc[unit].begin_fd; fd_p != (fd_t *)NULL;
	     fd_p = pc_softc[unit].begin_fd) {
		if (fd_p->status == 0xffff || fd_p->rbd_offset == 0xffff) {
			if (pc586hwrst(unit) != TRUE)
				printf("pc%d rcv(): hwrst ffff trouble.\n",
					unit);
			return;
		} else if (fd_p->status & AC_SW_C) {
			fd_t *bfd = (fd_t *)ram_to_ptr(fd_p->link_offset, unit);

			if (fd_p->status == (RFD_DONE|RFD_RSC)) {
					/* lost one */;
#ifdef	notdef
				printf("pc%d RCV: RSC %x\n",
					unit, fd_p->status);
#endif	notdef
				pc586_cntrs[unit].rcv.partial++;
			} else if (!(fd_p->status & RFD_OK))
				printf("pc%d RCV: !OK %x\n",
					unit, fd_p->status);
			else if (fd_p->status & 0xfff)
				printf("pc%d RCV: ERRs %x\n",
					unit, fd_p->status);
			else
				if (!pc586read(unit, fd_p))
					return;
			if (!pc586requeue(unit, fd_p)) {	/* abort on chain error */
				if (pc586hwrst(unit) != TRUE)
					printf("pc%d rcv(): hwrst trouble.\n", unit);
				return;
			}
			pc_softc[unit].begin_fd = bfd;
		} else
			break;
	}
	return;
}

/*
 * pc586requeue:
 *
 *	This routine puts rbd's used in the last receive back onto the
 *	free list for the next receive.
 *
 */
pc586requeue(unit, fd_p)
int	unit;
fd_t	*fd_p;
{
	rbd_t	*l_rbdp;
	rbd_t	*f_rbdp;

#ifndef	REQUEUE_DBG
	if (bad_rbd_chain(fd_p->rbd_offset, unit))
		return 0;
#endif	REQUEUE_DBG
	f_rbdp = (rbd_t *)ram_to_ptr(fd_p->rbd_offset, unit);
	if (f_rbdp != NULL) {
		l_rbdp = f_rbdp;
		while ( (!(l_rbdp->status & RBD_SW_EOF)) && 
			(l_rbdp->next_rbd_offset != 0xffff)) 
		{
			l_rbdp->status = 0;
		   	l_rbdp = (rbd_t *)ram_to_ptr(l_rbdp->next_rbd_offset,
						     unit);
		}
		l_rbdp->next_rbd_offset = PC586NULL;
		l_rbdp->status = 0;
		l_rbdp->size |= AC_CW_EL;
		pc_softc[unit].end_rbd->next_rbd_offset = 
			ptr_to_ram((char *)f_rbdp, unit);
		pc_softc[unit].end_rbd->size &= ~AC_CW_EL;
		pc_softc[unit].end_rbd= l_rbdp;
	}

	fd_p->status = 0;
	fd_p->command = AC_CW_EL;
	fd_p->link_offset = PC586NULL;
	fd_p->rbd_offset = PC586NULL;

	pc_softc[unit].end_fd->link_offset = ptr_to_ram((char *)fd_p, unit);
	pc_softc[unit].end_fd->command = 0;
	pc_softc[unit].end_fd = fd_p;

	return 1;
}

/*
 * pc586xmt:
 *
 *	This routine fills in the appropriate registers and memory
 *	locations on the PC586 board and starts the board off on
 *	the transmit.
 *
 * input	: board number of interest, and a pointer to the mbuf
 * output	: board memory and registers are set for xfer and attention
 *
 */
#ifdef	DEBUG
int xmt_debug = 0;
#endif	DEBUG
pc586xmt(unit, m)
int	unit;
#ifdef	MACH_KERNEL
io_req_t	m;
#else	MACH_KERNEL
struct	mbuf	*m;
#endif	MACH_KERNEL
{
	pc_softc_t			*is = &pc_softc[unit];
	register u_char			*xmtdata_p = (u_char *)(is->sram + OFFSET_TBUF);
	register u_short		*xmtshort_p;
#ifdef	MACH_KERNEL
	register struct ether_header	*eh_p = (struct ether_header *)m->io_data;
#else	MACH_KERNEL
	struct	mbuf			*tm_p = m;
	register struct ether_header	*eh_p = mtod(m, struct ether_header *);
	u_char				*mb_p = mtod(m, u_char *) + sizeof(struct ether_header);
	u_short				count = m->m_len - sizeof(struct ether_header);
#endif	MACH_KERNEL
	volatile scb_t			*scb_p = (volatile scb_t *)(is->sram + OFFSET_SCB);
	volatile ac_t			*cb_p = (volatile ac_t *)(is->sram + OFFSET_CU);
	tbd_t				*tbd_p = (tbd_t *)(is->sram + OFFSET_TBD);
	u_short				tbd = OFFSET_TBD;
	u_short				len, clen = 0;

	cb_p->ac_status = 0;
	cb_p->ac_command = (AC_CW_EL|AC_TRANSMIT|AC_CW_I);
	cb_p->ac_link_offset = PC586NULL;
	cb_p->cmd.transmit.tbd_offset = OFFSET_TBD;

	bcopy16(eh_p->ether_dhost, cb_p->cmd.transmit.dest_addr, ETHER_ADD_SIZE);
	cb_p->cmd.transmit.length = (u_short)(eh_p->ether_type);

#ifndef	MACH_KERNEL
#ifdef	DEBUG
	if (xmt_debug)
		printf("XMT    mbuf: L%d @%x ", count, mb_p);
#endif	DEBUG
#endif	MACH_KERNEL
	tbd_p->act_count = 0;
	tbd_p->buffer_base = 0;
	tbd_p->buffer_addr = ptr_to_ram(xmtdata_p, unit);
#ifdef	MACH_KERNEL
	{ int Rlen, Llen;
	    clen = m->io_count - sizeof(struct ether_header);
	    Llen = clen & 1;
	    Rlen = ((int)(m->io_data + sizeof(struct ether_header))) & 1;

	    bcopy16(m->io_data + sizeof(struct ether_header) - Rlen,
		    xmtdata_p,
		    clen + (Rlen + Llen) );
	    xmtdata_p += clen + Llen;
	    tbd_p->act_count = clen;
	    tbd_p->buffer_addr += Rlen;
	}
#else	MACH_KERNEL
	do {
		if (count) {
			if (clen + count > ETHERMTU)
				break;
			if (count & 1)
				len = count + 1;
			else
				len = count;
			bcopy16(mb_p, xmtdata_p, len);
			clen += count;
			tbd_p->act_count += count;
			xmtdata_p += len;
			if ((tm_p = tm_p->m_next) == (struct mbuf *)0)
				break;
			if (count & 1) {
				/* go to the next descriptor */
				tbd_p++->next_tbd_offset = (tbd += sizeof (tbd_t));
				tbd_p->act_count = 0;
				tbd_p->buffer_base = 0;
				tbd_p->buffer_addr = ptr_to_ram(xmtdata_p, unit);
				/* at the end -> coallesce remaining mbufs */
				if (tbd == OFFSET_TBD + (N_TBD-1) * sizeof (tbd_t)) {
					pc586sftwsleaze(&count, &mb_p, &tm_p, unit);
					continue;
				}
				/* next mbuf short -> coallesce as needed */
				if ( (tm_p->m_next == (struct mbuf *) 0) ||
#define HDW_THRESHOLD 55
				      tm_p->m_len > HDW_THRESHOLD)
				      	/* ok */;
				else {
					pc586hdwsleaze(&count, &mb_p, &tm_p, unit);
					continue;
				}
			}
		} else if ((tm_p = tm_p->m_next) == (struct mbuf *)0)
			break;
		count = tm_p->m_len;
		mb_p = mtod(tm_p, u_char *);
#ifdef	DEBUG
		if (xmt_debug)
			printf("mbuf+ L%d @%x ", count, mb_p);
#endif	DEBUG
	} while (1);
#endif	MACH_KERNEL
#ifdef	DEBUG
	if (xmt_debug)
		printf("CLEN = %d\n", clen);
#endif	DEBUG
	if (clen < ETHERMIN) {
		tbd_p->act_count += ETHERMIN - clen;
		for (xmtshort_p = (u_short *)xmtdata_p;
		     clen < ETHERMIN;
		     clen += 2) *xmtshort_p++ = 0;
	}
	tbd_p->act_count |= TBD_SW_EOF;
	tbd_p->next_tbd_offset = PC586NULL;
#ifdef	IF_CNTRS
	clen += sizeof (struct ether_header) + 4 /* crc */;
	pc586_eout[log_2(clen)]++;
	if (clen < 128)  pc586_lout[clen>>3]++;
#endif	IF_CNTRS
#ifdef	DEBUG
	if (xmt_debug) {
		pc586tbd(unit);
		printf("\n");
	}
#endif	DEBUG

	while (scb_p->scb_command) ;
	scb_p->scb_command = SCB_CU_STRT;
	pc586chatt(unit);

#ifdef	MACH_KERNEL
	iodone(m);
#else	MACH_KERNEL
	for (count=0; ((count < 6) && (eh_p->ether_dhost[count] == 0xff)); count++)  ;
	if (count == 6) {
		pc586send_packet_up(m, eh_p, is);
	} else
		m_freem(m);
#endif	MACH_KERNEL
	return;
}

/*
 * pc586bldcu:
 *
 *	This function builds up the command unit structures.  It inits
 *	the scp, iscp, scb, cb, tbd, and tbuf.
 *
 */
pc586bldcu(unit)
{
	char		*sram = pc_softc[unit].sram;
	scp_t		*scp_p = (scp_t *)(sram + OFFSET_SCP);
	iscp_t		*iscp_p = (iscp_t *)(sram + OFFSET_ISCP);
	volatile scb_t	*scb_p = (volatile scb_t *)(sram + OFFSET_SCB);
	volatile ac_t	*cb_p = (volatile ac_t *)(sram + OFFSET_CU);
	tbd_t		*tbd_p = (tbd_t *)(sram + OFFSET_TBD);
	int		i;

	scp_p->scp_sysbus = 0;
	scp_p->scp_iscp = OFFSET_ISCP;
	scp_p->scp_iscp_base = 0;

	iscp_p->iscp_busy = 1;
	iscp_p->iscp_scb_offset = OFFSET_SCB;
	iscp_p->iscp_scb = 0;
	iscp_p->iscp_scb_base = 0;

	pc586_cntrs[unit].rcv.crc += scb_p->scb_crcerrs;
	pc586_cntrs[unit].rcv.frame += scb_p->scb_alnerrs;
	pc586_cntrs[unit].rcv.rscerrs += scb_p->scb_rscerrs;
	pc586_cntrs[unit].rcv.ovrnerrs += scb_p->scb_ovrnerrs;
	scb_p->scb_status = 0;
	scb_p->scb_command = 0;
	scb_p->scb_cbl_offset = OFFSET_CU;
	scb_p->scb_rfa_offset = OFFSET_RU;
	scb_p->scb_crcerrs = 0;
	scb_p->scb_alnerrs = 0;
	scb_p->scb_rscerrs = 0;
	scb_p->scb_ovrnerrs = 0;

	scb_p->scb_command = SCB_RESET;
	pc586chatt(unit);
	for (i = 1000000; iscp_p->iscp_busy && (i-- > 0); );
	if (!i) printf("pc%d bldcu(): iscp_busy timeout.\n", unit);
	for (i = STATUS_TRIES; i-- > 0; ) {
		if (scb_p->scb_status == (SCB_SW_CX|SCB_SW_CNA)) 
			break;
	}
	if (!i)
		printf("pc%d bldcu(): not ready after reset.\n", unit);
	pc586ack(unit);

	cb_p->ac_status = 0;
	cb_p->ac_command = AC_CW_EL;
	cb_p->ac_link_offset = OFFSET_CU;

	tbd_p->act_count = 0;
	tbd_p->next_tbd_offset = PC586NULL;
	tbd_p->buffer_addr = 0;
	tbd_p->buffer_base = 0;
	return;
}

/*
 * pc586bldru:
 *
 *	This function builds the linear linked lists of fd's and
 *	rbd's.  Based on page 4-32 of 1986 Intel microcom handbook.
 *
 */
char *
pc586bldru(unit)
int unit;
{
	fd_t	*fd_p = (fd_t *)(pc_softc[unit].sram + OFFSET_RU);
	ru_t	*rbd_p = (ru_t *)(pc_softc[unit].sram + OFFSET_RBD);
	int 	i;

	pc_softc[unit].begin_fd = fd_p;
	for(i = 0; i < N_FD; i++, fd_p++) {
		fd_p->status = 0;
		fd_p->command	= 0;
		fd_p->link_offset = ptr_to_ram((char *)(fd_p + 1), unit);
		fd_p->rbd_offset = PC586NULL;
	}
	pc_softc[unit].end_fd = --fd_p;
	fd_p->link_offset = PC586NULL;
	fd_p->command = AC_CW_EL;
	fd_p = (fd_t *)(pc_softc[unit].sram + OFFSET_RU);

	fd_p->rbd_offset = ptr_to_ram((char *)rbd_p, unit);
	for(i = 0; i < N_RBD; i++, rbd_p = (ru_t *) &(rbd_p->rbuffer[RCVBUFSIZE])) {
		rbd_p->r.status = 0;
		rbd_p->r.buffer_addr = ptr_to_ram((char *)(rbd_p->rbuffer),
					    	   unit);
		rbd_p->r.buffer_base = 0;
		rbd_p->r.size = RCVBUFSIZE;
		if (i != N_RBD-1) {
			rbd_p->r.next_rbd_offset=ptr_to_ram(&(rbd_p->rbuffer[RCVBUFSIZE]),
							    unit);
		} else {
			rbd_p->r.next_rbd_offset = PC586NULL;
			rbd_p->r.size |= AC_CW_EL;
			pc_softc[unit].end_rbd = (rbd_t *)rbd_p;
		}
	}
	return (char *)pc_softc[unit].begin_fd;
}

/*
 * pc586rustrt:
 *
 *	This routine starts the receive unit running.  First checks if the
 *	board is actually ready, then the board is instructed to receive
 *	packets again.
 *
 */
pc586rustrt(unit)
int unit;
{
	volatile scb_t	*scb_p = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	char		*strt;

	if ((scb_p->scb_status & SCB_RUS_READY) == SCB_RUS_READY)
		return;

	strt = pc586bldru(unit);
	scb_p->scb_command = SCB_RU_STRT;
	scb_p->scb_rfa_offset = ptr_to_ram(strt, unit);
	pc586chatt(unit);
	return;
}

/*
 * pc586diag:
 *
 *	This routine does a 586 op-code number 7, and obtains the
 *	diagnose status for the pc586.
 *
 */
pc586diag(unit)
int unit;
{
	volatile scb_t	*scb_p = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	volatile ac_t	*cb_p  = (volatile ac_t *)(pc_softc[unit].sram + OFFSET_CU);
	int		i;

	if (scb_p->scb_status & SCB_SW_INT) {
		printf("pc%d diag(): bad initial state %\n",
			unit, scb_p->scb_status);
		pc586ack(unit);
	}
	cb_p->ac_status	= 0;
	cb_p->ac_command = (AC_DIAGNOSE|AC_CW_EL);
	scb_p->scb_command = SCB_CU_STRT;
	pc586chatt(unit);

	for(i = 0; i < 0xffff; i++)
		if ((cb_p->ac_status & AC_SW_C))
			break;
	if (i == 0xffff || !(cb_p->ac_status & AC_SW_OK)) {
		printf("pc%d: diag failed; status = %x\n",
			unit, cb_p->ac_status);
		return(FALSE);
	}

	if ( (scb_p->scb_status & SCB_SW_INT) && (scb_p->scb_status != SCB_SW_CNA) )  {
		printf("pc%d diag(): bad final state %x\n",
			unit, scb_p->scb_status);
		pc586ack(unit);
	}
	return(TRUE);
}

/*
 * pc586config:
 *
 *	This routine does a standard config of the pc586 board.
 *
 */
pc586config(unit)
int unit;
{
	volatile scb_t	*scb_p	= (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	volatile ac_t	*cb_p	= (volatile ac_t *)(pc_softc[unit].sram + OFFSET_CU);
	int 		i;


/*
	if ((scb_p->scb_status != SCB_SW_CNA) && (scb_p->scb_status & SCB_SW_INT) ) {
		printf("pc%d config(): unexpected initial state %x\n",
			unit, scb_p->scb_status);
	}
*/
	pc586ack(unit);

	cb_p->ac_status	= 0;
	cb_p->ac_command = (AC_CONFIGURE|AC_CW_EL);

	/*
	 * below is the default board configuration from p2-28 from 586 book
	 */
	cb_p->cmd.configure.fifolim_bytecnt 	= 0x080c;
	cb_p->cmd.configure.addrlen_mode  	= 0x2600;
	cb_p->cmd.configure.linprio_interframe	= 0x6000;
	cb_p->cmd.configure.slot_time      	= 0xf200;
	cb_p->cmd.configure.hardware     	= 0x0000;
	cb_p->cmd.configure.min_frame_len   	= 0x0040;

	scb_p->scb_command = SCB_CU_STRT;
	pc586chatt(unit);

	for(i = 0; i < 0xffff; i++)
		if ((cb_p->ac_status & AC_SW_C))
			break;
	if (i == 0xffff || !(cb_p->ac_status & AC_SW_OK)) {
		printf("pc%d: config-configure failed; status = %x\n",
			unit, cb_p->ac_status);
		return(FALSE);
	}
/*
	if (scb_p->scb_status & SCB_SW_INT) {
		printf("pc%d configure(): bad configure state %x\n",
			unit, scb_p->scb_status);
		pc586ack(unit);
	}
*/
	cb_p->ac_status = 0;
	cb_p->ac_command = (AC_IASETUP|AC_CW_EL);

	bcopy16(pc_softc[unit].ds_addr, cb_p->cmd.iasetup, ETHER_ADD_SIZE);

	scb_p->scb_command = SCB_CU_STRT;
	pc586chatt(unit);

	for (i = 0; i < 0xffff; i++)
		if ((cb_p->ac_status & AC_SW_C))
			break;
	if (i == 0xffff || !(cb_p->ac_status & AC_SW_OK)) {
		printf("pc%d: config-address failed; status = %x\n",
			unit, cb_p->ac_status);
		return(FALSE);
	}
/*
	if ((scb_p->scb_status & SCB_SW_INT) != SCB_SW_CNA) {
		printf("pc%d configure(): unexpected final state %x\n",
			unit, scb_p->scb_status);
	}
*/
	pc586ack(unit);

	return(TRUE);
}

/*
 * pc586ack:
 */
pc586ack(unit)
{
	volatile scb_t	*scb_p = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	int i;

	if (!(scb_p->scb_command = scb_p->scb_status & SCB_SW_INT))
		return;
	CMD(CHANATT, 0x0001, unit);
	for (i = 1000000; scb_p->scb_command && (i-- > 0); );
	if (!i)
		printf("pc%d pc586ack(): board not accepting command.\n", unit);
}

char *
ram_to_ptr(offset, unit)
int unit;
u_short	offset;
{
	if (offset == PC586NULL)
		return(NULL);
	if (offset > 0x3fff) {
		printf("ram_to_ptr(%x, %d)\n", offset, unit);
		panic("range");
		return(NULL);
	}
	return(pc_softc[unit].sram + offset);
}

#ifndef	REQUEUE_DBG
bad_rbd_chain(offset, unit)
{
	rbd_t	*rbdp;
	char	*sram = pc_softc[unit].sram;

	for (;;) {
		if (offset == PC586NULL)
			return 0;
		if (offset > 0x3fff) {
			printf("pc%d: bad_rbd_chain offset = %x\n",
				unit, offset);
			pc586_cntrs[unit].rcv.bad_chain++;
			return 1;
		}

		rbdp = (rbd_t *)(sram + offset);
		offset = rbdp->next_rbd_offset;
	}
}
#endif	REQUEUE_DBG

u_short
ptr_to_ram(k_va, unit)
char	*k_va;
int unit;
{
	return((u_short)(k_va - pc_softc[unit].sram));
}

pc586scb(unit)
{
	volatile scb_t	*scb = (volatile scb_t *)(pc_softc[unit].sram + OFFSET_SCB);
	volatile u_short*cmd = (volatile u_short *)(pc_softc[unit].prom + OFFSET_NORMMODE);
	u_short		 i;

	i = scb->scb_status;
	printf("stat: stat %x, cus %x, rus %x //",
		(i&0xf000)>>12, (i&0x0700)>>8, (i&0x0070)>>4);
	i = scb->scb_command;
	printf(" cmd: ack %x, cuc %x, ruc %x\n",
		(i&0xf000)>>12, (i&0x0700)>>8, (i&0x0070)>>4);

	printf("crc %d[%d], align %d[%d], rsc %d[%d], ovr %d[%d]\n",
		scb->scb_crcerrs, pc586_cntrs[unit].rcv.crc,
		scb->scb_alnerrs, pc586_cntrs[unit].rcv.frame,
		scb->scb_rscerrs, pc586_cntrs[unit].rcv.rscerrs,
		scb->scb_ovrnerrs, pc586_cntrs[unit].rcv.ovrnerrs);

	printf("cbl %x, rfa %x //", scb->scb_cbl_offset, scb->scb_rfa_offset);
	printf(" norm %x, ena %x, xfer %x //",
		cmd[0] & 1, cmd[3] & 1, cmd[4] & 1);
	printf(" atn %x, reset %x, type %x, stat %x\n",
		cmd[1] & 1, cmd[2] & 1, cmd[5] & 1, cmd[6] & 1);
}

pc586tbd(unit)
{
	pc_softc_t	*is = &pc_softc[unit];
	tbd_t		*tbd_p = (tbd_t *)(is->sram + OFFSET_TBD);
	int 		i = 0;
	int		sum = 0;

	do {
		sum += (tbd_p->act_count & ~TBD_SW_EOF);
		printf("%d: addr %x, count %d (%d), next %x, base %x\n",
			i++, tbd_p->buffer_addr,
			(tbd_p->act_count & ~TBD_SW_EOF), sum,
			tbd_p->next_tbd_offset,
			tbd_p->buffer_base);
		if (tbd_p->act_count & TBD_SW_EOF)
			break;
		tbd_p = (tbd_t *)(is->sram + tbd_p->next_tbd_offset);
	} while (1);
}

#ifndef	MACH_KERNEL
pc586hdwsleaze(countp, mb_pp, tm_pp, unit)
struct mbuf **tm_pp;
u_char **mb_pp;
u_short *countp;
{
	struct mbuf	*tm_p = *tm_pp;
	u_char		*mb_p = *mb_pp;
	u_short		count = 0;
	u_char		*cp;
	int		len;

	pc586_cntrs[unit].xmt.sleaze++;
	/*
	 * can we get a run that will be coallesced or
	 * that terminates before breaking
	 */
	do {
		count += tm_p->m_len;
		if (tm_p->m_len & 1)
			break;
	} while ((tm_p = tm_p->m_next) != (struct mbuf *)0);
	if ( (tm_p == (struct mbuf *)0) ||
	      count > HDW_THRESHOLD) {
		*countp = (*tm_pp)->m_len;
		*mb_pp = mtod((*tm_pp), u_char *);
		printf("\n");
		return;
	}

	/* we need to copy */
	pc586_cntrs[unit].xmt.intrinsic++;
	tm_p = *tm_pp;
	mb_p = *mb_pp;
	count = 0;
	cp = (u_char *) t_packet;
	do {
		bcopy(mtod(tm_p, u_char *), cp, len = tm_p->m_len);
		count += len;
		if (count > HDW_THRESHOLD)
			break;
		cp += len;
		if (tm_p->m_next == (struct mbuf *)0)
			break;
		tm_p = tm_p->m_next;
	} while (1);
	pc586_cntrs[unit].xmt.intrinsic_count += count;
	*countp = count;
	*mb_pp = (u_char *) t_packet;
	*tm_pp = tm_p;
	return;
}

pc586sftwsleaze(countp, mb_pp, tm_pp, unit)
struct mbuf **tm_pp;
u_char **mb_pp;
u_short *countp;
{
	struct mbuf	*tm_p = *tm_pp;
	u_char		*mb_p = *mb_pp;
	u_short		count = 0;
	u_char		*cp = (u_char *) t_packet;
	int		len;

	pc586_cntrs[unit].xmt.chain++;
	/* we need to copy */
	do {
		bcopy(mtod(tm_p, u_char *), cp, len = tm_p->m_len);
		count += len;
		cp += len;
		if (tm_p->m_next == (struct mbuf *)0)
			break;
		tm_p = tm_p->m_next;
	} while (1);

	*countp = count;
	*mb_pp = (u_char *) t_packet;
	*tm_pp = tm_p;
	return;
}
#endif	MACH_KERNEL
