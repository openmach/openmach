/*
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
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

#include <sys/types.h>
#include <stdlib.h>

static unsigned seed[2];

int
rand(void)
{
	seed[0] += 0xa859c317;
	seed[0] += (seed[1] << 13) | (seed[1] >> 19);
	seed[1] += seed[0];
	return seed[0] % ((u_long)RAND_MAX + 1);
}

void
srand(unsigned new_seed)
{
	seed[0] = seed[1] = new_seed;
}

#if 0 /* test code */

#define CYCLES 100000000

void main(int argc, char **argv)
{
	unsigned orig_seed = atol(argv[1]);
	int i;

	srand(orig_seed);
	for(i = 0; i < CYCLES; i++)
	{
		int r = rand();
		/*printf("%08x\n", r);*/
		if ((seed[0] == orig_seed) && (seed[1] == orig_seed))
		{
			printf("repeates after %d cycles\n", i);
			exit(0);
		}
	}
	printf("still not repeating after %d cycles\n", CYCLES);
}

#endif

