/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 * the  rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	8/89
 *
 *	Old names for new error codes, for compatibility.
 */

#ifndef	_ERRNO_
#define	_ERRNO_

#include <device/device_types.h>	/* the real error names */

#define	EIO		D_IO_ERROR
#define	ENXIO		D_NO_SUCH_DEVICE
#define	EINVAL		D_INVALID_SIZE	/* XXX */
#define	EBUSY		D_ALREADY_OPEN
#define	ENOTTY		D_INVALID_OPERATION
#define ENOMEM		D_NO_MEMORY

#endif	_ERRNO_
