/* 
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
 *	File:	mach_debug/pc_info.h
 *	Author:	Brian Bershad
 *	Date:	January 1992
 *
 *	Definitions for the PC sampling interface.
 */

#ifndef	_MACH_DEBUG_PC_INFO_H_
#define _MACH_DEBUG_PC_INFO_H_


typedef struct sampled_pc {
    task_t 	task;
    thread_t 	thread;
    vm_offset_t pc;
} sampled_pc_t;

typedef sampled_pc_t *sampled_pc_array_t;
typedef unsigned int sampled_pc_seqno_t;

#endif	_MACH_DEBUG_PC_INFO_H_
