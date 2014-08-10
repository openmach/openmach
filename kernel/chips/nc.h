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

/*** NETWORK INTERFACE IMPLEMENTATION CORE ***/

#ifndef _NC_H_
#define _NC_H_

#ifndef STUB
#include <chips/nw.h>
#else
#include "nw.h"
#endif

/*** Types and data structures ***/

#if PRODUCTION
#define MAX_EP 1024
#define MAX_DEV 16
#else
#define MAX_EP 32
#define MAX_DEV 3
#endif

#define MASTER_LINE_EP 0
#define SIGNAL_EP 1

typedef struct nw_tx_headers {
  nw_buffer_t buffer;
  u_int msg_length;
  char *block;
  u_int block_length;
  nw_peer_s peer;
  nw_ep sender;
  nw_options options;
  struct nw_tx_headers *next;
} nw_tx_header_s;
 
typedef nw_tx_header_s *nw_tx_header_t;

typedef struct nw_rx_headers {
  nw_buffer_t buffer;
  nw_ep receiver;
  u_int reply;
  int time_stamp;
  struct nw_rx_headers *next;
} nw_rx_header_s, *nw_rx_header_t;

typedef enum {
  NW_CONNECTIONLESS,
  NW_CONNECTION_ORIENTED
} nw_dev_type;


typedef struct {
  nw_result (*initialize)(int);
  nw_result (*status)(int);
  void      (*slow_sweep)(int);
  void      (*fast_sweep)(int);
  int       (*poll)(int);
  nw_result (*send)(nw_ep, nw_tx_header_t, nw_options);
  nw_buffer_t (*rpc)(nw_ep, nw_tx_header_t, nw_options);
  void      (*signal)(nw_buffer_t);
  nw_result (*open)(nw_ep, nw_address_1, nw_address_2, nw_ep);
  nw_result (*accept)(nw_ep, nw_buffer_t, nw_ep_t);
  nw_result (*close)(nw_ep);
  nw_result (*add)(nw_ep, nw_address_1, nw_address_2, nw_ep);
  nw_result (*drop)(nw_ep, nw_address_1, nw_address_2, nw_ep);
} nw_dev_entry_s, *nw_dev_entry_t;
  
typedef struct {
  nw_result status;
  nw_dev_type type;
  char *addr;
  nw_address_1 local_addr_1;
  nw_address_2 local_addr_2;
  nw_dev_entry_t entry;
  int fast_req;
} nw_devcb;

extern nw_devcb devct[MAX_DEV];

typedef struct plists {
  nw_peer_s peer;
  struct plists *next;
} nw_plist_s, *nw_plist_t;

typedef struct nw_unused_buffers {
  u_int buf_used:1;
  u_int buf_length:31;
  struct nw_unused_buffers *next;
  struct nw_unused_buffers *previous;
} nw_unused_buffer_s, *nw_unused_buffer_t;

typedef struct ecbs{
  nw_protocol protocol;
  nw_acceptance accept;
  nw_state state;
  nw_plist_t conn;
  char *buf_start;
  char *buf_end;
  nw_unused_buffer_t free_buffer;
  nw_ep id:16;
  u_int overrun:1;
  u_int seqno:14;
  nw_tx_header_t tx_first;
  nw_tx_header_t tx_last;
  nw_tx_header_t tx_initial;
  nw_tx_header_t tx_current;
  nw_rx_header_t rx_first;
  nw_rx_header_t rx_last;
  nw_ep next:16;
  nw_ep previous:16;
} nw_ecb, *nw_ecb_t;

extern nw_ecb ect[MAX_EP];

extern int nw_free_ep_first, nw_free_ep_last;
extern int nw_free_line_first, nw_free_line_last;

typedef enum {
  NW_RECEIVE,
  NW_RECEIVE_URGENT,
  NW_SEND,
  NW_SIGNAL
} nw_delivery;


/*** System-independent functions implemented in core ***/

extern void nc_initialize();

extern nw_tx_header_t nc_tx_header_allocate();

extern void nc_tx_header_deallocate(nw_tx_header_t header);

extern nw_rx_header_t nc_rx_header_allocate();

extern void nc_rx_header_deallocate(nw_rx_header_t header);

extern nw_plist_t nc_peer_allocate();

extern void nc_peer_deallocate(nw_plist_t peer);

extern nw_result nc_device_register(u_int dev, nw_dev_type type,
				    char *dev_addr,
				    nw_dev_entry_t dev_entry_table);

extern nw_result nc_device_unregister(u_int dev, nw_result status);

extern void nc_fast_sweep();

extern void nc_fast_timer_set();

extern void nc_fast_timer_reset();

extern void nc_slow_sweep();

extern nw_result nc_update(nw_update_type up_type, int *up_info);

extern nw_result nc_lookup(nw_lookup_type lt, int *look_info);

extern nw_result nc_line_update(nw_peer_t peer, nw_ep line);

extern nw_ep nc_line_lookup(nw_peer_t peer);

extern nw_result nc_endpoint_allocate(nw_ep_t epp, nw_protocol protocol,
				      nw_acceptance accept,
				      char *buffer_address, u_int buffer_size);

extern nw_result nc_endpoint_deallocate(nw_ep ep);

extern nw_buffer_t nc_buffer_allocate(nw_ep ep, u_int size);

extern nw_result nc_buffer_deallocate(nw_ep ep, nw_buffer_t buffer);

extern nw_result nc_endpoint_status(nw_ep ep,
				    nw_state_t state, nw_peer_t peer);


/* System-dependent function implemented in wrapper*/

extern boolean_t nc_deliver_result(nw_ep ep, nw_delivery type, int result);

/* Support required in wrapper */

extern void h_initialize();

extern void h_fast_timer_set();

extern void h_fast_timer_reset();


/* Stubs for device table */

extern nw_result nc_succeed(int);
extern nw_result nc_fail(int);
extern void nc_null(int);
extern int nc_null_poll(int);
extern nw_result nc_null_send(nw_ep, nw_tx_header_t, nw_options);
extern nw_buffer_t nc_null_rpc(nw_ep, nw_tx_header_t, nw_options);
extern nw_result nc_local_send(nw_ep, nw_tx_header_t, nw_options);
extern nw_buffer_t nc_local_rpc(nw_ep, nw_tx_header_t, nw_options);
extern void nc_null_signal(nw_buffer_t);
extern nw_result nc_open_fail(nw_ep, nw_address_1, nw_address_2, nw_ep);
extern nw_result nc_accept_fail(nw_ep, nw_buffer_t, nw_ep_t);
extern nw_result nc_close_fail(nw_ep);

#endif /* _NC_H_ */
