
#include <mach/machine/asm.h>

	.text

ENTRY(do_boot)
	movw	%dx,%ds
	movw	%dx,%es
	movw	%dx,%fs
	movw	%dx,%gs
	movw	%dx,%ss

	rep
	movsb

	jmp	*%eax
ENTRY(do_boot_end)

