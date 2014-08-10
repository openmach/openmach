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

#include <mach.h>

extern void mig_init();
extern void mach_init_ports();

mach_port_t	mach_task_self_ = MACH_PORT_NULL;

vm_size_t	vm_page_size;

int		mach_init()
{
	vm_statistics_data_t	vm_stat;
	kern_return_t		kr;

#undef	mach_task_self

	/*
	 *	Get the important ports into the cached values,
	 *	as required by "mach_init.h".
	 */
	 
	mach_task_self_ = mach_task_self();

	/*
	 *	Initialize the single mig reply port
	 */

	mig_init(0);

	/*
	 *	Cache some other valuable system constants
	 */

	(void) vm_statistics(mach_task_self_, &vm_stat);
	vm_page_size = vm_stat.pagesize;

	mach_init_ports();

	return(0);
}

int		(*mach_init_routine)() = mach_init;
