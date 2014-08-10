/* 
 * Copyright (c) 1995 The University of Utah and
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
/*
 * PC-specific flag bits and priority values
 * for the List Memory Manager (LMM)
 * relevant for kernels managing physical memory.
 */
#ifndef _I386_PC_PHYS_MEM_H_
#define _I386_PC_PHYS_MEM_H_

#include_next "phys_mem.h"

/* <1MB memory is most precious, then <16MB memory, then high memory.
   Assign priorities to each region accordingly
   so that high memory will be used first when possible,
   then 16MB memory, then 1MB memory.  */
#define LMM_PRI_1MB	-2
#define LMM_PRI_16MB	-1
#define LMM_PRI_HIGH	0

/* For memory <1MB, both LMMF_1MB and LMMF_16MB will be set.
   For memory from 1MB to 16MB, only LMMF_16MB will be set.
   For all memory higher than that, neither will be set.  */
#define LMMF_1MB	0x01
#define LMMF_16MB	0x02


/* Call one of these routines to add a chunk of physical memory found
   to the malloc_lmm free list.
   It assigns the appropriate flags and priorities to the region,
   as defined above, breaking up the region if necessary.  */
void phys_mem_add(vm_offset_t min, vm_size_t size);
void i16_phys_mem_add(vm_offset_t min, vm_size_t size);

#endif _I386_PC_PHYS_MEM_H_
