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

#define	MACH_INIT_SLOTS		1
#include <mach.h>

mach_port_t	name_server_port = MACH_PORT_NULL;
mach_port_t	environment_port = MACH_PORT_NULL;
mach_port_t	service_port = MACH_PORT_NULL;

void mach_init_ports()
{
	mach_port_array_t	ports;
	natural_t		ports_count;
	kern_return_t		kr;

	/*
	 *	Find those ports important to every task.
	 */

	kr = mach_ports_lookup(mach_task_self(), &ports,
			       &ports_count);
	if ((kr != KERN_SUCCESS) ||
	    (ports_count < MACH_PORTS_SLOTS_USED))
	    return;

	name_server_port = ports[NAME_SERVER_SLOT];
	environment_port = ports[ENVIRONMENT_SLOT];
	service_port     = ports[SERVICE_SLOT];

	/* get rid of out-of-line data so brk has a chance of working */

	(void) vm_deallocate(mach_task_self(),
			     (vm_offset_t) ports,
			     (vm_size_t) (ports_count * sizeof *ports));
}

#ifndef	lint
/*
 *	Routines which our library must suck in, to avoid
 *	a later library from referencing them and getting
 *	the wrong version.
 */
_replacements()
{
	sbrk(0);
	fork();
}
#endif	lint
