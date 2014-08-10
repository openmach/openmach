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
#if 0

#include "lmm.h"

void lmm_remove(vm_offset_t rstart, vm_size_t rsize)
{
	vm_offset_t rend = rstart + rsize;
	struct lmm_region *reg;

	/* Align the start and end addresses appropriately.  */
	rstart = rstart & ~ALIGN_MASK;
	rend = (rend + ALIGN_MASK) & ~ALIGN_MASK;

	for (reg = lmm_region_list; reg; reg = reg->next)
	{
		
	}

	vm_offset_t prstart = rstart - sizeof(struct free_chunk);
	vm_offset_t prend = rend + sizeof(struct free_chunk);
	struct free_chunk **cp;
	struct free_chunk *c;
	vm_offset_t cstart;
	vm_offset_t cend;

	/* Clip the free list as necessary.  */
	for (cp = &free_list; *cp; cp = &(*cp)->next)
	{
		again:

		c = *cp;
		cstart = (vm_offset_t)c;
		cend = cstart + c->size;

		if ((cstart >= prstart) && (cend <= prend))
		{
			/* The free chunk is completely in the reserved region -
			   just remove the chunk.  */
			*cp = c->next;
			goto again;
		}
		else if ((cstart >= prstart) && (cstart < prend))
		{
			/* Cut off the beginning of the chunk.  */
			struct free_chunk *nc = (struct free_chunk*)rend;
			nc->size = c->size - (rend - cstart);
			nc->next = c->next;
			*cp = nc;
		}
		else if ((cend > prstart) && (cend <= prend))
		{
			/* Cut off the end of the chunk.  */
			c->size -= (cend - rstart);
		}
		else if ((cstart < prstart) && (cend > prend))
		{
			/* Split the chunk in two.  */
			struct free_chunk *nc = (struct free_chunk*)rend;
			nc->size = c->size - (rend - cstart);
			c->size = c->size - (cend - rstart);
			nc->next = c->next;
			c->next = nc;
		}
	}
}

#endif
