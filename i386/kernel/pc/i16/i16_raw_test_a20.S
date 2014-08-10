
#include <mach/machine/asm.h>

	.text
	.code16

/*
 * Test the A20 address line; return true if it is enabled.
 */
ENTRY(i16_raw_test_a20)
	xorw	%ax,%ax
	movw	%ax,%fs
	notw	%ax
	movw	%ax,%gs

	/* See if the values in already in the corresponding locations
	   are the same.  */
	movw	%fs:0,%ax
	cmpw	%gs:16,%ax
	jnz	1f

	/* Yes; try changing one and see if they're still the same.  */
	movw	%ax,%dx
	notw	%ax
	movw	%ax,%fs:0
	cmpw	%gs:16,%ax
	movw	%dx,%fs:0
	jnz	1f

	xorl	%eax,%eax
	ret
1:
	movl	$1,%eax
	ret

