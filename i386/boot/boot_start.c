
#include <mach/machine/vm_types.h>
#include <mach/machine/far_ptr.h>
#include <mach/machine/proc_reg.h>

#include "vm_param.h"
#include "cpu.h"
#include "gdt.h"
#include "boot.h"
#include "phys_mem.h"

extern char do_boot[], do_boot_end[];

void boot_start(void)
{
	vm_size_t stub_size;
	void *stub;
	struct far_pointer_32 ptr;
	vm_offset_t copy_source;
	vm_size_t copy_size;

	boot_info_dump(boot_info);

	printf("Booting kernel...\n");

	cli();

	/* All of the data structures that are important to the kernel,
	   are in memory guaranteed not to conflict with the kernel image
	   or with anything else.
	   However, the kernel image itself is not in its final position,
	   because that might be right on top of us,
	   or on top of anything we allocated before reserving the kernel image region.
	   Therefore, we must allocate (non-conflicting) memory for a small stub
	   to copy the kernel image to its final position and invoke it.  */
	stub_size = do_boot_end - do_boot;
	stub = mustmalloc(stub_size);
	bcopy(do_boot, stub, stub_size);

	ptr.seg = LINEAR_CS;
	ptr.ofs = kvtophys(stub);

	/* The kernel image source and destination regions may overlap,
	   so we may have to copy either forwards or backwards.  */
	copy_source = kvtophys(boot_kern_image);
	copy_size = boot_kern_hdr.load_end_addr - boot_kern_hdr.load_addr;
	if (copy_source > boot_kern_hdr.load_addr)
	{
		asm volatile("
			cld
			ljmp	%0
		" :
		  : "mr" (ptr),		/* XXX r is inappropriate but gcc wants it */
		    "a" (boot_kern_hdr.entry),
		    "S" (copy_source),
		    "D" (boot_kern_hdr.load_addr),
		    "c" (copy_size),
		    "b" (kvtophys(boot_info)),
		    "d" (LINEAR_DS));
	}
	else
	{
		printf("(copying backwards...)\n"); /* XXX */
		asm volatile("
			std
			ljmp	%0
		" :
		  : "mr" (ptr),		/* XXX r is inappropriate but gcc wants it */
		    "a" (boot_kern_hdr.entry),
		    "S" (copy_source + copy_size - 1),
		    "D" (boot_kern_hdr.load_addr + copy_size - 1),
		    "c" (copy_size),
		    "b" (kvtophys(boot_info)),
		    "d" (LINEAR_DS));
	}
}
