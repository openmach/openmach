/* 
 * Mach Operating System
 * Copyright (c) 1993,1991,1990,1989 Carnegie Mellon University
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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	8/88
 */

#ifndef	_DEVICE_CONF_H_
#define	_DEVICE_CONF_H_

#include <mach/machine/vm_types.h>

/*
 * Operations list for major device types.
 */
struct dev_ops {
	char *    	d_name;		/* name for major device */
	int		(*d_open)();	/* open device */
	int		(*d_close)();	/* close device */
	int		(*d_read)();	/* read */
	int		(*d_write)();	/* write */
	int		(*d_getstat)();	/* get status/control */
	int		(*d_setstat)();	/* set status/control */
	vm_offset_t	(*d_mmap)();	/* map memory */
	int		(*d_async_in)();/* asynchronous input setup */
	int		(*d_reset)();	/* reset device */
	int		(*d_port_death)();
					/* clean up reply ports */
	int		d_subdev;	/* number of sub-devices per
					   unit */
	int		(*d_dev_info)(); /* driver info for kernel */
};
typedef struct dev_ops *dev_ops_t;

/*
 * Routines for null entries.
 */
extern int	nulldev();		/* no operation - OK */
extern int	nodev();		/* no operation - error */
extern vm_offset_t nomap();		/* no operation - error */

/*
 * Flavor constants for d_dev_info routine
 */
#define D_INFO_BLOCK_SIZE	1

/*
 * Head of list of attached devices
 */
extern struct dev_ops	dev_name_list[];
extern int		dev_name_count;

/*
 * Macro to search device list
 */
#define	dev_search(dp)	\
	for (dp = dev_name_list; \
	     dp < &dev_name_list[dev_name_count]; \
	     dp++)

/*
 * Indirection vectors for certain devices.
 */
struct dev_indirect {
	char *		d_name;		/* name for device */
	dev_ops_t	d_ops;		/* operations (major device) */
	int		d_unit;		/* and unit number */
};
typedef struct dev_indirect	*dev_indirect_t;

/*
 * List of indirect devices.
 */
extern struct dev_indirect	dev_indirect_list[];
extern int			dev_indirect_count;

/*
 * Macro to search indirect list
 */
#define	dev_indirect_search(di) \
	for (di = dev_indirect_list; \
	     di < &dev_indirect_list[dev_indirect_count]; \
	     di++)

/*
 * Exported routine to set indirection.
 */
extern void	dev_set_indirect();

#endif	/* _DEVICE_CONF_H_ */

