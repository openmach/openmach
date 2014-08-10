/*
 * Linux scheduling support.
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
 *      Author: Shantanu Goel, University of Utah CSL
 */

/*
 *  linux/kernel/sched.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <sys/types.h>

#include <mach/boolean.h>

#include <kern/thread.h>
#include <kern/sched_prim.h>

#include <i386at/gpl/linux/linux_emul.h>

#define MACH_INCLUDE
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

#include <asm/system.h>

struct tq_struct tq_last =
{
  &tq_last, 0, 0, 0
};

DECLARE_TASK_QUEUE(tq_timer);

static struct wait_queue **auto_config_queue;

void
tqueue_bh (void *unused)
{
  run_task_queue(&tq_timer);
}

void
add_wait_queue (struct wait_queue **q, struct wait_queue *wait)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      save_flags (flags);
      cli ();
      assert_wait ((event_t) q, FALSE);
      restore_flags (flags);
      return;
    }

  if (auto_config_queue)
    printf ("add_wait_queue: queue not empty\n");
  auto_config_queue = q;
}

void
remove_wait_queue (struct wait_queue **q, struct wait_queue *wait)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      save_flags (flags);
      thread_wakeup ((event_t) q);
      restore_flags (flags);
      return;
    }

  auto_config_queue = NULL;
}

void
__down (struct semaphore *sem)
{
  int s;
  unsigned long flags;

  if (! linux_auto_config)
    {
      save_flags (flags);
      s = splhigh ();
      while (sem->count <= 0)
	{
	  assert_wait ((event_t) &sem->wait, FALSE);
	  splx (s);
	  thread_block (0);
	  s = splhigh ();
	}
      splx (s);
      restore_flags (flags);
      return;
    }

  while (sem->count <= 0)
    barrier ();
}

void
__sleep_on (struct wait_queue **q, int interruptible)
{
  unsigned long flags;

  if (! q)
    return;
  save_flags (flags);
  if (! linux_auto_config)
    {
      assert_wait ((event_t) q, interruptible);
      sti ();
      thread_block (0);
      restore_flags (flags);
      return;
    }

  add_wait_queue (q, NULL);
  sti ();
  while (auto_config_queue)
    barrier ();
  restore_flags (flags);
}

void
sleep_on (struct wait_queue **q)
{
  __sleep_on (q, FALSE);
}

void
interruptible_sleep_on (struct wait_queue **q)
{
  __sleep_on (q, TRUE);
}

void
wake_up (struct wait_queue **q)
{
  unsigned long flags;

  if (! linux_auto_config)
    {
      if (q != &wait_for_request)
	{
	  save_flags (flags);
	  thread_wakeup ((event_t) q);
	  restore_flags (flags);
	}
      return;
    }

  if (auto_config_queue == q)
    auto_config_queue = NULL;
}

void
__wait_on_buffer (struct buffer_head *bh)
{
  unsigned long flags;

  save_flags (flags);
  if (! linux_auto_config)
    {
      while (1)
	{
	  cli ();
	  if (! buffer_locked (bh))
	    break;
	  bh->b_wait = (struct wait_queue *) 1;
	  assert_wait ((event_t) bh, FALSE);
	  sti ();
	  thread_block (0);
	}
      restore_flags (flags);
      return;
    }

  sti ();
  while (buffer_locked (bh))
    barrier ();
  restore_flags (flags);
}

void
unlock_buffer (struct buffer_head *bh)
{
  unsigned long flags;

  save_flags (flags);
  cli ();
  clear_bit (BH_Lock, &bh->b_state);
  if (bh->b_wait && ! linux_auto_config)
    {
      bh->b_wait = NULL;
      thread_wakeup ((event_t) bh);
    }
  restore_flags (flags);
}

void
schedule ()
{
  if (! linux_auto_config)
    thread_block (0);
}

void
cdrom_sleep (int t)
{
  int xxx;

  assert_wait ((event_t) &xxx, TRUE);
  thread_set_timeout (t);
  thread_block (0);
}
