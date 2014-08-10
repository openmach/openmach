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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *	File: scsi_user_dma.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	4/91
 *
 *	Defines for Mach 2.5 compat, user-space DMA routines
 */

/* There is one such structure per I/O device
   that needs to xfer data to/from user space */

typedef struct fdma {
	vm_offset_t	kernel_virtual;
	vm_size_t	max_data;
	vm_offset_t	user_virtual;
	int		xfer_size_rnd;
} *fdma_t;

extern int
	fdma_init(/* fdma_t, vm_size_t */),
	fdma_map(/* fdma_t, struct buf* */),
	fdma_unmap(/* fdma_t, struct buf* */);
