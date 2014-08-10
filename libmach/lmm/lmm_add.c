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

#include <mach/machine/debug_reg.h>

void lmm_add(lmm_t *lmm, vm_offset_t min, vm_size_t size,
	     lmm_flags_t flags, lmm_pri_t pri)
{
	vm_offset_t max = min + size;
	struct lmm_region *reg, **rp, *r;

	assert((sizeof(struct lmm_region) & ALIGN_MASK) == 0);

	/* Align the start and end addresses appropriately.  */
	min = (min + ALIGN_MASK) & ~ALIGN_MASK;
	max &= ~ALIGN_MASK;

	/* If there's not enough memory to do anything with,
	   then just drop it on the floor.  */
	if ((max < min) || (max - min < sizeof(struct lmm_region) + sizeof(struct lmm_node)))
		return;

	/* Initialize the new region header and its initial free node.  */
	reg = (struct lmm_region*)min;
	reg->nodes = (struct lmm_node*)(reg + 1);
	reg->nodes->next = 0;
	reg->nodes->size = max - (vm_offset_t)reg->nodes;
	reg->flags = flags;
	reg->pri = pri;
	reg->size = max - min;
	reg->free = reg->nodes->size;

	/* Add the region to the lmm's region list in descending priority order.
	   For regions with the same priority, sort from largest to smallest
	   to reduce the average amount of list traversing we need to do.  */
	for (rp = &lmm->regions;
	     (r = *rp) && ((r->pri > pri) || ((r->pri == pri) && (r->size > reg->size)));
	     rp = &r->next);
	reg->next = r;
	*rp = reg;
}

