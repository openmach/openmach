/*
 * Copyright (c) 1990,1992,1993 Carnegie Mellon University
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
 *  Software Distribution Coordinator  or  Software_Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef HAVE_VPRINTF

/*
 * ansi varargs versions of printf routines
 * This are directly included to deal with nonansi libc's.
 */
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>

/*
 * Forward declaration.
 */
static void _doprnt_ansi(const char *_fmt, va_list _args, FILE *_stream);

int
vprintf(const char *fmt, va_list args)
{
	_doprnt_ansi(fmt, args, stdout);
	return (ferror(stdout) ? EOF : 0);
}

int
vfprintf(FILE *f, const char *fmt, va_list args)
{
	_doprnt_ansi(fmt, args, f);
	return (ferror(f) ? EOF : 0);
}

int
vsprintf(char *s, const char *fmt, va_list args)
{
	FILE fakebuf;

	fakebuf._flag = _IOSTRG;	/* no _IOWRT: avoid stdio bug */
	fakebuf._ptr = s;
	fakebuf._cnt = 32767;
	_doprnt_ansi(fmt, args, &fakebuf);
	putc('\0', &fakebuf);
	return (strlen(s));
}

int
vsnprintf(char *s, int n, const char *fmt, va_list args)
{
	FILE fakebuf;

	fakebuf._flag = _IOSTRG;	/* no _IOWRT: avoid stdio bug */
	fakebuf._ptr = s;
	fakebuf._cnt = n-1;
	_doprnt_ansi(fmt, args, &fakebuf);
	fakebuf._cnt++;
	putc('\0', &fakebuf);
	if (fakebuf._cnt<0)
	    fakebuf._cnt = 0;
	return (n-fakebuf._cnt-1);
}

/*
 *  Common code for printf et al.
 *
 *  The calling routine typically takes a variable number of arguments,
 *  and passes the address of the first one.  This implementation
 *  assumes a straightforward, stack implementation, aligned to the
 *  machine's wordsize.  Increasing addresses are assumed to point to
 *  successive arguments (left-to-right), as is the case for a machine
 *  with a downward-growing stack with arguments pushed right-to-left.
 *
 *  To write, for example, fprintf() using this routine, the code
 *
 *	fprintf(fd, format, args)
 *	FILE *fd;
 *	char *format;
 *	{
 *	_doprnt_ansi(format, &args, fd);
 *	}
 *
 *  would suffice.  (This example does not handle the fprintf's "return
 *  value" correctly, but who looks at the return value of fprintf
 *  anyway?)
 *
 *  This version implements the following printf features:
 *
 *	%d	decimal conversion
 *	%u	unsigned conversion
 *	%x	hexadecimal conversion
 *	%X	hexadecimal conversion with capital letters
 *	%o	octal conversion
 *	%c	character
 *	%s	string
 *	%m.n	field width, precision
 *	%-m.n	left adjustment
 *	%0m.n	zero-padding
 *	%*.*	width and precision taken from arguments
 *
 *  This version does not implement %f, %e, or %g.  It accepts, but
 *  ignores, an `l' as in %ld, %lo, %lx, and %lu, and therefore will not
 *  work correctly on machines for which sizeof(long) != sizeof(int).
 *  It does not even parse %D, %O, or %U; you should be using %ld, %o and
 *  %lu if you mean long conversion.
 *
 *  As mentioned, this version does not return any reasonable value.
 *
 *  Permission is granted to use, modify, or propagate this code as
 *  long as this notice is incorporated.
 *
 *  Steve Summit 3/25/87
 */

/*
 * Added for general use:
 *	#	prefix for alternate format:
 *		0x (0X) for hex
 *		leading 0 for octal
 *	+	print '+' if positive
 *	blank	print ' ' if positive
 *
 */

/*
 * Fixed to handle `l' and `h' prefixes, %% format, and ANSI %p format.
 * It does not handle the ANSI %n format.
 *
 * ANSI NOTE: The formating of %d, %o, %u, %x, and %X are not compliant.
 *
 * NOTE: Given that this routine uses stdarg.h, I'm not sure that the
 * comment above about stack layout is valid.
 *
 * Peter Stout, 1/11/93
 */

#define Ctod(c) ((c) - '0')

#define MAXBUF (sizeof(long int) * 8)		 /* enough for binary */

typedef	int	boolean_t;
#define	FALSE	((boolean_t) 0)
#define	TRUE	((boolean_t) 1)

#define	SHORT	sizeof(short)
#define	INT	sizeof(int)
#define	LONG	sizeof(long)

static void
_doprnt_ansi(register const char *fmt, va_list args, FILE *stream)
{
	int		length;
	int		prec;
	boolean_t	ladjust;
	char		padc;
	long		n;
	unsigned long	u;
	int		plus_sign;
	int		sign_char;
	boolean_t	altfmt;
	int		base;
	int		size;
	unsigned char	char_buf[2];

	char_buf[1] = '\0';

	while (*fmt != '\0') {
	    if (*fmt != '%') {
		putc(*fmt++, stream);
		continue;
	    }

	    fmt++;

	    length = 0;
	    prec = -1;
	    ladjust = FALSE;
	    padc = ' ';
	    plus_sign = 0;
	    sign_char = 0;
	    altfmt = FALSE;

	    while (TRUE) {
		if (*fmt == '#') {
		    altfmt = TRUE;
		    fmt++;
		}
		else if (*fmt == '-') {
		    ladjust = TRUE;
		    fmt++;
		}
		else if (*fmt == '+') {
		    plus_sign = '+';
		    fmt++;
		}
		else if (*fmt == ' ') {
		    if (plus_sign == 0)
			plus_sign = ' ';
		    fmt++;
		}
		else
		    break;
	    }

	    if (*fmt == '0') {
		padc = '0';
		fmt++;
	    }

	    if (isdigit(*fmt)) {
		while(isdigit(*fmt))
		    length = 10 * length + Ctod(*fmt++);
	    }
	    else if (*fmt == '*') {
		length = va_arg(args, int);
		fmt++;
		if (length < 0) {
		    ladjust = !ladjust;
		    length = -length;
		}
	    }

	    if (*fmt == '.') {
		prec = 0;
		fmt++;
		if (isdigit(*fmt)) {
		    prec = 0;
		    while(isdigit(*fmt))
			prec = 10 * prec + Ctod(*fmt++);
		}
		else if (*fmt == '*') {
		    prec = va_arg(args, int);
		    fmt++;
		}
	    }

	    if (*fmt == 'l' || *fmt == 'h')
		size = *(fmt++) == 'l' ? LONG : SHORT;
	    else
		size = INT;

	    switch(*fmt) {
		case 'c':
		{
		    register const char *p;
		    register const char *p2;

		    char_buf[0] = va_arg(args, int);
		    p = char_buf;
		    prec = 1;
		    goto put_string;

		case 's':
		    if (prec == -1)
			prec = INT_MAX;

		    p = va_arg(args, char *);

		    if (p == (const char *)0)
			p = "";

		put_string:
		    if (length > 0 && !ladjust) {
			n = 0;
			p2 = p;

			for (; *p != '\0' && n < prec; p++)
			    n++;

			p = p2;

			while (n < length) {
			    putc(padc, stream);
			    n++;
			}
		    }

		    n = 0;

		    while (*p != '\0') {
			if (++n > prec)
			    break;

			putc(*p++, stream);
		    }

		    if (n < length && ladjust) {
			while (n < length) {
			    putc(' ', stream);
			    n++;
			}
		    }

		    break;
		}

		case 'o':
		    base = 8;
		    goto print_unsigned;

		case 'i':
		case 'd':
		    base = 10;
		    goto print_signed;

		case 'u':
		    base = 10;
		    goto print_unsigned;

		case 'x':
		case 'X':
		    base = 16;
		    goto print_unsigned;
		
		case 'p':
		    base = 16;
		    altfmt = TRUE;
		    u = (unsigned long) va_arg(args, void *);
		    goto print_num;

		print_signed:
		    if (size == INT)
			n = va_arg(args, int);
		    else if (size == LONG)
		    	n = va_arg(args, long);
		    else
			n = (short) va_arg(args, int);
		    if (n >= 0) {
			u = n;
			sign_char = plus_sign;
		    }
		    else {
			u = -n;
			sign_char = '-';
		    }
		    goto print_num;

		print_unsigned:
		    if (size == INT)
		    	u = va_arg(args, unsigned int);
		    else if (size == LONG)
			u = va_arg(args, unsigned long);
		    else
			u = (unsigned short) va_arg(args, unsigned int);
		    goto print_num;

		print_num:
		{
		    char	buf[MAXBUF];	/* build number here */
		    register char *	p = &buf[MAXBUF-1];
		    static const char digits[] = "0123456789abcdef";
		    const char *prefix = 0;

		    if (u != 0 && altfmt) {
			if (base == 8)
			    prefix = "0";
			else if (base == 16)
			    prefix = "0x";
		    }

		    do {
			*p-- = digits[u % base];
			u /= base;
		    } while (u != 0);

		    length -= (&buf[MAXBUF-1] - p);
		    if (sign_char)
			length--;
		    if (prefix)
			length -= strlen(prefix);

		    if (padc == ' ' && !ladjust) {
			/* blank padding goes before prefix */
			while (--length >= 0)
			    putc(' ', stream);
		    }
		    if (sign_char)
			putc(sign_char, stream);
		    if (prefix)
			while (*prefix)
			    putc(*prefix++, stream);
		    if (padc == '0') {
			/* zero padding goes after sign and prefix */
			while (--length >= 0)
			    putc('0', stream);
		    }
		    while (++p != &buf[MAXBUF])
			putc(*p, stream);

		    if (ladjust) {
			while (--length >= 0)
			    putc(' ', stream);
		    }
		    break;
		}

		case '%':
		    putc('%', stream);
		    break;

		case '\0':
		    fmt--;
		    break;

		default:
		    putc(*fmt, stream);
	    }
	fmt++;
	}
}

#endif !HAVE_VPRINTF
