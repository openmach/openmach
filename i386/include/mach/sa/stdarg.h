/*
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University.
 * Copyright (c) 1994 The University of Utah and
 * the Center for Software Science (CSS).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
#ifndef _MACH_SA_STDARG_H_
#define _MACH_SA_STDARG_H_

#define __va_size(type) ((sizeof(type)+3) & ~0x3)

#ifndef _VA_LIST_
#define _VA_LIST_
typedef	char *va_list;
#endif

#define	va_start(pvar, lastarg)			\
	((pvar) = (char*)(void*)&(lastarg) + __va_size(lastarg))
#define	va_end(pvar)
#define	va_arg(pvar,type)			\
	((pvar) += __va_size(type),		\
	 *((type *)((pvar) - __va_size(type))))

#endif _MACH_SA_STDARG_H_
