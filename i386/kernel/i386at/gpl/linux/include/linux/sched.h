#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

/*
 * define DEBUG if you want the wait-queues to have some extra
 * debugging code. It's not normally used, but might catch some
 * wait-queue coding errors.
 *
 *  #define DEBUG
 */

#include <asm/param.h>	/* for HZ */

extern unsigned long intr_count;
extern unsigned long event;

#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/tasks.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/page.h>

#include <linux/smp.h>
#include <linux/tty.h>
#include <linux/sem.h>

/*
 * cloning flags:
 */
#define CSIGNAL		0x000000ff	/* signal mask to be sent at exit */
#define CLONE_VM	0x00000100	/* set if VM shared between processes */
#define CLONE_FS	0x00000200	/* set if fs info shared between processes */
#define CLONE_FILES	0x00000400	/* set if open files shared between processes */
#define CLONE_SIGHAND	0x00000800	/* set if signal handlers shared */
#define CLONE_PID	0x00001000	/* set if pid shared */

/*
 * These are the constant used to fake the fixed-point load-average
 * counting. Some notes:
 *  - 11 bit fractions expand to 22 bits by the multiplies: this gives
 *    a load-average precision of 10 bits integer + 11 bits fractional
 *  - if you want to count load-averages more often, you need more
 *    precision, or rounding will get you. With 2-second counting freq,
 *    the EXP_n values would be 1981, 2034 and 2043 if still using only
 *    11 bit fractions.
 */
extern unsigned long avenrun[];		/* Load averages */

#define FSHIFT		11		/* nr of bits of precision */
#define FIXED_1		(1<<FSHIFT)	/* 1.0 as fixed-point */
#define LOAD_FREQ	(5*HZ)		/* 5 sec intervals */
#define EXP_1		1884		/* 1/exp(5sec/1min) as fixed-point */
#define EXP_5		2014		/* 1/exp(5sec/5min) */
#define EXP_15		2037		/* 1/exp(5sec/15min) */

#define CALC_LOAD(load,exp,n) \
	load *= exp; \
	load += n*(FIXED_1-exp); \
	load >>= FSHIFT;

#define CT_TO_SECS(x)	((x) / HZ)
#define CT_TO_USECS(x)	(((x) % HZ) * 1000000/HZ)

extern int nr_running, nr_tasks;

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/signal.h>
#include <linux/time.h>
#include <linux/param.h>
#include <linux/resource.h>
#include <linux/vm86.h>
#include <linux/math_emu.h>
#include <linux/ptrace.h>
#include <linux/timer.h>

#include <asm/processor.h>

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4
#define TASK_SWAPPING		5

/*
 * Scheduling policies
 */
#define SCHED_OTHER		0
#define SCHED_FIFO		1
#define SCHED_RR		2

struct sched_param {
	int sched_priority;
};

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifdef __KERNEL__

#define barrier() __asm__("": : :"memory")

extern void sched_init(void);
extern void show_state(void);
extern void trap_init(void);

asmlinkage void schedule(void);

struct files_struct {
	int count;
	fd_set close_on_exec;
	struct file * fd[NR_OPEN];
};

#define INIT_FILES { \
	1, \
	{ { 0, } }, \
	{ NULL, } \
}

struct fs_struct {
	int count;
	unsigned short umask;
	struct inode * root, * pwd;
};

#define INIT_FS { \
	1, \
	0022, \
	NULL, NULL \
}

struct mm_struct {
	int count;
	pgd_t * pgd;
	unsigned long context;
	unsigned long start_code, end_code, start_data, end_data;
	unsigned long start_brk, brk, start_stack, start_mmap;
	unsigned long arg_start, arg_end, env_start, env_end;
	unsigned long rss, total_vm, locked_vm;
	unsigned long def_flags;
	struct vm_area_struct * mmap;
	struct vm_area_struct * mmap_avl;
};

#define INIT_MM { \
		1, \
		swapper_pg_dir, \
		0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, \
		0, \
		&init_mmap, &init_mmap }

struct signal_struct {
	int count;
	struct sigaction action[32];
};

#define INIT_SIGNALS { \
		1, \
		{ {0,}, } }

struct task_struct {
/* these are hardcoded - don't touch */
	volatile long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	unsigned long signal;
	unsigned long blocked;	/* bitmap of masked signals */
	unsigned long flags;	/* per process flags, defined below */
	int errno;
	long debugreg[8];  /* Hardware debugging registers */
	struct exec_domain *exec_domain;
/* various fields */
	struct linux_binfmt *binfmt;
	struct task_struct *next_task, *prev_task;
	struct task_struct *next_run,  *prev_run;
	unsigned long saved_kernel_stack;
	unsigned long kernel_stack_page;
	int exit_code, exit_signal;
	unsigned long personality;
	int dumpable:1;
	int did_exec:1;
	int pid,pgrp,tty_old_pgrp,session,leader;
	int	groups[NGROUPS];
	/* 
	 * pointers to (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with 
	 * p->p_pptr->pid)
	 */
	struct task_struct *p_opptr, *p_pptr, *p_cptr, *p_ysptr, *p_osptr;
	struct wait_queue *wait_chldexit;	/* for wait4() */
	unsigned short uid,euid,suid,fsuid;
	unsigned short gid,egid,sgid,fsgid;
	unsigned long timeout, policy, rt_priority;
	unsigned long it_real_value, it_prof_value, it_virt_value;
	unsigned long it_real_incr, it_prof_incr, it_virt_incr;
	struct timer_list real_timer;
	long utime, stime, cutime, cstime, start_time;
/* mm fault and swap info: this can arguably be seen as either mm-specific or thread-specific */
	unsigned long min_flt, maj_flt, nswap, cmin_flt, cmaj_flt, cnswap;
	int swappable:1;
	unsigned long swap_address;
	unsigned long old_maj_flt;	/* old value of maj_flt */
	unsigned long dec_flt;		/* page fault count of the last time */
	unsigned long swap_cnt;		/* number of pages to swap on next pass */
/* limits */
	struct rlimit rlim[RLIM_NLIMITS];
	unsigned short used_math;
	char comm[16];
/* file system info */
	int link_count;
	struct tty_struct *tty; /* NULL if no tty */
/* ipc stuff */
	struct sem_undo *semundo;
	struct sem_queue *semsleeping;
/* ldt for this task - used by Wine.  If NULL, default_ldt is used */
	struct desc_struct *ldt;
/* tss for this task */
	struct thread_struct tss;
/* filesystem information */
	struct fs_struct *fs;
/* open file information */
	struct files_struct *files;
/* memory management info */
	struct mm_struct *mm;
/* signal handlers */
	struct signal_struct *sig;
#ifdef __SMP__
	int processor;
	int last_processor;
	int lock_depth;		/* Lock depth. We can context switch in and out of holding a syscall kernel lock... */	
#endif	
};

/*
 * Per process flags
 */
#define PF_ALIGNWARN	0x00000001	/* Print alignment warning msgs */
					/* Not implemented yet, only for 486*/
#define PF_PTRACED	0x00000010	/* set if ptrace (0) has been called. */
#define PF_TRACESYS	0x00000020	/* tracing system calls */

#define PF_STARTING	0x00000100	/* being created */
#define PF_EXITING	0x00000200	/* getting shut down */

#define PF_USEDFPU	0x00100000	/* Process used the FPU this quantum (SMP only) */

/*
 * Limit the stack by to some sane default: root can always
 * increase this limit if needed..  8MB seems reasonable.
 */
#define _STK_LIM	(8*1024*1024)

#define DEF_PRIORITY	(20*HZ/100)	/* 200 ms time slices */

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x1fffff (=2MB)
 */
#define INIT_TASK \
/* state etc */	{ 0,DEF_PRIORITY,DEF_PRIORITY,0,0,0,0, \
/* debugregs */ { 0, },            \
/* exec domain */&default_exec_domain, \
/* binfmt */	NULL, \
/* schedlink */	&init_task,&init_task, &init_task, &init_task, \
/* stack */	0,(unsigned long) &init_kernel_stack, \
/* ec,brk... */	0,0,0,0,0, \
/* pid etc.. */	0,0,0,0,0, \
/* suppl grps*/ {NOGROUP,}, \
/* proc links*/ &init_task,&init_task,NULL,NULL,NULL,NULL, \
/* uid etc */	0,0,0,0,0,0,0,0, \
/* timeout */	0,SCHED_OTHER,0,0,0,0,0,0,0, \
/* timer */	{ NULL, NULL, 0, 0, it_real_fn }, \
/* utime */	0,0,0,0,0, \
/* flt */	0,0,0,0,0,0, \
/* swp */	0,0,0,0,0, \
/* rlimits */   INIT_RLIMITS, \
/* math */	0, \
/* comm */	"swapper", \
/* fs info */	0,NULL, \
/* ipc */	NULL, NULL, \
/* ldt */	NULL, \
/* tss */	INIT_TSS, \
/* fs */	&init_fs, \
/* files */	&init_files, \
/* mm */	&init_mm, \
/* signals */	&init_signals, \
}

extern struct   mm_struct init_mm;
extern struct task_struct init_task;
extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current_set[NR_CPUS];
/*
 *	On a single processor system this comes out as current_set[0] when cpp
 *	has finished with it, which gcc will optimise away.
 */
#define current (0+current_set[smp_processor_id()])	/* Current on this processor */
extern unsigned long volatile jiffies;
extern unsigned long itimer_ticks;
extern unsigned long itimer_next;
extern struct timeval xtime;
extern int need_resched;
extern void do_timer(struct pt_regs *);

extern unsigned int * prof_buffer;
extern unsigned long prof_len;
extern unsigned long prof_shift;

extern int securelevel;	/* system security level */

#define CURRENT_TIME (xtime.tv_sec)

extern void sleep_on(struct wait_queue ** p);
extern void interruptible_sleep_on(struct wait_queue ** p);
extern void wake_up(struct wait_queue ** p);
extern void wake_up_interruptible(struct wait_queue ** p);
extern void wake_up_process(struct task_struct * tsk);

extern void notify_parent(struct task_struct * tsk);
extern int send_sig(unsigned long sig,struct task_struct * p,int priv);
extern int in_group_p(gid_t grp);

extern int request_irq(unsigned int irq,void (*handler)(int, struct pt_regs *),
	unsigned long flags, const char *device);
extern void free_irq(unsigned int irq);

extern void copy_thread(int, unsigned long, unsigned long, struct task_struct *, struct pt_regs *);
extern void flush_thread(void);
extern void exit_thread(void);

extern void exit_fs(struct task_struct *);
extern void exit_files(struct task_struct *);
extern void exit_sighand(struct task_struct *);
extern void release_thread(struct task_struct *);

extern int do_execve(char *, char **, char **, struct pt_regs *);
extern int do_fork(unsigned long, unsigned long, struct pt_regs *);

#ifdef MACH
extern void add_wait_queue(struct wait_queue **, struct wait_queue *);
extern void remove_wait_queue(struct wait_queue **, struct wait_queue *);
#else /* ! MACH */
/*
 * The wait-queues are circular lists, and you have to be *very* sure
 * to keep them correct. Use only these two functions to add/remove
 * entries in the queues.
 */
extern inline void add_wait_queue(struct wait_queue ** p, struct wait_queue * wait)
{
	unsigned long flags;

#ifdef DEBUG
	if (wait->next) {
		__label__ here;
		unsigned long pc;
		pc = (unsigned long) &&here;
	      here:
		printk("add_wait_queue (%08lx): wait->next = %08lx\n",pc,(unsigned long) wait->next);
	}
#endif
	save_flags(flags);
	cli();
	if (!*p) {
		wait->next = wait;
		*p = wait;
	} else {
		wait->next = (*p)->next;
		(*p)->next = wait;
	}
	restore_flags(flags);
}

extern inline void remove_wait_queue(struct wait_queue ** p, struct wait_queue * wait)
{
	unsigned long flags;
	struct wait_queue * tmp;
#ifdef DEBUG
	unsigned long ok = 0;
#endif

	save_flags(flags);
	cli();
	if ((*p == wait) &&
#ifdef DEBUG
	    (ok = 1) &&
#endif
	    ((*p = wait->next) == wait)) {
		*p = NULL;
	} else {
		tmp = wait;
		while (tmp->next != wait) {
			tmp = tmp->next;
#ifdef DEBUG
			if (tmp == *p)
				ok = 1;
#endif
		}
		tmp->next = wait->next;
	}
	wait->next = NULL;
	restore_flags(flags);
#ifdef DEBUG
	if (!ok) {
		__label__ here;
		ok = (unsigned long) &&here;
		printk("removed wait_queue not on list.\n");
		printk("list = %08lx, queue = %08lx\n",(unsigned long) p, (unsigned long) wait);
	      here:
		printk("eip = %08lx\n",ok);
	}
#endif
}

extern inline void select_wait(struct wait_queue ** wait_address, select_table * p)
{
	struct select_table_entry * entry;

	if (!p || !wait_address)
		return;
	if (p->nr >= __MAX_SELECT_TABLE_ENTRIES)
		return;
 	entry = p->entry + p->nr;
	entry->wait_address = wait_address;
	entry->wait.task = current;
	entry->wait.next = NULL;
	add_wait_queue(wait_address,&entry->wait);
	p->nr++;
}
#endif /* ! MACH */

extern void __down(struct semaphore * sem);

/*
 * These are not yet interrupt-safe
 */
extern inline void down(struct semaphore * sem)
{
	if (sem->count <= 0)
		__down(sem);
	sem->count--;
}

extern inline void up(struct semaphore * sem)
{
	sem->count++;
	wake_up(&sem->wait);
}	

#define REMOVE_LINKS(p) do { unsigned long flags; \
	save_flags(flags) ; cli(); \
	(p)->next_task->prev_task = (p)->prev_task; \
	(p)->prev_task->next_task = (p)->next_task; \
	restore_flags(flags); \
	if ((p)->p_osptr) \
		(p)->p_osptr->p_ysptr = (p)->p_ysptr; \
	if ((p)->p_ysptr) \
		(p)->p_ysptr->p_osptr = (p)->p_osptr; \
	else \
		(p)->p_pptr->p_cptr = (p)->p_osptr; \
	} while (0)

#define SET_LINKS(p) do { unsigned long flags; \
	save_flags(flags); cli(); \
	(p)->next_task = &init_task; \
	(p)->prev_task = init_task.prev_task; \
	init_task.prev_task->next_task = (p); \
	init_task.prev_task = (p); \
	restore_flags(flags); \
	(p)->p_ysptr = NULL; \
	if (((p)->p_osptr = (p)->p_pptr->p_cptr) != NULL) \
		(p)->p_osptr->p_ysptr = p; \
	(p)->p_pptr->p_cptr = p; \
	} while (0)

#define for_each_task(p) \
	for (p = &init_task ; (p = p->next_task) != &init_task ; )

#endif /* __KERNEL__ */

#endif
