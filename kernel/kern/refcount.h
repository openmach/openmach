/*
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
/*
 *	File:	refcount.h
 *
 *	This defines the system-independent part of the atomic reference count data type.
 *
 */

#ifndef	_KERN_REFCOUNT_H_
#define _KERN_REFCOUNT_H_

#include <kern/macro_help.h>

#include "refcount.h" /*XXX*/

/* Unless the above include file specified otherwise,
   use the system-independent (unoptimized) atomic reference counter.  */
#ifndef MACHINE_REFCOUNT

#include <kern/lock.h>

struct RefCount {
	decl_simple_lock_data(,lock)	/* lock for reference count */
	int		ref_count;	/* number of references */
};
typedef struct RefCount RefCount;

#define refcount_init(refcount, refs)			\
	MACRO_BEGIN					\
		simple_lock_init(&(refcount)->lock);	\
		((refcount)->ref_count = (refs));	\
	MACRO_END

#define refcount_take(refcount)				\
	MACRO_BEGIN					\
		simple_lock(&(refcount)->lock);		\
		(refcount)->ref_count++;		\
		simple_unlock(&(refcount)->lock);	\
	MACRO_END

#define refcount_drop(refcount, func)			\
	MACRO_BEGIN					\
		int new_value;				\
		simple_lock(&(refcount)->lock);		\
		new_value = --(refcount)->ref_count;	\
		simple_unlock(&(refcount)->lock);	\
		if (new_value == 0) { func; }		\
	MACRO_END

#endif

#endif _KERN_REFCOUNT_H_
