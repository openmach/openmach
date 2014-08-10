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

void *lmm_alloc(lmm_t *lmm, vm_size_t size, lmm_flags_t flags)
{
	struct lmm_region *reg;

	size = (size + ALIGN_MASK) & ~ALIGN_MASK;

	for (reg = lmm->regions; reg; reg = reg->next)
	{
		struct lmm_node **nodep, *node;

		assert((vm_offset_t)reg->nodes >= (vm_offset_t)(reg+1));
		assert(reg->free <= reg->size - sizeof(struct lmm_region));

		if (flags & ~reg->flags)
			continue;

		for (nodep = &reg->nodes; node = *nodep; nodep = &node->next)
		{
			assert(((vm_offset_t)node & ALIGN_MASK) == 0);
			assert(((vm_offset_t)node->size & ALIGN_MASK) == 0);
			assert((node->next == 0) || (node->next > node));
			assert((vm_offset_t)node < (vm_offset_t)reg + reg->size);

			if (node->size >= size)
			{
				if (node->size > size)
				{
					struct lmm_node *newnode;

					/* Split the node and return its head.  */
					newnode = (struct lmm_node*)((void*)node + size);
					newnode->next = node->next;
					newnode->size = node->size - size;
					*nodep = newnode;
				}
				else
				{
					/* Remove and return the entire node.  */
					*nodep = node->next;
				}

				/* Adjust the region's free memory counter.  */
				assert(reg->free >= size);
				reg->free -= size;

				return (void*)node;
			}
		}
	}

	return 0;
}

