/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Global definitions for the Token-Ring IEEE 802.5 interface.
 *
 * Version:	@(#)if_tr.h	0.0	07/11/94
 *
 * Author:	Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald Becker, <becker@super.org>
 *    Peter De Schrijver, <stud11@cc4.kuleuven.ac.be>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IF_TR_H
#define _LINUX_IF_TR_H


/* IEEE 802.5 Token-Ring magic constants.  The frame sizes omit the preamble
   and FCS/CRC (frame check sequence). */
#define TR_ALEN	6		/* Octets in one ethernet addr	 */
#define TR_HLEN   (sizeof(struct trh_hdr)+sizeof(struct trllc))
#define AC			0x10
#define LLC_FRAME 0x40
#if 0
#define ETH_HLEN	14		/* Total octets in header.	 */
#define ETH_ZLEN	60		/* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
#define ETH_FRAME_LEN	1514		/* Max. octets in frame sans FCS */
#endif


/* These are some defined Ethernet Protocol ID's. */
#define ETH_P_IP	0x0800		/* Internet Protocol packet	*/
#define ETH_P_ARP	0x0806		/* Address Resolution packet	*/
#define ETH_P_RARP      0x8035		/* Reverse Addr Res packet	*/

/* LLC and SNAP constants */
#define EXTENDED_SAP 0xAA
#define UI_CMD       0x03

/* This is an Token-Ring frame header. */
struct trh_hdr {
	unsigned char   ac;	/* access control field */
	unsigned char   fc;	/* frame control field */
	unsigned char   daddr[TR_ALEN];	/* destination address */
	unsigned char   saddr[TR_ALEN];	/* source address */
	unsigned short  rcf;	/* route control field */
	unsigned short  rseg[8];/* routing registers */
};

/* This is an Token-Ring LLC structure */
struct trllc {
	unsigned char   dsap;	/* destination SAP */
	unsigned char   ssap;	/* source SAP */
	unsigned char   llc;	/* LLC control field */
	unsigned char   protid[3];	/* protocol id */
	unsigned short  ethertype;	/* ether type field */
};


/* Token-Ring statistics collection data. */
struct tr_statistics{
  int	rx_packets;			/* total packets received	*/
  int	tx_packets;			/* total packets transmitted	*/
  int	rx_errors;			/* bad packets received		*/
  int	tx_errors;			/* packet transmit problems	*/
  int	rx_dropped;			/* no space in linux buffers	*/
  int	tx_dropped;			/* no space available in linux	*/
  int	multicast;			/* multicast packets received	*/
  int   transmit_collision;

	/* detailed Token-Ring errors. See IBM Token-Ring Network Architecture
      for more info */

	int line_errors;
	int internal_errors;
	int burst_errors;
	int A_C_errors;
	int abort_delimiters;
	int lost_frames;
	int recv_congest_count;
	int frame_copied_errors;
	int frequency_errors;
	int token_errors;
	int dummy1;
	
};

/* source routing stuff */

#define TR_RII 0x80
#define TR_RCF_DIR_BIT 0x80
#define TR_RCF_LEN_MASK 0x1f00
#define TR_RCF_BROADCAST 0x8000
#define TR_RCF_LIMITED_BROADCAST 0xA000
#define TR_RCF_FRAME2K 0x20
#define TR_RCF_BROADCAST_MASK 0xC000

#endif	/* _LINUX_IF_TR_H */
