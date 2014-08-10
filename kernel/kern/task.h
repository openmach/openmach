/* 
 * Mach Operating System
 * Copyright (c) 1993-1988 Carnegie Mellon University
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
/*
 *	File:	task.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for tasks.
 *
 */

#ifndef	_KERN_TASK_H_
#define _KERN_TASK_H_

#include <norma_task.h>
#include <fast_tas.h>
#include <net_atm.h>

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/time_value.h>
#include <mach/mach_param.h>
#include <mach/task_info.h>
#include <kern/kern_types.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/pc_sample.h>
#include <kern/processor.h>
#include <kern/syscall_emulation.h>
#include <vm/vm_map.h>

#if	NET_ATM
typedef struct nw_ep_owned {
  unsigned int ep;
  struct nw_ep_owned *next;
} nw_ep_owned_s, *nw_ep_owned_t;
#endif

struct task {
	/* Synchronization/destruction information */
	decl_simple_lock_data(,lock)	/* Task's lock */
	int		ref_count;	/* Number of references to me */
	boolean_t	active;		/* Task has not been terminated */

	/* Miscellaneous */
	vm_map_t	map;		/* Address space description */
	queue_chain_t	pset_tasks;	/* list of tasks assigned to pset */
	int		suspend_count;	/* Internal scheduling only */

	/* Thread information */
	queue_head_t	thread_list;	/* list of threads */
	int		thread_count;	/* number of threads */
	processor_set_t	processor_set;	/* processor set for new threads */
	boolean_t	may_assign;	/* can assigned pset be changed? */
	boolean_t	assign_active;	/* waiting for may_assign */

	/* User-visible scheduling information */
	int		user_stop_count;	/* outstanding stops */
	int		priority;		/* for new threads */

	/* Statistics */
	time_value_t	total_user_time;
				/* total user time for dead threads */
	time_value_t	total_system_time;
				/* total system time for dead threads */

	/* IPC structures */
	decl_simple_lock_data(, itk_lock_data)
	struct ipc_port *itk_self;	/* not a right, doesn't hold ref */
	struct ipc_port *itk_sself;	/* a send right */
	struct ipc_port *itk_exception;	/* a send right */
	struct ipc_port *itk_bootstrap;	/* a send right */
	struct ipc_port *itk_registered[TASK_PORT_REGISTER_MAX];
					/* all send rights */

	struct ipc_space *itk_space;

	/* User space system call emulation support */
	struct 	eml_dispatch	*eml_dispatch;

	sample_control_t pc_sample;

#if	NORMA_TASK
	long		child_node;	/* if != -1, node for new children */
#endif	/* NORMA_TASK */

#if	FAST_TAS
#define TASK_FAST_TAS_NRAS	8	
	vm_offset_t	fast_tas_base[TASK_FAST_TAS_NRAS];
	vm_offset_t	fast_tas_end[TASK_FAST_TAS_NRAS];
#endif	/* FAST_TAS */

#if	NET_ATM
	nw_ep_owned_t   nw_ep_owned;
#endif	/* NET_ATM */
};

#define task_lock(task)		simple_lock(&(task)->lock)
#define task_unlock(task)	simple_unlock(&(task)->lock)

#define	itk_lock_init(task)	simple_lock_init(&(task)->itk_lock_data)
#define	itk_lock(task)		simple_lock(&(task)->itk_lock_data)
#define	itk_unlock(task)	simple_unlock(&(task)->itk_lock_data)

/*
 *	Exported routines/macros
 */

extern kern_return_t	task_create(
	task_t		parent_task,
	boolean_t	inherit_memory,
	task_t		*child_task);
extern kern_return_t	task_terminate(
	task_t		task);
extern kern_return_t	task_suspend(
	task_t		task);
extern kern_return_t	task_resume(
	task_t		task);
extern kern_return_t	task_threads(
	task_t		task,
	thread_array_t	*thread_list,
	natural_t	*count);
extern kern_return_t	task_info(
	task_t		task,
	int		flavor,
	task_info_t	task_info_out,
	natural_t	*task_info_count);
extern kern_return_t	task_get_special_port(
	task_t		task,
	int		which,
	struct ipc_port	**portp);
extern kern_return_t	task_set_special_port(
	task_t		task,
	int		which,
	struct ipc_port	*port);
extern kern_return_t	task_assign(
	task_t		task,
	processor_set_t	new_pset,
	boolean_t	assign_threads);
extern kern_return_t	task_assign_default(
	task_t		task,
	boolean_t	assign_threads);

/*
 *	Internal only routines
 */

extern void		task_init();
extern void		task_reference();
extern void		task_deallocate();
extern kern_return_t	task_hold();
extern kern_return_t	task_dowait();
extern kern_return_t	task_release();
extern kern_return_t	task_halt();

extern kern_return_t	task_suspend_nowait();
extern task_t		kernel_task_create();

extern task_t	kernel_task;

#endif	_KERN_TASK_H_
