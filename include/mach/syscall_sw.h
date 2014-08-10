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

#ifndef	_MACH_SYSCALL_SW_H_
#define _MACH_SYSCALL_SW_H_

/*
 *	The machine-dependent "syscall_sw.h" file should
 *	define a macro for
 *		kernel_trap(trap_name, trap_number, arg_count)
 *	which will expand into assembly code for the
 *	trap.
 *
 *	N.B.: When adding calls, do not put spaces in the macros.
 */

#include <mach/machine/syscall_sw.h>

/*
 *	These trap numbers should be taken from the
 *	table in <kern/syscall_sw.c>.
 */

kernel_trap(evc_wait,-17,1)
kernel_trap(evc_wait_clear,-18,1)

kernel_trap(mach_msg_trap,-25,7)
kernel_trap(mach_reply_port,-26,0)
kernel_trap(mach_thread_self,-27,0)
kernel_trap(mach_task_self,-28,0)
kernel_trap(mach_host_self,-29,0)

kernel_trap(swtch_pri,-59,1)
kernel_trap(swtch,-60,0)
kernel_trap(thread_switch,-61,3)
kernel_trap(nw_update,-80,3)
kernel_trap(nw_lookup,-81,2)
kernel_trap(nw_endpoint_allocate,-82,4)
kernel_trap(nw_endpoint_deallocate,-83,1)
kernel_trap(nw_buffer_allocate,-84,2)
kernel_trap(nw_buffer_deallocate,-85,2)
kernel_trap(nw_connection_open,-86,4)
kernel_trap(nw_connection_accept,-87,3)
kernel_trap(nw_connection_close,-88,1)
kernel_trap(nw_multicast_add,-89,4)
kernel_trap(nw_multicast_drop,-90,4)
kernel_trap(nw_endpoint_status,-91,3)
kernel_trap(nw_send,-92,3)
kernel_trap(nw_receive,-93,2)
kernel_trap(nw_rpc,-94,4)
kernel_trap(nw_select,-95,3)


/*
 *	These are syscall versions of Mach kernel calls.
 *	They only work on local tasks.
 */

kernel_trap(syscall_vm_map,-64,11)
kernel_trap(syscall_vm_allocate,-65,4)
kernel_trap(syscall_vm_deallocate,-66,3)

kernel_trap(syscall_task_create,-68,3)
kernel_trap(syscall_task_terminate,-69,1)
kernel_trap(syscall_task_suspend,-70,1)
kernel_trap(syscall_task_set_special_port,-71,3)

kernel_trap(syscall_mach_port_allocate,-72,3)
kernel_trap(syscall_mach_port_deallocate,-73,2)
kernel_trap(syscall_mach_port_insert_right,-74,4)
kernel_trap(syscall_mach_port_allocate_name,-75,3)
kernel_trap(syscall_thread_depress_abort,-76,1)

/* These are screwing up glibc somehow.  */
/*kernel_trap(syscall_device_writev_request,-39,6)*/
/*kernel_trap(syscall_device_write_request,-40,6)*/

/*
 *	These "Mach" traps are not implemented by the kernel;
 *	the emulation library and Unix server implement them.
 *	But they are traditionally part of libmach, and use
 *	the Mach trap calling conventions and numbering.
 */

#if	UNIXOID_TRAPS

kernel_trap(task_by_pid,-33,1)
kernel_trap(pid_by_task,-34,4)
kernel_trap(init_process,-41,0)
kernel_trap(map_fd,-43,5)
kernel_trap(rfs_make_symlink,-44,3)
kernel_trap(htg_syscall,-52,3)
kernel_trap(set_ras_address,-53,2)

#endif	/* UNIXOID_TRAPS */

/* Traps for the old IPC interface. */

#if	MACH_IPC_COMPAT

kernel_trap(task_self,-10,0)
kernel_trap(thread_reply,-11,0)
kernel_trap(task_notify,-12,0)
kernel_trap(thread_self,-13,0)
kernel_trap(msg_send_trap,-20,4)
kernel_trap(msg_receive_trap,-21,5)
kernel_trap(msg_rpc_trap,-22,6)
kernel_trap(host_self,-55,0)

#endif	/* MACH_IPC_COMPAT */

#ifdef FIPC
kernel_trap(fipc_send,-96,4)
kernel_trap(fipc_recv,-97,5)
#endif

#endif	/* _MACH_SYSCALL_SW_H_ */
