/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989 Carnegie Mellon University
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
 * cthread_internals.h
 *
 *
 * Private definitions for the C Threads implementation.
 *
 * The cproc structure is used for different implementations
 * of the basic schedulable units that execute cthreads.
 *
 */


#include "options.h"
#include <mach/port.h>
#include <mach/message.h>
#include <mach/thread_switch.h>
#include <mach_error.h>

/*
 * Low-level thread implementation.
 * This structure must agree with struct ur_cthread in cthreads.h
 */
typedef struct cproc {
	struct cproc *next;		/* for lock, condition, and ready queues */
	cthread_t incarnation;		/* for cthread_self() */

	struct cproc *list;		/* for master cproc list */
	/* Not ifdeffed (WAIT_DEBUG) to keep size constant */
	volatile char *waiting_for;	/* address of mutex/cond waiting for */

	mach_port_t reply_port;		/* for mig_get_reply_port() */

	natural_t context;
	spin_lock_t lock;
	volatile int state;			/* current state */
#define CPROC_RUNNING	0
#define CPROC_SWITCHING 1
#define CPROC_BLOCKED	2
#define CPROC_CONDWAIT	4

	mach_port_t wired;		/* is cthread wired to kernel thread */
	vm_offset_t busy;		/* used with cthread_msg calls */

	mach_msg_header_t msg;

	vm_offset_t stack_base;
	vm_offset_t stack_size;

#if	defined(WAIT_DEBUG)
	enum wait_enum {
		CTW_NONE, CTW_MUTEX, CTW_CONDITION, CTW_PORT_ENTRY} wait_type;
#endif	/* defined(WAIT_DEBUG) */
} *cproc_t;

#define	NO_CPROC		((cproc_t) 0)
#define	cproc_self()		((cproc_t) ur_cthread_self())

/*
 * Macro for MACH kernel calls.
 */
#if	defined(CHECK_STATUS)
#define	MACH_CALL(expr, ret)	\
	if (((ret) = (expr)) != KERN_SUCCESS) { \
	quit(1, "error in %s at %d: %s\n", __FILE__, __LINE__, \
	     mach_error_string(ret)); \
	} else
#else	/* not defined(CHECK_STATUS) */
#define MACH_CALL(expr, ret) (ret) = (expr)
#endif	/* defined(CHECK_STATUS) */

#define private static
#if	defined(ASSERT)
#else	/* not defined(ASSERT) */
#define ASSERT(x)
#endif
#define TRACE(x)

/*
 * What we do to yield the processor:
 * (This depresses the thread's priority for up to 10ms.)
 */

#define yield()		\
	(void) thread_switch(MACH_PORT_NULL, SWITCH_OPTION_DEPRESS, 10)

/*
 * Functions implemented in malloc.c.
 */

#if	defined(DEBUG)
extern void	print_malloc_free_list(void);
#endif	/* defined(DEBUG) */

extern void		malloc_fork_prepare(void);

extern void		malloc_fork_parent(void);

extern void		malloc_fork_child(void);


/*
 * Functions implemented in stack.c.
 */

extern vm_offset_t	stack_init(cproc_t _cproc);

extern void		alloc_stack(cproc_t _cproc);

extern vm_offset_t	cproc_stack_base(cproc_t _cproc, int _offset);

extern void		stack_fork_child(void);

/*
 * Functions implemented in cprocs.c.
 */

extern vm_offset_t	cproc_init(void);

extern void		cproc_waiting(cproc_t _waiter);

extern cproc_t		cproc_create(void);

extern void		cproc_fork_prepare(void);

extern void		cproc_fork_parent(void);

extern void		cproc_fork_child(void);

/*
 * Function implemented in cthreads.c.
 */

extern void		cthread_body(cproc_t _self);

/*
 * Functions from machine dependent files.
 */

extern void		cproc_switch(natural_t *_cur, const natural_t *_new,
				     spin_lock_t *_lock);

extern void		cproc_start_wait(natural_t *_parent, cproc_t _child,
					 vm_offset_t _stackp,
					 spin_lock_t *_lock);

extern void		cproc_prepare(cproc_t _child, natural_t *_child_context,
				      vm_offset_t _stackp);

extern void		cproc_setup(cproc_t _child, thread_t _mach_thread,
				    void (*_routine)(cproc_t));

/*
 * Debugging support.
 */
#if	defined(DEBUG)

extern int stderr;

#if	defined(ASSERT)
#else	/* not defined(ASSERT) */
/*
 * Assertion macro, similar to <assert.h>
 */
#define	ASSERT(p) \
	MACRO_BEGIN \
	if (!(p)) { \
		fprintf(stderr, \
			"File %s, line %d: assertion "#p" failed.\n", \
			__FILE__, __LINE__); \
		abort(); \
	} \
	MACRO_END

#endif	/* defined(ASSERT) */

#define	SHOULDNT_HAPPEN	0

extern int cthread_debug;

#else	/* not defined(DEBUG) */

#if	defined(ASSERT)
#else	/* not defined(ASSERT) */
#define	ASSERT(p)
#endif	/* defined(ASSERT) */

#endif	/* defined(DEBUG) */

