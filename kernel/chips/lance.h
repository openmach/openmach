/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989 Carnegie Mellon University
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
 *	File:	if_se.h
 *	Authors: Robert V. Baron and Alessandro Forin
 *	Date:	1989
 *
 */
/*
 *	AMD 7990 "Lance" Ethernet interface definitions.
 *	All structures are as seen from the Lance,
 *	both in the memory-alignment sense and in the
 *	byte-order sense.  Mapping to host memory is
 *	model specific: on pmaxen there is a 16 bit gap
 *	every other 16 bits.
 */

#include <scsi/scsi_endian.h>

/*
 * Selection of one of the four Lance CSR is done in a
 * two-step process: select which CSR first by writing
 * into the RAP, then access the register via the RDP.
 * Note that (a) the selection remains, and (b) all
 * but CSR0 can only be accessed when the chip is stopped.
 * These registers are mapped in the 'registers' I/O segment.
 */
#ifndef	se_reg_type
#define	se_reg_type unsigned short
#endif
typedef volatile se_reg_type *se_reg_t;

#define	CSR0_SELECT	0x0	/* Valid RAP selections			*/
#define	CSR1_SELECT	0x1
#define	CSR2_SELECT	0x2
#define	CSR3_SELECT	0x3

/*
 * Bit definitions for the CSR0.
 * Legend:
 *	R=Readable	 W=Writeable
 *	S=Set-on-write-1 C=Clear-on-write-1
 */

#define LN_CSR0_INIT	0x0001	/* (RS) Initialize			*/
#define LN_CSR0_STRT	0x0002	/* (RS) Start				*/
#define LN_CSR0_STOP	0x0004	/* (RS) Stop				*/
#define LN_CSR0_TDMD	0x0008	/* (RS) Transmit demand			*/
#define LN_CSR0_TXON	0x0010	/* (R)  Transmitter enabled		*/
#define LN_CSR0_RXON	0x0020	/* (R)	Receiver enabled		*/
#define LN_CSR0_INEA	0x0040	/* (RW) Interrupt enable		*/
#define LN_CSR0_INTR	0x0080	/* (R)  Interrupt pending		*/
#define LN_CSR0_IDON	0x0100	/* (RC) Initialization done		*/
#define LN_CSR0_TINT	0x0200	/* (RC) Transmitter interrupt		*/
#define LN_CSR0_RINT	0x0400	/* (RC) Receiver interrupt		*/
#define LN_CSR0_MERR	0x0800	/* (RC) Memory error during DMA		*/
#define LN_CSR0_MISS	0x1000	/* (RC) No available receive buffers	*/
#define LN_CSR0_CERR	0x2000	/* (RC) Signal quality (SQE) test	*/
#define LN_CSR0_BABL	0x4000	/* (RC) Babble error: xmit too long	*/
#define LN_CSR0_ERR	0x8000	/* (R)  Error summary: any of the 4 above */

#define	LN_CSR0_WTC	0x7f00	/* Write-to-clear bits */

/*
 * Bit definitions for the CSR1.
 */

#define LN_CSR1_MBZ	0x0001	/*      Must be zero			*/
#define LN_CSR1_IADR	0xfffe	/* (RW) Initialization block address (low) */

/*
 * Bit definitions for the CSR2.
 */

#define LN_CSR2_IADR	0x00ff	/* (RW) Initialization block address (high) */
#define LN_CSR2_XXXX	0xff00	/* (RW) Reserved			*/

/*
 * Bit definitions for the CSR3.
 */

#define LN_CSR3_BCON	0x0001	/* (RW) BM/HOLD Control			*/
#define LN_CSR3_ACON	0x0002	/* (RW) ALE Control			*/
#define LN_CSR3_BSWP	0x0004	/* (RW) Byte Swap			*/
#define LN_CSR3_XXXX	0xfff8	/* (RW) Reserved			*/


/*
 * Initialization Block
 *
 * Read when the INIT command is sent to the lance.
 */

struct se_init_block {
	unsigned short	mode;			/* Mode Register, see below */
	unsigned short	phys_addr_low;		/* Ethernet address	*/
	unsigned short	phys_addr_med;		/* Ethernet address	*/
	unsigned short	phys_addr_high;		/* Ethernet address	*/
	unsigned short	logical_addr_filter0;	/* Multicast filter	*/
	unsigned short	logical_addr_filter1;	/* Multicast filter	*/
	unsigned short	logical_addr_filter2;	/* Multicast filter	*/
	unsigned short	logical_addr_filter3;	/* Multicast filter	*/
	unsigned short	recv_ring_pointer_lo;	/* Receive Ring ptr, low   */
	BITFIELD_3(unsigned char,
		        recv_ring_pointer_hi,	/* Receive Ring ptr, high  */
			reserved0 : 5,
			recv_ring_len : 3);	/* Length: log2(nbuffers)  */
	unsigned short	xmit_ring_pointer_lo;	/* Transmit Ring ptr, low  */
	BITFIELD_3(unsigned char,
		   	xmit_ring_pointer_hi,	/* Transmit Ring ptr, high */
			reserved1 : 5,
			xmit_ring_len : 3);	/* Length: log2(nbuffers)  */
};

typedef volatile struct se_init_block *se_init_block_t;

/*
 * Bit definitions for the MODE word
 * (Normally set to 0)
 */

#define LN_MODE_DRX	0x0001	/* Disable Receiver			*/
#define LN_MODE_DTX	0x0002	/* Disable Transmitter			*/
#define LN_MODE_LOOP	0x0004	/* Loopback mode			*/
#define LN_MODE_DTRC	0x0008	/* Disable CRC generation		*/
#define LN_MODE_COLL	0x0010	/* Force collision			*/
#define LN_MODE_DRTY	0x0020	/* Disable retry			*/
#define LN_MODE_INTL	0x0040	/* Internal Loopback mode		*/
#define LN_MODE_XXXX	0x7f80	/* Reserved				*/
#define LN_MODE_PROM	0x8000	/* Promiscuous mode			*/

/*
 * Bit definitions for the ring pointers
 */

#define LN_RNGP_LOW	0xfffc	/* longword aligned			*/


/*
 * Buffer Descriptors
 * Legend:
 *	H-set-by-Host	C-set-by-chip
 */

struct se_desc {
	unsigned short	addr_low;		/* (H)  Buffer pointer low  */
	BITFIELD_2(unsigned char,
		   	addr_hi,		/* (H)  Buffer pointer high */
			status);		/* (HC) Buffer status	*/
	unsigned short	buffer_size;		/* (H)  Buffer length (bytes),*/
						/* bits 15..12 must be ones */
	union {
	   struct {
	        BITFIELD_2(unsigned short,
			bcnt : 12,		/* (C)  Rcvd data size	*/
			res : 4);		/*      Reads as zeroes	*/
	   } rcv;
	   struct {
		BITFIELD_2(unsigned short,
			TDR : 10,		/* (C)	Time Domain Reflectometry */
			flg2 : 6);		/* (C)	Xmit status	*/
	   } xmt;
	   unsigned short bits;
	} desc4;
#define			message_size	desc4.rcv.bcnt
#define			tdr		desc4.xmt.TDR
#define			status2		desc4.xmt.flg2
};

typedef volatile struct se_desc  *se_desc_t;

/*
 * Bit definition for STATUS byte (receive case)
 */

#define LN_RSTATE_ENP	0x01	/* (C) End of Packet			*/
#define LN_RSTATE_STP	0x02	/* (C) Start of packet			*/
#define LN_RSTATE_BUFF	0x04	/* (C) Buffer error			*/
#define LN_RSTATE_CRC	0x08	/* (C) CRC error			*/
#define LN_RSTATE_OFLO	0x10	/* (C) SILO Overflow			*/
#define LN_RSTATE_FRAM	0x20	/* (C) Framing error			*/
#define LN_RSTATE_ERR	0x40	/* (C) Error summary			*/
#define LN_RSTATE_OWN	0x80	/* (C) Owned by Lance Chip (if set)	*/


/*
 * Bit definition for STATUS byte (transmit case)
 */

#define LN_TSTATE_ENP	0x01	/* (H) End of Packet			*/
#define LN_TSTATE_STP	0x02	/* (H) Start of packet			*/
#define LN_TSTATE_DEF	0x04	/* (C) Deferred				*/
#define LN_TSTATE_ONE	0x08	/* (C) Retried exactly once		*/
#define LN_TSTATE_MORE	0x10	/* (C) Retried more than once		*/
#define LN_TSTATE_XXXX	0x20	/* Reserved				*/
#define LN_TSTATE_ERR	0x40	/* (C) Error summary (see status2)	*/
#define LN_TSTATE_OWN	0x80	/* (H) Owned by Lance Chip (if set)	*/

/*
 * Bit definitions for STATUS2 byte (transmit case)
 */

#define LN_TSTATE2_RTRY	0x01	/* (C) Failed after 16 retransmissions	*/
#define LN_TSTATE2_LCAR	0x02	/* (C) Loss of Carrier			*/
#define LN_TSTATE2_LCOL	0x04	/* (C) Late collision			*/
#define LN_TSTATE2_XXXX	0x08	/* Reserved				*/
#define LN_TSTATE2_UFLO	0x10	/* (C) Underflow (late memory)		*/
#define LN_TSTATE2_BUFF	0x20	/* (C) Buffering error (no ENP)		*/

				/* Errors that disable the transmitter	*/
#define LN_TSTATE2_DISABLE (LN_TSTATE2_UFLO|LN_TSTATE2_BUFF|LN_TSTATE2_RTRY)

/*
 * Other chip characteristics
 */

#define LN_MINBUF_CH	100	/* Minimum size of first lance buffer, if
				   chaining */

#define LN_MINBUF_NOCH	60	/* Minimum size of a lance buffer, if
				   no chaining and DTCR==1 */

#define LN_MINBUF_NOCH_RAW 64	/* Minimum size of a lance buffer, if
				   no chaining and DTCR==0 */

/*
 * Information for mapped ether
 */
typedef struct mapped_ether_info {
	volatile unsigned int	interrupt_count;
					/* tot interrupts received */
	volatile unsigned short	saved_csr0;
					/* copy of csr0 at last intr */
	unsigned char		rom_stride;
	unsigned char		ram_stride;
					/* rom&ram strides */
	unsigned		buffer_size;
					/* how much ram for lance */
	natural_t		buffer_physaddr;
					/* where it is in phys memory */
	unsigned		wait_event;
} *mapped_ether_info_t;

#ifdef	KERNEL
extern struct se_switch {
	vm_offset_t	regspace;
	vm_offset_t	bufspace;
	vm_offset_t	ln_bufspace;
	vm_offset_t	romspace;
	short		romstride;
	short		ramstride;
	int		ramsize;
	void		(*desc_copyin)( vm_offset_t, vm_offset_t, int);
	void		(*desc_copyout)( vm_offset_t, vm_offset_t, int);
	void		(*data_copyin)( vm_offset_t, vm_offset_t, int);
	void		(*data_copyout)( vm_offset_t, vm_offset_t, int);
	void		(*bzero)( vm_offset_t, int );
	vm_offset_t	(*mapaddr)( vm_offset_t );
	vm_size_t	(*mapoffs)( vm_size_t );
} *se_sw;
#endif	KERNEL
