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
 * Mach Interface Generator errors
 *
 */

#ifndef	_MACH_MIG_ERRORS_H_
#define _MACH_MIG_ERRORS_H_

#ifdef	MACH_KERNEL
#include <mach_ipc_compat.h>
#endif	/* MACH_KERNEL */

#include <mach/kern_return.h>
#include <mach/message.h>

/*
 *	These error codes should be specified as system 4, subsytem 2.
 *	But alas backwards compatibility makes that impossible.
 *	The problem is old clients of new servers (eg, the kernel)
 *	which get strange large error codes when there is a Mig problem
 *	in the server.  Unfortunately, the IPC system doesn't have
 *	the knowledge to convert the codes in this situation.
 */

#define MIG_TYPE_ERROR		-300	/* client type check failure */
#define MIG_REPLY_MISMATCH	-301	/* wrong reply message ID */
#define MIG_REMOTE_ERROR	-302	/* server detected error */
#define MIG_BAD_ID		-303	/* bad request message ID */
#define MIG_BAD_ARGUMENTS	-304	/* server type check failure */
#define MIG_NO_REPLY		-305	/* no reply should be sent */
#define MIG_EXCEPTION		-306	/* server raised exception */
#define MIG_ARRAY_TOO_LARGE	-307	/* array not large enough */
#define MIG_SERVER_DIED		-308	/* server died */
#define MIG_DESTROY_REQUEST	-309	/* destroy request with no reply */

typedef struct {
	mach_msg_header_t	Head;
	mach_msg_type_t		RetCodeType;
	kern_return_t		RetCode;
} mig_reply_header_t;

typedef struct mig_symtab {
	char	*ms_routine_name;
	int	ms_routine_number;
#if	defined(__STDC__) || defined(c_plus_plus) || defined(hc)
	void
#else
	int
#endif
		(*ms_routine)();
} mig_symtab_t;

/*
 * Definition for server stub routines.  These routines
 * unpack the request message, call the server procedure,
 * and pack the reply message.
 */
#if	defined(__STDC__) || defined(c_plus_plus)
typedef	void	(*mig_routine_t)(mach_msg_header_t *, mach_msg_header_t *);
#else
#if	defined(hc)
typedef	void	(*mig_routine_t)();
#else
typedef	int	(*mig_routine_t)();	/* PCC cannot handle void (*)() */
#endif
#endif

/* Definitions for the old IPC interface. */

#if	MACH_IPC_COMPAT

typedef struct {
	msg_header_t		Head;
	msg_type_t		RetCodeType;
	kern_return_t		RetCode;
} death_pill_t;

#endif	/* MACH_IPC_COMPAT */

#endif	/* _MACH_MIG_ERRORS_H_ */
