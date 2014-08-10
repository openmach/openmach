/* 
 * Copyright (c) 1995-1994 The University of Utah and
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

#include <mach/boolean.h>
#include <mach/vm_param.h>
#include <mach/machine/code16.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/paging.h>
#include <mach/machine/eflags.h>
#include <mach/machine/proc_reg.h>
#include <mach/machine/far_ptr.h>
#include <mach/machine/vcpi.h>
#include <mach/machine/asm.h>

#include "config.h"
#include "i16.h"
#include "i16_dos.h"
#include "cpu.h"
#include "real.h"
#include "debug.h"
#include "vm_param.h"

#ifdef ENABLE_VCPI

static boolean_t ems_page_allocated;
static unsigned short ems_handle;

static vm_offset_t vcpi_pdir, vcpi_ptable0;

struct far_pointer_32 vcpi_pmode_entry = {0, VCPI_CS};
struct vcpi_switch_data vcpi_switch_data;

static struct pseudo_descriptor gdt_pdesc, idt_pdesc;

static boolean_t pic_reprogrammed;

/* Save area for the DOS interrupt vectors
   that used to be in the place we relocated the master PIC to.  */
static struct far_pointer_16 master_save_vecs[8];


#ifdef ENABLE_PAGING
#define VCPI_PAGING_INIT(pdir_pa, first_unmapped_pa) vcpi_paging_init(pdir_pa, first_unmapped_pa)
#else
#define VCPI_PAGING_INIT(pdir_pa, first_unmapped_pa) ((void)0)
#endif

#ifdef ENABLE_KERNEL_LDT
#define KERNEL_LDT_INIT() (vcpi_switch_data.ldt_sel = KERNEL_LDT)
#else
#define KERNEL_LDT_INIT() ((void)0)
#endif


CODE16

static void i16_vcpi_switch_to_pmode()
{
	extern vm_offset_t boot_image_pa;

	i16_cli();

	i16_assert(i16_get_ds() == i16_get_cs());
	i16_assert(i16_get_es() == i16_get_cs());
	i16_assert(i16_get_ss() == i16_get_cs());

	/* Make sure the TSS isn't marked busy.  */
	cpu[0].tables.gdt[KERNEL_TSS_IDX].access &= ~ACC_TSS_BUSY;

	/* Ask the VCPI server to switch to protected mode.  */
	asm volatile("
		movl	%%esp,%%edx
		int	$0x67
	"SEXT(pmode_return)":
		movl	%%edx,%%esp
		movw	%2,%%dx
		movw	%%dx,%%ss
		movw	%%dx,%%ds
		movw	%%dx,%%es
		xorw	%%dx,%%dx
		movw	%%dx,%%fs
		movw	%%dx,%%gs
	" :
	  : "a" ((unsigned short)0xde0c),
	    "S" (boot_image_pa + (vm_offset_t)&vcpi_switch_data),
	    "i" (KERNEL_DS)
	  : "eax", "edx", "esi");

	/* Make sure the direction flag is still clear.  */
	i16_cld();
}

static void i16_vcpi_switch_to_real_mode()
{
	i16_cli();

	/* As requested by VCPI spec... */
	i16_clts();

	/* Perform the switch.  */
	asm volatile("
		movl	%%esp,%%edx
		pushl	%1
		pushl	%1
		pushl	%1
		pushl	%1
		pushl	%1
		pushl	%%edx
		pushl	$0
		pushl	%1
		pushl	$1f
		movw	%2,%%ds
		lcall	%%ss:"SEXT(vcpi_pmode_entry)"
	1:
	" :
	  : "a" ((unsigned short)0xde0c),
	    "r" ((unsigned)real_cs),
	    "r" ((unsigned short)LINEAR_DS)
	  : "eax", "edx");

	i16_assert(!(i16_get_eflags() & EFL_IF));
	i16_assert(i16_get_ds() == i16_get_cs());
	i16_assert(i16_get_es() == i16_get_cs());
	i16_assert(i16_get_ss() == i16_get_cs());

	/* Make sure the direction flag is still clear.  */
	i16_cld();
}

CODE32

static void vcpi_real_int(int intnum, struct real_call_data *rcd)
{
	do_16bit(
		unsigned int eflags;

		i16_vcpi_switch_to_real_mode();
		i16_real_int(intnum, rcd);
		i16_vcpi_switch_to_pmode();
	);
}

static void vcpi_exit(int rc)
{
	do_16bit(
		i16_vcpi_switch_to_real_mode();
		i16_exit(rc);
		while (1);
	);
}

CODE16

static inline void
i16_vcpi_set_int_vecs(unsigned short master, unsigned short slave)
{
	unsigned short rc;

	i16_assert(!(get_eflags() & EFL_IF));
	asm volatile("int $0x67"
		: "=a" (rc)
		: "a" ((unsigned short)0xde0b),
		  "b" ((unsigned short)master),
		  "c" ((unsigned short)slave));
	i16_assert((rc & 0xff00) == 0);
	i16_assert(!(get_eflags() & EFL_IF));
}

/* Find a (hopefully) empty set of interrupt vectors
   to use for the master hardware interrupts.
   We assume that eight interrupt vectors in a row
   that all have the same value are unused.
   If VCPI servers weren't so brain-damaged
   and took care of this during interrupt reflection
   (like we do when running in raw mode),
   this kludgery wouldn't be needed...  */
static int i16_find_free_vec_range()
{
	/* i will track the first vector in a range;
	   j will track the last.  */
	int i, j;
	struct far_pointer_16 iv, jv;

	j = 0xff;
	i16_dos_get_int_vec(j, &jv);

	for (i = j-1; ; i--)
	{
		if (i == 0x50)
		{
			/* No completely free sets found.
			   Stop here and just use 0x50-0x57.  */
			break;
		}

		i16_dos_get_int_vec(i, &iv);
		if ((iv.ofs != jv.ofs) || (iv.seg != jv.seg))
		{
			/* Vector contents changed.  */
			j = i;
			jv = iv;
			continue;
		}

		if ((j-i+1 >= 8) && ((i & 7) == 0))
		{
			/* Found a free range.  */
			break;
		}
	}

	return i;
}

void i16_vcpi_check()
{
	extern vm_offset_t dos_mem_phys_free_mem;
	extern vm_offset_t dos_mem_phys_free_size;
	extern void pmode_return();
	extern vm_offset_t boot_image_pa;
	extern void (*i16_switch_to_real_mode)();
	extern void (*i16_switch_to_pmode)();

	unsigned short rc;
	unsigned short first_free_pte;
	unsigned short vcpi_ver;

	i16_assert(boot_image_pa == kvtophys(0));

	/* Check for presence of EMM driver.  */
	{
		int dev_info, out_status;
		int fh;

		fh = i16_dos_open("EMMXXXX0", 0);
		if (fh < 0)
			return;
		dev_info = i16_dos_get_device_info(fh);
		out_status = i16_dos_get_output_status(fh);
		i16_dos_close(fh);
		if ((dev_info < 0) || !(dev_info & 0x80)
		    || (out_status != 0xff))
			return;
	}

	/* Allocate an EMS page to force the EMM to be turned on.
	   If it fails, keep going anyway -
	   it may simply mean all the EMS pages are allocated.  */
	asm volatile("int $0x67"
			: "=a" (rc),
			  "=d" (ems_handle)
			: "a" ((unsigned short)0x4300),
			  "b" ((unsigned short)1));
	if (!(rc & 0xff00))
		ems_page_allocated = TRUE;

	/* Check for VCPI.  */
	asm volatile("int $0x67" : "=a" (rc), "=b" (vcpi_ver) : "a" ((unsigned short)0xde00));
	if (rc & 0xff00)
		return;
	i16_assert(vcpi_ver >= 0x0100);

	/* OK, it's there - we're now committed to using VCPI.  */
	i16_switch_to_real_mode = i16_vcpi_switch_to_real_mode;
	i16_switch_to_pmode = i16_vcpi_switch_to_pmode;
	real_int = vcpi_real_int;
	real_exit = vcpi_exit;

	do_debug(i16_puts("VCPI detected"));

	/* Allocate a page directory and page table from low DOS memory.  */
	{
		vm_offset_t new_dos_mem;

		new_dos_mem = ((dos_mem_phys_free_mem + PAGE_MASK) & ~PAGE_MASK)
				+ PAGE_SIZE*2;
		if ((!dos_mem_phys_free_mem)
		    || (new_dos_mem - dos_mem_phys_free_mem
		        > dos_mem_phys_free_size))
			i16_die("not enough low DOS memory available");
		dos_mem_phys_free_size -= new_dos_mem - dos_mem_phys_free_mem;
		dos_mem_phys_free_mem = new_dos_mem;
		vcpi_pdir = new_dos_mem - PAGE_SIZE*2;
		vcpi_ptable0 = vcpi_pdir + PAGE_SIZE;
	}

	/* Initialize them.  */
	{
		int i;
		pt_entry_t pde0 = vcpi_ptable0
			| INTEL_PTE_VALID | INTEL_PTE_WRITE | INTEL_PTE_USER;

		set_fs(vcpi_pdir >> 4);
		asm volatile("movl %0,%%fs:(0)" : : "r" (pde0));
		for (i = 1; i < NPDES + NPTES; i++)
			asm volatile("movl $0,%%fs:(,%0,4)" : : "r" (i));
	}

	/* Initialize the protected-mode interface.  */
	asm volatile("
		pushw	%%es
		movw	%4,%%es
		int	$0x67
		popw	%%es
	"
		: "=a" (rc),
		  "=b" (vcpi_pmode_entry.ofs),
		  "=D" (first_free_pte)
		: "a" ((unsigned short)0xde01),
		  "r" ((unsigned short)(vcpi_ptable0 >> 4)),
		  "D" (0),
		  "S" (&cpu[0].tables.gdt[VCPI_CS_IDX]));
	i16_assert((rc & 0xff00) == 0);
	i16_assert(get_ds() == get_cs());
	i16_assert(get_es() == get_cs());

#ifdef DEBUG
	/* Sanity check: make sure the server did what it was supposed to do.  */

	i16_assert((cpu[0].tables.gdt[VCPI_CS_IDX].access & ACC_P|ACC_CODE) == ACC_P|ACC_CODE);
	if (cpu[0].tables.gdt[VCPI_CS_IDX].granularity & SZ_G)
		i16_assert(vcpi_pmode_entry.ofs <
			   (((vm_offset_t)cpu[0].tables.gdt[VCPI_CS_IDX].limit_high << 28)
			    | ((vm_offset_t)cpu[0].tables.gdt[VCPI_CS_IDX].limit_low << 12)
			    | (vm_offset_t)0xfff));
	else
		i16_assert(vcpi_pmode_entry.ofs <
			   (((vm_offset_t)cpu[0].tables.gdt[VCPI_CS_IDX].limit_high << 16)
			    | (vm_offset_t)cpu[0].tables.gdt[VCPI_CS_IDX].limit_low));

	i16_assert(first_free_pte/sizeof(pt_entry_t) >= 1*1024*1024/PAGE_SIZE);
	i16_assert(first_free_pte/sizeof(pt_entry_t) <= 4*1024*1024/PAGE_SIZE);

	{
		int i;

		for (i = 0; i < 1*1024*1024/PAGE_SIZE; i++)
		{
			pt_entry_t entry;

			set_ds(vcpi_ptable0 >> 4);
			entry = ((pt_entry_t*)0)[i];
			set_ds(get_cs());
			i16_assert(entry & INTEL_PTE_VALID);
			if (i < 0xf0000/PAGE_SIZE)
				i16_assert(entry & INTEL_PTE_WRITE);
			i16_assert(entry & INTEL_PTE_USER);
			i16_assert(!(entry & INTEL_PTE_AVAIL));
		}
	}
#endif /* DEBUG */

	/* Find the VCPI server's hardware interrupt vector mappings.  */
	asm volatile("int $0x67"
		: "=a" (rc),
		  "=b" (irq_master_base),
		  "=c" (irq_slave_base)
		: "a" ((unsigned short)0xde0a));
	i16_assert((rc & 0xff00) == 0);
	irq_master_base &= 0xffff;
	irq_slave_base &= 0xffff;
	i16_assert((irq_master_base & 7) == 0);
	i16_assert((irq_master_base == 0x08) || (irq_master_base >= 0x20));
	i16_assert((irq_slave_base & 7) == 0);
	i16_assert(irq_slave_base >= 0x20);

	/* If they're the usual DOS values, change them.  */
	if (irq_master_base == 0x08)
	{
		pic_reprogrammed = TRUE;

		i16_cli();

		irq_master_base = i16_find_free_vec_range();

		/* Save the old vectors in that range
		   and set them to a copy of vectors 8-15.  */
		{
			int i;

			for (i = 0; i < 8; i++)
			{
				struct far_pointer_16 hw_vec;

				i16_dos_get_int_vec(irq_master_base+i,
						    &master_save_vecs[i]);
				i16_dos_get_int_vec(0x08+i, &hw_vec);
				i16_dos_set_int_vec(irq_master_base+i, &hw_vec);
			}
		}

		/* Reprogram the PIC.  */
		i16_pic_set_master(irq_master_base);

		/* Inform the VCPI server.  */
		i16_vcpi_set_int_vecs(irq_master_base, irq_slave_base);
	}

	/* Initialize the switch-to-pmode data structure.  */
	vcpi_switch_data.phys_pdir = vcpi_pdir;
	vcpi_switch_data.lin_gdt = boot_image_pa+(vm_offset_t)&gdt_pdesc.limit;
	vcpi_switch_data.lin_idt = boot_image_pa+(vm_offset_t)&idt_pdesc.limit;
	vcpi_switch_data.tss_sel = KERNEL_TSS;
	vcpi_switch_data.entry_eip = (unsigned short)(vm_offset_t)&pmode_return;
	vcpi_switch_data.entry_cs = KERNEL_16_CS;

	/* Initialize the GDT and IDT pseudo-descriptors.  */
	gdt_pdesc.limit = sizeof(cpu[0].tables.gdt)-1;
	gdt_pdesc.linear_base = boot_image_pa + (vm_offset_t)&cpu[0].tables.gdt;
	idt_pdesc.limit = sizeof(cpu[0].tables.idt)-1;
	idt_pdesc.linear_base = boot_image_pa + (vm_offset_t)&cpu[0].tables.idt;

	/* Set the GDT to temporary settings
	   just for getting into pmode the first time.  */
	i16_gdt_init_temp();

	/* VCPI insists on loading a TSS immediately on entering pmode,
	   so initialize the KERNEL_TSS descriptor in the GDT.  */
	i16_fill_gdt_descriptor(&cpu[0], KERNEL_TSS,
				boot_image_pa + (vm_offset_t)&cpu[0].tables.tss,
				sizeof(cpu[0].tables.tss)-1,
				ACC_PL_K|ACC_TSS, 0);
	cpu[0].tables.tss.io_bit_map_offset = sizeof(cpu[0].tables.tss);

#if 0
	/* Dump the various VCPI data structures, for debugging.  */
	{
		int i;

		i16_puts("Switch data");
		i16_writehexl(switch_data.phys_pdir); i16_putchar(' ');
		i16_writehexl(switch_data.lin_gdt); i16_putchar(' ');
		i16_writehexl(switch_data.lin_idt); i16_putchar(' ');
		i16_writehexw(switch_data.ldt_sel); i16_putchar(' ');
		i16_writehexw(switch_data.tss_sel); i16_putchar(' ');
		i16_writehexl(switch_data.entry_eip); i16_putchar(' ');
		i16_writehexw(switch_data.entry_cs); i16_puts("");

		i16_puts("GDT pdesc");
		i16_writehexw(gdt_pdesc.limit); i16_putchar(' ');
		i16_writehexl(gdt_pdesc.linear_base); i16_puts("");

		i16_puts("IDT pdesc");
		i16_writehexw(idt_pdesc.limit); i16_putchar(' ');
		i16_writehexl(idt_pdesc.linear_base); i16_puts("");

		i16_puts("GDT");
		for (i = 0; i < GDTSZ; i++)
		{
			i16_writehexw(i*8); i16_putchar(' ');
			i16_writehexll(*((long long*)&cpu[0].tables.gdt[i]));
			i16_puts("");
		}
	}
#endif

	/* Switch into pmode briefly to initialize the CPU tables and such.  */
	i16_vcpi_switch_to_pmode();
	i16_do_32bit(

		/* Note that right now we can only access the first 1MB of memory,
		   because paging is enabled and that's the only memory region that's been mapped.
		   The rest of physical memory won't be mapped until VCPI_PAGING_INIT,
		   but VCPI_PAGING_INIT requires allocating memory for page tables,
		   and we can't call phys_mem_collect() to provide memory to the allocator
		   until all physical memory can be read and written.
		   To get out of this catch-22,
		   we call dos_mem_collect() beforehand here
		   to make low DOS memory available for allocation by VCPI_PAGING_INIT.
		   The call to phys_mem_collect() later will cause dos_mem_collect
		   to be called a second time, but it'll just do nothing then.  */
		dos_mem_collect();

		/* Initialize the basic CPU tables.  */
		cpu_init(&cpu[0]);

		/* Initialize the paging system.  */
		VCPI_PAGING_INIT(vcpi_pdir, (vm_offset_t)first_free_pte / 4 * PAGE_SIZE);

		/* Now that we can access all physical memory,
		   collect the remaining memory regions we discovered while in 16-bit mode
		   and add them to our free memory list.  */
		phys_mem_collect();

		/* Initialize the hardware interrupt vectors in the IDT.  */
		idt_irq_init();

		/* Now that we have an initialized LDT descriptor, start using it.  */
		KERNEL_LDT_INIT();

		/* Switch to real mode and back again once more,
		   to make sure everything's loaded properly.  */
		do_16bit(
			i16_vcpi_switch_to_real_mode();
			i16_vcpi_switch_to_pmode();
		);

		vcpi_start();
	);
}

/* Shouldn't be necessary, but just in case the end of the above function,
   containing the .code16, gets "optimized away"...  */
CODE16

void i16_vcpi_shutdown()
{
	if (pic_reprogrammed)
	{
		pic_reprogrammed = FALSE;

		i16_cli();

		i16_assert(irq_master_base >= 0x20);

		/* Reprogram the PIC.  */
		i16_pic_set_master(0x08);

		/* Inform the VCPI server.  */
		i16_vcpi_set_int_vecs(0x08, irq_slave_base);

		/* Restore the old interrupt vectors.  */
		{
			int i;

			for (i = 0; i < 8; i++)
			{
				i16_dos_set_int_vec(irq_master_base+i,
						    &master_save_vecs[i]);
			}
		}

		i16_sti();
	}

	if (ems_page_allocated)
	{
		ems_page_allocated = 0;
		asm volatile("int $0x67" : : "a" (0x4500), "d" (ems_handle));
	}
}

#endif ENABLE_VCPI

