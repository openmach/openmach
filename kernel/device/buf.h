/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
 *	Date: 	3/90
 *
 * 	Definitions to make new IO structures look like old ones
 */

/*
 * io_req and fields
 */
#include <device/io_req.h>

#define	buf	io_req

/*
 * Redefine fields for drivers using old names
 */
#define	b_flags		io_op
#define	b_bcount	io_count
#define	b_error		io_error
#define	b_dev		io_unit
#define	b_blkno		io_recnum
#define	b_resid		io_residual
#define	b_un		io_un
#define	b_addr		data
#define	av_forw		io_next
#define	av_back		io_prev
#define b_physblock     io_physrec
#define b_blocktotal    io_rectotal

/*
 * Redefine fields for driver request list heads, using old names.
 */
#define	b_actf		io_next
#define	b_actl		io_prev
#define	b_forw		io_link
#define	b_back		io_rlink
#define	b_active	io_count
#define	b_errcnt	io_residual
#define	b_bufsize	io_alloc_size

/*
 * Redefine flags
 */
#define	B_WRITE		IO_WRITE
#define	B_READ		IO_READ
#define	B_OPEN		IO_OPEN
#define	B_DONE		IO_DONE
#define	B_ERROR		IO_ERROR
#define	B_BUSY		IO_BUSY
#define	B_WANTED	IO_WANTED
#define	B_BAD		IO_BAD
#define	B_CALL		IO_CALL

#define	B_MD1		IO_SPARE_START

/*
 * Redefine uio structure
 */
#define	uio	io_req

/*
 * Redefine physio routine
 */
#define	physio(strat, xbuf, dev, ops, minphys, ior) \
		block_io(strat, minphys, ior)

/*
 * Export standard minphys routine.
 */
extern	minphys();

/*
 * Alternate name for iodone
 */
#define	biodone	iodone
#define biowait iowait
