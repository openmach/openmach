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

#ifndef	_MACHID_TYPES_H_
#define	_MACHID_TYPES_H_

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/task_info.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/vm_inherit.h>

/* define types for machid_types.defs */

typedef unsigned int mach_id_t;
typedef unsigned int mach_type_t;

typedef mach_id_t mhost_t;
typedef mach_id_t mhost_priv_t;

typedef mach_id_t mdefault_pager_t;

typedef mach_id_t mprocessor_t;
typedef mprocessor_t *mprocessor_array_t;

typedef mach_id_t mprocessor_set_t;
typedef mprocessor_set_t *mprocessor_set_array_t;
typedef mach_id_t mprocessor_set_name_t;
typedef mprocessor_set_name_t *mprocessor_set_name_array_t;

typedef mach_id_t mtask_t;
typedef mtask_t *mtask_array_t;

typedef mach_id_t mthread_t;
typedef mthread_t *mthread_array_t;

typedef mach_id_t mobject_t;
typedef mach_id_t mobject_control_t;
typedef mach_id_t mobject_name_t;

typedef struct vm_region {
    vm_offset_t vr_address;
    vm_size_t vr_size;
/*vm_prot_t*/integer_t vr_prot;
/*vm_prot_t*/integer_t vr_max_prot;
/*vm_inherit_t*/integer_t vr_inherit;
/*boolean_t*/integer_t vr_shared;
/*mobject_name_t*/integer_t vr_name;
    vm_offset_t vr_offset;
} vm_region_t;

#include <mach/machine/thread_status.h>

#ifdef	mips
typedef struct mips_thread_state mips_thread_state_t;
#endif /* mips */

#ifdef	sun3
typedef struct sun_thread_state sun3_thread_state_t;
#endif /* sun3 */

#ifdef	sun4
typedef struct sparc_thread_state sparc_thread_state_t;
#endif	/* sun4 */

#ifdef	vax
typedef struct vax_thread_state vax_thread_state_t;
#endif /* vax */

#ifdef	i386
typedef struct i386_thread_state i386_thread_state_t;
#endif /* i386 */

#ifdef	alpha
typedef struct alpha_thread_state alpha_thread_state_t;
#endif /* alpha */

#ifdef	parisc
typedef struct parisc_thread_state parisc_thread_state_t;
#endif /* parisc */

typedef int unix_pid_t;
typedef char *unix_command_t;

#endif	/* _MACHID_TYPES_H_ */
