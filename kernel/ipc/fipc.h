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
 *  Utah $Hdr: fipc.h 1.1 96/2/29$
 *  Author: Linus Kamb
 */

#include <kern/lock.h>
#include <device/if_ether.h>


#define N_MAX_OPEN_FIPC_PORTS	32	/* In practice, 
                                     * this should be much larger */
#define MAX_FIPC_PORT_NUM		4095	/* ditto */

#define FIPC_MSG_TYPE			0x1234

#define FIPC_BUFFER_SIZE		ETHERMTU
#define FIPC_MSG_SIZE			(FIPC_BUFFER_SIZE - sizeof(fipc_header_t))

#define FIPC_RECV_Q_SIZE		4
#define N_MIN_RECV_BUFS			5	/* 2 pages worth */
#define N_MAX_RECV_BUFS			(N_MAX_OPEN_FIPC_PORTS * FIPC_RECV_Q_SIZE)
#define N_MIN_SEND_BUFS			2
#define N_MAX_SEND_BUFS			5

#define INVALID					-1

#define ETHER_HWADDR_SIZE		6
#define ETHER_DEVICE_NAME		"ne0"

typedef struct fipc_endpoint_structure
{
	unsigned char hwaddr[ETHER_HWADDR_SIZE];
	unsigned short port;
} fipc_endpoint_t;

typedef struct fipc_buffer_structure
{
	char *buffer;
	unsigned short size;
	fipc_endpoint_t sender;
} fipc_buffer_q_ent;

typedef struct fipc_port_structure
{
	simple_lock_data_t lock;
	boolean_t bound;
	int valid_msg;
	fipc_buffer_q_ent recv_q[FIPC_RECV_Q_SIZE];
	int rq_head, rq_tail;
} fipc_port_t;

typedef struct fipc_header_structure
{
	unsigned short dest_port;
	unsigned short send_port;
	unsigned int msg_size;
} fipc_header_t;

typedef struct fipc_lookup_table_ent_structure
{
	int fipc_port;
	int fpt_num;	/* f_ports[] entry number */
} fipc_lookup_table_ent;

typedef struct fipc_stat_structure
{
	int dropped_msgs;
} fipc_stat_t;

#define min(a,b)	(((a)<=(b)?(a):(b)))

char* get_fipc_buffer(int, boolean_t, boolean_t);
void fipc_packet(char*, struct ether_header);

extern int fipc_sends;
extern int fipc_recvs;

