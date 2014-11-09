/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * Character subroutines
 */

#include <stdarg.h>

#define	EXPORT_BOOLEAN
#include <mach/boolean.h>

/*
 * Concatenate a group of strings together into a buffer.
 * Return a pointer to the trailing '\0' character in
 * the result string.
 * The list of strings ends with a '(char *)0'.
 */
/*VARARGS1*/
char *
strbuild(char *dest, ...)
{
	va_list	argptr;
	register char *	src;
	register int	c;

	va_start(argptr, dest);
	while ((src = va_arg(argptr, char *)) != (char *)0) {

	    while ((c = *src++) != '\0')
		*dest++ = c;
	}
	*dest = '\0';
	return (dest);
}

/*
 * Return TRUE if string 2 is a prefix of string 1.
 */
boolean_t
strprefix(s1, s2)
	register char *s1, *s2;
{
	register int	c;

	while ((c = *s2++) != '\0') {
	    if (c != *s1++)
		return (FALSE);
	}
	return (TRUE);
}

/* 
 * ovbcopy - like bcopy, but recognizes overlapping ranges and handles 
 *           them correctly.
 */
ovbcopy(from, to, bytes)
	char *from, *to;
	int bytes;			/* num bytes to copy */
{
	/* Assume that bcopy copies left-to-right (low addr first). */
	if (from + bytes <= to || to + bytes <= from || to == from)
		bcopy(from, to, bytes);	/* non-overlapping or no-op*/
	else if (from > to)
		bcopy(from, to, bytes);	/* overlapping but OK */
	else {
		/* to > from: overlapping, and must copy right-to-left. */
		from += bytes - 1;
		to += bytes - 1;
		while (bytes-- > 0)
			*to-- = *from--;
	}
}

/*
 * Return a pointer to the first occurence of 'c' in
 * string s, or 0 if none.
 */
char *
index(s, c)
	char *s;
	char  c;
{
	char cc;

	while ((cc = *s) != c) {
	    if (cc == 0)
		return 0;
	    s++;
	}
	return s;
}

