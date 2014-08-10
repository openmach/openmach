/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie Mellon rights
 * to redistribute these changes.
 */
/*
 *	Each kernel has a major and minor version number.  Changes in
 *	the major number in general indicate a change in exported features.
 *	Changes in minor number usually correspond to internal-only
 *	changes that the user need not be aware of (in general).  These
 *	values are stored at boot time in the machine_info strucuture and
 *	can be obtained by user programs with the host_info kernel call.
 *	This mechanism is intended to be the formal way for Mach programs
 *	to provide for backward compatibility in future releases.
 *
 *	[ This needs to be reconciled somehow with the major/minor version
 *	  number stuffed into the version string - mja, 5/8/87 ]
 *
 *	Following is an informal history of the numbers:
 *
 *	25-March-87  Avadis Tevanian, Jr.
 *		Created version numbering scheme.  Started with major 1,
 *		minor 0.
 */

#define KERNEL_MAJOR_VERSION	4
#define KERNEL_MINOR_VERSION	0

/* 
 *  Version number of the kernel include files.
 *
 *  This number must be changed whenever an incompatible change is made to one
 *  or more of our include files which are used by application programs that
 *  delve into kernel memory.  The number should normally be simply incremented
 *  but may actually be changed in any manner so long as it differs from the
 *  numbers previously assigned to any other versions with which the current
 *  version is incompatible.  It is used at boot time to determine which
 *  versions of the system programs to install.
 *
 *  Note that the symbol _INCLUDE_VERSION must be set to this in the symbol
 *  table.  On the VAX for example, this is done in locore.s.
 */

/*
 * Current allocation strategy: bump either branch by 2, until non-MACH is
 * excised from the CSD environment.
 */
#define	INCLUDE_VERSION	0
