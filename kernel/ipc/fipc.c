/*
 * Copyright (c) 1996-1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *  Utah $Hdr: fipc.c 1.1 96/2/29$
 *  Author: Linus Kamb
 */

#ifdef FIPC

#include <mach/kern_return.h>

#include <device/device_types.h> 
#include <device/device.h>
#include <device/dev_hdr.h>
#include <device/device_port.h>
#include <device/io_req.h>
#include <device/if_ether.h>
#include <net_io.h>
#include <spl.h>
#include <kern/lock.h>

#include "fipc.h" 

void fipc_packet();
void allocate_fipc_buffers(boolean_t);
int fipc_lookup(unsigned short port);
int fipc_lookup_table_enter(unsigned short port);
int fipc_lookup_table_remove(unsigned short port);
int f_lookup_hash(unsigned short port);
int fipc_done(io_req_t ior);


/********************************************************************/
/* fipc variables
/********************************************************************/

fipc_port_t fports[N_MAX_OPEN_FIPC_PORTS];
fipc_lookup_table_ent fipc_lookup_table[N_MAX_OPEN_FIPC_PORTS];

int n_free_recv_bufs = 0;
int n_free_send_bufs = 0;
int n_fipc_recv_ports_used = 0;

int fipc_sends = 0;
int fipc_recvs =0;

fipc_stat_t fipc_stats;

char *fipc_recv_free_list = NULL;
char *fipc_recv_free_list_tail = NULL;
char *fipc_send_free_list = NULL;
char *fipc_send_free_list_tail = NULL;

/* fipc locks */
decl_simple_lock_data(, fipc_lock);
decl_simple_lock_data(, fipc_buf_q_lock);


/*
 * Routine: fipc_init(): initializes the fipc data structures.
 */

void fipc_init(void)
{
	int i;

	allocate_fipc_buffers(TRUE);	/* recv buffers */
	allocate_fipc_buffers(FALSE);	/* send buffers */

	fipc_stats.dropped_msgs = 0;

	bzero (&fports, sizeof(fports));
	for (i=0; i<N_MAX_OPEN_FIPC_PORTS; i++)
	{
		simple_lock_init(&(fports[i].lock));
		fipc_lookup_table[i].fpt_num = INVALID;
		fipc_lookup_table[i].fipc_port = INVALID;
	}
}


/*
 * Routine: allocate_fipc_buffers(): allocate more buffers
 * Currently we are only allocating 1500 byte (ETHERMTU) buffers.
 * We use the first word in the buffer as the pointer to the next.
 */

void allocate_fipc_buffers(boolean_t r_buf)
{
	char *new_pg;
	char **free_list, **free_list_tail;
	int *free_count, min_count, max_count;
	int total_buffer_size;

	if (r_buf)
	{	
		free_count = &n_free_recv_bufs;
		min_count = N_MIN_RECV_BUFS;
		max_count = N_MAX_RECV_BUFS;
		free_list = &fipc_recv_free_list;
		free_list_tail = &fipc_recv_free_list_tail;
		total_buffer_size = (N_MAX_RECV_BUFS * FIPC_BUFFER_SIZE);
		total_buffer_size = round_page(total_buffer_size);
	}
	else
	{
		free_count = &n_free_send_bufs;
		min_count = N_MIN_SEND_BUFS;
		max_count = N_MAX_SEND_BUFS;
		free_list = &fipc_send_free_list;
		free_list_tail = &fipc_send_free_list_tail;
		total_buffer_size = (N_MAX_SEND_BUFS * FIPC_BUFFER_SIZE);
		total_buffer_size = round_page(total_buffer_size);
	}

	if (!(*free_count))  /* empty buffer pool */
	{
#ifdef FI_DEBUG
		printf ("Allocating new fipc ");
		if (r_buf)
			printf ("recv buffer pool.\n");
		else
			printf ("send buffer pool.\n");
#endif
		*free_list = (char*)kalloc (total_buffer_size);
		if (!*free_list)	/* bummer */
			panic("allocate_fipc_buffers: no memory");
		*free_list_tail = *free_list;
		for (*free_count=1; *free_count<max_count; (*free_count)++)
		{
			*(char**)*free_list_tail = *free_list_tail + FIPC_BUFFER_SIZE;
			*free_list_tail += FIPC_BUFFER_SIZE;
		}
		*(char**)*free_list_tail = NULL;
	}	
	else  /* Request to grow the buffer pool. */
	{
#ifdef FI_DEBUG
		printf ("Growing fipc ");
		if (r_buf)
			printf ("recv buffer pool.\n");
		else
			printf ("send buffer pool.\n");
#endif

#define GROW_SIZE 8192
		new_pg = (char*)kalloc (round_page(GROW_SIZE));
		if (new_pg)
		{
			int new_cnt, n_new = GROW_SIZE / FIPC_BUFFER_SIZE;

			if (*free_list_tail != NULL)
				*(char**)*free_list_tail = new_pg;
			for ( new_cnt =0; new_cnt<n_new; new_cnt++)
			{
				*(char**)*free_list_tail = *free_list_tail + FIPC_BUFFER_SIZE;
				*free_list_tail += FIPC_BUFFER_SIZE;
			}
			*(char**)*free_list_tail = NULL;
			*free_count +=new_cnt;
		}
#ifdef FI_DEBUG
		else
			printf ("### kalloc failed in allocate_fipc_buffers()\n");
#endif

	}
}


/*
 * Routine: get_fipc_buffer (): returns a free buffer
 * Takes a size (currently not used), a boolean flag to tell if it is a
 * receive buffer, and a boolean flag if the request is coming at interrupt
 * level.
 */

inline 
char* get_fipc_buffer(int size, boolean_t r_buf, boolean_t at_int_lvl)
{
	/* we currently don't care about size, since there is only one 
	 * buffer pool. */

	char* head;
	char **free_list;
	int *free_count, min_count;

	if (r_buf)
	{	
		free_count = &n_free_recv_bufs;
		free_list = &fipc_recv_free_list;
		min_count = N_MIN_RECV_BUFS;
	}
	else
	{
		free_count = &n_free_send_bufs;
		free_list = &fipc_send_free_list;
		min_count = N_MIN_SEND_BUFS;
	}

	/*
	 * Since we currently allocate a full complement of receive buffers,
	 * there is no need to allocate more receive buffers.  But that is likely
	 * to change, I'm sure.
	 */

	if (*free_count < min_count)
	{
		if (!at_int_lvl) 
			allocate_fipc_buffers(r_buf);
	}

	if (*free_count)
	{
		head = *free_list;
		*free_list = *(char**)*free_list;
		(*free_count)--;
		return head;
	}
	else
		return NULL;
}


/*
 * Routine: return_fipc_buffer (): puts a used buffer back in the pool.
 */

inline
void return_fipc_buffer(char* buf, int size,  
			boolean_t r_buf, boolean_t at_int_lvl)
{
	/* return the buffer to the free pool */
	char **free_list, **free_list_tail;
	int *free_count, min_count;

	if (r_buf)
	{	
		free_count = &n_free_recv_bufs;
		free_list = &fipc_recv_free_list;
		free_list_tail = &fipc_recv_free_list_tail;
		min_count = N_MIN_RECV_BUFS;
	}
	else
	{
		free_count = &n_free_send_bufs;
		free_list = &fipc_send_free_list;
		free_list_tail = &fipc_send_free_list_tail;
		min_count = N_MIN_SEND_BUFS;
	}

#ifdef FI_SECURE
	bzero(buf, FIPC_BUFFER_SIZE);
#endif

	if (*free_list_tail != NULL)
		*(char**)*free_list_tail = buf;
	*free_list_tail = buf;
	(*free_count)++;
	*(char**)buf = NULL;

	if (!at_int_lvl)
		if (*free_count < min_count)
			allocate_fipc_buffers(r_buf);

	return;
}

inline
int f_lookup_hash(unsigned short port)
{
	/* Ok, so it's not really a hash function */
	int bail=0;
	int chk=0;

	if (n_fipc_recv_ports_used == N_MAX_OPEN_FIPC_PORTS ||
		port > MAX_FIPC_PORT_NUM)
			return INVALID;

	while (fipc_lookup_table[chk].fipc_port != port &&
			fipc_lookup_table[chk].fpt_num != INVALID &&
			bail < N_MAX_OPEN_FIPC_PORTS)
	{
		chk = (chk+1) % N_MAX_OPEN_FIPC_PORTS;
		bail++;
	}

	/* This is redundent, but better safe then sorry */
	if (bail<N_MAX_OPEN_FIPC_PORTS)
		return chk;
	else
		return INVALID;
}

inline
int fipc_lookup_table_enter(unsigned short port)
{
	int cfpn = n_fipc_recv_ports_used;
	int lu_tbl_num = f_lookup_hash(port);

	if (lu_tbl_num == INVALID)
		return INVALID;

	fipc_lookup_table[lu_tbl_num].fipc_port = port;
	fipc_lookup_table[lu_tbl_num].fpt_num = cfpn;
	n_fipc_recv_ports_used += 1;
	return cfpn;
}

inline
int fipc_lookup(unsigned short port)
{
	int chk = f_lookup_hash(port);

	if (chk == INVALID)
		return INVALID;

	if (fipc_lookup_table[chk].fpt_num == INVALID)
		return fipc_lookup_table_enter(port);
	else
		return fipc_lookup_table[chk].fpt_num;
}

inline
int fipc_lookup_table_remove(unsigned short port)
{
	int chk = f_lookup_hash(port);

	if (chk == INVALID)
		return 0;

	if (fipc_lookup_table[chk].fipc_port == port)
	{
		fports[fipc_lookup_table[chk].fpt_num].valid_msg = 0;
		fports[fipc_lookup_table[chk].fpt_num].bound = FALSE;
		fipc_lookup_table[chk].fpt_num = INVALID;
		fipc_lookup_table[chk].fipc_port = INVALID;
		n_fipc_recv_ports_used -=1;
		return 1;
	}
	return 0;
}

/*
 * Routine: fipc_packet(): handles incoming fipc packets.
 * does some simple packet handling and wakes up receiving thread, if any.
 * called by device controller (currently, nerecv only.)
 * called at interrupt level and splimp.
 * Messages are dropped if the recv queue is full.
 */

 void fipc_packet( char* msg_buf, struct ether_header sender)
 {
	int to_port = ((fipc_header_t*)msg_buf)->dest_port;
	int from_port = ((fipc_header_t*)msg_buf)->send_port;
	int f_tbl_num;
	fipc_port_t *cfp;
	fipc_buffer_q_ent *crqe;
	int *tail;

#ifdef FI_DEBUG
	printf  ("fipc_packet :(0x%x) %s", msg_buf,
				msg_buf+sizeof(fipc_header_t));
#endif

	f_tbl_num = fipc_lookup(to_port);
	if (f_tbl_num == INVALID)
	{
#ifdef FI_DEBUG
		printf ("Lookup failed.\n");
#endif
		fipc_stats.dropped_msgs += 1;
		return_fipc_buffer (msg_buf, FIPC_BUFFER_SIZE, TRUE, TRUE);
		return;
	}

	cfp = &fports[f_tbl_num];
	tail = &cfp->rq_tail;
	crqe = &cfp->recv_q[*tail];
		
	if (cfp->valid_msg == FIPC_RECV_Q_SIZE) 
	{
		/* Queue full.
		 * Drop packet, return buffer, and return. */
#ifdef FI_DEBUG
		printf ("Port %d queue is full: valid_msg count: %d\n", 
				to_port, cfp->valid_msg);
#endif
		fipc_stats.dropped_msgs += 1;
		return_fipc_buffer (msg_buf, FIPC_BUFFER_SIZE, TRUE, TRUE);
		return;
	}

	/* "enqueue" at "tail" */
	crqe->buffer = msg_buf;
	crqe->size = ((fipc_header_t*)msg_buf)->msg_size;
    /* This could certainly be done faster... */
    bcopy(&(sender.ether_shost), &(crqe->sender.hwaddr), ETHER_HWADDR_SIZE);
    /* This is actually useless, since there _is_ no sender port.. duh.  */
    crqe->sender.port = from_port;

	*tail = ((*tail)+1) % FIPC_RECV_Q_SIZE;

	if (cfp->bound)
		thread_wakeup(&(cfp->valid_msg));
	cfp->valid_msg++;
#ifdef FI_DEBUG
	printf ("valid_msg: %d\n", cfp->valid_msg);
#endif

	return;
 }


/********************************************************************/
/* Routine: fipc_send
/********************************************************************/

kern_return_t syscall_fipc_send(fipc_endpoint_t dest,
								char *user_buffer, int len)
{
	/* static mach_device_t 	eth_device = 0; */
	static device_t 	eth_device = 0;
	static unsigned char	hwaddr[ETHER_HWADDR_SIZE+2];

	io_return_t rc;
	kern_return_t open_res, kr;
	dev_mode_t mode = D_WRITE;
	/* register */ io_req_t ior;
	struct ether_header *ehdr;
	fipc_header_t *fhdr;
	int *d_addr;
	int data_count;
	char *fipc_buf, *data_buffer;

#ifdef FI_DEBUG
	printf("fipc_send(port:%d, len:%d, buf:x%x) !!!\n", 
		dest.port, len, user_buffer);
#endif

	if (dest.port > MAX_FIPC_PORT_NUM || 
		len > FIPC_MSG_SIZE)
	{
#ifdef FI_DEBUG
			printf ("len: %d, dest.port: %u\n", len, dest.port);
#endif
			return KERN_INVALID_ARGUMENT;
	}

	/* We'll use this io_req if we have to open the device. 
	 * Which we shouldn't have to do. */

	io_req_alloc (ior, 0);

	/* We only need to probe the device once. */

	if (!eth_device)
	{
		unsigned char net_hwaddr[ETHER_HWADDR_SIZE+2];
		int stat_count = sizeof(net_hwaddr)/sizeof(int);

		/* XXX Automatic lookup for ne0 or ne1 was failing... */
		eth_device = device_lookup(ETHER_DEVICE_NAME); 
		if (eth_device == DEVICE_NULL || 
			eth_device == (device_t)D_NO_SUCH_DEVICE) 
			{	
#ifdef FI_DEBUG
				printf ("FIPC: Couldn't find ethernet device %s.\n",
					ETHER_DEVICE_NAME);
#endif
				return (KERN_FAILURE); 
			}


		/* The device should be open! */
		if (eth_device->state != DEV_STATE_OPEN)
		{
#ifdef FI_DEBUG
			printf ("Opening ethernet device.\n");
#endif

			ior->io_device = eth_device;
			ior->io_unit = eth_device->dev_number;
			ior->io_op = IO_OPEN | IO_CALL;
			ior->io_mode = mode; 
			ior->io_error = 0;
			ior->io_done = 0;
			ior->io_reply_port = MACH_PORT_NULL;
			ior->io_reply_port_type = 0;
			
			/* open the device */
			open_res = 
			(*eth_device->dev_ops->d_open)(eth_device->dev_number, 
												(int)mode, ior);
			if (ior->io_error != D_SUCCESS)
			{
#ifdef FI_DEBUG
				printf ("Failed to open device ne0\n");
#endif
				return open_res;
			}
		}	
#ifdef i386
		rc = mach_device_get_status(eth_device, NET_ADDRESS,
					    net_hwaddr, &stat_count);
#else
		rc = ds_device_get_status(eth_device, NET_ADDRESS, net_hwaddr,
					  &stat_count);
#endif
		if (rc != D_SUCCESS)
		{
#ifdef FI_DEBUG
			printf("FIPC: Couldn't determine hardware ethernet address: %d\n", 
					rc);
#endif
			return KERN_FAILURE;
		}
		*(int*)hwaddr = ntohl(*(int*)net_hwaddr);
		*(int*)(hwaddr+4) = ntohl(*(int*)(net_hwaddr+4));
	}	
		
	data_count = len + sizeof (struct ether_header) 
					 + sizeof (fipc_header_t);

	fipc_buf = get_fipc_buffer(data_count, FALSE, FALSE);

	if (fipc_buf == NULL)
		return KERN_RESOURCE_SHORTAGE;

	/* Set up the device information. */
	ior->io_device      = eth_device;
	ior->io_unit        = eth_device->dev_number;
	ior->io_op		    = IO_WRITE | IO_INBAND | IO_INTERNAL;
	ior->io_mode        = D_WRITE;
	ior->io_recnum      = 0;     
	ior->io_data        = fipc_buf; 
	ior->io_count       = data_count;
	ior->io_total       = data_count;
	ior->io_alloc_size  = 0;
	ior->io_residual    = 0;
	ior->io_error       = 0;
	ior->io_done        = fipc_done; 
	ior->io_reply_port  = MACH_PORT_NULL; 
	ior->io_reply_port_type = 0; 
	ior->io_copy        = VM_MAP_COPY_NULL; 

	ehdr = (struct ether_header *)ior->io_data;
	d_addr = (int *)ehdr->ether_dhost;

	*(int *)ehdr->ether_dhost = *(int*)dest.hwaddr;
	*(int *)(ehdr->ether_dhost+4) = *(int*)(dest.hwaddr+4);
	*(int *)ehdr->ether_shost = *(int *)hwaddr;
	*(int *)(ehdr->ether_shost+4) = *(int *)(hwaddr+4);
	ehdr->ether_type = 0x1234;                /* Yep. */


#ifdef FI_DEBUG
	printf("sending from %s ", ether_sprintf(ehdr->ether_shost));
	printf("to %s, type x%x, user_port x%x\n",
		ether_sprintf(ehdr->ether_dhost),
		(int)ehdr->ether_type,
		(int)dest.port);
#endif

	if (len <= FIPC_MSG_SIZE)
	{
		fhdr = (fipc_header_t*)(ior->io_data+sizeof(struct ether_header));
		fhdr->dest_port = dest.port;
		fhdr->msg_size = len;
		data_buffer = (char*)fhdr+sizeof(fipc_header_t);

		copyin (user_buffer, data_buffer, 
			min (FIPC_BUFFER_SIZE-sizeof(fipc_header_t), len));

		/* Now write to the device */
		/* d_port_death has been co-opted for fipc stuff.
	  	 * It maps to nefoutput(). */

		rc = (*eth_device->dev_ops->d_port_death) /* that's the one */
				(eth_device->dev_number, ior);
	}
#ifdef FI_DEBUG
	else	/* len > ETHERMTU: multi-packet request */
		printf  ("### multi-packet messages are not supported.\n");
#endif

	if (rc == D_IO_QUEUED)
		return KERN_SUCCESS;
	else
		return KERN_FAILURE;
}

/********************************************************************
/* syscall_fipc_recv()
/*
/********************************************************************/

kern_return_t syscall_fipc_recv(unsigned short user_port, 
	char *user_buffer, int *user_size, fipc_endpoint_t *user_sender)
{
	char* f_buffer;
	fipc_port_t *cfp; 
	fipc_buffer_q_ent *crqe;
	int *head;
	int msg_size;
	int fport_num = fipc_lookup(user_port);
	spl_t spl;

#ifdef FI_DEBUG
	printf("fipc_recv(0x%x, 0x%x) !!!\n", user_port, user_buffer);
#endif

	if (user_port > MAX_FIPC_PORT_NUM)
	{
#ifdef FI_DEBUG
		printf ("Invalid FIPC port: %u\n", user_port);
#endif
		return KERN_INVALID_ARGUMENT;
	}

	if (fport_num == INVALID)
		return KERN_RESOURCE_SHORTAGE;

	cfp = &fports[fport_num]; 
	head = &cfp->rq_head;
	crqe = &cfp->recv_q[*head];

	if (cfp->bound != FALSE)
	{
#ifdef FI_DEBUG
		printf ("FIPC Port %u is currently bound.\n", user_port);
#endif
		return KERN_RESOURCE_SHORTAGE;
	}

	copyin(user_size, &msg_size, sizeof(int));

	spl = splimp();

	cfp->bound = TRUE;
	while (!(cfp->valid_msg))
	{
		assert_wait(&(cfp->valid_msg), TRUE);
		splx(spl);
		thread_block ((void(*)())0);
		if (current_thread()->wait_result != THREAD_AWAKENED)
		{
			cfp->bound = FALSE;
			return KERN_FAILURE;
		}
		spl = splimp();
	}

	cfp->valid_msg--;
	f_buffer = crqe->buffer;
	msg_size = min (crqe->size, msg_size);

	crqe->buffer = NULL;
	crqe->size = 0;
	*head = ((*head)+1) % FIPC_RECV_Q_SIZE;
	cfp->bound = FALSE;

	splx(spl);

	copyout(f_buffer+sizeof(fipc_header_t), user_buffer, msg_size); 
	copyout(&(crqe->sender), user_sender, sizeof(fipc_endpoint_t));
	copyout(&msg_size, user_size, sizeof(msg_size));

	return_fipc_buffer(f_buffer, FIPC_BUFFER_SIZE, TRUE, FALSE);

	return KERN_SUCCESS;
}

/*
 * Final clean-up after the packet has been sent off.
 */
int fipc_done(io_req_t ior)
{
    return_fipc_buffer(ior->io_data, FIPC_BUFFER_SIZE, FALSE, FALSE);

	return 1;
}

#endif /* FIPC */
