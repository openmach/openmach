/*
 * Defintions for Linux driver emulation.
 *
 * Copyright (C) 1996 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 	Author: Shantanu Goel, University of Utah CSL
 */

#include <i386/ipl.h>

extern int linux_auto_config;
extern int linux_intr_pri;

int linux_to_mach_error (int);
void *alloc_contig_mem (unsigned, unsigned, unsigned, vm_page_t *);
void free_contig_mem (vm_page_t);
void collect_buffer_pages (void);
