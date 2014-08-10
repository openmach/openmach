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

#ifndef _NW_MK_H_
#define _NW_MK_H_ 1

#ifndef STUB
#include <chips/nw.h>
#include <kern/thread.h>
#else
#include "nw.h"
#include "stub.h"
#endif

/*** NETWORK INTERFACE -- WRAPPER FOR MACH KERNEL  ***/

/*** User-trappable functions ***/

extern nw_result mk_update(mach_port_t master_port, nw_update_type up_type,
			   int *up_info);

extern nw_result mk_lookup(nw_lookup_type lt, int *look_info);

extern nw_result mk_endpoint_allocate(nw_ep_t epp, nw_protocol protocol,
				      nw_acceptance accept, u_int buffer_size);

extern nw_result mk_endpoint_deallocate(nw_ep ep);

extern nw_buffer_t mk_buffer_allocate(nw_ep ep, u_int size);

extern nw_result mk_buffer_deallocate(nw_ep ep, nw_buffer_t buffer);

extern nw_result mk_connection_open(nw_ep local_ep, nw_address_1 rem_addr_1,
				    nw_address_2 rem_addr_2, nw_ep remote_ep);

extern nw_result mk_connection_accept(nw_ep ep, nw_buffer_t msg,
				      nw_ep_t new_epp);

extern nw_result mk_connection_close(nw_ep ep);

extern nw_result mk_multicast_add(nw_ep local_ep, nw_address_1 rem_addr_1,
				  nw_address_2 rem_addr_2, nw_ep remote_ep);

extern nw_result mk_multicast_drop(nw_ep local_ep, nw_address_1 rem_addr_1,
				   nw_address_2 rem_addr_2, nw_ep remote_ep);

extern nw_result mk_endpoint_status(nw_ep ep, nw_state_t state,
				    nw_peer_t peer);

extern nw_result mk_send(nw_ep ep, nw_buffer_t msg, nw_options options);

extern nw_buffer_t mk_receive(nw_ep ep, int time_out);

extern nw_buffer_t mk_rpc(nw_ep ep, nw_buffer_t send_msg,
			  nw_options options, int time_out);

extern nw_buffer_t mk_select(u_int nep, nw_ep_t epp, int time_out);


/*** System-dependent support ***/

extern void mk_endpoint_collect(task_t task);

extern void mk_waited_collect(thread_t thread);

extern void mk_return();

extern boolean_t mk_deliver_result(thread_t thread, int result);

extern int mk_fast_sweep();

extern int mk_slow_sweep();

#endif /* _NW_MK_H_ */
