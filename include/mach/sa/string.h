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
#ifndef _MACH_STRING_H_
#define _MACH_STRING_H_

#include <sys/cdefs.h>

#ifndef NULL
#define NULL 0
#endif

__BEGIN_DECLS

__DECL(char *,strdup(const char *s));
__DECL(char *,strcat(char *dest, const char *src));
__DECL(int,strcmp(const char *a, const char *b));
__DECL(int,strncpy(char *dest, const char *src, int n));
__DECL(int,strncmp(const char *a, const char *b, int n));

__DECL(char *,strchr(const char *s, int c));
__DECL(char *,strrchr(const char *s, int c));
__DECL(char *,index(const char *s, int c));
__DECL(char *,rindex(const char *s, int c));
__DECL(void *,strstr(const char *haystack, const char *needle));

#ifndef __GNUC__
__DECL(void *,memcpy(void *to, const void *from, unsigned int n));
#endif
__DECL(void *,memset(void *to, int ch, unsigned int n));

__DECL(void,bcopy(const void *from, void *to, unsigned int n));
__DECL(void,bzero(void *to, unsigned int n));

__END_DECLS

#endif _MACH_STRING_H_
