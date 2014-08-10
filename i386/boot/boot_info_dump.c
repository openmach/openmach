
#include "vm_param.h"
#include "boot.h"
#include "debug.h"

void boot_info_dump()
{
	struct multiboot_info *bi = boot_info;
	struct multiboot_module *m;
	int i;

	printf("MultiBoot info flags: %08x\n", bi->flags);

	if (bi->flags & MULTIBOOT_MEMORY)
	{
		printf(" PC Memory: lower: %d, upper: %d.\n",
		       bi->mem_lower, bi->mem_upper);
	}
	if (bi->flags & MULTIBOOT_CMDLINE)
	{
		printf(" Kernel command line: `%s'\n",
			(char*)phystokv(bi->cmdline));
	}

	if (bi->flags & MULTIBOOT_MODS)
	{
		printf(" Boot modules: %d\n", bi->mods_count);
		m = (struct multiboot_module*)phystokv(bi->mods_addr);
		for (i = 0; i < bi->mods_count; i++)
		{
			printf("  Module %d: %08x-%08x (size %d)\n",
				i, m[i].mod_start, m[i].mod_end,
				m[i].mod_end - m[i].mod_start);
			if (m[i].string)
			{
				printf("   String: `%s' at %08x\n",
					(char*)phystokv(m[i].string),
					m[i].string);
			}
		}
	}

	if (bi->flags & MULTIBOOT_AOUT_SYMS)
	{
		printf(" Symbol table (a.out-style): start %08x,\n  symtab %08x, strtab %08x\n", bi->syms.a.addr, bi->syms.a.tabsize, bi->syms.a.strsize);
	}

	if (bi->flags & MULTIBOOT_BOOT_DEVICE)
	{
		printf(" Boot device: %d %d %d %d\n",
			bi->boot_device[0], bi->boot_device[1],
			bi->boot_device[2], bi->boot_device[3]);
	}
}

