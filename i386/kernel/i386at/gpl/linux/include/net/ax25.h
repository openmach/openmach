/*
 *	Declarations of AX.25 type objects.
 *
 *	Alan Cox (GW4PTS) 	10/11/93
 */
 
#ifndef _AX25_H
#define _AX25_H 
#include <linux/ax25.h>

#define PR_SLOWHZ	10		/*  Run timing at 1/10 second - gives us better resolution for 56kbit links */

#define	AX25_T1CLAMPLO  (1 * PR_SLOWHZ)	/* If defined, clamp at 1 second **/
#define	AX25_T1CLAMPHI (30 * PR_SLOWHZ)	/* If defined, clamp at 30 seconds **/

#define	AX25_BROKEN_NETMAC

#define	AX25_BPQ_HEADER_LEN	16
#define	AX25_KISS_HEADER_LEN	1

#define	AX25_HEADER_LEN		17
#define	AX25_ADDR_LEN		7
#define	AX25_DIGI_HEADER_LEN	(AX25_MAX_DIGIS * AX25_ADDR_LEN)
#define	AX25_MAX_HEADER_LEN	(AX25_HEADER_LEN + AX25_DIGI_HEADER_LEN)
 
#define AX25_P_IP	0xCC
#define AX25_P_ARP	0xCD
#define AX25_P_TEXT 	0xF0
#define AX25_P_NETROM 	0xCF
#define	AX25_P_SEGMENT	0x08

#define	SEG_REM		0x7F
#define	SEG_FIRST	0x80

#define LAPB_UI		0x03
#define LAPB_C		0x80
#define LAPB_E		0x01

#define SSSID_SPARE	0x60	/* Unused bits in SSID for standard AX.25 */
#define ESSID_SPARE	0x20	/* Unused bits in SSID for extended AX.25 */
#define DAMA_FLAG	0x40	/* Well, it is *NOT* unused! (dl1bke 951121 */

#define AX25_REPEATED	0x80

#define	ACK_PENDING_CONDITION		0x01
#define	REJECT_CONDITION		0x02
#define	PEER_RX_BUSY_CONDITION		0x04
#define	OWN_RX_BUSY_CONDITION		0x08

#ifndef _LINUX_NETDEVICE_H
#include <linux/netdevice.h>
#endif

/*
 * These headers are taken from the KA9Q package by Phil Karn. These specific
 * files have been placed under the GPL (not the whole package) by Phil.
 *
 *
 * Copyright 1991 Phil Karn, KA9Q
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 dated June, 1991.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program;  if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave., Cambridge, MA 02139, USA.
 */

/* Upper sub-layer (LAPB) definitions */

/* Control field templates */
#define	I	0x00	/* Information frames */
#define	S	0x01	/* Supervisory frames */
#define	RR	0x01	/* Receiver ready */
#define	RNR	0x05	/* Receiver not ready */
#define	REJ	0x09	/* Reject */
#define	U	0x03	/* Unnumbered frames */
#define	SABM	0x2f	/* Set Asynchronous Balanced Mode */
#define	SABME	0x6f	/* Set Asynchronous Balanced Mode Extended */
#define	DISC	0x43	/* Disconnect */
#define	DM	0x0f	/* Disconnected mode */
#define	UA	0x63	/* Unnumbered acknowledge */
#define	FRMR	0x87	/* Frame reject */
#define	UI	0x03	/* Unnumbered information */
#define	PF	0x10	/* Poll/final bit for standard AX.25 */
#define	EPF	0x01	/* Poll/final bit for extended AX.25 */

#define ILLEGAL	0x100	/* Impossible to be a real frame type */

#define	POLLOFF		0
#define	POLLON		1

/* AX25 L2 C-bit */

#define C_COMMAND	1	/* C_ otherwise it clashes with the de600 defines (sigh)) */
#define C_RESPONSE	2

/* Define Link State constants. */

#define AX25_STATE_0	0
#define AX25_STATE_1	1
#define AX25_STATE_2	2
#define AX25_STATE_3	3
#define AX25_STATE_4	4

#define MODULUS 	8			/*  Standard AX.25 modulus */
#define	EMODULUS	128			/*  Extended AX.25 modulus */

#define	AX25_DEF_IPDEFMODE	'D'
#define	AX25_DEF_AXDEFMODE	8
#define	AX25_DEF_NETROM		1
#define	AX25_DEF_TEXT		1
#define	AX25_DEF_BACKOFF	'E'
#define	AX25_DEF_CONMODE	1
#define	AX25_DEF_WINDOW		2
#define	AX25_DEF_EWINDOW	32
#define	AX25_DEF_T1		10
#define	AX25_DEF_T2		3
#define	AX25_DEF_T3		300
#define	AX25_DEF_N2		10
#define	AX25_DEF_DIGI		(AX25_DIGI_INBAND|AX25_DIGI_XBAND)

typedef struct ax25_uid_assoc {
	struct ax25_uid_assoc *next;
	uid_t uid;
	ax25_address call;
} ax25_uid_assoc;

typedef struct {
	ax25_address calls[AX25_MAX_DIGIS];
	unsigned char repeated[AX25_MAX_DIGIS];
	unsigned char ndigi;
	char lastrepeat;
} ax25_digi;

typedef struct ax25_cb {
	struct ax25_cb		*next;
	ax25_address		source_addr, dest_addr;
	struct device		*device;
	unsigned char		dama_slave;	/* dl1bke 951121 */
	unsigned char		state, modulus, hdrincl;
	unsigned short		vs, vr, va;
	unsigned char		condition, backoff;
	unsigned char		n2, n2count;
	unsigned short		t1, t2, t3, rtt;
	unsigned short		t1timer, t2timer, t3timer;
	unsigned short		fragno, fraglen;
	ax25_digi		*digipeat;
	struct sk_buff_head	write_queue;
	struct sk_buff_head	reseq_queue;
	struct sk_buff_head	ack_queue;
	struct sk_buff_head	frag_queue;
	unsigned char		window;
	struct timer_list	timer;
	struct sock		*sk;		/* Backlink to socket */
} ax25_cb;

/* af_ax25.c */
extern ax25_address null_ax25_address;
extern char *ax2asc(ax25_address *);
extern int  ax25cmp(ax25_address *, ax25_address *);
extern int  ax25_send_frame(struct sk_buff *, ax25_address *, ax25_address *, ax25_digi *, struct device *);
extern void ax25_destroy_socket(ax25_cb *);
extern struct device *ax25rtr_get_dev(ax25_address *);
extern int  ax25_encapsulate(struct sk_buff *, struct device *, unsigned short,
	void *, void *, unsigned int);
extern int  ax25_rebuild_header(unsigned char *, struct device *, unsigned long, struct sk_buff *);
extern ax25_uid_assoc *ax25_uid_list;
extern int  ax25_uid_policy;
extern ax25_address *ax25_findbyuid(uid_t);
extern void ax25_queue_xmit(struct sk_buff *, struct device *, int);
extern int  ax25_dev_is_dama_slave(struct device *);	/* dl1bke 951121 */

#include <net/ax25call.h>

/* ax25_in.c */
extern int  ax25_process_rx_frame(ax25_cb *, struct sk_buff *, int, int);

/* ax25_out.c */
extern void ax25_output(ax25_cb *, struct sk_buff *);
extern void ax25_kick(ax25_cb *);
extern void ax25_transmit_buffer(ax25_cb *, struct sk_buff *, int);
extern void ax25_nr_error_recovery(ax25_cb *);
extern void ax25_establish_data_link(ax25_cb *);
extern void ax25_transmit_enquiry(ax25_cb *);
extern void ax25_enquiry_response(ax25_cb *);
extern void ax25_timeout_response(ax25_cb *);
extern void ax25_check_iframes_acked(ax25_cb *, unsigned short);
extern void ax25_check_need_response(ax25_cb *, int, int);
extern void dama_enquiry_response(ax25_cb *);			/* dl1bke 960114 */
extern void dama_check_need_response(ax25_cb *, int, int);	/* dl1bke 960114 */
extern void dama_establish_data_link(ax25_cb *);

/* ax25_route.c */
extern void ax25_rt_rx_frame(ax25_address *, struct device *, ax25_digi *);
extern int  ax25_rt_get_info(char *, char **, off_t, int, int);
extern int  ax25_cs_get_info(char *, char **, off_t, int, int);
extern int  ax25_rt_autobind(ax25_cb *, ax25_address *);
extern void ax25_rt_build_path(ax25_cb *, ax25_address *);
extern void ax25_dg_build_path(struct sk_buff *, ax25_address *, struct device *);
extern void ax25_rt_device_down(struct device *);
extern int  ax25_rt_ioctl(unsigned int, void *);
extern void ax25_ip_mode_set(ax25_address *, struct device *, char);
extern char ax25_ip_mode_get(ax25_address *, struct device *);
extern unsigned short ax25_dev_get_value(struct device *, int);
extern void ax25_dev_device_up(struct device *);
extern void ax25_dev_device_down(struct device *);
extern int  ax25_dev_ioctl(unsigned int, void *);
extern int  ax25_bpq_get_info(char *, char **, off_t, int, int);
extern ax25_address *ax25_bpq_get_addr(struct device *);
extern int  ax25_bpq_ioctl(unsigned int, void *);

/* ax25_subr.c */
extern void ax25_clear_queues(ax25_cb *);
extern void ax25_frames_acked(ax25_cb *, unsigned short);
extern void ax25_requeue_frames(ax25_cb *);
extern int  ax25_validate_nr(ax25_cb *, unsigned short);
extern int  ax25_decode(ax25_cb *, struct sk_buff *, int *, int *, int *);
extern void ax25_send_control(ax25_cb *, int, int, int);
extern unsigned short ax25_calculate_t1(ax25_cb *);
extern void ax25_calculate_rtt(ax25_cb *);
extern unsigned char *ax25_parse_addr(unsigned char *, int, ax25_address *,
	ax25_address *, ax25_digi *, int *, int *);	/* dl1bke 951121 */
extern int  build_ax25_addr(unsigned char *, ax25_address *, ax25_address *,
	ax25_digi *, int, int);
extern int  size_ax25_addr(ax25_digi *);
extern void ax25_digi_invert(ax25_digi *, ax25_digi *);
extern void ax25_return_dm(struct device *, ax25_address *, ax25_address *, ax25_digi *);
extern void ax25_dama_on(ax25_cb *);	/* dl1bke 951121 */
extern void ax25_dama_off(ax25_cb *);	/* dl1bke 951121 */

/* ax25_timer */
extern void ax25_set_timer(ax25_cb *);
extern void ax25_t1_timeout(ax25_cb *);

/* ... */

extern ax25_cb * volatile ax25_list;

#endif
