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

/*
 * Generic NS8390 definitions.
 * Derived from the Linux driver by Donald Becker.
 */

#define ETHER_ADDR_LEN	6

/*
 * 8390 state.
 */
struct nssoftc {
	struct	ifnet sc_if;	/* interface header */
	u_char	sc_addr[ETHER_ADDR_LEN];	/* station address */
	/*
	 * The following are board specific.
	 * reset() - resets the NIC and board
	 * input() - read data into buffer from supplied offset
	 * output() - write data from buffer into supplied page
	 *	      the data is padded if necessary and the actual
	 *	      count is returned.
	 */
	void	(*sc_reset)(struct nssoftc *);
	void	(*sc_input)(struct nssoftc *, int, char *, int);
	int	(*sc_output)(struct nssoftc *, int, char *, int);
	int	sc_word16:1;	/* 16 bit (vs 8 bit) board */
	int	sc_txing:1;	/* transmit active */
	int	sc_pingpong:1;	/* using ping-pong driver */
	int	sc_oactive:1;	/* transmitter is active */
	u_char	sc_txstrtpg;	/* starting transmit page */
	u_char	sc_rxstrtpg;	/* starting receive page */
	u_char	sc_stoppg;	/* stop page */
	u_char	sc_curpg;	/* current page */
	short	sc_tx1;		/* packet lengths for ping-pong transmit */
	short	sc_tx2;
	short	sc_lasttx;
	u_char	sc_timer;	/* watchdog */
	int	sc_port;	/* I/O port of 8390 */
	char	*sc_name;	/* name of board */
	int	sc_unit;	/* unit in driver */
};

#define TX_2X_PAGES	12
#define TX_1X_PAGES	6
#define TX_PAGES(ns)	((ns)->sc_pingpong ? TX_2X_PAGES : TX_1X_PAGES)

/* Some generic ethernet register configurations. */
#define E8390_TX_IRQ_MASK 0xa	/* For register EN0_ISR */
#define E8390_RX_IRQ_MASK 0x5
#define E8390_RXCONFIG    0x4	/* EN0_RXCR: broadcasts, no multicast,errors */
#define E8390_RXOFF	  0x20	/* EN0_RXCR: Accept no packets */
#define E8390_TXCONFIG	  0x00	/* EN0_TXCR: Normal transmit mode */
#define E8390_TXOFF	  0x02	/* EN0_TXCR: Transmitter off */

/*  Register accessed at EN_CMD, the 8390 base addr.  */
#define E8390_STOP	0x01	/* Stop and reset the chip */
#define E8390_START	0x02	/* Start the chip, clear reset */
#define E8390_TRANS	0x04	/* Transmit a frame */
#define E8390_RREAD	0x08	/* Remote read */
#define E8390_RWRITE	0x10	/* Remote write  */
#define E8390_NODMA	0x20	/* Remote DMA */
#define E8390_PAGE0	0x00	/* Select page chip registers */
#define E8390_PAGE1	0x40	/* using the two high-order bits */
#define E8390_PAGE2	0x80	/* Page 3 is invalid. */

#define E8390_CMD	0x00	/* The command register (for all pages) */
/* Page 0 register offsets. */
#define EN0_CLDALO	0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	0x01	/* Starting page of ring bfr WR */
#define EN0_CLDAHI	0x02	/* High byte of current local dma addr  RD */
#define EN0_STOPPG	0x02	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR		0x04	/* Transmit status reg RD */
#define EN0_TPSR	0x04	/* Transmit starting page WR */
#define EN0_NCR		0x05	/* Number of collision reg RD */
#define EN0_TCNTLO	0x05	/* Low  byte of tx byte count WR */
#define EN0_FIFO	0x06	/* FIFO RD */
#define EN0_TCNTHI	0x06	/* High byte of tx byte count WR */
#define EN0_ISR		0x07	/* Interrupt status reg RD WR */
#define EN0_CRDALO	0x08	/* low byte of current remote dma address RD */
#define EN0_RSARLO	0x08	/* Remote start address reg 0 */
#define EN0_CRDAHI	0x09	/* high byte, current remote dma address RD */
#define EN0_RSARHI	0x09	/* Remote start address reg 1 */
#define EN0_RCNTLO	0x0a	/* Remote byte count reg WR */
#define EN0_RCNTHI	0x0b	/* Remote byte count reg WR */
#define EN0_RSR		0x0c	/* rx status reg RD */
#define EN0_RXCR	0x0c	/* RX configuration reg WR */
#define EN0_TXCR	0x0d	/* TX configuration reg WR */
#define EN0_COUNTER0	0x0d	/* Rcv alignment error counter RD */
#define EN0_DCFG	0x0e	/* Data configuration reg WR */
#define EN0_COUNTER1	0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR		0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	0x0f	/* Rcv missed frame error counter RD */

/* Bits in EN0_ISR - Interrupt status register */
#define ENISR_RX	0x01	/* Receiver, no error */
#define ENISR_TX	0x02	/* Transmitter, no error */
#define ENISR_RX_ERR	0x04	/* Receiver, with error */
#define ENISR_TX_ERR	0x08	/* Transmitter, with error */
#define ENISR_OVER	0x10	/* Receiver overwrote the ring */
#define ENISR_COUNTERS	0x20	/* Counters need emptying */
#define ENISR_RDC	0x40	/* remote dma complete */
#define ENISR_RESET	0x80	/* Reset completed */
#define ENISR_ALL	0x3f	/* Interrupts we will enable */

/* Bits in EN0_DCFG - Data config register */
#define ENDCFG_WTS	0x01	/* word transfer mode selection */

/* Page 1 register offsets. */
#define EN1_PHYS   0x01	/* This board's physical enet addr RD WR */
#define EN1_CURPAG 0x07	/* Current memory page RD WR */
#define EN1_MULT   0x08	/* Multicast filter mask array (8 bytes) RD WR */

/* Bits in received packet status byte and EN0_RSR*/
#define ENRSR_RXOK	0x01	/* Received a good packet */
#define ENRSR_CRC	0x02	/* CRC error */
#define ENRSR_FAE	0x04	/* frame alignment error */
#define ENRSR_FO	0x08	/* FIFO overrun */
#define ENRSR_MPA	0x10	/* missed pkt */
#define ENRSR_PHY	0x20	/* physical/multicase address */
#define ENRSR_DIS	0x40	/* receiver disable. set in monitor mode */
#define ENRSR_DEF	0x80	/* deferring */

/* Transmitted packet status, EN0_TSR. */
#define ENTSR_PTX 0x01	/* Packet transmitted without error */
#define ENTSR_ND  0x02	/* The transmit wasn't deferred. */
#define ENTSR_COL 0x04	/* The transmit collided at least once. */
#define ENTSR_ABT 0x08  /* The transmit collided 16 times, and was deferred. */
#define ENTSR_CRS 0x10	/* The carrier sense was lost. */
#define ENTSR_FU  0x20  /* A "FIFO underrun" occured during transmit. */
#define ENTSR_CDH 0x40	/* The collision detect "heartbeat" signal was lost. */
#define ENTSR_OWC 0x80  /* There was an out-of-window collision. */

/* The per-packet-header format. */
struct nspkthdr {
	u_char	status;	/* status */
	u_char	next;	/* pointer to next packet. */
	u_short	count;	/* header + packet length in bytes */
};

void	nsinit(struct nssoftc *);
void	nsstart(struct nssoftc *);
void	nsintr(struct nssoftc *);
