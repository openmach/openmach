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

#include <stdio.h>
#include <stdarg.h>

#include "global.h"
#include "error.h"
#include "lexxer.h"

static const char *program;
int errors = 0;

void
fatal(const char *format, ...)
{
    va_list pvar;
    va_start(pvar, format);
    fprintf(stderr, "%s: fatal: ", program);
    (void) vfprintf(stderr, format, pvar);
    fprintf(stderr, "\n");
    va_end(pvar);
    exit(1);
}

void
warn(const char *format, ...)
{
    va_list pvar;
    va_start(pvar, format);
    if (!BeQuiet && (errors == 0))
    {
	fprintf(stderr, "\"%s\", line %d: warning: ", inname, lineno-1);
	(void) vfprintf(stderr, format, pvar);
	fprintf(stderr, "\n");
    }
    va_end(pvar);
}

void
error(const char *format, ...)
{
    va_list pvar;
    va_start(pvar, format);
    fprintf(stderr, "\"%s\", line %d: ", inname, lineno-1);
    (void) vfprintf(stderr, format, pvar);
    fprintf(stderr, "\n");
    va_end(pvar);
    errors++;
}

const char *
unix_error_string(int error_num)
{
    static char buffer[256];
    const char *error_mess;

#ifdef HAVE_STRERROR
    error_mess = strerror (error_num);
#else
    extern int sys_nerr;
    extern char *sys_errlist[];

    if ((0 <= error_num) && (error_num < sys_nerr))
	error_mess = sys_errlist[error_num];
    else
	error_mess = "strange errno";
#endif

    sprintf(buffer, "%s (%d)", error_mess, error_num);
    return buffer;
}

void
set_program_name(const char *name)
{
    program = name;
}
