/* 
 * Copyright (c) 1995-1994 The University of Utah and
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
#ifndef _KUKM_I386_PC_DEBUG_H_
#define _KUKM_I386_PC_DEBUG_H_

#ifdef ASSEMBLER
#ifdef DEBUG


/* Poke a character directly onto the VGA text display,
   as a very quick, mostly-reliable status indicator.
   Assumes ss is a kernel data segment register.  */
#define POKE_STATUS(char,scratch)		\
	ss/*XXX gas bug */			;\
	movl	%ss:_phys_mem_va,scratch	;\
	addl	$0xb8000+80*2*13+40*2,scratch	;\
	movb	char,%ss:(scratch)		;\
	movb	$0xf0,%ss:1(scratch)


#else !DEBUG

#define POKE_STATUS(char,scratch)

#endif !DEBUG
#else !ASSEMBLER
#ifdef DEBUG

#include <mach/machine/vm_types.h>


#define POKE_STATUS(string)						\
	({	unsigned char *s = (string);				\
		extern vm_offset_t phys_mem_va;				\
		short *d = (short*)(phys_mem_va+0xb8000+80*2*13+40*2);	\
		while (*s) { (*d++) = 0x3000 | (*s++); }		\
		*d = ' ';						\
	})


#else !DEBUG

#define POKE_STATUS(char)

#endif !DEBUG
#endif !ASSEMBLER


#include_next "debug.h"

#endif _KUKM_I386_PC_DEBUG_H_
