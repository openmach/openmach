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
#ifndef _MACH_SA_STDLIB_H_
#define _MACH_SA_STDLIB_H_

#include <mach/machine/vm_types.h>
#include <sys/cdefs.h>

#ifndef _SIZE_T
#define _SIZE_T
typedef natural_t size_t;
#endif

#ifndef NULL
#define NULL 0
#endif

__BEGIN_DECLS

int rand(void);

long atol(const char *str);
#define atoi(str) ((int)atol(str))

#define abs(n) __builtin_abs(n)

void exit(int status);

void srand(unsigned seed);
int rand(void);

void *malloc(size_t size);
void *calloc(size_t nelt, size_t eltsize);
void *realloc(void *buf, size_t new_size);
void free(void *buf);

__END_DECLS

#endif _MACH_SA_STDLIB_H_
