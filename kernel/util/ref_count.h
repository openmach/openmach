/* 
 * Copyright (c) 1995-1993 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
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
 *	This defines the system-independent atomic reference count data type.
 *	(This file will often be overridden
 *	by better machine-dependent implementations.)
 *
 */

#ifndef	_KERN_REF_COUNT_H_
#define _KERN_REF_COUNT_H_

#include <mach/macro_help.h>

#include "simple_lock.h"


struct ref_count {
	decl_simple_lock_data(,lock)	/* lock for reference count */
	int		count;		/* number of references */
};

#define ref_count_init(refcount, refs)			\
	MACRO_BEGIN					\
		simple_lock_init(&(refcount)->lock);	\
		((refcount)->count = (refs));		\
	MACRO_END

#define ref_count_take(refcount)			\
	MACRO_BEGIN					\
		simple_lock(&(refcount)->lock);		\
		(refcount)->count++;			\
		simple_unlock(&(refcount)->lock);	\
	MACRO_END

#define ref_count_drop(refcount, func)			\
	MACRO_BEGIN					\
		int new_value;				\
		simple_lock(&(refcount)->lock);		\
		new_value = --(refcount)->count;	\
		simple_unlock(&(refcount)->lock);	\
		if (new_value == 0) { func; }		\
	MACRO_END


#endif _KERN_REF_COUNT_H_
