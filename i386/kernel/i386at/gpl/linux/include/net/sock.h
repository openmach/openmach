/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the AF_INET socket handler.
 *
 * Version:	@(#)sock.h	1.0.4	05/13/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche <flla@stud.uni-sb.de>
 *
 * Fixes:
 *		Alan Cox	:	Volatiles in skbuff pointers. See
 *					skbuff comments. May be overdone,
 *					better to prove they can be removed
 *					than the reverse.
 *		Alan Cox	:	Added a zapped field for tcp to note
 *					a socket is reset and must stay shut up
 *		Alan Cox	:	New fields for options
 *	Pauline Middelink	:	identd support
 *		Alan Cox	:	Eliminate low level recv/recvfrom
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _SOCK_H
#define _SOCK_H

#include <linux/timer.h>
#include <linux/ip.h>		/* struct options */
#include <linux/in.h>		/* struct sockaddr_in */
#include <linux/tcp.h>		/* struct tcphdr */
#include <linux/config.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>	/* struct sk_buff */
#include <net/protocol.h>		/* struct inet_protocol */
#ifdef CONFIG_AX25
#include <net/ax25.h>
#ifdef CONFIG_NETROM
#include <net/netrom.h>
#endif
#endif
#ifdef CONFIG_IPX
#include <net/ipx.h>
#endif
#ifdef CONFIG_ATALK
#include <linux/atalk.h>
#endif

#include <linux/igmp.h>

/* Think big (also on some systems a byte is faster) */
#define SOCK_ARRAY_SIZE	256


/*
 *	The AF_UNIX specific socket options
 */
 
struct unix_opt
{
	int 			family;
	char *			name;
	int  			locks;
	struct inode *		inode;
	struct semaphore	readsem;
	struct sock *		other;
};

/*
 *	IP packet socket options
 */

struct inet_packet_opt
{
	struct notifier_block	notifier;		/* Used when bound */
	struct device		*bound_dev;
	unsigned long		dev_stamp;
	struct packet_type	*prot_hook;
	char			device_name[15];
};


/*
 * This structure really needs to be cleaned up.
 * Most of it is for TCP, and not used by any of
 * the other protocols.
 */
struct sock 
{
	struct options		*opt;
	volatile unsigned long	wmem_alloc;
	volatile unsigned long	rmem_alloc;
	unsigned long		allocation;		/* Allocation mode */
	__u32			write_seq;
	__u32			sent_seq;
	__u32			acked_seq;
	__u32			copied_seq;
	__u32			rcv_ack_seq;
	__u32			window_seq;
	__u32			fin_seq;
	__u32			urg_seq;
	__u32			urg_data;
	int			users;			/* user count */
  /*
   *	Not all are volatile, but some are, so we
   * 	might as well say they all are.
   */
	volatile char		dead,
				urginline,
				intr,
				blog,
				done,
				reuse,
				keepopen,
				linger,
				delay_acks,
				destroy,
				ack_timed,
				no_check,
				zapped,	/* In ax25 & ipx means not linked */
				broadcast,
				nonagle,
				bsdism;
	unsigned long	        lingertime;
	int			proc;
	struct sock		*next;
	struct sock		*prev; /* Doubly linked chain.. */
	struct sock		*pair;
	struct sk_buff		* volatile send_head;
	struct sk_buff		* volatile send_tail;
	struct sk_buff_head	back_log;
	struct sk_buff		*partial;
	struct timer_list	partial_timer;
	long			retransmits;
	struct sk_buff_head	write_queue,
				receive_queue;
	struct proto		*prot;
	struct wait_queue	**sleep;
	__u32			daddr;
	__u32			saddr;		/* Sending source */
	__u32			rcv_saddr;	/* Bound address */
	unsigned short		max_unacked;
	unsigned short		window;
	__u32                   lastwin_seq;    /* sequence number when we last updated the window we offer */
	volatile unsigned long  ato;            /* ack timeout */
	volatile unsigned long  lrcvtime;       /* jiffies at last rcv */
	unsigned short		bytes_rcv;
/*
 *	mss is min(mtu, max_window) 
 */
	unsigned short		mtu;       /* mss negotiated in the syn's */
	volatile unsigned short	mss;       /* current eff. mss - can change */
	volatile unsigned short	user_mss;  /* mss requested by user in ioctl */
	volatile unsigned short	max_window;
	unsigned long 		window_clamp;
	unsigned short		num;
	volatile unsigned short	cong_window;
	volatile unsigned short	cong_count;
	volatile unsigned short	ssthresh;
	volatile unsigned short	packets_out;
	volatile unsigned short	shutdown;
	volatile unsigned long	rtt;
	volatile unsigned long	mdev;
	volatile unsigned long	rto;

/*
 *	currently backoff isn't used, but I'm maintaining it in case
 *	we want to go back to a backoff formula that needs it
 */
 
	volatile unsigned short	backoff;
	volatile int		err, err_soft;	/* Soft holds errors that don't
						   cause failure but are the cause
						   of a persistent failure not just
						   'timed out' */
	unsigned char		protocol;
	volatile unsigned char	state;
	volatile unsigned char	ack_backlog;
	unsigned char		max_ack_backlog;
	unsigned char		priority;
	unsigned char		debug;
	unsigned short		rcvbuf;
	unsigned short		sndbuf;
	unsigned short		type;
	unsigned char		localroute;	/* Route locally only */
#ifdef CONFIG_IPX
/*
 *	Once the IPX ncpd patches are in these are going into protinfo
 */
	ipx_address		ipx_dest_addr;
	ipx_interface		*ipx_intrfc;
	unsigned short		ipx_port;

/* To handle asynchronous messages from the NetWare server, we have to
 * know the connection this socket belongs to. Sorry to blow up this
 * structure even more. */
	struct ncp_server       *ipx_ncp_server;

#ifdef CONFIG_IPX_INTERN
	unsigned char           ipx_node[IPX_NODE_LEN];
#endif
	unsigned short		ipx_type;
#endif
#ifdef CONFIG_AX25
	ax25_cb			*ax25;
#ifdef CONFIG_NETROM
	nr_cb			*nr;
#endif
#endif
  
/*
 *	This is where all the private (optional) areas that don't
 *	overlap will eventually live. 
 */

	union
	{
	  	struct unix_opt	af_unix;
#ifdef CONFIG_ATALK
		struct atalk_sock	af_at;
#endif
#ifdef CONFIG_INET
		struct inet_packet_opt  af_packet;
#endif
	} protinfo;  		

/* 
 *	IP 'private area' or will be eventually 
 */
	int			ip_ttl;			/* TTL setting */
	int			ip_tos;			/* TOS */
	struct tcphdr		dummy_th;
	struct timer_list	keepalive_timer;	/* TCP keepalive hack */
	struct timer_list	retransmit_timer;	/* TCP retransmit timer */
	struct timer_list	ack_timer;		/* TCP delayed ack timer */
	int			ip_xmit_timeout;	/* Why the timeout is running */
	struct rtable		*ip_route_cache;	/* Cached output route */
	unsigned char		ip_hdrincl;		/* Include headers ? */
#ifdef CONFIG_IP_MULTICAST  
	int			ip_mc_ttl;		/* Multicasting TTL */
	int			ip_mc_loop;		/* Loopback */
	char			ip_mc_name[MAX_ADDR_LEN];/* Multicast device name */
	struct ip_mc_socklist	*ip_mc_list;		/* Group array */
#endif  

/*
 *	This part is used for the timeout functions (timer.c). 
 */
 
	int			timeout;	/* What are we waiting for? */
	struct timer_list	timer;		/* This is the TIME_WAIT/receive timer
					 * when we are doing IP
					 */
	struct timeval		stamp;

 /*
  *	Identd 
  */
  
	struct socket		*socket;
  
  /*
   *	Callbacks 
   */
   
	void			(*state_change)(struct sock *sk);
	void			(*data_ready)(struct sock *sk,int bytes);
	void			(*write_space)(struct sock *sk);
	void			(*error_report)(struct sock *sk);
  
};

/*
 *	IP protocol blocks we attach to sockets.
 */
 
struct proto 
{
	void			(*close)(struct sock *sk, unsigned long timeout);
	int			(*build_header)(struct sk_buff *skb,
					__u32 saddr,
					__u32 daddr,
					struct device **dev, int type,
					struct options *opt, int len,
					int tos, int ttl, struct rtable ** rp);
	int			(*connect)(struct sock *sk,
				        struct sockaddr_in *usin, int addr_len);
	struct sock *		(*accept) (struct sock *sk, int flags);
	void			(*queue_xmit)(struct sock *sk,
				        struct device *dev, struct sk_buff *skb,
				        int free);
	void			(*retransmit)(struct sock *sk, int all);
	void			(*write_wakeup)(struct sock *sk);
	void			(*read_wakeup)(struct sock *sk);
	int			(*rcv)(struct sk_buff *buff, struct device *dev,
				        struct options *opt, __u32 daddr,
				        unsigned short len, __u32 saddr,
				        int redo, struct inet_protocol *protocol);
	int			(*select)(struct sock *sk, int which,
					select_table *wait);
	int			(*ioctl)(struct sock *sk, int cmd,
					unsigned long arg);
	int			(*init)(struct sock *sk);
	void			(*shutdown)(struct sock *sk, int how);
	int			(*setsockopt)(struct sock *sk, int level, int optname,
					char *optval, int optlen);
	int			(*getsockopt)(struct sock *sk, int level, int optname,
					char *optval, int *option);  	 
	int			(*sendmsg)(struct sock *sk, struct msghdr *msg, int len,
					int noblock, int flags);
	int			(*recvmsg)(struct sock *sk, struct msghdr *msg, int len,
					int noblock, int flags, int *addr_len);
	int			(*bind)(struct sock *sk, struct sockaddr *uaddr, int addr_len);
	unsigned short		max_header;
	unsigned long		retransmits;
	char			name[32];
	int			inuse, highestinuse;
	struct sock *		sock_array[SOCK_ARRAY_SIZE];
};

#define TIME_WRITE	1
#define TIME_CLOSE	2
#define TIME_KEEPOPEN	3
#define TIME_DESTROY	4
#define TIME_DONE	5	/* Used to absorb those last few packets */
#define TIME_PROBE0	6
/*
 *	About 10 seconds 
 */
#define SOCK_DESTROY_TIME (10*HZ)


/*
 *	Sockets 0-1023 can't be bound too unless you are superuser 
 */
 
#define PROT_SOCK	1024


#define SHUTDOWN_MASK	3
#define RCV_SHUTDOWN	1
#define SEND_SHUTDOWN	2

/*
 * Used by processes to "lock" a socket state, so that
 * interrupts and bottom half handlers won't change it
 * from under us. It essentially blocks any incoming
 * packets, so that we won't get any new data or any
 * packets that change the state of the socket.
 *
 * Note the 'barrier()' calls: gcc may not move a lock
 * "downwards" or a unlock "upwards" when optimizing.
 */
extern void __release_sock(struct sock *sk);

static inline void lock_sock(struct sock *sk)
{
#if 1
/* debugging code: the test isn't even 100% correct, but it can catch bugs */
/* Note that a double lock is ok in theory - it's just _usually_ a bug */
	if (sk->users) {
		__label__ here;
		printk("double lock on socket at %p\n", &&here);
here:
	}
#endif
	sk->users++;
	barrier();
}

static inline void release_sock(struct sock *sk)
{
	barrier();
#if 1
/* debugging code: remove me when ok */
	if (sk->users == 0) {
		__label__ here;
		sk->users = 1;
		printk("trying to unlock unlocked socket at %p\n", &&here);
here:
	}
#endif
	if (!--sk->users)
		__release_sock(sk);
}


extern void			destroy_sock(struct sock *sk);
extern unsigned short		get_new_socknum(struct proto *,
						unsigned short);
extern void			put_sock(unsigned short, struct sock *); 
extern struct sock		*get_sock(struct proto *, unsigned short,
					  unsigned long, unsigned short,
					  unsigned long);
extern struct sock		*get_sock_mcast(struct sock *, unsigned short,
					  unsigned long, unsigned short,
					  unsigned long);
extern struct sock		*get_sock_raw(struct sock *, unsigned short,
					  unsigned long, unsigned long);

extern struct sk_buff		*sock_wmalloc(struct sock *sk,
					      unsigned long size, int force,
					      int priority);
extern struct sk_buff		*sock_rmalloc(struct sock *sk,
					      unsigned long size, int force,
					      int priority);
extern void			sock_wfree(struct sock *sk,
					   struct sk_buff *skb);
extern void			sock_rfree(struct sock *sk,
					   struct sk_buff *skb);
extern unsigned long		sock_rspace(struct sock *sk);
extern unsigned long		sock_wspace(struct sock *sk);

extern int			sock_setsockopt(struct sock *sk, int level,
						int op, char *optval,
						int optlen);

extern int			sock_getsockopt(struct sock *sk, int level,
						int op, char *optval, 
						int *optlen);
extern struct sk_buff 		*sock_alloc_send_skb(struct sock *skb,
						     unsigned long size,
						     unsigned long fallback,
						     int noblock,
						     int *errcode);

/*
 * 	Queue a received datagram if it will fit. Stream and sequenced
 *	protocols can't normally use this as they need to fit buffers in
 *	and play with them.
 *
 * 	Inlined as its very short and called for pretty much every
 *	packet ever received.
 */

extern __inline__ int sock_queue_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	unsigned long flags;
	if(sk->rmem_alloc + skb->truesize >= sk->rcvbuf)
		return -ENOMEM;
	save_flags(flags);
	cli();
	sk->rmem_alloc+=skb->truesize;
	skb->sk=sk;
	restore_flags(flags);
	skb_queue_tail(&sk->receive_queue,skb);
	if(!sk->dead)
		sk->data_ready(sk,skb->len);
	return 0;
}

/*
 *	Recover an error report and clear atomically
 */
 
extern __inline__ int sock_error(struct sock *sk)
{
	int err=xchg(&sk->err,0);
	return -err;
}

/* 
 *	Declarations from timer.c 
 */
 
extern struct sock *timer_base;

extern void delete_timer (struct sock *);
extern void reset_timer (struct sock *, int, unsigned long);
extern void net_timer (unsigned long);


/* 
 *	Enable debug/info messages 
 */

#define NETDEBUG(x)		x

#endif	/* _SOCK_H */
