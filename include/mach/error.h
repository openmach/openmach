/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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
 * File:	mach/error.h
 * Purpose:
 *	error module definitions
 *
 */

#ifndef	_MACH_ERROR_H_
#define _MACH_ERROR_H_
#include <mach/kern_return.h>

/*
 *	error number layout as follows:
 *
 *	hi		 		       lo
 *	| system(6) | subsystem(12) | code(14) |
 */


#define	err_none		(mach_error_t)0
#define ERR_SUCCESS		(mach_error_t)0
#define	ERR_ROUTINE_NIL		(mach_error_fn_t)0


#define	err_system(x)		(((x)&0x3f)<<26)
#define err_sub(x)		(((x)&0xfff)<<14)

#define err_get_system(err)	(((err)>>26)&0x3f)
#define err_get_sub(err)	(((err)>>14)&0xfff)
#define err_get_code(err)	((err)&0x3fff)

#define system_emask		(err_system(0x3f))
#define sub_emask		(err_sub(0xfff))
#define code_emask		(0x3fff)


/*	Mach error systems	*/
#define	err_kern		err_system(0x0)		/* kernel */
#define	err_us			err_system(0x1)		/* user space library */
#define	err_server		err_system(0x2)		/* user space servers */
#define	err_ipc			err_system(0x3)		/* old ipc errors */
#define err_mach_ipc		err_system(0x4)		/* mach-ipc errors */
#define err_bootstrap		err_system(0x5)		/* bootstrap errors */
#define err_hurd		err_system(0x10)	/* GNU Hurd server errors */
#define err_local		err_system(0x3e)	/* user defined errors */
#define	err_ipc_compat		err_system(0x3f)	/* (compatibility) mach-ipc errors */

#define	err_max_system		0x3f


/*	special old "subsystems" that don't really follow the above rules */
#define err_mig			-300
#define err_exec		6000

/*	unix errors get lumped into one subsystem  */
#define err_unix		(err_kern|err_sub(3))
#define	unix_err(errno)		(err_kern|err_sub(3)|errno)

/*	MS-DOS extended error codes */
#define err_dos			(err_kern|err_sub(0xd05))

/*	Flux OS error systems */
#define err_fluke		err_system(0x20)	/* Fluke API */

#ifndef ASSEMBLER
typedef	kern_return_t	mach_error_t;
typedef mach_error_t	(* mach_error_fn_t)();
#endif /* ASSEMBLER */

#endif	/* _MACH_ERROR_H_ */
