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

#ifndef _SPANS_H_
#define _SPANS_H_ 1

#ifndef STUB
#include <chips/nc.h>
#else
#include "nc.h"
#endif

extern nw_result spans_initialize(int dev);

extern void spans_input(nw_buffer_t msg);

extern nw_result spans_open(nw_ep ep, nw_address_1 rem_addr_1,
			    nw_address_2 rem_addr_2, nw_ep remote_ep);

extern nw_result spans_accept(nw_ep ep, nw_buffer_t msg, nw_ep_t new_epp);

extern nw_result spans_close(nw_ep ep);

extern nw_result spans_add(nw_ep ep, nw_address_1 rem_addr_1,
			   nw_address_2 rem_addr_2, nw_ep remote_ep);

extern nw_result spans_drop(nw_ep ep, nw_address_1 rem_addr_1,
			    nw_address_2 rem_addr_2, nw_ep remote_ep);

extern void spans_timer_sweep();


#endif /* _SPANS_H_ */
