/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the IP protocol.
 *
 * Version:	@(#)ip.h	1.0.2	04/28/93
 *
 * Authors:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IP_H
#define _LINUX_IP_H
#include <asm/byteorder.h>

#define IPOPT_END	0
#define IPOPT_NOOP	1
#define IPOPT_SEC	130
#define IPOPT_LSRR	131
#define IPOPT_SSRR	137
#define IPOPT_RR	7
#define IPOPT_SID	136
#define IPOPT_TIMESTAMP	68


#define MAXTTL		255

struct timestamp {
	__u8	len;
	__u8	ptr;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	flags:4,
		overflow:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	overflow:4,
		flags:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif						
	__u32	data[9];
};


#define MAX_ROUTE	16

struct route {
  char		route_size;
  char		pointer;
  unsigned long route[MAX_ROUTE];
};

#define IPOPT_OPTVAL 0
#define IPOPT_OLEN   1
#define IPOPT_OFFSET 2
#define IPOPT_MINOFF 4
#define MAX_IPOPTLEN 40
#define IPOPT_NOP IPOPT_NOOP
#define IPOPT_EOL IPOPT_END
#define IPOPT_TS  IPOPT_TIMESTAMP

#define	IPOPT_TS_TSONLY		0		/* timestamps only */
#define	IPOPT_TS_TSANDADDR	1		/* timestamps and addresses */
#define	IPOPT_TS_PRESPEC	2		/* specified modules only */

struct options {
  __u32		faddr;				/* Saved first hop address */
  unsigned char	optlen;
  unsigned char srr;
  unsigned char rr;
  unsigned char ts;
  unsigned char is_setbyuser:1,			/* Set by setsockopt?			*/
                is_data:1,			/* Options in __data, rather than skb	*/
                is_strictroute:1,		/* Strict source route			*/
                srr_is_hit:1,			/* Packet destination addr was our one	*/
                is_changed:1,			/* IP checksum more not valid		*/	
                rr_needaddr:1,			/* Need to record addr of outgoing dev	*/
                ts_needtime:1,			/* Need to record timestamp		*/
                ts_needaddr:1;			/* Need to record addr of outgoing dev  */
  unsigned char __pad1;
  unsigned char __pad2;
  unsigned char __pad3;
  unsigned char __data[0];
};

struct iphdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	ihl:4,
		version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
	__u8	version:4,
  		ihl:4;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8	tos;
	__u16	tot_len;
	__u16	id;
	__u16	frag_off;
	__u8	ttl;
	__u8	protocol;
	__u16	check;
	__u32	saddr;
	__u32	daddr;
	/*The options start here. */
};


#endif	/* _LINUX_IP_H */
