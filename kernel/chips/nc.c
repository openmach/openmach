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

#ifndef STUB
#include <chips/nc.h>
#else
#include "nc.h"
#endif

/*** Types and data structures ***/

#if PRODUCTION
#define MAX_HASH 701
#define MAX_HOST 4000
#else
#define MAX_HASH 7
#define MAX_HOST 4
#endif

nw_dev_entry_s nc_failure_entry_table =  {nc_fail, nc_fail,
			   nc_null, nc_null,
			   nc_null_poll, nc_null_send, nc_null_rpc,
			   nc_null_signal, nc_open_fail, nc_accept_fail,
			   nc_close_fail, nc_open_fail, nc_open_fail};
					    
nw_dev_entry_s nc_local_entry_table =  {nc_succeed, nc_succeed,
			   nc_null, nc_null,
			   nc_null_poll, nc_local_send, nc_local_rpc,
			   nc_null_signal, nc_open_fail, nc_accept_fail,
			   nc_close_fail, nc_open_fail, nc_open_fail};


typedef struct {
  nw_address_s address;
  int name_next:16;
  int ip_next:16;
  int nw_next:16;
  nw_ep line:16;
} nw_alist_s, *nw_alist_t;
	       

boolean_t nc_initialized = FALSE;
nw_tx_header_s nw_tx[MAX_EP/2];
nw_tx_header_t nw_free_tx_header;
nw_rx_header_s nw_rx[2*MAX_EP];
nw_rx_header_t nw_free_rx_header;
nw_plist_s nw_peer[MAX_EP];
nw_plist_t nw_free_peer;

nw_devcb devct[MAX_DEV];

nw_ecb ect[MAX_EP];

int nw_free_ep_first, nw_free_ep_last;
int nw_free_line_first, nw_free_line_last;

nw_alist_s nw_address[MAX_HOST];
int nw_free_address;

int nw_name[MAX_HASH];
int nw_ip[MAX_HASH];
int nw_nw[MAX_HASH];

int nw_fast_req;

/*** System-independent functions ***/

void nc_initialize() {
  int ep, last_ep;
  
  if (!nc_initialized) {
    last_ep = sizeof(nw_tx)/sizeof(nw_tx_header_s) - 1;
    for (ep = 0; ep < last_ep; ep++)
      nw_tx[ep].next = &nw_tx[ep+1];
    nw_tx[last_ep].next = NULL;
    nw_free_tx_header = &nw_tx[0];
    last_ep = sizeof(nw_rx)/sizeof(nw_rx_header_s) - 1;
    for (ep = 0; ep < last_ep; ep++)
      nw_rx[ep].next = &nw_rx[ep+1];
    nw_rx[last_ep].next = NULL;
    nw_free_rx_header = &nw_rx[0];
    last_ep = sizeof(nw_peer)/sizeof(nw_plist_s) - 1;
    for (ep = 0; ep < last_ep; ep++)
      nw_peer[ep].next = &nw_peer[ep+1];
    nw_peer[last_ep].next = NULL;
    nw_free_peer = &nw_peer[0];
    for (ep = 0; ep < MAX_DEV; ep++) {
      devct[ep].status = NW_FAILURE;
      devct[ep].type = NW_CONNECTIONLESS;
      devct[ep].addr = NULL;
      devct[ep].local_addr_1 = 0;
      devct[ep].local_addr_2 = 0;
      devct[ep].entry = &nc_failure_entry_table;
      devct[ep].fast_req = 0;
    }
    devct[NW_NULL].status = NW_SUCCESS;
    devct[NW_NULL].entry = &nc_local_entry_table;
    last_ep = sizeof(ect)/sizeof(nw_ecb);
    for (ep = 0; ep < last_ep; ep++) {
      ect[ep].state = NW_INEXISTENT;
      ect[ep].id = ep;
      ect[ep].seqno = 0;
      ect[ep].previous = ep - 1;
      ect[ep].next = ep + 1;
    }
    ect[0].next = ect[0].previous = 0;
    ect[last_ep-1].next = 0;
    nw_free_ep_first = 1;
    nw_free_ep_last = last_ep - 1;
    nw_free_line_first = nw_free_line_last = 0;
    for (ep = 0; ep < MAX_HOST; ep++) {
      nw_address[ep].nw_next = ep + 1;
    }
    nw_address[MAX_HOST - 1].nw_next = -1;
    nw_free_address = 0;
    for (ep = 0; ep < MAX_HASH; ep++) {
      nw_name[ep] = -1;
      nw_ip[ep] = -1;
      nw_nw[ep] = -1;
    }
    nw_fast_req = 0;
    h_initialize();
    nc_initialized = TRUE;
  }
}

nw_tx_header_t nc_tx_header_allocate() {
  nw_tx_header_t header;

  header = nw_free_tx_header;
  if (header != NULL)
    nw_free_tx_header = header->next;
  return header;
}

void nc_tx_header_deallocate(nw_tx_header_t header) {
  nw_tx_header_t first_header;

  first_header = header;
  while (header->next != NULL)
    header = header->next;
  header->next = nw_free_tx_header;
  nw_free_tx_header = first_header;
}

nw_rx_header_t nc_rx_header_allocate() {
  nw_rx_header_t header;

  header = nw_free_rx_header;
  if (header != NULL)
    nw_free_rx_header = header->next;
  return header;
}

void nc_rx_header_deallocate(nw_rx_header_t header) {

  header->next = nw_free_rx_header;
  nw_free_rx_header = header;
}

nw_plist_t nc_peer_allocate() {
  nw_plist_t peer;

  peer = nw_free_peer;
  if (peer != NULL)
    nw_free_peer = peer->next;
  return peer;
}

void nc_peer_deallocate(nw_plist_t peer) {
  nw_plist_t first_peer;

  first_peer = peer;
  while (peer->next != NULL)
    peer = peer->next;
  peer->next = nw_free_peer;
  nw_free_peer = first_peer;
}


nw_result nc_device_register(u_int dev, nw_dev_type type, char *dev_addr,
			     nw_dev_entry_t dev_entry_table) {
  nw_result rc;

  if (dev >= MAX_DEV) {
    rc = NW_FAILURE;
  } else {
    devct[dev].status = NW_SUCCESS;
    devct[dev].type = type;
    devct[dev].addr = dev_addr;
    devct[dev].entry = dev_entry_table;
    devct[dev].fast_req = 0;
    rc = NW_SUCCESS;
  }
  return rc;
}
  
nw_result nc_device_unregister(u_int dev, nw_result status) {
  nw_result rc;

  if (dev >= MAX_DEV) {
    rc = NW_FAILURE;
  } else {
    devct[dev].status = status;
    devct[dev].addr = NULL;
    devct[dev].entry = &nc_failure_entry_table;
    devct[dev].fast_req = 0;
    rc = NW_SUCCESS;
  }
  return rc;
}
  
void nc_slow_sweep() {
  int dev;

  for (dev = 0; dev < MAX_DEV; dev++) {
    if (devct[dev].status == NW_SUCCESS) {
      (*(devct[dev].entry->slow_sweep)) (dev);
    }
  }
}

void nc_fast_timer_set(int dev) {

  devct[dev].fast_req++;
  if (nw_fast_req++ == 0)
    h_fast_timer_set();
}

void nc_fast_timer_reset(int dev) {

  devct[dev].fast_req--;
  if (nw_fast_req-- == 0)
    h_fast_timer_reset();
}


void nc_fast_sweep() {
  int dev;

  for (dev = 0; dev < MAX_DEV; dev++) {
    if (devct[dev].status == NW_SUCCESS &&
	devct[dev].fast_req > 0) {
      devct[dev].fast_req = 0;
      (*(devct[dev].entry->fast_sweep)) (dev);
    }
  }
}

int nc_hash_name(char *cp) {
  int h;
  char ch;
  char *cp_end;

  cp_end = cp + 19;
  *cp_end = '\0';
  h = 0;
  ch = *cp++;
  while (ch != '\0') {
    h = (h << 7) + ch;
    ch = *cp++;
    if (ch != '\0') {
      h = (h << 7) + ch;
      ch = *cp++;
      if (ch != '\0') {
	h = (h << 7) + ch;
	ch = *cp++;
      }
    }
    h %= MAX_HASH;
  }
  return h;
}


nw_result nc_update(nw_update_type up_type, int *up_info) {
  nw_result rc;
  nw_alist_t ad;
  int h, slot, previous_slot, found_slot;
  nw_address_1 n1;
  nw_address_2 n2;

  if (up_type == NW_HOST_ADDRESS_REGISTER) {
    if (nw_free_address == -1) {
      rc = NW_NO_RESOURCES;
    } else {
      slot = nw_free_address;
      ad = &nw_address[slot];
      nw_free_address = ad->nw_next;
      ad->address = *((nw_address_t) up_info);
      h = nc_hash_name(ad->address.name);
      ad->name_next = nw_name[h];
      nw_name[h] = slot;
      h = ad->address.ip_addr % MAX_HASH;
      ad->ip_next = nw_ip[h];
      nw_ip[h] = slot;
      h = (ad->address.nw_addr_1 % MAX_HASH + ad->address.nw_addr_2)
	      % MAX_HASH;
      ad->nw_next = nw_nw[h];
      nw_nw[h] = slot;
      ad->line = 0;
      rc = NW_SUCCESS;
    }
  } else if (up_type == NW_HOST_ADDRESS_UNREGISTER) {
    n1 = ((nw_address_t) up_info)->nw_addr_1;
    n2 = ((nw_address_t) up_info)->nw_addr_2;
    h = (n1 % MAX_HASH + n2) % MAX_HASH;
    slot = nw_nw[h];
    previous_slot = -1;
    ad = &nw_address[slot];
    while (slot != -1 && (ad->address.nw_addr_1 != n1 ||
			  ad->address.nw_addr_2 != n2)) {
      previous_slot = slot;
      slot = ad->nw_next;
      ad = &nw_address[slot];
    }
    if (slot == -1) {
      rc = NW_BAD_ADDRESS;
    } else {
      if (previous_slot == -1)
	nw_nw[h] = ad->nw_next;
      else
	nw_address[previous_slot].nw_next = ad->nw_next;
      ad->nw_next = nw_free_address;
      nw_free_address = slot;
      found_slot = slot;
      if (ad->address.ip_addr != 0) {
	h = ad->address.ip_addr % MAX_HASH;
	slot = nw_ip[h];
	previous_slot = -1;
	while (slot != -1 && slot != found_slot) {
	  previous_slot = slot;
	  slot = nw_address[slot].ip_next;
	}
	if (slot == found_slot) {
	  if (previous_slot == -1)
	    nw_ip[h] = ad->ip_next;
	  else
	    nw_address[previous_slot].ip_next = ad->ip_next;
	}
      }
      if (ad->address.name[0] != '\0') {
	h = nc_hash_name(ad->address.name);
	slot = nw_name[h];
	previous_slot = -1;
	while (slot != -1 && slot != found_slot) {
	  previous_slot = slot;
	  slot = nw_address[slot].name_next;
	}
	if (slot == found_slot) {
	  if (previous_slot == -1)
	    nw_name[h] = ad->name_next;
	  else
	    nw_address[previous_slot].name_next = ad->name_next;
	}
      }
      rc = NW_SUCCESS;
    }
  } else {
    rc = NW_INVALID_ARGUMENT;
  }
  return rc;
}

nw_result nc_lookup(nw_lookup_type lt, int *look_info) {
  nw_result rc;
  nw_address_t addr;
  nw_alist_t ad;
  int h, slot;
  ip_address ip;
  nw_address_1 n1;
  nw_address_2 n2;

  if (lt == NW_HOST_ADDRESS_LOOKUP) {
    addr = (nw_address_t) look_info;
    if (addr->ip_addr != 0) {
      ip = addr->ip_addr;
      h = ip % MAX_HASH;
      slot = nw_ip[h];
      ad = &nw_address[slot];
      while (slot != -1 && ad->address.ip_addr != ip) {
	slot = ad->ip_next;
	ad = &nw_address[slot];
      }
      if (slot != -1) {
	strcpy(addr->name, ad->address.name);
	addr->nw_addr_1 = ad->address.nw_addr_1;
	addr->nw_addr_2 = ad->address.nw_addr_2;
	return NW_SUCCESS;
      }
    }
    if (addr->name[0] != '\0') {
      h = nc_hash_name(addr->name);
      slot = nw_name[h];
      ad = &nw_address[slot];
      while (slot != -1 && strcmp(ad->address.name, addr->name) != 0) {
	slot = ad->name_next;
	ad = &nw_address[slot];
      }
      if (slot != -1) {
	addr->ip_addr = ad->address.ip_addr;
	addr->nw_addr_1 = ad->address.nw_addr_1;
	addr->nw_addr_2 = ad->address.nw_addr_2;
	return NW_SUCCESS;
      }
    }
    if (addr->nw_addr_1 != 0 || addr->nw_addr_2 != 0) {
      n1 = addr->nw_addr_1;
      n2 = addr->nw_addr_2;
      h = (n1 % MAX_HASH + n2) % MAX_HASH;
      slot = nw_nw[h];
      ad = &nw_address[slot];
      while (slot != -1 && (ad->address.nw_addr_1 != n1 ||
			    ad->address.nw_addr_2 != n2)) {
	slot = ad->nw_next;
	ad = &nw_address[slot];
      }
      if (slot != -1) {
	strcpy(addr->name, ad->address.name);
	addr->ip_addr = ad->address.ip_addr;
	return NW_SUCCESS;
      }
    }
    rc = NW_BAD_ADDRESS;
  } else {
    rc = NW_INVALID_ARGUMENT;
  }
  return rc;
}

nw_result nc_line_update(nw_peer_t peer, nw_ep line) {
  nw_result rc;
  nw_alist_t ad;
  int h, slot;
  nw_address_1 n1;
  nw_address_2 n2;

  n1 = peer->rem_addr_1;
  n2 = peer->rem_addr_2;
  h = (n1 % MAX_HASH + n2) % MAX_HASH;
  slot = nw_nw[h];
  ad = &nw_address[slot];
  while (slot != -1 && (ad->address.nw_addr_1 != n1 ||
			ad->address.nw_addr_2 != n2)) {
    slot = ad->nw_next;
    ad = &nw_address[slot];
  }
  if (slot == -1) {
    rc = NW_FAILURE;
  } else {
    ad->line = line;
    rc = NW_SUCCESS;
  }
  return rc;
}

nw_ep nc_line_lookup(nw_peer_t peer) {
  nw_ep lep;
  nw_alist_t ad;
  int h, slot;
  nw_address_1 n1;
  nw_address_2 n2;

  n1 = peer->rem_addr_1;
  n2 = peer->rem_addr_2;
  h = (n1 % MAX_HASH + n2) % MAX_HASH;
  slot = nw_nw[h];
  ad = &nw_address[slot];
  while (slot != -1 && (ad->address.nw_addr_1 != n1 ||
			ad->address.nw_addr_2 != n2)) {
    slot = ad->nw_next;
    ad = &nw_address[slot];
  }
  if (slot == -1) {
    lep = -1;
  } else {
    lep = ad->line;
  }
  return lep;
}

nw_result nc_endpoint_allocate(nw_ep_t epp, nw_protocol protocol,
			       nw_acceptance accept,
			       char *buffer_address, u_int buffer_size) {
  nw_result rc;
  nw_ep ep;
  nw_ecb_t ecb;

  if (ect[(ep = *epp)].state != NW_INEXISTENT) {
    rc = NW_BAD_EP;
  } else if (nw_free_ep_first == 0) {
    *epp = nw_free_line_first;
    rc = NW_NO_EP;
  } else {
    if (ep == 0) {
      ecb = &ect[nw_free_ep_first];
      *epp = ep = ecb->id;
      nw_free_ep_first = ecb->next;
      if (nw_free_ep_first == 0)
	nw_free_ep_last = 0;
    } else {
      ecb = &ect[ep];
      if (ecb->previous == 0)
	nw_free_ep_first = ecb->next;
      else
	ect[ecb->previous].next = ecb->next;
      if (ecb->next == 0)
	nw_free_ep_last = ecb->previous;
      else
	ect[ecb->next].previous = ecb->previous;
    }
    if (protocol == NW_LINE) {
      if (nw_free_line_last == 0)
	nw_free_line_first = ep;
      else
	ect[nw_free_line_last].next = ep;
      ecb->previous = nw_free_line_last;
      ecb->next = 0;
      nw_free_line_last = ep;
    }
    ecb->protocol = protocol;
    ecb->accept = accept;
    ecb->state = NW_UNCONNECTED;
    ecb->conn = NULL;
    ecb->buf_start = buffer_address;
    ecb->buf_end = buffer_address + buffer_size;
    ecb->free_buffer = (nw_unused_buffer_t) buffer_address;
    ecb->free_buffer->buf_used = 0;
    ecb->free_buffer->buf_length = buffer_size;
    ecb->free_buffer->previous = NULL;
    ecb->free_buffer->next = NULL;
    ecb->overrun = 0;
    ecb->seqno = 0;
    ecb->tx_first = NULL;
    ecb->tx_last = NULL;
    ecb->tx_initial = NULL;
    ecb->tx_current = NULL;
    ecb->rx_first = NULL;
    ecb->rx_last = NULL;
    rc = NW_SUCCESS;
  }
  return rc;
}

nw_result nc_endpoint_deallocate(nw_ep ep) {
  nw_ecb_t ecb;
  nw_rx_header_t rx_header;

  ecb = &ect[ep];
  if (ecb->conn != NULL) 
    nc_peer_deallocate(ecb->conn);
  if (ecb->tx_first != NULL)
    nc_tx_header_deallocate(ecb->tx_first);
  if (ecb->tx_initial != NULL)
    nc_tx_header_deallocate(ecb->tx_initial);
  while (ecb->rx_first != NULL) {
    rx_header = ecb->rx_first;
    ecb->rx_first = rx_header->next;
    nc_rx_header_deallocate(rx_header);
  }
  if (ecb->protocol == NW_LINE) {
    if (ecb->previous == 0)
      nw_free_line_first = ecb->next;
    else
      ect[ecb->previous].next = ecb->next;
    if (ecb->next == 0)
      nw_free_line_last = ecb->previous;
    else
      ect[ecb->next].previous = ecb->previous;
  }
  ecb->next = 0;
  ecb->previous = nw_free_ep_last;
  if (nw_free_ep_last == 0)
    nw_free_ep_first = ep;
  else
    ect[nw_free_ep_last].next = ep;
  nw_free_ep_last = ep;
  ecb->id = ep;
  ecb->state = NW_INEXISTENT;
  return NW_SUCCESS;
}

void nc_buffer_coalesce(nw_ecb_t ecb) {
  nw_unused_buffer_t p, q, buf_free, buf_start, buf_end;

  buf_start = p = (nw_unused_buffer_t) ecb->buf_start;
  buf_end = (nw_unused_buffer_t) ecb->buf_end;
  buf_free = NULL;
  while (p >= buf_start && p < buf_end) {
    if (p->buf_length & 0x3)
      goto trash_area;
    if (p->buf_used) {
      p = (nw_unused_buffer_t) ((char *) p + p->buf_length);
    } else {
      q = (nw_unused_buffer_t) ((char *) p + p->buf_length);
      while (q >= buf_start && q < buf_end && !q->buf_used) {
	if (q->buf_length & 0x3)
	  goto trash_area;
	p->buf_length += q->buf_length;
	q = (nw_unused_buffer_t) ((char *) q + q->buf_length);
      }
      p->next = buf_free;
      p->previous = NULL;
      if (buf_free != NULL)
	buf_free->previous = p;
      buf_free = p;
      p = q;
    }
  }
  ecb->free_buffer = buf_free;
  return;

 trash_area:
  ecb->free_buffer = NULL;
  return;
}

  
nw_buffer_t nc_buffer_allocate(nw_ep ep, u_int size) {
  nw_ecb_t ecb;
  nw_unused_buffer_t buf, buf_start, buf_end;

  ecb = &ect[ep];
  buf_start = (nw_unused_buffer_t) ecb->buf_start;
  buf_end = (nw_unused_buffer_t) (ecb->buf_end - sizeof(nw_buffer_s));
  if (size < sizeof(nw_buffer_s))
    size = sizeof(nw_buffer_s);
  else
    size = ((size + 3) >> 2) << 2;
  buf = ecb->free_buffer;
  if (buf != NULL) {
    while (buf->buf_length < size) {
      buf = buf->next;
      if (buf < buf_start || buf > buf_end || ((int) buf & 0x3)) {
	buf = NULL;
	break;
      }
    }
  }
  if (buf == NULL) {
    nc_buffer_coalesce(ecb);
    buf = ecb->free_buffer;
    while (buf != NULL && buf->buf_length < size)
      buf = buf->next;
  }
  if (buf == NULL) {
    ecb->overrun = 1;
  } else {
    if (buf->buf_length < size + sizeof(nw_buffer_s)) {
      if (buf->previous == NULL)
	ecb->free_buffer = buf->next;
      else
	buf->previous->next = buf->next;
      if (buf->next != NULL)
	buf->next->previous = buf->previous;
    } else {
      buf->buf_length -= size;
      buf = (nw_unused_buffer_t) ((char *) buf + buf->buf_length);
      buf->buf_length = size;
    }
    buf->buf_used = 1;
  }
  return (nw_buffer_t) buf;
}

nw_result nc_buffer_deallocate(nw_ep ep, nw_buffer_t buffer) {
  nw_ecb_t ecb;
  nw_unused_buffer_t buf;

  ecb = &ect[ep];
  buf = (nw_unused_buffer_t) buffer;
  buf->buf_used = 0;
  buf->previous = NULL;
  buf->next = ecb->free_buffer;
  if (ecb->free_buffer != NULL)
    ecb->free_buffer->previous = buf;
  ecb->free_buffer = buf;
  return NW_SUCCESS;
}

nw_result nc_endpoint_status(nw_ep ep, nw_state_t state, nw_peer_t peer) {
  nw_result rc;
  nw_ecb_t ecb;

  ecb = &ect[ep];
  *state = ecb->state;
  if (ecb->conn)
    *peer = ecb->conn->peer;
  if (ecb->overrun) {
    ecb->overrun = 0;
    rc = NW_OVERRUN;
  } else if (ecb->rx_first != NULL) {
    rc = NW_QUEUED;
  } else {
    rc = NW_SUCCESS;
  }
  return rc;
}


nw_result nc_local_send(nw_ep ep, nw_tx_header_t header, nw_options options) {
  nw_result rc;
  nw_ep receiver;
  int length;
  nw_buffer_t buffer;
  nw_tx_header_t first_header;
  nw_rx_header_t rx_header;
  char *bufp;
  nw_ecb_t ecb;

  receiver = header->peer.remote_ep;
  length = header->msg_length;
  buffer = nc_buffer_allocate(receiver, sizeof(nw_buffer_s) + length);
  if (buffer == NULL) {
    rc = NW_OVERRUN;
  } else {
    buffer->buf_next = NULL;
    buffer->block_offset = sizeof(nw_buffer_s);
    buffer->block_length = length;
    buffer->peer.rem_addr_1 = NW_NULL << 28;
    buffer->peer.rem_addr_2 = 0;
    buffer->peer.remote_ep = ep;
    buffer->peer.local_ep = receiver;
    bufp = (char *) buffer + sizeof(nw_buffer_s);
    first_header = header;
    while (header != NULL) {
      length = header->block_length;
      bcopy(header->block, bufp, length);
      bufp += length;
      if (header->buffer != NULL) 
	nc_buffer_deallocate(ep, header->buffer);
      header = header->next;
    }
    nc_tx_header_deallocate(first_header);
    ecb = &ect[receiver];
    if (options == NW_URGENT) {
      buffer->msg_seqno = 0;
      if (nc_deliver_result(receiver, NW_RECEIVE_URGENT, (int) buffer))
	rc = NW_SUCCESS;
      else
	rc = NW_NO_RESOURCES;
    } else {
      if (ecb->seqno == 1023)
	buffer->msg_seqno = ecb->seqno = 1;
      else
	buffer->msg_seqno = ++ecb->seqno;
      if (nc_deliver_result(receiver, NW_RECEIVE, (int) buffer)) 
	rc = NW_SUCCESS;
      else
	rc = NW_NO_RESOURCES;
    }
  }
  return rc;
}

nw_buffer_t nc_local_rpc(nw_ep ep, nw_tx_header_t header, nw_options options) {
  nw_buffer_t buf;
  nw_ecb_t ecb;
  nw_rx_header_t rx_header;

  ecb = &ect[ep];
  rx_header = ecb->rx_first;
  if (nc_local_send(ep, header, options) != NW_SUCCESS) {
    buf = NW_BUFFER_ERROR;
  } else if (rx_header == NULL) {
    buf = NULL;
  } else {
    buf = rx_header->buffer;
    ecb->rx_first = rx_header->next;
    if (ecb->rx_first == NULL)
      ecb->rx_last = NULL;
    nc_rx_header_deallocate(rx_header);
  }
  return buf;
}
			 

nw_result nc_succeed(int dev) {

  return NW_SUCCESS;
}
			      
void nc_null(int dev) {

}

nw_result nc_fail(int dev) {

  return NW_FAILURE;
}

int nc_null_poll(int dev) {
  
  return 1000000;
}

nw_result nc_null_send(nw_ep ep, nw_tx_header_t header, nw_options options) {

  return NW_FAILURE;
}

nw_buffer_t nc_null_rpc(nw_ep ep, nw_tx_header_t header, nw_options options) {

  return NW_BUFFER_ERROR;
}

void nc_null_signal(nw_buffer_t msg) {

}

nw_result nc_open_fail(nw_ep lep, nw_address_1 a1,
		       nw_address_2 a2, nw_ep rep) {

  return NW_FAILURE;
}

nw_result nc_close_fail(nw_ep ep) {

  return NW_FAILURE;
}

nw_result nc_accept_fail(nw_ep ep, nw_buffer_t msg, nw_ep_t epp) {

  return NW_FAILURE;
}

