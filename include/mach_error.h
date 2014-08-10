/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
/*
 *	File:	mach_error.h
 *	Author:	Douglas Orr, Carnegie Mellon University
 *	Date:	Mar. 1988
 *
 *	Definitions of routines in mach_error.c
 */

#ifndef	_MACH_ERROR_
#define	_MACH_ERROR_	1

#include <mach/error.h>

char		*mach_error_string(
/*
 *	Returns a string appropriate to the error argument given
 */
#if	c_plusplus
	mach_error_t error_value
#endif	c_plusplus
				);

void		mach_error(
/*
 *	Prints an appropriate message on the standard error stream
 */
#if	c_plusplus
	char 		*str,
	mach_error_t	error_value
#endif	c_plusplus
				);

char		*mach_error_type(
/*
 *	Returns a string with the error system, subsystem and code
*/
#if	c_plusplus
	mach_error_t	error_value
#endif  c_plusplus
				);

#endif	_MACH_ERROR_
