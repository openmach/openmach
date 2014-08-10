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
 *	File: scsi_endian.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	6/91
 *
 *	Byte/Bit order issues are solved here.
 */


#ifndef	_SCSI_ENDIAN_H_
#define	_SCSI_ENDIAN_H_	1

/*
 * Macros to take care of bitfield placement within a byte.
 * It might be possible to extend these to something that
 * takes care of multibyte structures, using perhaps the
 * type ("t") parameter.  Someday.
 */
#if	BYTE_MSF

#define	BITFIELD_2(t,a,b)		t b,a
#define	BITFIELD_3(t,a,b,c)		t c,b,a
#define	BITFIELD_4(t,a,b,c,d)		t d,c,b,a
#define	BITFIELD_5(t,a,b,c,d,e)		t e,d,c,b,a
#define	BITFIELD_6(t,a,b,c,d,e,f)	t f,e,d,c,b,a
#define	BITFIELD_7(t,a,b,c,d,e,f,g)	t g,f,e,d,c,b,a
#define	BITFIELD_8(t,a,b,c,d,e,f,g,h)	t h,g,f,e,d,c,b,a

#else	/*BYTE_MSF*/

#define	BITFIELD_2(t,a,b)		t a,b
#define	BITFIELD_3(t,a,b,c)		t a,b,c
#define	BITFIELD_4(t,a,b,c,d)		t a,b,c,d
#define	BITFIELD_5(t,a,b,c,d,e)		t a,b,c,d,e
#define	BITFIELD_6(t,a,b,c,d,e,f)	t a,b,c,d,e
#define	BITFIELD_7(t,a,b,c,d,e,f,g)	t a,b,c,d,e,f,g
#define	BITFIELD_8(t,a,b,c,d,e,f,g,h)	t a,b,c,d,e,f,g,h

#endif	/*BYTE_MSF*/

#endif	/*_SCSI_ENDIAN_H_*/
