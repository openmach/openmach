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

#if !defined(DEBUG) && !defined(NDEBUG)
#define NDEBUG
#endif
#include <assert.h>

#include <mach/lmm.h>

struct lmm_region
{
	struct lmm_region *next;

	/* List of free nodes in this region.  */
	struct lmm_node *nodes;

	/* Attributes of this memory.  */
	lmm_flags_t flags;

	/* Allocation priority of this region with respect to other regions.  */
	lmm_pri_t pri;

	/* Total size of the region including this header structure.  */
	vm_size_t size;

	/* Current amount of free space in this region in bytes.  */
	vm_size_t free;
};

struct lmm_node
{
	struct lmm_node *next;
	vm_size_t size;
};

#define ALIGN_SIZE	sizeof(struct lmm_node)
#define ALIGN_MASK	(ALIGN_SIZE - 1)

