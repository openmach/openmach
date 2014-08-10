/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

/*** FORE TCA-100 Turbochannel ATM computer interface ***/

#define RX_COUNT_INTR 	0x0001
#define RX_EOM_INTR 	0x0002
#define RX_TIME_INTR 	0x0004
#define TX_COUNT_INTR 	0x0008
#define RX_CELL_LOST 	0x0010
#define RX_NO_CARRIER 	0x0020
#define CR_RX_ENABLE 	0x0040
#define CR_TX_ENABLE 	0x0080
#define CR_RX_RESET 	0x0100
#define CR_TX_RESET 	0x0200

#define ATM_READ_REG(reg)	((reg) & 0x3ff)	/* 10 bit register mask */


struct atm_device {
  unsigned int prom[64 * 1024 / 4];
  volatile unsigned int sreg;
  volatile unsigned int creg_set;
  volatile unsigned int creg_clr;
  volatile unsigned int creg;
  volatile unsigned int rxtimer;
  unsigned int pad1;
  volatile unsigned int rxtimerv;
  unsigned int pad2;
  volatile unsigned int rxcount;
  unsigned int pad3;
  volatile unsigned int rxthresh;
  unsigned int pad4;
  volatile unsigned int txcount;
  unsigned int pad5;
  volatile unsigned int txthresh;
  unsigned int pad6[64*1024/4 - 15];
  volatile unsigned int rxfifo[14];
  unsigned int pad7[64*1024/4 - 14];
  volatile unsigned int txfifo[14];
  unsigned int pad8[64*1024/4 - 14];
};
/* MUST BE PAGE ALIGNED OR YOU WILL GET KILLED BELOW WITH ATM_INFO */

struct sar_data {
	int header;
	int payload[12];
	int trailer;
};


/*
 * Information for mapped atm device
 */
typedef struct mapped_atm_info {
    volatile unsigned int	interrupt_count;    /* tot interrupts received */
    volatile  unsigned short	saved_status_reg;   /* copy of status reg from last interrupt  */
    unsigned int 	hello_world;
    unsigned		wait_event;
} *mapped_atm_info_t;



#define ATM_DEVICE(p)	(struct atm_device*)(p) 
#define ATM_INFO(p)	(mapped_atm_info_t)( (p) + sizeof(struct atm_device) )
			 
