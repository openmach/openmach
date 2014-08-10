/* 
 * Copyright (c) 1994 Shantanu Goel
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/*
 * Written 1992,1993 by Donald Becker.
 * 
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.	 This software may be used and
 * distributed according to the terms of the GNU Public License,
 * incorporated herein by reference.
 *
 * The Author may be reached as becker@super.org or
 * C/O Supercomputing Research Ctr., 17100 Science Dr., Bowie MD 20715
 */

#include <ul.h>
#include <wd.h>
#include <hpp.h>
#if NUL > 0 || NWD > 0 || NHPP > 0
/*
 * Generic NS8390 routines.
 * Derived from the Linux driver by Donald Becker.
 *
 * Shantanu Goel (goel@cs.columbia.edu)
 */
#include <sys/types.h>
#include <device/device_types.h>
#include <device/errno.h>
#include <device/io_req.h>
#include <device/if_hdr.h>
#include <device/if_ether.h>
#include <device/net_status.h>
#include <device/net_io.h>
#include <chips/busses.h>
#include <i386/machspl.h>
#include <i386/pio.h>
#include <i386at/gpl/if_nsreg.h>

#define IO_DELAY	__asm__ __volatile__ ("outb %al,$0x80")
#define outb_p(p, v)	{ outb(p, v); IO_DELAY; }
#define inb_p(p)	({ unsigned char _v; _v = inb(p); IO_DELAY; _v; })

#define NSDEBUG
#ifdef NSDEBUG
int	nsdebug = 0;
#define DEBUGF(stmt)	{ if (nsdebug) stmt; }
#else
#define DEBUGF(stmt)
#endif

void	nsxint(struct nssoftc *);
void	nsrint(struct nssoftc *);
void	nsxmit(struct nssoftc *, unsigned, int);
void	nsrxoverrun(struct nssoftc *);

/*
 * Initialize the NIC.
 * Must be called at splimp().
 */
void
nsinit(sc)
	struct nssoftc *sc;
{
	int port = sc->sc_port, i, rxconfig;
	int endcfg = sc->sc_word16 ? (0x48 | ENDCFG_WTS) : 0x48;
	struct ifnet *ifp = &sc->sc_if;

	/*
	 * Reset the board.
	 */
	(*sc->sc_reset)(sc);

	sc->sc_oactive = 0;
	sc->sc_txing = 0;
	sc->sc_timer = 0;
	sc->sc_tx1 = sc->sc_tx2 = 0;
	sc->sc_curpg = sc->sc_rxstrtpg;

	/*
	 * Follow National Semiconductor's recommendations for
	 * initializing the DP83902.
	 */
	outb_p(port, E8390_NODMA+E8390_PAGE0+E8390_STOP);	/* 0x21 */
	outb_p(port + EN0_DCFG, endcfg);	/* 0x48 or 0x49 */

	/*
	 * Clear remote byte count registers.
	 */
	outb_p(port + EN0_RCNTLO, 0);
	outb_p(port + EN0_RCNTHI, 0);

	/*
	 * Set to monitor and loopback mode -- this is vital!
	 */
	outb_p(port + EN0_RXCR, E8390_RXOFF);	/* 0x20 */
	outb_p(port + EN0_TXCR, E8390_TXOFF);	/* 0x02 */

	/*
	 * Set transmit page and receive ring.
	 */
	outb_p(port + EN0_TPSR, sc->sc_txstrtpg);
	outb_p(port + EN0_STARTPG, sc->sc_rxstrtpg);
	outb_p(port + EN0_BOUNDARY, sc->sc_stoppg - 1);
	outb_p(port + EN0_STOPPG, sc->sc_stoppg);

	/*
	 * Clear pending interrupts and mask.
	 */
	outb_p(port + EN0_ISR, 0xff);

	/*
         * Enable the following interrupts: receive/transmit complete,
         * receive/transmit error, and Receiver OverWrite.
         *
         * Counter overflow and Remote DMA complete are *not* enabled.
         */
	outb_p(port + EN0_IMR, ENISR_RX | ENISR_TX | ENISR_RX_ERR |
              ENISR_TX_ERR | ENISR_OVER );

	/*
	 * Copy station address into 8390 registers.
	 */
	outb_p(port, E8390_NODMA + E8390_PAGE1 + E8390_STOP);	/* 0x61 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		outb_p(port + EN1_PHYS + i, sc->sc_addr[i]);

	/*
	 * Set up to accept all multicast packets.
	 */
	for (i = 0; i < 8; i++)
		outb_p(port + EN1_MULT + i, 0xff);

	/*
	 * Initialize CURRent pointer
	 */
	outb_p(port + EN1_CURPAG, sc->sc_rxstrtpg);

	/*
	 * Program command register for page 0.
	 */
	outb_p(port, E8390_NODMA + E8390_PAGE0 + E8390_STOP);

#if 0
	outb_p(port + EN0_ISR, 0xff);
	outb_p(port + EN0_IMR, ENISR_ALL);
#endif

	outb_p(port + E8390_CMD, E8390_NODMA + E8390_PAGE0 + E8390_START);
	outb_p(port + EN0_TXCR, E8390_TXCONFIG);	/* xmit on */

	/* 3c503 TechMan says rxconfig only after the NIC is started. */
	rxconfig = E8390_RXCONFIG;
	if (ifp->if_flags & IFF_ALLMULTI)
		rxconfig |= 0x08;
	if (ifp->if_flags & IFF_PROMISC)
		rxconfig |= 0x10;
	outb_p(port + EN0_RXCR, rxconfig);	/* rx on */

	/*
	 * Mark interface as up and start output.
	 */
	ifp->if_flags |= IFF_RUNNING;
	nsstart(sc);
}

/*
 * Start output on interface.
 * Must be called at splimp().
 */
void
nsstart(sc)
	struct nssoftc *sc;
{
	io_req_t ior;
	struct ifnet *ifp = &sc->sc_if;

	/*
	 * Drop packets if interface is down.
	 */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		while (1) {
			IF_DEQUEUE(&ifp->if_snd, ior);
			if (ior == 0)
				return;
			iodone(ior);
		}
	}
	/*
	 * If transmitter is busy, bail out.
	 */
	if (sc->sc_oactive)
		return;

	/*
	 * Dequeue a packet.
	 */
	IF_DEQUEUE(&ifp->if_snd, ior);
	if (ior == 0)
		return;

	/* Mask interrupts from the ethercard. */
    	outb( sc->sc_port + EN0_IMR, 0x00);

	if (sc->sc_pingpong) {
		int count, output_page;

		if (sc->sc_tx1 == 0) {
			output_page = sc->sc_txstrtpg;
			sc->sc_tx1 = count = (*sc->sc_output)(sc,
							      ior->io_count,
							      ior->io_data,
							      sc->sc_txstrtpg);
		} else if (sc->sc_tx2 == 0) {
			output_page = sc->sc_txstrtpg + 6;
			sc->sc_tx2 = count = (*sc->sc_output)(sc,
							      ior->io_count,
							      ior->io_data,
							      output_page);
		} else {
			sc->sc_oactive = 1;
			IF_PREPEND(&ifp->if_snd, ior);
			return;
		}

		DEBUGF({
			struct ether_header *eh;

			eh =  (struct ether_header *)ior->io_data;
			printf("send: %s%d: %x:%x:%x:%x:%x:%x, "
			       "olen %d, len %d\n",
			       sc->sc_name, sc->sc_unit,
			       eh->ether_dhost[0], eh->ether_dhost[1],
			       eh->ether_dhost[2], eh->ether_dhost[3],
			       eh->ether_dhost[4], eh->ether_dhost[5],
			       ior->io_count, count);
		});

		if (!sc->sc_txing) {
			nsxmit(sc, count, output_page);
			if (output_page == sc->sc_txstrtpg)
				sc->sc_tx1 = -1, sc->sc_lasttx = -1;
			else
				sc->sc_tx2 = -1, sc->sc_lasttx = -2;
		}
		sc->sc_oactive = (sc->sc_tx1 && sc->sc_tx2);
	} else {
		int count;

		count = (*sc->sc_output)(sc, ior->io_count,
					 ior->io_data, sc->sc_txstrtpg);

		DEBUGF({
			struct ether_header *eh;

			eh =  (struct ether_header *)ior->io_data;
			printf("send: %s%d: %x:%x:%x:%x:%x:%x, "
			       "olen %d, len %d\n",
			       sc->sc_name, sc->sc_unit,
			       eh->ether_dhost[0], eh->ether_dhost[1],
			       eh->ether_dhost[2], eh->ether_dhost[3],
			       eh->ether_dhost[4], eh->ether_dhost[5],
			       ior->io_count, count);
		});

		nsxmit(sc, count, sc->sc_txstrtpg);
		sc->sc_oactive = 1;
	}

	/* reenable 8390 interrupts. */	
	outb_p(sc->sc_port + EN0_IMR, ENISR_ALL);
	
	iodone(ior);
}

/*
 * Interrupt routine.
 * Called by board level driver.
 */
void
nsintr(sc)
	struct nssoftc *sc;
{
	int port = sc->sc_port;
	int interrupts, boguscount = 0;
	struct ifnet *ifp = &sc->sc_if;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		DEBUGF(printf("nsintr: %s%d: interface down\n",
			      sc->sc_name, sc->sc_unit));
		return;
	}

	/*
	 * Change to page 0 and read intr status reg.
	 */
	outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE0);

	while ((interrupts = inb_p(port + EN0_ISR)) != 0 && ++boguscount < 9) {
		if (interrupts & ENISR_RDC) {
			/*
			 * Ack meaningless DMA complete.
			 */
			outb_p(port + EN0_ISR, ENISR_RDC);
		}
		
		if (interrupts & ENISR_OVER) 
			nsrxoverrun(sc);
		else if (interrupts & (ENISR_RX+ENISR_RX_ERR)) {
			nsrint(sc);
		}
		
		if (interrupts & ENISR_TX) {
			nsxint(sc);
		}
		else if (interrupts & ENISR_COUNTERS) {
			/*
			 * XXX - We really should be storing statistics 
			 * about the interface.  For now we just drop them.
			 */

			/* reading resets the counters! */
			(void) inb_p(port + EN0_COUNTER0);   	/* frame */
		  	(void) inb_p(port + EN0_COUNTER1);	/* crc */
		  	(void) inb_p(port + EN0_COUNTER2);	/* miss */

			DEBUGF(printf("%s%d: acked counter interrupt.\n",
				      sc->sc_name, sc->sc_unit));

			outb_p(port + EN0_ISR, ENISR_COUNTERS);	/* ack intr */
		}
		
		if (interrupts & ENISR_TX_ERR) {
			DEBUGF(printf("acking transmit error\n"));
			outb_p(port + EN0_ISR, ENISR_TX_ERR);	/* ack intr */
		}

		outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_START);
	}

	DEBUGF({
		if (interrupts) {
			printf("%s%d: unknown interrupt 0x%x",
			       sc->sc_name, sc->sc_unit, interrupts);
			outb_p(port + E8390_CMD,
			       E8390_NODMA+E8390_PAGE0+E8390_START);
			outb_p(port + EN0_ISR, 0xff);	/* ack all intrs */
		}
	})
}

/*
 * Process a transmit interrupt.
 */
void
nsxint(sc)
	struct nssoftc *sc;
{
	int port = sc->sc_port, status;
	struct ifnet *ifp = &sc->sc_if;

	status = inb(port + EN0_TSR);
	outb_p(port + EN0_ISR, ENISR_TX);	/* ack intr */

	sc->sc_txing = 0;
	sc->sc_timer = 0;
	sc->sc_oactive = 0;

	if (sc->sc_pingpong) {
		if (sc->sc_tx1 < 0) {
			if (sc->sc_lasttx != 1 && sc->sc_lasttx != -1)
				printf("%s%d: bogus last_tx_buffer %d,"
				       "tx1 = %d\n",
				       sc->sc_name, sc->sc_unit,
				       sc->sc_lasttx, sc->sc_tx1);
			sc->sc_tx1 = 0;
			if (sc->sc_tx2 > 0) {
				nsxmit(sc, sc->sc_tx2, sc->sc_txstrtpg + 6);
				sc->sc_tx2 = -1;
				sc->sc_lasttx = 2;
			} else
				sc->sc_lasttx = 20;
		} else if (sc->sc_tx2 < 0) {
			if (sc->sc_lasttx != 2 && sc->sc_lasttx != -2)
				printf("%s%d: bogus last_tx_buffer %d,"
				       "tx2 = %d\n",
				       sc->sc_name, sc->sc_unit,
				       sc->sc_lasttx, sc->sc_tx2);
			sc->sc_tx2 = 0;
			if (sc->sc_tx1 > 0) {
				nsxmit(sc, sc->sc_tx1, sc->sc_txstrtpg);
				sc->sc_tx1 = -1;
				sc->sc_lasttx = 1;
			} else
				sc->sc_lasttx = 10;
		} else
			printf("%s%d: unexpected TX-done interrupt, "
			       "lasttx = %d\n",
			       sc->sc_name, sc->sc_unit, sc->sc_lasttx);
	}
	/*
	 * Update stats.
	 */
	if (status & ENTSR_COL) {
		if (status & ENTSR_ABT)
			ifp->if_collisions += 16;
		else
			ifp->if_collisions += inb(port + EN0_NCR);
	}
	if (status & ENTSR_PTX) {
		DEBUGF(printf("sent: %s%d\n", sc->sc_name, sc->sc_unit));
		ifp->if_opackets++;
	} else
		ifp->if_oerrors++;

	/*
	 * Start output on interface.
	 */
	nsstart(sc);
}

/*
 * Process a receive interrupt.
 */
void
nsrint(sc)
	struct nssoftc *sc;
{
	int port = sc->sc_port;
	int rxing_page, this_frame, next_frame, current_offset;
	int rx_pkt_count = 0;
	int num_rx_pages = sc->sc_stoppg - sc->sc_rxstrtpg;
	struct nspkthdr rx_frame;
	struct ifnet *ifp = &sc->sc_if;

	while (++rx_pkt_count < 10) {
		int pkt_len;

		/*
		 * Get the rx page (incoming packet pointer).
		 */
		outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE1);
		rxing_page = inb_p(port + EN1_CURPAG);
		outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE0);

		/*
		 * Remove one frame from the ring.
		 * Boundary is always a page behind.
		 */
		this_frame = inb_p(port + EN0_BOUNDARY) + 1;
		if (this_frame >= sc->sc_stoppg)
			this_frame = sc->sc_rxstrtpg;

		DEBUGF({
			if (this_frame != sc->sc_curpg)
				printf("%s%d: mismatched read page pointers "
				       "%x vs %x\n",
				       sc->sc_name, sc->sc_unit,
				       this_frame, sc->sc_curpg);
		});

		if (this_frame == rxing_page) {
		  	DEBUGF(printf("this_frame = rxing_page!\n"));
			break;
		}

		current_offset = this_frame << 8;
		(*sc->sc_input)(sc, sizeof(rx_frame), (char *)&rx_frame,
				current_offset);

		pkt_len = rx_frame.count - sizeof(rx_frame);

		next_frame = this_frame + 1 + ((pkt_len + 4) >> 8);

		if (rx_frame.next != next_frame
		    && rx_frame.next != next_frame + 1
		    && rx_frame.next != next_frame - num_rx_pages
		    && rx_frame.next != next_frame + 1 - num_rx_pages) {
			sc->sc_curpg = rxing_page;
			outb(port + EN0_BOUNDARY, sc->sc_curpg - 1);
			ifp->if_ierrors++;
			DEBUGF(printf("INPUT ERROR?\n"));
			continue;
		}
		if (pkt_len < 60 || pkt_len > 1518) {
			ifp->if_ierrors++;
			DEBUGF(printf("%s%d: bad packet length %d\n",
				      sc->sc_name, sc->sc_unit, pkt_len));
		} else if ((rx_frame.status & 0x0f) == ENRSR_RXOK) {
			ipc_kmsg_t kmsg;

			kmsg = net_kmsg_get();
			if (kmsg == 0) {
				DEBUGF(printf("%s%d: dropped packet\n",
					      sc->sc_name, sc->sc_unit));
				ifp->if_rcvdrops++;
			} else {
				int len, off;
				struct ether_header *eh;
				struct packet_header *pkt;

				ifp->if_ipackets++;
				off = current_offset + sizeof(rx_frame);
				eh = ((struct ether_header *)
				      (&net_kmsg(kmsg)->header[0]));
				(*sc->sc_input)(sc,
						sizeof(struct ether_header),
						(char *)eh, off);
				off += sizeof(struct ether_header);
				len = pkt_len - sizeof(struct ether_header);

				DEBUGF(printf("rcv: %s%d: %x:%x:%x:%x:%x:%x, "
					      "len %d, type 0x%x\n",
					      sc->sc_name, sc->sc_unit,
					      eh->ether_shost[0],
					      eh->ether_shost[1],
					      eh->ether_shost[2],
					      eh->ether_shost[3],
					      eh->ether_shost[4],
					      eh->ether_shost[5],
					      len, eh->ether_type));

				pkt = ((struct packet_header *)
				       (&net_kmsg(kmsg)->packet[0]));
				(*sc->sc_input)(sc, len, (char *)(pkt+1), off);
				pkt->type = eh->ether_type;
				pkt->length = len+sizeof(struct packet_header);
				net_packet(ifp, kmsg, pkt->length,
					   ethernet_priority(kmsg));
			}
		} else {
			DEBUGF(printf("%s%d: bogus packet: "
				      "status=0x%x nxpg=0x%x size=%d\n",
				      sc->sc_name, sc->sc_unit,
				      rx_frame.status, rx_frame.next,
				      rx_frame.count));
			ifp->if_ierrors++;
		}
		next_frame = rx_frame.next;
		if (next_frame >= sc->sc_stoppg) {
			DEBUGF(printf("%s%d: next frame inconsistency, 0x%x\n",
				      sc->sc_name, sc->sc_unit, next_frame));
			next_frame = sc->sc_rxstrtpg;
		}
		sc->sc_curpg = next_frame;
		outb(port + EN0_BOUNDARY, next_frame - 1);
	}

	/*
	 * Bug alert!  Reset ENISR_OVER to avoid spurious overruns!
	 */
	outb_p(port + EN0_ISR, ENISR_RX+ENISR_RX_ERR+ENISR_OVER);
}

/*
 * Handle a receive overrun condition.
 *
 * XXX - this needs to be gone over in light of the NS documentation.
 */
void
nsrxoverrun(sc)
	struct nssoftc *sc;
{
	int port = sc->sc_port, i;
	extern unsigned delaycount;

	printf("%s%d: receive overrun\n", sc->sc_name, sc->sc_unit);

	/*
	 * We should already be stopped and in page0, but just to be sure...
	 */
	outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_STOP);

	/*
	 * Clear remote byte counter registers.
	 */
	outb_p(port + EN0_RCNTLO, 0);
	outb_p(port + EN0_RCNTHI, 0);

	/*
	 * Wait for reset to complete.
	 */
	for (i = delaycount*2; i && !(inb_p(port+EN0_ISR) & ENISR_RESET); i--)
		;
	if (i == 0) {
		printf("%s%d: reset did not complete at overrun\n",
		       sc->sc_name, sc->sc_unit);
		nsinit(sc);
		return;
	}
	/*
	 * Disable transmitter.
	 */
	outb_p(port + EN0_TXCR, E8390_TXOFF);

	/*
	 * Remove packets.
	 */
	nsrint(sc);

	outb_p(port + EN0_ISR, 0xff);
	outb_p(port + E8390_CMD, E8390_NODMA+E8390_PAGE0+E8390_START);
	outb_p(port + EN0_TXCR, E8390_TXCONFIG);
}

/*
 * Trigger a transmit start.
 */
void
nsxmit(sc, length, start_page)
	struct nssoftc *sc;
	unsigned length;
	int start_page;
{
	int port = sc->sc_port;

	sc->sc_txing = 1;
	outb_p(port, E8390_NODMA+E8390_PAGE0);
	if (inb_p(port) & E8390_TRANS) {
		printf("%s%d: nsxmit() called with the transmitter busy\n",
		       sc->sc_name, sc->sc_unit);
		return;
	}
	outb_p(port + EN0_TCNTLO, length & 0xff);
	outb_p(port + EN0_TCNTHI, (length >> 8) & 0xff);
	outb_p(port + EN0_TPSR, start_page);
	outb_p(port, E8390_NODMA+E8390_TRANS+E8390_START);
	sc->sc_timer = 4;
}

#endif	/* NUL > 0 || NWD > 0 */
