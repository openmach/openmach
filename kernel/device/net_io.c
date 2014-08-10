 /* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	3/98
 *
 *	Network IO.
 *
 *	Packet filter code taken from vaxif/enet.c written		 
 *		CMU and Stanford. 
 */

/*
 *	Note:  don't depend on anything in this file.
 *	It may change a lot real soon.	-cmaeda 11 June 1993
 */

#include <norma_ether.h>
#include <mach_ttd.h>

#include <sys/types.h>
#include <device/net_status.h>
#include <machine/machspl.h>		/* spl definitions */
#include <device/net_io.h>
#include <device/if_hdr.h>
#include <device/io_req.h>
#include <device/ds_routines.h>

#include <mach/boolean.h>
#include <mach/vm_param.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_mqueue.h>

#include <kern/counters.h>
#include <kern/lock.h>
#include <kern/queue.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>

#if	NORMA_ETHER
#include <norma/ipc_ether.h>
#endif	/*NORMA_ETHER*/

#include <machine/machspl.h>

#if	MACH_TTD
#include <ttd/ttd_stub.h>
#endif	/* MACH_TTD */

#if	MACH_TTD
int kttd_async_counter= 0;
#endif	/* MACH_TTD */


/*
 *	Packet Buffer Management
 *
 *	This module manages a private pool of kmsg buffers.
 */

/*
 * List of net kmsgs queued to be sent to users.
 * Messages can be high priority or low priority.
 * The network thread processes high priority messages first.
 */
decl_simple_lock_data(,net_queue_lock)
boolean_t	net_thread_awake = FALSE;
struct ipc_kmsg_queue	net_queue_high;
int		net_queue_high_size = 0;
int		net_queue_high_max = 0;		/* for debugging */
struct ipc_kmsg_queue	net_queue_low;
int		net_queue_low_size = 0;
int		net_queue_low_max = 0;		/* for debugging */

/*
 * List of net kmsgs that can be touched at interrupt level.
 * If it is empty, we will also steal low priority messages.
 */
decl_simple_lock_data(,net_queue_free_lock)
struct ipc_kmsg_queue	net_queue_free;
int		net_queue_free_size = 0;	/* on free list */
int		net_queue_free_max = 0;		/* for debugging */

/*
 * This value is critical to network performance.
 * At least this many buffers should be sitting in net_queue_free.
 * If this is set too small, we will drop network packets.
 * Even a low drop rate (<1%) can cause severe network throughput problems.
 * We add one to net_queue_free_min for every filter.
 */
int		net_queue_free_min = 3;

int		net_queue_free_hits = 0;	/* for debugging */
int		net_queue_free_steals = 0;	/* for debugging */
int		net_queue_free_misses = 0;	/* for debugging */

int		net_kmsg_send_high_hits = 0;	/* for debugging */
int		net_kmsg_send_low_hits = 0;	/* for debugging */
int		net_kmsg_send_high_misses = 0;	/* for debugging */
int		net_kmsg_send_low_misses = 0;	/* for debugging */

int		net_thread_awaken = 0;		/* for debugging */
int		net_ast_taken = 0;		/* for debugging */

decl_simple_lock_data(,net_kmsg_total_lock)
int		net_kmsg_total = 0;		/* total allocated */
int		net_kmsg_max;			/* initialized below */

vm_size_t	net_kmsg_size;			/* initialized below */

/*
 *	We want more buffers when there aren't enough in the free queue
 *	and the low priority queue.  However, we don't want to allocate
 *	more than net_kmsg_max.
 */

#define net_kmsg_want_more()		\
	(((net_queue_free_size + net_queue_low_size) < net_queue_free_min) && \
	 (net_kmsg_total < net_kmsg_max))

ipc_kmsg_t
net_kmsg_get(void)
{
	register ipc_kmsg_t kmsg;
	spl_t s;

	/*
	 *	First check the list of free buffers.
	 */
	s = splimp();
	simple_lock(&net_queue_free_lock);
	kmsg = ipc_kmsg_queue_first(&net_queue_free);
	if (kmsg != IKM_NULL) {
	    ipc_kmsg_rmqueue_first_macro(&net_queue_free, kmsg);
	    net_queue_free_size--;
	    net_queue_free_hits++;
	}
	simple_unlock(&net_queue_free_lock);

	if (kmsg == IKM_NULL) {
	    /*
	     *	Try to steal from the low priority queue.
	     */
	    simple_lock(&net_queue_lock);
	    kmsg = ipc_kmsg_queue_first(&net_queue_low);
	    if (kmsg != IKM_NULL) {
		ipc_kmsg_rmqueue_first_macro(&net_queue_low, kmsg);
		net_queue_low_size--;
		net_queue_free_steals++;
	    }
	    simple_unlock(&net_queue_lock);
	}

	if (kmsg == IKM_NULL)
	    net_queue_free_misses++;
	(void) splx(s);

	if (net_kmsg_want_more() || (kmsg == IKM_NULL)) {
	    boolean_t awake;

	    s = splimp();
	    simple_lock(&net_queue_lock);
	    awake = net_thread_awake;
	    net_thread_awake = TRUE;
	    simple_unlock(&net_queue_lock);
	    (void) splx(s);

	    if (!awake)
		thread_wakeup((event_t) &net_thread_awake);
	}

	return kmsg;
}

void
net_kmsg_put(register ipc_kmsg_t kmsg)
{
	spl_t s;

	s = splimp();
	simple_lock(&net_queue_free_lock);
	ipc_kmsg_enqueue_macro(&net_queue_free, kmsg);
	if (++net_queue_free_size > net_queue_free_max)
	    net_queue_free_max = net_queue_free_size;
	simple_unlock(&net_queue_free_lock);
	(void) splx(s);
}

void
net_kmsg_collect(void)
{
	register ipc_kmsg_t kmsg;
	spl_t s;

	s = splimp();
	simple_lock(&net_queue_free_lock);
	while (net_queue_free_size > net_queue_free_min) {
	    kmsg = ipc_kmsg_dequeue(&net_queue_free);
	    net_queue_free_size--;
	    simple_unlock(&net_queue_free_lock);
	    (void) splx(s);

	    net_kmsg_free(kmsg);
	    simple_lock(&net_kmsg_total_lock);
	    net_kmsg_total--;
	    simple_unlock(&net_kmsg_total_lock);

	    s = splimp();
	    simple_lock(&net_queue_free_lock);
	}
	simple_unlock(&net_queue_free_lock);
	(void) splx(s);
}

void
net_kmsg_more(void)
{
	register ipc_kmsg_t kmsg;

	/*
	 * Replenish net kmsg pool if low.  We don't have the locks
	 * necessary to look at these variables, but that's OK because
	 * misread values aren't critical.  The danger in this code is
	 * that while we allocate buffers, interrupts are happening
	 * which take buffers out of the free list.  If we are not
	 * careful, we will sit in the loop and allocate a zillion
	 * buffers while a burst of packets arrives.  So we count
	 * buffers in the low priority queue as available, because
	 * net_kmsg_get will make use of them, and we cap the total
	 * number of buffers we are willing to allocate.
	 */

	while (net_kmsg_want_more()) {
	    simple_lock(&net_kmsg_total_lock);
	    net_kmsg_total++;
	    simple_unlock(&net_kmsg_total_lock);
	    kmsg = net_kmsg_alloc();
	    net_kmsg_put(kmsg);
	}
}

/*
 *	Packet Filter Data Structures
 *
 *	Each network interface has a set of packet filters
 *	that are run on incoming packets.
 *
 *	Each packet filter may represent a single network
 *	session or multiple network sessions.  For example,
 *	all application level TCP sessions would be represented
 *	by a single packet filter data structure.
 *	
 *	If a packet filter has a single session, we use a
 *	struct net_rcv_port to represent it.  If the packet
 *	filter represents multiple sessions, we use a 
 *	struct net_hash_header to represent it.
 */

/*
 * Each interface has a write port and a set of read ports.
 * Each read port has one or more filters to determine what packets
 * should go to that port.
 */

/*
 * Receive port for net, with packet filter.
 * This data structure by itself represents a packet
 * filter for a single session.
 */
struct net_rcv_port {
	queue_chain_t	chain;		/* list of open_descriptors */
	ipc_port_t	rcv_port;	/* port to send packet to */
	int		rcv_qlimit;	/* port's qlimit */
	int		rcv_count;	/* number of packets received */
	int		priority;	/* priority for filter */
	filter_t	*filter_end;	/* pointer to end of filter */
	filter_t	filter[NET_MAX_FILTER];
					/* filter operations */
};
typedef struct net_rcv_port *net_rcv_port_t;

zone_t		net_rcv_zone;	/* zone of net_rcv_port structs */


#define NET_HASH_SIZE   256
#define N_NET_HASH      4
#define N_NET_HASH_KEYS 4

unsigned int bpf_hash (int, unsigned int *);

/*
 * A single hash entry.
 */
struct net_hash_entry {
	queue_chain_t   chain;	        /* list of entries with same hval */
#define he_next chain.next
#define he_prev chain.prev
	ipc_port_t      rcv_port;	/* destination port */
	int             rcv_qlimit;	/* qlimit for the port */
	unsigned int	keys[N_NET_HASH_KEYS];
};
typedef struct net_hash_entry *net_hash_entry_t;

zone_t  net_hash_entry_zone;

/*
 * This structure represents a packet filter with multiple sessions.
 *
 * For example, all application level TCP sessions might be
 * represented by one of these structures.  It looks like a 
 * net_rcv_port struct so that both types can live on the
 * same packet filter queues.
 */
struct net_hash_header {
	struct net_rcv_port rcv;
        int n_keys;			/* zero if not used */
        int ref_count;			/* reference count */
        net_hash_entry_t table[NET_HASH_SIZE];
} filter_hash_header[N_NET_HASH];

typedef struct net_hash_header *net_hash_header_t;

decl_simple_lock_data(,net_hash_header_lock)

#define HASH_ITERATE(head, elt) (elt) = (net_hash_entry_t) (head); do {
#define HASH_ITERATE_END(head, elt) \
	(elt) = (net_hash_entry_t) queue_next((queue_entry_t) (elt));	   \
	} while ((elt) != (head));


#define FILTER_ITERATE(ifp, fp, nextfp) \
	for ((fp) = (net_rcv_port_t) queue_first(&(ifp)->if_rcv_port_list);\
	     !queue_end(&(ifp)->if_rcv_port_list, (queue_entry_t)(fp));    \
	     (fp) = (nextfp)) {						   \
		(nextfp) = (net_rcv_port_t) queue_next(&(fp)->chain);
#define FILTER_ITERATE_END }

/* entry_p must be net_rcv_port_t or net_hash_entry_t */
#define ENQUEUE_DEAD(dead, entry_p) { \
	queue_next(&(entry_p)->chain) = (queue_entry_t) (dead);	\
	(dead) = (queue_entry_t)(entry_p);			\
}

extern boolean_t net_do_filter();	/* CSPF */
extern int bpf_do_filter();		/* BPF */


/*
 *	ethernet_priority:
 *
 *	This function properly belongs in the ethernet interfaces;
 *	it should not be called by this module.  (We get packet
 *	priorities as an argument to net_filter.)  It is here
 *	to avoid massive code duplication.
 *
 *	Returns TRUE for high-priority packets.
 */

boolean_t ethernet_priority(kmsg)
	ipc_kmsg_t kmsg;
{
	register unsigned char *addr =
		(unsigned char *) net_kmsg(kmsg)->header;

	/*
	 *	A simplistic check for broadcast packets.
	 */

	if ((addr[0] == 0xff) && (addr[1] == 0xff) &&
	    (addr[2] == 0xff) && (addr[3] == 0xff) &&
	    (addr[4] == 0xff) && (addr[5] == 0xff))
	    return FALSE;
	else
	    return TRUE;
}

mach_msg_type_t header_type = {
	MACH_MSG_TYPE_BYTE,
	8,
	NET_HDW_HDR_MAX,
	TRUE,
	FALSE,
	FALSE,
	0
};

mach_msg_type_t packet_type = {
	MACH_MSG_TYPE_BYTE,	/* name */
	8,			/* size */
	0,			/* number */
	TRUE,			/* inline */
	FALSE,			/* longform */
	FALSE			/* deallocate */
};

/*
 *	net_deliver:
 *
 *	Called and returns holding net_queue_lock, at splimp.
 *	Dequeues a message and delivers it at spl0.
 *	Returns FALSE if no messages.
 */
boolean_t net_deliver(nonblocking)
	boolean_t nonblocking;
{
	register ipc_kmsg_t kmsg;
	boolean_t high_priority;
	struct ipc_kmsg_queue send_list;

	/*
	 * Pick up a pending network message and deliver it.
	 * Deliver high priority messages before low priority.
	 */

	if ((kmsg = ipc_kmsg_dequeue(&net_queue_high)) != IKM_NULL) {
	    net_queue_high_size--;
	    high_priority = TRUE;
	} else if ((kmsg = ipc_kmsg_dequeue(&net_queue_low)) != IKM_NULL) {
	    net_queue_low_size--;
	    high_priority = FALSE;
	} else
	    return FALSE;
	simple_unlock(&net_queue_lock);
	(void) spl0();

	/*
	 * Run the packet through the filters,
	 * getting back a queue of packets to send.
	 */
	net_filter(kmsg, &send_list);

	if (!nonblocking) {
	    /*
	     * There is a danger of running out of available buffers
	     * because they all get moved into the high priority queue
	     * or a port queue.  In particular, we might need to
	     * allocate more buffers as we pull (previously available)
	     * buffers out of the low priority queue.  But we can only
	     * allocate if we are allowed to block.
	     */
	    net_kmsg_more();
	}

	while ((kmsg = ipc_kmsg_dequeue(&send_list)) != IKM_NULL) {
	    int count;

	    /*
	     * Fill in the rest of the kmsg.
	     */
	    count = net_kmsg(kmsg)->net_rcv_msg_packet_count;

	    ikm_init_special(kmsg, IKM_SIZE_NETWORK);

	    kmsg->ikm_header.msgh_bits =
		    MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND, 0);
	    /* remember message sizes must be rounded up */
	    kmsg->ikm_header.msgh_size =
		    ((mach_msg_size_t) (sizeof(struct net_rcv_msg)
					- NET_RCV_MAX + count))+3 &~ 3;
	    kmsg->ikm_header.msgh_local_port = MACH_PORT_NULL;
	    kmsg->ikm_header.msgh_kind = MACH_MSGH_KIND_NORMAL;
	    kmsg->ikm_header.msgh_id = NET_RCV_MSG_ID;

	    net_kmsg(kmsg)->header_type = header_type;
	    net_kmsg(kmsg)->packet_type = packet_type;
	    net_kmsg(kmsg)->net_rcv_msg_packet_count = count;

	    /*
	     * Send the packet to the destination port.  Drop it
	     * if the destination port is over its backlog.
	     */

	    if (ipc_mqueue_send(kmsg, MACH_SEND_TIMEOUT, 0) ==
						    MACH_MSG_SUCCESS) {
		if (high_priority)
		    net_kmsg_send_high_hits++;
		else
		    net_kmsg_send_low_hits++;
		/* the receiver is responsible for the message now */
	    } else {
		if (high_priority)
		    net_kmsg_send_high_misses++;
		else
		    net_kmsg_send_low_misses++;
		ipc_kmsg_destroy(kmsg);
	    }
	}

	(void) splimp();
	simple_lock(&net_queue_lock);
	return TRUE;
}

/*
 *	We want to deliver packets using ASTs, so we can avoid the
 *	thread_wakeup/thread_block needed to get to the network
 *	thread.  However, we can't allocate memory in the AST handler,
 *	because memory allocation might block.  Hence we have the
 *	network thread to allocate memory.  The network thread also
 *	delivers packets, so it can be allocating and delivering for a
 *	burst.  net_thread_awake is protected by net_queue_lock
 *	(instead of net_queue_free_lock) so that net_packet and
 *	net_ast can safely determine if the network thread is running.
 *	This prevents a race that might leave a packet sitting without
 *	being delivered.  It is possible for net_kmsg_get to think
 *	the network thread is awake, and so avoid a wakeup, and then
 *	have the network thread sleep without allocating.  The next
 *	net_kmsg_get will do a wakeup.
 */

void net_ast()
{
	spl_t s;

	net_ast_taken++;

	/*
	 *	If the network thread is awake, then we would
	 *	rather deliver messages from it, because
	 *	it can also allocate memory.
	 */

	s = splimp();
	simple_lock(&net_queue_lock);
	while (!net_thread_awake && net_deliver(TRUE))
		continue;

	/*
	 *	Prevent an unnecessary AST.  Either the network
	 *	thread will deliver the messages, or there are
	 *	no messages left to deliver.
	 */

	simple_unlock(&net_queue_lock);
	(void) splsched();
	ast_off(cpu_number(), AST_NETWORK);
	(void) splx(s);
}

void net_thread_continue()
{
	for (;;) {
		spl_t s;

		net_thread_awaken++;

		/*
		 *	First get more buffers.
		 */
		net_kmsg_more();

		s = splimp();
		simple_lock(&net_queue_lock);
		while (net_deliver(FALSE))
			continue;

		net_thread_awake = FALSE;
		assert_wait(&net_thread_awake, FALSE);
		simple_unlock(&net_queue_lock);
		(void) splx(s);
		counter(c_net_thread_block++);
		thread_block(net_thread_continue);
	}
}

void net_thread()
{
	spl_t s;

	/*
	 *	We should be very high priority.
	 */

	thread_set_own_priority(0);

	/*
	 *	We sleep initially, so that we don't allocate any buffers
	 *	unless the network is really in use and they are needed.
	 */

	s = splimp();
	simple_lock(&net_queue_lock);
	net_thread_awake = FALSE;
	assert_wait(&net_thread_awake, FALSE);
	simple_unlock(&net_queue_lock);
	(void) splx(s);
	counter(c_net_thread_block++);
	thread_block(net_thread_continue);
	net_thread_continue();
	/*NOTREACHED*/
}

void
reorder_queue(first, last)
	register queue_t	first, last;
{
	register queue_entry_t	prev, next;

	prev = first->prev;
	next = last->next;

	prev->next = last;
	next->prev = first;

	last->prev = prev;
	last->next = first;

	first->next = next;
	first->prev = last;
}

/*
 * Incoming packet.  Header has already been moved to proper place.
 * We are already at splimp.
 */
void
net_packet(ifp, kmsg, count, priority)
	register struct ifnet	*ifp;
	register ipc_kmsg_t	kmsg;
	unsigned int		count;
	boolean_t		priority;
{
	boolean_t awake;

#if	NORMA_ETHER
	if (netipc_net_packet(kmsg, count)) {
		return;
	}
#endif	NORMA_ETHER

#if	MACH_TTD
	/*
	 * Do a quick check to see if it is a kernel TTD packet.
	 *
	 * Only check if KernelTTD is enabled, ie. the current
	 * device driver supports TTD, and the bootp succeded.
	 */
	if (kttd_enabled && kttd_handle_async(kmsg)) {
		/* 
		 * Packet was a valid ttd packet and
		 * doesn't need to be passed up to filter.
		 * The ttd code put the used kmsg buffer
		 * back onto the free list.
		 */
		if (kttd_debug)
			printf("**%x**", kttd_async_counter++);
		return;
	}
#endif	/* MACH_TTD */

	kmsg->ikm_header.msgh_remote_port = (mach_port_t) ifp;
	net_kmsg(kmsg)->net_rcv_msg_packet_count = count;

	simple_lock(&net_queue_lock);
	if (priority) {
	    ipc_kmsg_enqueue(&net_queue_high, kmsg);
	    if (++net_queue_high_size > net_queue_high_max)
		net_queue_high_max = net_queue_high_size;
	} else {
	    ipc_kmsg_enqueue(&net_queue_low, kmsg);
	    if (++net_queue_low_size > net_queue_low_max)
		net_queue_low_max = net_queue_low_size;
	}
	/*
	 *	If the network thread is awake, then we don't
	 *	need to take an AST, because the thread will
	 *	deliver the packet.
	 */
	awake = net_thread_awake;
	simple_unlock(&net_queue_lock);

	if (!awake) {
	    spl_t s = splsched();
	    ast_on(cpu_number(), AST_NETWORK);
	    (void) splx(s);
	}
}

int net_filter_queue_reorder = 0; /* non-zero to enable reordering */

/*
 * Run a packet through the filters, returning a list of messages.
 * We are *not* called at interrupt level.
 */
void
net_filter(kmsg, send_list)
	register ipc_kmsg_t	kmsg;
	ipc_kmsg_queue_t	send_list;
{
	register struct ifnet	*ifp;
	register net_rcv_port_t	infp, nextfp;
	register ipc_kmsg_t	new_kmsg;

 	net_hash_entry_t	entp, *hash_headp;
 	ipc_port_t		dest;
 	queue_entry_t		dead_infp = (queue_entry_t) 0;
 	queue_entry_t		dead_entp = (queue_entry_t) 0;
 	unsigned int		ret_count;

	int count = net_kmsg(kmsg)->net_rcv_msg_packet_count;
	ifp = (struct ifnet *) kmsg->ikm_header.msgh_remote_port;
	ipc_kmsg_queue_init(send_list);

	/*
	 * Unfortunately we can't allocate or deallocate memory
	 * while holding this lock.  And we can't drop the lock
	 * while examining the filter list.
	 */
	simple_lock(&ifp->if_rcv_port_list_lock);
 	FILTER_ITERATE(ifp, infp, nextfp)
 	{
 	    entp = (net_hash_entry_t) 0;
 	    if (infp->filter[0] == NETF_BPF) {
 		ret_count = bpf_do_filter(infp, net_kmsg(kmsg)->packet, count,
 					  net_kmsg(kmsg)->header,
 					  &hash_headp, &entp);
		if (entp == (net_hash_entry_t) 0)
		  dest = infp->rcv_port;
		else
		  dest = entp->rcv_port;
 	    } else {
 		ret_count = net_do_filter(infp, net_kmsg(kmsg)->packet, count,
 					  net_kmsg(kmsg)->header);
 		if (ret_count)
 		    ret_count = count;
 		dest = infp->rcv_port;
 	    }		    

 	    if (ret_count) {

		/*
		 * Make a send right for the destination.
		 */

 		dest = ipc_port_copy_send(dest);
		if (!IP_VALID(dest)) {
		    /*
		     * This filter is dead.  We remove it from the
		     * filter list and set it aside for deallocation.
		     */

 		    if (entp == (net_hash_entry_t) 0) {
 			queue_remove(&ifp->if_rcv_port_list, infp,
 				     net_rcv_port_t, chain);
 			ENQUEUE_DEAD(dead_infp, infp);
 			continue;
 		    } else {
 			hash_ent_remove (ifp,
 					 (net_hash_header_t)infp,
 					 FALSE,		/* no longer used */
 					 hash_headp,
 					 entp,
 					 &dead_entp);
 			continue;
 		    }
		}

		/*
		 * Deliver copy of packet to this channel.
		 */
		if (ipc_kmsg_queue_empty(send_list)) {
		    /*
		     * Only receiver, so far
		     */
		    new_kmsg = kmsg;
		} else {
		    /*
		     * Other receivers - must allocate message and copy.
		     */
		    new_kmsg = net_kmsg_get();
		    if (new_kmsg == IKM_NULL) {
			ipc_port_release_send(dest);
			break;
		    }

		    bcopy(
			net_kmsg(kmsg)->packet,
			net_kmsg(new_kmsg)->packet,
			ret_count);
		    bcopy(
			net_kmsg(kmsg)->header,
			net_kmsg(new_kmsg)->header,
			NET_HDW_HDR_MAX);
		}
 		net_kmsg(new_kmsg)->net_rcv_msg_packet_count = ret_count;
		new_kmsg->ikm_header.msgh_remote_port = (mach_port_t) dest;
		ipc_kmsg_enqueue(send_list, new_kmsg);

	    {
		register net_rcv_port_t prevfp;
		int rcount = ++infp->rcv_count;

		/*
		 * See if ordering of filters is wrong
		 */
		if (infp->priority >= NET_HI_PRI) {
		    prevfp = (net_rcv_port_t) queue_prev(&infp->chain);
		    /*
		     * If infp is not the first element on the queue,
		     * and the previous element is at equal priority
		     * but has a lower count, then promote infp to
		     * be in front of prevfp.
		     */
		    if ((queue_t)prevfp != &ifp->if_rcv_port_list &&
			infp->priority == prevfp->priority) {
			/*
			 * Threshold difference to prevent thrashing
			 */
			if (net_filter_queue_reorder
			    && (100 + prevfp->rcv_count < rcount))
				reorder_queue(&prevfp->chain, &infp->chain);
		    }
		    /*
		     * High-priority filter -> no more deliveries
		     */
		    break;
		}
	    }
	    }
	}
	FILTER_ITERATE_END

	simple_unlock(&ifp->if_rcv_port_list_lock);

	/*
	 * Deallocate dead filters.
	 */
 	if (dead_infp != 0)
 		net_free_dead_infp(dead_infp);
 	if (dead_entp != 0)
 		net_free_dead_entp(dead_entp);

	if (ipc_kmsg_queue_empty(send_list)) {
	    /* Not sent - recycle */
	    net_kmsg_put(kmsg);
	}
}

boolean_t
net_do_filter(infp, data, data_count, header)
	net_rcv_port_t	infp;
	char *		data;
	unsigned int	data_count;
	char *		header;
{
	int		stack[NET_FILTER_STACK_DEPTH+1];
	register int	*sp;
	register filter_t	*fp, *fpe;
	register unsigned int	op, arg;

	/*
	 * The filter accesses the header and data
	 * as unsigned short words.
	 */
	data_count /= sizeof(unsigned short);

#define	data_word	((unsigned short *)data)
#define	header_word	((unsigned short *)header)

	sp = &stack[NET_FILTER_STACK_DEPTH];
	fp = &infp->filter[0];
	fpe = infp->filter_end;

	*sp = TRUE;

	while (fp < fpe) {
	    arg = *fp++;
	    op = NETF_OP(arg);
	    arg = NETF_ARG(arg);

	    switch (arg) {
		case NETF_NOPUSH:
		    arg = *sp++;
		    break;
		case NETF_PUSHZERO:
		    arg = 0;
		    break;
		case NETF_PUSHLIT:
		    arg = *fp++;
		    break;
		case NETF_PUSHIND:
		    arg = *sp++;
		    if (arg >= data_count)
			return FALSE;
		    arg = data_word[arg];
		    break;
		case NETF_PUSHHDRIND:
		    arg = *sp++;
		    if (arg >= NET_HDW_HDR_MAX/sizeof(unsigned short))
			return FALSE;
		    arg = header_word[arg];
		    break;
		default:
		    if (arg >= NETF_PUSHSTK) {
			arg = sp[arg - NETF_PUSHSTK];
		    }
		    else if (arg >= NETF_PUSHHDR) {
			arg = header_word[arg - NETF_PUSHHDR];
		    }
		    else {
			arg -= NETF_PUSHWORD;
			if (arg >= data_count)
			    return FALSE;
			arg = data_word[arg];
		    }
		    break;

	    }
	    switch (op) {
		case NETF_OP(NETF_NOP):
		    *--sp = arg;
		    break;
		case NETF_OP(NETF_AND):
		    *sp &= arg;
		    break;
		case NETF_OP(NETF_OR):
		    *sp |= arg;
		    break;
		case NETF_OP(NETF_XOR):
		    *sp ^= arg;
		    break;
		case NETF_OP(NETF_EQ):
		    *sp = (*sp == arg);
		    break;
		case NETF_OP(NETF_NEQ):
		    *sp = (*sp != arg);
		    break;
		case NETF_OP(NETF_LT):
		    *sp = (*sp < arg);
		    break;
		case NETF_OP(NETF_LE):
		    *sp = (*sp <= arg);
		    break;
		case NETF_OP(NETF_GT):
		    *sp = (*sp > arg);
		    break;
		case NETF_OP(NETF_GE):
		    *sp = (*sp >= arg);
		    break;
		case NETF_OP(NETF_COR):
		    if (*sp++ == arg)
			return (TRUE);
		    break;
		case NETF_OP(NETF_CAND):
		    if (*sp++ != arg)
			return (FALSE);
		    break;
		case NETF_OP(NETF_CNOR):
		    if (*sp++ == arg)
			return (FALSE);
		    break;
		case NETF_OP(NETF_CNAND):
		    if (*sp++ != arg)
			return (TRUE);
		    break;
		case NETF_OP(NETF_LSH):
		    *sp <<= arg;
		    break;
		case NETF_OP(NETF_RSH):
		    *sp >>= arg;
		    break;
		case NETF_OP(NETF_ADD):
		    *sp += arg;
		    break;
		case NETF_OP(NETF_SUB):
		    *sp -= arg;
		    break;
	    }
	}
	return ((*sp) ? TRUE : FALSE);

#undef	data_word
#undef	header_word
}

/*
 * Check filter for invalid operations or stack over/under-flow.
 */
boolean_t
parse_net_filter(filter, count)
	register filter_t	*filter;
	unsigned int		count;
{
	register int	sp;
	register filter_t	*fpe = &filter[count];
	register filter_t	op, arg;

	sp = NET_FILTER_STACK_DEPTH;

	for (; filter < fpe; filter++) {
	    op = NETF_OP(*filter);
	    arg = NETF_ARG(*filter);

	    switch (arg) {
		case NETF_NOPUSH:
		    break;
		case NETF_PUSHZERO:
		    sp--;
		    break;
		case NETF_PUSHLIT:
		    filter++;
		    if (filter >= fpe)
			return (FALSE);	/* literal value not in filter */
		    sp--;
		    break;
		case NETF_PUSHIND:
		case NETF_PUSHHDRIND:
		    break;
		default:
		    if (arg >= NETF_PUSHSTK) {
			if (arg - NETF_PUSHSTK + sp > NET_FILTER_STACK_DEPTH)
			    return FALSE;
		    }
		    else if (arg >= NETF_PUSHHDR) {
			if (arg - NETF_PUSHHDR >=
				NET_HDW_HDR_MAX/sizeof(unsigned short))
			    return FALSE;
		    }
		    /* else... cannot check for packet bounds
				without packet */
		    sp--;
		    break;
	    }
	    if (sp < 2) {
		return (FALSE);	/* stack overflow */
	    }
	    if (op == NETF_OP(NETF_NOP))
		continue;

	    /*
	     * all non-NOP operators are binary.
	     */
	    if (sp > NET_MAX_FILTER-2)
		return (FALSE);

	    sp++;
	    switch (op) {
		case NETF_OP(NETF_AND):
		case NETF_OP(NETF_OR):
		case NETF_OP(NETF_XOR):
		case NETF_OP(NETF_EQ):
		case NETF_OP(NETF_NEQ):
		case NETF_OP(NETF_LT):
		case NETF_OP(NETF_LE):
		case NETF_OP(NETF_GT):
		case NETF_OP(NETF_GE):
		case NETF_OP(NETF_COR):
		case NETF_OP(NETF_CAND):
		case NETF_OP(NETF_CNOR):
		case NETF_OP(NETF_CNAND):
		case NETF_OP(NETF_LSH):
		case NETF_OP(NETF_RSH):
		case NETF_OP(NETF_ADD):
		case NETF_OP(NETF_SUB):
		    break;
		default:
		    return (FALSE);
	    }
	}
	return (TRUE);
}

/*
 * Set a filter for a network interface.
 *
 * We are given a naked send right for the rcv_port.
 * If we are successful, we must consume that right.
 */
io_return_t
net_set_filter(ifp, rcv_port, priority, filter, filter_count)
	struct ifnet	*ifp;
	ipc_port_t	rcv_port;
	int		priority;
	filter_t	*filter;
	unsigned int	filter_count;
{
    int				filter_bytes;
    bpf_insn_t			match;
    register net_rcv_port_t	infp, my_infp;
    net_rcv_port_t		nextfp;
    net_hash_header_t		hhp;
    register net_hash_entry_t	entp, hash_entp;
    net_hash_entry_t		*head, nextentp;
    queue_entry_t		dead_infp, dead_entp;
    int				i;
    int				ret, is_new_infp;
    io_return_t			rval;

    /*
     * Check the filter syntax.
     */

    filter_bytes = CSPF_BYTES(filter_count);
    match = (bpf_insn_t) 0;

    if (filter_count > 0 && filter[0] == NETF_BPF) {
	ret = bpf_validate((bpf_insn_t)filter, filter_bytes, &match);
	if (!ret)
	    return (D_INVALID_OPERATION);
    } else {
	if (!parse_net_filter(filter, filter_count))
	    return (D_INVALID_OPERATION);
    }

    rval = D_SUCCESS;			/* default return value */
    dead_infp = dead_entp = 0;

    if (match == (bpf_insn_t) 0) {
        /*
	 * If there is no match instruction, we allocate
	 * a normal packet filter structure.
	 */
	my_infp = (net_rcv_port_t) zalloc(net_rcv_zone);
	my_infp->rcv_port = rcv_port;
	is_new_infp = TRUE;
    } else {
        /*
	 * If there is a match instruction, we assume there will
	 * multiple session with a common substructure and allocate
	 * a hash table to deal with them.
	 */
	my_infp = 0;
	hash_entp = (net_hash_entry_t) zalloc(net_hash_entry_zone);
	is_new_infp = FALSE;
    }    

    /*
     * Look for an existing filter on the same reply port.
     * Look for filters with dead ports (for GC).
     * Look for a filter with the same code except KEY insns.
     */
    
    simple_lock(&ifp->if_rcv_port_list_lock);
    
    FILTER_ITERATE(ifp, infp, nextfp)
    {
	    if (infp->rcv_port == MACH_PORT_NULL) {
		    if (match != 0
			&& infp->priority == priority
			&& my_infp == 0
			&& (infp->filter_end - infp->filter) == filter_count
			&& bpf_eq((bpf_insn_t)infp->filter,
				  filter, filter_bytes))
			    {
				    my_infp = infp;
			    }

		    for (i = 0; i < NET_HASH_SIZE; i++) {
			    head = &((net_hash_header_t) infp)->table[i];
			    if (*head == 0)
				    continue;

			    /*
			     * Check each hash entry to make sure the
			     * destination port is still valid.  Remove
			     * any invalid entries.
			     */
			    entp = *head;
			    do {
				    nextentp = (net_hash_entry_t) entp->he_next;
  
				    /* checked without 
				       ip_lock(entp->rcv_port) */
				    if (entp->rcv_port == rcv_port
					|| !IP_VALID(entp->rcv_port)
					|| !ip_active(entp->rcv_port)) {
				
					    ret = hash_ent_remove (ifp,
						(net_hash_header_t)infp,
						(my_infp == infp),
						head,
						entp,
						&dead_entp);
					    if (ret)
						    goto hash_loop_end;
				    }
			
				    entp = nextentp;
			    /* While test checks head since hash_ent_remove
			       might modify it.
			       */
			    } while (*head != 0 && entp != *head);
		    }
		hash_loop_end:
		    ;
		    
	    } else if (infp->rcv_port == rcv_port
		       || !IP_VALID(infp->rcv_port)
		       || !ip_active(infp->rcv_port)) {
		    /* Remove the old filter from list */
		    remqueue(&ifp->if_rcv_port_list, (queue_entry_t)infp);
		    ENQUEUE_DEAD(dead_infp, infp);
	    }
    }
    FILTER_ITERATE_END

    if (my_infp == 0) {
	/* Allocate a dummy infp */
	simple_lock(&net_hash_header_lock);
	for (i = 0; i < N_NET_HASH; i++) {
	    if (filter_hash_header[i].n_keys == 0)
		break;
	}
	if (i == N_NET_HASH) {
	    simple_unlock(&net_hash_header_lock);
	    simple_unlock(&ifp->if_rcv_port_list_lock);

            ipc_port_release_send(rcv_port);
	    if (match != 0)
		    zfree (net_hash_entry_zone, (vm_offset_t)hash_entp);

	    rval = D_NO_MEMORY;
	    goto clean_and_return;
	}

	hhp = &filter_hash_header[i];
	hhp->n_keys = match->jt;
	simple_unlock(&net_hash_header_lock);

	hhp->ref_count = 0;
	for (i = 0; i < NET_HASH_SIZE; i++)
	    hhp->table[i] = 0;

	my_infp = (net_rcv_port_t)hhp;
	my_infp->rcv_port = MACH_PORT_NULL;	/* indication of dummy */
	is_new_infp = TRUE;
    }

    if (is_new_infp) {
	my_infp->priority = priority;
	my_infp->rcv_count = 0;

	/* Copy filter program. */
	bcopy ((vm_offset_t)filter, (vm_offset_t)my_infp->filter,
	       filter_bytes);
	my_infp->filter_end =
	    (filter_t *)((char *)my_infp->filter + filter_bytes);

	if (match == 0) {
	    my_infp->rcv_qlimit = net_add_q_info(rcv_port);
	} else {
	    my_infp->rcv_qlimit = 0;
	}

	/* Insert my_infp according to priority */
	queue_iterate(&ifp->if_rcv_port_list, infp, net_rcv_port_t, chain)
	    if (priority > infp->priority)
		break;
	enqueue_tail((queue_t)&infp->chain, (queue_entry_t)my_infp);
    }
    
    if (match != 0)
    {	    /* Insert to hash list */
	net_hash_entry_t *p;
	int j;
	
	hash_entp->rcv_port = rcv_port;
	for (i = 0; i < match->jt; i++)		/* match->jt is n_keys */
	    hash_entp->keys[i] = match[i+1].k;
	p = &((net_hash_header_t)my_infp)->
			table[bpf_hash(match->jt, hash_entp->keys)];
	
	/* Not checking for the same key values */
	if (*p == 0) {
	    queue_init ((queue_t) hash_entp);
	    *p = hash_entp;
	} else {
	    enqueue_tail((queue_t)*p, hash_entp);
	}

	((net_hash_header_t)my_infp)->ref_count++;
	hash_entp->rcv_qlimit = net_add_q_info(rcv_port);

    }
    
    simple_unlock(&ifp->if_rcv_port_list_lock);

clean_and_return:
    /* No locks are held at this point. */

    if (dead_infp != 0)
	    net_free_dead_infp(dead_infp);
    if (dead_entp != 0)
	    net_free_dead_entp(dead_entp);
    
    return (rval);
}

/*
 * Other network operations
 */
io_return_t
net_getstat(ifp, flavor, status, count)
	struct ifnet	*ifp;
	dev_flavor_t	flavor;
	dev_status_t	status;		/* pointer to OUT array */
	natural_t	*count;		/* OUT */
{
	switch (flavor) {
	    case NET_STATUS:
	    {
		register struct net_status *ns = (struct net_status *)status;

		if (*count < NET_STATUS_COUNT)
		    return (D_INVALID_OPERATION);
		
		ns->min_packet_size = ifp->if_header_size;
		ns->max_packet_size = ifp->if_header_size + ifp->if_mtu;
		ns->header_format   = ifp->if_header_format;
		ns->header_size	    = ifp->if_header_size;
		ns->address_size    = ifp->if_address_size;
		ns->flags	    = ifp->if_flags;
		ns->mapped_size	    = 0;

		*count = NET_STATUS_COUNT;
		break;
	    }
	    case NET_ADDRESS:
	    {
		register int	addr_byte_count;
		register int	addr_int_count;
		register int	i;

		addr_byte_count = ifp->if_address_size;
		addr_int_count = (addr_byte_count + (sizeof(int)-1))
					 / sizeof(int);

		if (*count < addr_int_count)
		{
/* XXX debug hack. */
printf ("net_getstat: count: %d, addr_int_count: %d\n",
		*count, addr_int_count);
		    return (D_INVALID_OPERATION);
		}

		bcopy((char *)ifp->if_address,
		      (char *)status,
		      (unsigned) addr_byte_count);
		if (addr_byte_count < addr_int_count * sizeof(int))
		    bzero((char *)status + addr_byte_count,
			  (unsigned) (addr_int_count * sizeof(int)
				      - addr_byte_count));

		for (i = 0; i < addr_int_count; i++) {
		    register int word;

		    word = status[i];
		    status[i] = htonl(word);
		}
		*count = addr_int_count;
		break;
	    }
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

io_return_t
net_write(ifp, start, ior)
	register struct ifnet *ifp;
	int		(*start)();
	io_req_t	ior;
{
	spl_t	s;
	kern_return_t	rc;
	boolean_t	wait;

	/*
	 * Reject the write if the interface is down.
	 */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
	    return (D_DEVICE_DOWN);

	/*
	 * Reject the write if the packet is too large or too small.
	 */
	if (ior->io_count < ifp->if_header_size ||
	    ior->io_count > ifp->if_header_size + ifp->if_mtu)
	    return (D_INVALID_SIZE);

	/*
	 * Wire down the memory.
	 */

	rc = device_write_get(ior, &wait);
	if (rc != KERN_SUCCESS)
	    return (rc);

	/*
	 *	Network interfaces can't cope with VM continuations.
	 *	If wait is set, just panic.
	*/
	if (wait) {
		panic("net_write: VM continuation");
	}

	/*
	 * Queue the packet on the output queue, and
	 * start the device.
	 */
	s = splimp();
	IF_ENQUEUE(&ifp->if_snd, ior);
	(*start)(ifp->if_unit);
	splx(s);
	
	return (D_IO_QUEUED);
}

#ifdef FIPC
/* This gets called by nefoutput for dev_ops->d_port_death ... */

io_return_t
net_fwrite(ifp, start, ior)
	register struct ifnet *ifp;
	int		(*start)();
	io_req_t	ior;
{
	spl_t	s;
	kern_return_t	rc;
	boolean_t	wait;

	/*
	 * Reject the write if the interface is down.
	 */
	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
	    return (D_DEVICE_DOWN);

	/*
	 * Reject the write if the packet is too large or too small.
	 */
	if (ior->io_count < ifp->if_header_size ||
	    ior->io_count > ifp->if_header_size + ifp->if_mtu)
	    	return (D_INVALID_SIZE);

	/*
	 * DON'T Wire down the memory.
	 */
#if 0
	rc = device_write_get(ior, &wait);
	if (rc != KERN_SUCCESS)
	    return (rc);
#endif
	/*
	 *	Network interfaces can't cope with VM continuations.
	 *	If wait is set, just panic.
	*/
	/* I'll have to figure out who was setting wait...*/
#if 0
	if (wait) {
		panic("net_write: VM continuation");
	}
#endif
	/*
	 * Queue the packet on the output queue, and
	 * start the device.
	 */
	s = splimp();
	IF_ENQUEUE(&ifp->if_snd, ior);
	(*start)(ifp->if_unit);
	splx(s);
	
	return (D_IO_QUEUED);
}
#endif /* FIPC */

/*
 * Initialize the whole package.
 */
void
net_io_init()
{
	register vm_size_t	size;

	size = sizeof(struct net_rcv_port);
	net_rcv_zone = zinit(size,
			     size * 1000,
			     PAGE_SIZE,
			     FALSE,
			     "net_rcv_port");

 	size = sizeof(struct net_hash_entry);
 	net_hash_entry_zone = zinit(size,
 				    size * 100,
 				    PAGE_SIZE,
 				    FALSE,
 				    "net_hash_entry");

	size = ikm_plus_overhead(sizeof(struct net_rcv_msg));
	net_kmsg_size = round_page(size);

	/*
	 *	net_kmsg_max caps the number of buffers
	 *	we are willing to allocate.  By default,
	 *	we allow for net_queue_free_min plus
	 *	the queue limit for each filter.
	 *	(Added as the filters are added.)
	 */

	simple_lock_init(&net_kmsg_total_lock);
	if (net_kmsg_max == 0)
	    net_kmsg_max = net_queue_free_min;

	simple_lock_init(&net_queue_free_lock);
	ipc_kmsg_queue_init(&net_queue_free);

	simple_lock_init(&net_queue_lock);
	ipc_kmsg_queue_init(&net_queue_high);
	ipc_kmsg_queue_init(&net_queue_low);

 	simple_lock_init(&net_hash_header_lock);
}


/* ======== BPF: Berkeley Packet Filter ======== */

/*-
 * Copyright (c) 1990-1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence 
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bpf.c	7.5 (Berkeley) 7/15/91
 */

#if defined(sparc) || defined(mips) || defined(ibm032) || defined(alpha)
#define BPF_ALIGN
#endif

#ifndef BPF_ALIGN
#define EXTRACT_SHORT(p)	((u_short)ntohs(*(u_short *)p))
#define EXTRACT_LONG(p)		(ntohl(*(u_long *)p))
#else
#define EXTRACT_SHORT(p)\
	((u_short)\
		((u_short)*((u_char *)p+0)<<8|\
		 (u_short)*((u_char *)p+1)<<0))
#define EXTRACT_LONG(p)\
		((u_long)*((u_char *)p+0)<<24|\
		 (u_long)*((u_char *)p+1)<<16|\
		 (u_long)*((u_char *)p+2)<<8|\
		 (u_long)*((u_char *)p+3)<<0)
#endif

/*
 * Execute the filter program starting at pc on the packet p
 * wirelen is the length of the original packet
 * buflen is the amount of data present
 */

int
bpf_do_filter(infp, p, wirelen, header, hash_headpp, entpp)
	net_rcv_port_t	infp;
	char *		p;		/* packet data */
	unsigned int	wirelen;	/* data_count (in bytes) */
	char *		header;
	net_hash_entry_t	**hash_headpp, *entpp;	/* out */
{
	register bpf_insn_t pc, pc_end;
	register unsigned int buflen;

	register unsigned long A, X;
	register int k;
	long mem[BPF_MEMWORDS];

	pc = ((bpf_insn_t) infp->filter) + 1;
					/* filter[0].code is BPF_BEGIN */
	pc_end = (bpf_insn_t)infp->filter_end;
	buflen = NET_RCV_MAX;
	*entpp = 0;			/* default */

#ifdef lint
	A = 0;
	X = 0;
#endif
	for (; pc < pc_end; ++pc) {
		switch (pc->code) {

		default:
#ifdef KERNEL
			return 0;
#else
			abort();
#endif			
		case BPF_RET|BPF_K:
			if (infp->rcv_port == MACH_PORT_NULL &&
			    *entpp == 0) {
				return 0;
			}
			return ((u_int)pc->k <= wirelen) ?
						pc->k : wirelen;

		case BPF_RET|BPF_A:
			if (infp->rcv_port == MACH_PORT_NULL &&
			    *entpp == 0) {
				return 0;
			}
			return ((u_int)A <= wirelen) ?
						A : wirelen;

		case BPF_RET|BPF_MATCH_IMM:
			if (bpf_match ((net_hash_header_t)infp, pc->jt, mem,
				       hash_headpp, entpp)) {
				return ((u_int)pc->k <= wirelen) ?
							pc->k : wirelen;
			}
			return 0;

		case BPF_LD|BPF_W|BPF_ABS:
			k = pc->k;
			if ((u_int)k + sizeof(long) <= buflen) {
#ifdef BPF_ALIGN
				if (((int)(p + k) & 3) != 0)
					A = EXTRACT_LONG(&p[k]);
				else
#endif
					A = ntohl(*(long *)(p + k));
				continue;
			}

			k -= BPF_DLBASE;
			if ((u_int)k + sizeof(long) <= NET_HDW_HDR_MAX) {
#ifdef BPF_ALIGN
				if (((int)(header + k) & 3) != 0)
					A = EXTRACT_LONG(&header[k]);
				else
#endif
					A = ntohl(*(long *)(header + k));
				continue;
			} else {
				return 0;
			}

		case BPF_LD|BPF_H|BPF_ABS:
			k = pc->k;
			if ((u_int)k + sizeof(short) <= buflen) {
				A = EXTRACT_SHORT(&p[k]);
				continue;
			}

			k -= BPF_DLBASE;
			if ((u_int)k + sizeof(short) <= NET_HDW_HDR_MAX) {
				A = EXTRACT_SHORT(&header[k]);
				continue;
			} else {
				return 0;
			}

		case BPF_LD|BPF_B|BPF_ABS:
			k = pc->k;
			if ((u_int)k < buflen) {
				A = p[k];
				continue;
			}
			
			k -= BPF_DLBASE;
			if ((u_int)k < NET_HDW_HDR_MAX) {
				A = header[k];
				continue;
			} else {
				return 0;
			}

		case BPF_LD|BPF_W|BPF_LEN:
			A = wirelen;
			continue;

		case BPF_LDX|BPF_W|BPF_LEN:
			X = wirelen;
			continue;

		case BPF_LD|BPF_W|BPF_IND:
			k = X + pc->k;
			if (k + sizeof(long) > buflen)
				return 0;
#ifdef BPF_ALIGN
			if (((int)(p + k) & 3) != 0)
				A = EXTRACT_LONG(&p[k]);
			else
#endif
				A = ntohl(*(long *)(p + k));
			continue;

		case BPF_LD|BPF_H|BPF_IND:
			k = X + pc->k;
			if (k + sizeof(short) > buflen)
				return 0;
			A = EXTRACT_SHORT(&p[k]);
			continue;

		case BPF_LD|BPF_B|BPF_IND:
			k = X + pc->k;
			if (k >= buflen)
				return 0;
			A = p[k];
			continue;

		case BPF_LDX|BPF_MSH|BPF_B:
			k = pc->k;
			if (k >= buflen)
				return 0;
			X = (p[pc->k] & 0xf) << 2;
			continue;

		case BPF_LD|BPF_IMM:
			A = pc->k;
			continue;

		case BPF_LDX|BPF_IMM:
			X = pc->k;
			continue;

		case BPF_LD|BPF_MEM:
			A = mem[pc->k];
			continue;
			
		case BPF_LDX|BPF_MEM:
			X = mem[pc->k];
			continue;

		case BPF_ST:
			mem[pc->k] = A;
			continue;

		case BPF_STX:
			mem[pc->k] = X;
			continue;

		case BPF_JMP|BPF_JA:
			pc += pc->k;
			continue;

		case BPF_JMP|BPF_JGT|BPF_K:
			pc += (A > pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_K:
			pc += (A >= pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_K:
			pc += (A == pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_K:
			pc += (A & pc->k) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGT|BPF_X:
			pc += (A > X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JGE|BPF_X:
			pc += (A >= X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JEQ|BPF_X:
			pc += (A == X) ? pc->jt : pc->jf;
			continue;

		case BPF_JMP|BPF_JSET|BPF_X:
			pc += (A & X) ? pc->jt : pc->jf;
			continue;

		case BPF_ALU|BPF_ADD|BPF_X:
			A += X;
			continue;
			
		case BPF_ALU|BPF_SUB|BPF_X:
			A -= X;
			continue;
			
		case BPF_ALU|BPF_MUL|BPF_X:
			A *= X;
			continue;
			
		case BPF_ALU|BPF_DIV|BPF_X:
			if (X == 0)
				return 0;
			A /= X;
			continue;
			
		case BPF_ALU|BPF_AND|BPF_X:
			A &= X;
			continue;
			
		case BPF_ALU|BPF_OR|BPF_X:
			A |= X;
			continue;

		case BPF_ALU|BPF_LSH|BPF_X:
			A <<= X;
			continue;

		case BPF_ALU|BPF_RSH|BPF_X:
			A >>= X;
			continue;

		case BPF_ALU|BPF_ADD|BPF_K:
			A += pc->k;
			continue;
			
		case BPF_ALU|BPF_SUB|BPF_K:
			A -= pc->k;
			continue;
			
		case BPF_ALU|BPF_MUL|BPF_K:
			A *= pc->k;
			continue;
			
		case BPF_ALU|BPF_DIV|BPF_K:
			A /= pc->k;
			continue;
			
		case BPF_ALU|BPF_AND|BPF_K:
			A &= pc->k;
			continue;
			
		case BPF_ALU|BPF_OR|BPF_K:
			A |= pc->k;
			continue;

		case BPF_ALU|BPF_LSH|BPF_K:
			A <<= pc->k;
			continue;

		case BPF_ALU|BPF_RSH|BPF_K:
			A >>= pc->k;
			continue;

		case BPF_ALU|BPF_NEG:
			A = -A;
			continue;

		case BPF_MISC|BPF_TAX:
			X = A;
			continue;

		case BPF_MISC|BPF_TXA:
			A = X;
			continue;
		}
	}

	return 0;
}

/*
 * Return 1 if the 'f' is a valid filter program without a MATCH
 * instruction. Return 2 if it is a valid filter program with a MATCH
 * instruction. Otherwise, return 0.
 * The constraints are that each jump be forward and to a valid
 * code.  The code must terminate with either an accept or reject. 
 * 'valid' is an array for use by the routine (it must be at least
 * 'len' bytes long).  
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.
 */
int
bpf_validate(f, bytes, match)
	bpf_insn_t f;
	int bytes;
	bpf_insn_t *match;
{
	register int i, j, len;
	register bpf_insn_t p;

	len = BPF_BYTES2LEN(bytes);
	/* f[0].code is already checked to be BPF_BEGIN. So skip f[0]. */

	for (i = 1; i < len; ++i) {
		/*
		 * Check that that jumps are forward, and within 
		 * the code block.
		 */
		p = &f[i];
		if (BPF_CLASS(p->code) == BPF_JMP) {
			register int from = i + 1;

			if (BPF_OP(p->code) == BPF_JA) {
				if (from + p->k >= len)
					return 0;
			}
			else if (from + p->jt >= len || from + p->jf >= len)
				return 0;
		}
		/*
		 * Check that memory operations use valid addresses.
		 */
		if ((BPF_CLASS(p->code) == BPF_ST ||
		     (BPF_CLASS(p->code) == BPF_LD && 
		      (p->code & 0xe0) == BPF_MEM)) &&
		    (p->k >= BPF_MEMWORDS || p->k < 0))
			return 0;
		/*
		 * Check for constant division by 0.
		 */
		if (p->code == (BPF_ALU|BPF_DIV|BPF_K) && p->k == 0)
			return 0;
		/*
		 * Check for match instruction.
		 * Only one match instruction per filter is allowed.
		 */
		if (p->code == (BPF_RET|BPF_MATCH_IMM)) {
			if (*match != 0 ||
			    p->jt == 0 ||
			    p->jt > N_NET_HASH_KEYS)
				return 0;
			i += p->jt;		/* skip keys */
			if (i + 1 > len)
				return 0;

			for (j = 1; j <= p->jt; j++) {
			    if (p[j].code != (BPF_MISC|BPF_KEY))
				return 0;
			}

			*match = p;
		}
	}
	if (BPF_CLASS(f[len - 1].code) == BPF_RET)
		return ((*match == 0) ? 1 : 2);
	else
		return 0;
}

int
bpf_eq (f1, f2, bytes)
	register bpf_insn_t f1, f2;
	register int bytes;
{
	register int count;

	count = BPF_BYTES2LEN(bytes);
	for (; count--; f1++, f2++) {
		if (!BPF_INSN_EQ(f1, f2)) {
			if ( f1->code == (BPF_MISC|BPF_KEY) &&
			     f2->code == (BPF_MISC|BPF_KEY) )
				continue;
			return FALSE;
		}
	};
	return TRUE;
}

unsigned int
bpf_hash (n, keys)
	register int n;
	register unsigned int *keys;
{
	register unsigned int hval = 0;
	
	while (n--) {
		hval += *keys++;
	}
	return (hval % NET_HASH_SIZE);
}


int
bpf_match (hash, n_keys, keys, hash_headpp, entpp)
	net_hash_header_t hash;
	register int n_keys;
	register unsigned int *keys;
	net_hash_entry_t **hash_headpp, *entpp;
{
	register net_hash_entry_t head, entp;
	register int i;

	if (n_keys != hash->n_keys)
		return FALSE;

	*hash_headpp = &hash->table[bpf_hash(n_keys, keys)];
	head = **hash_headpp;

	if (head == 0)
		return FALSE;

	HASH_ITERATE (head, entp)
	{
		for (i = 0; i < n_keys; i++) {
			if (keys[i] != entp->keys[i])
				break;
		}
		if (i == n_keys) {
			*entpp = entp;
			return TRUE;
		}
	}
	HASH_ITERATE_END (head, entp)
	return FALSE;
}	


/*
 * Removes a hash entry (ENTP) from its queue (HEAD).
 * If the reference count of filter (HP) becomes zero and not USED,
 * HP is removed from ifp->if_rcv_port_list and is freed.
 */

int
hash_ent_remove (ifp, hp, used, head, entp, dead_p)
    struct ifnet	*ifp;
    net_hash_header_t 	hp;
    int			used;
    net_hash_entry_t	*head, entp;
    queue_entry_t	*dead_p;
{    
	hp->ref_count--;

	if (*head == entp) {

		if (queue_empty((queue_t) entp)) {
			*head = 0;
			ENQUEUE_DEAD(*dead_p, entp);
			if (hp->ref_count == 0 && !used) {
				remqueue((queue_t) &ifp->if_rcv_port_list,
					 (queue_entry_t)hp);
				hp->n_keys = 0;
				return TRUE;
			}
			return FALSE;
		} else {
			*head = (net_hash_entry_t)queue_next((queue_t) entp);
		}
	}

	remqueue((queue_t)*head, (queue_entry_t)entp);
	ENQUEUE_DEAD(*dead_p, entp);
	return FALSE;
}    

int
net_add_q_info (rcv_port)
	ipc_port_t	rcv_port;
{
	mach_port_msgcount_t qlimit = 0;
	    
	/*
	 * We use a new port, so increase net_queue_free_min
	 * and net_kmsg_max to allow for more queued messages.
	 */
	    
	if (IP_VALID(rcv_port)) {
		ip_lock(rcv_port);
		if (ip_active(rcv_port))
			qlimit = rcv_port->ip_qlimit;
		ip_unlock(rcv_port);
	}
	    
	simple_lock(&net_kmsg_total_lock);
	net_queue_free_min++;
	net_kmsg_max += qlimit + 1;
	simple_unlock(&net_kmsg_total_lock);

	return (int)qlimit;
}

net_del_q_info (qlimit)
	int qlimit;
{
	simple_lock(&net_kmsg_total_lock);
	net_queue_free_min--;
	net_kmsg_max -= qlimit + 1;
	simple_unlock(&net_kmsg_total_lock);
}


/*
 * net_free_dead_infp (dead_infp)
 *	queue_entry_t dead_infp;	list of dead net_rcv_port_t.
 *
 * Deallocates dead net_rcv_port_t.
 * No locks should be held when called.
 */
net_free_dead_infp (dead_infp)
	queue_entry_t dead_infp;
{
	register net_rcv_port_t infp, nextfp;

	for (infp = (net_rcv_port_t) dead_infp; infp != 0; infp = nextfp)
	{
		nextfp = (net_rcv_port_t) queue_next(&infp->chain);
		ipc_port_release_send(infp->rcv_port);
		net_del_q_info(infp->rcv_qlimit);
		zfree(net_rcv_zone, (vm_offset_t) infp);
	}	    
}
    
/*
 * net_free_dead_entp (dead_entp)
 *	queue_entry_t dead_entp;	list of dead net_hash_entry_t.
 *
 * Deallocates dead net_hash_entry_t.
 * No locks should be held when called.
 */
net_free_dead_entp (dead_entp)
	queue_entry_t dead_entp;
{
	register net_hash_entry_t entp, nextentp;

	for (entp = (net_hash_entry_t)dead_entp; entp != 0; entp = nextentp)
	{
		nextentp = (net_hash_entry_t) queue_next(&entp->chain);

		ipc_port_release_send(entp->rcv_port);
		net_del_q_info(entp->rcv_qlimit);
		zfree(net_hash_entry_zone, (vm_offset_t) entp);
	}
}

