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
 *	File:	ipc/ipc_hash.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations of entry hash table operations.
 */

#ifndef	_IPC_IPC_HASH_H_
#define _IPC_IPC_HASH_H_

#include <mach_ipc_debug.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>

extern void
ipc_hash_init();

#if	MACH_IPC_DEBUG

extern unsigned int
ipc_hash_info(/* hash_info_bucket_t *, unsigned int */);

#endif	MACH_IPC_DEBUG

extern boolean_t
ipc_hash_lookup(/* ipc_space_t space, ipc_object_t obj,
		   mach_port_t *namep, ipc_entry_t *entryp */);

extern void
ipc_hash_insert(/* ipc_space_t space, ipc_object_t obj,
		   mach_port_t name, ipc_entry_t entry */);

extern void
ipc_hash_delete(/* ipc_space_t space, ipc_object_t obj,
		   mach_port_t name, ipc_entry_t entry */);

/*
 *	For use by functions that know what they're doing:
 *	the global primitives, for splay tree entries,
 *	and the local primitives, for table entries.
 */

extern boolean_t
ipc_hash_global_lookup(/* ipc_space_t space, ipc_object_t obj,
			  mach_port_t *namep, ipc_tree_entry_t *entryp */);

extern void
ipc_hash_global_insert(/* ipc_space_t space, ipc_object_t obj,
			  mach_port_t name, ipc_tree_entry_t entry */);

extern void
ipc_hash_global_delete(/* ipc_space_t space, ipc_object_t obj,
			  mach_port_t name, ipc_tree_entry_t entry */);

extern boolean_t
ipc_hash_local_lookup(/* ipc_space_t space, ipc_object_t obj,
			 mach_port_t *namep, ipc_entry_t *entryp */);

extern void
ipc_hash_local_insert(/* ipc_space_t space, ipc_object_t obj,
			 mach_port_index_t index, ipc_entry_t entry */);

extern void
ipc_hash_local_delete(/* ipc_space_t space, ipc_object_t obj,
			 mach_port_index_t index, ipc_entry_t entry */);

#endif	_IPC_IPC_HASH_H_
