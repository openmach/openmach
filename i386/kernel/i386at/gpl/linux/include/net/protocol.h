/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the protocol dispatcher.
 *
 * Version:	@(#)protocol.h	1.0.2	05/07/93
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *	Changes:
 *		Alan Cox	:	Added a name field and a frag handler
 *					field for later.
 *		Alan Cox	:	Cleaned up, and sorted types.
 */
 
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#define MAX_INET_PROTOS	32		/* Must be a power of 2		*/


/* This is used to register protocols. */
struct inet_protocol {
  int			(*handler)(struct sk_buff *skb, struct device *dev,
				   struct options *opt, __u32 daddr,
				   unsigned short len, __u32 saddr,
				   int redo, struct inet_protocol *protocol);
  void			(*err_handler)(int type, int code, unsigned char *buff,
				       __u32 daddr,
				       __u32 saddr,
				       struct inet_protocol *protocol);
  struct inet_protocol *next;
  unsigned char		protocol;
  unsigned char		copy:1;
  void			*data;
  const char		*name;
};


extern struct inet_protocol *inet_protocol_base;
extern struct inet_protocol *inet_protos[MAX_INET_PROTOS];


extern void		inet_add_protocol(struct inet_protocol *prot);
extern int		inet_del_protocol(struct inet_protocol *prot);


#endif	/* _PROTOCOL_H */
