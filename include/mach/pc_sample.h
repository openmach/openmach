/* 
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
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

#ifndef	_MACH_PC_SAMPLE_H_
#define _MACH_PC_SAMPLE_H_

#include <mach/machine/vm_types.h>

typedef natural_t	sampled_pc_flavor_t;


#define SAMPLED_PC_PERIODIC			0x1	/* default */


#define SAMPLED_PC_VM_ZFILL_FAULTS		0x10
#define SAMPLED_PC_VM_REACTIVATION_FAULTS	0x20
#define SAMPLED_PC_VM_PAGEIN_FAULTS		0x40
#define SAMPLED_PC_VM_COW_FAULTS		0x80
#define SAMPLED_PC_VM_FAULTS_ANY		0x100
#define SAMPLED_PC_VM_FAULTS		\
			(SAMPLED_PC_VM_ZFILL_FAULTS | \
			 SAMPLED_PC_VM_REACTIVATION_FAULTS |\
			 SAMPLED_PC_VM_PAGEIN_FAULTS |\
			 SAMPLED_PC_VM_COW_FAULTS )




/*
 *	Definitions for the PC sampling interface.
 */

typedef struct sampled_pc {
    natural_t		id;
    vm_offset_t		pc;
    sampled_pc_flavor_t sampletype;
} sampled_pc_t;

typedef sampled_pc_t *sampled_pc_array_t;
typedef unsigned int sampled_pc_seqno_t;


#endif	/* _MACH_PC_SAMPLE_H_ */
