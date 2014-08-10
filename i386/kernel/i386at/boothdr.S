
#include <mach/machine/asm.h>

#include "i386asm.h"

	.text

	/* We should never be entered this way.  */
	.globl	start,_start
start:
_start:
	jmp	boot_entry

	/* MultiBoot header - see multiboot.h.  */
#define	MULTIBOOT_MAGIC		0x1BADB002
#ifdef __ELF__
#define MULTIBOOT_FLAGS		0x00000002
#else  /* __ELF__ */
#define MULTIBOOT_FLAGS		0x00010002
#endif /* __ELF__ */
	P2ALIGN(2)
boot_hdr:
	.long	MULTIBOOT_MAGIC
	.long	MULTIBOOT_FLAGS
		/*
		 * The next item here is the checksum.
		 * XX this works OK until we need at least the 30th bit.
		 */
	.long	- (MULTIBOOT_MAGIC+MULTIBOOT_FLAGS)
#ifndef __ELF__	/* a.out kludge */
	.long	boot_hdr	/* header_addr */
	.long	_start		/* load_addr */
	.long	_edata		/* load_end_addr */
	.long	_end		/* bss_end_addr */
	.long	boot_entry	/* entry */
#endif /* __ELF__ */

boot_entry:

	/* Switch to our own interrupt stack.  */
	movl	$_intstack+INTSTACK_SIZE,%esp

	/* Reset EFLAGS to a known state.  */
	pushl	$0
	popf

	/* Clear uninitialized data.  */
	lea     _edata,%edi
	lea     _end,%ecx
	subl    %edi,%ecx
	xorl    %eax,%eax
	rep
	stosb

	/* Push the boot_info pointer to be the second argument.  */
	pushl	%ebx

	/* Jump into C code.  */
	call	EXT(c_boot_entry)

	.comm	_intstack,INTSTACK_SIZE

