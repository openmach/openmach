/* 
 * Copyright (c) 1994 The University of Utah and
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
#ifndef _MACH_A_OUT_
#define _MACH_A_OUT_

struct exec
{
        unsigned long  	a_magic;        /* magic number */
        unsigned long   a_text;         /* size of text segment */
        unsigned long   a_data;         /* size of initialized data */
        unsigned long   a_bss;          /* size of uninitialized data */
        unsigned long   a_syms;         /* size of symbol table */
        unsigned long   a_entry;        /* entry point */
        unsigned long   a_trsize;       /* size of text relocation */
        unsigned long   a_drsize;       /* size of data relocation */
};

struct nlist {
	long n_strx;
	unsigned char n_type;
	char n_other;
	short n_desc;
	unsigned long n_value;
};

#define OMAGIC 0407
#define NMAGIC 0410
#define ZMAGIC 0413
#define QMAGIC 0314

#define N_GETMAGIC(ex) \
	( (ex).a_magic & 0xffff )
#define N_GETMAGIC_NET(ex) \
	(ntohl((ex).a_magic) & 0xffff)

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	(N_GETMAGIC(ex) != OMAGIC && N_GETMAGIC(ex) != NMAGIC && \
	 N_GETMAGIC(ex) != ZMAGIC && N_GETMAGIC(ex) != QMAGIC && \
	 N_GETMAGIC_NET(ex) != OMAGIC && N_GETMAGIC_NET(ex) != NMAGIC && \
	 N_GETMAGIC_NET(ex) != ZMAGIC && N_GETMAGIC_NET(ex) != QMAGIC)

/* We don't provide any N_???OFF macros here
   because they vary too much between the different a.out variants;
   it's practically impossible to create one set of macros
   that works for UX, FreeBSD, NetBSD, Linux, etc.  */

#endif /* _MACH_A_OUT_ */
