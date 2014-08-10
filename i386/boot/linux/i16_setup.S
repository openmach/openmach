
#include <mach/machine/asm.h>

/* NOTE! These had better be the same as in bootsect.s! */
#include <linux/config.h>

#define INITSEG    DEF_INITSEG	/* we move boot here - out of the way */
#define SYSSEG     DEF_SYSSEG	/* system loaded at 0x10000 (65536). */
#define SETUPSEG   DEF_SETUPSEG	/* this is the current segment */

#define HIGHSECS	5	/* nr of sectors loaded at INITSEG */

#define STACK_SIZE 4096

	.text
	.code16

/*
 * Entrypoint from Linux boot loaders such as LILO.
 *
 * Note that whereas in the Linux 16-bit boot code,
 * the 'bootsect' and 'setup' fragments are compiled and linked separately,
 * in this code they are linked into one big small-model binary image.
 * This means that, whereas the Linux setup code expects to run at 9020:0000,
 * this setup code wants to run at 9000:0200.
 * Since LILO and other Linux boot loaders will be starting us at 9020:0000,
 * we need to fix the CS and EIP early on to conform to our environment.
 * That's what the ljmp instruction below is for.
 */
setup_entry:
	cld

	/* Move the rest of the linuxboot code and data up from SYSSEG.  */
	movw	$SYSSEG,%ax
	movw	%ax,%ds
	movw	$INITSEG,%ax
	movw	%ax,%es
	xorw	%si,%si
	movw	$HIGHSECS*512,%di
	movl	$_edata-HIGHSECS*512,%ecx
	rep
	movsb

	/* Jump to the segment we want to be running in.  */
	ljmp	$INITSEG,$0f
0:
	movw	%ax,%ds

	/* Switch to a stack in the same segment as everything else.  */
	movw	%ax,%ss
	movl	$stack+STACK_SIZE,%esp
	.comm	stack,STACK_SIZE

	/* Clear our BSS segment.  */
	movl	$EXT(edata),%edi
	movl	$EXT(end),%ecx
	subw	%di,%cx
	xorb	%al,%al
	cld
	rep
	stosb

	jmp	EXT(i16_main)

