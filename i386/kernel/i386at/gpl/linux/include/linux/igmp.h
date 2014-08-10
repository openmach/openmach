/*
 *	Linux NET3:	Internet Gateway Management Protocol  [IGMP]
 *
 *	Authors:
 *		Alan Cox <Alan.Cox@linux.org>
 *
 *	Extended to talk the BSD extended IGMP protocol of mrouted 3.6
 *
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_IGMP_H
#define _LINUX_IGMP_H

/*
 *	IGMP protocol structures
 */

/*
 *	Header in on cable format
 */

struct igmphdr
{
	__u8 type;
	__u8 code;		/* For newer IGMP */
	__u16 csum;
	__u32 group;
};

#define IGMP_HOST_MEMBERSHIP_QUERY	0x11	/* From RFC1112 */
#define IGMP_HOST_MEMBERSHIP_REPORT	0x12	/* Ditto */
#define IGMP_DVMRP			0x13	/* DVMRP routing */
#define IGMP_PIM			0x14	/* PIM routing */
#define IGMP_HOST_NEW_MEMBERSHIP_REPORT 0x16	/* New version of 0x11 */
#define IGMP_HOST_LEAVE_MESSAGE 	0x17	/* An extra BSD seems to send */

#define IGMP_MTRACE_RESP		0x1e
#define IGMP_MTRACE			0x1f


/*
 *	Use the BSD names for these for compatibility
 */

#define IGMP_DELAYING_MEMBER		0x01
#define IGMP_IDLE_MEMBER		0x02
#define IGMP_LAZY_MEMBER		0x03
#define IGMP_SLEEPING_MEMBER		0x04
#define IGMP_AWAKENING_MEMBER		0x05

#define IGMP_OLD_ROUTER 		0x00
#define IGMP_NEW_ROUTER 		0x01

#define IGMP_MINLEN			8

#define IGMP_MAX_HOST_REPORT_DELAY	10	/* max delay for response to */
						/* query (in seconds)	*/

#define IGMP_TIMER_SCALE		10	/* denotes that the igmphdr->timer field */
						/* specifies time in 10th of seconds	 */

#define IGMP_AGE_THRESHOLD		540	/* If this host don't hear any IGMP V1	*/
						/* message in this period of time,	*/
						/* revert to IGMP v2 router.		*/

#define IGMP_ALL_HOSTS		htonl(0xE0000001L)
#define IGMP_ALL_ROUTER 	htonl(0xE0000002L)
#define IGMP_LOCAL_GROUP	htonl(0xE0000000L)
#define IGMP_LOCAL_GROUP_MASK	htonl(0xFFFFFF00L)

/*
 * struct for keeping the multicast list in
 */

#ifdef __KERNEL__
struct ip_mc_socklist
{
	unsigned long multiaddr[IP_MAX_MEMBERSHIPS];	/* This is a speed trade off */
	struct device *multidev[IP_MAX_MEMBERSHIPS];
};

struct ip_mc_list
{
	struct device *interface;
	unsigned long multiaddr;
	struct ip_mc_list *next;
	struct timer_list timer;
	int tm_running;
	int users;
};

struct ip_router_info
{
	struct device *dev;
	int    type;	/* type of router which is querier on this interface */
	int    time;	/* # of slow timeouts since last old query */
	struct timer_list timer;
	struct ip_router_info *next;
};

extern struct ip_mc_list *ip_mc_head;


extern int igmp_rcv(struct sk_buff *, struct device *, struct options *, __u32, unsigned short,
	__u32, int , struct inet_protocol *);
extern void ip_mc_drop_device(struct device *dev);
extern int ip_mc_join_group(struct sock *sk, struct device *dev, unsigned long addr);
extern int ip_mc_leave_group(struct sock *sk, struct device *dev,unsigned long addr);
extern void ip_mc_drop_socket(struct sock *sk);
extern void ip_mr_init(void);
#endif
#endif
