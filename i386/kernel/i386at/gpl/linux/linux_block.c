/*
 * Linux block driver support.
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
 *	Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/drivers/block/ll_rw_blk.c
 *
 * Copyright (C) 1991, 1992 Linus Torvalds
 * Copyright (C) 1994,      Karl Keyte: Added support for disk statistics
 */

/*
 *  linux/fs/block_dev.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/*
 *  linux/fs/buffer.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
#include <mach/vm_param.h>
#include <mach/notify.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <device/device_types.h>
#include <device/device_port.h>
#include <device/disk_status.h>
#include "device_reply.h"

#include <i386at/dev_hdr.h>
#include <i386at/device_emul.h>
#include <i386at/disk.h>

#include <i386at/gpl/linux/linux_emul.h>

#define MACH_INCLUDE
#include <linux/fs.h>
#include <linux/blk.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/malloc.h>

/* Location of VTOC in units for sectors (512 bytes).  */
#define PDLOCATION 29

/* Linux kernel variables.  */

/* One of these exists for each
   driver associated with a major number.  */
struct device_struct
{
  const char *name;		/* device name */
  struct file_operations *fops;	/* operations vector */
  int busy:1;			/* driver is being opened/closed */
  int want:1;			/* someone wants to open/close driver */
  struct gendisk *gd;		/* DOS partition information */
  int *default_slice;		/* what slice to use when none is given */
  struct disklabel **label;	/* disklabels for each DOS partition */
};

/* An entry in the Mach name to Linux major number conversion table.  */
struct name_map
{
  const char *name;	/* Mach name for device */
  unsigned major;	/* Linux major number */
  unsigned unit;	/* Linux unit number */
  int read_only;	/* 1 if device is read only */
};

/* Driver operation table.  */
static struct device_struct blkdevs[MAX_BLKDEV];

/* Driver request function table.  */
struct blk_dev_struct blk_dev[MAX_BLKDEV] =
{
  { NULL, NULL },		/* 0 no_dev */
  { NULL, NULL },		/* 1 dev mem */
  { NULL, NULL },		/* 2 dev fd */
  { NULL, NULL },		/* 3 dev ide0 or hd */
  { NULL, NULL },		/* 4 dev ttyx */
  { NULL, NULL },		/* 5 dev tty */
  { NULL, NULL },		/* 6 dev lp */
  { NULL, NULL },		/* 7 dev pipes */
  { NULL, NULL },		/* 8 dev sd */
  { NULL, NULL },		/* 9 dev st */
  { NULL, NULL },		/* 10 */
  { NULL, NULL },		/* 11 */
  { NULL, NULL },		/* 12 */
  { NULL, NULL },		/* 13 */
  { NULL, NULL },		/* 14 */
  { NULL, NULL },		/* 15 */
  { NULL, NULL },		/* 16 */
  { NULL, NULL },		/* 17 */
  { NULL, NULL },		/* 18 */
  { NULL, NULL },		/* 19 */
  { NULL, NULL },		/* 20 */
  { NULL, NULL },		/* 21 */
  { NULL, NULL }		/* 22 dev ide1 */
};

/*
 * blk_size contains the size of all block-devices in units of 1024 byte
 * sectors:
 *
 * blk_size[MAJOR][MINOR]
 *
 * if (!blk_size[MAJOR]) then no minor size checking is done.
 */
int *blk_size[MAX_BLKDEV] = { NULL, NULL, };

/*
 * blksize_size contains the size of all block-devices:
 *
 * blksize_size[MAJOR][MINOR]
 *
 * if (!blksize_size[MAJOR]) then 1024 bytes is assumed.
 */
int *blksize_size[MAX_BLKDEV] = { NULL, NULL, };

/*
 * hardsect_size contains the size of the hardware sector of a device.
 *
 * hardsect_size[MAJOR][MINOR]
 *
 * if (!hardsect_size[MAJOR])
 *		then 512 bytes is assumed.
 * else
 *		sector_size is hardsect_size[MAJOR][MINOR]
 * This is currently set by some scsi device and read by the msdos fs driver
 * This might be a some uses later.
 */
int *hardsect_size[MAX_BLKDEV] = { NULL, NULL, };

/* This specifies how many sectors to read ahead on the disk.
   This is unused in Mach.  It is here to make drivers compile.  */
int read_ahead[MAX_BLKDEV] = {0, };

/* Use to wait on when there are no free requests.
   This is unused in Mach.  It is here to make drivers compile.  */
struct wait_queue *wait_for_request = NULL;

/* Initialize block drivers.  */
void
blk_dev_init ()
{
#ifdef CONFIG_BLK_DEV_IDE
  ide_init ();
#endif
#ifdef CONFIG_BLK_DEV_FD
  floppy_init ();
#endif
}

/* Return 1 if major number MAJOR corresponds to a disk device.  */
static inline int
disk_major (int major)
{
  return (major == IDE0_MAJOR
	  || major == IDE1_MAJOR
	  || major == IDE2_MAJOR
	  || major == IDE3_MAJOR
	  || major == SCSI_DISK_MAJOR);
}

/* Linux kernel block support routines.  */

/* Register a driver for major number MAJOR,
   with name NAME, and operations vector FOPS.  */
int
register_blkdev (unsigned major, const char *name,
		 struct file_operations *fops)
{
  int err = 0;

  if (major == 0)
    {
      for (major = MAX_BLKDEV - 1; major > 0; major--)
	if (blkdevs[major].fops == NULL)
	  goto out;
      return -LINUX_EBUSY;
    }
  if (major >= MAX_BLKDEV)
    return -LINUX_EINVAL;
  if (blkdevs[major].fops && blkdevs[major].fops != fops)
    return -LINUX_EBUSY;

out:
  blkdevs[major].name = name;
  blkdevs[major].fops = fops;
  blkdevs[major].busy = 0;
  blkdevs[major].want = 0;
  blkdevs[major].gd = NULL;
  blkdevs[major].default_slice = NULL;
  blkdevs[major].label = NULL;
  return 0;
}

/* Unregister the driver associated with
   major number MAJOR and having the name NAME.  */
int
unregister_blkdev (unsigned major, const char *name)
{
  int err;

  if (major >= MAX_BLKDEV)
    return -LINUX_EINVAL;
  if (! blkdevs[major].fops || strcmp (blkdevs[major].name, name))
    return -LINUX_EINVAL;
  blkdevs[major].fops = NULL;
  if (blkdevs[major].default_slice)
    {
      assert (blkdevs[major].gd);
      kfree ((vm_offset_t) blkdevs[major].default_slice,
	     sizeof (int) * blkdevs[major].gd->max_nr);
    }
  if (blkdevs[major].label)
    {
      assert (blkdevs[major].gd);
      kfree ((vm_offset_t) blkdevs[major].label,
	     (sizeof (struct disklabel *)
	      * blkdevs[major].gd->max_p * blkdevs[major].gd->max_nr));
    }
  return 0;
}

/* One of these is associated with
   each page allocated by the buffer management routines.  */
struct pagehdr
{
  unsigned char busy;		/* page header is in use */
  unsigned char avail;		/* number of blocks available in page */
  unsigned short bitmap;	/* free space bitmap */
  void *blks;			/* the actual page */
  struct pagehdr *next;		/* next header in list */
};

/* This structure describes the different block sizes.  */
struct bufsize
{
  unsigned short size;		/* size of block */
  unsigned short avail;		/* # available blocks */
  struct pagehdr *pages;	/* page list */
};

/* List of supported block sizes.  */
static struct bufsize bufsizes[] =
{
  {  512, 0, NULL },
  { 1024, 0, NULL },
  { 2048, 0, NULL },
  { 4096, 0, NULL },
};

/* Page headers.  */
static struct pagehdr pagehdrs[50];	/* XXX: needs to be dynamic */

/* Find the block size that is greater than or equal to SIZE.  */
static struct bufsize *
get_bufsize (int size)
{
  struct bufsize *bs, *ebs;

  bs = &bufsizes[0];
  ebs = &bufsizes[sizeof (bufsizes) / sizeof (bufsizes[0])];
  while (bs < ebs)
    {
      if (bs->size >= size)
	return bs;
      bs++;
    }

  panic ("%s:%d: alloc_buffer: bad buffer size %d", __FILE__, __LINE__, size);
}

/* Free all pages that are not in use.
   Called by __get_free_pages when pages are running low.  */
void
collect_buffer_pages ()
{
  struct bufsize *bs, *ebs;
  struct pagehdr *ph, **prev_ph;

  bs = &bufsizes[0];
  ebs = &bufsizes[sizeof (bufsizes) / sizeof (bufsizes[0])];
  while (bs < ebs)
    {
      if (bs->avail >= PAGE_SIZE / bs->size)
	{
	  ph = bs->pages;
	  prev_ph = &bs->pages;
	  while (ph)
	    if (ph->avail == PAGE_SIZE / bs->size)
	      {
		bs->avail -= ph->avail;
		ph->busy = 0;
		*prev_ph = ph->next;
		free_pages ((unsigned long) ph->blks, 0);
		ph = *prev_ph;
	      }
	    else
	      {
		prev_ph = &ph->next;
		ph = ph->next;
	      }
	}
      bs++;
    }
}

/* Allocate a buffer of at least SIZE bytes.  */
static void *
alloc_buffer (int size)
{
  int i;
  unsigned flags;
  struct bufsize *bs;
  struct pagehdr *ph, *eph;

  bs = get_bufsize (size);
  save_flags (flags);
  cli ();
  if (bs->avail == 0)
    {
      ph = &pagehdrs[0];
      eph = &pagehdrs[sizeof (pagehdrs) / sizeof (pagehdrs[0])];
      while (ph < eph && ph->busy)
	ph++;
      if (ph == eph)
	{
	  restore_flags (flags);
	  printf ("%s:%d: alloc_buffer: ran out of page headers\n",
		  __FILE__, __LINE__);
	  return NULL;
	}
      ph->blks = (void *) __get_free_pages (GFP_KERNEL, 0, ~0UL);
      if (! ph->blks)
	{
	  restore_flags (flags);
	  return NULL;
	}
      ph->busy = 1;
      ph->avail = PAGE_SIZE / bs->size;
      ph->bitmap = 0;
      ph->next = bs->pages;
      bs->pages = ph;
      bs->avail += ph->avail;
    }
  for (ph = bs->pages; ph; ph = ph->next)
    if (ph->avail)
      for (i = 0; i < PAGE_SIZE / bs->size; i++)
	if ((ph->bitmap & (1 << i)) == 0)
	  {
	    bs->avail--;
	    ph->avail--;
	    ph->bitmap |= 1 << i;
	    restore_flags (flags);
	    return ph->blks + i * bs->size;
	  }

  panic ("%s:%d: alloc_buffer: list destroyed", __FILE__, __LINE__);
}

/* Free buffer P of SIZE bytes previously allocated by alloc_buffer.  */
static void
free_buffer (void *p, int size)
{
  int i;
  unsigned flags;
  struct bufsize *bs;
  struct pagehdr *ph;

  bs = get_bufsize (size);
  save_flags (flags);
  cli ();
  for (ph = bs->pages; ph; ph = ph->next)
    if (p >= ph->blks && p < ph->blks + PAGE_SIZE)
      break;
  assert (ph);
  i = (int) (p - ph->blks) / bs->size;
  assert (ph->bitmap & (1 << i));
  ph->bitmap &= ~(1 << i);
  ph->avail++;
  bs->avail++;
  restore_flags (flags);
}

/* Allocate a buffer of SIZE bytes and
   associate it with block number BLOCK of device DEV.  */
struct buffer_head *
getblk (kdev_t dev, int block, int size)
{
  struct buffer_head *bh;

  assert (size <= PAGE_SIZE);

  bh = linux_kmalloc (sizeof (struct buffer_head), GFP_KERNEL);
  if (! bh)
    return NULL;
  bh->b_data = alloc_buffer (size);
  if (! bh->b_data)
    {
      linux_kfree (bh);
      return NULL;
    }
  bh->b_dev = dev;
  bh->b_size = size;
  bh->b_state = 1 << BH_Lock;
  bh->b_blocknr = block;
  bh->b_page_list = NULL;
  bh->b_request = NULL;
  bh->b_reqnext = NULL;
  bh->b_wait = NULL;
  bh->b_sem = NULL;
  return bh;
}

/* Release buffer BH previously allocated by getblk.  */
void
__brelse (struct buffer_head *bh)
{
  if (bh->b_request)
    linux_kfree (bh->b_request);
  free_buffer (bh->b_data, bh->b_size);
  linux_kfree (bh);
}

/* Check for I/O errors upon completion of I/O operation RW
   on the buffer list BH.  The number of buffers is NBUF.
   Copy any data from bounce buffers and free them.  */
static int
check_for_error (int rw, int nbuf, struct buffer_head **bh)
{
  int err;
  struct request *req;

  req = bh[0]->b_request;
  if (! req)
    {
      while (--nbuf >= 0)
	if (bh[nbuf]->b_page_list)
	  {
	    bh[nbuf]->b_page_list = NULL;
	    free_buffer (bh[nbuf]->b_data, bh[nbuf]->b_size);
	  }
      return -LINUX_ENOMEM;
    }

  bh[0]->b_request = NULL;
  err = 0;

  while (--nbuf >= 0)
    {
      struct buffer_head *bhp = bh[nbuf];

      if (bhp->b_page_list)
	{
	  if (rw == READ && buffer_uptodate (bhp))
	    {
	      int amt;
	      vm_page_t *pages = bhp->b_page_list;

	      amt = PAGE_SIZE - bhp->b_off;
	      if (amt > bhp->b_usrcnt)
		amt = bhp->b_usrcnt;
	      memcpy ((void *) pages[bhp->b_index]->phys_addr + bhp->b_off,
		      bhp->b_data, amt);
	      if (amt < bhp->b_usrcnt)
		memcpy ((void *) pages[bhp->b_index + 1]->phys_addr,
			bhp->b_data + amt, bhp->b_usrcnt - amt);
	    }
	  bhp->b_page_list = NULL;
	  free_buffer (bhp->b_data, bhp->b_size);
	}
      if (! buffer_uptodate (bhp))
	err = -LINUX_EIO;
    }

  linux_kfree (req);
  return err;
}

/* Allocate a buffer of SIZE bytes and fill it with data
   from device DEV starting at block number BLOCK.  */
struct buffer_head *
bread (kdev_t dev, int block, int size)
{
  int err;
  struct buffer_head *bh;

  bh = getblk (dev, block, size);
  if (! bh)
    return NULL;
  ll_rw_block (READ, 1, &bh);
  wait_on_buffer (bh);
  err = check_for_error (READ, 1, &bh);
  if (err)
    {
      __brelse (bh);
      return NULL;
    }
  return bh;
}

/* Return the block size for device DEV in *BSIZE and
   log2(block size) in *BSHIFT.  */
static inline void
get_block_size (kdev_t dev, int *bsize, int *bshift)
{
  int i, size, shift;

  size = BLOCK_SIZE;
  if (blksize_size[MAJOR (dev)]
      && blksize_size[MAJOR (dev)][MINOR (dev)])
    size = blksize_size[MAJOR (dev)][MINOR (dev)];
  for (i = size, shift = 0; i != 1; shift++, i >>= 1)
    ;
  *bsize = size;
  *bshift = shift;
}

/* Enqueue request REQ on a driver's queue.  */
static inline void
enqueue_request (struct request *req)
{
  struct request *tmp;
  struct blk_dev_struct *dev;

  dev = blk_dev + MAJOR (req->rq_dev);
  cli ();
  tmp = dev->current_request;
  if (! tmp)
    {
      dev->current_request = req;
      (*dev->request_fn) ();
      sti ();
      return;
    }
  while (tmp->next)
    {
      if ((IN_ORDER (tmp, req) || ! IN_ORDER (tmp, tmp->next))
	  && IN_ORDER (req, tmp->next))
	break;
      tmp = tmp->next;
    }
  req->next = tmp->next;
  tmp->next = req;
  if (scsi_major (MAJOR (req->rq_dev)))
    (*dev->request_fn) ();
  sti ();
}

/* Perform the I/O operation RW on the buffer list BH
   containing NR buffers.  */
void
ll_rw_block (int rw, int nr, struct buffer_head **bh)
{
  int i, bsize, bshift;
  unsigned major;
  struct request *r;

  r = (struct request *) linux_kmalloc (sizeof (struct request), GFP_KERNEL);
  if (! r)
    {
      bh[0]->b_request = NULL;
      return;
    }
  bh[0]->b_request = r;

  major = MAJOR (bh[0]->b_dev);
  assert (major < MAX_BLKDEV);

  get_block_size (bh[0]->b_dev, &bsize, &bshift);
  assert (bsize <= PAGE_SIZE);

  for (i = 0, r->nr_sectors = 0; i < nr - 1; i++)
    {
      r->nr_sectors += bh[i]->b_size >> 9;
      bh[i]->b_reqnext = bh[i + 1];
    }
  r->nr_sectors += bh[i]->b_size >> 9;
  bh[i]->b_reqnext = NULL;

  r->rq_status = RQ_ACTIVE;
  r->rq_dev = bh[0]->b_dev;
  r->cmd = rw;
  r->errors = 0;
  r->sector = bh[0]->b_blocknr << (bshift - 9);
  r->current_nr_sectors = bh[0]->b_size >> 9;
  r->buffer = bh[0]->b_data;
  r->sem = bh[0]->b_sem;
  r->bh = bh[0];
  r->bhtail = bh[nr - 1];
  r->next = NULL;

  enqueue_request (r);
}

/* Maximum amount of data to write per invocation of the driver.  */
#define WRITE_MAXPHYS	(VM_MAP_COPY_PAGE_LIST_MAX << PAGE_SHIFT)
#define WRITE_MAXPHYSPG	(WRITE_MAXPHYS >> PAGE_SHIFT)

int linux_block_write_trace = 0;

/* Write COUNT bytes of data from user buffer BUF
   to device specified by INODE at location specified by FILP.  */
int
block_write (struct inode *inode, struct file *filp,
	     const char *buf, int count)
{
  char *p;
  int i, bsize, bmask, bshift;
  int err = 0, have_page_list = 1;
  int resid = count, unaligned;
  int page_index, pages, amt, cnt, nbuf;
  unsigned blk;
  vm_map_copy_t copy;
  struct request req;
  struct semaphore sem;
  struct name_map *np = filp->f_np;
  struct buffer_head *bh, *bhp, **bhlist;

  /* Compute device block size.  */
  get_block_size (inode->i_rdev, &bsize, &bshift);
  assert (bsize <= PAGE_SIZE);
  bmask = bsize - 1;

  copy = (vm_map_copy_t) buf;
  assert (copy);
  assert (copy->type == VM_MAP_COPY_PAGE_LIST);

  p = (char *) copy->offset;
  pages = copy->cpy_npages;
  blk = (filp->f_pos + bmask) >> bshift;

  if (linux_block_write_trace)
    printf ("block_write: at %d: f_pos 0x%x, count %d, blk 0x%x, p 0x%x\n",
	    __LINE__, (unsigned) filp->f_pos, count, blk, p);

  /* Allocate buffer headers.  */
  nbuf = ((round_page ((vm_offset_t) p + resid) - trunc_page ((vm_offset_t) p))
	  >> PAGE_SHIFT);
  if (nbuf > WRITE_MAXPHYSPG)
    nbuf = WRITE_MAXPHYSPG;
  if ((filp->f_pos & bmask) || ((int) p & PAGE_MASK))
    nbuf *= 2;
  bh = (struct buffer_head *) kalloc ((sizeof (*bh) + sizeof (*bhlist))
				      * nbuf);
  if (! bh)
    {
      err = -LINUX_ENOMEM;
      goto out;
    }
  bhlist = (struct buffer_head **) (bh + nbuf);

  /* Write any partial block.  */
  if (filp->f_pos & bmask)
    {
      char *b, *q;
      int use_req;

      use_req = (disk_major (MAJOR (inode->i_rdev)) && ! np->read_only);

      amt = bsize - (filp->f_pos & bmask);
      if (amt > resid)
	amt = resid;

      if (linux_block_write_trace)
	printf ("block_write: at %d: amt %d, resid %d\n",
		__LINE__, amt, resid);

      if (use_req)
	{
	  i = (amt + 511) & ~511;
	  req.buffer = b = alloc_buffer (i);
	  if (! b)
	    {
	      printf ("%s:%d: block_write: ran out of buffers\n",
		      __FILE__, __LINE__);
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  req.sector = filp->f_pos >> 9;
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = READ;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    {
	      free_buffer (b, i);
	      err = -LINUX_EIO;
	      goto out;
	    }
	  q = b + (filp->f_pos & 511);
	}
      else
	{
	  i = bsize;
	  bhp = bh;
	  bhp->b_data = b = alloc_buffer (i);
	  if (! b)
	    {
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  bhp->b_blocknr = filp->f_pos >> bshift;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_size = bsize;
	  bhp->b_state = 1 << BH_Lock;
	  bhp->b_page_list = NULL;
	  bhp->b_request = NULL;
	  bhp->b_reqnext = NULL;
	  bhp->b_wait = NULL;
	  bhp->b_sem = NULL;

	  ll_rw_block (READ, 1, &bhp);
	  wait_on_buffer (bhp);
	  err = check_for_error (READ, 1, &bhp);
	  if (err)
	    {
	      free_buffer (b, i);
	      goto out;
	    }
	  q = b + (filp->f_pos & bmask);
	}

      cnt = PAGE_SIZE - ((int) p & PAGE_MASK);
      if (cnt > amt)
	cnt = amt;
      memcpy (q, ((void *) copy->cpy_page_list[0]->phys_addr
		  + ((int) p & PAGE_MASK)),
	      cnt);
      if (cnt < amt)
	{
	  assert (copy->cpy_npages >= 2);
	  memcpy (q + cnt,
		  (void *) copy->cpy_page_list[1]->phys_addr, amt - cnt);
	}
      else
	assert (copy->cpy_npages >= 1);

      if (use_req)
	{
	  req.buffer = b;
	  req.sector = filp->f_pos >> 9;
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = WRITE;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    err = -LINUX_EIO;
	}
      else
	{
	  bhp->b_state = (1 << BH_Dirty) | (1 << BH_Lock);
	  ll_rw_block (WRITE, 1, &bhp);
	  err = check_for_error (WRITE, 1, &bhp);
	}
      free_buffer (b, i);
      if (err)
	{
	  if (linux_block_write_trace)
	    printf ("block_write: at %d\n", __LINE__);

	  goto out;
	}
      resid -= amt;
      if (resid == 0)
	goto out;
      p += amt;
    }

  unaligned = (int) p & 511;

  /* Write full blocks.  */
  while (resid > bsize)
    {
      assert (have_page_list == 1);

      /* Construct buffer list.  */
      for (i = 0, bhp = bh; resid > bsize && i < nbuf; i++, bhp++)
	{
	  page_index = ((trunc_page ((vm_offset_t) p)
			 - trunc_page (copy->offset))
			>> PAGE_SHIFT);

	  if (page_index == pages)
	    break;

	  bhlist[i] = bhp;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_state = (1 << BH_Dirty) | (1 << BH_Lock);
	  bhp->b_blocknr = blk;
	  bhp->b_wait = NULL;
	  bhp->b_page_list = NULL;
	  bhp->b_sem = &sem;

	  cnt = PAGE_SIZE - ((int) p & PAGE_MASK);
	  if (! unaligned && cnt >= bsize)
	    {
	      if (cnt > resid)
		cnt = resid;
	      bhp->b_size = cnt & ~bmask;
	      bhp->b_data = (((char *)
			      copy->cpy_page_list[page_index]->phys_addr)
			     + ((int) p & PAGE_MASK));
	    }
	  else
	    {
	      if (cnt < bsize)
		{
		  if (page_index == pages - 1)
		    break;
		  bhp->b_size = bsize;
		}
	      else
		{
		  bhp->b_size = cnt;
		  if (bhp->b_size > resid)
		    bhp->b_size = resid;
		  bhp->b_size &= ~bmask;
		}
	      bhp->b_data = alloc_buffer (bhp->b_size);
	      if (! bhp->b_data)
		{
		  printf ("%s:%d: block_write: ran out of buffers\n",
			  __FILE__, __LINE__);
		  while (--i >= 0)
		    if (bhlist[i]->b_page_list)
		      free_buffer (bhlist[i]->b_data, bhlist[i]->b_size);
		  err = -LINUX_ENOMEM;
		  goto out;
		}
	      bhp->b_page_list = (void *) 1;
	      if (cnt > bhp->b_size)
		cnt = bhp->b_size;
	      memcpy (bhp->b_data,
		      ((void *) copy->cpy_page_list[page_index]->phys_addr
		       + ((int) p & PAGE_MASK)),
		      cnt);
	      if (cnt < bhp->b_size)
		memcpy (bhp->b_data + cnt,
			((void *)
			 copy->cpy_page_list[page_index + 1]->phys_addr),
			bhp->b_size - cnt);
	    }

	  p += bhp->b_size;
	  resid -= bhp->b_size;
	  blk += bhp->b_size >> bshift;
	}

      assert (i > 0);

      sem.count = 0;
      sem.wait = NULL;

      /* Do the write.  */
      ll_rw_block (WRITE, i, bhlist);
      __down (&sem);
      err = check_for_error (WRITE, i, bhlist);
      if (err || resid == 0)
	goto out;

      /* Discard current page list.  */
      vm_map_copy_discard (copy);
      have_page_list = 0;

      /* Compute # pages to wire down.  */
      pages = ((round_page ((vm_offset_t) p + resid)
		- trunc_page ((vm_offset_t) p))
	       >> PAGE_SHIFT);
      if (pages > WRITE_MAXPHYSPG)
	pages = WRITE_MAXPHYSPG;

      /* Wire down user pages and get page list.  */
      err = vm_map_copyin_page_list (current_map (),
				     trunc_page ((vm_offset_t) p),
				     pages << PAGE_SHIFT, FALSE,
				     FALSE, &copy, FALSE);
      if (err)
	{
	  if (err == KERN_INVALID_ADDRESS || err == KERN_PROTECTION_FAILURE)
	    err = -LINUX_EINVAL;
	  else
	    err = -LINUX_ENOMEM;
	  goto out;
	}

      assert (pages == copy->cpy_npages);
      assert (! vm_map_copy_has_cont (copy));

      have_page_list = 1;
    }

  /* Write any partial count.  */
  if (resid > 0)
    {
      char *b;
      int use_req;

      assert (have_page_list);
      assert (pages >= 1);

      use_req = (disk_major (MAJOR (inode->i_rdev)) && ! np->read_only);

      if (linux_block_write_trace)
	printf ("block_write: at %d: resid %d\n", __LINE__, resid);

      if (use_req)
	{
	  i = (resid + 511) & ~511;
	  req.buffer = b = alloc_buffer (i);
	  if (! b)
	    {
	      printf ("%s:%d: block_write: ran out of buffers\n",
		      __FILE__, __LINE__);
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  req.sector = blk << (bshift - 9);
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = READ;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    {
	      free_buffer (b, i);
	      err = -LINUX_EIO;
	      goto out;
	    }
	}
      else
	{
	  i = bsize;
	  bhp = bh;
	  bhp->b_data = b = alloc_buffer (i);
	  if (! b)
	    {
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  bhp->b_blocknr = blk;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_size = bsize;
	  bhp->b_state = 1 << BH_Lock;
	  bhp->b_page_list = NULL;
	  bhp->b_request = NULL;
	  bhp->b_reqnext = NULL;
	  bhp->b_wait = NULL;
	  bhp->b_sem = NULL;

	  ll_rw_block (READ, 1, &bhp);
	  wait_on_buffer (bhp);
	  err = check_for_error (READ, 1, &bhp);
	  if (err)
	    {
	      free_buffer (b, i);
	      goto out;
	    }
	}

      page_index = ((trunc_page ((vm_offset_t) p) - trunc_page (copy->offset))
		    >> PAGE_SHIFT);
      cnt = PAGE_SIZE - ((int) p & PAGE_MASK);
      if (cnt > resid)
	cnt = resid;
      memcpy (b, ((void *) copy->cpy_page_list[page_index]->phys_addr
		  + ((int) p & PAGE_MASK)),
	      cnt);
      if (cnt < resid)
	{
	  assert (copy->cpy_npages >= 2);
	  memcpy (b + cnt,
		  (void *) copy->cpy_page_list[page_index + 1]->phys_addr,
		  resid - cnt);
	}
      else
	assert (copy->cpy_npages >= 1);

      if (use_req)
	{
	  req.buffer = b;
	  req.sector = blk << (bshift - 9);
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = WRITE;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    err = -LINUX_EIO;
	}
      else
	{
	  bhp->b_state = (1 << BH_Dirty) | (1 << BH_Lock);
	  ll_rw_block (WRITE, 1, &bhp);
	  err = check_for_error (WRITE, 1, &bhp);
	}
      free_buffer (b, i);
      if (! err)
	resid = 0;
    }

out:
  if (have_page_list)
    vm_map_copy_discard (copy);
  if (bh)
    kfree ((vm_offset_t) bh,
	   (sizeof (*bh) + sizeof (*bhlist)) * nbuf);
  filp->f_resid = resid;
  return err;
}

int linux_block_read_trace = 0;
#define LINUX_BLOCK_READ_TRACE (linux_block_read_trace == -1 \
				|| linux_block_read_trace == inode->i_rdev)

/* Maximum amount of data to read per driver invocation.  */
#define READ_MAXPHYS	(64*1024)
#define READ_MAXPHYSPG	(READ_MAXPHYS >> PAGE_SHIFT)

/* Read COUNT bytes of data into user buffer BUF
   from device specified by INODE from location specified by FILP.  */
int
block_read (struct inode *inode, struct file *filp, char *buf, int count)
{
  int err = 0, resid = count;
  int i, bsize, bmask, bshift;
  int pages, amt, unaligned;
  int page_index, nbuf;
  int have_page_list = 0;
  unsigned blk;
  vm_offset_t off, wire_offset, offset;
  vm_object_t object;
  vm_page_t *page_list;
  struct request req;
  struct semaphore sem;
  struct name_map *np = filp->f_np;
  struct buffer_head *bh, *bhp, **bhlist;

  /* Get device block size.  */
  get_block_size (inode->i_rdev, &bsize, &bshift);
  assert (bsize <= PAGE_SIZE);
  bmask = bsize - 1;

  off = 0;
  blk = (filp->f_pos + bmask) >> bshift;

  /* Allocate buffer headers.  */
  nbuf = round_page (count) >> PAGE_SHIFT;
  if (nbuf > READ_MAXPHYSPG)
    nbuf = READ_MAXPHYSPG;
  if (filp->f_pos & bmask)
    nbuf *= 2;
  bh = (struct buffer_head *) kalloc ((sizeof (*bh) + sizeof (*bhlist)) * nbuf
				      + sizeof (*page_list) * READ_MAXPHYSPG);
  if (! bh)
    return -LINUX_ENOMEM;
  bhlist = (struct buffer_head **) (bh + nbuf);
  page_list = (vm_page_t *) (bhlist + nbuf);

  /* Allocate an object to hold the data.  */
  object = vm_object_allocate (round_page (count));
  if (! object)
    {
      err = -LINUX_ENOMEM;
      goto out;
    }

  /* Compute number of pages to be wired at a time.  */
  pages = round_page (count) >> PAGE_SHIFT;
  if (pages > READ_MAXPHYSPG)
    pages = READ_MAXPHYSPG;

  /* Allocate and wire down pages in the object.  */
  for (i = 0, wire_offset = offset = 0; i < pages; i++, offset += PAGE_SIZE)
    {
      while (1)
	{
	  page_list[i] = vm_page_grab ();
	  if (page_list[i])
	    {
	      assert (page_list[i]->busy);
	      assert (! page_list[i]->wanted);
	      break;
	    }
	  vm_page_wait (NULL);
	}
      vm_object_lock (object);
      vm_page_lock_queues ();
      assert (! vm_page_lookup (object, offset));
      vm_page_insert (page_list[i], object, offset);
      assert (page_list[i]->wire_count == 0);
      vm_page_wire (page_list[i]);
      vm_page_unlock_queues ();
      vm_object_unlock (object);
    }
  have_page_list = 1;

  /* Read any partial block.  */
  if (filp->f_pos & bmask)
    {
      char *b, *q;
      int use_req;

      use_req = (disk_major (MAJOR (inode->i_rdev)) && ! np->read_only);

      amt = bsize - (filp->f_pos & bmask);
      if (amt > resid)
	amt = resid;

      if (LINUX_BLOCK_READ_TRACE)
	printf ("block_read: at %d: amt %d, resid %d\n",
		__LINE__, amt, resid);

      if (use_req)
	{
	  i = (amt + 511) & ~511;
	  req.buffer = b = alloc_buffer (i);
	  if (! b)
	    {
	      printf ("%s:%d: block_read: ran out of buffers\n",
		      __FILE__, __LINE__);
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  req.sector = filp->f_pos >> 9;
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = READ;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    {
	      free_buffer (b, i);
	      err = -LINUX_EIO;
	      goto out;
	    }
	  q = b + (filp->f_pos & 511);
	}
      else
	{
	  i = bsize;
	  bhp = bh;
	  bhp->b_data = b = alloc_buffer (i);
	  if (! b)
	    {
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  bhp->b_blocknr = filp->f_pos >> bshift;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_size = bsize;
	  bhp->b_state = 1 << BH_Lock;
	  bhp->b_page_list = NULL;
	  bhp->b_request = NULL;
	  bhp->b_reqnext = NULL;
	  bhp->b_wait = NULL;
	  bhp->b_sem = NULL;

	  ll_rw_block (READ, 1, &bhp);
	  wait_on_buffer (bhp);
	  err = check_for_error (READ, 1, &bhp);
	  if (err)
	    {
	      free_buffer (b, i);
	      goto out;
	    }
	  q = b + (filp->f_pos & bmask);
	}

      memcpy ((void *) page_list[0]->phys_addr, q, amt);

      free_buffer (b, i);
      resid -= amt;
      if (resid == 0)
	{
	  if (LINUX_BLOCK_READ_TRACE)
	    printf ("block_read: at %d\n", __LINE__);

	  assert (pages == 1);
	  goto out;
	}
      off += amt;
    }

  unaligned = off & 511;

  /* Read full blocks.  */
  while (resid > bsize)
    {
      /* Construct buffer list to hand to the driver.  */
      for (i = 0, bhp = bh; resid > bsize && i < nbuf; bhp++, i++)
	{
	  if (off == wire_offset + (pages << PAGE_SHIFT))
	    break;

	  bhlist[i] = bhp;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_state = 1 << BH_Lock;
	  bhp->b_blocknr = blk;
	  bhp->b_wait = NULL;
	  bhp->b_sem = &sem;

	  page_index = (trunc_page (off) - wire_offset) >> PAGE_SHIFT;
	  amt = PAGE_SIZE - (off & PAGE_MASK);
	  if (! unaligned && amt >= bsize)
	    {
	      if (amt > resid)
		amt = resid;
	      bhp->b_size = amt & ~bmask;
	      bhp->b_data = ((char *) page_list[page_index]->phys_addr
			     + (off & PAGE_MASK));
	      bhp->b_page_list = NULL;
	    }
	  else
	    {
	      if (amt < bsize)
		{
		  if (page_index == pages - 1)
		    {
		      assert (round_page (count) - off >= resid);
		      break;
		    }
		  bhp->b_size = bsize;
		}
	      else
		{
		  if (amt > resid)
		    amt = resid;
		  bhp->b_size = amt & ~bmask;
		}
	      bhp->b_data = alloc_buffer (bhp->b_size);
	      if (! bhp->b_data)
		{
		  printf ("%s:%d: block_read: ran out of buffers\n",
			  __FILE__, __LINE__);

		  while (--i >= 0)
		    if (bhp->b_page_list)
		      free_buffer (bhp->b_data, bhp->b_size);
		  err = -LINUX_ENOMEM;
		  goto out;
		}
	      bhp->b_page_list = page_list;
	      bhp->b_index = page_index;
	      bhp->b_off = off & PAGE_MASK;
	      bhp->b_usrcnt = bhp->b_size;
	    }

	  resid -= bhp->b_size;
	  off += bhp->b_size;
	  blk += bhp->b_size >> bshift;
	}

      assert (i > 0);

      sem.count = 0;
      sem.wait = NULL;

      /* Do the read.  */
      ll_rw_block (READ, i, bhlist);
      __down (&sem);
      err = check_for_error (READ, i, bhlist);
      if (err || resid == 0)
	goto out;

      /* Unwire the pages and mark them dirty.  */
      offset = trunc_page (off);
      for (i = 0; wire_offset < offset; i++, wire_offset += PAGE_SIZE)
	{
	  vm_object_lock (object);
	  vm_page_lock_queues ();
	  assert (vm_page_lookup (object, wire_offset) == page_list[i]);
	  assert (page_list[i]->wire_count == 1);
	  assert (! page_list[i]->active && ! page_list[i]->inactive);
	  assert (! page_list[i]->reference);
	  page_list[i]->dirty = TRUE;
	  page_list[i]->reference = TRUE;
	  page_list[i]->busy = FALSE;
	  vm_page_unwire (page_list[i]);
	  vm_page_unlock_queues ();
	  vm_object_unlock (object);
	}

      assert (i <= pages);

      /* Wire down the next chunk of the object.  */
      if (i == pages)
	{
	  i = 0;
	  offset = wire_offset;
	  have_page_list = 0;
	}
      else
	{
	  int j;

	  for (j = 0; i < pages; page_list[j++] = page_list[i++])
	    offset += PAGE_SIZE;
	  i = j;
	}
      pages = (round_page (count) - wire_offset) >> PAGE_SHIFT;
      if (pages > READ_MAXPHYSPG)
	pages = READ_MAXPHYSPG;
      while (i < pages)
	{
	  while (1)
	    {
	      page_list[i] = vm_page_grab ();
	      if (page_list[i])
		{
		  assert (page_list[i]->busy);
		  assert (! page_list[i]->wanted);
		  break;
		}
	      vm_page_wait (NULL);
	    }
	  vm_object_lock (object);
	  vm_page_lock_queues ();
	  assert (! vm_page_lookup (object, offset));
	  vm_page_insert (page_list[i], object, offset);
	  assert (page_list[i]->wire_count == 0);
	  vm_page_wire (page_list[i]);
	  vm_page_unlock_queues ();
	  vm_object_unlock (object);
	  i++;
	  offset += PAGE_SIZE;
	}
      have_page_list = 1;
    }

  /* Read any partial count.  */
  if (resid > 0)
    {
      char *b;
      int use_req;

      assert (have_page_list);
      assert (pages >= 1);

      use_req = (disk_major (MAJOR (inode->i_rdev)) && ! np->read_only);

      amt = bsize - (filp->f_pos & bmask);
      if (amt > resid)
	amt = resid;

      if (LINUX_BLOCK_READ_TRACE)
	printf ("block_read: at %d: resid %d\n", __LINE__, amt, resid);

      if (use_req)
	{
	  i = (resid + 511) & ~511;
	  req.buffer = b = alloc_buffer (i);
	  if (! b)
	    {
	      printf ("%s:%d: block_read: ran out of buffers\n",
		      __FILE__, __LINE__);
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  req.sector = blk << (bshift - 9);
	  req.nr_sectors = i >> 9;
	  req.current_nr_sectors = i >> 9;
	  req.rq_status = RQ_ACTIVE;
	  req.rq_dev = inode->i_rdev;
	  req.cmd = READ;
	  req.errors = 0;
	  req.sem = &sem;
	  req.bh = NULL;
	  req.bhtail = NULL;
	  req.next = NULL;

	  sem.count = 0;
	  sem.wait = NULL;

	  enqueue_request (&req);
	  __down (&sem);

	  if (req.errors)
	    {
	      free_buffer (b, i);
	      err = -LINUX_EIO;
	      goto out;
	    }
	}
      else
	{
	  i = bsize;
	  bhp = bh;
	  bhp->b_data = b = alloc_buffer (i);
	  if (! b)
	    {
	      err = -LINUX_ENOMEM;
	      goto out;
	    }
	  bhp->b_blocknr = blk;
	  bhp->b_dev = inode->i_rdev;
	  bhp->b_size = bsize;
	  bhp->b_state = 1 << BH_Lock;
	  bhp->b_page_list = NULL;
	  bhp->b_request = NULL;
	  bhp->b_reqnext = NULL;
	  bhp->b_wait = NULL;
	  bhp->b_sem = NULL;

	  ll_rw_block (READ, 1, &bhp);
	  wait_on_buffer (bhp);
	  err = check_for_error (READ, 1, &bhp);
	  if (err)
	    {
	      free_buffer (b, i);
	      goto out;
	    }
	}

      page_index = (trunc_page (off) - wire_offset) >> PAGE_SHIFT;
      amt = PAGE_SIZE - (off & PAGE_MASK);
      if (amt > resid)
	amt = resid;
      memcpy (((void *) page_list[page_index]->phys_addr
	       + (off & PAGE_MASK)),
	      b, amt);
      if (amt < resid)
	{
	  assert (pages >= 2);
	  memcpy ((void *) page_list[page_index + 1]->phys_addr,
		  b + amt, resid - amt);
	}
      else
	assert (pages >= 1);

      free_buffer (b, i);
    }

out:
  if (have_page_list)
    {
      for (i = 0; i < pages; i++, wire_offset += PAGE_SIZE)
	{
	  vm_object_lock (object);
	  vm_page_lock_queues ();
	  assert (vm_page_lookup (object, wire_offset) == page_list[i]);
	  assert (page_list[i]->wire_count == 1);
	  assert (! page_list[i]->active && ! page_list[i]->inactive);
	  assert (! page_list[i]->reference);
	  page_list[i]->dirty = TRUE;
	  page_list[i]->reference = TRUE;
	  page_list[i]->busy = FALSE;
	  vm_page_unwire (page_list[i]);
	  vm_page_unlock_queues ();
	  vm_object_unlock (object);
	}
    }      
  kfree ((vm_offset_t) bh,
	 ((sizeof (*bh) + sizeof (*bhlist)) * nbuf
	  + sizeof (*page_list) * READ_MAXPHYSPG));
  if (err)
    {
      if (object)
	{
	  assert (object->ref_count == 1);
	  vm_object_deallocate (object);
	}
    }
  else
    {
      assert (object);
      assert (object->ref_count == 1);

      filp->f_resid = 0;
      filp->f_object = object;
    }

  if (LINUX_BLOCK_READ_TRACE)
    printf ("block_read: at %d: err %d\n", __LINE__, err);

  return err;
}

/*
 * This routine checks whether a removable media has been changed,
 * and invalidates all buffer-cache-entries in that case. This
 * is a relatively slow routine, so we have to try to minimize using
 * it. Thus it is called only upon a 'mount' or 'open'. This
 * is the best way of combining speed and utility, I think.
 * People changing diskettes in the middle of an operation deserve
 * to loose :-)
 */
int
check_disk_change (kdev_t dev)
{
  unsigned i;
  struct file_operations * fops;

  i = MAJOR(dev);
  if (i >= MAX_BLKDEV || (fops = blkdevs[i].fops) == NULL)
    return 0;
  if (fops->check_media_change == NULL)
    return 0;
  if (! (*fops->check_media_change) (dev))
    return 0;

  printf ("Disk change detected on device %s\n", kdevname(dev));

  if (fops->revalidate)
    (*fops->revalidate) (dev);

  return 1;
}

/* Mach device interface routines.  */

/* Mach name to Linux major/minor number mapping table.  */
static struct name_map name_to_major[] =
{
  /* IDE disks */
  { "hd0", IDE0_MAJOR, 0, 0 },
  { "hd1", IDE0_MAJOR, 1, 0 },
  { "hd2", IDE1_MAJOR, 0, 0 },
  { "hd3", IDE1_MAJOR, 1, 0 },
  { "hd4", IDE2_MAJOR, 0, 0 },
  { "hd5", IDE2_MAJOR, 1, 0 },
  { "hd6", IDE3_MAJOR, 0, 0 },
  { "hd7", IDE3_MAJOR, 1, 0 },

  /* IDE CDROMs */
  { "wcd0", IDE0_MAJOR, 0, 1 },
  { "wcd1", IDE0_MAJOR, 1, 1 },
  { "wcd2", IDE1_MAJOR, 0, 1 },
  { "wcd3", IDE1_MAJOR, 1, 1 },
  { "wcd4", IDE2_MAJOR, 0, 1 },
  { "wcd5", IDE2_MAJOR, 1, 1 },
  { "wcd6", IDE3_MAJOR, 0, 1 },
  { "wcd7", IDE3_MAJOR, 1, 1 },

  /* SCSI disks */
  { "sd0", SCSI_DISK_MAJOR, 0, 0 },
  { "sd1", SCSI_DISK_MAJOR, 1, 0 },
  { "sd2", SCSI_DISK_MAJOR, 2, 0 },
  { "sd3", SCSI_DISK_MAJOR, 3, 0 },
  { "sd4", SCSI_DISK_MAJOR, 4, 0 },
  { "sd5", SCSI_DISK_MAJOR, 5, 0 },
  { "sd6", SCSI_DISK_MAJOR, 6, 0 },
  { "sd7", SCSI_DISK_MAJOR, 7, 0 },

  /* SCSI CDROMs */
  { "cd0", SCSI_CDROM_MAJOR, 0, 1 },
  { "cd1", SCSI_CDROM_MAJOR, 1, 1 },

  /* Floppy disks */
  { "fd0", FLOPPY_MAJOR, 0, 0 },
  { "fd1", FLOPPY_MAJOR, 1, 0 },
};

#define NUM_NAMES (sizeof (name_to_major) / sizeof (name_to_major[0]))

/* One of these is associated with each open instance of a device.  */
struct block_data
{
  const char *name;		/* Mach name for device */
  int want:1;			/* someone is waiting for I/O to complete */
  int open_count;		/* number of opens */
  int iocount;			/* number of pending I/O operations */
  int part;			/* BSD partition number (-1 if none) */
  ipc_port_t port;		/* port representing device */
  struct device_struct *ds;	/* driver operation table entry */
  struct device device;		/* generic device header */
  struct file file;		/* Linux file structure */
  struct inode inode;		/* Linux inode structure */
  struct name_map *np;		/* name to inode map */
  struct block_data *next;	/* forward link */
};

/* List of open devices.  */
static struct block_data *open_list;

/* Forward declarations.  */

extern struct device_emulation_ops linux_block_emulation_ops;

static io_return_t device_close (void *);

/* Return a send right for block device BD.  */
static ipc_port_t
dev_to_port (void *bd)
{
  return (bd
	  ? ipc_port_make_send (((struct block_data *) bd)->port)
	  : IP_NULL);
}

/* Return 1 if C is a letter of the alphabet.  */
static inline int
isalpha (int c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Return 1 if C is a digit.  */
static inline int
isdigit (int c)
{
  return c >= '0' && c <= '9';
}

int linux_device_open_trace = 0;

static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp)
{
  char *p;
  int i, part = -1, slice = 0, err = 0;
  unsigned major, minor;
  kdev_t dev;
  ipc_port_t notify;
  struct file file;
  struct inode inode;
  struct name_map *np;
  struct device_struct *ds;
  struct block_data *bd = NULL, *bdp;

  if (linux_device_open_trace)
    printf ("device_open: at %d: name %s\n", __LINE__, name);

  /* Parse name into name, unit, DOS partition (slice) and partition.  */
  for (p = name; isalpha (*p); p++)
    ;
  if (p == name || ! isdigit (*p))
    {
      if (linux_device_open_trace)
	printf ("device_open: at %d\n", __LINE__);

      return D_NO_SUCH_DEVICE;
    }
  do
    p++;
  while (isdigit (*p));
  if (*p)
    {
      char *q = p;

      if (! isalpha (*q))
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  return D_NO_SUCH_DEVICE;
	}
      if (*q == 's' && isdigit (*(q + 1)))
	{
	  q++;
	  slice = 0;
	  do
	    slice = slice * 10 + *q++ - '0';
	  while (isdigit (*q));
	  if (! *q)
	    goto find_major;
	  if (! isalpha (*q))
	    {
	      if (linux_device_open_trace)
		printf ("device_open: at %d\n", __LINE__);

	      return D_NO_SUCH_DEVICE;
	    }
	}
      if (*(q + 1))
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  return D_NO_SUCH_DEVICE;
	}
      part = *q - 'a';
    }
  else
    slice = -1;

find_major:
  /* Convert name to major number.  */
  for (i = 0, np = name_to_major; i < NUM_NAMES; i++, np++)
    {
      int len = strlen (np->name);

      if (len == p - name && ! strncmp (np->name, name, len))
	break;
    }
  if (i == NUM_NAMES)
    {
      if (linux_device_open_trace)
	printf ("device_open: at %d\n", __LINE__);

      return D_NO_SUCH_DEVICE;
    }

  major = np->major;
  ds = &blkdevs[major];

  /* Check that driver exists.  */
  if (! ds->fops)
    {
      if (linux_device_open_trace)
	printf ("device_open: at %d\n", __LINE__);

      return D_NO_SUCH_DEVICE;
    }

  /* Slice and partition numbers are only used by disk drives.
     The test for read-only is for IDE CDROMs.  */
  if (! disk_major (major) || np->read_only)
    {
      slice = -1;
      part = -1;
    }

  /* Wait for any other open/close calls to finish.  */
  ds = &blkdevs[major];
  while (ds->busy)
    {
      ds->want = 1;
      assert_wait ((event_t) ds, FALSE);
      thread_block (0);
    }
  ds->busy = 1;

  /* Compute minor number.  */
  if (disk_major (major) && ! ds->gd)
    {
      struct gendisk *gd;

      for (gd = gendisk_head; gd && gd->major != major; gd = gd->next)
	;
      assert (gd);
      ds->gd = gd;
    }
  minor = np->unit;
  if (ds->gd)
    minor <<= ds->gd->minor_shift;
  dev = MKDEV (major, minor);

  /* If no DOS partition is specified, find one we can handle.  */
  if (slice == 0 && (! ds->default_slice || ds->default_slice[np->unit] == 0))
    {
      int sysid, bsize, bshift;
      struct mboot *mp;
      struct ipart *pp;
      struct buffer_head *bhp;

      /* Open partition 0.  */
      inode.i_rdev = dev;
      file.f_mode = O_RDONLY;
      file.f_flags = 0;
      if (ds->fops->open)
	{
	  linux_intr_pri = SPL5;
	  err = (*ds->fops->open) (&inode, &file);
	  if (err)
	    {
	      if (linux_device_open_trace)
		printf ("device_open: at %d\n", __LINE__);

	      err = linux_to_mach_error (err);
	      goto out;
	    }
	}

      /* Allocate a buffer for I/O.  */
      get_block_size (inode.i_rdev, &bsize, &bshift);
      assert (bsize <= PAGE_SIZE);
      bhp = getblk (inode.i_rdev, 0, bsize);
      if (! bhp)
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  err = D_NO_MEMORY;
	  goto slice_done;
	}

      /* Read DOS partition table.  */
      ll_rw_block (READ, 1, &bhp);
      wait_on_buffer (bhp);
      err = check_for_error (READ, 1, &bhp);
      if (err)
	{
	  printf ("%s: error reading boot sector\n", np->name);
	  err = linux_to_mach_error (err);
	  goto slice_done;
	}

      /* Check for valid partition table.  */
      mp = (struct mboot *) bhp->b_data;
      if (mp->signature != BOOT_MAGIC)
	{
	  printf ("%s: invalid partition table\n", np->name);
	  err = D_NO_SUCH_DEVICE;
	  goto slice_done;
	}

      /* Search for a Mach, BSD or Linux partition.  */
      sysid = 0;
      pp = (struct ipart *) mp->parts;
      for (i = 0; i < FD_NUMPART; i++, pp++)
	{
	  if ((pp->systid == UNIXOS
	       || pp->systid == BSDOS
	       || pp->systid == LINUXOS)
	      && (! sysid || pp->bootid == ACTIVE))
	    {
	      sysid = pp->systid;
	      slice = i + 1;
	    }
	}
      if (! sysid)
	{
	  printf ("%s: No Mach, BSD or Linux partition found\n", np->name);
	  err = D_NO_SUCH_DEVICE;
	  goto slice_done;
	}

      printf ("%s: default slice %d: %s OS\n", np->name, slice,
	      (sysid == UNIXOS ? "Mach" : (sysid == BSDOS ? "BSD" : "LINUX")));

    slice_done:
      if (ds->fops->release)
	(*ds->fops->release) (&inode, &file);
      __brelse (bhp);
      if (err)
	goto out;
      if (! ds->default_slice)
	{
	  ds->default_slice = (int *) kalloc (sizeof (int) * ds->gd->max_nr);
	  if (! ds->default_slice)
	    {
	      if (linux_device_open_trace)
		printf ("device_open: at %d\n", __LINE__);

	      err = D_NO_MEMORY;
	      goto out;
	    }
	  memset (ds->default_slice, 0, sizeof (int) * ds->gd->max_nr);
	}
      ds->default_slice[np->unit] = slice;
    }

  /* Add slice to minor number.  */
  if (slice == 0)
    slice = ds->default_slice[np->unit];
  if (slice > 0)
    {
      if (slice >= ds->gd->max_p)
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  err = D_NO_SUCH_DEVICE;
	  goto out;
	}
      minor |= slice;

      if (linux_device_open_trace)
	printf ("device_open: at %d: start_sect 0x%x, nr_sects %d\n",
		__LINE__, ds->gd->part[minor].start_sect,
		ds->gd->part[minor].nr_sects);
    }
  dev = MKDEV (major, minor);

  /* Initialize file structure.  */
  file.f_mode = (mode == D_READ || np->read_only) ? O_RDONLY : O_RDWR;
  file.f_flags = (mode & D_NODELAY) ? O_NDELAY : 0;

  /* Check if the device is currently open.  */
  for (bdp = open_list; bdp; bdp = bdp->next)
    if (bdp->inode.i_rdev == dev
	&& bdp->part == part
	&& bdp->file.f_mode == file.f_mode
	&& bdp->file.f_flags == file.f_flags)
      {
	bd = bdp;
	goto out;
      }

  /* Open the device.  */
  if (ds->fops->open)
    {
      inode.i_rdev = dev;
      linux_intr_pri = SPL5;
      err = (*ds->fops->open) (&inode, &file);
      if (err)
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  err = linux_to_mach_error (err);
	  goto out;
	}
    }
      
  /* Read disklabel.  */
  if (part >= 0 && (! ds->label || ! ds->label[minor]))
    {
      int bsize, bshift;
      struct evtoc *evp;
      struct disklabel *lp, *dlp;
      struct buffer_head *bhp;

      assert (disk_major (major));

      /* Allocate a disklabel.  */
      lp = (struct disklabel *) kalloc (sizeof (struct disklabel));
      if (! lp)
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  err = D_NO_MEMORY;
	  goto bad;
	}

      /* Allocate a buffer for I/O.  */
      get_block_size (dev, &bsize, &bshift);
      assert (bsize <= PAGE_SIZE);
      bhp = getblk (dev, LBLLOC >> (bshift - 9), bsize);
      if (! bhp)
	{
	  if (linux_device_open_trace)
	    printf ("device_open: at %d\n", __LINE__);

	  err = D_NO_MEMORY;
	  goto label_done;
	}

      /* Set up 'c' partition to span the entire DOS partition.  */
      lp->d_npartitions = PART_DISK + 1;
      memset (lp->d_partitions, 0, MAXPARTITIONS * sizeof (struct partition));
      lp->d_partitions[PART_DISK].p_offset = ds->gd->part[minor].start_sect;
      lp->d_partitions[PART_DISK].p_size = ds->gd->part[minor].nr_sects;

      /* Try reading a BSD disklabel.  */
      ll_rw_block (READ, 1, &bhp);
      wait_on_buffer (bhp);
      err = check_for_error (READ, 1, &bhp);
      if (err)
	{
	  printf ("%s: error reading BSD label\n", np->name);
	  err = 0;
	  goto vtoc;
	}
      dlp = (struct disklabel *) (bhp->b_data + ((LBLLOC << 9) & (bsize - 1)));
      if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC)
	goto vtoc;
      printf ("%s: BSD LABEL\n", np->name);
      lp->d_npartitions = dlp->d_npartitions;
      memcpy (lp->d_partitions, dlp->d_partitions,
	      MAXPARTITIONS * sizeof (struct partition));

      /* Check for NetBSD DOS partition bogosity.  */
      for (i = 0; i < lp->d_npartitions; i++)
	if (lp->d_partitions[i].p_size > ds->gd->part[minor].nr_sects)
	  ds->gd->part[minor].nr_sects = lp->d_partitions[i].p_size;
      goto label_done;

    vtoc:
      /* Try reading VTOC.  */
      bhp->b_blocknr = PDLOCATION >> (bshift - 9);
      bhp->b_state = 1 << BH_Lock;
      ll_rw_block (READ, 1, &bhp);
      wait_on_buffer (bhp);
      err = check_for_error (READ, 1, &bhp);
      if (err)
	{
	  printf ("%s: error reading evtoc\n", np->name);
	  err = linux_to_mach_error (err);
	  goto label_done;
	}
      evp = (struct evtoc *) (bhp->b_data + ((PDLOCATION << 9) & (bsize - 1)));
      if (evp->sanity != VTOC_SANE)
	{
	  printf ("%s: No BSD or Mach label found\n", np->name);
	  err = D_NO_SUCH_DEVICE;
	  goto label_done;
	}
      printf ("%s: LOCAL LABEL\n", np->name);
      lp->d_npartitions = (evp->nparts > MAXPARTITIONS
			   ? MAXPARTITIONS : evp->nparts);
      for (i = 0; i < lp->d_npartitions; i++)
	{
	  lp->d_partitions[i].p_size = evp->part[i].p_size;
	  lp->d_partitions[i].p_offset = evp->part[i].p_start;
	  lp->d_partitions[i].p_fstype = FS_BSDFFS;
	}

    label_done:
      if (bhp)
	__brelse (bhp);
      if (err)
	{
	  kfree ((vm_offset_t) lp, sizeof (struct disklabel));
	  goto bad;
	}
      if (! ds->label)
	{
	  ds->label = (struct disklabel **) kalloc (sizeof (struct disklabel *)
						    * ds->gd->max_p
						    * ds->gd->max_nr);
	  if (! ds->label)
	    {
	      if (linux_device_open_trace)
		printf ("device_open: at %d\n", __LINE__);

	      kfree ((vm_offset_t) lp, sizeof (struct disklabel));
	      err = D_NO_MEMORY;
	      goto bad;
	    }
	  memset (ds->label, 0,
		  (sizeof (struct disklabel *)
		   * ds->gd->max_p * ds->gd->max_nr));
	}
      ds->label[minor] = lp;
    }

  /* Check partition number.  */
  if (part >= 0
      && (part >= ds->label[minor]->d_npartitions
	  || ds->label[minor]->d_partitions[part].p_size == 0))
    {
      err = D_NO_SUCH_DEVICE;
      goto bad;
    }

  /* Allocate and initialize device data.  */
  bd = (struct block_data *) kalloc (sizeof (struct block_data));
  if (! bd)
    {
      if (linux_device_open_trace)
	printf ("device_open: at %d\n", __LINE__);

      err = D_NO_MEMORY;
      goto bad;
    }
  bd->want = 0;
  bd->open_count = 0;
  bd->iocount = 0;
  bd->part = part;
  bd->ds = ds;
  bd->device.emul_data = bd;
  bd->device.emul_ops = &linux_block_emulation_ops;
  bd->inode.i_rdev = dev;
  bd->file.f_mode = file.f_mode;
  bd->file.f_np = np;
  bd->file.f_flags = file.f_flags;
  bd->port = ipc_port_alloc_kernel ();
  if (bd->port == IP_NULL)
    {
      if (linux_device_open_trace)
	printf ("device_open: at %d\n", __LINE__);

      err = KERN_RESOURCE_SHORTAGE;
      goto bad;
    }
  ipc_kobject_set (bd->port, (ipc_kobject_t) &bd->device, IKOT_DEVICE);
  notify = ipc_port_make_sonce (bd->port);
  ip_lock (bd->port);
  ipc_port_nsrequest (bd->port, 1, notify, &notify);
  assert (notify == IP_NULL);

  goto out;

bad:
  if (ds->fops->release)
    (*ds->fops->release) (&inode, &file);

out:
  ds->busy = 0;
  if (ds->want)
    {
      ds->want = 0;
      thread_wakeup ((event_t) ds);
    }

  if (bd && bd->open_count > 0)
    {
      if (err)
	*devp = NULL;
      else
	{
	  *devp = &bd->device;
	  bd->open_count++;
	}
      return err;
    }

  if (err)
    {
      if (bd)
	{
	  if (bd->port != IP_NULL)
	    {
	      ipc_kobject_set (bd->port, IKO_NULL, IKOT_NONE);
	      ipc_port_dealloc_kernel (bd->port);
	    }
	  kfree ((vm_offset_t) bd, sizeof (struct block_data));
	  bd = NULL;
	}
    }
  else
    {
      bd->open_count = 1;
      bd->next = open_list;
      open_list = bd;
    }

  if (IP_VALID (reply_port))
    ds_device_open_reply (reply_port, reply_port_type, err, dev_to_port (bd));
  else if (! err)
    device_close (bd);

  return MIG_NO_REPLY;
}

static io_return_t
device_close (void *d)
{
  struct block_data *bd = d, *bdp, **prev;
  struct device_struct *ds = bd->ds;

  /* Wait for any other open/close to complete.  */
  while (ds->busy)
    {
      ds->want = 1;
      assert_wait ((event_t) ds, FALSE);
      thread_block (0);
    }
  ds->busy = 1;

  if (--bd->open_count == 0)
    {
      /* Wait for pending I/O to complete.  */
      while (bd->iocount > 0)
	{
	  bd->want = 1;
	  assert_wait ((event_t) bd, FALSE);
	  thread_block (0);
	}

      /* Remove device from open list.  */
      prev = &open_list;
      bdp = open_list;
      while (bdp)
	{
	  if (bdp == bd)
	    {
	      *prev = bdp->next;
	      break;
	    }
	  prev = &bdp->next;
	  bdp = bdp->next;
	}

      assert (bdp == bd);

      if (ds->fops->release)
	(*ds->fops->release) (&bd->inode, &bd->file);

      ipc_kobject_set (bd->port, IKO_NULL, IKOT_NONE);
      ipc_port_dealloc_kernel (bd->port);
      kfree ((vm_offset_t) bd, sizeof (struct block_data));
    }

  ds->busy = 0;
  if (ds->want)
    {
      ds->want = 0;
      thread_wakeup ((event_t) ds);
    }
  return D_SUCCESS;
}

/* XXX: Assumes all drivers use block_write.  */
static io_return_t
device_write (void *d, ipc_port_t reply_port,
	      mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	      recnum_t bn, io_buf_ptr_t data, unsigned int count,
	      int *bytes_written)
{
  int major, minor;
  unsigned sz, maxsz, off;
  io_return_t err = 0;
  struct block_data *bd = d;

  if (! bd->ds->fops->write)
    {
      printf ("device_write: at %d\n", __LINE__);
      return D_INVALID_OPERATION;
    }

  if ((int) count <= 0)
    {
      printf ("device_write: at %d\n", __LINE__);
      return D_INVALID_SIZE;
    }

  major = MAJOR (bd->inode.i_rdev);
  minor = MINOR (bd->inode.i_rdev);

  if (disk_major (major))
    {
      assert (bd->ds->gd);

      if (bd->part >= 0)
	{
	  struct disklabel *lp;

	  assert (bd->ds->label);
	  lp = bd->ds->label[minor];
	  assert (lp);
	  maxsz = lp->d_partitions[bd->part].p_size;
	  off = (lp->d_partitions[bd->part].p_offset
		 - bd->ds->gd->part[minor].start_sect);

	  if (linux_block_write_trace)
	    printf ("device_write: at %d: dev %s, part %d, "
		    "offset 0x%x (%u), start_sect 0x%x (%u), "
		    "maxsz 0x%x (%u)\n",
		    __LINE__,
		    kdevname (bd->inode.i_rdev),
		    bd->part,
		    lp->d_partitions[bd->part].p_offset,
		    lp->d_partitions[bd->part].p_offset,
		    bd->ds->gd->part[minor].start_sect,
		    bd->ds->gd->part[minor].start_sect,
		    maxsz, maxsz);

	  assert (off < bd->ds->gd->part[minor].nr_sects);
	}
      else
	{
	  maxsz = bd->ds->gd->part[minor].nr_sects;
	  off = 0;
	}
    }
  else
    {
      assert (blk_size[major]);
      maxsz = blk_size[major][minor] << (BLOCK_SIZE_BITS - 9);
      off = 0;
    }

  if (bn >= maxsz)
    {
      if (linux_block_write_trace)
	printf ("device_write: at %d\n", __LINE__);

      return D_INVALID_SIZE;
    }

  bd->iocount++;

  sz = (count + 511) >> 9;
  if (sz > maxsz - bn)
    {
      sz = maxsz - bn;
      if (count > (sz << 9))
	count = sz << 9;
    }

  bd->file.f_pos = (loff_t) (bn + off) << 9;

  err = (*bd->ds->fops->write) (&bd->inode, &bd->file, (char *) data, count);
  if (err)
    err = linux_to_mach_error (err);

  if (linux_block_write_trace)
    printf ("device_write: at %d: err %d\n", __LINE__, err);

  if (IP_VALID (reply_port))
    ds_device_write_reply (reply_port, reply_port_type,
			   err, count - bd->file.f_resid);

  if (--bd->iocount == 0 && bd->want)
    {
      bd->want = 0;
      thread_wakeup ((event_t) bd);
    }
  return MIG_NO_REPLY;
}

/* XXX: Assumes all drivers use block_read.  */
static io_return_t
device_read (void *d, ipc_port_t reply_port,
	     mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	     recnum_t bn, int count, io_buf_ptr_t *data,
	     unsigned *bytes_read)
{
  int major, minor;
  unsigned sz, maxsz, off;
  io_return_t err = 0;
  vm_offset_t addr;
  vm_object_t object;
  vm_map_copy_t copy;
  struct block_data *bd = d;
  struct inode *inode = &bd->inode;

  *data = 0;
  *bytes_read = 0;

  if (! bd->ds->fops->read)
    return D_INVALID_OPERATION;

  if (count <= 0)
    return D_INVALID_SIZE;

  major = MAJOR (bd->inode.i_rdev);
  minor = MINOR (bd->inode.i_rdev);

  if (LINUX_BLOCK_READ_TRACE)
    printf ("device_read: at %d: major %d, minor %d, count %d, recnum %u\n",
	    __LINE__, major, minor, count, bn);

  if (disk_major (major))
    {
      assert (bd->ds->gd);

      if (bd->part >= 0)
	{
	  struct disklabel *lp;

	  assert (bd->ds->label);
	  lp = bd->ds->label[minor];
	  assert (lp);
	  maxsz = lp->d_partitions[bd->part].p_size;
	  off = (lp->d_partitions[bd->part].p_offset
		 - bd->ds->gd->part[minor].start_sect);

	  if (LINUX_BLOCK_READ_TRACE)
	    printf ("device_read: at %d: dev %s, part %d, offset 0x%x, "
		    "size %d, start_sect 0x%x, nr_sects %d\n",
		    __LINE__, kdevname (major), bd->part, off, maxsz,
		    bd->ds->gd->part[minor].start_sect,
		    bd->ds->gd->part[minor].nr_sects);

	  assert (off < bd->ds->gd->part[minor].nr_sects);
	}
      else
	{
	  maxsz = bd->ds->gd->part[minor].nr_sects;
	  off = 0;
	}
    }
  else
    {
      assert (blk_size[major]);
      maxsz = blk_size[major][minor] << (BLOCK_SIZE_BITS - 9);
      off = 0;
    }

  if (bn > maxsz)
    return D_INVALID_SIZE;

  /* Be backward compatible with Unix.  */
  if (bn == maxsz)
    {
      if (LINUX_BLOCK_READ_TRACE)
	printf ("device_read: at %d\n", __LINE__);
      return 0;
    }

  sz = (count + 511) >> 9;
  if (sz > maxsz - bn)
    {
      sz = maxsz - bn;
      if (count > (sz << 9))
	count = sz << 9;
    }

  bd->file.f_pos = (loff_t) (bn + off) << 9;
  bd->file.f_object = NULL;

  if (LINUX_BLOCK_READ_TRACE)
    printf ("device_read: at %d: f_pos 0x%x\n",
	    __LINE__, (unsigned) bd->file.f_pos);

  bd->iocount++;

  err = (*bd->ds->fops->read) (&bd->inode, &bd->file, (char *) data, count);
  if (err)
    err = linux_to_mach_error (err);
  else
    {
      object = bd->file.f_object;
      assert (object);
      assert (object->ref_count == 1);
      err = vm_map_copyin_object (object, 0, round_page (count), &copy);
      assert (object->ref_count == 1);
      if (err)
	vm_object_deallocate (object);
      else
	{
	  assert (copy->cpy_object->ref_count == 1);
	  *data = (io_buf_ptr_t) copy;
	  *bytes_read = count - bd->file.f_resid;
	}
    }
  if (--bd->iocount == 0 && bd->want)
    {
      bd->want = 0;
      thread_wakeup ((event_t) bd);
    }
  return err;
}

static io_return_t
device_get_status (void *d, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *status_count)
{
  struct block_data *bd = d;

  switch (flavor)
    {
    case DEV_GET_SIZE:
      if (*status_count != DEV_GET_SIZE_COUNT)
	return D_INVALID_SIZE;
      if (disk_major (MAJOR (bd->inode.i_rdev)))
	{
	  assert (bd->ds->gd);

	  if (bd->part >= 0)
	    {
	      struct disklabel *lp;

	      assert (bd->ds->label);
	      lp = bd->ds->label[MINOR (bd->inode.i_rdev)];
	      assert (lp);
	      (status[DEV_GET_SIZE_DEVICE_SIZE]
	       = lp->d_partitions[bd->part].p_size << 9);
	    }
	  else
	    (status[DEV_GET_SIZE_DEVICE_SIZE]
	     = bd->ds->gd->part[MINOR (bd->inode.i_rdev)].nr_sects << 9);
	}
      else
	{
	  assert (blk_size[MAJOR (bd->inode.i_rdev)]);
	  (status[DEV_GET_SIZE_DEVICE_SIZE]
	   = (blk_size[MAJOR (bd->inode.i_rdev)][MINOR (bd->inode.i_rdev)]
	      << BLOCK_SIZE_BITS));
	}
      /* It would be nice to return the block size as reported by
	 the driver, but a lot of user level code assumes the sector
	 size to be 512.  */
      status[DEV_GET_SIZE_RECORD_SIZE] = 512;
      break;

    default:
      return D_INVALID_OPERATION;
    }

  return D_SUCCESS;
}

struct device_emulation_ops linux_block_emulation_ops =
{
  NULL,
  NULL,
  dev_to_port,
  device_open,
  device_close,
  device_write,
  NULL,
  device_read,
  NULL,
  NULL,
  device_get_status,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};
