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
#ifndef _I386_I16_
#define _I386_I16_

#include <mach/machine/code16.h>

#include "gdt.h"


/* Macros to switch between 16-bit and 32-bit code
   in the middle of a C function.
   Be careful with these!  */
#define i16_switch_to_32bit() asm volatile("
		ljmp	%0,$1f
		.code32
	1:
	" : : "i" (KERNEL_CS));
#define switch_to_16bit() asm volatile("
		ljmp	%0,$1f
		.code16
	1:
	" : : "i" (KERNEL_16_CS));


/* From within one type of code, execute 'stmt' in the other.
   These are safer and harder to screw up with than the above macros.  */
#define i16_do_32bit(stmt) \
	({	i16_switch_to_32bit(); \
		{ stmt; } \
		switch_to_16bit(); })
#define do_16bit(stmt) \
	({	switch_to_16bit(); \
		{ stmt; } \
		i16_switch_to_32bit(); })


#endif _I386_I16_
