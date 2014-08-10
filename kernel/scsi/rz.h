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
 *	File: rz.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Mapping between U*x-like indexing and controller+slave
 *	Each controller handles at most 8 slaves, few controllers.
 */

#if 0
#define	rzcontroller(dev)	(((dev)>>6)&0x3)
#define	rzslave(dev)		(((dev)>>3)&0x7)
#endif 0
#define	rzcontroller(dev)	(((dev)>>13)&0x3)
#define rzslave(dev)		(((dev)>>10)&0x7)

#if 0
#define	rzpartition(dev)	((PARTITION_TYPE(dev)==0xf)?MAXPARTITIONS:((dev)&0x7))
#endif 0
#define rzpartition(dev)	((dev)&0x3ff)

/* To address the full 256 luns use upper bits 8..12 */
/* NOTE: Under U*x this means the next major up.. what a mess */
#define rzlun(dev)		(((dev)&0x7) | (((dev)>>5)&0xf8))

/* note: whatever this was used for is no longer cared about -- Kevin */
#define PARTITION_TYPE(dev)	(((dev)>>24)&0xf)
#define PARTITION_ABSOLUTE	(0xf<<24)

#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
#define tape_unit(dev)		((((dev)&0xe0)>>3)|((dev)&0x3))
#define	TAPE_UNIT(dev)		((dev)&(~0xff))|(tape_unit((dev))<<3)
#define	TAPE_REWINDS(dev)	(((dev)&0x1c)==0)||(((dev)&0x1c)==8)
#endif	/*MACH_KERNEL*/
