/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 *	File:	mach/port.h
 *
 *	Definition of a port
 *
 *	[The basic mach_port_t type should probably be machine-dependent,
 *	as it must be represented by a 32-bit integer.]
 */

#ifndef	_MACH_PORT_H_
#define _MACH_PORT_H_

#ifdef	MACH_KERNEL
#include <mach_ipc_compat.h>
#endif	/* MACH_KERNEL */

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>


typedef natural_t mach_port_t;
typedef mach_port_t *mach_port_array_t;
typedef int *rpc_signature_info_t;

/*
 *  MACH_PORT_NULL is a legal value that can be carried in messages.
 *  It indicates the absence of any port or port rights.  (A port
 *  argument keeps the message from being "simple", even if the
 *  value is MACH_PORT_NULL.)  The value MACH_PORT_DEAD is also
 *  a legal value that can be carried in messages.  It indicates
 *  that a port right was present, but it died.
 */

#define MACH_PORT_NULL		((mach_port_t) 0)
#define MACH_PORT_DEAD		((mach_port_t) ~0)

#define	MACH_PORT_VALID(name)	\
		(((name) != MACH_PORT_NULL) && ((name) != MACH_PORT_DEAD))

/*
 *  These are the different rights a task may have.
 *  The MACH_PORT_RIGHT_* definitions are used as arguments
 *  to mach_port_allocate, mach_port_get_refs, etc, to specify
 *  a particular right to act upon.  The mach_port_names and
 *  mach_port_type calls return bitmasks using the MACH_PORT_TYPE_*
 *  definitions.  This is because a single name may denote
 *  multiple rights.
 */

typedef natural_t mach_port_right_t;

#define MACH_PORT_RIGHT_SEND		((mach_port_right_t) 0)
#define MACH_PORT_RIGHT_RECEIVE		((mach_port_right_t) 1)
#define MACH_PORT_RIGHT_SEND_ONCE	((mach_port_right_t) 2)
#define MACH_PORT_RIGHT_PORT_SET	((mach_port_right_t) 3)
#define MACH_PORT_RIGHT_DEAD_NAME	((mach_port_right_t) 4)
#define MACH_PORT_RIGHT_NUMBER		((mach_port_right_t) 5)

typedef natural_t mach_port_type_t;
typedef mach_port_type_t *mach_port_type_array_t;

#define MACH_PORT_TYPE(right)	    ((mach_port_type_t)(1 << ((right)+16)))
#define MACH_PORT_TYPE_NONE	    ((mach_port_type_t) 0)
#define MACH_PORT_TYPE_SEND	    MACH_PORT_TYPE(MACH_PORT_RIGHT_SEND)
#define MACH_PORT_TYPE_RECEIVE	    MACH_PORT_TYPE(MACH_PORT_RIGHT_RECEIVE)
#define MACH_PORT_TYPE_SEND_ONCE    MACH_PORT_TYPE(MACH_PORT_RIGHT_SEND_ONCE)
#define MACH_PORT_TYPE_PORT_SET	    MACH_PORT_TYPE(MACH_PORT_RIGHT_PORT_SET)
#define MACH_PORT_TYPE_DEAD_NAME    MACH_PORT_TYPE(MACH_PORT_RIGHT_DEAD_NAME)

/* Convenient combinations. */

#define MACH_PORT_TYPE_SEND_RECEIVE					\
		(MACH_PORT_TYPE_SEND|MACH_PORT_TYPE_RECEIVE)
#define	MACH_PORT_TYPE_SEND_RIGHTS					\
		(MACH_PORT_TYPE_SEND|MACH_PORT_TYPE_SEND_ONCE)
#define	MACH_PORT_TYPE_PORT_RIGHTS					\
		(MACH_PORT_TYPE_SEND_RIGHTS|MACH_PORT_TYPE_RECEIVE)
#define	MACH_PORT_TYPE_PORT_OR_DEAD					\
		(MACH_PORT_TYPE_PORT_RIGHTS|MACH_PORT_TYPE_DEAD_NAME)
#define MACH_PORT_TYPE_ALL_RIGHTS					\
		(MACH_PORT_TYPE_PORT_OR_DEAD|MACH_PORT_TYPE_PORT_SET)

/* Dummy type bits that mach_port_type/mach_port_names can return. */

#define MACH_PORT_TYPE_DNREQUEST	0x80000000U
#define MACH_PORT_TYPE_MAREQUEST	0x40000000
#define	MACH_PORT_TYPE_COMPAT		0x20000000

/* User-references for capabilities. */

typedef natural_t mach_port_urefs_t;
typedef integer_t mach_port_delta_t;			/* change in urefs */

/* Attributes of ports.  (See mach_port_get_receive_status.) */

typedef natural_t mach_port_seqno_t;		/* sequence number */
typedef unsigned int mach_port_mscount_t;	/* make-send count */
typedef unsigned int mach_port_msgcount_t;	/* number of msgs */
typedef unsigned int mach_port_rights_t;	/* number of rights */

typedef struct mach_port_status {
	mach_port_t		mps_pset;	/* containing port set */
	mach_port_seqno_t	mps_seqno;	/* sequence number */
/*mach_port_mscount_t*/natural_t mps_mscount;	/* make-send count */
/*mach_port_msgcount_t*/natural_t mps_qlimit;	/* queue limit */
/*mach_port_msgcount_t*/natural_t mps_msgcount;	/* number in the queue */
/*mach_port_rights_t*/natural_t	mps_sorights;	/* how many send-once rights */
/*boolean_t*/natural_t		mps_srights;	/* do send rights exist? */
/*boolean_t*/natural_t		mps_pdrequest;	/* port-deleted requested? */
/*boolean_t*/natural_t		mps_nsrequest;	/* no-senders requested? */
} mach_port_status_t;

#define MACH_PORT_QLIMIT_DEFAULT	((mach_port_msgcount_t) 5)
#define MACH_PORT_QLIMIT_MAX		((mach_port_msgcount_t) 16)

/*
 *  Compatibility definitions, for code written
 *  before there was an mps_seqno field.
 */

typedef struct old_mach_port_status {
	mach_port_t		mps_pset;	/* containing port set */
/*mach_port_mscount_t*/natural_t mps_mscount;	/* make-send count */
/*mach_port_msgcount_t*/natural_t mps_qlimit;	/* queue limit */
/*mach_port_msgcount_t*/natural_t mps_msgcount;	/* number in the queue */
/*mach_port_rights_t*/natural_t	mps_sorights;	/* how many send-once rights */
/*boolean_t*/natural_t		mps_srights;	/* do send rights exist? */
/*boolean_t*/natural_t		mps_pdrequest;	/* port-deleted requested? */
/*boolean_t*/natural_t		mps_nsrequest;	/* no-senders requested? */
} old_mach_port_status_t;


/* Definitions for the old IPC interface. */

#if	MACH_IPC_COMPAT

typedef integer_t	port_name_t;		/* A capability's name */
typedef port_name_t	port_set_name_t;	/* Descriptive alias */
typedef port_name_t	*port_name_array_t;

typedef integer_t	port_type_t;		/* What kind of capability? */
typedef port_type_t	*port_type_array_t;

	/* Values for port_type_t */

#define PORT_TYPE_NONE		0		/* No rights */
#define PORT_TYPE_SEND		1		/* Send rights */
#define PORT_TYPE_RECEIVE	3		/* obsolete */
#define PORT_TYPE_OWN		5		/* obsolete */
#define PORT_TYPE_RECEIVE_OWN	7		/* Send, receive, ownership */
#define PORT_TYPE_SET		9		/* Set ownership */
#define PORT_TYPE_LAST		10		/* Last assigned */

typedef	port_name_t	port_t;			/* Port with send rights */
typedef	port_t		port_rcv_t;		/* Port with receive rights */
typedef	port_t		port_own_t;		/* Port with ownership rights */
typedef	port_t		port_all_t;		/* Port with receive and ownership */
typedef	port_t		*port_array_t;

#define PORT_NULL	((port_name_t) 0)	/* Used to denote no port; legal value */

#endif	/* MACH_IPC_COMPAT */

#endif	/* _MACH_PORT_H_ */
