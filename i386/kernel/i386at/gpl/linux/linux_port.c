/*
 * Linux I/O port management.
 * Copyright (C) 1995 Shantanu Goel.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/ioport.h>

#define NPORTS		65536
#define BITS_PER_WORD	32
#define NWORDS		(NPORTS / BITS_PER_WORD)

/*
 * This bitmap keeps track of all allocated ports.
 * A bit is set if the port has been allocated.
 */
static unsigned port_bitmap[NWORDS];

void snarf_region(unsigned, unsigned);

/*
 * Check if a region is available for use.
 */
int
check_region(unsigned port, unsigned size)
{
	unsigned i;

	for (i = port; i < port + size; i++)
		if (port_bitmap[i/BITS_PER_WORD] & (1 << (i%BITS_PER_WORD)))
			return (1);
	return (0);
}

/*
 * Allocate a region.
 */
void
request_region(unsigned port, unsigned size, const char *name)
{
	unsigned i;

	for (i = port; i < port + size; i++)
		port_bitmap[i / BITS_PER_WORD] |= 1 << (i % BITS_PER_WORD);
}

/*
 * For compatibility with older kernels.
 */
void
snarf_region(unsigned port, unsigned size)
{
	request_region(port, size, 0);
}

/*
 * Deallocate a region.
 */
void
release_region(unsigned port, unsigned size)
{
	unsigned i;

	for (i = port; i < port + size; i++)
		port_bitmap[i / BITS_PER_WORD] &= ~(1 << (i % BITS_PER_WORD));
}
