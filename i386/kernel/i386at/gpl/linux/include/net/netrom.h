/*
 *	Declarations of NET/ROM type objects.
 *
 *	Jonathan Naylor G4KLX	9/4/95
 */
 
#ifndef _NETROM_H
#define _NETROM_H 
#include <linux/netrom.h>

#define	NR_T1CLAMPLO   (1 * PR_SLOWHZ)	/* If defined, clamp at 1 second **/
#define	NR_T1CLAMPHI (300 * PR_SLOWHZ)	/* If defined, clamp at 30 seconds **/

#define	NR_NETWORK_LEN		15
#define	NR_TRANSPORT_LEN	5
 
#define	NR_PROTO_IP		0x0C

#define	NR_PROTOEXT		0x00
#define	NR_CONNREQ		0x01
#define	NR_CONNACK		0x02
#define	NR_DISCREQ		0x03
#define	NR_DISCACK		0x04
#define	NR_INFO			0x05
#define	NR_INFOACK		0x06

#define	NR_CHOKE_FLAG		0x80
#define	NR_NAK_FLAG		0x40
#define	NR_MORE_FLAG		0x20

/* Define Link State constants. */

#define NR_STATE_0		0
#define NR_STATE_1		1
#define NR_STATE_2		2
#define NR_STATE_3		3

#define NR_DEFAULT_T1		(120 * PR_SLOWHZ)	/* Outstanding frames - 120 seconds */
#define NR_DEFAULT_T2		(5   * PR_SLOWHZ)	/* Response delay     - 5 seconds */
#define NR_DEFAULT_N2		3			/* Number of Retries */
#define	NR_DEFAULT_T4		(180 * PR_SLOWHZ)	/* Transport Busy Delay */
#define	NR_DEFAULT_WINDOW	4			/* Default Window Size	*/
#define	NR_DEFAULT_OBS		6			/* Default Obscolesence Count */
#define	NR_DEFAULT_QUAL		10			/* Default Neighbour Quality */
#define	NR_DEFAULT_TTL		16			/* Default Time To Live */
#define NR_MODULUS 		256
#define NR_MAX_WINDOW_SIZE	127			/* Maximum Window Allowable */

typedef struct {
	ax25_address		user_addr, source_addr, dest_addr;
	struct device		*device;
	unsigned char		my_index,   my_id;
	unsigned char		your_index, your_id;
	unsigned char		state, condition, bpqext, hdrincl;
	unsigned short		vs, vr, va, vl;
	unsigned char		n2, n2count;
	unsigned short		t1, t2, rtt;
	unsigned short		t1timer, t2timer, t4timer;
	unsigned short		fraglen;
	struct sk_buff_head	ack_queue;
	struct sk_buff_head	reseq_queue;
	struct sk_buff_head	frag_queue;
	struct sock		*sk;		/* Backlink to socket */
} nr_cb;

struct nr_route {
	unsigned char  quality;
	unsigned char  obs_count;
	unsigned short neighbour;
};

struct nr_node {
	struct nr_node  *next;
	ax25_address    callsign;
	char mnemonic[7];
	unsigned char   which;
	unsigned char   count;
	struct nr_route routes[3];
};

struct nr_neigh {
	struct nr_neigh *next;
	ax25_address    callsign;
	ax25_digi       *digipeat;
	struct device   *dev;
	unsigned char   quality;
	unsigned char   locked;
	unsigned short  count;
	unsigned short  number;
};

/* af_netrom.c */
extern struct nr_parms_struct nr_default;
extern int  nr_rx_frame(struct sk_buff *, struct device *);
extern void nr_destroy_socket(struct sock *);

/* nr_dev.c */
extern int  nr_rx_ip(struct sk_buff *, struct device *);
extern int  nr_init(struct device *);

#include <net/nrcall.h>

/* nr_in.c */
extern int  nr_process_rx_frame(struct sock *, struct sk_buff *);

/* nr_out.c */
extern void nr_output(struct sock *, struct sk_buff *);
extern void nr_send_nak_frame(struct sock *);
extern void nr_kick(struct sock *);
extern void nr_transmit_buffer(struct sock *, struct sk_buff *);
extern void nr_establish_data_link(struct sock *);
extern void nr_enquiry_response(struct sock *);
extern void nr_check_iframes_acked(struct sock *, unsigned short);

/* nr_route.c */
extern void nr_rt_device_down(struct device *);
extern struct device *nr_dev_first(void);
extern struct device *nr_dev_get(ax25_address *);
extern int  nr_rt_ioctl(unsigned int, void *);
extern void nr_link_failed(ax25_address *, struct device *);
extern int  nr_route_frame(struct sk_buff *, ax25_cb *);
extern int  nr_nodes_get_info(char *, char **, off_t, int, int);
extern int  nr_neigh_get_info(char *, char **, off_t, int, int);

/* nr_subr.c */
extern void nr_clear_queues(struct sock *);
extern void nr_frames_acked(struct sock *, unsigned short);
extern void nr_requeue_frames(struct sock *);
extern int  nr_validate_nr(struct sock *, unsigned short);
extern int  nr_in_rx_window(struct sock *, unsigned short);
extern void nr_write_internal(struct sock *, int);
extern void nr_transmit_dm(struct sk_buff *);
extern unsigned short nr_calculate_t1(struct sock *);
extern void nr_calculate_rtt(struct sock *);

/* ax25_timer */
extern void nr_set_timer(struct sock *);

#endif
