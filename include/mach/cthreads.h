/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990,1989 Carnegie Mellon University
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
 * 	File: 	cthreads.h
 *	Author: Eric Cooper, Carnegie Mellon University
 *	Date:	Jul, 1987
 *
 * 	Definitions for the C Threads package.
 *
 */


#ifndef	_CTHREADS_
#define	_CTHREADS_ 1

#include <mach/machine/cthreads.h>
#include <mach.h>
#include <mach/macro_help.h>
#include <mach/machine/vm_param.h>

#ifdef __STDC__
extern void *malloc();
#else
extern char *malloc();
#endif

typedef void *any_t;	    /* XXX - obsolete, should be deleted. */

#if	defined(TRUE)
#else	/* not defined(TRUE) */
#define	TRUE	1
#define	FALSE	0
#endif

/*
 * C Threads package initialization.
 */

extern vm_offset_t cthread_init(void);


/*
 * Queues.
 */
typedef struct cthread_queue {
	struct cthread_queue_item *head;
	struct cthread_queue_item *tail;
} *cthread_queue_t;

typedef struct cthread_queue_item {
	struct cthread_queue_item *next;
} *cthread_queue_item_t;

#define	NO_QUEUE_ITEM	((cthread_queue_item_t) 0)

#define	QUEUE_INITIALIZER	{ NO_QUEUE_ITEM, NO_QUEUE_ITEM }

#define	cthread_queue_alloc()	((cthread_queue_t) calloc(1, sizeof(struct cthread_queue)))
#define	cthread_queue_init(q)	((q)->head = (q)->tail = 0)
#define	cthread_queue_free(q)	free((q))

#define	cthread_queue_enq(q, x) \
	MACRO_BEGIN \
		(x)->next = 0; \
		if ((q)->tail == 0) \
			(q)->head = (cthread_queue_item_t) (x); \
		else \
			(q)->tail->next = (cthread_queue_item_t) (x); \
		(q)->tail = (cthread_queue_item_t) (x); \
	MACRO_END

#define	cthread_queue_preq(q, x) \
	MACRO_BEGIN \
		if ((q)->tail == 0) \
			(q)->tail = (cthread_queue_item_t) (x); \
		((cthread_queue_item_t) (x))->next = (q)->head; \
		(q)->head = (cthread_queue_item_t) (x); \
	MACRO_END

#define	cthread_queue_head(q, t)	((t) ((q)->head))

#define	cthread_queue_deq(q, t, x) \
	MACRO_BEGIN \
	if (((x) = (t) ((q)->head)) != 0 && \
	    ((q)->head = (cthread_queue_item_t) ((x)->next)) == 0) \
		(q)->tail = 0; \
	MACRO_END

#define	cthread_queue_map(q, t, f) \
	MACRO_BEGIN \
		register cthread_queue_item_t x, next; \
		for (x = (cthread_queue_item_t) ((q)->head); x != 0; x = next){\
			next = x->next; \
			(*(f))((t) x); \
		} \
	MACRO_END

/*
 * Spin locks.
 */
extern void		spin_lock_solid(spin_lock_t *_lock);

#if	defined(spin_unlock)
#else	/* not defined(spin_unlock) */
extern void		spin_unlock(spin_lock_t *_lock);
#endif

#if	defined(spin_try_lock)
#else	/* not defined(spin_try_lock) */
extern boolean_t	spin_try_lock(spin_lock_t *_lock);
#endif

#define spin_lock(p) \
	MACRO_BEGIN \
	if (!spin_try_lock(p)) { \
		spin_lock_solid(p); \
	} \
	MACRO_END

/*
 * Mutex objects.
 */
typedef struct mutex {
	spin_lock_t lock;
	const char *name;
	struct cthread_queue queue;
	spin_lock_t held;
	/* holder is for WAIT_DEBUG. Not ifdeffed to keep size constant. */
	struct cthread *holder;
} *mutex_t;

#define	MUTEX_INITIALIZER	{ SPIN_LOCK_INITIALIZER, 0, QUEUE_INITIALIZER, SPIN_LOCK_INITIALIZER}
#define	MUTEX_NAMED_INITIALIZER(Name) { SPIN_LOCK_INITIALIZER, Name, QUEUE_INITIALIZER, SPIN_LOCK_INITIALIZER}

#ifdef WAIT_DEBUG
#define mutex_set_holder(m,h)	((m)->holder = (h))
#else
#define mutex_set_holder(m,h)	(0)
#endif

#define	mutex_alloc()		((mutex_t) calloc(1, sizeof(struct mutex)))
#define	mutex_init(m) \
	MACRO_BEGIN \
	spin_lock_init(&(m)->lock); \
	cthread_queue_init(&(m)->queue); \
	spin_lock_init(&(m)->held); \
	mutex_set_holder(m, 0); \
	MACRO_END
#define	mutex_set_name(m, x)	((m)->name = (x))
#define	mutex_name(m)		((m)->name != 0 ? (m)->name : "?")
#define	mutex_clear(m)		/* nop */???
#define	mutex_free(m)		free((m))

extern void	mutex_lock_solid(mutex_t _mutex);	/* blocking */

extern void	mutex_unlock_solid(mutex_t _mutex);

#define mutex_try_lock(m) \
	(spin_try_lock(&(m)->held) ? mutex_set_holder((m), cthread_self()), TRUE : FALSE)
#define mutex_lock(m) \
	MACRO_BEGIN \
	if (!spin_try_lock(&(m)->held)) { \
		mutex_lock_solid(m); \
	} \
	mutex_set_holder(m, cthread_self()); \
	MACRO_END
#define mutex_unlock(m) \
	MACRO_BEGIN \
	mutex_set_holder(m, 0); \
	if (spin_unlock(&(m)->held), \
	    cthread_queue_head(&(m)->queue, vm_offset_t) != 0) { \
		mutex_unlock_solid(m); \
	} \
	MACRO_END

/*
 * Condition variables.
 */
typedef struct condition {
	spin_lock_t lock;
	struct cthread_queue queue;
	const char *name;
} *condition_t;

#define	CONDITION_INITIALIZER		{ SPIN_LOCK_INITIALIZER, QUEUE_INITIALIZER, 0 }
#define	CONDITION_NAMED_INITIALIZER(Name) { SPIN_LOCK_INITIALIZER, QUEUE_INITIALIZER, Name }

#define	condition_alloc() \
	((condition_t) calloc(1, sizeof(struct condition)))
#define	condition_init(c) \
	MACRO_BEGIN \
	spin_lock_init(&(c)->lock); \
	cthread_queue_init(&(c)->queue); \
	MACRO_END
#define	condition_set_name(c, x)	((c)->name = (x))
#define	condition_name(c)		((c)->name != 0 ? (c)->name : "?")
#define	condition_clear(c) \
	MACRO_BEGIN \
	condition_broadcast(c); \
	spin_lock(&(c)->lock); \
	MACRO_END
#define	condition_free(c) \
	MACRO_BEGIN \
	condition_clear(c); \
	free((c)); \
	MACRO_END

#define	condition_signal(c) \
	MACRO_BEGIN \
	if ((c)->queue.head) { \
		cond_signal(c); \
	} \
	MACRO_END

#define	condition_broadcast(c) \
	MACRO_BEGIN \
	if ((c)->queue.head) { \
		cond_broadcast(c); \
	} \
	MACRO_END

extern void	cond_signal(condition_t _cond);

extern void	cond_broadcast(condition_t _cond);

extern void	condition_wait(condition_t _cond, mutex_t _mutex);

/*
 * Threads.
 */

typedef void *	(*cthread_fn_t)(void *arg);

/* XXX We really should be using the setjmp.h that goes with the libc
 * that we're planning on using, since that's where the setjmp()
 * functions are going to be comming from.
 */
#include <mach/setjmp.h>

typedef struct cthread {
	struct cthread *next;
	struct mutex lock;
	struct condition done;
	int state;
	jmp_buf catch_exit;
	cthread_fn_t func;
	void *arg;
	void *result;
	const char *name;
	void *data;
	void *ldata;
	void *private_data;
	struct ur_cthread *ur;
} *cthread_t;

#define	NO_CTHREAD	((cthread_t) 0)

extern cthread_t	cthread_fork(cthread_fn_t _func, void *_arg);

extern void		cthread_detach(cthread_t _thread);

extern any_t		cthread_join(cthread_t _thread);

extern void		cthread_yield(void);

extern void		cthread_exit(void *_result);

/*
 * This structure must agree with struct cproc in cthread_internals.h
 */
typedef struct ur_cthread {
	struct ur_cthread *next;
	cthread_t incarnation;
} *ur_cthread_t;

#ifndef	cthread_sp
extern vm_offset_t
cthread_sp(void);
#endif

extern vm_offset_t cthread_stack_mask;

#if	defined(STACK_GROWTH_UP)
#define	ur_cthread_ptr(sp) \
	(* (ur_cthread_t *) ((sp) & cthread_stack_mask))
#else	/* not defined(STACK_GROWTH_UP) */
#define	ur_cthread_ptr(sp) \
	(* (ur_cthread_t *) ( ((sp) | cthread_stack_mask) + 1 \
			      - sizeof(ur_cthread_t *)) )
#endif	/* defined(STACK_GROWTH_UP) */

#define	ur_cthread_self()	(ur_cthread_ptr(cthread_sp()))

#define	cthread_assoc(id, t)	((((ur_cthread_t) (id))->incarnation = (t)), \
				((t) ? ((t)->ur = (ur_cthread_t)(id)) : 0))
#define	cthread_self()		(ur_cthread_self()->incarnation)

extern void		cthread_set_name(cthread_t _thread, const char *_name);

extern const char *	cthread_name(cthread_t _thread);

extern int		cthread_count(void);

extern void		cthread_set_limit(int _limit);

extern int		cthread_limit(void);

extern void		cthread_set_kernel_limit(int _n);

extern int		cthread_kernel_limit(void);

extern void		cthread_wire(void);

extern void		cthread_unwire(void);

extern void		cthread_msg_busy(mach_port_t _port, int _min, int _max);

extern void		cthread_msg_active(mach_port_t _prt, int _min, int _max);

extern mach_msg_return_t cthread_mach_msg(mach_msg_header_t *_header,
					  mach_msg_option_t _option,
					  mach_msg_size_t _send_size,
					  mach_msg_size_t _rcv_size,
					  mach_port_t _rcv_name,
					  mach_msg_timeout_t _timeout,
					  mach_port_t _notify,
					  int _min, int _max);

extern void		cthread_fork_prepare(void);

extern void		cthread_fork_parent(void);

extern void		cthread_fork_child(void);

#if	defined(THREAD_CALLS)
/*
 * Routines to replace thread_*.
 */
extern kern_return_t	cthread_get_state(cthread_t _thread);

extern kern_return_t	cthread_set_state(cthread_t _thread);

extern kern_return_t	cthread_abort(cthread_t _thread);

extern kern_return_t	cthread_resume(cthread_t _thread);

extern kern_return_t	cthread_suspend(cthread_t _thread);

extern kern_return_t	cthread_call_on(cthread_t _thread);
#endif	/* defined(THREAD_CALLS) */

#if	defined(CTHREAD_DATA_XX)
/*
 * Set or get thread specific "global" variable
 *
 * The thread given must be the calling thread (ie. thread_self).
 * XXX This is for compatibility with the old cthread_data. XXX
 */
extern int		cthread_set_data(cthread_t _thread, void *_val);

extern void *		cthread_data(cthread_t _thread);
#else	/* defined(CTHREAD_DATA_XX) */

#define cthread_set_data(_thread, _val) ((_thread)->data) = (void *)(_val);
#define cthread_data(_thread) ((_thread)->data)

#define cthread_set_ldata(_thread, _val) ((_thread)->ldata) = (void *)(_val);
#define cthread_ldata(_thread) ((_thread)->ldata)

#endif	/* defined(CTHREAD_DATA_XX) */


/* 
 * Support for POSIX thread specific data
 *
 * Multiplexes a thread specific "global" variable
 * into many thread specific "global" variables.
 */
#define CTHREAD_DATA_VALUE_NULL		(void *)0
#define	CTHREAD_KEY_INVALID		(cthread_key_t)-1

typedef int	cthread_key_t;

/*
 * Create key to private data visible to all threads in task.
 * Different threads may use same key, but the values bound to the key are
 * maintained on a thread specific basis.
 */
extern int		cthread_keycreate(cthread_key_t *_key);

/*
 * Get value currently bound to key for calling thread
 */
extern int		cthread_getspecific(cthread_key_t _key, void **_value);

/*
 * Bind value to given key for calling thread
 */
extern int		cthread_setspecific(cthread_key_t _key, void *_value);

#endif	/* not defined(_CTHREADS_) */
