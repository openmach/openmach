/* 
 * Mach Operating System
 * Copyright (c) 1994-1989 Carnegie Mellon University
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
 *	File: busses.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	4/90
 *
 *	Structures used by configuration routines to
 *	explore a given bus structure.
 */

#ifndef	_CHIPS_BUSSES_H_
#define	_CHIPS_BUSSES_H_

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>

/*
 *
 * This is mildly modeled after the Unibus on Vaxen,
 * one of the most complicated bus structures.
 * Therefore, let's hope this can be done once and for all.
 *
 * At the bottom level there is a "bus_device", which
 * might exist in isolation (e.g. a clock on the CPU
 * board) or be a standard component of an architecture
 * (e.g. the bitmap display on some workstations).
 *
 * Disk devices and communication lines support multiple
 * units, hence the "bus_driver" structure which is more
 * flexible and allows probing and dynamic configuration
 * of the number and type of attached devices.
 *
 * At the top level there is a "bus_ctlr" structure, used
 * in systems where the I/O bus(ses) are separate from
 * the memory bus(ses), and/or when memory boards can be
 * added to the main bus (and they must be config-ed
 * and/or can interrupt the processor for ECC errors).
 *
 * The autoconfiguration process typically starts at
 * the top level and walks down tables that are
 * defined either in a generic file or are specially
 * created by config.
 */

/*
 * Per-controller structure.
 */
struct bus_ctlr {
	struct bus_driver  *driver;	/* myself, as a device */
	char		   *name;	/* readability */
	int		    unit;	/* index in driver */
	int		  (*intr)();	/* interrupt handler(s) */
	vm_offset_t	    address;	/* device virtual address */
	int		    am;		/* address modifier */
	vm_offset_t	    phys_address;/* device phys address */
	char		    adaptor;	/* slot where found */
	char		    alive;	/* probed successfully */
	char		    flags;	/* any special conditions */
	vm_offset_t	    sysdep;	/* On some systems, queue of
					 * operations in-progress */
	natural_t	    sysdep1;	/* System dependent */
};


/*
 * Per-``device'' structure
 */
struct bus_device {
	struct bus_driver  *driver;	/* autoconf info */
	char		   *name;	/* my name */
	int		    unit;
	int		  (*intr)();
	vm_offset_t	    address;	/* device address */
	int		    am;		/* address modifier */
	vm_offset_t	    phys_address;/* device phys address */
	char		    adaptor;
	char		    alive;
	char		    ctlr;
	char		    slave;
	int		    flags;
	struct bus_ctlr    *mi;		/* backpointer to controller */
	struct bus_device  *next;	/* optional chaining */
	vm_offset_t	    sysdep;	/* System dependent */
	natural_t	    sysdep1;	/* System dependent */
};

/*
 * General flag definitions
 */
#define BUS_INTR_B4_PROBE  0x01		/* enable interrupts before probe */
#define BUS_INTR_DISABLED  0x02		/* ignore all interrupts */
#define	BUS_CTLR	   0x04		/* descriptor for a bus adaptor */
#define BUS_XCLU	   0x80		/* want exclusive use of bdp's */

/*
 * Per-driver structure.
 *
 * Each bus driver defines entries for a set of routines
 * that are used at boot time by the configuration program.
 */
struct bus_driver {
	int	(*probe)(		/* see if the driver is there */
		    /*	vm_offset_t	address,
			struct bus_ctlr * */ );
	int	(*slave)(          	/* see if any slave is there */	
		    /*	struct bus_device *,
			vm_offset_t	  */ );
	void	(*attach)(		/* setup driver after probe */
		    /*	struct bus_device * */);
	int	(*dgo)();		/* start transfer */
	vm_offset_t *addr;		/* device csr addresses */
	char	*dname;			/* name of a device */
	struct	bus_device **dinfo;	/* backpointers to init structs */
	char	*mname;			/* name of a controller */
	struct	bus_ctlr **minfo;	/* backpointers to init structs */
	int	flags;
};

#ifdef	KERNEL
extern struct bus_ctlr		bus_master_init[];
extern struct bus_device	bus_device_init[];

extern boolean_t configure_bus_master(char *, vm_offset_t, vm_offset_t,
				      int, char * );
extern boolean_t configure_bus_device(char *, vm_offset_t, vm_offset_t,
				      int, char * );
#endif	/* KERNEL */


#endif	/* _CHIPS_BUSSES_H_ */
