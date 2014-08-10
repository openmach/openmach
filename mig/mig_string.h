/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef	_MIG_STRING_H
#define	_MIG_STRING_H

#include <string.h>

#include "boolean.h"

typedef char *string_t;
typedef const char *const_string_t;
typedef const_string_t identifier_t;

#define	strNULL		((string_t) 0)

extern string_t strmake(const char *string);
extern string_t strconcat(const_string_t left, const_string_t right);
extern void strfree(string_t string);

#define	streql(a, b)	(strcmp((a), (b)) == 0)

extern const char *strbool(boolean_t bool);
extern const char *strstring(const_string_t string);

#endif	/* _MIG_STRING_H */
