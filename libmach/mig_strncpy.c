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
 * mig_strncpy.c - by Joshua Block
 *
 * mig_strncpy -- Bounded string copy.  Does what the library routine strncpy
 * OUGHT to do:  Copies the (null terminated) string in src into dest, a 
 * buffer of length len.  Assures that the copy is still null terminated
 * and doesn't overflow the buffer, truncating the copy if necessary.
 *
 * Parameters:
 * 
 *     dest - Pointer to destination buffer.
 * 
 *     src - Pointer to source string.
 * 
 *     len - Length of destination buffer.
 *
 * Result:
 *	length of string copied, INCLUDING the trailing 0.
 */

#include <mach/mig_support.h>

vm_size_t mig_strncpy(register char *dest, register const char *src,
		   register vm_size_t len)
{
    register vm_size_t i;

    if (len == 0)
	return 0;

    for (i=1; i<len; i++)
	if (! (*dest++ = *src++))
	    return i;

    *dest = '\0';
    return i;
}
