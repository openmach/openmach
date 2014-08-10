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
#ifndef _MACH_CTYPE_H_
#define _MACH_CTYPE_H_

#include <sys/cdefs.h>

__INLINE_FUNC int isdigit(char c)
{
	return ((c) >= '0') && ((c) <= '9');
}

__INLINE_FUNC int isspace(char c)
{
	return ((c) == ' ') || ((c) == '\f')
		|| ((c) == '\n') || ((c) == '\r')
		|| ((c) == '\t') || ((c) == '\v');
}

__INLINE_FUNC int isalpha(char c)
{
	return (((c) >= 'a') && ((c) <= 'z'))
		|| (((c) >= 'A') && ((c) <= 'Z'));
}

__INLINE_FUNC int isalnum(char c)
{
	return isalpha(c) || isdigit(c);
}

__INLINE_FUNC int toupper(char c)
{
	return ((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c;
}

__INLINE_FUNC int tolower(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c;
}


#endif _MACH_CTYPE_H_
