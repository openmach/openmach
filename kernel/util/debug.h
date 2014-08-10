/* 
 * Copyright (c) 1995-1993 The University of Utah and
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
/*
 *	File:	debug.h
 *	Author:	Bryan Ford
 *
 *	This file contains definitions for kernel debugging,
 *	which are compiled in on the DEBUG symbol.
 *
 */
#ifndef _MACH_UTIL_DEBUG_H_
#define _MACH_UTIL_DEBUG_H_

#include <mach/macro_help.h>


#ifdef DEBUG

extern void panic(const char *fmt, ...);

#define here() printf("@ %s:%d\n", __FILE__, __LINE__)
#define message(args) ({ printf("@ %s:%d: ", __FILE__, __LINE__); printf args; printf("\n"); })

#define otsan() panic("%s:%d: off the straight and narrow!", __FILE__, __LINE__)

#define assert(v)							\
	MACRO_BEGIN							\
		if (!(v))						\
			panic("%s:%d: failed assertion `"#v"'\n",	\
				__FILE__, __LINE__);			\
	MACRO_END

#define do_debug(stmt) stmt

#define debugmsg(args) \
	({ printf("%s:%d: ", __FILE__, __LINE__); printf args; printf("\n"); })

#define struct_id_decl		unsigned struct_id;
#define struct_id_init(p,id)	((p)->struct_id = (id))
#define struct_id_denit(p)	((p)->struct_id = 0)
#define struct_id_verify(p,id) \
	({ if ((p)->struct_id != (id)) \
		panic("%s:%d: "#p" (%08x) struct_id should be "#id" (%08x), is %08x\n", \
			__FILE__, __LINE__, (p), (id), (p->struct_id)); \
	})

#else !DEBUG

#define otsan()
#define assert(v)
#define do_debug(stmt)
#define debugmsg(args)

#define struct_id_decl
#define struct_id_init(p,id)
#define struct_id_denit(p)
#define struct_id_verify(p,id)

#endif !DEBUG

#endif _MACH_UTIL_DEBUG_H_
