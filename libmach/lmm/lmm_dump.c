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

#ifdef DEBUG

void lmm_dump(lmm_t *lmm)
{
	struct lmm_region *reg;

	printf("lmm_dump(lmm=%08x)\n", lmm);

	for (reg = lmm->regions; reg; reg = reg->next)
	{
		struct lmm_node *node;
		vm_size_t free_check;

		printf(" region %08x-%08x size=%08x flags=%08x pri=%d free=%08x\n",
			reg, (vm_offset_t)reg + reg->size, reg->size,
			reg->flags, reg->pri, reg->free);

		assert((vm_offset_t)reg->nodes >= (vm_offset_t)(reg+1));
		assert(reg->free <= reg->size - sizeof(struct lmm_region));

		free_check = 0;
		for (node = reg->nodes; node; node = node->next)
		{
			printf("  node %08x-%08x size=%08x next=%08x\n", 
				node, (vm_offset_t)node + node->size, node->size, node->next);

			assert(((vm_offset_t)node & ALIGN_MASK) == 0);
			assert(((vm_offset_t)node->size & ALIGN_MASK) == 0);
			assert((node->next == 0) || (node->next > node));
			assert((vm_offset_t)node < (vm_offset_t)reg + reg->size);

			free_check += node->size;
		}

		printf(" free_check=%08x\n", free_check);
		assert(reg->free == free_check);
	}

	printf("lmm_dump done\n");
}

#endif /* DEBUG */
