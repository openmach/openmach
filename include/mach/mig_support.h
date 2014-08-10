/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 *  Abstract:
 *	Header file for support routines called by MiG generated interfaces.
 *
 */

#ifndef	_MACH_MIG_SUPPORT_H_
#define	_MACH_MIG_SUPPORT_H_

#include <mach/message.h>
#include <mach/mach_types.h>

#if	defined(MACH_KERNEL)

#if	defined(bcopy)
#else	/* not defined(bcopy) */
extern void	bcopy(const void *, void *, vm_size_t);
#define	memcpy(_dst,_src,_len)	bcopy((_src),(_dst),(_len))
#endif	/* defined(bcopy) */

#endif	/* defined(MACH_KERNEL) */

extern void		mig_init(void *_first);

extern void		mig_allocate(vm_address_t *_addr_p, vm_size_t _size);

extern void		mig_deallocate(vm_address_t _addr, vm_size_t _size);

extern void		mig_dealloc_reply_port(mach_port_t);

extern void		mig_put_reply_port(mach_port_t);

extern mach_port_t	mig_get_reply_port(void);

extern void		mig_reply_setup(const mach_msg_header_t *_request,
					mach_msg_header_t *reply);

#ifndef MACH_KERNEL
extern vm_size_t		mig_strncpy(char *_dest, const char *_src, vm_size_t _len);
#endif

#endif	/* not defined(_MACH_MIG_SUPPORT_H_) */
