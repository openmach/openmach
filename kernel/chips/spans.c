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

/*** SPANS SIGNALING  ***/

#ifndef STUB
#include <chips/spans.h>
#include <chips/tca100_if.h>
#else
#include "spans.h"
#include "tca100_if.h"
#endif

nw_result spans_initialize(int dev) {

#if !PERMANENT_VIRTUAL_CONNECTIONS
#endif

}


void spans_input(nw_buffer_t msg) {

#if !PERMANENT_VIRTUAL_CONNECTIONS
#endif

}

nw_result spans_open(nw_ep ep, nw_address_1 rem_addr_1,
		     nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;

#if PERMANENT_VIRTUAL_CONNECTIONS
  rc = NW_FAILURE;
#else
#endif

  return rc;
}

nw_result spans_accept(nw_ep ep, nw_buffer_t msg, nw_ep_t new_epp) {
  nw_result rc;

#if PERMANENT_VIRTUAL_CONNECTIONS
  rc = NW_FAILURE;
#else
#endif

  return rc;
}

nw_result spans_close(nw_ep ep) {
  nw_result rc;

  tct[ep].rx_sar_header = 0;
  rc = NW_SUCCESS;
  return rc;
}

nw_result spans_add(nw_ep ep, nw_address_1 rem_addr_1,
		    nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;

#if PERMANENT_VIRTUAL_CONNECTIONS
  rc = NW_FAILURE;
#else
#endif

  return rc;
}

nw_result spans_drop(nw_ep ep, nw_address_1 rem_addr_1,
		     nw_address_2 rem_addr_2, nw_ep remote_ep) {
  nw_result rc;

#if PERMANENT_VIRTUAL_CONNECTIONS
  rc = NW_FAILURE;
#else
#endif

  return rc;
}

void spans_timer_sweep() {

#if !PERMANENT_VIRTUAL_CONNECTIONS
#endif

}


