/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Interfaces handler.
 *
 * Version:	@(#)dev.h	1.0.10	08/12/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Donald J. Becker, <becker@super.org>
 *		Alan Cox, <A.Cox@swansea.ac.uk>
 *		Bjorn Ekwall. <bj0rn@blox.se>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Moved to /usr/include/linux for NET3
 */
#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/config.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>

/* for future expansion when we will have different priorities. */
#define DEV_NUMBUFFS	3
#define MAX_ADDR_LEN	7
#ifndef CONFIG_AX25
#ifndef CONFIG_TR
#ifndef CONFIG_NET_IPIP
#define MAX_HEADER	32		/* We really need about 18 worst case .. so 32 is aligned */
#else
#define MAX_HEADER	48		/* We need to allow for having tunnel headers */
#endif  /* IPIP */
#else
#define MAX_HEADER	48		/* Token Ring header needs 40 bytes ... 48 is aligned */ 
#endif /* TR */
#else
#define MAX_HEADER	96		/* AX.25 + NetROM */
#endif /* AX25 */

#define IS_MYADDR	1		/* address is (one of) our own	*/
#define IS_LOOPBACK	2		/* address is for LOOPBACK	*/
#define IS_BROADCAST	3		/* address is a valid broadcast	*/
#define IS_INVBCAST	4		/* Wrong netmask bcast not for us (unused)*/
#define IS_MULTICAST	5		/* Multicast IP address */

/*
 *	We tag multicasts with these structures.
 */
 
struct dev_mc_list
{	
	struct dev_mc_list *next;
	char dmi_addr[MAX_ADDR_LEN];
	unsigned short dmi_addrlen;
	unsigned short dmi_users;
};

struct hh_cache
{
	struct hh_cache *hh_next;
	void		*hh_arp;	/* Opaque pointer, used by
					 * any address resolution module,
					 * not only ARP.
					 */
	unsigned int	hh_refcnt;	/* number of users */
	unsigned short  hh_type;	/* protocol identifier, f.e ETH_P_IP */
	char		hh_uptodate;	/* hh_data is valid */
	char		hh_data[16];    /* cached hardware header */
};

/*
 * The DEVICE structure.
 * Actually, this whole structure is a big mistake.  It mixes I/O
 * data with strictly "high-level" data, and it has to know about
 * almost every data structure used in the INET module.  
 */
#ifdef MACH
#ifndef MACH_INCLUDE
#define device linux_device
#endif
struct linux_device
#else
struct device 
#endif
{

  /*
   * This is the first field of the "visible" part of this structure
   * (i.e. as seen by users in the "Space.c" file).  It is the name
   * the interface.
   */
  char			  *name;

  /* I/O specific fields - FIXME: Merge these and struct ifmap into one */
  unsigned long		  rmem_end;		/* shmem "recv" end	*/
  unsigned long		  rmem_start;		/* shmem "recv" start	*/
  unsigned long		  mem_end;		/* shared mem end	*/
  unsigned long		  mem_start;		/* shared mem start	*/
  unsigned long		  base_addr;		/* device I/O address	*/
  unsigned char		  irq;			/* device IRQ number	*/

  /* Low-level status flags. */
  volatile unsigned char  start,		/* start an operation	*/
                          interrupt;		/* interrupt arrived	*/
  unsigned long		  tbusy;		/* transmitter busy must be long for bitops */

  struct linux_device	  *next;

  /* The device initialization function. Called only once. */
  int			  (*init)(struct linux_device *dev);

  /* Some hardware also needs these fields, but they are not part of the
     usual set specified in Space.c. */
  unsigned char		  if_port;		/* Selectable AUI, TP,..*/
  unsigned char		  dma;			/* DMA channel		*/

  struct enet_statistics* (*get_stats)(struct linux_device *dev);

  /*
   * This marks the end of the "visible" part of the structure. All
   * fields hereafter are internal to the system, and may change at
   * will (read: may be cleaned up at will).
   */

  /* These may be needed for future network-power-down code. */
  unsigned long		  trans_start;	/* Time (in jiffies) of last Tx	*/
  unsigned long		  last_rx;	/* Time of last Rx		*/

  unsigned short	  flags;	/* interface flags (a la BSD)	*/
  unsigned short	  family;	/* address family ID (AF_INET)	*/
  unsigned short	  metric;	/* routing metric (not used)	*/
  unsigned short	  mtu;		/* interface MTU value		*/
  unsigned short	  type;		/* interface hardware type	*/
  unsigned short	  hard_header_len;	/* hardware hdr length	*/
  void			  *priv;	/* pointer to private data	*/

  /* Interface address info. */
  unsigned char		  broadcast[MAX_ADDR_LEN];	/* hw bcast add	*/
  unsigned char		  pad;				/* make dev_addr aligned to 8 bytes */
  unsigned char		  dev_addr[MAX_ADDR_LEN];	/* hw address	*/
  unsigned char		  addr_len;	/* hardware address length	*/
  unsigned long		  pa_addr;	/* protocol address		*/
  unsigned long		  pa_brdaddr;	/* protocol broadcast addr	*/
  unsigned long		  pa_dstaddr;	/* protocol P-P other side addr	*/
  unsigned long		  pa_mask;	/* protocol netmask		*/
  unsigned short	  pa_alen;	/* protocol address length	*/

  struct dev_mc_list	 *mc_list;	/* Multicast mac addresses	*/
  int			 mc_count;	/* Number of installed mcasts	*/
  
  struct ip_mc_list	 *ip_mc_list;	/* IP multicast filter chain    */
  __u32			tx_queue_len;	/* Max frames per queue allowed */
    
  /* For load balancing driver pair support */
  
  unsigned long		   pkt_queue;	/* Packets queued */
  struct linux_device		  *slave;	/* Slave device */
  struct net_alias_info		*alias_info;	/* main dev alias info */
  struct net_alias		*my_alias;	/* alias devs */
  
  /* Pointer to the interface buffers. */
  struct sk_buff_head	  buffs[DEV_NUMBUFFS];

  /* Pointers to interface service routines. */
  int			  (*open)(struct linux_device *dev);
  int			  (*stop)(struct linux_device *dev);
  int			  (*hard_start_xmit) (struct sk_buff *skb,
					      struct linux_device *dev);
  int			  (*hard_header) (struct sk_buff *skb,
					  struct linux_device *dev,
					  unsigned short type,
					  void *daddr,
					  void *saddr,
					  unsigned len);
  int			  (*rebuild_header)(void *eth,
					    struct linux_device *dev,
				unsigned long raddr, struct sk_buff *skb);
#define HAVE_MULTICAST			 
  void			  (*set_multicast_list)(struct linux_device *dev);
#define HAVE_SET_MAC_ADDR  		 
  int			  (*set_mac_address)(struct linux_device *dev,
					     void *addr);
#define HAVE_PRIVATE_IOCTL
  int			  (*do_ioctl)(struct linux_device *dev,
				      struct ifreq *ifr, int cmd);
#define HAVE_SET_CONFIG
  int			  (*set_config)(struct linux_device *dev,
					struct ifmap *map);
#define HAVE_HEADER_CACHE
  void			  (*header_cache_bind)(struct hh_cache **hhp,
					       struct linux_device *dev,
					       unsigned short htype,
					       __u32 daddr);
  void			  (*header_cache_update)(struct hh_cache *hh,
						 struct linux_device *dev,
						 unsigned char *  haddr);
#ifdef MACH
#ifdef MACH_INCLUDE
  struct net_data *net_data;
#else
  void *net_data;
#endif
#endif
};


struct packet_type {
  unsigned short	type;	/* This is really htons(ether_type). */
  struct linux_device *	dev;
  int			(*func) (struct sk_buff *, struct linux_device *,
				 struct packet_type *);
  void			*data;
  struct packet_type	*next;
};


#ifdef __KERNEL__

#include <linux/notifier.h>

/* Used by dev_rint */
#define IN_SKBUFF	1

extern volatile unsigned long in_bh;

extern struct linux_device	loopback_dev;
extern struct linux_device	*dev_base;
extern struct packet_type *ptype_base[16];


extern int		ip_addr_match(unsigned long addr1, unsigned long addr2);
extern int		ip_chk_addr(unsigned long addr);
extern struct linux_device	*ip_dev_check(unsigned long daddr);
extern unsigned long	ip_my_addr(void);
extern unsigned long	ip_get_mask(unsigned long addr);
extern struct linux_device 	*ip_dev_find(unsigned long addr);
extern struct linux_device    *dev_getbytype(unsigned short type);

extern void		dev_add_pack(struct packet_type *pt);
extern void		dev_remove_pack(struct packet_type *pt);
extern struct linux_device	*dev_get(const char *name);
extern int		dev_open(struct linux_device *dev);
extern int		dev_close(struct linux_device *dev);
extern void		dev_queue_xmit(struct sk_buff *skb,
				       struct linux_device *dev,
				       int pri);
#define HAVE_NETIF_RX 1
extern void		netif_rx(struct sk_buff *skb);
extern void		dev_transmit(void);
extern int		in_net_bh(void);
extern void		net_bh(void *tmp);
#ifdef MACH
#define dev_tint(dev)
#else
extern void		dev_tint(struct linux_device *dev);
#endif
extern int		dev_get_info(char *buffer, char **start, off_t offset, int length, int dummy);
extern int		dev_ioctl(unsigned int cmd, void *);

extern void		dev_init(void);

/* Locking protection for page faults during outputs to devices unloaded during the fault */

extern int		dev_lockct;

/*
 *	These two dont currently need to be interrupt safe
 *	but they may do soon. Do it properly anyway.
 */

extern __inline__ void  dev_lock_list(void)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	dev_lockct++;
	restore_flags(flags);
}

extern __inline__ void  dev_unlock_list(void)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	dev_lockct--;
	restore_flags(flags);
}

/*
 *	This almost never occurs, isnt in performance critical paths
 *	and we can thus be relaxed about it
 */
 
extern __inline__ void dev_lock_wait(void)
{
	while(dev_lockct)
		schedule();
}


/* These functions live elsewhere (drivers/net/net_init.c, but related) */

extern void		ether_setup(struct linux_device *dev);
extern void		tr_setup(struct linux_device *dev);
extern int		ether_config(struct linux_device *dev,
				     struct ifmap *map);
/* Support for loadable net-drivers */
extern int		register_netdev(struct linux_device *dev);
extern void		unregister_netdev(struct linux_device *dev);
extern int 		register_netdevice_notifier(struct notifier_block *nb);
extern int		unregister_netdevice_notifier(struct notifier_block *nb);
/* Functions used for multicast support */
extern void		dev_mc_upload(struct linux_device *dev);
extern void 		dev_mc_delete(struct linux_device *dev,
				      void *addr, int alen, int all);
extern void		dev_mc_add(struct linux_device *dev,
				   void *addr, int alen, int newonly);
extern void		dev_mc_discard(struct linux_device *dev);
/* This is the wrong place but it'll do for the moment */
extern void		ip_mc_allhost(struct linux_device *dev);
#endif /* __KERNEL__ */

#endif	/* _LINUX_DEV_H */
