/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	File: compat_30.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	4/91
 *
 *	Compatibility defs to retrofit Mach 3.0 drivers
 *	into Mach 2.6.
 */

#ifndef	_SCSI_COMPAT_30_
#define	_SCSI_COMPAT_30_

#include <kern/assert.h>

#ifdef	MACH_KERNEL
/*
 * Mach 3.0 compiles with these definitions
 */

#include <device/param.h>
#include <device/io_req.h>
#include <device/device_types.h>
#include <device/disk_status.h>

/*
 * Scratch temporary in io_req structure (for error handling)
 */
#define	io_temporary	io_error

#else	/*MACH_KERNEL*/
/*
 * Mach 2.x compiles with these definitions
 */

/* ??? */
typedef	int	dev_mode_t;
typedef int	*dev_status_t;	/* Variable-length array of integers */
/* ??? */

/* Buffer structures */

typedef	int	io_return_t;

#include <sys/param.h>
#include <sys/buf.h>

#define	io_req	buf
typedef	struct buf	*io_req_t;

#define	io_req_alloc(ior,size)	ior = geteblk(size)
#define	io_req_free(ior)	brelse(ior)

/*
 * Redefine fields for drivers using new names
 */
#define	io_op		b_flags
#define	io_count	b_bcount
#define	io_error	b_error
#define	io_unit		b_dev
#define	io_recnum	b_blkno
#define	io_residual	b_resid
#define	io_data		b_un.b_addr
#define	io_done		b_iodone

/*
 * Redefine fields for driver request list heads, using new names.
 */
#define	io_next		av_forw
#define	io_prev		av_back
/*#define	io_next		b_actf*/
/*#define	io_prev		b_actl*/
#define	io_link		b_forw
#define	io_rlink	b_back
/*#define	io_count	b_active*/
/*#define	io_residual	b_errcnt*/
#define	io_alloc_size	b_bufsize

/*
 * Scratch temporary in io_req structure (for error handling)
 */
#define	io_temporary	b_pfcent

/*
 * Redefine flags
 */
#define	IO_WRITE	B_WRITE
#define	IO_READ		B_READ
#define	IO_OPEN		B_OPEN
#define	IO_DONE		B_DONE
#define	IO_ERROR	B_ERROR
#define	IO_BUSY		B_BUSY
#define	IO_WANTED	B_WANTED
#define	IO_BAD		B_BAD
#define	IO_CALL		B_CALL
#define	IO_INTERNAL	B_MD1

#define	IO_SPARE_START	B_MD1

#include <sys/disklabel.h>

/* Error codes */

#include <sys/errno.h>

#define	D_SUCCESS		ESUCCESS
#define	D_IO_ERROR		EIO
#define	D_NO_SUCH_DEVICE	ENXIO
#define	D_INVALID_SIZE		EINVAL
#define	D_ALREADY_OPEN		EBUSY
#define	D_INVALID_OPERATION	EINVAL
#define D_NO_MEMORY		ENOMEM
#define D_WOULD_BLOCK		EWOULDBLOCK
#define D_DEVICE_DOWN		EIO
#define	D_READ_ONLY		EROFS

/*
 * Debugging support
 */
#define db_printf		kdbprintf
#define db_printsym(s,m)	kdbpsymoff(s,1,"")

/*
 * Miscellaneous utils
 */

#define	check_memory(addr,dow)	((dow) ? wbadaddr(addr,4) : badaddr(addr,4))

#include <sys/kernel.h>		/* for hz */
#include <scsi/adapters/scsi_user_dma.h>

#ifdef	DECSTATION
#include <mach/mips/vm_param.h>	/* for page size */
#define	ULTRIX_COMPAT	1	/* support for rzdisk disk formatter  */
#endif	/*DECSTATION*/

#endif	/*MACH_KERNEL*/

#endif	/*_SCSI_COMPAT_30_*/
