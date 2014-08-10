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
 *	File:	sbrk.c
 *	Author: Avadis Tevanian, Carnegie Mellon University
 *	Date:	June 1986
 *
 *	Unix compatibility for sbrk system call.
 */

#define  EXPORT_BOOLEAN
#include <mach.h>		/* for vm_allocate, vm_offset_t */
#include <mach_init.h>		/* for vm_page_size */

/* OBSOLETE, do not keep on reproducing this silliness pls */

#if	(defined(vax) || defined(sun3) || defined(i386))

asm(".data");
asm(".globl	curbrk");
asm(".globl	minbrk");
asm(".globl	_curbrk");
asm(".globl	_minbrk");
asm(".globl	_end");
asm("_minbrk:");
asm("minbrk:	.long	_end");
asm("_curbrk:");
asm("curbrk:	.long	_end");
asm(".text");

#else	/* none of the above */

/* This is the proper way, no ugly asm() hacks to define cubrk, minbrk. */

/* .. but these machines are silly in a different way */
#if	defined(mips) || defined(alpha)
#define curbrk _curbrk
#define minbrk _minbrk
#endif

/* This is just a symbol, *not* necessarily an array */
extern char end;
vm_offset_t curbrk = (vm_offset_t) &end;
vm_offset_t minbrk = (vm_offset_t) &end;

#endif	/* the proper way */

/* if asm() was used */
extern vm_offset_t curbrk;
extern vm_offset_t minbrk;

#define	roundup(a,b)	((((a) + (b) - 1) / (b)) * (b))

vm_offset_t sbrk(size)
	int	size;
{
	vm_offset_t	addr;
	kern_return_t	ret;
	vm_offset_t	ocurbrk;

	if (size <= 0)
		return(curbrk);	/* Compatible with Unix bugs. Sigh. */

	addr = (vm_offset_t) roundup(curbrk,vm_page_size);
	ocurbrk = curbrk;
	if ((curbrk+size) > addr)
	{	ret = vm_allocate(mach_task_self(), &addr, 
			    (vm_size_t) size -(addr-curbrk), FALSE);
		if (ret == KERN_NO_SPACE) {
			ret = vm_allocate(mach_task_self(), &addr, (vm_size_t) size, TRUE);
			ocurbrk = addr;
		}
		if (ret != KERN_SUCCESS) 
			return((vm_offset_t) -1);
	}

	curbrk = ocurbrk + size;
	return(ocurbrk);

}

vm_offset_t _brk(addr)
	vm_offset_t addr;
{
	vm_offset_t rnd_addr, rnd_brk;
	kern_return_t	ret;

	rnd_addr = roundup(addr, vm_page_size);
	rnd_brk = roundup(curbrk, vm_page_size);

	if (rnd_addr < rnd_brk)
		ret = vm_deallocate(mach_task_self(), rnd_addr, rnd_brk - rnd_addr);
	else if (rnd_addr > rnd_brk)
		ret = vm_allocate(mach_task_self(), &rnd_brk, rnd_addr - rnd_brk, FALSE);
	if (ret != KERN_SUCCESS) {
#if true_unix
		errno = ENOMEM;
#endif
		return (vm_offset_t) -1L;
	}
	curbrk = addr;
	return (vm_offset_t) 0L;
}


vm_offset_t brk(addr)
	vm_offset_t addr;
{
	if (addr < minbrk)
		addr = minbrk;

	return _brk(addr);
}

