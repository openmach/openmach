/*
 * bios32.c - BIOS32, PCI BIOS functions.
 *
 * Sponsored by
 *	iX Multiuser Multitasking Magazine
 *	Hannover, Germany
 *	hm@ix.de
 *
 * Copyright 1993, 1994 Drew Eckhardt
 *      Visionary Computing
 *      (Unix and Linux consulting and custom programming)
 *      Drew@Colorado.EDU
 *      +1 (303) 786-7975
 *
 * For more information, please consult
 *
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * M/S HF3-15A
 * 5200 N.E. Elam Young Parkway
 * Hillsboro, Oregon 97124-6497
 * +1 (503) 696-2000
 * +1 (800) 433-5177
 *
 * Manuals are $25 each or $50 for all three, plus $7 shipping
 * within the United States, $35 abroad.
 *
 *
 * CHANGELOG :
 * Jun 17, 1994 : Modified to accommodate the broken pre-PCI BIOS SPECIFICATION
 *	Revision 2.0 present on <thys@dennis.ee.up.ac.za>'s ASUS mainboard.
 *
 * Jan 5,  1995 : Modified to probe PCI hardware at boot time by Frederic
 *     Potter, potter@cao-vlsi.ibp.fr
 *
 * Jan 10, 1995 : Modified to store the information about configured pci
 *      devices into a list, which can be accessed via /proc/pci by
 *      Curtis Varner, cvarner@cs.ucr.edu
 *
 * Jan 12, 1995 : CPU-PCI bridge optimization support by Frederic Potter.
 *	Alpha version. Intel & UMC chipset support only.
 *
 * Apr 16, 1995 : Source merge with the DEC Alpha PCI support. Most of the code
 *	moved to drivers/pci/pci.c.
 *
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bios32.h>
#include <linux/pci.h>

#include <asm/segment.h>

#define PCIBIOS_PCI_FUNCTION_ID 	0xb1XX
#define PCIBIOS_PCI_BIOS_PRESENT 	0xb101
#define PCIBIOS_FIND_PCI_DEVICE		0xb102
#define PCIBIOS_FIND_PCI_CLASS_CODE	0xb103
#define PCIBIOS_GENERATE_SPECIAL_CYCLE	0xb106
#define PCIBIOS_READ_CONFIG_BYTE	0xb108
#define PCIBIOS_READ_CONFIG_WORD	0xb109
#define PCIBIOS_READ_CONFIG_DWORD	0xb10a
#define PCIBIOS_WRITE_CONFIG_BYTE	0xb10b
#define PCIBIOS_WRITE_CONFIG_WORD	0xb10c
#define PCIBIOS_WRITE_CONFIG_DWORD	0xb10d


/* BIOS32 signature: "_32_" */
#define BIOS32_SIGNATURE	(('_' << 0) + ('3' << 8) + ('2' << 16) + ('_' << 24))

/* PCI signature: "PCI " */
#define PCI_SIGNATURE		(('P' << 0) + ('C' << 8) + ('I' << 16) + (' ' << 24))

/* PCI service signature: "$PCI" */
#define PCI_SERVICE		(('$' << 0) + ('P' << 8) + ('C' << 16) + ('I' << 24))

/*
 * This is the standard structure used to identify the entry point
 * to the BIOS32 Service Directory, as documented in
 * 	Standard BIOS 32-bit Service Directory Proposal
 * 	Revision 0.4 May 24, 1993
 * 	Phoenix Technologies Ltd.
 *	Norwood, MA
 * and the PCI BIOS specification.
 */

union bios32 {
	struct {
		unsigned long signature;	/* _32_ */
		unsigned long entry;		/* 32 bit physical address */
		unsigned char revision;		/* Revision level, 0 */
		unsigned char length;		/* Length in paragraphs should be 01 */
		unsigned char checksum;		/* All bytes must add up to zero */
		unsigned char reserved[5]; 	/* Must be zero */
	} fields;
	char chars[16];
};

/*
 * Physical address of the service directory.  I don't know if we're
 * allowed to have more than one of these or not, so just in case
 * we'll make pcibios_present() take a memory start parameter and store
 * the array there.
 */

static unsigned long bios32_entry = 0;
static struct {
	unsigned long address;
	unsigned short segment;
} bios32_indirect = { 0, KERNEL_CS };

#ifdef CONFIG_PCI
/*
 * Returns the entry point for the given service, NULL on error
 */

static unsigned long bios32_service(unsigned long service)
{
	unsigned char return_code;	/* %al */
	unsigned long address;		/* %ebx */
	unsigned long length;		/* %ecx */
	unsigned long entry;		/* %edx */

	__asm__("lcall (%%edi)"
		: "=a" (return_code),
		  "=b" (address),
		  "=c" (length),
		  "=d" (entry)
		: "0" (service),
		  "1" (0),
		  "D" (&bios32_indirect));

	switch (return_code) {
		case 0:
			return address + entry;
		case 0x80:	/* Not present */
			printk("bios32_service(%ld) : not present\n", service);
			return 0;
		default: /* Shouldn't happen */
			printk("bios32_service(%ld) : returned 0x%x, mail drew@colorado.edu\n",
				service, return_code);
			return 0;
	}
}

static long pcibios_entry = 0;
static struct {
	unsigned long address;
	unsigned short segment;
} pci_indirect = { 0, KERNEL_CS };


extern unsigned long check_pcibios(unsigned long memory_start, unsigned long memory_end)
{
	unsigned long signature;
	unsigned char present_status;
	unsigned char major_revision;
	unsigned char minor_revision;
	int pack;

	if ((pcibios_entry = bios32_service(PCI_SERVICE))) {
		pci_indirect.address = pcibios_entry;

		__asm__("lcall (%%edi)\n\t"
			"jc 1f\n\t"
			"xor %%ah, %%ah\n"
			"1:\tshl $8, %%eax\n\t"
			"movw %%bx, %%ax"
			: "=d" (signature),
			  "=a" (pack)
			: "1" (PCIBIOS_PCI_BIOS_PRESENT),
			  "D" (&pci_indirect)
			: "bx", "cx");

		present_status = (pack >> 16) & 0xff;
		major_revision = (pack >> 8) & 0xff;
		minor_revision = pack & 0xff;
		if (present_status || (signature != PCI_SIGNATURE)) {
			printk ("pcibios_init : %s : BIOS32 Service Directory says PCI BIOS is present,\n"
				"	but PCI_BIOS_PRESENT subfunction fails with present status of 0x%x\n"
				"	and signature of 0x%08lx (%c%c%c%c).  mail drew@Colorado.EDU\n",
				(signature == PCI_SIGNATURE) ?  "WARNING" : "ERROR",
				present_status, signature,
				(char) (signature >>  0), (char) (signature >>  8),
				(char) (signature >> 16), (char) (signature >> 24));

			if (signature != PCI_SIGNATURE)
				pcibios_entry = 0;
		}
		if (pcibios_entry) {
			printk ("pcibios_init : PCI BIOS revision %x.%02x entry at 0x%lx\n",
				major_revision, minor_revision, pcibios_entry);
		}
	}
	return memory_start;
}

int pcibios_present(void)
{
	return pcibios_entry ? 1 : 0;
}

int pcibios_find_class (unsigned int class_code, unsigned short index,
	unsigned char *bus, unsigned char *device_fn)
{
	unsigned long bx;
	unsigned long ret;

	__asm__ ("lcall (%%edi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=b" (bx),
		  "=a" (ret)
		: "1" (PCIBIOS_FIND_PCI_CLASS_CODE),
		  "c" (class_code),
		  "S" ((int) index),
		  "D" (&pci_indirect));
	*bus = (bx >> 8) & 0xff;
	*device_fn = bx & 0xff;
	return (int) (ret & 0xff00) >> 8;
}


int pcibios_find_device (unsigned short vendor, unsigned short device_id,
	unsigned short index, unsigned char *bus, unsigned char *device_fn)
{
	unsigned short bx;
	unsigned short ret;

	__asm__("lcall (%%edi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=b" (bx),
		  "=a" (ret)
		: "1" (PCIBIOS_FIND_PCI_DEVICE),
		  "c" (device_id),
		  "d" (vendor),
		  "S" ((int) index),
		  "D" (&pci_indirect));
	*bus = (bx >> 8) & 0xff;
	*device_fn = bx & 0xff;
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_read_config_byte(unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned char *value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (PCIBIOS_READ_CONFIG_BYTE),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_read_config_word (unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned short *value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (PCIBIOS_READ_CONFIG_WORD),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_read_config_dword (unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned int *value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (PCIBIOS_READ_CONFIG_DWORD),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_byte (unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned char value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_BYTE),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_word (unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned short value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_WORD),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

int pcibios_write_config_dword (unsigned char bus,
	unsigned char device_fn, unsigned char where, unsigned int value)
{
	unsigned long ret;
	unsigned long bx = (bus << 8) | device_fn;

	__asm__("lcall (%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (PCIBIOS_WRITE_CONFIG_DWORD),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pci_indirect));
	return (int) (ret & 0xff00) >> 8;
}

const char *pcibios_strerror (int error)
{
	static char buf[80];

	switch (error) {
		case PCIBIOS_SUCCESSFUL:
			return "SUCCESSFUL";

		case PCIBIOS_FUNC_NOT_SUPPORTED:
			return "FUNC_NOT_SUPPORTED";

		case PCIBIOS_BAD_VENDOR_ID:
			return "SUCCESSFUL";

		case PCIBIOS_DEVICE_NOT_FOUND:
			return "DEVICE_NOT_FOUND";

		case PCIBIOS_BAD_REGISTER_NUMBER:
			return "BAD_REGISTER_NUMBER";

                case PCIBIOS_SET_FAILED:          
			return "SET_FAILED";

                case PCIBIOS_BUFFER_TOO_SMALL:    
			return "BUFFER_TOO_SMALL";

		default:
			sprintf (buf, "UNKNOWN RETURN 0x%x", error);
			return buf;
	}
}


unsigned long pcibios_fixup(unsigned long mem_start, unsigned long mem_end)
{
return mem_start;
}


#endif

unsigned long pcibios_init(unsigned long memory_start, unsigned long memory_end)
{
	union bios32 *check;
	unsigned char sum;
	int i, length;

	/*
	 * Follow the standard procedure for locating the BIOS32 Service
	 * directory by scanning the permissible address range from
	 * 0xe0000 through 0xfffff for a valid BIOS32 structure.
	 *
	 */

	for (check = (union bios32 *) 0xe0000; check <= (union bios32 *) 0xffff0; ++check) {
		if (check->fields.signature != BIOS32_SIGNATURE)
			continue;
		length = check->fields.length * 16;
		if (!length)
			continue;
		sum = 0;
		for (i = 0; i < length ; ++i)
			sum += check->chars[i];
		if (sum != 0)
			continue;
		if (check->fields.revision != 0) {
			printk("pcibios_init : unsupported revision %d at 0x%p, mail drew@colorado.edu\n",
				check->fields.revision, check);
			continue;
		}
		printk ("pcibios_init : BIOS32 Service Directory structure at 0x%p\n", check);
		if (!bios32_entry) {
			if (check->fields.entry >= 0x100000) {
				printk("pcibios_init: entry in high memory, unable to access\n");
			} else {
				bios32_indirect.address = bios32_entry = check->fields.entry;
				printk ("pcibios_init : BIOS32 Service Directory entry at 0x%lx\n", bios32_entry);
			}
		} else {
			printk ("pcibios_init : multiple entries, mail drew@colorado.edu\n");
			/*
			 * Jeremy Fitzhardinge reports at least one PCI BIOS
			 * with two different service directories, and as both
			 * worked for him, we'll just mention the fact, and
			 * not actually disallow it..
			 */
		}
	}
#ifdef CONFIG_PCI
	if (bios32_entry) {
		memory_start = check_pcibios (memory_start, memory_end);
	}
#endif
	return memory_start;
}
