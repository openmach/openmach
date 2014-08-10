/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
 *	File: strings.c
 * 	Author: Robert V. Baron, Carnegie Mellon University
 *	Date:	??/92
 *
 *	String functions.
 */

#include <kern/strings.h>	/* make sure we sell the truth */

#ifdef	strcpy
#undef strcmp
#undef strncmp
#undef strcpy
#undef strncpy
#undef strlen
#endif

/*
 * Abstract:
 *	strcmp (s1, s2) compares the strings "s1" and "s2".
 *	It returns 0 if the strings are identical. It returns
 *	> 0 if the first character that differs in the two strings
 *	is larger in s1 than in s2 or if s1 is longer than s2 and 
 *	the contents are identical up to the length of s2.
 *	It returns < 0 if the first differing character is smaller 
 *	in s1 than in s2 or if s1 is shorter than s2 and the
 *	contents are identical upto the length of s1.
 */

int
strcmp(
	register const char *s1,
	register const char *s2)
{
	register unsigned int a, b;

	do {
		a = *s1++;
		b = *s2++;
		if (a != b)
			return a-b;	/* includes case when
					   'a' is zero and 'b' is not zero
					   or vice versa */
	} while (a != '\0');

	return 0;	/* both are zero */
}


/*
 * Abstract:
 *	strncmp (s1, s2, n) compares the strings "s1" and "s2"
 *	in exactly the same way as strcmp does.  Except the
 *	comparison runs for at most "n" characters.
 */

int
strncmp(
	register const char *s1,
	register const char *s2,
	unsigned long n)
{
	register unsigned int a, b;

	while (n != 0) {
		a = *s1++;
		b = *s2++;
		if (a != b)
			return a-b;	/* includes case when
					   'a' is zero and 'b' is not zero
					   or vice versa */
		if (a == '\0')
			return 0;	/* both are zero */
		n--;
	}

	return 0;
}


/*
 * Abstract:
 *	strcpy copies the contents of the string "from" including 
 *	the null terminator to the string "to". A pointer to "to"
 *	is returned.
 */

char *
strcpy(
	register char *to,
	register const char *from)
{
	register char *ret = to;

	while ((*to++ = *from++) != '\0')
		continue;

	return ret;
}

/*
 * Abstract:
 *	strncpy copies "count" characters from the "from" string to
 *	the "to" string. If "from" contains less than "count" characters
 *	"to" will be padded with null characters until exactly "count"
 *	characters have been written. The return value is a pointer
 *	to the "to" string.
 */

char *
strncpy(
	register char *to,
	register const char *from,
	register unsigned long count)
{
	register char *ret = to;

	while (count != 0) {
		count--;
		if ((*to++ = *from++) == '\0')
			break;
	}

	while (count != 0) {
		*to++ = '\0';
		count--;
	}

	return ret;
}

/*
 * Abstract:
 *	strlen returns the number of characters in "string" preceeding 
 *	the terminating null character.
 */

unsigned long
strlen(
	register const char *string)
{
	register const char *ret = string;

	while (*string++ != '\0')
		continue;

	return string - 1 - ret;
}
