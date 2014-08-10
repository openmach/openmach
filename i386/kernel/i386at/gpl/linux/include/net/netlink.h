#ifndef __NET_NETLINK_H
#define __NET_NETLINK_H

#define NET_MAJOR 36		/* Major 18 is reserved for networking 						*/
#define MAX_LINKS 4		/* 18,0 for route updates, 18,1 for SKIP, 18,2 debug tap 18,3 PPP reserved 	*/
#define MAX_QBYTES 32768	/* Maximum bytes in the queue 							*/

#include <linux/config.h>

extern int netlink_attach(int unit, int (*function)(struct sk_buff *skb));
extern int netlink_donothing(struct sk_buff *skb);
extern void netlink_detach(int unit);
extern int netlink_post(int unit, struct sk_buff *skb);
extern int init_netlink(void);

#define NETLINK_ROUTE		0	/* Routing/device hook				*/
#define NETLINK_SKIP		1	/* Reserved for ENskip  			*/
#define NETLINK_USERSOCK	2	/* Reserved for user mode socket protocols 	*/
#define NETLINK_FIREWALL	3	/* Firewalling hook				*/

#ifdef CONFIG_RTNETLINK
extern void ip_netlink_msg(unsigned long, __u32, __u32, __u32, short, short, char *);
#else
#define ip_netlink_msg(a,b,c,d,e,f,g)
#endif
#endif
