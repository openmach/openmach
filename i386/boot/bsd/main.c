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

#include <mach/exec/exec.h>
#include <mach/exec/a.out.h>
#include <mach/machine/multiboot.h>
#include <mach/machine/proc_reg.h>
#include <sys/reboot.h>
#include <stdlib.h>

#include "vm_param.h"
#include "phys_mem.h"
#include "cpu.h"
#include "pic.h"
#include "real.h"
#include "debug.h"
#include "boottype.h"
#include "boot.h"


/* Keeps track of the highest physical memory address we know about.
   Initialized by phys_mem_add().  */
vm_offset_t phys_mem_max;
/* PC memory values (in K) from BIOS */
vm_offset_t phys_mem_lower;
vm_offset_t phys_mem_upper;

/* Always zero - virtual addresses are physical addresses.  */
vm_offset_t phys_mem_va;

int irq_master_base = PICM_VECTBASE;
int irq_slave_base = PICS_VECTBASE;

struct multiboot_info *boot_info;
static struct multiboot_module *boot_mods;

struct multiboot_header boot_kern_hdr;
void *boot_kern_image;
struct exec_info boot_kern_info;

extern int boottype;
extern unsigned long boothowto, bootdev;

extern struct lmod
{
	void *start;
	void *end;
	char *string;
} boot_modules[];

/* Find the extended memory available and add it to malloc's free list.  */
static void grab_ext_mem(void)
{
	extern char _end[];

	struct real_call_data rcd;
	vm_offset_t free_bot, free_top;

	/* Find the top of lower memory (up to 640K).  */
	rcd.flags = 0;
	real_int(0x12, &rcd);
	phys_mem_lower = (rcd.eax & 0xFFFF);

	/* Find the top of extended memory (up to 64MB).  */
	rcd.eax = 0x8800;
	rcd.flags = 0;
	real_int(0x15, &rcd);
	phys_mem_upper = (rcd.eax & 0xFFFF);
	free_top = 0x100000 + (phys_mem_upper * 1024);

	/* Add the available memory to the free list.  */
	free_bot = (vm_offset_t)kvtophys(_end);
	assert(free_bot > 0x100000); assert(free_bot < free_top);
	phys_mem_add(free_bot, free_top - free_bot);

	printf("using extended memory %08x-%08x\n", free_bot, free_top);
}

/* Unlike the Linux boot mechanism, the older Mach and BSD systems
   do not have any notion of a general command line,
   only a fixed, kludgy (and diverging) set of flags and values.
   Here we make a best-effort attempt
   to piece together a sane Linux-style command line
   from the boothowto and bootdev values provided by the boot loader.
   Naturally, the interpretation changes depending on who loaded us;
   that's what the boottype is for (see crt0.S).  */
static void init_cmdline(void)
{
	static char buf[30];
	char *major;

	/* First handle the option flags.
	   Just supply the flags as they would appear on the command line
	   of the Mach or BSD reboot command line,
	   and let the kernel handle or ignore them as appropriate.  */
	if (boothowto & RB_ASKNAME)
		strcat(buf, "-a ");
	if (boothowto & RB_SINGLE)
		strcat(buf, "-s ");
	if (boothowto & RB_DFLTROOT)
		strcat(buf, "-r ");
	if (boothowto & RB_HALT)
		strcat(buf, "-b ");
	if (boottype == BOOTTYPE_MACH)
	{
		/* These were determined from what the command-line parser does
		   in boot.c in the original Mach boot loader.  */
		if (boothowto & RB_KDB)
			strcat(buf, "-d ");
	}
	else
	{
		/* These were determined from what the command-line parser does
		   in boot.c in the FreeBSD 2.0 boot loader.  */
		if (boothowto & BSD_RB_KDB)
			strcat(buf, "-d ");
		if (boothowto & BSD_RB_CONFIG)
			strcat(buf, "-c ");
	}

	/* Now indicate the root device with a "root=" option.  */
	major = 0;
	if (boottype == BOOTTYPE_MACH)
	{
		static char *devs[] = {"hd", "fd", "wt", "sd", "ha"};
		if (B_TYPE(bootdev) < sizeof(devs)/sizeof(devs[0]))
			major = devs[B_TYPE(bootdev)];
	}
	else
	{
		static char *devs[] = {"wd", "hd", "fd", "wt", "sd"};
		if (B_TYPE(bootdev) < sizeof(devs)/sizeof(devs[0]))
			major = devs[B_TYPE(bootdev)];
	}
	if (major)
	{
		sprintf(buf + strlen(buf), "root=%s%d%c",
			major, B_UNIT(bootdev), 'a' + B_PARTITION(bootdev));
	}

	/* Insert the command line into the boot_info structure.  */
	boot_info->cmdline = (vm_offset_t)kvtophys(buf);
	boot_info->flags |= MULTIBOOT_CMDLINE;
}

static void init_symtab(struct multiboot_header *h)
{
	if (h->flags & MULTIBOOT_AOUT_KLUDGE)
	{
		struct exec *hdr;

		/* This file is probably (but not necessarily) in a.out format.
		   Check for an a.out symbol/string table.
		   If we can't find one, or if the a.out header appears bogus,
		   then continue but just don't supply a symbol table.  */
		hdr = (struct exec*)boot_modules[0].start;
		if (!N_BADMAG(*hdr) && (hdr->a_syms > 0))
		{
			void *in_symtab, *in_strtab, *symtab;

			/* Calculate the address of the symbol table.
			   Don't try to use the information in the header
			   to find the beginning of the text segment;
			   its interpretation varies widely.
			   Instead, use the boot_kern_image pointer
			   we calculated from tho MULTIBOOT_AOUT_KLUDGE data,
			   which should consistently tell us
			   where the beginning of the text segment is.  */
			in_symtab = boot_kern_image
				    + hdr->a_text
				    + hdr->a_data
				    + hdr->a_trsize
				    + hdr->a_drsize;
			in_strtab = in_symtab
				    + hdr->a_syms;

			/* Note that tabsize doesn't include the size word,
			   but strsize does.  */
			boot_info->syms.a.tabsize = hdr->a_syms;
			boot_info->syms.a.strsize = *(vm_size_t *)(in_strtab);

			printf("bsdboot: Symbols found, tab = %#x str = %#x.\n",
			       boot_info->syms.a.tabsize,
			       boot_info->syms.a.strsize);

			/* Allocate a new buffer for the symbol table
			   in non-conflicting memory.  */
			symtab = mustmalloc(sizeof(vm_size_t)
					    + boot_info->syms.a.tabsize
					    + boot_info->syms.a.strsize);
			*((vm_size_t *)symtab) = boot_info->syms.a.tabsize;
			memcpy(symtab + sizeof(vm_size_t), in_symtab,
			       boot_info->syms.a.tabsize
			       + boot_info->syms.a.strsize);

			/* Register the symbol table in the boot_info.  */
			boot_info->flags |= MULTIBOOT_AOUT_SYMS;
			boot_info->syms.a.addr = (vm_offset_t)kvtophys(symtab);
		}
	}
	else
	{
		/* XXX get symbol/string table from exec_load().  */
	}
}

static
int kimg_read(void *handle, vm_offset_t file_ofs, void *buf, vm_size_t size, vm_size_t *out_actual)
{
	/* XXX limit length */
	memcpy(buf, boot_modules[0].start + file_ofs, size);
	*out_actual = size;
	return 0;
}

static
int kimg_read_exec_1(void *handle, vm_offset_t file_ofs, vm_size_t file_size,
		     vm_offset_t mem_addr, vm_size_t mem_size,
		     exec_sectype_t section_type)
{
	if (!(section_type & EXEC_SECTYPE_ALLOC))
		return 0;

	assert(mem_size > 0);
	if (mem_addr < boot_kern_hdr.load_addr)
		boot_kern_hdr.load_addr = mem_addr;
	if (mem_addr+file_size > boot_kern_hdr.load_end_addr)
		boot_kern_hdr.load_end_addr = mem_addr+file_size;
	if (mem_addr+mem_size > boot_kern_hdr.bss_end_addr)
		boot_kern_hdr.bss_end_addr = mem_addr+mem_size;

	return 0;
}

static
int kimg_read_exec_2(void *handle, vm_offset_t file_ofs, vm_size_t file_size,
		     vm_offset_t mem_addr, vm_size_t mem_size,
		     exec_sectype_t section_type)
{
	if (!(section_type & EXEC_SECTYPE_ALLOC))
		return 0;

	assert(mem_size > 0);
	assert(mem_addr >= boot_kern_hdr.load_addr);
	assert(mem_addr+file_size <= boot_kern_hdr.load_end_addr);
	assert(mem_addr+mem_size <= boot_kern_hdr.bss_end_addr);

	memcpy(boot_kern_image + mem_addr - boot_kern_hdr.load_addr,
		boot_modules[0].start + file_ofs, file_size);

	return 0;
}

void raw_start(void)
{
	struct multiboot_header *h;
	int i, err;


	cpu_tables_init(&cpu[0]);
	cpu_tables_load(&cpu[0]);
	pic_init(PICM_VECTBASE, PICS_VECTBASE);
	idt_irq_init();

	/* Get some memory to work in.  */
	grab_ext_mem();

	if (boot_modules[0].start == 0)
		die("This boot image contains no boot modules!?!?");

	/* Scan for the multiboot_header.  */
	for (i = 0; ; i += 4)
	{
		if (i >= MULTIBOOT_SEARCH)
			die("kernel image has no multiboot_header");
		h = (struct multiboot_header*)(boot_modules[0].start+i);
		if (h->magic == MULTIBOOT_MAGIC
		    && !(h->magic + h->flags + h->checksum))
			break;
	}
	if (h->flags & MULTIBOOT_MUSTKNOW & ~MULTIBOOT_MEMORY_INFO)
		die("unknown multiboot_header flag bits %08x",
			h->flags & MULTIBOOT_MUSTKNOW & ~MULTIBOOT_MEMORY_INFO);
	boot_kern_hdr = *h;

	if (h->flags & MULTIBOOT_AOUT_KLUDGE)
	{
		boot_kern_image = (void*)h + h->load_addr - h->header_addr;
	}
	else
	{
		/* No a.out-kludge information available;
		   attempt to interpret the exec header instead,
		   using the simple interpreter in libmach_exec.a.  */

		/* Perform the "load" in two passes.
		   In the first pass, find the number of sections the load image contains
		   and reserve the physical memory containing each section.
		   Also, initialize the boot_kern_hdr to reflect the extent of the image.
		   In the second pass, load the sections into a temporary area
		   that can be copied to the final location all at once by do_boot.S.  */

		boot_kern_hdr.load_addr = 0xffffffff;
		boot_kern_hdr.load_end_addr = 0;
		boot_kern_hdr.bss_end_addr = 0;

		if (err = exec_load(kimg_read, kimg_read_exec_1, 0,
				    &boot_kern_info))
			panic("cannot load kernel image: error code %d", err);
		boot_kern_hdr.entry = boot_kern_info.entry;

		/* It's OK to malloc this before reserving the memory the kernel will occupy,
		   because do_boot.S can deal with overlapping source and destination.  */
		assert(boot_kern_hdr.load_addr < boot_kern_hdr.load_end_addr);
		assert(boot_kern_hdr.load_end_addr < boot_kern_hdr.bss_end_addr);
		boot_kern_image = malloc(boot_kern_hdr.load_end_addr - boot_kern_hdr.load_addr);

		if (err = exec_load(kimg_read, kimg_read_exec_2, 0,
				    &boot_kern_info))
			panic("cannot load kernel image: error code %d", err);
		assert(boot_kern_hdr.entry == boot_kern_info.entry);
	}

	/* Reserve the memory that the kernel will eventually occupy.
	   All malloc calls after this are guaranteed to stay out of this region.  */
	malloc_reserve(phystokv(boot_kern_hdr.load_addr), phystokv(boot_kern_hdr.bss_end_addr));

	printf("kernel at %08x-%08x text+data %d bss %d\n",
		boot_kern_hdr.load_addr, boot_kern_hdr.bss_end_addr,
		boot_kern_hdr.load_end_addr - boot_kern_hdr.load_addr,
		boot_kern_hdr.bss_end_addr - boot_kern_hdr.load_end_addr);
	assert(boot_kern_hdr.load_addr < boot_kern_hdr.load_end_addr);
	assert(boot_kern_hdr.load_end_addr < boot_kern_hdr.bss_end_addr);
	if (boot_kern_hdr.load_addr < 0x1000)
		panic("kernel wants to be loaded too low!");
#if 0
	if (boot_kern_hdr.bss_end_addr > phys_mem_max)
		panic("kernel wants to be loaded beyond available physical memory!");
#endif
	if ((boot_kern_hdr.load_addr < 0x100000)
	    && (boot_kern_hdr.bss_end_addr > 0xa0000))
		panic("kernel wants to be loaded on top of I/O space!");

	boot_info = (struct multiboot_info*)mustcalloc(sizeof(*boot_info));

	/* Build a command line to pass to the kernel.  */
	init_cmdline();

	/* Add memory information */
	boot_info->flags |= MULTIBOOT_MEMORY;
	boot_info->mem_upper = phys_mem_upper;
	boot_info->mem_lower = phys_mem_lower;

	/* Indicate to the kernel which BIOS disk device we booted from.
	   The Mach and BSD boot loaders obscure this information somewhat;
	   we have to extract it from the mangled bootdev value.
	   We assume that any unit other than floppy means BIOS hard drive.
	   XXX If we boot from FreeBSD's netboot, we shouldn't set this.  */
	boot_info->flags |= MULTIBOOT_BOOT_DEVICE;
	if (boottype == BOOTTYPE_MACH)
		boot_info->boot_device[0] = B_TYPE(bootdev) == 1 ? 0 : 0x80;
	else
		boot_info->boot_device[0] = B_TYPE(bootdev) == 2 ? 0 : 0x80;
	boot_info->boot_device[0] += B_UNIT(bootdev);
	boot_info->boot_device[1] = 0xff;
	boot_info->boot_device[2] = B_PARTITION(bootdev);
	boot_info->boot_device[3] = 0xff;

	/* Find the symbol table to supply to the kernel, if possible.  */
	init_symtab(h);

	/* Initialize the boot module entries in the boot_info.  */
	boot_info->flags |= MULTIBOOT_MODS;
	for (i = 1; boot_modules[i].start; i++);
	boot_info->mods_count = i-1;
	boot_mods = (struct multiboot_module*)mustcalloc(
		boot_info->mods_count * sizeof(*boot_mods));
	boot_info->mods_addr = kvtophys(boot_mods);
	for (i = 0; i < boot_info->mods_count; i++)
	{
		struct lmod *lm = &boot_modules[1+i];
		struct multiboot_module *bm = &boot_mods[i];

		assert(lm->end > lm->start);

		/* Try to leave the boot module where it is and pass its address.  */
		bm->mod_start = kvtophys(lm->start);
		bm->mod_end = kvtophys(lm->end);

		/* However, if the current location of the boot module
		   overlaps with the final location of the kernel image,
		   we have to move the boot module somewhere else.  */
		if ((bm->mod_start < boot_kern_hdr.load_end_addr)
		    && (bm->mod_end > boot_kern_hdr.load_addr))
		{
			vm_size_t size = lm->end - lm->start;
			void *newaddr = mustmalloc(size);

			printf("moving boot module %d from %08x to %08x\n",
				i, kvtophys(lm->start), kvtophys(newaddr));
			memcpy(newaddr, lm->start, size);

			bm->mod_start = kvtophys(newaddr);
			bm->mod_end = bm->mod_start + size;
		}

		/* Also provide the string associated with the module.  */
		printf("lm->string '%s'\n", lm->string);
		{
			char *newstring = mustmalloc(strlen(lm->string)+1);
			strcpy(newstring, lm->string);
			bm->string = kvtophys(newstring);
		}

		bm->reserved = 0;
	}

	boot_start();
}

