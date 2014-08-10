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

#ifndef	_KERN_COUNTERS_
#define	_KERN_COUNTERS_

#include <mach_counters.h>

/*
 *	We can count various interesting events and paths.
 *
 *	Use counter() to change the counters, eg:
 *		counter(c_idle_thread_block++);
 *	Use counter_always() for non-conditional counters.
 */

#define counter_always(code)	code

#if	MACH_COUNTERS

#define counter(code)		counter_always(code)

#else	MACH_COUNTERS

#define counter(code)

#endif	MACH_COUNTERS

/*
 *	We define the counters with individual integers,
 *	instead of a big structure, so that ddb
 *	will know the addresses of the counters.
 */

typedef unsigned int mach_counter_t;

extern mach_counter_t c_thread_invoke_hits;
extern mach_counter_t c_thread_invoke_misses;
extern mach_counter_t c_thread_invoke_csw;
extern mach_counter_t c_thread_handoff_hits;
extern mach_counter_t c_thread_handoff_misses;

#if	MACH_COUNTERS
extern mach_counter_t c_threads_current;
extern mach_counter_t c_threads_max;
extern mach_counter_t c_threads_min;
extern mach_counter_t c_threads_total;
extern mach_counter_t c_stacks_current;
extern mach_counter_t c_stacks_max;
extern mach_counter_t c_stacks_min;
extern mach_counter_t c_stacks_total;
extern mach_counter_t c_clock_ticks;
extern mach_counter_t c_ipc_mqueue_send_block;
extern mach_counter_t c_ipc_mqueue_receive_block_user;
extern mach_counter_t c_ipc_mqueue_receive_block_kernel;
extern mach_counter_t c_mach_msg_trap_block_fast;
extern mach_counter_t c_mach_msg_trap_block_slow;
extern mach_counter_t c_mach_msg_trap_block_exc;
extern mach_counter_t c_exception_raise_block;
extern mach_counter_t c_swtch_block;
extern mach_counter_t c_swtch_pri_block;
extern mach_counter_t c_thread_switch_block;
extern mach_counter_t c_thread_switch_handoff;
extern mach_counter_t c_ast_taken_block;
extern mach_counter_t c_thread_halt_self_block;
extern mach_counter_t c_vm_fault_page_block_busy_user;
extern mach_counter_t c_vm_fault_page_block_busy_kernel;
extern mach_counter_t c_vm_fault_page_block_backoff_user;
extern mach_counter_t c_vm_fault_page_block_backoff_kernel;
extern mach_counter_t c_vm_page_wait_block_user;
extern mach_counter_t c_vm_page_wait_block_kernel;
extern mach_counter_t c_vm_pageout_block;
extern mach_counter_t c_vm_pageout_scan_block;
extern mach_counter_t c_idle_thread_block;
extern mach_counter_t c_idle_thread_handoff;
extern mach_counter_t c_sched_thread_block;
extern mach_counter_t c_io_done_thread_block;
extern mach_counter_t c_net_thread_block;
extern mach_counter_t c_reaper_thread_block;
extern mach_counter_t c_swapin_thread_block;
extern mach_counter_t c_action_thread_block;
#endif	MACH_COUNTERS

#endif	_KERN_COUNTERS_
