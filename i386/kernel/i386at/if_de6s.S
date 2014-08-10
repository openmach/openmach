/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 * HISTORY
 * $Log: if_de6s.S,v $
# Revision 1.3  1995/04/26  19:22:12  baford
# got alignment working right for both ELF and a.out
#
# Revision 1.2  1995/04/25  16:13:28  baford
# got kernel working with ELF build tools
#
# Revision 1.1  1994/11/08  20:47:25  baford
# merged in CMU's MK83-MK83a diffs
#
 * Revision 2.2  93/11/17  18:33:19  dbg
 * 	Moved source into kernel/i386at/DLINK/if_de6c.c, since we
 * 	can't release it but don't want to lose it.
 * 	[93/11/17            dbg]
 * 
 * Revision 2.2.2.1  93/09/21  21:00:39  dbg
 * 	<no changes>
 * 
 * Revision 2.2.1.1  93/09/03  15:06:26  dbg
 * 	Created.
 * 	[92/08/13            rvb]
 * 
 *
 *	File:	if_de6s.s
 *	Author: Robert V. Baron
 */

#include <mach/machine/asm.h>
#undef DATA

#include <i386at/if_de6c.h>

	P2ALIGN(2)
de6csetmemaddr:
	movl	8(%ebp), %ebx		/* addr */
	movb 	%bl, %al		/* low byte; low nibble */
	salb	$4, %al
	orb	$(RW_ADR), %al
	outb	%al, %dx
	  movl	%edi, %ecx
0:	  loop	0b

	movb	%bl, %al		/* low byte; high nibble */
	andb	$0xf0, %al
	orb	$(RW_ADR|STROBE), %al
	outb	%al, %dx
	  movl	%edi, %ecx
0:	  loop	0b

	movb	%bh, %al		/* high byte; low nibble */
	salb	$4, %al
	orb	$(RW_ADR), %al
	outb	%al, %dx
	  movl	%edi, %ecx
0:	  loop	0b

	movb	%bh, %al		/* high byte; high nibble */
	andb	$0xf0, %al
	orb	$(RW_ADR|STROBE), %al
	outb	%al, %dx
	  movl	%edi, %ecx
0:	  loop	0b
	ret

/* de6cwriteasm(address, buf, len, port, delay) */
ENTRY(de6cwriteasm)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	20(%ebp), %edx		/* port */
	movl	24(%ebp), %edi		/* delay */

	call	de6csetmemaddr

	cld
	movl	16(%ebp), %ecx		/* cnt */
	movl	12(%ebp), %esi		/* source */

	cmpl	$1, %edi		/* no delay/latency */
	je	2f
1:	lodsb				/* leave delay/latency */
	  pushl	%ecx
	  movb	%al, %bl		/* high byte; low nibble */
	  salb	$4, %al
	  outb	%al, %dx
	    movl  %edi, %ecx
0:	    loop  0b

	  movb	%bl, %al		/* high byte; high nibble */
	  andb	$0xf0, %al
	  orb	$(WRITE|STROBE), %al	/* NB: WRITE == 0 */
	  outb	%al, %dx
	    movl  %edi, %ecx
0:	    loop  0b
	  popl	%ecx
	loop	1b

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret
					/* edi and ebx free */
2:	lodsb
	  movb	%al, %bl		/* high byte; low nibble */
	  salb	$4, %al
	  outb	%al, %dx

	  movb	%bl, %al		/* high byte; high nibble */
	  andb	$0xf0, %al
	  orb	$(WRITE|STROBE), %al	/* NB: WRITE == 0 */
	  outb	%al, %dx
	loop	2b

6:	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret


/* de6creadasm(address, buf, len, port, delay) */
ENTRY(de6creadasm)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	movl	20(%ebp), %edx		/* port */
	movl	24(%ebp), %edi		/* delay for desetmemaddr*/
	movl	%edi, %esi		/* delay; cause edi used by stosl */

	call	de6csetmemaddr

	cld
	movl	16(%ebp), %ecx		/* cnt */
	movl	12(%ebp), %edi		/* destination */
	movw	$0x0901, %bx		/* bl = 1 = READ; bh = READ|STROBE */

#ifdef	out_in
	cmpl	$1, %esi		/* no delay/latency */
	je	2f
#endif	/* out_in */
1:	xorw	%ax, %ax		/* leave delay/latency */
	pushl	%ecx
	  movb	%bl, %al
	  outb	%al, %dx
	    movl  %esi, %ecx
0:	    loop  0b
	   incw	%dx			/* inb from STAT == port + 1 */
	   inb	%dx, %al		/* first byte high nibble goes into */
	   decw	%dx
	  salw	$4, %ax			/* ... low nibble formed byte */

	  movb	%bh, %al
	  outb	%al, %dx
	    movl  %esi, %ecx
0:	    loop  0b
	   incw	%dx			/* inb from STAT == port + 1 */
	   inb	%dx, %al		/* second byte high nibble goes into */
	   decw	%dx
	  andb	$0xf0, %al		/* ... high nibble formed byte */
	  orb	%ah, %al
	stosb
	popl	%ecx
	loop	1b

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret

2:	xorw	%ax, %ax		/* leave delay/latency */
	  movb	%bl, %al
	  outb	%al, %dx
	   incw	%dx			/* inb from STAT == port + 1 */
	   inb	%dx, %al		/* high nibble goes into low nibble */
	   decw	%dx
	  salw	$4, %ax

	  movb	%bh, %al
	  outb	%al, %dx
	   incw	%dx			/* inb from STAT == port + 1 */
	   inb	%dx, %al
	   decw	%dx
	  andb	$0xf0, %al
	  orb	%ah, %al
	stosb
	loop	2b

	popl	%ebx
	popl	%esi
	popl	%edi
	leave
	ret


#ifdef	unroll_wins
	unrolled loop for write iff no delay
2:	lodsl
	  movl	%eax, %ebx		/* byte one; low nibble */
	  salb	$4, %al
	  outb	%al, %dx

	  movb	%bl, %al		/* byte one; high nibble */
	  andb	$0xf0, %al
	  orb	$8, %al
	  outb	%al, %dx
loop	3f
jmp	6f
3:	  sarl	$8, %ebx
	  movb	%bl, %al		/* byte two; low nibble */
	  salb	$4, %al
	  outb	%al, %dx

	  movb	%bl, %al		/* byte two; high nibble */
	  andb	$0xf0, %al
	  orb	$8, %al
	  outb	%al, %dx
loop	4f
jmp	6f
4:	  sarl	$8, %ebx
	  movb	%bl, %al		/* byte three; low nibble */
	  salb	$4, %al
	  outb	%al, %dx

	  movb	%bl, %al		/* byte three; high nibble */
	  andb	$0xf0, %al
	  orb	$8, %al
	  outb	%al, %dx
loop	5f
jmp	6f
5:	  sarl	$8, %ebx
	  movb	%bl, %al		/* byte three; low nibble */
	  salb	$4, %al
	  outb	%al, %dx

	  movb	%bl, %al		/* byte four; high nibble */
	  andb	$0xf0, %al
	  orb	$8, %al
	  outb	%al, %dx
	loop	2b
#endif	/* unroll_wins */

