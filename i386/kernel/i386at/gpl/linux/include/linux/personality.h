#ifndef _PERSONALITY_H
#define _PERSONALITY_H

#include <linux/linkage.h>
#include <linux/ptrace.h>


/* Flags for bug emulation. These occupy the top three bytes. */
#define STICKY_TIMEOUTS		0x4000000
#define WHOLE_SECONDS		0x2000000

/* Personality types. These go in the low byte. Avoid using the top bit,
 * it will conflict with error returns.
 */
#define PER_MASK		(0x00ff)
#define PER_LINUX		(0x0000)
#define PER_SVR4		(0x0001 | STICKY_TIMEOUTS)
#define PER_SVR3		(0x0002 | STICKY_TIMEOUTS)
#define PER_SCOSVR3		(0x0003 | STICKY_TIMEOUTS | WHOLE_SECONDS)
#define PER_WYSEV386		(0x0004 | STICKY_TIMEOUTS)
#define PER_ISCR4		(0x0005 | STICKY_TIMEOUTS)
#define PER_BSD			(0x0006)
#define PER_XENIX		(0x0007 | STICKY_TIMEOUTS)

/* Prototype for an lcall7 syscall handler. */
typedef asmlinkage void (*lcall7_func)(struct pt_regs *);


/* Description of an execution domain - personality range supported,
 * lcall7 syscall handler, start up / shut down functions etc.
 * N.B. The name and lcall7 handler must be where they are since the
 * offset of the handler is hard coded in kernel/sys_call.S.
 */
struct exec_domain {
	const char *name;
	lcall7_func handler;
	unsigned char pers_low, pers_high;
	unsigned long * signal_map;
	unsigned long * signal_invmap;
	int *use_count;
	struct exec_domain *next;
};

extern struct exec_domain default_exec_domain;

extern struct exec_domain *lookup_exec_domain(unsigned long personality);
extern int register_exec_domain(struct exec_domain *it);
extern int unregister_exec_domain(struct exec_domain *it);
extern asmlinkage int sys_personality(unsigned long personality);

#endif /* _PERSONALITY_H */
