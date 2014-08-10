/*
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University.
 * Copyright (c) 1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 * i386-specific routines for loading a.out files.
 */

#include <mach/exec/a.out.h>
#include <mach/exec/exec.h>

/* See below...
   Must be a power of two.
   This should be kept small, because we may be running on a small stack,
   and this code is not likely to be performance-critical anyway.  */
#define SCAN_CHUNK	256

int exec_load_aout(exec_read_func_t *read, exec_read_exec_func_t *read_exec,
		   void *handle, exec_info_t *out_info)
{
	struct exec	x;
	vm_size_t	actual;
	vm_offset_t	text_start;	/* text start in memory */
	vm_size_t	text_size;
	vm_offset_t	text_offset;	/* text offset in file */
	vm_size_t	data_size;
	int		err;

	/* Read the exec header.  */
	if (err = (*read)(handle, 0, &x, sizeof(x), &actual))
		return err;
	if (actual != sizeof(x))
		return EX_NOT_EXECUTABLE;

	/*printf("get_loader_info: magic %04o\n", (int)x.a_magic);*/

	switch ((int)x.a_magic & 0xFFFF) {

	    case OMAGIC:
		text_start  = 0;
		text_size   = 0;
		text_offset = sizeof(struct exec);
		data_size   = x.a_text + x.a_data;
		break;

	    case NMAGIC:
		text_start  = 0;
		text_size   = x.a_text;
		text_offset = sizeof(struct exec);
		data_size   = x.a_data;
		break;

	    case ZMAGIC:
	    {
	    	char buf[SCAN_CHUNK];

		/* This kludge is not for the faint-of-heart...
		   Basically we're trying to sniff out the beginning of the text segment.
		   We assume that the first nonzero byte is the first byte of code,
		   and that x.a_entry is the virtual address of that first byte.  */
		for (text_offset = 0; ; text_offset++)
		{
			if ((text_offset & (SCAN_CHUNK-1)) == 0)
			{
				if (err = (*read)(handle, text_offset, buf, SCAN_CHUNK, &actual))
					return err;
				if (actual < SCAN_CHUNK)
					buf[actual] = 0xff; /* ensure termination */
				if (text_offset == 0)
					text_offset = sizeof(struct exec);
			}
			if (buf[text_offset & (SCAN_CHUNK-1)])
				break;
		}

		/* Account for the (unlikely) event that the first instruction
		   is actually an add instruction with a zero opcode.
		   Surely every a.out variant should be sensible enough at least
		   to align the text segment on a 32-byte boundary...  */
		text_offset &= ~0x1f;

		text_start = x.a_entry;
		text_size = x.a_text;
		data_size   = x.a_data;
		break;
	    }

	    case QMAGIC:
		text_start	= 0x1000;
		text_offset	= 0;
		text_size	= x.a_text;
		data_size	= x.a_data;
		break;

	    default:
		/* Check for NetBSD big-endian ZMAGIC executable */
		if ((int)x.a_magic == 0x0b018600) {
			text_start  = 0x1000;
			text_size   = x.a_text;
			text_offset = 0;
			data_size   = x.a_data;
			break;
		}
		return (EX_NOT_EXECUTABLE);
	}

	/* If the text segment overlaps the same page as the beginning of the data segment,
	   then cut the text segment short and grow the data segment appropriately.  */
	if ((text_start + text_size) & 0xfff)
	{
		vm_size_t incr = (text_start + text_size) & 0xfff;
		if (incr > text_size) incr = text_size;
		text_size -= incr;
		data_size += incr;
	}

	/*printf("exec_load_aout: text_start %08x text_offset %08x text_size %08x data_size %08x\n",
		text_start, text_offset, text_size, data_size);*/

	/* Load the read-only text segment, if any.  */
	if (text_size > 0)
	{
		if (err = (*read_exec)(handle, text_offset, text_size,
				       text_start, text_size,
				       EXEC_SECTYPE_READ |
				       EXEC_SECTYPE_EXECUTE |
				       EXEC_SECTYPE_ALLOC |
				       EXEC_SECTYPE_LOAD))
			return err;
	}

	/* Load the read-write data segment, if any.  */
	if (data_size + x.a_bss > 0)
	{
		if (err = (*read_exec)(handle,
				       text_offset + text_size,
				       data_size,
				       text_start + text_size,
				       data_size + x.a_bss,
				       EXEC_SECTYPE_READ |
				       EXEC_SECTYPE_WRITE |
				       EXEC_SECTYPE_EXECUTE |
				       EXEC_SECTYPE_ALLOC |
				       EXEC_SECTYPE_LOAD))
			return err;
	}

	/* XXX symbol table */

	out_info->entry = x.a_entry;

	return(0);
}
