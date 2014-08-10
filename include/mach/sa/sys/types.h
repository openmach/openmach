/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
#ifndef	_MACH_SA_SYS_TYPES_H_
#define	_MACH_SA_SYS_TYPES_H_

#include <mach/machine/vm_types.h>

#ifndef _SIZE_T
#define _SIZE_T
typedef natural_t size_t;
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
typedef integer_t ssize_t;
#endif

typedef	unsigned short	dev_t;		/* device id */
typedef	unsigned long	gid_t;		/* group id */
typedef	unsigned long	ino_t;		/* inode number */
typedef	unsigned short	mode_t;		/* permissions */
typedef	unsigned short	nlink_t;	/* link count */
typedef	natural_t	off_t;		/* file offset */
typedef	unsigned long	uid_t;		/* user id */


/* Symbols allowed but not required by POSIX */

typedef	char *		caddr_t;	/* address of a (signed) char */

#ifndef _TIME_T
#define	_TIME_T
typedef	unsigned int	time_t;
#endif

#define RAND_MAX	0x7fffffff

/* Symbols not allowed by POSIX */
#ifndef _POSIX_SOURCE

/*
 * Common type definitions that lots of old files seem to want.
 */

typedef	unsigned char	u_char;		/* unsigned char */
typedef	unsigned short	u_short;	/* unsigned short */
typedef	unsigned int	u_int;		/* unsigned int */
typedef unsigned long	u_long;		/* unsigned long */

typedef struct _quad_ {
	unsigned int	val[2];		/* 2 32-bit values make... */
} quad;					/* an 8-byte item */

typedef unsigned int	daddr_t;	/* disk address */

#define	major(i)	(((i) >> 8) & 0xFF)
#define	minor(i)	((i) & 0xFF)
#define	makedev(i,j)	((((i) & 0xFF) << 8) | ((j) & 0xFF))

#define	NBBY		8

#ifndef	NULL
#define	NULL		((void *) 0) 	/* the null pointer */
#endif

#endif /* _POSIX_SOURCE */

#endif	/* _MACH_SA_SYS_TYPES_H_ */
