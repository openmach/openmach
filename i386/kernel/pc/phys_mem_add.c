/* 
 * Copyright (c) 1995 The University of Utah and
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

#include <mach/machine/vm_types.h>
#include <mach/lmm.h>
#include <malloc.h>

#include "vm_param.h"
#include "phys_mem.h"

/* Note that this routine takes _physical_ addresses, not virtual.  */
void phys_mem_add(vm_offset_t min, vm_size_t size)
{
	vm_offset_t max = min + size;

	/* Add the memory region with the proper flags and priority.  */
	if (max <= 1*1024*1024)
	{
		lmm_add(&malloc_lmm, phystokv(min), size,
			LMMF_1MB | LMMF_16MB, LMM_PRI_1MB);
	}
	else
	{
		if (min < 16*1024*1024)
		{
			vm_offset_t nmax = max;
			if (nmax > 16*1024*1024) nmax = 16*1024*1024;
			lmm_add(&malloc_lmm, phystokv(min), nmax - min,
				LMMF_16MB, LMM_PRI_16MB);
		}
		if (max > 16*1024*1024)
		{
			vm_offset_t nmin = min;
			if (nmin < 16*1024*1024) nmin = 16*1024*1024;
			lmm_add(&malloc_lmm, phystokv(nmin), max - nmin, 0, 0);
		}
	}

	if (max > phys_mem_max)
		phys_mem_max = max;
}

