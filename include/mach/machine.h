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
/*	File:	machine.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1986
 *
 *	Machine independent machine abstraction.
 */

#ifndef	_MACH_MACHINE_H_
#define _MACH_MACHINE_H_

#ifdef	MACH_KERNEL
#include <cpus.h>
#endif	/* MACH_KERNEL */

#include <mach/machine/vm_types.h>
#include <mach/boolean.h>

/*
 *	For each host, there is a maximum possible number of
 *	cpus that may be available in the system.  This is the
 *	compile-time constant NCPUS, which is defined in cpus.h.
 *
 *	In addition, there is a machine_slot specifier for each
 *	possible cpu in the system.
 */

struct machine_info {
	integer_t	major_version;	/* kernel major version id */
	integer_t	minor_version;	/* kernel minor version id */
	integer_t	max_cpus;	/* max number of cpus compiled */
	integer_t	avail_cpus;	/* number actually available */
	vm_size_t	memory_size;	/* size of memory in bytes */
};

typedef struct machine_info	*machine_info_t;
typedef struct machine_info	machine_info_data_t;	/* bogus */

typedef integer_t	cpu_type_t;
typedef integer_t	cpu_subtype_t;

#define CPU_STATE_MAX		3

#define CPU_STATE_USER		0
#define CPU_STATE_SYSTEM	1
#define CPU_STATE_IDLE		2

struct machine_slot {
/*boolean_t*/integer_t	is_cpu;		/* is there a cpu in this slot? */
	cpu_type_t	cpu_type;	/* type of cpu */
	cpu_subtype_t	cpu_subtype;	/* subtype of cpu */
/*boolean_t*/integer_t	running;	/* is cpu running */
	integer_t	cpu_ticks[CPU_STATE_MAX];
	integer_t	clock_freq;	/* clock interrupt frequency */
};

typedef struct machine_slot	*machine_slot_t;
typedef struct machine_slot	machine_slot_data_t;	/* bogus */

#ifdef	MACH_KERNEL
extern struct machine_info	machine_info;
extern struct machine_slot	machine_slot[NCPUS];
#endif	/* MACH_KERNEL */

/*
 *	Machine types known by all.
 *
 *	When adding new types & subtypes, please also update slot_name.c
 *	in the libmach sources.
 */

#define CPU_TYPE_VAX		((cpu_type_t) 1)
#define CPU_TYPE_ROMP		((cpu_type_t) 2)
#define CPU_TYPE_MC68020	((cpu_type_t) 3)
#define CPU_TYPE_NS32032	((cpu_type_t) 4)
#define CPU_TYPE_NS32332        ((cpu_type_t) 5)
#define CPU_TYPE_NS32532        ((cpu_type_t) 6)
#define CPU_TYPE_I386		((cpu_type_t) 7)
#define CPU_TYPE_MIPS		((cpu_type_t) 8)
#define	CPU_TYPE_MC68030	((cpu_type_t) 9)
#define CPU_TYPE_MC68040	((cpu_type_t) 10)
#define CPU_TYPE_HPPA           ((cpu_type_t) 11)
#define CPU_TYPE_ARM		((cpu_type_t) 12)
#define CPU_TYPE_MC88000	((cpu_type_t) 13)
#define CPU_TYPE_SPARC		((cpu_type_t) 14)
#define CPU_TYPE_I860		((cpu_type_t) 15)
#define	CPU_TYPE_ALPHA		((cpu_type_t) 16)

/*
 *	Machine subtypes (these are defined here, instead of in a machine
 *	dependent directory, so that any program can get all definitions
 *	regardless of where is it compiled).
 */

/*
 *	VAX subtypes (these do *not* necessarily conform to the actual cpu
 *	ID assigned by DEC available via the SID register).
 */

#define CPU_SUBTYPE_VAX780	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_VAX785	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_VAX750	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_VAX730	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_UVAXI	((cpu_subtype_t) 5)
#define CPU_SUBTYPE_UVAXII	((cpu_subtype_t) 6)
#define CPU_SUBTYPE_VAX8200	((cpu_subtype_t) 7)
#define CPU_SUBTYPE_VAX8500	((cpu_subtype_t) 8)
#define CPU_SUBTYPE_VAX8600	((cpu_subtype_t) 9)
#define CPU_SUBTYPE_VAX8650	((cpu_subtype_t) 10)
#define CPU_SUBTYPE_VAX8800	((cpu_subtype_t) 11)
#define CPU_SUBTYPE_UVAXIII	((cpu_subtype_t) 12)

/*
 *	ROMP subtypes.
 */

#define CPU_SUBTYPE_RT_PC	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_RT_APC	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_RT_135	((cpu_subtype_t) 3)

/*
 *	68020 subtypes.
 */

#define CPU_SUBTYPE_SUN3_50	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_SUN3_160	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_SUN3_260	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_SUN3_110	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_SUN3_60	((cpu_subtype_t) 5)

#define CPU_SUBTYPE_HP_320	((cpu_subtype_t) 6)
	/* 16.67 Mhz HP 300 series, custom MMU [HP 320] */
#define CPU_SUBTYPE_HP_330	((cpu_subtype_t) 7)
	/* 16.67 Mhz HP 300 series, MC68851 MMU [HP 318,319,330,349] */
#define CPU_SUBTYPE_HP_350	((cpu_subtype_t) 8)
	/* 25.00 Mhz HP 300 series, custom MMU [HP 350] */

/*
 *	32032/32332/32532 subtypes.
 */

#define CPU_SUBTYPE_MMAX_DPC	    ((cpu_subtype_t) 1)	/* 032 CPU */
#define CPU_SUBTYPE_SQT		    ((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MMAX_APC_FPU    ((cpu_subtype_t) 3)	/* 32081 FPU */
#define CPU_SUBTYPE_MMAX_APC_FPA    ((cpu_subtype_t) 4)	/* Weitek FPA */
#define CPU_SUBTYPE_MMAX_XPC	    ((cpu_subtype_t) 5)	/* 532 CPU */
#define CPU_SUBTYPE_PC532           ((cpu_subtype_t) 6) /* pc532 board */

/*
 *	80386/80486 subtypes.
 */

#define CPU_SUBTYPE_AT386	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_EXL		((cpu_subtype_t) 2)
#define CPU_SUBTYPE_iPSC386	((cpu_subtype_t) 3)
#define	CPU_SUBTYPE_SYMMETRY	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_PS2         ((cpu_subtype_t) 5)    /* PS/2 w/ MCA */

/*
 *	Mips subtypes.
 */

#define CPU_SUBTYPE_MIPS_R2300	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_MIPS_R2600	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_MIPS_R2800	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_MIPS_R2000a	((cpu_subtype_t) 4)	/* pmax */
#define CPU_SUBTYPE_MIPS_R2000	((cpu_subtype_t) 5)
#define CPU_SUBTYPE_MIPS_R3000a	((cpu_subtype_t) 6)	/* 3max */
#define CPU_SUBTYPE_MIPS_R3000	((cpu_subtype_t) 7)

/*
 * 	MC68030 subtypes.
 */

#define CPU_SUBTYPE_NeXT	((cpu_subtype_t) 1) 
	/* NeXt thinks MC68030 is 6 rather than 9 */
#define CPU_SUBTYPE_HP_340	((cpu_subtype_t) 2) 
	/* 16.67 Mhz HP 300 series [HP 332,340] */
#define CPU_SUBTYPE_HP_360	((cpu_subtype_t) 3) 
	/* 25.00 Mhz HP 300 series [HP 360] */
#define CPU_SUBTYPE_HP_370	((cpu_subtype_t) 4) 
	/* 33.33 Mhz HP 300 series [HP 370] */

/*
 *	HPPA subtypes.
 */

#define CPU_SUBTYPE_HPPA_825	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_HPPA_835	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_HPPA_840	((cpu_subtype_t) 3)
#define CPU_SUBTYPE_HPPA_850	((cpu_subtype_t) 4)
#define CPU_SUBTYPE_HPPA_855	((cpu_subtype_t) 5)

/* 
 * 	ARM subtypes.
 */

#define CPU_SUBTYPE_ARM_A500_ARCH	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_ARM_A500		((cpu_subtype_t) 2)
#define CPU_SUBTYPE_ARM_A440		((cpu_subtype_t) 3)
#define CPU_SUBTYPE_ARM_M4		((cpu_subtype_t) 4)
#define CPU_SUBTYPE_ARM_A680		((cpu_subtype_t) 5)

/*
 *	MC88000 subtypes.
 */

#define CPU_SUBTYPE_MMAX_JPC		((cpu_subtype_t) 1)
#define CPU_SUBTYPE_LUNA88K             ((cpu_subtype_t) 2)

/*
 *	Sparc subtypes.
 */

#define CPU_SUBTYPE_SUN4_260		((cpu_subtype_t) 1)
#define CPU_SUBTYPE_SUN4_110		((cpu_subtype_t) 2)
#define CPU_SUBTYPE_SUN4_330		((cpu_subtype_t) 3)
#define CPU_SUBTYPE_SUN4C_60		((cpu_subtype_t) 4)
#define CPU_SUBTYPE_SUN4C_65		((cpu_subtype_t) 5)
#define CPU_SUBTYPE_SUN4C_20		((cpu_subtype_t) 6)
#define CPU_SUBTYPE_SUN4C_30		((cpu_subtype_t) 7)
#define CPU_SUBTYPE_SUN4C_40		((cpu_subtype_t) 8)
#define CPU_SUBTYPE_SUN4C_50		((cpu_subtype_t) 9)
#define CPU_SUBTYPE_SUN4C_75		((cpu_subtype_t) 10)

/*
 *	i860 subtypes.
 */

#define CPU_SUBTYPE_iPSC860		((cpu_subtype_t) 1)
#define CPU_SUBTYPE_OKI860		((cpu_subtype_t) 2)

/*
 *	Alpha subtypes.
 */

#define CPU_SUBTYPE_ALPHA_EV3		((cpu_subtype_t) 1)
#define CPU_SUBTYPE_ALPHA_EV4		((cpu_subtype_t) 2)
#define CPU_SUBTYPE_ALPHA_ISP		((cpu_subtype_t) 3)
#define CPU_SUBTYPE_ALPHA_21064		((cpu_subtype_t) 4)


#endif	/* _MACH_MACHINE_H_ */
