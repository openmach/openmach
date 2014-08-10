/* 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
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
#ifndef _MACH_SA_STDIO_H
#define _MACH_SA_STDIO_H

#include <sys/cdefs.h>

/* This is a very naive standard I/O implementation
   which simply chains to the low-level I/O routines
   without doing any buffering or anything.  */

#ifndef NULL
#define NULL 0
#endif

typedef struct
{
	int fd;
} FILE;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#ifndef EOF
#define EOF -1
#endif

__BEGIN_DECLS

int putchar(int c);
int puts(const char *str);
int printf(const char *format, ...);
int sprintf(char *dest, const char *format, ...);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fread(void *buf, int size, int count, FILE *stream);
int fwrite(void *buf, int size, int count, FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int feof(FILE *stream);
long ftell(FILE *stream);
void rewind(FILE *stream);
int rename(const char *from, const char *to);

#define putc(c, stream) fputc(c, stream)

__END_DECLS

#endif _MACH_SA_STDIO_H
