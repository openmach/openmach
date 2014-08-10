#ifndef __ASM_SYSTEM_H
#define __ASM_SYSTEM_H

#include <asm/segment.h>

/*
 * Entry into gdt where to find first TSS. GDT layout:
 *   0 - nul
 *   1 - kernel code segment
 *   2 - kernel data segment
 *   3 - user code segment
 *   4 - user data segment
 * ...
 *   8 - TSS #0
 *   9 - LDT #0
 *  10 - TSS #1
 *  11 - LDT #1
 */
#define FIRST_TSS_ENTRY 8
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
#define load_TR(n) __asm__("ltr %%ax": /* no output */ :"a" (_TSS(n)))
#define load_ldt(n) __asm__("lldt %%ax": /* no output */ :"a" (_LDT(n)))
#define store_TR(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"0" (0),"i" (FIRST_TSS_ENTRY<<3))

/* This special macro can be used to load a debugging register */

#define loaddebug(register) \
		__asm__("movl %0,%%edx\n\t" \
			"movl %%edx,%%db" #register "\n\t" \
			: /* no output */ \
			:"m" (current->debugreg[register]) \
			:"dx");


/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * the math co-processor latest.
 *
 * It also reloads the debug regs if necessary..
 */

 
#ifdef __SMP__
	/*
	 *	Keep the lock depth straight. If we switch on an interrupt from
	 *	kernel->user task we need to lose a depth, and if we switch the
	 *	other way we need to gain a depth. Same layer switches come out
	 *	the same.
	 *
	 *	We spot a switch in user mode because the kernel counter is the
	 *	same as the interrupt counter depth. (We never switch during the
	 *	message/invalidate IPI).
	 *
	 *	We fsave/fwait so that an exception goes off at the right time
	 *	(as a call from the fsave or fwait in effect) rather than to
	 *	the wrong process.
	 */

#define switch_to(tsk) do { \
	cli();\
	if(current->flags&PF_USEDFPU) \
	{ \
		__asm__ __volatile__("fnsave %0":"=m" (current->tss.i387.hard)); \
		__asm__ __volatile__("fwait"); \
		current->flags&=~PF_USEDFPU;	 \
	} \
	current->lock_depth=syscall_count; \
	kernel_counter+=next->lock_depth-current->lock_depth; \
	syscall_count=next->lock_depth; \
__asm__("pushl %%edx\n\t" \
	"movl "SYMBOL_NAME_STR(apic_reg)",%%edx\n\t" \
	"movl 0x20(%%edx), %%edx\n\t" \
	"shrl $22,%%edx\n\t" \
	"and  $0x3C,%%edx\n\t" \
	"xchgl %%ecx,"SYMBOL_NAME_STR(current_set)"(,%%edx)\n\t" \
	"popl %%edx\n\t" \
	"ljmp %0\n\t" \
	"sti\n\t" \
	: /* no output */ \
	:"m" (*(((char *)&tsk->tss.tr)-4)), \
	 "c" (tsk) \
	:"cx"); \
	/* Now maybe reload the debug registers */ \
	if(current->debugreg[7]){ \
		loaddebug(0); \
		loaddebug(1); \
		loaddebug(2); \
		loaddebug(3); \
		loaddebug(6); \
	} \
} while (0)

#else
#define switch_to(tsk) do { \
__asm__("cli\n\t" \
	"xchgl %%ecx,"SYMBOL_NAME_STR(current_set)"\n\t" \
	"ljmp %0\n\t" \
	"sti\n\t" \
	"cmpl %%ecx,"SYMBOL_NAME_STR(last_task_used_math)"\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	: /* no output */ \
	:"m" (*(((char *)&tsk->tss.tr)-4)), \
	 "c" (tsk) \
	:"cx"); \
	/* Now maybe reload the debug registers */ \
	if(current->debugreg[7]){ \
		loaddebug(0); \
		loaddebug(1); \
		loaddebug(2); \
		loaddebug(3); \
		loaddebug(6); \
	} \
} while (0)
#endif

#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2" \
	: /* no output */ \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
	 "d" (base) \
	:"dx")

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1" \
	: /* no output */ \
	:"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "d" (limit) \
	:"dx")

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

static inline unsigned long _get_base(char * addr)
{
	unsigned long __base;
	__asm__("movb %3,%%dh\n\t"
		"movb %2,%%dl\n\t"
		"shll $16,%%edx\n\t"
		"movw %1,%%dx"
		:"=&d" (__base)
		:"m" (*((addr)+2)),
		 "m" (*((addr)+4)),
		 "m" (*((addr)+7)));
	return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

static inline unsigned long get_limit(unsigned long segment)
{
	unsigned long __limit;
	__asm__("lsll %1,%0"
		:"=r" (__limit):"r" (segment));
	return __limit+1;
}

#define nop() __asm__ __volatile__ ("nop")

/*
 * Clear and set 'TS' bit respectively
 */
#define clts() __asm__ __volatile__ ("clts")
#define stts() \
__asm__ __volatile__ ( \
	"movl %%cr0,%%eax\n\t" \
	"orl $8,%%eax\n\t" \
	"movl %%eax,%%cr0" \
	: /* no outputs */ \
	: /* no inputs */ \
	:"ax")


#define xchg(ptr,x) ((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))
#define tas(ptr) (xchg((ptr),1))

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((volatile struct __xchg_dummy *)(x))

static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
	switch (size) {
		case 1:
			__asm__("xchgb %b0,%1"
				:"=q" (x), "=m" (*__xg(ptr))
				:"0" (x), "m" (*__xg(ptr)));
			break;
		case 2:
			__asm__("xchgw %w0,%1"
				:"=r" (x), "=m" (*__xg(ptr))
				:"0" (x), "m" (*__xg(ptr)));
			break;
		case 4:
			__asm__("xchgl %0,%1"
				:"=r" (x), "=m" (*__xg(ptr))
				:"0" (x), "m" (*__xg(ptr)));
			break;
	}
	return x;
}

#define mb()  __asm__ __volatile__ (""   : : :"memory")
#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")

#define save_flags(x) \
__asm__ __volatile__("pushfl ; popl %0":"=r" (x): /* no input */ :"memory")

#define restore_flags(x) \
__asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"r" (x):"memory")

#define iret() __asm__ __volatile__ ("iret": : :"memory")

#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ __volatile__ ("movw %%dx,%%ax\n\t" \
	"movw %2,%%dx\n\t" \
	"movl %%eax,%0\n\t" \
	"movl %%edx,%1" \
	:"=m" (*((long *) (gate_addr))), \
	 "=m" (*(1+(long *) (gate_addr))) \
	:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	 "d" ((char *) (addr)),"a" (KERNEL_CS << 16) \
	:"ax","dx")

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

#define set_call_gate(a,addr) \
	_set_gate(a,12,3,addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*((gate_addr)+1) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*(gate_addr) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,limit,type) \
__asm__ __volatile__ ("movw $" #limit ",%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	: /* no output */ \
	:"a" (addr+0xc0000000), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),235,"0x89")
#define set_ldt_desc(n,addr,size) \
	_set_tssldt_desc(((char *) (n)),((int)(addr)),((size << 3) - 1),"0x82")

/*
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt;

/*
 * disable hlt during certain critical i/o operations
 */
#ifndef MACH
#define HAVE_DISABLE_HLT
#endif
void disable_hlt(void);
void enable_hlt(void);

#endif
