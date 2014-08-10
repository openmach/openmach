/* 
 * Copyright (c) 1996-1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _FLUX_KERNEL_I386_DOS_I16_DPMI_H_
#define _FLUX_KERNEL_I386_DOS_I16_DPMI_H_

#include <mach/inline.h>
#include <mach/machine/seg.h>

typedef unsigned short dpmi_error_t;

#define DPMI_UNSUPPORTED_FUNCTION		0x8001
#define DPMI_OBJECT_WRONG_STATE			0x8002
#define DPMI_SYSTEM_INTEGRITY			0x8003
#define DPMI_DEADLOCK				0x8004
#define DPMI_SERIALIZATION_CANCELLED		0x8005
#define DPMI_OUT_OF_RESOURCES			0x8010
#define DPMI_DESCRIPTOR_UNAVAILABLE		0x8011
#define DPMI_LINEAR_MEMORY_UNAVAILABLE		0x8012
#define DPMI_PHYSICAL_MEMORY_UNAVAILABLE	0x8013
#define DPMI_BACKING_STORE_UNAVAILABLE		0x8014
#define DPMI_CALLBACK_UNAVAILABLE		0x8015
#define DPMI_HANDLE_UNAVAILABLE			0x8016
#define DPMI_MAX_LOCK_COUNT_EXCEEDED		0x8017
#define DPMI_ALREADY_SERIALIZED_EXCLUSIVELY	0x8018
#define DPMI_ALREADY_SERIALIZED_SHARED		0x8019
#define DPMI_INVALID_VALUE			0x8021
#define DPMI_INVALID_SELECTOR			0x8022
#define DPMI_INVALID_HANDLE			0x8023
#define DPMI_INVALID_CALLBACK			0x8024
#define DPMI_INVALID_LINEAR_ADDRESS		0x8025
#define DPMI_NOT_SUPPORTED_BY_HARDWARE		0x8026

struct real_call_data; /*XXX*/

MACH_INLINE dpmi_error_t dpmi_switch_to_pmode(
	struct far_pointer_16	*pmode_entry_vector,
	unsigned short		host_data_seg)
{
	dpmi_error_t err;

	asm volatile("
		movw	%3,%%es
		lcallw	%2
		jc	1f
		xorw	%%ax,%%ax
	1:	pushw	%%ds
		popw	%%es
	" : "=a" (err)
	  : "a" (1),
	    "m" (*pmode_entry_vector),
	    "rm" (host_data_seg));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_allocate_descriptors(
	unsigned short		count,
	unsigned short		*out_selector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		movw	%%ax,%1
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=rm" (*out_selector)
	  : "a" (0x0000),
	    "c" (count));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_get_segment_base(
	unsigned short		selector,
	unsigned long		*out_base)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
		shll	$16,%ecx
		movw	%dx,%cx
	1:
	" : "=a" (err),
	    "=c" (*out_base)
	  : "a" (0x0006),
	    "b" (selector)
	  : "edx");

	return err;
}

MACH_INLINE dpmi_error_t dpmi_set_segment_base(
	unsigned short		selector,
	unsigned long		base)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0007),
	    "b" (selector),
	    "c" (base >> 16),
	    "d" (base));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_set_segment_limit(
	unsigned short		selector,
	unsigned		limit)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0008),
	    "b" (selector),
	    "c" (limit >> 16),
	    "d" (limit));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_create_code_segment_alias(
	unsigned short		code_selector,
	unsigned short		*out_data_selector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		movw	%%ax,%1
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=rm" (*out_data_selector)
	  : "a" (0x000a),
	    "b" (code_selector));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_get_descriptor(
	unsigned short		selector,
	struct i386_descriptor	*out_descriptor)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x000b),
	    "b" (selector),
	    "D" (out_descriptor));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_set_descriptor(
	unsigned short		selector,
	struct i386_descriptor	*descriptor)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x000c),
	    "b" (selector),
	    "D" (descriptor));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_allocate_specific_descriptor(
	unsigned short		selector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x000d),
	    "b" (selector));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_get_exception_handler(
	unsigned char		trapno,
	struct far_pointer_32	*out_vector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=c" (out_vector->seg),
	    "=d" (out_vector->ofs)
	  : "a" (0x0202),
	    "b" (trapno));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_set_exception_handler(
	unsigned char		trapno,
	struct far_pointer_32	*vector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0203),
	    "b" (trapno),
	    "c" (vector->seg),
	    "d" (vector->ofs));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_get_interrupt_handler(
	unsigned char		intvec,
	struct far_pointer_32	*out_vector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=c" (out_vector->seg),
	    "=d" (out_vector->ofs)
	  : "a" (0x0204),
	    "b" (intvec));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_set_interrupt_handler(
	unsigned char		intvec,
	struct far_pointer_32	*vector)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0205),
	    "b" (intvec),
	    "c" (vector->seg),
	    "d" (vector->ofs));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_simulate_real_mode_interrupt(
	unsigned char		intnum,
	struct real_call_data	*call_data)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0300),
	    "b" ((unsigned short)intnum),
	    "c" (0),
	    "D" (call_data));

	return err;
}

struct dpmi_version_status
{
	unsigned char minor_version;
	unsigned char major_version;
	unsigned short flags;
	unsigned char slave_pic_base;
	unsigned char master_pic_base;
	unsigned char processor_type;
};

MACH_INLINE void dpmi_get_version(struct dpmi_version_status *status)
{
	asm volatile("
		int	$0x31
	" : "=a" (*((short*)&status->minor_version)),
	    "=b" (status->flags),
	    "=c" (status->processor_type),
	    "=d" (*((short*)&status->slave_pic_base))
	  : "a" (0x0400));
}

MACH_INLINE dpmi_error_t dpmi_allocate_memory(
	unsigned		size,
	unsigned		*out_linear_addr,
	unsigned		*out_mem_handle)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		shll	$16,%%ebx
		movw	%%cx,%%bx
		shll	$16,%%esi
		movw	%%di,%%si
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=b" (*out_linear_addr),
	    "=S" (*out_mem_handle)
	  : "a" (0x0501),
	    "b" (size >> 16),
	    "c" (size)
	  : "ebx", "ecx", "esi", "edi");

	return err;
}

MACH_INLINE dpmi_error_t dpmi_free_memory(
	unsigned		mem_handle)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0502),
	    "S" (mem_handle >> 16),
	    "D" (mem_handle));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_allocate_linear_memory(
	unsigned		linear_addr,
	unsigned		size,
	unsigned		flags,
	unsigned		*out_linear_addr,
	unsigned		*out_mem_handle)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=b" (*out_linear_addr),
	    "=S" (*out_mem_handle)
	  : "a" (0x0504),
	    "b" (linear_addr),
	    "c" (size),
	    "d" (flags));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_resize_linear_memory(
	unsigned		handle,
	unsigned		new_size,
	unsigned		flags,
	unsigned short		*update_selector_array,
	unsigned		update_selector_count,
	unsigned		*out_new_linear_addr)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=b" (*out_new_linear_addr)
	  : "a" (0x0505),
	    "b" (update_selector_array),
	    "c" (new_size),
	    "d" (flags),
	    "S" (handle),
	    "D" (update_selector_count));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_map_conventional_memory(
	unsigned		handle,
	vm_offset_t		offset,
	vm_offset_t		low_addr,
	vm_size_t		page_count)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0509),
	    "S" (handle),
	    "b" (offset),
	    "c" (page_count),
	    "d" (low_addr));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_lock_linear_region(
	vm_offset_t		start_la,
	vm_size_t		size)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0600),
	    "b" (start_la >> 16),
	    "c" (start_la),
	    "S" (size >> 16),
	    "D" (size));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_unlock_linear_region(
	vm_offset_t		start_la,
	vm_size_t		size)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		xorw	%%ax,%%ax
	1:
	" : "=a" (err)
	  : "a" (0x0601),
	    "b" (start_la >> 16),
	    "c" (start_la),
	    "S" (size >> 16),
	    "D" (size));

	return err;
}

MACH_INLINE dpmi_error_t dpmi_get_page_size(
	unsigned		*out_page_size)
{
	dpmi_error_t err;

	asm volatile("
		int	$0x31
		jc	1f
		shll	$16,%%ebx
		movw	%%cx,%%bx
		xorw	%%ax,%%ax
	1:
	" : "=a" (err),
	    "=b" (*out_page_size)
	  : "a" (0x0604)
	  : "ecx");

	return err;
}


#endif /* _FLUX_KERNEL_I386_DOS_I16_DPMI_H_ */
