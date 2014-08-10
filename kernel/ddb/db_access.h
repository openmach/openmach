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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
/*
 * Data access functions for debugger.
 */
#include <mach/boolean.h>
#include <machine/db_machdep.h>
#include <ddb/db_task_thread.h>
#include "vm_param.h"

/* implementation dependent access capability */
#define	DB_ACCESS_KERNEL	0	/* only kernel space */
#define DB_ACCESS_CURRENT	1	/* kernel or current task space */
#define DB_ACCESS_ANY		2	/* any space */

#ifndef	DB_ACCESS_LEVEL
#define DB_ACCESS_LEVEL		DB_ACCESS_KERNEL
#endif	DB_ACCESS_LEVEL

#ifndef DB_VALID_KERN_ADDR
#define DB_VALID_KERN_ADDR(addr)	((addr) >= VM_MIN_KERNEL_ADDRESS \
					  && (addr) < VM_MAX_KERNEL_ADDRESS)
#define DB_VALID_ADDRESS(addr,user)	((user != 0) ^ DB_VALID_KERN_ADDR(addr))
#define DB_PHYS_EQ(task1,addr1,task2,addr2)	0
#define DB_CHECK_ACCESS(addr,size,task)	db_is_current_task(task)
#endif	DB_VALID_KERN_ADDR

extern int db_access_level;

extern db_expr_t db_get_value(	db_addr_t addr,
				int size,
				boolean_t is_signed );

extern void	 db_put_value(	db_addr_t addr,
				int size,
				db_expr_t value );

extern db_expr_t db_get_task_value(	db_addr_t addr,
					int size,
					boolean_t is_signed,
					task_t task );

extern void	 db_put_task_value(	db_addr_t addr,
					int size,
					db_expr_t value,
					task_t task );
