/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *	File:	ipc/ipc_kmsg.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Definitions for kernel messages.
 */

#ifndef	_IPC_IPC_KMSG_H_
#define _IPC_IPC_KMSG_H_

#include <cpus.h>
#include <mach_ipc_compat.h>
#include <norma_ipc.h>

#include <mach/machine/vm_types.h>
#include <mach/message.h>
#include <kern/assert.h>
#include "cpu_number.h"
#include <kern/macro_help.h>
#include <kern/kalloc.h>
#include <ipc/ipc_marequest.h>
#if	NORMA_IPC
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#endif	NORMA_IPC

/*
 *	This structure is only the header for a kmsg buffer;
 *	the actual buffer is normally larger.  The rest of the buffer
 *	holds the body of the message.
 *
 *	In a kmsg, the port fields hold pointers to ports instead
 *	of port names.  These pointers hold references.
 *
 *	The ikm_header.msgh_remote_port field is the destination
 *	of the message.
 */

typedef struct ipc_kmsg {
	struct ipc_kmsg *ikm_next, *ikm_prev;
	vm_size_t ikm_size;
	ipc_marequest_t ikm_marequest;
#if	NORMA_IPC
	vm_page_t ikm_page;
	vm_map_copy_t ikm_copy;
	unsigned long ikm_source_node;
#endif	NORMA_IPC
	mach_msg_header_t ikm_header;
} *ipc_kmsg_t;

#define	IKM_NULL		((ipc_kmsg_t) 0)

#define	IKM_OVERHEAD							\
		(sizeof(struct ipc_kmsg) - sizeof(mach_msg_header_t))

#define	ikm_plus_overhead(size)	((vm_size_t)((size) + IKM_OVERHEAD))
#define	ikm_less_overhead(size)	((mach_msg_size_t)((size) - IKM_OVERHEAD))

/*
 * XXX	For debugging.
 */
#define IKM_BOGUS		((ipc_kmsg_t) 0xffffff10)

/*
 *	We keep a per-processor cache of kernel message buffers.
 *	The cache saves the overhead/locking of using kalloc/kfree.
 *	The per-processor cache seems to miss less than a per-thread cache,
 *	and it also uses less memory.  Access to the cache doesn't
 *	require locking.
 */

extern ipc_kmsg_t	ipc_kmsg_cache[NCPUS];

#define ikm_cache()     ipc_kmsg_cache[cpu_number()]

/*
 *	The size of the kernel message buffers that will be cached.
 *	IKM_SAVED_KMSG_SIZE includes overhead; IKM_SAVED_MSG_SIZE doesn't.
 */

#define	IKM_SAVED_KMSG_SIZE	((vm_size_t) 256)
#define	IKM_SAVED_MSG_SIZE	ikm_less_overhead(IKM_SAVED_KMSG_SIZE)

#define	ikm_alloc(size)							\
		((ipc_kmsg_t) kalloc(ikm_plus_overhead(size)))

#define	ikm_init(kmsg, size)						\
MACRO_BEGIN								\
	ikm_init_special((kmsg), ikm_plus_overhead(size));		\
MACRO_END

#define	ikm_init_special(kmsg, size)					\
MACRO_BEGIN								\
	(kmsg)->ikm_size = (size);					\
	(kmsg)->ikm_marequest = IMAR_NULL;				\
MACRO_END

#define	ikm_check_initialized(kmsg, size)				\
MACRO_BEGIN								\
	assert((kmsg)->ikm_size == (size));				\
	assert((kmsg)->ikm_marequest == IMAR_NULL);			\
MACRO_END

/*
 *	Non-positive message sizes are special.  They indicate that
 *	the message buffer doesn't come from ikm_alloc and
 *	requires some special handling to free.
 *
 *	ipc_kmsg_free is the non-macro form of ikm_free.
 *	It frees kmsgs of all varieties.
 */

#define	IKM_SIZE_NORMA		0
#define	IKM_SIZE_NETWORK	-1

#define	ikm_free(kmsg)							\
MACRO_BEGIN								\
	register vm_size_t _size = (kmsg)->ikm_size;			\
									\
	if ((integer_t)_size > 0)					\
		kfree((vm_offset_t) (kmsg), _size);			\
	else								\
		ipc_kmsg_free(kmsg);					\
MACRO_END

/*
 *	struct ipc_kmsg_queue is defined in kern/thread.h instead of here,
 *	so that kern/thread.h doesn't have to include ipc/ipc_kmsg.h.
 */

#include <ipc/ipc_kmsg_queue.h>

typedef struct ipc_kmsg_queue *ipc_kmsg_queue_t;

#define	IKMQ_NULL		((ipc_kmsg_queue_t) 0)


#define	ipc_kmsg_queue_init(queue)		\
MACRO_BEGIN					\
	(queue)->ikmq_base = IKM_NULL;		\
MACRO_END

#define	ipc_kmsg_queue_empty(queue)	((queue)->ikmq_base == IKM_NULL)

/* Enqueue a kmsg */
extern void ipc_kmsg_enqueue(
	ipc_kmsg_queue_t	queue,
	ipc_kmsg_t		kmsg);

/* Dequeue and return a kmsg */
extern ipc_kmsg_t ipc_kmsg_dequeue(
	ipc_kmsg_queue_t        queue);

/* Pull a kmsg out of a queue */
extern void ipc_kmsg_rmqueue(
	ipc_kmsg_queue_t	queue,
	ipc_kmsg_t		kmsg);

#define	ipc_kmsg_queue_first(queue)		((queue)->ikmq_base)

/* Return the kmsg following the given kmsg */
extern ipc_kmsg_t ipc_kmsg_queue_next(
	ipc_kmsg_queue_t	queue,
	ipc_kmsg_t		kmsg);

#define	ipc_kmsg_rmqueue_first_macro(queue, kmsg)			\
MACRO_BEGIN								\
	register ipc_kmsg_t _next;					\
									\
	assert((queue)->ikmq_base == (kmsg));				\
									\
	_next = (kmsg)->ikm_next;					\
	if (_next == (kmsg)) {						\
		assert((kmsg)->ikm_prev == (kmsg));			\
		(queue)->ikmq_base = IKM_NULL;				\
	} else {							\
		register ipc_kmsg_t _prev = (kmsg)->ikm_prev;		\
									\
		(queue)->ikmq_base = _next;				\
		_next->ikm_prev = _prev;				\
		_prev->ikm_next = _next;				\
	}								\
  	/* XXX Debug paranoia */					\
  	kmsg->ikm_next = IKM_BOGUS;					\
  	kmsg->ikm_prev = IKM_BOGUS;					\
MACRO_END

#define	ipc_kmsg_enqueue_macro(queue, kmsg)				\
MACRO_BEGIN								\
	register ipc_kmsg_t _first = (queue)->ikmq_base;		\
									\
	if (_first == IKM_NULL) {					\
		(queue)->ikmq_base = (kmsg);				\
		(kmsg)->ikm_next = (kmsg);				\
		(kmsg)->ikm_prev = (kmsg);				\
	} else {							\
		register ipc_kmsg_t _last = _first->ikm_prev;		\
									\
		(kmsg)->ikm_next = _first;				\
		(kmsg)->ikm_prev = _last;				\
		_first->ikm_prev = (kmsg);				\
		_last->ikm_next = (kmsg);				\
	}								\
MACRO_END

extern void
ipc_kmsg_destroy(/* ipc_kmsg_t */);

extern void
ipc_kmsg_clean(/* ipc_kmsg_t */);

extern void
ipc_kmsg_free(/* ipc_kmsg_t */);

extern mach_msg_return_t
ipc_kmsg_get(/* mach_msg_header_t *, mach_msg_size_t, ipc_kmsg_t * */);

extern mach_msg_return_t
ipc_kmsg_get_from_kernel(/* mach_msg_header_t *, mach_msg_size_t,
			    ipc_kmsg_t * */);

extern mach_msg_return_t
ipc_kmsg_put(/* mach_msg_header_t *, ipc_kmsg_t, mach_msg_size_t */);

extern void
ipc_kmsg_put_to_kernel(/* mach_msg_header_t *, ipc_kmsg_t, mach_msg_size_t */);

extern mach_msg_return_t
ipc_kmsg_copyin_header(/* mach_msg_header_t *, ipc_space_t, mach_port_t */);

extern mach_msg_return_t
ipc_kmsg_copyin(/* ipc_kmsg_t, ipc_space_t, vm_map_t, mach_port_t */);

extern void
ipc_kmsg_copyin_from_kernel(/* ipc_kmsg_t */);

extern mach_msg_return_t
ipc_kmsg_copyout_header(/* mach_msg_header_t *, ipc_space_t, mach_port_t */);

extern mach_msg_return_t
ipc_kmsg_copyout_object(/* ipc_space_t, ipc_object_t,
			   mach_msg_type_name_t, mach_port_t * */);

extern mach_msg_return_t
ipc_kmsg_copyout_body(/* vm_offset_t, vm_offset_t, ipc_space_t, vm_map_t */);

extern mach_msg_return_t
ipc_kmsg_copyout(/* ipc_kmsg_t, ipc_space_t, vm_map_t, mach_port_t */);

extern mach_msg_return_t
ipc_kmsg_copyout_pseudo(/* ipc_kmsg_t, ipc_space_t, vm_map_t */);

extern void
ipc_kmsg_copyout_dest(/* ipc_kmsg_t, ipc_space_t */);

#if	MACH_IPC_COMPAT

extern mach_msg_return_t
ipc_kmsg_copyin_compat(/* ipc_kmsg_t, ipc_space_t, vm_map_t */);

extern mach_msg_return_t
ipc_kmsg_copyout_compat(/* ipc_kmsg_t, ipc_space_t, vm_map_t */);

#endif	MACH_IPC_COMPAT
#endif	_IPC_IPC_KMSG_H_
