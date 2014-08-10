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
 *	Machine-dependent definitions for cpu identification.
 *
 */
#ifndef	_I386_CPU_NUMBER_H_
#define	_I386_CPU_NUMBER_H_

#if	NCPUS > 1

/* More-specific code must define cpu_number() and CPU_NUMBER.  */
#define	CX(addr, reg)	addr(,reg,4)

#else	/* NCPUS == 1 */

#define	CPU_NUMBER(reg)
#define	CX(addr,reg)	addr

#endif	/* NCPUS == 1 */

#ifndef ASSEMBLER
#include "kern/cpu_number.h"
#endif

#endif	/* _I386_CPU_NUMBER_H_ */
