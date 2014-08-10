/*
 * Linux memory allocation.
 * 
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
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
 *
 * 	Author: Shantanu Goel, University of Utah CSL
 *
 */

#include <sys/types.h>

#include <mach/mach_types.h>
#include <mach/vm_param.h>

#include <kern/assert.h>
#include <kern/kalloc.h>

#include <vm/vm_page.h>

#include <i386at/gpl/linux/linux_emul.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/malloc.h>
#include <linux/delay.h>

#include <asm/system.h>

/* Amount of memory to reserve for Linux memory allocator.
   We reserve 64K chunks to stay within DMA limits.
   Increase MEM_CHUNKS if the kernel is running out of memory.  */
#define MEM_CHUNK_SIZE	(64 * 1024)
#define MEM_CHUNKS	3

/* Mininum amount that linux_kmalloc will allocate.  */
#define MIN_ALLOC	12

#ifndef NBPW
#define NBPW		32
#endif

/* Memory block header.  */
struct blkhdr
{
  unsigned short free;	/* 1 if block is free */
  unsigned short size;	/* size of block */
};

/* This structure heads a page allocated by linux_kmalloc.  */
struct pagehdr
{
  unsigned size;	/* size (multiple of PAGE_SIZE) */
  struct pagehdr *next;	/* next header in list */
};

/* This structure describes a memory chunk.  */
struct chunkhdr
{
  unsigned long start;	/* start address */
  unsigned long end;	/* end address */
  unsigned long bitmap;	/* busy/free bitmap of pages */
};

/* Chunks from which pages are allocated.  */
static struct chunkhdr pages_free[MEM_CHUNKS];

/* Memory list maintained by linux_kmalloc.  */
static struct pagehdr *memlist;

/* Some statistics.  */
int num_block_coalesce = 0;
int num_page_collect = 0;
int linux_mem_avail;

/* Initialize the Linux memory allocator.  */
void
linux_kmem_init ()
{
  int i, j;
  vm_page_t p, pages;

  for (i = 0; i < MEM_CHUNKS; i++)
    {
      /* Allocate memory.  */
      pages_free[i].start = (unsigned long) alloc_contig_mem (MEM_CHUNK_SIZE,
							      16 * 1024 * 1024,
							      0xffff, &pages);

      assert (pages_free[i].start);
      assert ((pages_free[i].start & 0xffff) == 0);

      /* Sanity check: ensure pages are contiguous and within DMA limits.  */
      for (p = pages, j = 0; j < MEM_CHUNK_SIZE - PAGE_SIZE; j += PAGE_SIZE)
	{
	  assert (p->phys_addr < 16 * 1024 * 1024);
	  assert (p->phys_addr + PAGE_SIZE
		  == ((vm_page_t) p->pageq.next)->phys_addr);

	  p = (vm_page_t) p->pageq.next;
	}

      pages_free[i].end = pages_free[i].start + MEM_CHUNK_SIZE;

      /* Initialize free page bitmap.  */
      pages_free[i].bitmap = 0;
      j = MEM_CHUNK_SIZE >> PAGE_SHIFT;
      while (--j >= 0)
	pages_free[i].bitmap |= 1 << j;
    }

  linux_mem_avail = (MEM_CHUNKS * MEM_CHUNK_SIZE) >> PAGE_SHIFT;
}

/* Return the number by which the page size should be
   shifted such that the resulting value is >= SIZE.  */
static unsigned long
get_page_order (int size)
{
  unsigned long order;

  for (order = 0; (PAGE_SIZE << order) < size; order++)
    ;
  return order;
}

#ifdef LINUX_DEV_DEBUG
static void
check_page_list (int line)
{
  unsigned size;
  struct pagehdr *ph;
  struct blkhdr *bh;

  for (ph = memlist; ph; ph = ph->next)
    {
      if ((int) ph & PAGE_MASK)
	panic ("%s:%d: page header not aligned", __FILE__, line);

      size = 0;
      bh = (struct blkhdr *) (ph + 1);
      while (bh < (struct blkhdr *) ((void *) ph + ph->size))
	{
	  size += bh->size + sizeof (struct blkhdr);
	  bh = (void *) (bh + 1) + bh->size;
	}

      if (size + sizeof (struct pagehdr) != ph->size)
	panic ("%s:%d: memory list destroyed", __FILE__, line);
    }
}
#else
#define check_page_list(line)
#endif

/* Merge adjacent free blocks in the memory list.  */
static void
coalesce_blocks ()
{
  struct pagehdr *ph;
  struct blkhdr *bh, *bhp, *ebh;

  num_block_coalesce++;

  for (ph = memlist; ph; ph = ph->next)
    {
      bh = (struct blkhdr *) (ph + 1);
      ebh = (struct blkhdr *) ((void *) ph + ph->size);
      while (1)
	{
	  /* Skip busy blocks.  */
	  while (bh < ebh && ! bh->free)
	    bh = (struct blkhdr *) ((void *) (bh + 1) + bh->size);
	  if (bh == ebh)
	    break;

	  /* Merge adjacent free blocks.  */
	  while (1)
	    {
	      bhp = (struct blkhdr *) ((void *) (bh + 1) + bh->size);
	      if (bhp == ebh)
		{
		  bh = bhp;
		  break;
		}
	      if (! bhp->free)
		{
		  bh = (struct blkhdr *) ((void *) (bhp + 1) + bhp->size);
		  break;
		}
	      bh->size += bhp->size + sizeof (struct blkhdr);
	    }
	}
    }
}

/* Allocate SIZE bytes of memory.
   The PRIORITY parameter specifies various flags
   such as DMA, atomicity, etc.  It is not used by Mach.  */
void *
linux_kmalloc (unsigned int size, int priority)
{
  int order, coalesced = 0;
  unsigned flags;
  struct pagehdr *ph;
  struct blkhdr *bh, *new_bh;

  if (size < MIN_ALLOC)
    size = MIN_ALLOC;
  else
    size = (size + sizeof (int) - 1) & ~(sizeof (int) - 1);

  assert (size <= (MEM_CHUNK_SIZE
		   - sizeof (struct pagehdr)
		   - sizeof (struct blkhdr)));

  save_flags (flags);
  cli ();

again:
  check_page_list (__LINE__);

  /* Walk the page list and find the first free block with size
     greater than or equal to the one required.  */
  for (ph = memlist; ph; ph = ph->next)
    {
      bh = (struct blkhdr *) (ph + 1);
      while (bh < (struct blkhdr *) ((void *) ph + ph->size))
	{
	  if (bh->free && bh->size >= size)
	    {
	      bh->free = 0;
	      if (bh->size - size >= MIN_ALLOC + sizeof (struct blkhdr))
		{
		  /* Split the current block and create a new free block.  */
		  new_bh = (void *) (bh + 1) + size;
		  new_bh->free = 1;
		  new_bh->size = bh->size - size - sizeof (struct blkhdr);
		  bh->size = size;
		}

	      check_page_list (__LINE__);

	      restore_flags (flags);
	      return bh + 1;
	    }
	  bh = (void *) (bh + 1) + bh->size;
	}
    }

  check_page_list (__LINE__); 

  /* Allocation failed; coalesce free blocks and try again.  */
  if (! coalesced)
    {
      coalesce_blocks ();
      coalesced = 1;
      goto again;
    }

  /* Allocate more pages.  */
  order = get_page_order (size
			  + sizeof (struct pagehdr)
			  + sizeof (struct blkhdr));
  ph = (struct pagehdr *) __get_free_pages (GFP_KERNEL, order, ~0UL);
  if (! ph)
    {
      restore_flags (flags);
      return NULL;
    }

  ph->size = PAGE_SIZE << order;
  ph->next = memlist;
  memlist = ph;
  bh = (struct blkhdr *) (ph + 1);
  bh->free = 0;
  bh->size = ph->size - sizeof (struct pagehdr) - sizeof (struct blkhdr);
  if (bh->size - size >= MIN_ALLOC + sizeof (struct blkhdr))
    {
      new_bh = (void *) (bh + 1) + size;
      new_bh->free = 1;
      new_bh->size = bh->size - size - sizeof (struct blkhdr);
      bh->size = size;
    }

  check_page_list (__LINE__);

  restore_flags (flags);
  return bh + 1;
}

/* Free memory P previously allocated by linux_kmalloc.  */
void
linux_kfree (void *p)
{
  unsigned flags;
  struct blkhdr *bh, *bhp;
  struct pagehdr *ph;

  assert (((int) p & (sizeof (int) - 1)) == 0);

  save_flags (flags);
  cli ();

  check_page_list (__LINE__);

  for (ph = memlist; ph; ph = ph->next)
    if (p >= (void *) ph && p < (void *) ph + ph->size)
      break;

  assert (ph);

  bh = (struct blkhdr *) p - 1;

  assert (! bh->free);
  assert (bh->size >= MIN_ALLOC);
  assert ((bh->size & (sizeof (int) - 1)) == 0);

  bh->free = 1;

  check_page_list (__LINE__);

  restore_flags (flags);
}

/* Free any pages that are not in use.
   Called by __get_free_pages when pages are running low.  */
static void
collect_kmalloc_pages ()
{
  struct blkhdr *bh;
  struct pagehdr *ph, **prev_ph;

  check_page_list (__LINE__);

  coalesce_blocks ();

  check_page_list (__LINE__);

  ph = memlist;
  prev_ph = &memlist;
  while (ph)
    {
      bh = (struct blkhdr *) (ph + 1);
      if (bh->free && (void *) (bh + 1) + bh->size == (void *) ph + ph->size)
	{
	  *prev_ph = ph->next;
	  free_pages ((unsigned long) ph, get_page_order (ph->size));
	  ph = *prev_ph;
	}
      else
	{
	  prev_ph = &ph->next;
	  ph = ph->next;
	}
    }

  check_page_list (__LINE__);
}

/* Allocate ORDER + 1 number of physically contiguous pages.
   PRIORITY and MAX_ADDR are not used in Mach.

   XXX: This needs to be dynamic.  To do that we need to make
   the Mach page manipulation routines interrupt safe and they
   must provide machine dependant hooks.  */
unsigned long
__get_free_pages (int priority, unsigned long order, unsigned long max_addr)
{
  int i, pages_collected = 0;
  unsigned flags, bits, off, j, len;

  assert ((PAGE_SIZE << order) <= MEM_CHUNK_SIZE);

  /* Construct bitmap of contiguous pages.  */
  bits = 0;
  j = 0;
  len = 0;
  while (len < (PAGE_SIZE << order))
    {
      bits |= 1 << j++;
      len += PAGE_SIZE;
    }

again:
  save_flags (flags);
  cli ();

  /* Search each chunk for the required number of contiguous pages.  */
  for (i = 0; i < MEM_CHUNKS; i++)
    {
      off = 0;
      j = bits;
      while (MEM_CHUNK_SIZE - off >= (PAGE_SIZE << order))
	{
	  if ((pages_free[i].bitmap & j) == j)
	    {
	      pages_free[i].bitmap &= ~j;
	      linux_mem_avail -= order + 1;
	      restore_flags (flags);
	      return pages_free[i].start + off;
	    }
	  j <<= 1;
	  off += PAGE_SIZE;
	}
    }

  /* Allocation failed; collect kmalloc and buffer pages
     and try again.  */
  if (! pages_collected)
    {
      num_page_collect++;
      collect_kmalloc_pages ();
      collect_buffer_pages ();
      pages_collected = 1;
      goto again;
    }

  printf ("%s:%d: __get_free_pages: ran out of pages\n", __FILE__, __LINE__);

  restore_flags (flags);
  return 0;
}

/* Free ORDER + 1 number of physically
   contiguous pages starting at address ADDR.  */
void
free_pages (unsigned long addr, unsigned long order)
{
  int i;
  unsigned flags, bits, len, j;

  assert ((addr & PAGE_MASK) == 0);

  for (i = 0; i < MEM_CHUNKS; i++)
    if (addr >= pages_free[i].start && addr < pages_free[i].end)
      break;

  assert (i < MEM_CHUNKS);

  /* Contruct bitmap of contiguous pages.  */
  len = 0;
  j = 0;
  bits = 0;
  while (len < (PAGE_SIZE << order))
    {
      bits |= 1 << j++;
      len += PAGE_SIZE;
    }
  bits <<= (addr - pages_free[i].start) >> PAGE_SHIFT;

  save_flags (flags);
  cli ();

  assert ((pages_free[i].bitmap & bits) == 0);

  pages_free[i].bitmap |= bits;
  linux_mem_avail += order + 1;
  restore_flags (flags);
}

/* Allocate SIZE bytes of memory.  The pages need not be contiguous.  */
void *
vmalloc (unsigned long size)
{
  return (void *) __get_free_pages (GFP_KERNEL, get_page_order (size), ~0UL);
}
