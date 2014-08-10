/* 
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
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
 *	File: frc.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	3/92
 *
 *	Generic, mappable free running counter driver.
 */

#include <frc.h>
#if	NFRC > 0

#include <mach/std_types.h>
#include <chips/busses.h>
#include <device/device_types.h>

/*
 * Machine defines
 * All you need to do to get this working on a
 * random box is to define one macro and provide
 * the correct virtual address.
 */
#include	<platforms.h>
#ifdef	DECSTATION
#define	btop(x)		mips_btop(x)
#endif	/* DECSTATION */

/*
 * Autoconf info
 */

static vm_offset_t frc_std[NFRC] = { 0 };
static vm_size_t frc_offset[NFRC] = { 0 };
static struct bus_device *frc_info[NFRC];
static int frc_probe(vm_offset_t,struct bus_ctlr *);
static void frc_attach(struct bus_device *);

struct bus_driver frc_driver =
       { frc_probe, 0, frc_attach, 0, frc_std, "frc", frc_info, };

/*
 * Externally visible functions
 */
io_return_t	frc_openclose(int,int);			/* user */
vm_offset_t	frc_mmap(int,vm_offset_t,vm_prot_t);
void		frc_set_address(int,vm_size_t);

/*
 * FRC's in kernel virtual memory.  For in-kernel timestamps.
 */
vm_offset_t frc_address[NFRC];

/* machine-specific setups */
void
frc_set_address(
	int		unit,
	vm_size_t	offset)
{
	if (unit < NFRC) {
		frc_offset[unit] =  offset;
	}
}


/*
 * Probe chip to see if it is there
 */
static frc_probe (
	vm_offset_t	reg,
	struct bus_ctlr *ui)
{
	/* see if something present at the given address */
	if (check_memory(reg, 0)) {
		frc_address[ui->unit] = 0;
		return 0;
	}
	frc_std[ui->unit] = (vm_offset_t) reg;
	printf("[mappable] ");
	return 1;
}

static void
frc_attach (
		   struct bus_device *ui)
{
	if (ui->unit < NFRC) {
		frc_address[ui->unit] =
			(vm_offset_t) frc_std[ui->unit] + frc_offset[ui->unit];
		printf(": free running counter %d at kernel vaddr 0x%x",
		       ui->unit, frc_address[ui->unit]);
	}
	else
		panic("frc: unknown unit number"); /* shouldn't happen */
}

int frc_intr()
{
	/* we do not expect interrupts */
	panic("frc_intr");
}

io_return_t
frc_openclose(
	      int dev, 
	      int flag)
{
  if (frc_std[dev])
    return D_SUCCESS;
  else
    return D_NO_SUCH_DEVICE;
}

vm_offset_t
frc_mmap(
	int		dev,
	vm_offset_t	off,
	vm_prot_t	prot)
{
  	vm_offset_t addr;
	if ((prot & VM_PROT_WRITE) || (off >= PAGE_SIZE) )
		return (-1);
	addr = (vm_offset_t) frc_std[dev] + frc_offset[dev];
	return btop(pmap_extract(pmap_kernel(), addr));
}

#endif
