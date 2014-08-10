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
 *	Date: 	11/89
 *
 * 	Bind an IO operation to the master CPU.
 */

#include <cpus.h>

#if	NCPUS > 1

#include <kern/macro_help.h>
#include <kern/cpu_number.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/processor.h>

#define	io_grab_master() \
	MACRO_BEGIN \
	thread_bind(current_thread(), master_processor); \
	if (current_processor() != master_processor) \
	    thread_block((void (*)()) 0); \
	MACRO_END

#define	io_release_master() \
	MACRO_BEGIN \
	thread_bind(current_thread(), PROCESSOR_NULL); \
	MACRO_END

#else	NCPUS > 1

#define	io_grab_master()
#define	io_release_master()

#endif	NCPUS > 1
