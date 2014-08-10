#ifndef _I386_PTRACE_H
#define _I386_PTRACE_H

#define EBX 0
#define ECX 1
#define EDX 2
#define ESI 3
#define EDI 4
#define EBP 5
#define EAX 6
#define DS 7
#define ES 8
#define FS 9
#define GS 10
#define ORIG_EAX 11
#define EIP 12
#define CS  13
#define EFL 14
#define UESP 15
#define SS   16


/* this struct defines the way the registers are stored on the 
   stack during a system call. */

struct pt_regs {
	long ebx;
	long ecx;
	long edx;
	long esi;
	long edi;
	long ebp;
	long eax;
	unsigned short ds, __dsu;
	unsigned short es, __esu;
	unsigned short fs, __fsu;
	unsigned short gs, __gsu;
	long orig_eax;
	long eip;
	unsigned short cs, __csu;
	long eflags;
	long esp;
	unsigned short ss, __ssu;
};

#ifdef __KERNEL__
#define user_mode(regs) ((VM_MASK & (regs)->eflags) || (3 & (regs)->cs))
#define instruction_pointer(regs) ((regs)->eip)
extern void show_regs(struct pt_regs *);
#endif

#endif
