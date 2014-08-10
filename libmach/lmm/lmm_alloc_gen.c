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

void *lmm_alloc_gen(lmm_t *lmm, vm_size_t size, unsigned flags,
		    int align_bits, vm_offset_t align_ofs,
		    vm_offset_t in_min, vm_size_t in_size)
{
	vm_offset_t align_size = (vm_offset_t)1 << align_bits;
	vm_offset_t align_mask = align_size - 1;
	vm_offset_t in_max = in_min + in_size;
	struct lmm_region *reg;

#if 0
	printf("lmm_alloc_gen %08x\n", size);
	lmm_dump(lmm);
#endif

	for (reg = lmm->regions; reg; reg = reg->next)
	{
		struct lmm_node **nodep, *node;

		assert((vm_offset_t)reg->nodes >= (vm_offset_t)(reg+1));
		assert(reg->free <= reg->size - sizeof(struct lmm_region));

		/* First trivially reject the entire region if possible.  */
		if ((flags & ~reg->flags) 
		    || ((vm_offset_t)reg >= in_max)
		    || ((vm_offset_t)reg + reg->size <= in_min))
			continue;

		for (nodep = &reg->nodes; node = *nodep; nodep = &node->next)
		{
			vm_offset_t addr;
			struct lmm_node *anode;
			int i;

			assert(((vm_offset_t)node & ALIGN_MASK) == 0);
			assert(((vm_offset_t)node->size & ALIGN_MASK) == 0);
			assert((node->next == 0) || (node->next > node));
			assert((vm_offset_t)node < (vm_offset_t)reg + reg->size);

			/* Now make a first-cut trivial elimination check
			   to skip chunks that are _definitely_ too small.  */
			if (node->size < size)
				continue;

			/* Now compute the address at which the allocated chunk would have to start.  */
			addr = (vm_offset_t)node;
			if (addr < in_min)
				addr = in_min;
			for (i = 0; i < align_bits; i++)
			{
				vm_offset_t bit = (vm_offset_t)1 << i;
				if ((addr ^ align_ofs) & bit)
					addr += bit;
			}

			/* See if the block at the adjusted address is still entirely within the node.  */
			if ((addr - (vm_offset_t)node + size) > node->size)
				continue;

			/* If the block extends past the range constraint,
			   then all of the rest of the nodes in this region will too. */
			if (addr + size > in_max)
				break;

			/* OK, we can allocate the block from this node.  */

			/* If the allocation leaves at least ALIGN_SIZE space before it,
			   then split the node.  */
			anode = (struct lmm_node*)(addr & ~ALIGN_MASK);
			assert(anode >= node);
			if (anode > node)
			{
				vm_size_t split_size = (vm_offset_t)anode - (vm_offset_t)node;
				assert(((vm_size_t)split_size & ALIGN_MASK) == 0);
				anode->next = node->next;
				anode->size = node->size - split_size;
				node->size = split_size;
				nodep = &node->next;
			}

			/* Now use the first part of the anode to satisfy the allocation,
			   splitting off the tail end if necessary.  */
			size = ((addr & ALIGN_MASK) + size + ALIGN_MASK) & ~ALIGN_MASK;
			if (anode->size > size)
			{
				struct lmm_node *newnode;

				/* Split the node and return its head.  */
				newnode = (struct lmm_node*)((void*)anode + size);
				newnode->next = anode->next;
				newnode->size = anode->size - size;
				*nodep = newnode;
			}
			else
			{
				/* Remove and return the entire node.  */
				*nodep = anode->next;
			}

			/* Adjust the region's free memory counter.  */
			assert(reg->free >= size);
			reg->free -= size;

#if 0
			printf("lmm_alloc_gen returning %08x\n", addr);
			lmm_dump(lmm);
#endif

			return (void*)addr;
		}
	}

#if 0
	printf("lmm_alloc_gen failed\n");
#endif

	return 0;
}

