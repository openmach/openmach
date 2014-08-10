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

#include <sys/types.h>
#include <stdlib.h>

#include "boolean.h"
#include "error.h"
#include "mig_string.h"

string_t
strmake(const char *string)
{
    register string_t saved;

    saved = malloc(strlen(string) + 1);
    if (saved == strNULL)
	fatal("strmake('%s'): %s", string, unix_error_string(errno));
    return strcpy(saved, string);
}

string_t
strconcat(const_string_t left, const_string_t right)
{
    register string_t saved;

    saved = malloc(strlen(left) + strlen(right) + 1);
    if (saved == strNULL)
	fatal("strconcat('%s', '%s'): %s",
	      left, right, unix_error_string(errno));
    return strcat(strcpy(saved, left), right);
}

void
strfree(string_t string)
{
    free(string);
}

const char *
strbool(boolean_t bool)
{
    if (bool)
	return "TRUE";
    else
	return "FALSE";
}

const char *
strstring(const_string_t string)
{
    if (string == strNULL)
	return "NULL";
    else
	return string;
}
