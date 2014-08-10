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
 *	File: dz_defs.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Internal definitions for the DZ Serial Line Driver
 */

#include <mach/std_types.h>
#include <chips/busses.h>

#include <kern/time_out.h>
#include <sys/syslog.h>

#include <device/io_req.h>
#include <device/conf.h>
#include <device/tty.h>
#include <device/errno.h>

#include <chips/dz_7085.h>

extern struct tty *dz_tty[];

extern struct pseudo_dma {
	dz_regmap_t	*p_addr;
	char		*p_mem;
	char		*p_end;
	int		p_arg;
	int		(*p_fcn)();
} dz_pdma[];

extern int rcline, cnline;
extern int	console;

/*
 * Modem control operations on DZ lines
 */

extern unsigned dz_mctl(/* int, int, int */);

