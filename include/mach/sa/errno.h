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
 * This header file defines a set of POSIX errno values
 * that fits consistently into the Mach error code "space" -
 * i.e. these error code values can be mixed with kern_return_t's
 * and mach_msg_return_t's and such without conflict.
 * Higher-level services are not required to use these values
 * (or, for that matter, any of the mach/sa header files),
 * but if they use other values of their own choosing,
 * those values may conflict with values in the Mach error code space,
 * making it necessary to keep the different types of error codes separate.
 *
 * (For example, Lites uses BSD's errno values,
 * which conflict with Mach's kern_return_t values,
 * and therefore must carefully distinguish between BSD and Mach error codes
 * and never return one type when the other is expected, etc. -
 * we've found this to be a frequent source of bugs.)
 *
 * One (probably the main) disadvantage of using these error codes
 * is that, since they don't start from around 0 like typical Unix errno values,
 * it's impossible to provide a conventional Unix-style sys_errlist table for them.
 * However, they are compatible with the POSIX-blessed strerror and perror routines.
 */
#ifndef _MACH_SA_ERRNO_H_
#define _MACH_SA_ERRNO_H_

extern int errno;			/* global error number */

/* ISO/ANSI C-1990 errors */
#define	EDOM		0xc001		/* Numerical argument out of domain */
#define	ERANGE		0xc002		/* Result too large */

/* POSIX-1990 errors */
#define	E2BIG		0xc003		/* Argument list too long */
#define	EACCES		0xc004		/* Permission denied */
#define	EAGAIN		0xc005		/* Resource temporarily unavailable */
#define	EBADF		0xc006		/* Bad file descriptor */
#define	EBUSY		0xc007		/* Device busy */
#define	ECHILD		0xc008		/* No child processes */
#define	EDEADLK		0xc009		/* Resource deadlock avoided */
#define	EEXIST		0xc00a		/* File exists */
#define	EFAULT		0xc00b		/* Bad address */
#define	EFBIG		0xc00c		/* File too large */
#define	EINTR		0xc00d		/* Interrupted system call */
#define	EINVAL		0xc00e		/* Invalid argument */
#define	EIO		0xc00f		/* Input/output error */
#define	EISDIR		0xc010		/* Is a directory */
#define	EMFILE		0xc011		/* Too many open files */
#define	EMLINK		0xc012		/* Too many links */
#define	ENAMETOOLONG	0xc013		/* File name too long */
#define	ENFILE		0xc014		/* Too many open files in system */
#define	ENODEV		0xc015		/* Operation not supported by device */
#define	ENOENT		0xc016		/* No such file or directory */
#define	ENOEXEC		0xc017		/* Exec format error */
#define	ENOLCK		0xc018		/* No locks available */
#define	ENOMEM		0xc019		/* Cannot allocate memory */
#define	ENOSPC		0xc01a		/* No space left on device */
#define	ENOSYS		0xc01b		/* Function not implemented */
#define	ENOTDIR		0xc01c		/* Not a directory */
#define	ENOTEMPTY	0xc01d		/* Directory not empty */
#define	ENOTTY		0xc01e		/* Inappropriate ioctl for device */
#define	ENXIO		0xc01f		/* Device not configured */
#define	EPERM		0xc020		/* Operation not permitted */
#define	EPIPE		0xc021		/* Broken pipe */
#define	EROFS		0xc022		/* Read-only file system */
#define	ESPIPE		0xc023		/* Illegal seek */
#define	ESRCH		0xc024		/* No such process */
#define	EXDEV		0xc025		/* Cross-device link */

/* POSIX-1993 errors */
#define EBADMSG		0xc026
#define ECANCELED	0xc027
#define EINPROGRESS	0xc028
#define EMSGSIZE	0xc029
#define ENOTSUP		0xc02a

#endif _MACH_SA_ERRNO_H_
