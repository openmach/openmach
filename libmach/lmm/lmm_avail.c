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

vm_size_t lmm_avail(lmm_t *lmm, lmm_flags_t flags)
{
	struct lmm_region *reg;
	vm_size_t count;

	count = 0;
	for (reg = lmm->regions; reg; reg = reg->next)
	{
		/* Don't count inapplicable regions.  */
		if (flags & ~reg->flags)
			continue;

		count += reg->free;

		assert((vm_offset_t)reg->nodes >= (vm_offset_t)(reg+1));
		assert(reg->free <= reg->size - sizeof(struct lmm_region));

		/* XXX sanity-check the region's free count */
	}
	return count;
}

