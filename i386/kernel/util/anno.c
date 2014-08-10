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

#include "anno.h"
#include "debug.h"

#ifdef ENABLE_ANNO

void anno_init()
{
	extern struct anno_entry __ANNO_START__[], __ANNO_END__[];
	struct anno_entry *base;

	/* Sort the tables using a slow, simple selection sort;
	   it only needs to be done once.  */
	for (base = __ANNO_START__; base < __ANNO_END__; base++)
	{
		struct anno_entry *cur, *low, tmp;

		/* Select the lowermost remaining entry,
		   and swap it into the base slot.
		   Sort by table first, then by val1, val2, val3.  */
		low = base;
		for (cur = base+1; cur < __ANNO_END__; cur++)
			if ((cur->table < low->table)
			    || ((cur->table == low->table)
			        && ((cur->val1 < low->val1)
				    || ((cur->val1 == low->val1)
				        && ((cur->val2 < low->val2)
					    || ((cur->val2 == low->val2)
					        && (cur->val3 < low->val3)))))))
				low = cur;
		tmp = *base;
		*base = *low;
		*low = tmp;
	}

	/* Initialize each anno_table structure with entries in the array.  */
	for (base = __ANNO_START__; base < __ANNO_END__; )
	{
		struct anno_entry *end;

		for (end = base;
		     (end < __ANNO_END__) && (end->table == base->table);
		     end++);
		base->table->start = base;
		base->table->end = end;

		base = end;
	}

#if 0 /* debugging code */
	{
		struct anno_table *t = 0;

		for (base = __ANNO_START__; base < __ANNO_END__; base++)
		{
			if (t != base->table)
			{
				t = base->table;
				printf("table %08x: %08x-%08x (%d entries)\n",
					t, t->start, t->end, t->end - t->start);
				assert(t->start == base);
			}
			printf("  vals %08x %08x %08x\n",
				base->table, base->val1, base->val2, base->val3);
		}
	}
#endif
}

#endif ENABLE_ANNO
