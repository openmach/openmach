/*
 * Linux network driver support.
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
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Ethernet-type device handling.
 *
 * Version:	@(#)eth.c	1.0.7	05/25/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Mark Evans, <evansmp@uhura.aston.ac.uk>
 *		Florian  La Roche, <rzsfl@rz.uni-sb.de>
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 * 
 * Fixes:
 *		Mr Linux	: Arp problems
 *		Alan Cox	: Generic queue tidyup (very tiny here)
 *		Alan Cox	: eth_header ntohs should be htons
 *		Alan Cox	: eth_rebuild_header missing an htons and
 *				  minor other things.
 *		Tegge		: Arp bug fixes. 
 *		Florian		: Removed many unnecessary functions, code cleanup
 *				  and changes for new arp and skbuff.
 *		Alan Cox	: Redid header building to reflect new format.
 *		Alan Cox	: ARP only when compiled with CONFIG_INET
 *		Greg Page	: 802.2 and SNAP stuff.
 *		Alan Cox	: MAC layer pointers/new format.
 *		Paul Gortmaker	: eth_copy_and_sum shouldn't csum padding.
 *		Alan Cox	: Protect against forwarding explosions with
 *				  older network drivers and IFF_ALLMULTI
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
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
#include <device/if_hdr.h>
#include <device/if_ether.h>
#include <device/if_hdr.h>
#include <device/net_io.h>
#include "device_reply.h"

#include <i386at/dev_hdr.h>
#include <i386at/device_emul.h>

#include <i386at/gpl/linux/linux_emul.h>

#define MACH_INCLUDE
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

/* One of these is associated with each instance of a device.  */
struct net_data
{
  ipc_port_t port;		/* device port */
  struct ifnet ifnet;		/* Mach ifnet structure (needed for filters) */
  struct device device;		/* generic device structure */
  struct linux_device *dev;	/* Linux network device structure */
};

/* List of sk_buffs waiting to be freed.  */
static struct sk_buff_head skb_done_list;

/* Forward declarations.  */

extern struct device_emulation_ops linux_net_emulation_ops;

static int print_packet_size = 0;

/* Linux kernel network support routines.  */

/* Requeue packet SKB for transmission after the interface DEV
   has timed out.  The priority of the packet is PRI.
   In Mach, we simply drop the packet like the native drivers.  */
void
dev_queue_xmit (struct sk_buff *skb, struct linux_device *dev, int pri)
{
  dev_kfree_skb (skb, FREE_WRITE);
}

/* Close the device DEV.  */
int
dev_close (struct linux_device *dev)
{
  return 0;
}

/* Network software interrupt handler.  */
void
net_bh (void *xxx)
{
  int len;
  struct sk_buff *skb;
  struct linux_device *dev;

  /* Start transmission on interfaces.  */
  for (dev = dev_base; dev; dev = dev->next)
    {
      if (dev->base_addr && dev->base_addr != 0xffe0)
	while (1)
	  {
	    skb = skb_dequeue (&dev->buffs[0]);
	    if (skb)
	      {
		len = skb->len;
		if ((*dev->hard_start_xmit) (skb, dev))
		  {
		    skb_queue_head (&dev->buffs[0], skb);
		    mark_bh (NET_BH);
		    break;
		  }
		else if (print_packet_size)
		  printf ("net_bh: length %d\n", len);
	      }
	    else
	      break;
	  }
    }
}

/* Free all sk_buffs on the done list.
   This routine is called by the iodone thread in ds_routines.c.  */
void
free_skbuffs ()
{
  struct sk_buff *skb;

  while (1)
    {
      skb = skb_dequeue (&skb_done_list);
      if (skb)
	{
	  if (skb->copy)
	    {
	      vm_map_copy_discard (skb->copy);
	      skb->copy = NULL;
	    }
	  if (IP_VALID (skb->reply))
	    {
	      ds_device_write_reply (skb->reply, skb->reply_type, 0, skb->len);
	      skb->reply = IP_NULL;
	    }
	  dev_kfree_skb (skb, FREE_WRITE);
	}
      else
	break;
    }
}

/* Allocate an sk_buff with SIZE bytes of data space.  */
struct sk_buff *
alloc_skb (unsigned int size, int priority)
{
  return dev_alloc_skb (size);
}

/* Free SKB.  */
void
kfree_skb (struct sk_buff *skb, int priority)
{
  dev_kfree_skb (skb, priority);
}

/* Allocate an sk_buff with SIZE bytes of data space.  */
struct sk_buff *
dev_alloc_skb (unsigned int size)
{
  struct sk_buff *skb;

  skb = linux_kmalloc (sizeof (struct sk_buff) + size, GFP_KERNEL);
  if (skb)
    {
      skb->dev = NULL;
      skb->reply = IP_NULL;
      skb->copy = NULL;
      skb->len = size;
      skb->prev = skb->next = NULL;
      skb->list = NULL;
      if (size)
	{
	  skb->data = (unsigned char *) (skb + 1);
	  skb->tail = skb->data + size;
	}
      else
	skb->data = skb->tail = NULL;
      skb->head = skb->data;
    }
  return skb;
}

/* Free the sk_buff SKB.  */
void
dev_kfree_skb (struct sk_buff *skb, int mode)
{
  unsigned flags;
  extern void *io_done_list;

  /* Queue sk_buff on done list if there is a
     page list attached or we need to send a reply.
     Wakeup the iodone thread to process the list.  */
  if (skb->copy || IP_VALID (skb->reply))
    {
      skb_queue_tail (&skb_done_list, skb);
      save_flags (flags);
      thread_wakeup ((event_t) &io_done_list);
      restore_flags (flags);
      return;
    }
  linux_kfree (skb);
}

/* Accept packet SKB received on an interface.  */
void
netif_rx (struct sk_buff *skb)
{
  ipc_kmsg_t kmsg;
  struct ether_header *eh;
  struct packet_header *ph;
  struct linux_device *dev = skb->dev;

  assert (skb != NULL);

  if (print_packet_size)
    printf ("netif_rx: length %d\n", skb->len);

  /* Allocate a kernel message buffer.  */
  kmsg = net_kmsg_get ();
  if (! kmsg)
    {
      dev_kfree_skb (skb, FREE_READ);
      return;
    }

  /* Copy packet into message buffer.  */
  eh = (struct ether_header *) (net_kmsg (kmsg)->header);
  ph = (struct packet_header *) (net_kmsg (kmsg)->packet);
  memcpy (eh, skb->data, sizeof (struct ether_header));
  memcpy (ph + 1, skb->data + sizeof (struct ether_header),
	  skb->len - sizeof (struct ether_header));
  ph->type = eh->ether_type;
  ph->length = (skb->len - sizeof (struct ether_header)
		+ sizeof (struct packet_header));

  dev_kfree_skb (skb, FREE_READ);

  /* Pass packet up to the microkernel.  */
  net_packet (&dev->net_data->ifnet, kmsg,
	      ph->length, ethernet_priority (kmsg));
}

/* Mach device interface routines.  */

/* Return a send right associated with network device ND.  */
static ipc_port_t
dev_to_port (void *nd)
{
  return (nd
	  ? ipc_port_make_send (((struct net_data *) nd)->port)
	  : IP_NULL);
}

static io_return_t
device_open (ipc_port_t reply_port, mach_msg_type_name_t reply_port_type,
	     dev_mode_t mode, char *name, device_t *devp)
{
  io_return_t err = D_SUCCESS;
  ipc_port_t notify;
  struct ifnet *ifp;
  struct linux_device *dev;
  struct net_data *nd;

  /* Search for the device.  */
  for (dev = dev_base; dev; dev = dev->next)
    if (dev->base_addr
	&& dev->base_addr != 0xffe0
	&& ! strcmp (name, dev->name))
      break;
  if (! dev)
    return D_NO_SUCH_DEVICE;

  /* Allocate and initialize device data if this is the first open.  */
  nd = dev->net_data;
  if (! nd)
    {
      dev->net_data = nd = ((struct net_data *)
			    kalloc (sizeof (struct net_data)));
      if (! nd)
	{
	  err = D_NO_MEMORY;
	  goto out;
	}
      nd->dev = dev;
      nd->device.emul_data = nd;
      nd->device.emul_ops = &linux_net_emulation_ops;
      nd->port = ipc_port_alloc_kernel ();
      if (nd->port == IP_NULL)
	{
	  err = KERN_RESOURCE_SHORTAGE;
	  goto out;
	}
      ipc_kobject_set (nd->port, (ipc_kobject_t) &nd->device, IKOT_DEVICE);
      notify = ipc_port_make_sonce (nd->port);
      ip_lock (nd->port);
      ipc_port_nsrequest (nd->port, 1, notify, &notify);
      assert (notify == IP_NULL);
	  
      ifp = &nd->ifnet;
      ifp->if_unit = dev->name[strlen (dev->name) - 1] - '0';
      ifp->if_flags = IFF_UP|IFF_RUNNING;
      ifp->if_mtu = dev->mtu;
      ifp->if_header_size = dev->hard_header_len;
      ifp->if_header_format = dev->type;
      ifp->if_address_size = dev->addr_len;
      ifp->if_address = dev->dev_addr;
      if_init_queues (ifp);

      if (dev->open)
	{
	  linux_intr_pri = SPL6;
	  if ((*dev->open) (dev))
	    err = D_NO_SUCH_DEVICE;
	}

    out:
      if (err)
	{
	  if (nd)
	    {
	      if (nd->port != IP_NULL)
		{
		  ipc_kobject_set (nd->port, IKO_NULL, IKOT_NONE);
		  ipc_port_dealloc_kernel (nd->port);
		}
	      kfree ((vm_offset_t) nd, sizeof (struct net_data));
	      nd = NULL;
	      dev->net_data = NULL;
	    }
	}
      else
	{
	  dev->flags |= LINUX_IFF_UP|LINUX_IFF_RUNNING;
	  skb_queue_head_init (&dev->buffs[0]);
	}
      if (IP_VALID (reply_port))
	ds_device_open_reply (reply_port, reply_port_type,
			      err, dev_to_port (nd));
      return MIG_NO_REPLY;
    }

  *devp = &nd->device;
  return D_SUCCESS;
}

static io_return_t
device_write (void *d, ipc_port_t reply_port,
	      mach_msg_type_name_t reply_port_type, dev_mode_t mode,
	      recnum_t bn, io_buf_ptr_t data, unsigned int count,
	      int *bytes_written)
{
  unsigned char *p;
  int i, amt, skblen, s;
  io_return_t err = 0;
  vm_map_copy_t copy = (vm_map_copy_t) data;
  struct net_data *nd = d;
  struct linux_device *dev = nd->dev;
  struct sk_buff *skb;

  if (count == 0 || count > dev->mtu + dev->hard_header_len)
    return D_INVALID_SIZE;

  /* Allocate a sk_buff.  */
  amt = PAGE_SIZE - (copy->offset & PAGE_MASK);
  skblen = (amt >= count) ? 0 : count;
  skb = dev_alloc_skb (skblen);
  if (! skb)
    return D_NO_MEMORY;

  /* Copy user data.  This is only required if it spans multiple pages.  */
  if (skblen == 0)
    {
      assert (copy->cpy_npages == 1);

      skb->copy = copy;
      skb->data = ((void *) copy->cpy_page_list[0]->phys_addr
		   + (copy->offset & PAGE_MASK));
      skb->len = count;
      skb->head = skb->data;
      skb->tail = skb->data + skb->len;
    }
  else
    {
      memcpy (skb->data,
	      ((void *) copy->cpy_page_list[0]->phys_addr
	       + (copy->offset & PAGE_MASK)),
	      amt);
      count -= amt;
      p = skb->data + amt;
      for (i = 1; count > 0 && i < copy->cpy_npages; i++)
	{
	  amt = PAGE_SIZE;
	  if (amt > count)
	    amt = count;
	  memcpy (p, (void *) copy->cpy_page_list[i]->phys_addr, amt);
	  count -= amt;
	  p += amt;
	}

      assert (count == 0);

      vm_map_copy_discard (copy);
    }

  skb->dev = dev;
  skb->reply = reply_port;
  skb->reply_type = reply_port_type;

  /* Queue packet for transmission and schedule a software interrupt.  */
  s = splimp ();
  if (dev->buffs[0].next != (struct sk_buff *) &dev->buffs[0]
      || (*dev->hard_start_xmit) (skb, dev))
    {
      __skb_queue_tail (&dev->buffs[0], skb);
      mark_bh (NET_BH);
    }
  splx (s);

  return MIG_NO_REPLY;
}

static io_return_t
device_get_status (void *d, dev_flavor_t flavor, dev_status_t status,
		   mach_msg_type_number_t *count)
{
  return net_getstat (&((struct net_data *) d)->ifnet, flavor, status, count);
}

static io_return_t
device_set_filter (void *d, ipc_port_t port, int priority,
		   filter_t *filter, unsigned filter_count)
{
  return net_set_filter (&((struct net_data *) d)->ifnet,
			 port, priority, filter, filter_count);
}

struct device_emulation_ops linux_net_emulation_ops =
{
  NULL,
  NULL,
  dev_to_port,
  device_open,
  NULL,
  device_write,
  NULL,
  NULL,
  NULL,
  NULL,
  device_get_status,
  device_set_filter,
  NULL,
  NULL,
  NULL,
  NULL
};

/* Do any initialization required for network devices.  */
void
linux_net_emulation_init ()
{
  skb_queue_head_init (&skb_done_list);
}
