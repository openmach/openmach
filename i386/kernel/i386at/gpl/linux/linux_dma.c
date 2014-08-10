/*
 * Linux DMA channel management.
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

#define MACH_INCLUDE
#include <linux/errno.h>
#include <asm/dma.h>

/*
 * Bitmap of allocated/free DMA channels.
 */
static int dma_busy = 0x10;

/*
 * Allocate a DMA channel.
 */
int
request_dma(unsigned int drq, const char *name)
{
	if (drq > 7)
		panic("request_dma: bad DRQ number");
	if (dma_busy & (1 << drq))
		return (-LINUX_EBUSY);
	dma_busy |= 1 << drq;
	return (0);
}

/*
 * Free a DMA channel.
 */
void
free_dma(unsigned int drq)
{
	if (drq > 7)
		panic("free_dma: bad DRQ number");
	dma_busy &= ~(1 << drq);
}
