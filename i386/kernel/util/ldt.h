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
#ifndef _I386_UTIL_LDT_
#define _I386_UTIL_LDT_

#include "config.h"

/* If more-specific code wants a standard LDT,
   then it should define ENABLE_KERNEL_LDT in config.h.  */
#ifdef ENABLE_KERNEL_LDT

#include <mach/machine/seg.h>

/* Fill a segment descriptor in a CPU's master LDT.  */
#define fill_ldt_descriptor(cpu, selector, base, limit, access, sizebits) \
	fill_descriptor(&(cpu)->tables.ldt[(selector)/8],		\
			base, limit, access, sizebits)

#define fill_ldt_gate(cpu, selector, offset, dest_selector, access, word_count) \
	fill_gate((struct i386_gate*)&(cpu)->tables.ldt[(selector)/8], \
		  offset, dest_selector, access, word_count)

#endif ENABLE_KERNEL_LDT

#endif _I386_UTIL_LDT_
