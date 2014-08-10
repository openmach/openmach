/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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
 * Interface to new debugger.
 */

#include "mach_kdb.h"
#if MACH_KDB

#include <cpus.h>

#include <sys/reboot.h>
#include <vm/pmap.h>

#include <i386/thread.h>
#include <i386/db_machdep.h>
#include <i386/seg.h>
#include <i386/trap.h>
#include <i386/setjmp.h>
#include <i386/pmap.h>
#include "gdt.h"
#include "trap.h"

#include "vm_param.h"
#include <vm/vm_map.h>
#include <kern/cpu_number.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <ddb/db_task_thread.h>
#include <machine/machspl.h>

struct	 i386_saved_state *i386_last_saved_statep;
struct	 i386_saved_state i386_nested_saved_state;
unsigned i386_last_kdb_sp;

extern	thread_t db_default_thread;

/*
 * Print trap reason.
 */
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: %s (%d), code=%x\n",
		trap_name(type), type, code);
}

/*
 *  kdb_trap - field a TRACE or BPT trap
 */

extern jmp_buf_t *db_recover;
spl_t saved_ipl[NCPUS];	/* just to know what was IPL before trap */

boolean_t
kdb_trap(
	int	type,
	int	code,
	register struct i386_saved_state *regs)
{
	spl_t	s;

	s = splhigh();
	saved_ipl[cpu_number()] = s;

	switch (type) {
	    case T_DEBUG:	/* single_step */
	    {
	    	extern int dr_addr[];
		int addr;
	    	int status = dr6();

		if (status & 0xf) {	/* hmm hdw break */
			addr =	status & 0x8 ? dr_addr[3] :
				status & 0x4 ? dr_addr[2] :
				status & 0x2 ? dr_addr[1] :
					       dr_addr[0];
			regs->efl |= EFL_RF;
			db_single_step_cmd(addr, 0, 1, "p");
		}
	    }
	    case T_INT3:	/* breakpoint */
	    case T_WATCHPOINT:	/* watchpoint */
	    case -1:	/* keyboard interrupt */
		break;

	    default:
		if (db_recover) {
		    i386_nested_saved_state = *regs;
		    db_printf("Caught %s (%d), code = %x, pc = %x\n",
			trap_name(type), type, code, regs->eip);
		    db_error("");
		    /*NOTREACHED*/
		}
		kdbprinttrap(type, code);
	}

#if	NCPUS > 1
	if (db_enter())
#endif	/* NCPUS > 1 */
	{
	    i386_last_saved_statep = regs;
	    i386_last_kdb_sp = (unsigned) &type;

	    /* XXX Should switch to ddb`s own stack here. */

	    ddb_regs = *regs;
	    if ((regs->cs & 0x3) == 0) {
		/*
		 * Kernel mode - esp and ss not saved
		 */
		ddb_regs.uesp = (int)&regs->uesp;   /* kernel stack pointer */
		ddb_regs.ss   = KERNEL_DS;
	    }

	    cnpollc(TRUE);
	    db_task_trap(type, code, (regs->cs & 0x3) != 0);
	    cnpollc(FALSE);

	    regs->eip    = ddb_regs.eip;
	    regs->efl    = ddb_regs.efl;
	    regs->eax    = ddb_regs.eax;
	    regs->ecx    = ddb_regs.ecx;
	    regs->edx    = ddb_regs.edx;
	    regs->ebx    = ddb_regs.ebx;
	    if (regs->cs & 0x3) {
		/*
		 * user mode - saved esp and ss valid
		 */
		regs->uesp = ddb_regs.uesp;		/* user stack pointer */
		regs->ss   = ddb_regs.ss & 0xffff;	/* user stack segment */
	    }
	    regs->ebp    = ddb_regs.ebp;
	    regs->esi    = ddb_regs.esi;
	    regs->edi    = ddb_regs.edi;
	    regs->es     = ddb_regs.es & 0xffff;
	    regs->cs     = ddb_regs.cs & 0xffff;
	    regs->ds     = ddb_regs.ds & 0xffff;
	    regs->fs     = ddb_regs.fs & 0xffff;
	    regs->gs     = ddb_regs.gs & 0xffff;

	    if ((type == T_INT3) &&
		(db_get_task_value(regs->eip, BKPT_SIZE, FALSE, TASK_NULL)
								 == BKPT_INST))
		regs->eip += BKPT_SIZE;
	}
#if	NCPUS > 1
	db_leave();
#endif	/* NCPUS > 1 */

	splx(s);
	return 1;
}

/*
 *	Enter KDB through a keyboard trap.
 *	We show the registers as of the keyboard interrupt
 *	instead of those at its call to KDB.
 */
struct int_regs {
	int	gs;
	int	fs;
	int	edi;
	int	esi;
	int	ebp;
	int	ebx;
	struct i386_interrupt_state *is;
};

void
kdb_kentry(
	struct int_regs	*int_regs)
{
	struct i386_interrupt_state *is = int_regs->is;
	spl_t	s = splhigh();

#if	NCPUS > 1
	if (db_enter())
#endif	/* NCPUS > 1 */
	{
	    if (is->cs & 0x3) {
		ddb_regs.uesp = ((int *)(is+1))[0];
		ddb_regs.ss   = ((int *)(is+1))[1];
	    }
	    else {
		ddb_regs.ss  = KERNEL_DS;
		ddb_regs.uesp= (int)(is+1);
	    }
	    ddb_regs.efl = is->efl;
	    ddb_regs.cs  = is->cs;
	    ddb_regs.eip = is->eip;
	    ddb_regs.eax = is->eax;
	    ddb_regs.ecx = is->ecx;
	    ddb_regs.edx = is->edx;
	    ddb_regs.ebx = int_regs->ebx;
	    ddb_regs.ebp = int_regs->ebp;
	    ddb_regs.esi = int_regs->esi;
	    ddb_regs.edi = int_regs->edi;
	    ddb_regs.ds  = is->ds;
	    ddb_regs.es  = is->es;
	    ddb_regs.fs  = int_regs->fs;
	    ddb_regs.gs  = int_regs->gs;

	    cnpollc(TRUE);
	    db_task_trap(-1, 0, (ddb_regs.cs & 0x3) != 0);
	    cnpollc(FALSE);

	    if (ddb_regs.cs & 0x3) {
		((int *)(is+1))[0] = ddb_regs.uesp;
		((int *)(is+1))[1] = ddb_regs.ss & 0xffff;
	    }
	    is->efl = ddb_regs.efl;
	    is->cs  = ddb_regs.cs & 0xffff;
	    is->eip = ddb_regs.eip;
	    is->eax = ddb_regs.eax;
	    is->ecx = ddb_regs.ecx;
	    is->edx = ddb_regs.edx;
	    int_regs->ebx = ddb_regs.ebx;
	    int_regs->ebp = ddb_regs.ebp;
	    int_regs->esi = ddb_regs.esi;
	    int_regs->edi = ddb_regs.edi;
	    is->ds  = ddb_regs.ds & 0xffff;
	    is->es  = ddb_regs.es & 0xffff;
	    int_regs->fs = ddb_regs.fs & 0xffff;
	    int_regs->gs = ddb_regs.gs & 0xffff;
	}
#if	NCPUS > 1
	db_leave();
#endif	/* NCPUS > 1 */

	(void) splx(s);
}

boolean_t db_no_vm_fault = TRUE;
  
int
db_user_to_kernel_address(
	task_t		task,
	vm_offset_t	addr,
	unsigned	*kaddr,
	int		flag)
{
	register pt_entry_t *ptp;
	boolean_t	faulted = FALSE;
	
	retry:
	ptp = pmap_pte(task->map->pmap, addr);
	if (ptp == PT_ENTRY_NULL || (*ptp & INTEL_PTE_VALID) == 0) {
	    if (!faulted && !db_no_vm_fault) {
		kern_return_t	err;

		faulted = TRUE;
		err = vm_fault( task->map,
				trunc_page(addr),
				VM_PROT_READ,
				FALSE, FALSE, 0);
		if (err == KERN_SUCCESS)
		    goto retry;
	    }
	    if (flag) {
		db_printf("\nno memory is assigned to address %08x\n", addr);
		db_error(0);
		/* NOTREACHED */
	    }
	    return(-1);
	}
	*kaddr = (unsigned)ptetokv(*ptp) + (addr & (INTEL_PGBYTES-1));
	return(0);
}
	
/*
 * Read bytes from kernel address space for debugger.
 */

void
db_read_bytes(
	vm_offset_t	addr,
	register int	size,
	register char	*data,
	task_t		task)
{
	register char	*src;
	register int	n;
	unsigned	kern_addr;

	src = (char *)addr;
	if (addr >= VM_MIN_KERNEL_ADDRESS || task == TASK_NULL) {
	    if (task == TASK_NULL)
	        task = db_current_task();
	    while (--size >= 0) {
		if (addr++ < VM_MIN_KERNEL_ADDRESS && task == TASK_NULL) {
		    db_printf("\nbad address %x\n", addr);
		    db_error(0);
		    /* NOTREACHED */
		}
		*data++ = *src++;
	    }
	    return;
	}
	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		return;
	    src = (char *)kern_addr;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	    while (--n >= 0)
		*data++ = *src++;
	}
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(
	vm_offset_t	addr,
	register int	size,
	register char	*data,
	task_t		task)
{
	register char	*dst;

	register pt_entry_t *ptep0 = 0;
	pt_entry_t	oldmap0 = 0;
	vm_offset_t	addr1;
	register pt_entry_t *ptep1 = 0;
	pt_entry_t	oldmap1 = 0;
	extern char	etext;
	void		db_write_bytes_user_space();

	if ((addr < VM_MIN_KERNEL_ADDRESS) ^ 
	    ((addr + size) <= VM_MIN_KERNEL_ADDRESS)) {
	    db_error("\ncannot write data into mixed space\n");
	    /* NOTREACHED */
	}
	if (addr < VM_MIN_KERNEL_ADDRESS) {
	    if (task) {
		db_write_bytes_user_space(addr, size, data, task);
		return;
	    } else if (db_current_task() == TASK_NULL) {
		db_printf("\nbad address %x\n", addr);
		db_error(0);
		/* NOTREACHED */
	    }
	}
	    
	if (addr >= VM_MIN_KERNEL_ADDRESS &&
	    addr <= (vm_offset_t)&etext)
	{
	    ptep0 = pmap_pte(kernel_pmap, addr);
	    oldmap0 = *ptep0;
	    *ptep0 |= INTEL_PTE_WRITE;

	    addr1 = i386_trunc_page(addr + size - 1);
	    if (i386_trunc_page(addr) != addr1) {
		/* data crosses a page boundary */

		ptep1 = pmap_pte(kernel_pmap, addr1);
		oldmap1 = *ptep1;
		*ptep1 |= INTEL_PTE_WRITE;
	    }
	    flush_tlb();
	}

	dst = (char *)addr;

	while (--size >= 0)
	    *dst++ = *data++;

	if (ptep0) {
	    *ptep0 = oldmap0;
	    if (ptep1) {
		*ptep1 = oldmap1;
	    }
	    flush_tlb();
	}
}
	
void
db_write_bytes_user_space(
	vm_offset_t	addr,
	register int	size,
	register char	*data,
	task_t		task)
{
	register char	*dst;
	register int	n;
	unsigned	kern_addr;

	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 1) < 0)
		return;
	    dst = (char *)kern_addr;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	    while (--n >= 0)
		*dst++ = *data++;
	}
}

boolean_t
db_check_access(
	vm_offset_t	addr,
	register int	size,
	task_t		task)
{
	register	n;
	vm_offset_t	kern_addr;

	if (addr >= VM_MIN_KERNEL_ADDRESS) {
	    if (kernel_task == TASK_NULL)
	        return TRUE;
	    task = kernel_task;
	} else if (task == TASK_NULL) {
	    if (current_thread() == THREAD_NULL)
		return FALSE;
	    task = current_thread()->task;
	}
	while (size > 0) {
	    if (db_user_to_kernel_address(task, addr, &kern_addr, 0) < 0)
		return FALSE;
	    n = intel_trunc_page(addr+INTEL_PGBYTES) - addr;
	    if (n > size)
		n = size;
	    size -= n;
	    addr += n;
	}
	return TRUE;
}

boolean_t
db_phys_eq(
	task_t		task1,
	vm_offset_t	addr1,
	task_t		task2,
	vm_offset_t	addr2)
{
	vm_offset_t	kern_addr1, kern_addr2;

	if (addr1 >= VM_MIN_KERNEL_ADDRESS || addr2 >= VM_MIN_KERNEL_ADDRESS)
	    return FALSE;
	if ((addr1 & (INTEL_PGBYTES-1)) != (addr2 & (INTEL_PGBYTES-1)))
	    return FALSE;
	if (task1 == TASK_NULL) {
	    if (current_thread() == THREAD_NULL)
		return FALSE;
	    task1 = current_thread()->task;
	}
	if (db_user_to_kernel_address(task1, addr1, &kern_addr1, 0) < 0
		|| db_user_to_kernel_address(task2, addr2, &kern_addr2, 0) < 0)
	    return FALSE;
	return(kern_addr1 == kern_addr2);
}

#define DB_USER_STACK_ADDR		(VM_MIN_KERNEL_ADDRESS)
#define DB_NAME_SEARCH_LIMIT		(DB_USER_STACK_ADDR-(INTEL_PGBYTES*3))

static boolean_t
db_search_null(
	task_t		task,
	vm_offset_t	*svaddr,
	vm_offset_t	evaddr,
	vm_offset_t	*skaddr,
	int		flag)
{
	register unsigned vaddr;
	register unsigned *kaddr;

	kaddr = (unsigned *)*skaddr;
	for (vaddr = *svaddr; vaddr > evaddr; vaddr -= sizeof(unsigned)) {
	    if (vaddr % INTEL_PGBYTES == 0) {
		vaddr -= sizeof(unsigned);
		if (db_user_to_kernel_address(task, vaddr, skaddr, 0) < 0)
		    return FALSE;
		kaddr = (vm_offset_t *)*skaddr;
	    } else {
		vaddr -= sizeof(unsigned);
		kaddr--;
	    }
	    if ((*kaddr == 0) ^ (flag  == 0)) {
		*svaddr = vaddr;
		*skaddr = (unsigned)kaddr;
		return TRUE;
	    }
	}
	return FALSE;
}

void
db_task_name(
	task_t		task)
{
	register char *p;
	register n;
	unsigned vaddr, kaddr;

	vaddr = DB_USER_STACK_ADDR;
	kaddr = 0;

	/*
	 * skip nulls at the end
	 */
	if (!db_search_null(task, &vaddr, DB_NAME_SEARCH_LIMIT, &kaddr, 0)) {
	    db_printf(DB_NULL_TASK_NAME);
	    return;
	}
	/*
	 * search start of args
	 */
	if (!db_search_null(task, &vaddr, DB_NAME_SEARCH_LIMIT, &kaddr, 1)) {
	    db_printf(DB_NULL_TASK_NAME);
	    return;
	}

	n = DB_TASK_NAME_LEN-1;
	p = (char *)kaddr + sizeof(unsigned);
	for (vaddr += sizeof(int); vaddr < DB_USER_STACK_ADDR && n > 0; 
							vaddr++, p++, n--) {
	    if (vaddr % INTEL_PGBYTES == 0) {
		(void)db_user_to_kernel_address(task, vaddr, &kaddr, 0);
		p = (char*)kaddr;
	    }
	    db_printf("%c", (*p < ' ' || *p > '~')? ' ': *p);
	}
	while (n-- >= 0)	/* compare with >= 0 for one more space */
	    db_printf(" ");
}

#endif MACH_KDB
