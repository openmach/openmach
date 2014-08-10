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

#include "lmm.h"

void lmm_find_free(lmm_t *lmm, vm_offset_t *inout_addr,
		   vm_size_t *out_size, lmm_flags_t *out_flags)
{
	struct lmm_region *reg;
	vm_offset_t start_addr = (*inout_addr + ALIGN_MASK) & ~ALIGN_MASK;
	vm_offset_t lowest_addr = (vm_offset_t)-1;
	vm_size_t lowest_size = 0;
	unsigned lowest_flags = 0;

	for (reg = lmm->regions; reg; reg = reg->next)
	{
		struct lmm_node *node;

		if ((reg->nodes == 0)
		    || ((vm_offset_t)reg + reg->size <= start_addr)
		    || ((vm_offset_t)reg > lowest_addr))
			continue;

		for (node = reg->nodes; node; node = node->next)
		{
			assert((vm_offset_t)node > (vm_offset_t)reg);
			assert((vm_offset_t)node < (vm_offset_t)reg + reg->size);

			if ((vm_offset_t)node >= lowest_addr)
				break;
			if ((vm_offset_t)node + node->size > start_addr)
			{
				if ((vm_offset_t)node > start_addr)
				{
					lowest_addr = (vm_offset_t)node;
					lowest_size = node->size;
				}
				else
				{
					lowest_addr = start_addr;
					lowest_size = node->size
						- (lowest_addr - (vm_offset_t)node);
				}
				lowest_flags = reg->flags;
				break;
			}
		}
	}

	*inout_addr = lowest_addr;
	*out_size = lowest_size;
	*out_flags = lowest_flags;
}

