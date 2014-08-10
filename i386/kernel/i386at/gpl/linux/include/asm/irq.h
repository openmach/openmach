#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 *	linux/include/asm/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds
 *
 *	IRQ/IPI changes taken from work by Thomas Radke <tomsoft@informatik.tu-chemnitz.de>
 */

#include <linux/linkage.h>
#include <asm/segment.h>

#define NR_IRQS 16

extern void disable_irq(unsigned int);
extern void enable_irq(unsigned int);

#define __STR(x) #x
#define STR(x) __STR(x)
 
#define SAVE_ALL \
	"cld\n\t" \
	"push %gs\n\t" \
	"push %fs\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %ebp\n\t" \
	"pushl %edi\n\t" \
	"pushl %esi\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"pushl %ebx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t" \
	"movl $" STR(USER_DS) ",%edx\n\t" \
	"mov %dx,%fs\n\t"   \
	"movl $0,%edx\n\t"  \
	"movl %edx,%db7\n\t"

/*
 * SAVE_MOST/RESTORE_MOST is used for the faster version of IRQ handlers,
 * installed by using the SA_INTERRUPT flag. These kinds of IRQ's don't
 * call the routines that do signal handling etc on return, and can have
 * more relaxed register-saving etc. They are also atomic, and are thus
 * suited for small, fast interrupts like the serial lines or the harddisk
 * drivers, which don't actually need signal handling etc.
 *
 * Also note that we actually save only those registers that are used in
 * C subroutines (%eax, %edx and %ecx), so if you do something weird,
 * you're on your own. The only segments that are saved (not counting the
 * automatic stack and code segment handling) are %ds and %es, and they
 * point to kernel space. No messing around with %fs here.
 */
#define SAVE_MOST \
	"cld\n\t" \
	"push %es\n\t" \
	"push %ds\n\t" \
	"pushl %eax\n\t" \
	"pushl %edx\n\t" \
	"pushl %ecx\n\t" \
	"movl $" STR(KERNEL_DS) ",%edx\n\t" \
	"mov %dx,%ds\n\t" \
	"mov %dx,%es\n\t"

#define RESTORE_MOST \
	"popl %ecx\n\t" \
	"popl %edx\n\t" \
	"popl %eax\n\t" \
	"pop %ds\n\t" \
	"pop %es\n\t" \
	"iret"

/*
 * The "inb" instructions are not needed, but seem to change the timings
 * a bit - without them it seems that the harddisk driver won't work on
 * all hardware. Arghh.
 */
#define ACK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ","SYMBOL_NAME_STR(cache_21)"\n\t" \
	"movb "SYMBOL_NAME_STR(cache_21)",%al\n\t" \
	"outb %al,$0x21\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0x20\n\t"

#define ACK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\torb $" #mask ","SYMBOL_NAME_STR(cache_A1)"\n\t" \
	"movb "SYMBOL_NAME_STR(cache_A1)",%al\n\t" \
	"outb %al,$0xA1\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tmovb $0x20,%al\n\t" \
	"outb %al,$0xA0\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\toutb %al,$0x20\n\t"

#define UNBLK_FIRST(mask) \
	"inb $0x21,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),"SYMBOL_NAME_STR(cache_21)"\n\t" \
	"movb "SYMBOL_NAME_STR(cache_21)",%al\n\t" \
	"outb %al,$0x21\n\t"

#define UNBLK_SECOND(mask) \
	"inb $0xA1,%al\n\t" \
	"jmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:\tandb $~(" #mask "),"SYMBOL_NAME_STR(cache_A1)"\n\t" \
	"movb "SYMBOL_NAME_STR(cache_A1)",%al\n\t" \
	"outb %al,$0xA1\n\t"

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)
#define FAST_IRQ_NAME(nr) IRQ_NAME2(fast_IRQ##nr)
#define BAD_IRQ_NAME(nr) IRQ_NAME2(bad_IRQ##nr)

#ifdef	__SMP__

#ifndef __SMP_PROF__
#define SMP_PROF_INT_SPINS 
#define SMP_PROF_IPI_CNT 
#else
#define SMP_PROF_INT_SPINS "incl "SYMBOL_NAME_STR(smp_spins)"(,%eax,4)\n\t"
#define SMP_PROF_IPI_CNT "incl "SYMBOL_NAME_STR(ipi_count)"\n\t" 
#endif

#define GET_PROCESSOR_ID \
	"movl "SYMBOL_NAME_STR(apic_reg)", %edx\n\t" \
	"movl 32(%edx), %eax\n\t" \
	"shrl $24,%eax\n\t" \
	"andb $0x0F,%al\n"
	
#define	ENTER_KERNEL \
	"pushl %eax\n\t" \
	"pushl %edx\n\t" \
	"pushfl\n\t" \
	"cli\n\t" \
	GET_PROCESSOR_ID \
	"1: " \
	"lock\n\t" \
	"btsl $0, "SYMBOL_NAME_STR(kernel_flag)"\n\t" \
	"jnc 3f\n\t" \
	"cmpb "SYMBOL_NAME_STR(active_kernel_processor)", %al\n\t" \
	"je 4f\n\t" \
	"2: " \
        SMP_PROF_INT_SPINS \
	"btl %al, "SYMBOL_NAME_STR(smp_invalidate_needed)"\n\t" \
	"jnc 5f\n\t" \
	"lock\n\t" \
	"btrl %al, "SYMBOL_NAME_STR(smp_invalidate_needed)"\n\t" \
	"jnc 5f\n\t" \
	"movl %cr3,%edx\n\t" \
	"movl %edx,%cr3\n" \
	"5: btl $0, "SYMBOL_NAME_STR(kernel_flag)"\n\t" \
	"jc 2b\n\t" \
	"jmp 1b\n\t" \
	"3: " \
	"movb %al, "SYMBOL_NAME_STR(active_kernel_processor)"\n\t" \
	"4: " \
	"incl "SYMBOL_NAME_STR(kernel_counter)"\n\t" \
	"popfl\n\t" \
	"popl %edx\n\t" \
	"popl %eax\n\t"

#define	LEAVE_KERNEL \
	"pushfl\n\t" \
	"cli\n\t" \
	"decl "SYMBOL_NAME_STR(kernel_counter)"\n\t" \
	"jnz 1f\n\t" \
	"movb $" STR (NO_PROC_ID) ", "SYMBOL_NAME_STR(active_kernel_processor)"\n\t" \
	"lock\n\t" \
	"btrl $0, "SYMBOL_NAME_STR(kernel_flag)"\n\t" \
	"1: " \
	"popfl\n\t"
	
	
/*
 *	the syscall count inc is a gross hack because ret_from_syscall is used by both irq and
 *	syscall return paths (urghh).
 */
 
#define BUILD_IRQ(chip,nr,mask) \
asmlinkage void IRQ_NAME(nr); \
asmlinkage void FAST_IRQ_NAME(nr); \
asmlinkage void BAD_IRQ_NAME(nr); \
__asm__( \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(IRQ) #nr "_interrupt:\n\t" \
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ENTER_KERNEL \
	ACK_##chip(mask) \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t"\
	"sti\n\t" \
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_IRQ)"\n\t" \
	"addl $8,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"incl "SYMBOL_NAME_STR(syscall_count)"\n\t" \
	"jmp ret_from_sys_call\n" \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(fast_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ENTER_KERNEL \
	ACK_##chip(mask) \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_fast_IRQ)"\n\t" \
	"addl $4,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	LEAVE_KERNEL \
	RESTORE_MOST \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(bad_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ENTER_KERNEL \
	ACK_##chip(mask) \
	LEAVE_KERNEL \
	RESTORE_MOST);
	
	
/*
 *	Message pass must be a fast IRQ..
 */

#define BUILD_MSGIRQ(chip,nr,mask) \
asmlinkage void IRQ_NAME(nr); \
asmlinkage void FAST_IRQ_NAME(nr); \
asmlinkage void BAD_IRQ_NAME(nr); \
__asm__( \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(IRQ) #nr "_interrupt:\n\t" \
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ENTER_KERNEL \
	ACK_##chip(mask) \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t"\
	"sti\n\t" \
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_IRQ)"\n\t" \
	"addl $8,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"incl "SYMBOL_NAME_STR(syscall_count)"\n\t" \
	"jmp ret_from_sys_call\n" \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(fast_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	SMP_PROF_IPI_CNT \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_fast_IRQ)"\n\t" \
	"addl $4,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	RESTORE_MOST \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(bad_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	RESTORE_MOST);

#define BUILD_RESCHEDIRQ(nr) \
asmlinkage void IRQ_NAME(nr); \
__asm__( \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(IRQ) #nr "_interrupt:\n\t" \
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ENTER_KERNEL \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t"\
	"sti\n\t" \
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(smp_reschedule_irq)"\n\t" \
	"addl $8,%esp\n\t" \
	"cli\n\t" \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"incl "SYMBOL_NAME_STR(syscall_count)"\n\t" \
	"jmp ret_from_sys_call\n");
#else
	
#define BUILD_IRQ(chip,nr,mask) \
asmlinkage void IRQ_NAME(nr); \
asmlinkage void FAST_IRQ_NAME(nr); \
asmlinkage void BAD_IRQ_NAME(nr); \
__asm__( \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(IRQ) #nr "_interrupt:\n\t" \
	"pushl $-"#nr"-2\n\t" \
	SAVE_ALL \
	ACK_##chip(mask) \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t"\
	"sti\n\t" \
	"movl %esp,%ebx\n\t" \
	"pushl %ebx\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_IRQ)"\n\t" \
	"addl $8,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"jmp ret_from_sys_call\n" \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(fast_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	"incl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	"pushl $" #nr "\n\t" \
	"call "SYMBOL_NAME_STR(do_fast_IRQ)"\n\t" \
	"addl $4,%esp\n\t" \
	"cli\n\t" \
	UNBLK_##chip(mask) \
	"decl "SYMBOL_NAME_STR(intr_count)"\n\t" \
	RESTORE_MOST \
"\n"__ALIGN_STR"\n" \
SYMBOL_NAME_STR(bad_IRQ) #nr "_interrupt:\n\t" \
	SAVE_MOST \
	ACK_##chip(mask) \
	RESTORE_MOST);

#endif
#endif
