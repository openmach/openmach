/*
 * Copyright (c) 1994 The University of Utah and
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
#ifndef _MACH_SA_MALLOC_H_
#define _MACH_SA_MALLOC_H_

#include <mach/machine/vm_types.h>
#include <sys/cdefs.h>

#ifndef _SIZE_T
#define _SIZE_T
typedef natural_t size_t;
#endif

/* The malloc package in the base C library
   is implemented on top of the List Memory Manager,
   and the underlying memory pool can be manipulated
   directly with the LMM primitives using this lmm structure.  */
extern struct lmm malloc_lmm;

__BEGIN_DECLS

void *malloc(size_t size);
void *calloc(size_t nelt, size_t eltsize);
void *realloc(void *buf, size_t new_size);
void free(void *buf);

/* malloc() and realloc() call this routine when they're about to fail;
   it should try to scare up more memory and add it to the malloc_lmm.
   Returns nonzero if it succeeds in finding more memory.  */
int morecore(size_t size);

__END_DECLS

#endif _MACH_SA_MALLOC_H_
