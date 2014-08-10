/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
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
/*
 */
/*
 *	File:	ipc/ipc_port.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Implementation specific complement to mach/port.h.
 */

#ifndef	_IPC_PORT_H_
#define _IPC_PORT_H_

#include <mach/port.h>

/*
 *	mach_port_t must be an unsigned type.  Port values
 *	have two parts, a generation number and an index.
 *	These macros encapsulate all knowledge of how
 *	a mach_port_t is layed out.  However, ipc/ipc_entry.c
 *	implicitly assumes when it uses the splay tree functions
 *	that the generation number is in the low bits, so that
 *	names are ordered first by index and then by generation.
 *
 *	If the size of generation numbers changes,
 *	be sure to update IE_BITS_GEN_MASK and friends
 *	in ipc/ipc_entry.h.
 */

#if PORT_GENERATIONS
#define	MACH_PORT_INDEX(name)		((name) >> 8)
#define	MACH_PORT_GEN(name)		(((name) & 0xff) << 24)
#define	MACH_PORT_MAKE(index, gen)	(((index) << 8) | ((gen) >> 24))
#else
#define	MACH_PORT_INDEX(name)		(name)
#define	MACH_PORT_GEN(name)		0
#define	MACH_PORT_MAKE(index, gen)	(index)
#endif

#define	MACH_PORT_NGEN(name)		MACH_PORT_MAKE(0, MACH_PORT_GEN(name))
#define	MACH_PORT_MAKEB(index, bits)	MACH_PORT_MAKE(index, IE_BITS_GEN(bits))

/*
 *	Typedefs for code cleanliness.  These must all have
 *	the same (unsigned) type as mach_port_t.
 */

typedef mach_port_t mach_port_index_t;		/* index values */
typedef mach_port_t mach_port_gen_t;		/* generation numbers */


#define	MACH_PORT_UREFS_MAX	((mach_port_urefs_t) ((1 << 16) - 1))

#define	MACH_PORT_UREFS_OVERFLOW(urefs, delta)				\
		(((delta) > 0) &&					\
		 ((((urefs) + (delta)) <= (urefs)) ||			\
		  (((urefs) + (delta)) > MACH_PORT_UREFS_MAX)))

#define	MACH_PORT_UREFS_UNDERFLOW(urefs, delta)				\
		(((delta) < 0) && (-(delta) > (urefs)))

#endif	/* _IPC_PORT_H_ */
