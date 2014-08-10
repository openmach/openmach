/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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

/*** TCA 100 ATM NETWORK INTERFACE  ***/

#ifndef STUB
#include <chips/nc.h>
#include <chips/spans.h>
#include <chips/tca100.h>
#else
#include "nc.h"
#include "spans.h"
#include "tca100.h"
#endif

typedef struct {
  nw_ep ep;
  int time_out;
  int retry;
} nw_control_s, *nw_control_t;


typedef struct {
  u_int rx_sar_header;
  u_int rx_cs_header;
  u_int *rx_p;
  u_int rx_count;
  u_int rx_next_synch;
  nw_buffer_t rx_buffer;
  nw_control_t rx_control;
  u_int tx_atm_header;
  u_int tx_sar_header;
  u_int tx_cs_header;
  u_int *tx_p;
  u_int tx_msg_count;
  u_int tx_block_count;
  u_int tx_synch;
  u_int tx_queued_count;
  nw_control_t tx_control;
  u_int reply;
} nw_tcb, *nw_tcb_t;

extern nw_tcb tct[MAX_EP];

extern nw_dev_entry_s tca100_entry_table;

extern nw_result tca100_initialize(int dev);

extern nw_result tca100_status(int dev);

extern void tca100_timer_sweep(int dev);

extern int tca100_poll(int dev);

extern nw_result tca100_send(nw_ep ep, nw_tx_header_t header,
                             nw_options options);

extern nw_buffer_t tca100_rpc(nw_ep ep, nw_tx_header_t header,
                              nw_options options);







