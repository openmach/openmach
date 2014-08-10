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
 *  School of Computer Scienctxe
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*** MACH KERNEL WRAPPER ***/

#ifndef STUB
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/eventcount.h>
#include <kern/time_out.h>	      
#include <machine/machspl.h>		/* spl definitions */
#include <vm/vm_kern.h>
#include <chips/nc.h>
#include <chips/nw_mk.h>

decl_simple_lock_data(, nw_simple_lock);
u_int previous_spl;

#define nw_lock() \
  previous_spl = splimp(); \
  simple_lock(&nw_simple_lock)

#define nw_unlock() \
  simple_unlock(&nw_simple_lock); \
  splx(previous_spl)

typedef struct nw_pvs {
  task_t owner;
  char *buf_start;
  char *buf_end;
  struct nw_pvs *next;
} nw_pv_s, *nw_pv_t;

typedef struct nw_waiters {
  thread_t waiter;
  struct nw_waiters *next;
} nw_waiter_s, *nw_waiter_t;

typedef struct {
  nw_pv_t pv;
  thread_t sig_waiter;
  nw_waiter_t rx_first;
  nw_waiter_t rx_last;
  nw_waiter_t tx_first;
  nw_waiter_t tx_last;
} nw_hecb, *nw_hecb_t;

#else
#include "nc.h"
#include "nw_mk.h"
#endif

/*** Types and data structures ***/

int h_initialized = FALSE;
nw_pv_s nw_pv[2*MAX_EP];
nw_pv_t nw_free_pv;
nw_waiter_s nw_waiter[2*MAX_EP];
nw_waiter_t nw_free_waiter;
nw_ep_owned_s nw_waited[3*MAX_EP];
nw_ep_owned_t nw_free_waited;
nw_hecb hect[MAX_EP];
timer_elt_data_t nw_fast_timer, nw_slow_timer;

/*** Initialization ***/

void h_initialize() {
  int ep, last_ep;

  if (!h_initialized) {
    last_ep = sizeof(nw_pv)/sizeof(nw_pv_s) - 1;
    for (ep = 0; ep < last_ep; ep++) {
      nw_pv[ep].next = &nw_pv[ep+1];
    }
    nw_pv[last_ep].next = NULL;
    nw_free_pv = &nw_pv[0];
    last_ep = sizeof(nw_waiter)/sizeof(nw_waiter_s) - 1;
    for (ep = 0; ep < last_ep; ep++) {
      nw_waiter[ep].next = &nw_waiter[ep+1];
    }
    nw_waiter[last_ep].next = NULL;
    nw_free_waiter = &nw_waiter[0];
    last_ep = sizeof(nw_waited)/sizeof(nw_ep_owned_s) - 1;
    for (ep = 0; ep < last_ep; ep++) {
      nw_waited[ep].next = &nw_waited[ep+1];
    }
    nw_waited[last_ep].next = NULL;
    nw_free_waited = &nw_waited[0];
    last_ep = sizeof(hect)/sizeof(nw_hecb);
    for (ep = 0; ep < last_ep; ep++) {
      hect[ep].pv = NULL;
      hect[ep].sig_waiter = NULL;
      hect[ep].rx_first = NULL;
      hect[ep].rx_last = NULL;
      hect[ep].tx_first = NULL;
      hect[ep].tx_last = NULL;
    }
    nw_fast_timer.fcn = mk_fast_sweep;
    nw_fast_timer.param = NULL;
    nw_fast_timer.set = TELT_UNSET;
    nw_slow_timer.fcn = mk_slow_sweep;
    nw_slow_timer.param = NULL;
#if PRODUCTION
    set_timeout(&nw_slow_timer, 2*hz);
#endif
    h_initialized = TRUE;
  }
}

/*** User-trappable functions ***/  

nw_result mk_update(mach_port_t master_port, nw_update_type up_type,
		    int *up_info) {
  nw_result rc;

  if (master_port == 0) {          /* XXX */
    rc = NW_FAILURE;
  } else {
    nw_lock();
    switch (up_type) {
    case NW_HOST_ADDRESS_REGISTER:
    case NW_HOST_ADDRESS_UNREGISTER:
      if (invalid_user_access(current_task()->map, (vm_offset_t) up_info,
			      (vm_offset_t) up_info + sizeof(nw_address_s) - 1,
			      VM_PROT_READ | VM_PROT_WRITE)) {
	rc = NW_INVALID_ARGUMENT;
      } else {
	rc = nc_update(up_type, up_info);
      }
      break;
    case NW_INITIALIZE:
      nc_initialize();
      rc = NW_SUCCESS;
      break;
    default:
      rc = NW_INVALID_ARGUMENT;
    }
    nw_unlock();
  }
  return rc;
}



nw_result mk_lookup(nw_lookup_type lt, int *look_info) {
  nw_result rc;
  int max_size, dev;
  
  nw_lock();
  switch (lt) {
  case NW_HOST_ADDRESS_LOOKUP:
    if (invalid_user_access(current_task()->map, (vm_offset_t) look_info,
			    (vm_offset_t) look_info + sizeof(nw_address_s) - 1,
			    VM_PROT_READ | VM_PROT_WRITE)) {
      rc = NW_INVALID_ARGUMENT;
    } else {
      rc = nc_lookup(lt, look_info);
    }
    break;
  case NW_STATUS:
    max_size = sizeof(nw_device);
    if (max_size < sizeof(nw_result))
      max_size = sizeof(nw_result);
    if (invalid_user_access(current_task()->map, (vm_offset_t) look_info,
			    (vm_offset_t) look_info + max_size - 1,
			    VM_PROT_READ | VM_PROT_WRITE) ||
	(dev = look_info[0]) >= MAX_DEV || dev < 0) {
      rc = NW_INVALID_ARGUMENT;
    } else {
      if (devct[dev].status != NW_SUCCESS) {
	look_info[0] = (int) devct[dev].status;
	rc = NW_SUCCESS;
      } else {
	rc = (*(devct[dev].entry->status)) (dev);
      }
    }
    break;
  default:
    rc = NW_INVALID_ARGUMENT;
  }
  nw_unlock();
  return rc;
}


nw_result mk_endpoint_allocate_internal(nw_ep_t epp, nw_protocol protocol,
					nw_acceptance accept,
					u_int buffer_size, boolean_t system) {
  nw_result rc;
  u_int ep;
  vm_offset_t kernel_addr, user_addr;
  nw_pv_t pv;
  nw_ep_owned_t owned;

    ep = *epp;
    if (buffer_size == 0)
      buffer_size = 0x1000;
    else
      buffer_size = (buffer_size + 0xfff) & ~0xfff;
    nw_lock();
    if (ep >= MAX_EP || (pv = hect[ep].pv) != NULL) {
      rc = NW_BAD_EP;
    } else if (nw_free_pv == NULL || nw_free_waited == NULL) {
      rc = NW_NO_EP;
    } else if (projected_buffer_allocate(current_task()->map, buffer_size, 0,
					 &kernel_addr, &user_addr,
					 VM_PROT_READ | VM_PROT_WRITE,
					 VM_INHERIT_NONE) != KERN_SUCCESS) {
      rc = NW_NO_RESOURCES;
    } else {
      rc = nc_endpoint_allocate(epp, protocol, accept,
				(char *) kernel_addr, buffer_size);
      if (rc == NW_NO_EP && (ep = *epp) != 0) {
	rc = (*(devct[NW_DEVICE(ect[ep].conn->peer.rem_addr_1)].entry->
		         close)) (ep);
	if (rc == NW_SYNCH) {
	  hect[ep].sig_waiter = current_thread();
	  assert_wait(0, TRUE);
	  simple_unlock(&nw_simple_lock);
	  thread_block((void (*)()) 0);
	}
	rc = nc_endpoint_deallocate(ep);
	if (rc == NW_SUCCESS) {
	  nc_line_update(&ect[ep].conn->peer, 0);
	  rc = nc_endpoint_allocate(epp, protocol, accept,
				    (char *) kernel_addr, buffer_size);
	}
      }
      if (rc == NW_SUCCESS) {
	ep = *epp;
	if (system) {
	  hect[ep].pv = NULL;
	} else {
	  hect[ep].pv = nw_free_pv;
	  nw_free_pv = nw_free_pv->next;
	  hect[ep].pv->owner = current_task();
	  hect[ep].pv->buf_start = (char *) user_addr;
	  hect[ep].pv->buf_end = (char *) user_addr + buffer_size;
	  hect[ep].pv->next = NULL;
	}
	hect[ep].sig_waiter = NULL;
	hect[ep].rx_first = NULL;
	hect[ep].rx_last = NULL;
	hect[ep].tx_first = NULL;
	hect[ep].tx_last = NULL;
	owned = nw_free_waited;
	nw_free_waited = nw_free_waited->next;
	owned->ep = ep;
	owned->next = current_task()->nw_ep_owned;
	current_task()->nw_ep_owned = owned;
      } else {
	projected_buffer_deallocate(current_task()->map, user_addr,
				    user_addr + buffer_size);
      } 
    }
  nw_unlock();
  return rc;
}


nw_result mk_endpoint_allocate(nw_ep_t epp, nw_protocol protocol,
			       nw_acceptance accept, u_int buffer_size) {
  nw_result rc;

  if (invalid_user_access(current_task()->map, (vm_offset_t) epp,
			  (vm_offset_t) epp + sizeof(nw_ep) - 1,
			  VM_PROT_READ | VM_PROT_WRITE) ||
      (protocol != NW_RAW && protocol != NW_DATAGRAM &&
       protocol != NW_SEQ_PACKET) || (accept != NW_NO_ACCEPT &&
       accept != NW_APPL_ACCEPT && accept  != NW_AUTO_ACCEPT)) {
    rc = NW_INVALID_ARGUMENT;
  } else {
    rc = mk_endpoint_allocate_internal(epp, protocol, accept,
				       buffer_size, FALSE);
  }
  return rc;
}

nw_result mk_endpoint_deallocate_internal(nw_ep ep, task_t task,
					  boolean_t shutdown) {
  nw_result rc;
  nw_pv_t pv, pv_previous;
  nw_ep_owned_t owned, owned_previous;
  nw_waiter_t w, w_previous, w_next;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    pv_previous = NULL;
    while (pv != NULL && pv->owner != task) {
      pv_previous = pv;
      pv = pv->next;
    }
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      if (projected_buffer_deallocate(task->map, pv->buf_start,
				      pv->buf_end) != KERN_SUCCESS) {
	rc = NW_INCONSISTENCY;
	printf("Endpoint deallocate: inconsistency p. buffer\n");
      } else {
	if (pv_previous == NULL)
	  hect[ep].pv = pv->next;
	else
	  pv_previous->next = pv->next;
	pv->next = nw_free_pv;
	nw_free_pv = pv;
	owned = task->nw_ep_owned;
	owned_previous = NULL;
	while (owned != NULL && owned->ep != ep) {
	  owned_previous = owned;
	  owned = owned->next;
	}
	if (owned == NULL) {
	  rc = NW_INCONSISTENCY;
	  printf("Endpoint deallocate: inconsistency owned\n");
	} else {
	  if (owned_previous == NULL)
	    task->nw_ep_owned = owned->next;
	  else
	    owned_previous->next = owned->next;
	  owned->next = nw_free_waited;
	  nw_free_waited = owned;
	  if (hect[ep].sig_waiter != NULL &&
	      hect[ep].sig_waiter->task == task) {
/*	    if (!shutdown)*/
	      mk_deliver_result(hect[ep].sig_waiter, NW_ABORTED);
	    hect[ep].sig_waiter = NULL;
	  }
	  w = hect[ep].rx_first;
	  w_previous = NULL;
	  while (w != NULL) {
	    if (w->waiter->task == task) {
/*	      if (!shutdown)*/
		mk_deliver_result(w->waiter, NULL);
	      w_next = w->next;
	      if (w_previous == NULL)
		hect[ep].rx_first = w_next;
	      else
		w_previous->next = w_next;
	      w->next = nw_free_waiter;
	      nw_free_waiter = w;
	      w = w_next;
	    } else {
	      w_previous = w;
	      w = w->next;
	    }
	  }
	  if (hect[ep].rx_first == NULL)
	    hect[ep].rx_last = NULL;
	  w = hect[ep].tx_first;
	  w_previous = NULL;
	  while (w != NULL) {
	    if (w->waiter->task == task) {
/*	      if (!shutdown)*/
		mk_deliver_result(w->waiter, NW_ABORTED);
	      w_next = w->next;
	      if (w_previous == NULL)
		hect[ep].tx_first = w_next;
	      else
		w_previous->next = w_next;
	      w->next = nw_free_waiter;
	      nw_free_waiter = w;
	      w = w_next;
	    } else {
	      w_previous = w;
	      w = w->next;
	    }
	  }
	  if (hect[ep].tx_first == NULL)
	    hect[ep].tx_last = NULL;
	  if (hect[ep].pv == NULL) {
	    if (ect[ep].state != NW_UNCONNECTED) {
	      rc = (*(devct[NW_DEVICE(ect[ep].conn->peer.rem_addr_1)].entry->
		         close)) (ep);
	      if (rc == NW_SYNCH) {
		hect[ep].sig_waiter = current_thread();
		assert_wait(0, TRUE);
		simple_unlock(&nw_simple_lock);
		thread_block((void (*)()) 0);
	      }
	    }
	    rc = nc_endpoint_deallocate(ep);
	  }
	}
      }
    }
  }
  nw_unlock();
  return rc;
}

nw_result mk_endpoint_deallocate(nw_ep ep) {

  mk_endpoint_deallocate_internal(ep, current_task(), FALSE);
}


nw_buffer_t mk_buffer_allocate(nw_ep ep, u_int size) {
  nw_buffer_t buf;
  nw_pv_t pv;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    buf = NW_BUFFER_ERROR;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      buf = NW_BUFFER_ERROR;
    } else {
      buf = nc_buffer_allocate(ep, size);
      if (buf != NULL) {
	buf = (nw_buffer_t) ((char *) buf - ect[ep].buf_start + pv->buf_start);
      }
    }
  }
  nw_unlock();
  return buf;
}



nw_result mk_buffer_deallocate(nw_ep ep, nw_buffer_t buffer) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      if ((char *) buffer < pv->buf_start ||
	  (char *) buffer + sizeof(nw_buffer_s) > pv->buf_end ||
	  !buffer->buf_used ||
	  (char *) buffer + buffer->buf_length > pv->buf_end) {
	rc = NW_BAD_BUFFER;
      } else {
	buffer = (nw_buffer_t) ((char *) buffer - pv->buf_start +
				ect[ep].buf_start);
	rc = nc_buffer_deallocate(ep, buffer);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_connection_open_internal(nw_ep local_ep, nw_address_1 rem_addr_1,
               		      nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;
  
  rc = (*devct[NW_DEVICE(rem_addr_1)].entry->open) (local_ep,
						    rem_addr_1, rem_addr_2,
						    remote_ep);
  if (rc == NW_SYNCH) {
    hect[local_ep].sig_waiter = current_thread();
    assert_wait(0, TRUE);
    simple_unlock(&nw_simple_lock);
    thread_block((void (*)()) 0);
  }
  return rc;
}
  
nw_result mk_connection_open(nw_ep local_ep, nw_address_1 rem_addr_1,
			     nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (local_ep >= MAX_EP || (pv = hect[local_ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      rc = (*(devct[NW_DEVICE(rem_addr_1)].entry->open))
	      (local_ep, rem_addr_1, rem_addr_2, remote_ep);
      if (rc == NW_SYNCH) {
	hect[local_ep].sig_waiter = current_thread();
	assert_wait(0, TRUE);
	current_thread()->nw_ep_waited = NULL;
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_connection_accept(nw_ep ep, nw_buffer_t msg,
			       nw_ep_t new_epp) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else if ((char *) msg < pv->buf_start ||
	       (char *) msg + sizeof(nw_buffer_s) > pv->buf_end ||
	       !msg->buf_used ||
	       (char *) msg + msg->buf_length > pv->buf_end) {
      rc = NW_BAD_BUFFER;
    } else if (new_epp != NULL &&
	       (invalid_user_access(current_task()->map, (vm_offset_t) new_epp,
				   (vm_offset_t) new_epp + sizeof(nw_ep) - 1,
				   VM_PROT_READ | VM_PROT_WRITE) ||
		(*new_epp != 0 && *new_epp != ep))) {
      rc = NW_INVALID_ARGUMENT;
    } else {
      rc = (*(devct[NW_DEVICE(ect[ep].conn->peer.rem_addr_1)].entry->accept))
	      (ep, msg, new_epp);
      if (rc == NW_SYNCH) {
	hect[ep].sig_waiter = current_thread();
	assert_wait(0, TRUE);
	current_thread()->nw_ep_waited = NULL;
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      }
    }
  }
  nw_unlock();
  return rc;
}

nw_result mk_connection_close(nw_ep ep) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      rc = (*devct[NW_DEVICE(ect[ep].conn->peer.rem_addr_1)].entry->close)
	      (ep);
      if (rc == NW_SYNCH) {
	hect[ep].sig_waiter = current_thread();
	assert_wait(0, TRUE);
	current_thread()->nw_ep_waited = NULL;
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_multicast_add(nw_ep local_ep, nw_address_1 rem_addr_1,
			   nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (local_ep >= MAX_EP || (pv = hect[local_ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      rc = (*(devct[NW_DEVICE(rem_addr_1)].entry->add))
 	      (local_ep, rem_addr_1, rem_addr_2, remote_ep);
      if (rc == NW_SYNCH) {
	hect[local_ep].sig_waiter = current_thread();
	assert_wait(0, TRUE);
	current_thread()->nw_ep_waited = NULL;
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_multicast_drop(nw_ep local_ep, nw_address_1 rem_addr_1,
			    nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (local_ep >= MAX_EP || (pv = hect[local_ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      rc = (*(devct[NW_DEVICE(rem_addr_1)].entry->drop))
	      (local_ep, rem_addr_1, rem_addr_2, remote_ep);
      if (rc == NW_SYNCH) {
	hect[local_ep].sig_waiter = current_thread();
	assert_wait(0, TRUE);
	current_thread()->nw_ep_waited = NULL;
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_endpoint_status(nw_ep ep, nw_state_t state,
			     nw_peer_t peer) {
  nw_result rc;
  nw_pv_t pv;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      if (invalid_user_access(current_task()->map, (vm_offset_t) state,
			      (vm_offset_t) state + sizeof(nw_state) - 1,
			      VM_PROT_WRITE) ||
	  invalid_user_access(current_task()->map, (vm_offset_t) peer,
			      (vm_offset_t) peer + sizeof(nw_peer_s) - 1,
			      VM_PROT_WRITE)) {
	rc = NW_INVALID_ARGUMENT;
      } else {
	rc = nc_endpoint_status(ep, state, peer);
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_result mk_send(nw_ep ep, nw_buffer_t msg, nw_options options) {
  nw_result rc;
  nw_pv_t pv;
  nw_ep sender;
  int dev;
  nw_ecb_t ecb;
  nw_tx_header_t header, first_header, previous_header;
  nw_hecb_t hecb;
  nw_waiter_t w;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BAD_EP;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_PROT_VIOLATION;
    } else {
      ecb = &ect[ep];
      if (ecb->state == NW_INEXISTENT ||
	  (ecb->protocol == NW_SEQ_PACKET && ecb->conn == NULL)) {
	rc = NW_BAD_EP;
      } else {
	first_header = header = nc_tx_header_allocate();
	previous_header = NULL;
	rc = NW_SUCCESS;
	while (header != NULL) {
	  if ((char *) msg < pv->buf_start ||
	      (char *) msg + sizeof(nw_buffer_s) > pv->buf_end ||
	      ((int) msg & 0x3) || (msg->block_offset & 0x3) ||
	      (msg->block_length & 0x3) || !msg->buf_used ||
	      (char *) msg + msg->buf_length > pv->buf_end ||
	      msg->block_offset + msg->block_length > msg->buf_length) {
	    rc = NW_BAD_BUFFER;
	    break;
	  } else {
	    if (previous_header == NULL) {
	      if (ecb->protocol == NW_SEQ_PACKET)
		header->peer = ecb->conn->peer;
	      else
		header->peer = msg->peer;
	    } else {
	      previous_header->next = header;
	    }
	    header->buffer = (nw_buffer_t) ((char *) msg - pv->buf_start +
					    ecb->buf_start);
	    header->block = (char *) header->buffer + msg->block_offset;
	    if (!msg->block_deallocate)
	      header->buffer = NULL;
	    header->msg_length = 0;
	    header->block_length = msg->block_length;
	    first_header->msg_length += header->block_length;
	    header->next = NULL;
	    if (msg->buf_next == NULL)
	      break;
	    msg = msg->buf_next;
	    previous_header = header;
	    header = nc_tx_header_allocate();
	  }
	}
	if (header == NULL) {
	  nc_tx_header_deallocate(first_header);
	  rc = NW_NO_RESOURCES;
	} else if (rc == NW_SUCCESS) {
	  dev = NW_DEVICE(first_header->peer.rem_addr_1);
	  if (ecb->protocol != NW_DATAGRAM ||
	      devct[dev].type != NW_CONNECTION_ORIENTED) {
	    sender = first_header->peer.local_ep;
	    rc = NW_SUCCESS;
	  } else {
	    sender = nc_line_lookup(&first_header->peer);
	    if (sender == -1) {
	      rc = NW_BAD_ADDRESS;
	    } else if (sender > 0) {
	      rc = NW_SUCCESS;
	    } else {
	      rc = mk_endpoint_allocate_internal(&sender, NW_LINE,
						 NW_AUTO_ACCEPT, 0, TRUE);
	      if (rc == NW_SUCCESS) {
		rc = mk_connection_open_internal(sender,
			                first_header->peer.rem_addr_1,
					first_header->peer.rem_addr_2,
					MASTER_LINE_EP);
		if (rc == NW_SUCCESS) 
		  nc_line_update(&first_header->peer, sender);
	      }
	    }
	  }
	  if (rc == NW_SUCCESS) {
	    first_header->sender = sender;
	    first_header->options = options;
	    rc = (*(devct[dev].entry->send)) (sender, first_header, options);
	    if ((rc == NW_SYNCH || rc == NW_QUEUED) &&
		nw_free_waiter != NULL) {
	      w = nw_free_waiter;
	      nw_free_waiter = w->next;
	      w->waiter = current_thread();
	      w->next = NULL;
	      hecb = &hect[sender];
	      if (hecb->tx_last == NULL) {
		hecb->tx_first = hecb->tx_last = w;
	      } else {
		hecb->tx_last = hecb->tx_last->next = w;
	      }
	      assert_wait(0, TRUE);
	      current_thread()->nw_ep_waited = NULL;
	      simple_unlock(&nw_simple_lock);
	      thread_block(mk_return);
	    }
	  }
	}
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_buffer_t mk_receive(nw_ep ep, int time_out) {
  nw_buffer_t rc;
  nw_pv_t pv;
  nw_ecb_t ecb;
  nw_rx_header_t header;
  nw_hecb_t hecb;
  nw_waiter_t w;
  nw_ep_owned_t waited;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BUFFER_ERROR;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_BUFFER_ERROR;
    } else {
      ecb = &ect[ep];
      header = ecb->rx_first;
      if (header != NULL) {
	rc = (nw_buffer_t) ((char *) header->buffer - ecb->buf_start +
			     pv->buf_start);
	ecb->rx_first = header->next;
	if (ecb->rx_first == NULL)
	  ecb->rx_last = NULL;
	nc_rx_header_deallocate(header);
      } else if (time_out != 0 && nw_free_waiter != NULL &&
		 (time_out == -1 || nw_free_waited != NULL)) {
	w = nw_free_waiter;
	nw_free_waiter = w->next;
	w->waiter = current_thread();
	w->next = NULL;
	hecb = &hect[ep];
	if (hecb->rx_last == NULL)
	  hecb->rx_first = hecb->rx_last = w;
	else
	  hecb->rx_last = hecb->rx_last->next = w;
	assert_wait(0, TRUE);
	if (time_out != -1) {
	  waited = nw_free_waited;
	  nw_free_waited = waited->next;
	  waited->ep = ep;
	  waited->next = NULL;
	  current_thread()->nw_ep_waited = waited;
	  current_thread()->wait_result = NULL;
	  if (!current_thread()->timer.set) 
	    thread_set_timeout(time_out);
	} else {
	  current_thread()->nw_ep_waited = NULL;
	}
	simple_unlock(&nw_simple_lock);
	thread_block(mk_return);
      } else {
	rc = NULL;
      }
    }
  }
  nw_unlock();
  return rc;
}


nw_buffer_t mk_rpc(nw_ep ep, nw_buffer_t msg, nw_options options,
		   int time_out) {
  nw_buffer_t rc;
  nw_result nrc;
  nw_ep sender;
  int dev;
  nw_pv_t pv;
  nw_ecb_t ecb;
  nw_tx_header_t header, first_header, previous_header;
  nw_hecb_t hecb;
  nw_waiter_t w;
  nw_ep_owned_t waited;

  nw_lock();
  if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
    rc = NW_BUFFER_ERROR;
  } else {
    while (pv != NULL && pv->owner != current_task())
      pv = pv->next;
    if (pv == NULL) {
      rc = NW_BUFFER_ERROR;
    } else {
      ecb = &ect[ep];
      if (ecb->state == NW_INEXISTENT ||
	  (ecb->protocol == NW_SEQ_PACKET && ecb->conn == NULL)) {
	rc = NW_BUFFER_ERROR;
      } else {
	first_header = header = nc_tx_header_allocate();
	previous_header = NULL;
	rc = NULL;
	while (header != NULL) {
	  if ((char *) msg < pv->buf_start ||
	      (char *) msg + sizeof(nw_buffer_s) > pv->buf_end ||
	      ((int) msg & 0x3) || (msg->block_offset & 0x3) ||
	      (msg->block_length & 0x3) || !msg->buf_used ||
	      (char *) msg + msg->buf_length > pv->buf_end ||
	      msg->block_offset + msg->block_length > msg->buf_length) {
	    rc = NW_BUFFER_ERROR;
	    break;
	  } else {
	    if (previous_header == NULL) {
	      if (ecb->protocol == NW_SEQ_PACKET)
		header->peer = ecb->conn->peer;
	      else
		header->peer = msg->peer;
	    } else {
	      previous_header->next = header;
	    }
	    header->buffer = (nw_buffer_t) ((char *) msg - pv->buf_start +
					    ecb->buf_start);
	    header->block = (char *) header->buffer + msg->block_offset;
	    if (!msg->block_deallocate)
	      header->buffer = NULL;
	    header->msg_length = 0;
	    header->block_length = msg->block_length;
	    first_header->msg_length += header->block_length;
	    header->next = NULL;
	    if (msg->buf_next == NULL)
	      break;
	    msg = msg->buf_next;
	    previous_header = header;
	    header = nc_tx_header_allocate();
	  }
	}
	if (header == NULL) {
	  nc_tx_header_deallocate(first_header);
	  rc = NW_BUFFER_ERROR;
	} else if (rc != NW_BUFFER_ERROR) {
	  dev = NW_DEVICE(first_header->peer.rem_addr_1);
	  if (ecb->protocol != NW_DATAGRAM ||
	      devct[dev].type != NW_CONNECTION_ORIENTED) {
	    sender = first_header->peer.local_ep;
	    nrc = NW_SUCCESS;
	  } else {
	    sender = nc_line_lookup(&first_header->peer);
	    if (sender == -1) {
	      nrc = NW_BAD_ADDRESS;
	    } else if (sender > 0) {
	      nrc = NW_SUCCESS;
	    } else {
	      nrc = mk_endpoint_allocate_internal(&sender, NW_LINE,
						  NW_AUTO_ACCEPT, 0, TRUE);
	      if (nrc == NW_SUCCESS) {
		nrc = mk_connection_open_internal(sender,
			                first_header->peer.rem_addr_1,
					first_header->peer.rem_addr_2,
					MASTER_LINE_EP);
		if (nrc == NW_SUCCESS) 
		  nc_line_update(&first_header->peer, sender);
	      }
	    }
	  }
	  if (nrc == NW_SUCCESS) {
	    first_header->sender = sender;
	    first_header->options = options;
	    rc = (*(devct[dev].entry->rpc)) (sender, first_header, options);
	    if (rc != NULL && rc != NW_BUFFER_ERROR) {
	      rc = (nw_buffer_t) ((char *) rc - ecb->buf_start +
				  pv->buf_start);
	    } else if (rc == NULL && time_out != 0 && nw_free_waiter != NULL &&
		       (time_out == -1 || nw_free_waited != NULL)) {
	      w = nw_free_waiter;
	      nw_free_waiter = w->next;
	      w->waiter = current_thread();
	      w->next = NULL;
	      hecb = &hect[ep];
	      if (hecb->rx_last == NULL)
		hecb->rx_first = hecb->rx_last = w;
	      else
		hecb->rx_last = hecb->rx_last->next = w;
	      assert_wait(0, TRUE);
	      if (time_out != -1) {
		waited = nw_free_waited;
		nw_free_waited = waited->next;
		waited->ep = ep;
		waited->next = NULL;
		current_thread()->nw_ep_waited = waited;
		current_thread()->wait_result = NULL;
		if (!current_thread()->timer.set) 
		  thread_set_timeout(time_out);
	      } else {
		current_thread()->nw_ep_waited = NULL;
	      }
	      simple_unlock(&nw_simple_lock);
	      thread_block(mk_return);
	    }
	  }
	}
      }
    }
  }
  nw_unlock();
  return rc;
}

nw_buffer_t mk_select(u_int nep, nw_ep_t epp, int time_out) {
  nw_buffer_t rc;
  nw_pv_t pv;
  int i;
  nw_ep ep;
  nw_ecb_t ecb;
  nw_rx_header_t header;
  nw_hecb_t hecb;
  nw_waiter_t w, w_next;
  nw_ep_owned_t waited;

  if (invalid_user_access(current_task()->map, (vm_offset_t) epp,
			  (vm_offset_t) epp + nep*sizeof(nw_ep) - 1,
			  VM_PROT_READ)) {
    rc = NW_BUFFER_ERROR;
  } else {
    nw_lock();
    for (i = 0; i < nep; i++) {
      ep = epp[i];
      if (ep >= MAX_EP || (pv = hect[ep].pv) == NULL) {
	rc = NW_BUFFER_ERROR;
	break;
      } else {
	while (pv != NULL && pv->owner != current_task())
	  pv = pv->next;
	if (pv == NULL) {
	  rc = NW_BUFFER_ERROR;
	  break;
	} else {
	  ecb = &ect[ep];
	  header = ecb->rx_first;
	  if (header != NULL) {
	    rc = (nw_buffer_t) ((char *) header->buffer - ecb->buf_start +
				 pv->buf_start);
	    ecb->rx_first = header->next;
	    if (ecb->rx_first == NULL)
	      ecb->rx_last = NULL;
	    nc_rx_header_deallocate(header);
	    break;
	  }
	}
      }
    }
    if (i == nep) {
      if (time_out == 0) {
	rc = NULL;
      } else {
	w = nw_free_waiter;
	waited = nw_free_waited;
	i = 0;
	while (i < nep &&
	       nw_free_waiter != NULL && nw_free_waited != NULL) {
	  nw_free_waiter = nw_free_waiter->next;
	  nw_free_waited = nw_free_waited->next;
	  i++;
	}
	if (i < nep) {
	  nw_free_waiter = w;
	  nw_free_waited = waited;
	  rc = NW_BUFFER_ERROR;
	} else {
	  current_thread()->nw_ep_waited = waited;
	  for (i = 0; i < nep; i++) {
	    ep = epp[i];
	    waited->ep = ep;
	    if (i < nep-1)
	      waited = waited->next;
	    else
	      waited->next = NULL;
	    w->waiter = current_thread();
	    w_next = w->next;
	    w->next = NULL;
	    hecb = &hect[ep];
	    if (hecb->rx_last == NULL)
	      hecb->rx_first = hecb->rx_last = w;
	    else
	      hecb->rx_last = hecb->rx_last->next = w;
	    w = w_next;
	  }
	  assert_wait(0, TRUE);
	  if (time_out != -1) {
	    current_thread()->wait_result = NULL;
	    if (!current_thread()->timer.set) 
	      thread_set_timeout(time_out);
	  }
	  simple_unlock(&nw_simple_lock);
	  thread_block(mk_return);
	}
      }
    }
    nw_unlock();
  }
  return rc;
}


/*** System-dependent support ***/

void mk_endpoint_collect(task_t task) {
  
  while (task->nw_ep_owned != NULL) {
    mk_endpoint_deallocate_internal(task->nw_ep_owned->ep, task, TRUE);
  }
}

void mk_waited_collect(thread_t thread) {
  nw_hecb_t hecb;
  nw_waiter_t w, w_previous;
  nw_ep_owned_t waited, waited_previous;

  waited = thread->nw_ep_waited;
  if (waited != NULL) {
    while (waited != NULL) {
      hecb = &hect[waited->ep];
      w = hecb->rx_first;
      w_previous = NULL;
      while (w != NULL && w->waiter != thread) {
	w_previous = w;
	w = w->next;
      }
      if (w != NULL) {
	if (w_previous == NULL)
	  hecb->rx_first = w->next;
	else
	  w_previous->next = w->next;
	if (w->next == NULL)
	  hecb->rx_last = w_previous;
	w->next = nw_free_waiter;
	nw_free_waiter = w;
      }
      waited_previous = waited;
      waited = waited->next;
    }
    waited_previous->next = nw_free_waited;
    nw_free_waited = thread->nw_ep_waited;
    thread->nw_ep_waited = NULL;
  }
}

void mk_return() {

  thread_syscall_return(current_thread()->wait_result);
}
  

boolean_t mk_deliver_result(thread_t thread, int result) {
  boolean_t rc;
  int state, s;
  
  s = splsched();
  thread_lock(thread);
  state = thread->state;

  reset_timeout_check(&thread->timer);

  switch (state & TH_SCHED_STATE) {
  case          TH_WAIT | TH_SUSP | TH_UNINT:
  case          TH_WAIT           | TH_UNINT:
  case          TH_WAIT:
    /*
     *      Sleeping and not suspendable - put on run queue.
     */
    thread->state = (state &~ TH_WAIT) | TH_RUN;
    thread->wait_result = (kern_return_t) result;
    simpler_thread_setrun(thread, TRUE);
    rc = TRUE;
    break;
    
  case          TH_WAIT | TH_SUSP:
  case TH_RUN | TH_WAIT:
  case TH_RUN | TH_WAIT | TH_SUSP:
  case TH_RUN | TH_WAIT           | TH_UNINT:
  case TH_RUN | TH_WAIT | TH_SUSP | TH_UNINT:
    /*
     *      Either already running, or suspended.
     */
    thread->state = state &~ TH_WAIT;
    thread->wait_result = (kern_return_t) result;
    rc = FALSE;
    break;

  default:
    /*
     *      Not waiting.
     */
    rc = FALSE;
    break;
  }
  thread_unlock(thread);
  splx(s);
  return rc;
}


boolean_t nc_deliver_result(nw_ep ep, nw_delivery type, int result) {
  boolean_t rc;
  nw_hecb_t hecb;
  nw_ecb_t ecb;
  nw_waiter_t w;
  thread_t thread;
  task_t task;
  nw_pv_t pv;
  nw_buffer_t buf;
  nw_rx_header_t rx_header;
  nw_tx_header_t tx_header;
  nw_ep lep;

  hecb = &hect[ep];
  ecb = &ect[ep];

  thread = NULL;
  if (type == NW_RECEIVE || type == NW_RECEIVE_URGENT) {
    w = hecb->rx_first;
    if (w != NULL) {
      thread = w->waiter;
      hecb->rx_first = w->next;
      if (hecb->rx_first == NULL)
	hecb->rx_last = NULL;
      w->next = nw_free_waiter;
      nw_free_waiter = w;
      task = thread->task;
      pv = hecb->pv;
      while (pv != NULL && pv->owner != task)
	pv = pv->next;
      if (pv == NULL) {
	rc = FALSE;
      } else {
	buf = (nw_buffer_t) ((char *) result - ecb->buf_start + pv->buf_start);
	rc = mk_deliver_result(thread, (int) buf);
      }
    } else {
      rx_header = nc_rx_header_allocate();
      if (rx_header == NULL) {
	rc = FALSE;
      } else {
	rx_header->buffer = (nw_buffer_t) result;
	if (type == NW_RECEIVE) {
	  rx_header->next = NULL;
	  if (ecb->rx_last == NULL)
	    ecb->rx_first = rx_header;
	  else
	    ecb->rx_last->next = rx_header;
	  ecb->rx_last = rx_header;
	} else {
	  rx_header->next = ecb->rx_first;
	  if (ecb->rx_first == NULL)
	    ecb->rx_last = rx_header;
	  ecb->rx_first = rx_header;
	}
	rc = TRUE;
      } 
    }
  } else if (type == NW_SEND) {
    w = hecb->tx_first;
    if (w == NULL) {
      rc = FALSE;
    } else {
      thread = w->waiter;
      hecb->tx_first = w->next;
      if (hecb->tx_first == NULL)
	hecb->tx_last = NULL;
      w->next = nw_free_waiter;
      nw_free_waiter = w;
      rc = mk_deliver_result(thread, result);
    }
    tx_header = ect[ep].tx_initial;
    if (result == NW_SUCCESS) {
      lep = tx_header->peer.local_ep;
      while (tx_header != NULL) {
	if (tx_header->buffer != NULL)
	  nc_buffer_deallocate(lep, tx_header->buffer);
	tx_header = tx_header->next;
      }
    }
    nc_tx_header_deallocate(ect[ep].tx_initial);
    ect[ep].tx_initial = ect[ep].tx_current = NULL;
  } else if (type == NW_SIGNAL) {
    thread = hecb->sig_waiter;
    hecb->sig_waiter = NULL;
    if (thread == NULL) {
      rc = FALSE;
    } else {
      rc = mk_deliver_result(thread, result);
    }
  }
  return rc;
}
    
int mk_fast_sweep() {

  nw_lock();
  nc_fast_sweep();
  nw_unlock();
  return 0;
}

void h_fast_timer_set() {
  
#ifdef PRODUCTION
  if (!nw_fast_timer.set)
    set_timeout(&nw_fast_timer, 1);
#endif
}

void h_fast_timer_reset() {

  if (nw_fast_timer.set)
    reset_timeout(&nw_fast_timer);
}

int mk_slow_sweep() {

#ifdef PRODUCTION
  nw_lock();
  nc_slow_sweep();
  nw_unlock();
  set_timeout(&nw_slow_timer, 2*hz);
  return 0;
#endif
}

