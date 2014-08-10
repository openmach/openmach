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
#include <chips/tca100_if.h>
# else
#include "tca100_if.h"
#endif

#define SMALL_WINDOW_SIZE (BOM_DATA_SIZE + EOM_DATA_SIZE)
#define INITIAL_WINDOW_SIZE BOM_DATA_SIZE
#define CONTINUATION_WINDOW_SIZE (71 * COM_DATA_SIZE)
#define FINAL_WINDOW_SIZE (70 * COM_DATA_SIZE + EOM_DATA_SIZE)
#define MAX_LONG_RX 2
#define MAX_LONG_TX 5
#define BASE_TIME_OUT 5
#define DELAYED_TIME_OUT 15
#define MAX_RETRY 3
#define POLL_LIMIT 100000
#define POLL_IDLE_TIME 1
#define POLL_CELL_TIME 8

#define TCA_SYNCH  0xfc00
#define TCA_ACK  (NW_SUCCESS << 10)
#define TCA_NAK  (NW_FAILURE << 10)
#define TCA_OVR  (NW_OVERRUN << 10)
#define TCA_SEQ  (NW_INCONSISTENCY << 10)

int tca100_verbose = 0;

int tick[MAX_DEV];

nw_control_s nw_tx_control[MAX_DEV][MAX_LONG_TX];
nw_control_s nw_rx_control[MAX_DEV][MAX_LONG_RX];

int long_tx_count[MAX_DEV], long_rx_count[MAX_DEV];

nw_tx_header_t delayed_tx_first[MAX_DEV], delayed_tx_last[MAX_DEV];
nw_rx_header_t delayed_rx_first[MAX_DEV], delayed_rx_last[MAX_DEV];

nw_tcb tct[MAX_EP];

u_int MTU[] = {9244, 65528, 65532, 65528};
u_int MTU_URGENT[] = {32, 28, 32, 28};

nw_dev_entry_s tca100_entry_table = {
  tca100_initialize, tca100_status, spans_timer_sweep, tca100_timer_sweep,
  tca100_poll, tca100_send, tca100_rpc, spans_input, spans_open, spans_accept,
  spans_close, spans_add, spans_drop};

typedef enum {
  ATM_HEADER,
  SAR_HEADER,
  SAR_TRAILER,
  CS_HEADER,
  CS_TRAILER,
  FRAME_ERROR,
  DELIVERY_ERROR,
  SYNCH_ERROR,
  SEQ_ERROR,
  OVERRUN_ERROR,
  RX_RETRANSMISSION,
  TX_RETRANSMISSION
} tca_error;

int tca_ec[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

nw_result tca100_initialize(int dev) {
  nw_result rc;
  int i;

  rc = spans_initialize(dev);
  if (rc = NW_SUCCESS) {
    tick[dev] = 0;
    for (i = 0; i < MAX_LONG_TX; i++)
      nw_tx_control[dev][i].ep = 0;
    long_tx_count[dev] = 0;
    delayed_tx_first[dev] = delayed_tx_last[dev] = NULL;
    for (i = 0; i < MAX_LONG_RX; i++)
      nw_rx_control[dev][i].ep = 0;
    long_rx_count[dev] = 0;
    delayed_rx_first[dev] = delayed_rx_last[dev] = NULL;
    for (i = 0; i < MAX_EP; i++) {
      tct[i].rx_sar_header = 0;
      tct[i].rx_control = NULL;
      tct[i].tx_queued_count = 0;
      tct[i].tx_control = NULL;
    }
    rc = NW_SUCCESS;
  }
  return rc;
}

nw_result tca100_status(int dev) {
  nw_result rc;
  atm_device_t atmp;
  u_int status;

  atmp = (atm_device_t) devct[dev].addr;
  status = atmp->sreg;
  if (status & RX_NO_CARRIER) {
    atmp->creg_set = CR_RX_RESET;
    atmp->creg_clr = ~CR_RX_RESET;
    atmp->creg_set = CR_RX_ENABLE;
    atmp->sreg = 0;
    rc = NW_NO_CARRIER;
  } else if (status & RX_CELL_LOST) {
    atmp->sreg = RX_COUNT_INTR;
    rc = NW_OVERRUN;
  } else {
    rc = NW_SUCCESS;
  }
  return rc;
}


void tca100_synch_send(int dev, nw_tcb_t tcb, u_int reply) {
  vol_u_int *tx_fifo = &((atm_device_t) devct[dev].addr)->txfifo[0];
  
#ifdef TRACING
  printf("Synch sent %x\n", reply);
#endif

  tx_fifo[0] = tcb->tx_atm_header;
  tx_fifo[1] = SSM;
  tx_fifo[2] = HTONL(8);
  tx_fifo[3] = HTONL(NW_SYNCHRONIZATION);
  tx_fifo[4] = htonl(reply);
  tx_fifo[5] = HTONL(8);
  tx_fifo[6] = 0;
  tx_fifo[7] = 0;
  tx_fifo[8] = 0;
  tx_fifo[9] = 0;
  tx_fifo[10] = 0;
  tx_fifo[11] = 0;
  tx_fifo[12] = 0;
  tx_fifo[13] = SYNCH_SEGMENT_TRAILER;
}

#define broken_cell_mend(length) {    \
  missing = length;                                         \
  while (missing != 0) {                                    \
    if (missing > block_count) {                            \
      limit = 0;                                            \
      missing -= block_count;                               \
    } else {                                                \
      limit = block_count - missing;                        \
      missing = 0;                                          \
    }                                                       \
    while (block_count > limit) {                           \
      t1 = block[0];                                        \
      block++;                                              \
      tx_fifo[1] = t1;                                      \
      block_count -= 4;                                     \
    }                                                       \
    if (block_count == 0) {                                 \
      ecb->tx_current = tx_header = ecb->tx_current->next;  \
      if (tx_header != NULL) {                              \
        block_count = tx_header->block_length;              \
        block = (vol_u_int *) tx_header->block;             \
      }                                                     \
    }                                                       \
  }                                                         \
}  


nw_result tca100_window_send(int dev, nw_ecb_t ecb, nw_tcb_t tcb,
			     boolean_t initial) {
  nw_result rc;
  register vol_u_int *tx_fifo = &((atm_device_t) devct[dev].addr)->txfifo[0];
  register vol_u_int *block;
  register u_int block_count, msg_count;
  register int com_count;
  int eom_count;
  register u_int atm_header, sar_header, sar_trailer;
  u_int cs_header, end_count;
  int limit, missing;
  register u_int t1, t2;
  nw_tx_header_t tx_header;
  nw_options options;

  atm_header = tcb->tx_atm_header;
  if (initial) {
    sar_header = tcb->tx_sar_header & MID;
    tx_header = ecb->tx_initial;
    block = (vol_u_int *) tx_header->block;
    block_count = tx_header->block_length;
    options = tx_header->options;
    msg_count = tx_header->msg_length;
    if (ecb->protocol == NW_LINE)
      msg_count += 4;
    if (options == NW_URGENT)
      msg_count += 4;
    cs_header = ecb->protocol | (sar_header & 0xff) << 8 |
      (msg_count & 0xff00) << 8 | msg_count << 24;
    tcb->tx_cs_header = cs_header;

    if (msg_count <= SSM_DATA_SIZE) {      /*Single segment message*/
      tx_fifo[0] = atm_header;
      sar_trailer = (msg_count + 8) << 26;
      tx_fifo[1] = SSM | sar_header;      /*Sequence number 0 is implicit*/
      end_count = msg_count + 4;
      tx_fifo[1] = cs_header;
      if (options == NW_URGENT) {
	tx_fifo[1] = HTONL(NW_URGENT);
	msg_count -= 4;
      }
      if (ecb->protocol == NW_LINE) {
	tx_fifo[1] = tx_header->peer.local_ep >> 8 |
	             (tx_header->peer.local_ep & 0x00ff) << 8 |
	             (tx_header->peer.remote_ep & 0xff00) << 8 |
		     tx_header->peer.remote_ep << 24;
	msg_count -= 4;
      }
      if (ecb->protocol == NW_SEQ_PACKET) {
	tcb->tx_synch = 0;
      } else {
	tcb->tx_synch = -1;
      }
      goto EOM_payload;
      
    } else {	                          /*Beginning of message*/
      tx_fifo[0] = atm_header;
      sar_trailer = FULL_SEGMENT_TRAILER;
      tx_fifo[1] = BOM | sar_header;      /*Sequence number 0 is implicit*/
      tx_fifo[2] = cs_header;
      if (block_count < BOM_DATA_SIZE) {
	if (ecb->protocol == NW_LINE) {
	  t1 = tx_header->peer.local_ep >> 8 |
	       (tx_header->peer.local_ep & 0x00ff) << 8 |
	       (tx_header->peer.remote_ep & 0xff00) << 8 |
	       tx_header->peer.remote_ep << 24;
	  missing = BOM_DATA_SIZE - 4;
	  tx_fifo[3] = t1;
	} else {
	  missing = BOM_DATA_SIZE;
	}
	broken_cell_mend(missing);
      } else {
	if (ecb->protocol == NW_LINE) {
	  t1 = tx_header->peer.local_ep >> 8 |
	       (tx_header->peer.local_ep & 0x00ff) << 8 |
	       (tx_header->peer.remote_ep & 0xff00) << 8 |
	       tx_header->peer.remote_ep << 24;
	} else {
	  t1 = block[0];
	  block_count -= 4;
	  block++;
	}
	t2 = block[0];
	tx_fifo[3] = t1;
	tx_fifo[4] = t2;
	t1 = block[1];
	t2 = block[2];
	tx_fifo[5] = t1;
	tx_fifo[6] = t2;
	t1 = block[3];
	t2 = block[4];
	tx_fifo[7] = t1;
	tx_fifo[8] = t2;
	t1 = block[5];
	t2 = block[6];
	tx_fifo[9] = t1;
	tx_fifo[10] = t2;
	t1 = block[7];
	t2 = block[8];
	tx_fifo[11] = t1;
	tx_fifo[12] = t2;
	block_count -= (BOM_DATA_SIZE - 4);
	block += 9;
      }
      if (ecb->protocol == NW_RAW) {
	msg_count -= BOM_DATA_SIZE;
	com_count = msg_count / COM_DATA_SIZE;
	msg_count = msg_count % COM_DATA_SIZE;
	eom_count = 1;
	tcb->tx_synch = -1;
      } else if (msg_count > SMALL_WINDOW_SIZE) {
	com_count = eom_count = 0;
	tcb->tx_synch = msg_count;
	msg_count -= BOM_DATA_SIZE;       
      } else {
	com_count = 0;
	eom_count = 1;
	if (ecb->protocol == NW_SEQ_PACKET) {
	  tcb->tx_synch = 0;
	} else {
	  tcb->tx_synch = -1;
	}
	msg_count -= BOM_DATA_SIZE;       
      }
      tx_fifo[13] = sar_trailer;
      sar_header += SEQ_INC;
    }

  } else {
    sar_header = tcb->tx_sar_header;
    sar_trailer = FULL_SEGMENT_TRAILER;
    block = (vol_u_int *) tcb->tx_p;
    block_count = tcb->tx_block_count;
    msg_count = tcb->tx_msg_count;
    if (msg_count > FINAL_WINDOW_SIZE) {
      com_count = (CONTINUATION_WINDOW_SIZE / COM_DATA_SIZE);
      eom_count = 0;
      tcb->tx_synch = msg_count;
      msg_count -= CONTINUATION_WINDOW_SIZE;
    } else {
      com_count = msg_count / COM_DATA_SIZE;
      msg_count = msg_count % COM_DATA_SIZE;
      eom_count = 1;
      if (ecb->protocol == NW_SEQ_PACKET) {
	tcb->tx_synch = 0;
      } else {
	tcb->tx_synch = -1;
      }
    }
  }

  while (com_count-- > 0) {                 /*Continuation of message*/
    tx_fifo[0] = atm_header;
    tx_fifo[1] = sar_header;               /*COM is 0 and is implicit*/
    if (block_count >= COM_DATA_SIZE) {
      t1 = block[0];
      t2 = block[1];
      tx_fifo[2] = t1;
      tx_fifo[3] = t2;
      t1 = block[2];
      t2 = block[3];
      tx_fifo[4] = t1;
      tx_fifo[5] = t2;
      t1 = block[4];
      t2 = block[5];
      tx_fifo[6] = t1;
      tx_fifo[7] = t2;
      t1 = block[6];
      t2 = block[7];
      tx_fifo[8] = t1;
      tx_fifo[9] = t2;
      t1 = block[8];
      t2 = block[9];
      tx_fifo[10] = t1;
      tx_fifo[11] = t2;
      t1 = block[10];
      block_count -= COM_DATA_SIZE;
      tx_fifo[12] = t1;
      tx_fifo[13] = sar_trailer;
      block += 11;
      sar_header = (sar_header + SEQ_INC) & (SEQ_NO | MID);
    } else {
      broken_cell_mend(COM_DATA_SIZE);
      tx_fifo[13] = sar_trailer;
      sar_header = (sar_header + SEQ_INC) & (SEQ_NO | MID);
    }
  }

  if (eom_count != 0) {                               /*End of message*/
    tx_fifo[0] = atm_header;
    tx_fifo[1] = EOM | sar_header;
    end_count = msg_count;
    sar_trailer = (msg_count + 4) << 26;

 EOM_payload:
    if (block_count >= msg_count) {
      if (msg_count & 0x4) {
	t1 = block[0];
	tx_fifo[1] = t1;
      }
      block = (vol_u_int *) ((char *) block + msg_count);
      switch (msg_count >> 3) {
      case 5:
	t1 = block[-10];
	t2 = block[-9];
	tx_fifo[1] = t1;
	tx_fifo[1] = t2;
      case 4:
	t1 = block[-8];
	t2 = block[-7];
	tx_fifo[1] = t1;
	tx_fifo[1] = t2;
      case 3:
	t1 = block[-6];
	t2 = block[-5];
	tx_fifo[1] = t1;
	tx_fifo[1] = t2;
      case 2:
	t1 = block[-4];
	t2 = block[-3];
	tx_fifo[1] = t1;
	tx_fifo[1] = t2;
      case 1:
	t1 = block[-2];
	t2 = block[-1];
	tx_fifo[1] = t1;
	tx_fifo[1] = t2;
      }
      msg_count = 0;
    } else {
      broken_cell_mend(msg_count);
      msg_count = 0;
    }

  EOM_cs_trailer:
    tx_fifo[1] = tcb->tx_cs_header;
    switch (end_count) {
    case 0: tx_fifo[1] = 0;
    case 4: tx_fifo[1] = 0;
    case 8: tx_fifo[1] = 0;
    case 12: tx_fifo[1] = 0;
    case 16: tx_fifo[1] = 0;
    case 20: tx_fifo[1] = 0;
    case 24: tx_fifo[1] = 0;
    case 28: tx_fifo[1] = 0;
    case 32: tx_fifo[1] = 0;
    case 36: tx_fifo[1] = 0;
    }
    tx_fifo[13] = sar_trailer;
  }

  if (tcb->tx_synch == -1) { 

#ifdef TRACING
    printf("Final window sent\n");
#endif

    sar_header = (sar_header + MID_INC) & MID;
    if (sar_header == 0)
      sar_header = 1;
    tcb->tx_sar_header = sar_header;
    rc = NW_SUCCESS;
  } else {

#ifdef TRACING
    printf("Window synch at %x\n", msg_count);
#endif

    tcb->tx_sar_header = sar_header;
    tcb->tx_p = (u_int *) block;
    tcb->tx_block_count = block_count;
    tcb->tx_msg_count = msg_count;
    rc = NW_SYNCH;
  }
  return rc;
}

nw_result tca100_send(nw_ep ep, nw_tx_header_t header, nw_options options) {
  nw_result rc;
  int i, dev;
  nw_ecb_t ecb;
  nw_tcb_t tcb;
  nw_control_t control;
  nw_tx_header_t tx_header, tx_previous;

  dev = NW_DEVICE(header->peer.rem_addr_1);
  ecb = &ect[ep];
  tcb = &tct[ep];
  if ((options == NW_URGENT && header->msg_length >
       MTU_URGENT[ecb->protocol]) || header->msg_length > MTU[ecb->protocol]) {
    rc = NW_BAD_LENGTH;
  } else if (tcb->tx_queued_count != 0 ||
	     (ecb->protocol != NW_RAW &&
	      long_tx_count[dev] >= MAX_LONG_TX &&
	      (header->msg_length > SMALL_WINDOW_SIZE ||
	       ecb->protocol == NW_SEQ_PACKET))) {
    if (options == NW_URGENT && tcb->tx_queued_count != 0) {
      tx_header = delayed_tx_first[dev];
      tx_previous = NULL;
      while (tx_header != NULL && tx_header->sender != ep) {
	tx_previous = tx_header;
	tx_header = tx_header->next;
      }
      if (tx_previous == NULL) 
	delayed_tx_first[dev] = header;
      else 
	tx_previous->next = header;
      while (header->next != NULL)
	header = header->next;
      header->next = tx_header;
    } else {
      if (delayed_tx_first[dev] == NULL)
	delayed_tx_first[dev] = header;
      else
	delayed_tx_last[dev]->next = header;
      delayed_tx_last[dev] = header;
    }
    tcb->tx_queued_count++;
    rc = NW_QUEUED;

#ifdef TRACING
    printf("Send enqueued ep %d\n", ep);
#endif
    
  } else {


#ifdef TRACING
    printf("Send ep %d\n", ep);
#endif

    ecb->tx_initial = ecb->tx_current = header;
    rc = tca100_window_send(dev, ecb, tcb, TRUE);
    if (rc == NW_SUCCESS) {
      while (header != NULL) {
	if (header->buffer != NULL)
	  nc_buffer_deallocate(ep, header->buffer);
	header = header->next;
      }
      nc_tx_header_deallocate(ecb->tx_initial);
      ecb->tx_initial = ecb->tx_current = NULL;
    } else {
      control = &nw_tx_control[dev][0];
      while (control->ep != 0) 
	control++; 
      control->ep = ep;
      control->time_out = tick[dev] + BASE_TIME_OUT;
      control->retry = 0;
      tcb->reply = TCA_SYNCH;
      tcb->tx_control = control;
      tcb->tx_queued_count++;
      if (long_tx_count[dev] + long_rx_count[dev] == 0)
	nc_fast_timer_set(dev);
      long_tx_count[dev]++;
    }
  }
  return rc;
}


nw_result tx_slot_free(int dev, nw_control_t control) {
  nw_result rc;
  nw_tcb_t tcb;
  nw_ecb_t ecb;
  nw_tx_header_t tx_header;
  nw_ep ep;

  tcb = &tct[control->ep];
  tcb->tx_control = NULL;                               
  tcb->tx_queued_count--;                             
  do {                                                            
    tx_header = delayed_tx_first[dev];                            
    if (tx_header == NULL) {                                      
      control->ep = 0;                                            
      long_tx_count[dev]--;                                       
      rc = NW_FAILURE;                                            
    } else {
      ep = tx_header->sender;

#ifdef TRACING
    printf("Send dequeued ep %d\n", ep);
#endif

      ecb = &ect[ep];
      tcb = &tct[ep];
      ecb->tx_initial = ecb->tx_current =  tx_header;
      while (tx_header->next != NULL &&                           
	     tx_header->next->msg_length == 0) {                  
	tx_header = tx_header->next;                              
      }                                                         
      delayed_tx_first[dev] = tx_header->next;                    
      if (tx_header->next == NULL)                                
	delayed_tx_last[dev] = NULL;                              
      tx_header->next = NULL;                                     
      rc = tca100_window_send(dev, ecb, tcb, TRUE); 
      if (rc == NW_SYNCH) {                                         
	control->ep = ep;                            
	tcb->tx_control = control;                
	tcb->reply = TCA_SYNCH;                                 
	control->time_out = tick[dev] + BASE_TIME_OUT;              
	control->retry = 0;                                         
      }                                                             
    }                                                               
  } while (rc == NW_SUCCESS);                                       
  return rc;
}

nw_result rx_slot_free(int dev, nw_control_t control) {
  nw_result rc;
  nw_rx_header_t rx_header;
  nw_ep ep;
  nw_tcb_t tcb;

  if (control == NULL) {   
    rc = NW_SUCCESS;
  } else {
    tct[control->ep].rx_control = NULL;                               
    while ((rx_header = delayed_rx_first[dev]) != NULL &&             
	   tick[dev] >= rx_header->time_stamp) {                      
      delayed_rx_first[dev] = rx_header->next;                        
      nc_buffer_deallocate(rx_header->buffer->peer.local_ep,          
			   rx_header->buffer);                        
      ep = rx_header->receiver;
      tcb = &tct[ep];
      tcb->rx_sar_header = SSM | (tcb->rx_sar_header & MID);         
      nc_rx_header_deallocate(rx_header);                             
    }                                                                 
    if (rx_header == NULL) {                                          
      delayed_rx_last[dev] = NULL;                                    
      control->ep = 0;                                                
      long_rx_count[dev]--;                                           
      rc = NW_FAILURE;
    } else {                                                          
      delayed_rx_first[dev] = rx_header->next;                        
      if (rx_header->next == NULL)                                    
	delayed_rx_last[dev] = NULL;                                  
      ep = rx_header->receiver;
      tcb = &tct[ep];
      tca100_synch_send(dev, tcb, rx_header->reply);
      control->ep = ep;
      control->time_out = tick[dev] + BASE_TIME_OUT;                  
      tcb->rx_control = control;                  
      nc_rx_header_deallocate(rx_header);                             
    }                                                                 
  }
}


int tca100_poll(int dev) {
  vol_u_int *status = &((atm_device_t) devct[dev].addr)->sreg;
  vol_u_int *ctl_set = &((atm_device_t) devct[dev].addr)->creg_set;
  vol_u_int *rx_counter = &((atm_device_t) devct[dev].addr)->rxcount;
  register vol_u_int *rx_fifo = &((atm_device_t) devct[dev].addr)->rxfifo[0];
  register u_int rx_cell_count;
  register u_int predicted_atm_header = 0;
  register u_int predicted_sar_header;
  u_int atm_header, sar_header, predicted_sar_trailer,
        cs_header, end_count, cs_pad, rx_cell_total, reply,
        block_length, initial_offset;
  register vol_u_int *msg;
  register int msg_count;
  register int next_synch;
  register u_int t1, t2;
  nw_ecb_t ecb, tx_ecb;
  nw_tcb_t new_tcb, tx_tcb;
  nw_tcb dummy_tcb_s;
  nw_tcb_t tcb = &dummy_tcb_s;
  nw_control_t control;
  nw_buffer_t buffer;
  nw_protocol protocol;
  nw_ep lep, rep;
  nw_delivery delivery_type = NW_RECEIVE;
  nw_rx_header_t rx_header;
  nw_tx_header_t tx_header;
  int i;
  u_int tx_seqno, rx_seqno, tx_count, rx_count;

  rx_cell_total = 0;
  while ((rx_cell_count = *rx_counter & RX_COUNTER_MASK) != 0) {
    rx_cell_total += rx_cell_count;
    while (rx_cell_count-- > 0) {
      atm_header = rx_fifo[0];         /*Check ATM header and SAR header*/
      sar_header = (rx_fifo[1] & SAR_HEADER_MASK);
      if (atm_header != predicted_atm_header) {
	                      /*Must be cell from a different connection*/
	if (atm_header & ~(ATM_VPVC_MASK | ATM_HEADER_RSV_BITS)) {
 atm_header_error:
	  tca_ec[ATM_HEADER]++;
	  if (tca100_verbose)
		  printf("ATM header error %x\n", atm_header);
 discard_cell:
	  *((char *) rx_fifo) = 0; 
	  delivery_type = NW_RECEIVE;
	  continue;
	} else {
	  t1 = (atm_header & ATM_VPVC_MASK) >> ATM_VPVC_SHIFT;
	  new_tcb = &tct[t1];
	  ecb = &ect[t1];

	  /*Switch cached connection*/
	  if (new_tcb->rx_sar_header == 0)
	    goto atm_header_error;
	  tcb->rx_sar_header = predicted_sar_header;
	  tcb->rx_p = (u_int *) msg;
	  tcb->rx_count = msg_count;
	  tcb->rx_next_synch = next_synch;
	  predicted_atm_header = atm_header;
	  tcb = new_tcb;
	  predicted_sar_header = tcb->rx_sar_header;
	  msg = tcb->rx_p;
	  msg_count = tcb->rx_count;
	  next_synch = tcb->rx_next_synch;
	}
      }
      
      if (sar_header != predicted_sar_header) {
	if ((sar_header ^ predicted_sar_header) == EOM &&
	    ((predicted_sar_header & BOM) || msg_count <= EOM_DATA_SIZE)) {
                                    /*Difference on end of message bit only*/
	  predicted_sar_header = sar_header;
	} else if (sar_header == SSM) {                             /*MID 0*/
	  cs_header = rx_fifo[2];
	  t1 = rx_fifo[3];
	  if (cs_header == HTONL(8) && t1 == HTONL(NW_SYNCHRONIZATION)) {
	    reply = rx_fifo[4];                                /*Synch cell*/
	    if (rx_fifo[5] != cs_header)
	      goto cs_header_error;
	    cs_pad = rx_fifo[6];
	    t1 = rx_fifo[7];
	    t2 = rx_fifo[8];
	    cs_pad |= t1;
	    cs_pad |= t2;
	    t1 = rx_fifo[9];
	    t2 = rx_fifo[10];
	    cs_pad |= t1;
	    cs_pad |= t2;
	    t1 = rx_fifo[11];
	    t2 = rx_fifo[12];
	    cs_pad |= t1;
	    cs_pad |= t2;
	    t1 = rx_fifo[13];
	    if (cs_pad)
	      goto cs_trailer_error;
	    if ((t1 & SAR_TRAILER_MASK) != SYNCH_SEGMENT_TRAILER)
	      goto sar_trailer_error;
	    if (tcb->tx_control == NULL) {
	      tca_ec[SYNCH_ERROR]++;
	      if (tca100_verbose)
		      printf("Synch error %x\n", ntohl(reply));
	    } else {
	      tcb->reply = ntohl(reply);

#ifdef TRACING
	      printf("Received synch ep %d %x\n", ecb->id, tcb->reply);
#endif

	    }
	    continue;
	  } else if (t1 == HTONL(NW_URGENT)) {              /*Urgent cell*/
	    delivery_type = NW_RECEIVE_URGENT;
	    goto cs_header_check;
	  } else {                                          /*Bad segment*/
	    goto sar_header_error;
	  }
	} else if (!(sar_header & ATM_HEADER_CRC_SYNDROME) &&
		   (sar_header & BOM) && (sar_header & SEQ_NO) == 0) {
	  if ((sar_header & MID) == (predicted_sar_header & MID)) {
	                                                 /*Retransmission*/
	    if (tcb->rx_control != NULL) {
	      tcb->rx_control->ep = 0;
	      long_rx_count[dev]--;
	    }
	    nc_buffer_deallocate(tcb->rx_buffer->peer.local_ep,
				 tcb->rx_buffer);
	    predicted_sar_header = sar_header;
	    tca_ec[RX_RETRANSMISSION]++;
	    if (tca100_verbose)
		    printf("Receiving retransmission ep %d sar %x\n",
			   ecb->id, sar_header); 
	  } else if (predicted_sar_header & BOM) {
                                                    /*Sequence number error*/
	    if (tca100_verbose)
		    printf("Sequence error ep %d pred %x real %x\n", ecb->id,
			   predicted_sar_header, sar_header);
	    if (ecb->protocol == NW_SEQ_PACKET) {
	      reply = 0xffff0000 | TCA_SEQ | (predicted_sar_header & MID);
	      tca100_synch_send(dev, tcb, reply);
	      tca_ec[SEQ_ERROR]++;
	      goto discard_cell;
	    } else {
	      predicted_sar_header = sar_header;
	    }
	  } else {
	    goto sar_header_error;                     /*Badly out of synch*/
	  }
	} else {                                                /*Cell loss*/

 sar_header_error:
	  if (!(predicted_sar_header & BOM)) {
	    rx_slot_free(dev, tcb->rx_control);
	    nc_buffer_deallocate(tcb->rx_buffer->peer.local_ep,
				 tcb->rx_buffer);
	    predicted_sar_header = SSM | (predicted_sar_header & MID);
	  }
	  tca_ec[SAR_HEADER]++;
	  if (tca100_verbose)
		  printf("SAR header error ep %d pred %x real %x\n", ecb->id,
			 predicted_sar_header, sar_header);
	  goto discard_cell;
	}  
      }

      if ((predicted_sar_header & SEG_TYPE) == COM) { 
	                                           /*Continuation of message*/
	if (msg_count <= next_synch) {
	  if (msg_count == next_synch &&
	      msg_count >= CONTINUATION_WINDOW_SIZE) {
	    reply = (msg_count << 16) | TCA_ACK | (predicted_sar_header & MID);
	    tca100_synch_send(dev, tcb, reply);
	    if (msg_count > (CONTINUATION_WINDOW_SIZE + FINAL_WINDOW_SIZE)) {
	      next_synch = msg_count - CONTINUATION_WINDOW_SIZE;
	    } else if (ecb->protocol == NW_SEQ_PACKET) {
	      next_synch = 0;
	    } else {
	      next_synch = -1;
	    }
	    tcb->rx_control->time_out = tick[dev] + BASE_TIME_OUT;
	  } else {
	    rx_slot_free(dev, tcb->rx_control);
	    nc_buffer_deallocate(tcb->rx_buffer->peer.local_ep,
				 tcb->rx_buffer);
	    predicted_sar_header = SSM | (predicted_sar_header & MID);
	    tca_ec[FRAME_ERROR]++;
	    if (tca100_verbose)
		    printf("Frame error ep %d\n", ecb->id);
	    goto discard_cell;
	  }
	}
	t1 = rx_fifo[2];
	t2 = rx_fifo[3];
	msg[0] = t1;
	msg[1] = t2;
	t1 = rx_fifo[4];
	t2 = rx_fifo[5];
	msg[2] = t1;
	msg[3] = t2;
	t1 = rx_fifo[6];
	t2 = rx_fifo[7];
	msg[4] = t1;
	msg[5] = t2;
	t1 = rx_fifo[8];
	t2 = rx_fifo[9];
	msg[6] = t1;
	msg[7] = t2;
	t1 = rx_fifo[10];
	t2 = rx_fifo[11];
	msg[8] = t1;
	msg[9] = t2;
	t1 = rx_fifo[12];
	t2 = rx_fifo[13];
	msg[10] = t1;
	if ((t2 & SAR_TRAILER_MASK) != FULL_SEGMENT_TRAILER) {
	  t1 = t2;
	  goto sar_trailer_error;
	}
	predicted_sar_header = (predicted_sar_header + SEQ_INC) &
	                                                    (SEQ_NO | MID);
	msg_count -= COM_DATA_SIZE;
	msg += 11;

      } else if ((predicted_sar_header & BOM) != 0) {
	cs_header = rx_fifo[2];

 cs_header_check:
	block_length = msg_count = (((cs_header >> 8) & 0xff00) |
				    (cs_header >> 24));
	protocol = cs_header & 0x00ff;
	if (protocol == NW_RAW || protocol == NW_SEQ_PACKET) {
	  lep = ecb->conn->peer.local_ep;
	  rep = ecb->conn->peer.remote_ep;
	  if (delivery_type == NW_RECEIVE)
	    initial_offset = 0;
	  else
	    initial_offset = 4;
	} else {
	  t1 = rx_fifo[3];
	  block_length -= 4;
	  lep = (t1 >> 8) & 0xff00 | t1 >> 24;
	  rep = (t1 & 0xff00) >> 8 | (t1 & 0x00ff) << 8;
	  if (delivery_type == NW_RECEIVE)
	    initial_offset = 4;
	  else
	    initial_offset = 8;
	} 
	if (protocol != ecb->protocol || (protocol == NW_DATAGRAM) ||
	    (protocol == NW_LINE && ect[lep].protocol != NW_DATAGRAM) ||
	    ((predicted_sar_header & 0x00ff) << 8) != (cs_header & 0xff00) ||
	    ((delivery_type != NW_RECEIVE) &&
	     msg_count - initial_offset > MTU_URGENT[protocol]) ||
	    msg_count > MTU[protocol] || (msg_count & 0x3)) {

 cs_header_error:
	  if ((protocol != NW_RAW && msg_count > SMALL_WINDOW_SIZE) ||
	      protocol == NW_SEQ_PACKET) {
	    reply = 0xffff0000 | TCA_NAK | (predicted_sar_header & MID);
	    tca100_synch_send(dev, tcb, reply);
	  }
	  tca_ec[CS_HEADER]++;
	  if (tca100_verbose)
		  printf("CS header error ep %d sar %x cs %x\n", ecb->id,
			 predicted_sar_header, cs_header);
	  goto discard_cell;
	}
	buffer = nc_buffer_allocate(lep, sizeof(nw_buffer_s) + block_length);
	if (buffer == NULL) {
	  if ((protocol != NW_RAW && msg_count > SMALL_WINDOW_SIZE) ||
	      protocol == NW_SEQ_PACKET) {
	    reply = 0xffff0000 | TCA_OVR | (predicted_sar_header & MID);
	    tca100_synch_send(dev, tcb, reply);
	  }
	  tca_ec[OVERRUN_ERROR]++;
	  if (tca100_verbose)
		  printf("Overrun error ep %d\n", ecb->id);
	  goto discard_cell;
	}
	if (protocol == NW_RAW) {
	  next_synch = -1;
	} else if (msg_count > SMALL_WINDOW_SIZE) {
	  reply = (msg_count << 16) | TCA_ACK | (predicted_sar_header & MID);
	  if (long_rx_count[dev] >= MAX_LONG_RX) {
	    rx_header = nc_rx_header_allocate();
	    if (rx_header == NULL) {
	      nc_buffer_deallocate(lep, buffer);
	      tca_ec[OVERRUN_ERROR]++;
	      goto discard_cell;
	    }
	    rx_header->buffer = buffer;
	    rx_header->receiver = ecb->id;
	    rx_header->reply = reply;
	    rx_header->time_stamp = tick[dev] + DELAYED_TIME_OUT;
	    rx_header->next = NULL;
	    if (delayed_rx_last[dev] == NULL)
	      delayed_rx_first[dev] = rx_header;
	    else
	      delayed_rx_last[dev]->next = rx_header;
	    delayed_rx_last[dev] = rx_header;
	  } else {
	    tca100_synch_send(dev, tcb, reply);
	    control = &nw_rx_control[dev][0];
	    while (control->ep != 0)
	      control++;
	    control->ep = ecb->id;
	    control->time_out = tick[dev] + BASE_TIME_OUT;
	    tcb->rx_control = control;
	    if (long_rx_count[dev] + long_tx_count[dev] == 0)
	      nc_fast_timer_set(dev);
	    long_rx_count[dev]++;
	  }
	  if (msg_count > INITIAL_WINDOW_SIZE + FINAL_WINDOW_SIZE)
	    next_synch = msg_count - INITIAL_WINDOW_SIZE;
	  else if (protocol == NW_SEQ_PACKET)
	    next_synch = 0;
	  else
	    next_synch = -1;
	} else if (protocol == NW_SEQ_PACKET) {
	  next_synch = 0;
	} else {
	  next_synch = -1;
	}
	msg = (vol_u_int *) ((char *) buffer + sizeof(nw_buffer_s));
	tcb->rx_cs_header = cs_header;
	tcb->rx_buffer = buffer;
	buffer->buf_next = NULL;
	buffer->msg_seqno = sar_header & MID;
	buffer->block_offset = sizeof(nw_buffer_s);
	buffer->block_length = block_length;
	buffer->peer.rem_addr_1 = ecb->conn->peer.rem_addr_1;
	buffer->peer.rem_addr_2 = ecb->conn->peer.rem_addr_2;
	buffer->peer.local_ep = lep;
	buffer->peer.remote_ep = rep;

	if ((predicted_sar_header & EOM) == 0) {                  /*BOM*/
	  if (initial_offset == 0) {
	    t1 = rx_fifo[3];
	    t2 = rx_fifo[4];
	    msg[0] = t1;
	    msg[1] = t2;
	    msg += 2;
	  } else {
	    msg[0] = rx_fifo[4];
	    msg++;
	  }
	  t1 = rx_fifo[5];
	  t2 = rx_fifo[6];
	  msg[0] = t1;
	  msg[1] = t2;
	  t1 = rx_fifo[7];
	  t2 = rx_fifo[8];
	  msg[2] = t1;
	  msg[3] = t2;
	  t1 = rx_fifo[9];
	  t2 = rx_fifo[10];
	  msg[4] = t1;
	  msg[5] = t2;
	  t1 = rx_fifo[11];
	  t2 = rx_fifo[12];
	  msg[6] = t1;
	  t1 = rx_fifo[13];
	  msg[7] = t2;
	  if ((t1 & SAR_TRAILER_MASK) != FULL_SEGMENT_TRAILER)
	    goto sar_trailer_error;
	  msg_count -= BOM_DATA_SIZE;
	  msg += 8;
	  predicted_sar_header = (predicted_sar_header + SEQ_INC) &
	                                                  (SEQ_NO | MID);

	} else {                                                  /*SSM*/
	  end_count = msg_count + 4;
	  predicted_sar_trailer = (msg_count + 8) << 26;
	  if (delivery_type != NW_RECEIVE) {
	    msg[0] = NW_URGENT;
	    msg++;
	  }
	  msg_count -= initial_offset;
	  goto EOM_payload;
	}
      } else {                                                    /*EOM*/
	end_count = msg_count;
	predicted_sar_trailer = (msg_count + 4) << 26;
	
 EOM_payload:
	if (msg_count & 0x4) {
	  msg[0] = rx_fifo[2];
	}
	msg = (vol_u_int *) ((char *) msg + msg_count);
	/*Fall-through the cases is intentional*/
	switch (msg_count >> 3) {
	case 5:
	  t1 = rx_fifo[2];
	  t2 = rx_fifo[2];
	  msg[-10] = t1;
	  msg[-9] = t2;
	case 4:
	  t1 = rx_fifo[2];
	  t2 = rx_fifo[2];
	  msg[-8] = t1;
	  msg[-7] = t2;
	case 3:
	  t1 = rx_fifo[2];
	  t2 = rx_fifo[2];
	  msg[-6] = t1;
	  msg[-5] = t2;
	case 2:
	  t1 = rx_fifo[2];
	  t2 = rx_fifo[2];
	  msg[-4] = t1;
	  msg[-3] = t2;
	case 1:
	  t1 = rx_fifo[2];
	  t2 = rx_fifo[2];
	  msg[-2] = t1;
	  msg[-1] = t2;
	}

	/*CS trailer should be equal to the CS header, followed by
	  padding zeros*/
	cs_pad = (rx_fifo[2] != tcb->rx_cs_header);
	/*Fall-through the cases is intentional*/
	t1 = t2 = 0;
	switch (end_count) {
	case 0:
	  t1 = rx_fifo[2];
	case 4:
	  t2 = rx_fifo[2];
	  cs_pad |= t1;
	case 8:
	  t1 = rx_fifo[2];
	  cs_pad |= t2;
	case 12:
	  t2 = rx_fifo[2];
	  cs_pad |= t1;
	case 16:
	  t1 = rx_fifo[2];
	  cs_pad |= t2;
	case 20:
	  t2 = rx_fifo[2];
	  cs_pad |= t1;
	case 24:
	  t1 = rx_fifo[2];
	  cs_pad |= t2;
	case 28:
	  t2 = rx_fifo[2];
	  cs_pad |= t1;
	case 32:
	  t1 = rx_fifo[2];
	  cs_pad |= t2;
	case 36:
	  t2 = rx_fifo[2];
	  cs_pad |= t1;
	  cs_pad |= t2;
	}
	t1 = rx_fifo[13];
	if (cs_pad != 0) {
                                 	     /*Errors in CS trailer or pad*/
 cs_trailer_error:
	  tca_ec[CS_TRAILER]++;
	  if (tca100_verbose)
		  printf("CS trailer error ep %d hd %x pad %x\n", ecb->id,
			 tcb->rx_cs_header, cs_pad);
	  goto trailer_error;

	} else if ((t1 & SAR_TRAILER_MASK) != predicted_sar_trailer) {
                                         /*Error in SAR trailer or framing*/
 sar_trailer_error:
	  tca_ec[SAR_TRAILER]++;
	  if (tca100_verbose)
		  printf("SAR trailer error ep %d pred %x real %x\n", ecb->id,
			 predicted_sar_trailer, t1);
	  goto trailer_error;

	} else if (!nc_deliver_result(tcb->rx_buffer->peer.local_ep,
				      delivery_type, (int) tcb->rx_buffer)) {
	  tca_ec[DELIVERY_ERROR]++;
	  if (tca100_verbose)
		  printf("Delivery error ep %d\n", ecb->id);

 trailer_error:
	  if (next_synch >= 0 && !(t1 & HEADER_CRC_ERROR)) {
	    reply = (msg_count << 16) | TCA_NAK | (predicted_sar_header & MID);
	    tca100_synch_send(dev, tcb, reply);
	  }
	  rx_slot_free(dev, tcb->rx_control);
	  nc_buffer_deallocate(tcb->rx_buffer->peer.local_ep,
			       tcb->rx_buffer);
	  predicted_sar_header = SSM | (predicted_sar_header & MID);
	  delivery_type = NW_RECEIVE;
	} else {

#ifdef TRACING
	  printf("Received correctly ep %d\n", ecb->id);
#endif

	  if (next_synch == 0) {
	    reply = TCA_ACK | (predicted_sar_header & MID);
	    tca100_synch_send(dev, tcb, reply);
	  }
	  rx_slot_free(dev, tcb->rx_control);
	  if (delivery_type != NW_RECEIVE) {
	    delivery_type = NW_RECEIVE;
	    predicted_sar_header = SSM | (predicted_sar_header & MID);
	  } else {
	    predicted_sar_header = (predicted_sar_header + MID_INC) & MID;
	    if (predicted_sar_header == 0)
	      predicted_sar_header = 1;
	    predicted_sar_header |= SSM;
	  }
	}
      }
    }

    control = &nw_tx_control[dev][0];
    for (i = 0; i < MAX_LONG_TX; i++) {
      if (control->ep != 0 && tct[control->ep].reply != TCA_SYNCH) {
	tx_ecb = &ect[control->ep];
	tx_tcb = &tct[control->ep];
	rx_seqno = tx_tcb->reply & MID;
	tx_seqno = tx_tcb->tx_sar_header & MID;
	rx_count = tx_tcb->reply >> 16;
	tx_count = tx_tcb->tx_synch;
	reply = tx_tcb->reply & TCA_SYNCH;
	if (reply == TCA_ACK) {
	  if (rx_seqno == tx_seqno && rx_count == tx_count) {
	    if (rx_count == 0) {
#ifdef TRACING
	      printf("Received final ack ep %d\n", tx_ecb->id);
#endif

	      tx_seqno = (tx_seqno + MID_INC) & MID;
	      if (tx_seqno == 0)
		tx_seqno = 1;
	      tx_tcb->tx_sar_header = tx_seqno;
	      tx_slot_free(dev, control);
	      tx_tcb->reply = NW_SUCCESS;
	      nc_deliver_result(tx_ecb->id, NW_SEND, NW_SUCCESS);
	    } else {
	      if (tca100_window_send(dev, tx_ecb, tx_tcb,
				     FALSE) == NW_SUCCESS) {
		nc_deliver_result(control->ep, NW_SEND, NW_SUCCESS);
		tx_tcb->reply = NW_SUCCESS;
		tx_slot_free(dev, control);
	      } else {
		control->time_out = tick[dev] + BASE_TIME_OUT;
		tx_tcb->reply = TCA_SYNCH;
	      }
	    }
	  } else {
	    goto synch_error;
	  }
	} else if (reply == TCA_OVR) {
	  if (rx_seqno == tx_seqno && rx_count == 0xffff &&
	      ((int) tx_ecb->tx_initial->msg_length -
	       (int) tx_tcb->tx_synch) <= (int) SMALL_WINDOW_SIZE) {
	    nc_deliver_result(control->ep, NW_SEND, NW_OVERRUN);
	    tx_tcb->reply = NW_OVERRUN;
	    tx_slot_free(dev, control);
	  } else {
	    goto synch_error;
	  }
	} else if (reply == TCA_NAK) {
	  if (rx_seqno == tx_seqno &&
	      (rx_count == tx_count || (rx_count == 0xffff &&
                 ((int) tx_ecb->tx_initial->msg_length -
		  (int) tx_tcb->tx_synch) <= (int) SMALL_WINDOW_SIZE))) {
	    if (++control->retry < MAX_RETRY) {
	      if (tca100_verbose)
		      printf("Sending retransmission ep %d\n", tx_ecb->id);
	      if (tca100_window_send(dev, tx_ecb, tx_tcb,
				     TRUE) == NW_SUCCESS) {
		nc_deliver_result(control->ep, NW_SEND, NW_SUCCESS);
		tx_tcb->reply = NW_SUCCESS;
		tx_slot_free(dev, control);
	      } else {
		control->time_out = tick[dev] + BASE_TIME_OUT;
		tx_tcb->reply = TCA_SYNCH;
	      }
	      tca_ec[TX_RETRANSMISSION]++;
	    } else {
	      nc_deliver_result(control->ep, NW_SEND, NW_FAILURE);
	      tx_tcb->reply = NW_FAILURE;
	      tx_slot_free(dev, control);
	    }
	  } else {
	    goto synch_error;
	  }
	} else if (reply == TCA_SEQ) {
	  if (rx_count == 0xffff && tx_ecb->protocol == NW_SEQ_PACKET &&
	      ((int) tx_ecb->tx_initial->msg_length -
	       (int) tx_tcb->tx_synch) <= (int) SMALL_WINDOW_SIZE &&
	      rx_seqno == ((((tx_seqno + MID_INC) & MID) == 0) ?
			   1 : tx_seqno + MID_INC)) {
	    tx_tcb->tx_sar_header = rx_seqno;
	    if (tca100_window_send(dev, tx_ecb, tx_tcb, 
				   TRUE) == NW_SUCCESS) {
	      nc_deliver_result(control->ep, NW_SEND, NW_SUCCESS);
	      tx_tcb->reply = NW_SUCCESS;
	      tx_slot_free(dev, control);
	    } else {
	      control->time_out = tick[dev] + BASE_TIME_OUT;
	      tx_tcb->reply = TCA_SYNCH;
	    }
	    tca_ec[TX_RETRANSMISSION]++;
	    if (tca100_verbose)
		    printf("Sending seq retransmission ep %d\n", tx_ecb->id);
	  } else {
	    goto synch_error;
	  }
	} else {
 synch_error:
	  tca_ec[SYNCH_ERROR]++;
	  tx_tcb->reply = NW_FAILURE;
	  if (tca100_verbose)
		  printf("Synch error\n");
	}
      }
      control++;
    }
  }
  *status = ~(RX_COUNT_INTR | RX_EOM_INTR | RX_TIME_INTR);
  tcb->rx_sar_header = predicted_sar_header;
  tcb->rx_p = (u_int *) msg;
  tcb->rx_count = msg_count;
  tcb->rx_next_synch = next_synch;
  *ctl_set = RX_COUNT_INTR;
  return rx_cell_total;
}



void tca100_timer_sweep(int dev) {
  int i, rt;
  u_int reply;
  nw_control_t control;
  nw_ecb_t ecb;
  nw_tcb_t tcb;
  nw_tx_header_t tx_header;
  nw_rx_header_t rx_header;

  tick[dev]++;
  control = &nw_rx_control[dev][0];
  for (i = 0; i < MAX_LONG_RX; i++) {
    if (control->ep != 0 && control->time_out < tick[dev]) {
      rx_slot_free(dev, control);
      tcb = &tct[control->ep];
      nc_buffer_deallocate(tcb->rx_buffer->peer.local_ep, tcb->rx_buffer);
      tcb->rx_sar_header = SSM | (tcb->rx_sar_header & MID);
    }
    control++;
  }
  control = &nw_tx_control[dev][0];
  for (i = 0; i < MAX_LONG_TX; i++) {
    if (control->ep != 0 && control->time_out < tick[dev]) {
      ecb = &ect[control->ep];
      tcb = &tct[control->ep];
      if (++control->retry < MAX_RETRY) {
	if (control->retry == 1) 
	  rt = ( /* random() */ + devct[dev].local_addr_2) & 0x000f;
	else
	  rt = ( /* random() */ + devct[dev].local_addr_1
		+ devct[dev].local_addr_2) & 0x00ff;
	control->time_out = tick[dev] + BASE_TIME_OUT + rt;
	tca100_window_send(dev, ecb, tcb, TRUE);
	tca_ec[TX_RETRANSMISSION]++;
      } else {
	nc_deliver_result(control->ep, NW_SEND, NW_TIME_OUT);
	tx_slot_free(dev, control);
      }
    }
    control++;
  }
  if (long_tx_count[dev] + long_rx_count[dev] > 0)
    nc_fast_timer_set(dev);
  else
    tick[dev] = 0;
}

nw_buffer_t tca100_rpc(nw_ep ep, nw_tx_header_t header, nw_options options) {
  nw_result rc;
  nw_buffer_t buf;
  nw_ecb_t ecb;
  nw_tcb_t tcb;
  nw_rx_header_t rx_header;
  int dev, poll_time, ncells;

  tcb = &tct[ep];
  ecb = &ect[header->peer.local_ep];
  dev = NW_DEVICE(header->peer.rem_addr_1);
  if ((rc = tca100_send(ep, header, options)) == NW_BAD_LENGTH) {
    buf = NW_BUFFER_ERROR;
  } else if (rc == NW_QUEUED) {
    buf = NULL;
  } else {
    poll_time = 0;
    if (rc == NW_SYNCH) {
      while (tcb->reply == TCA_SYNCH && poll_time < POLL_LIMIT) {
	ncells = tca100_poll(dev);
	if (ncells == 0)
	  poll_time += POLL_IDLE_TIME;
	else
	  poll_time += ncells * POLL_CELL_TIME;
      }
    }
    if (tcb->reply != NW_SUCCESS) {
      buf = NW_BUFFER_ERROR;
    } else {
      while (ecb->rx_first == NULL && poll_time < POLL_LIMIT) {
	ncells = tca100_poll(dev);
	if (ncells == 0)
	  poll_time += POLL_IDLE_TIME;
	else
	  poll_time += ncells * POLL_CELL_TIME;
      }
      if (ecb->rx_first == NULL) {
	buf = NULL;
      } else {
	rx_header = ecb->rx_first;
	buf = rx_header->buffer;
	ecb->rx_first = rx_header->next;
	if (ecb->rx_first == NULL)
	  ecb->rx_last = NULL;
	nc_rx_header_deallocate(rx_header);
      }
    }
  }

  return buf;
}









