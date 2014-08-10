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
#ifndef _FLUX_INCLUDE_FLUX_I386_VCPI_H_
#define _FLUX_INCLUDE_FLUX_I386_VCPI_H_

struct vcpi_switch_data
{
	vm_offset_t phys_pdir;
	vm_offset_t lin_gdt;
	vm_offset_t lin_idt;
	unsigned short ldt_sel;
	unsigned short tss_sel;
	unsigned long entry_eip;
	unsigned short entry_cs;
};

#endif /* _FLUX_INCLUDE_FLUX_I386_VCPI_H_ */
