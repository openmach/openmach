/* 
 * Copyright (c) 1995-1994 The University of Utah and
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
#ifndef _I386_PC_REAL_CALL_H_
#define _I386_PC_REAL_CALL_H_

/* This structure happens to correspond to the DPMI real-call structure.  */
struct real_call_data
{
	unsigned edi;
	unsigned esi;
	unsigned ebp;
	unsigned reserved;
	unsigned ebx;
	unsigned edx;
	unsigned ecx;
	unsigned eax;
	unsigned short flags;
	unsigned short es;
	unsigned short ds;
	unsigned short fs;
	unsigned short gs;
	unsigned short ip;
	unsigned short cs;
	unsigned short sp;
	unsigned short ss;
};

/* Code segment we originally had when we started in real mode.  */
extern unsigned short real_cs;

extern void (*real_int)(int intnum, struct real_call_data *rcd);
extern void (*real_exit)(int rc);

#define real_call_data_init(rcd)	\
	({ (rcd)->flags = 0;		\
	   (rcd)->ss = 0;		\
	   (rcd)->sp = 0;		\
	})

#endif /* _I386_PC_REAL_CALL_H_ */
