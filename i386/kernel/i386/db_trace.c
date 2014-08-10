/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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

#include "mach_kdb.h"
#if MACH_KDB

#include <mach/boolean.h>
#include <vm/vm_map.h>
#include <kern/thread.h>
#include <kern/task.h>

#include <machine/db_machdep.h>
#include <machine/machspl.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <ddb/db_task_thread.h>

#include "trap.h"

db_i386_reg_value(
	struct db_variable	*vp,
	db_expr_t		*valuep,
	int			flag,
	struct db_var_aux_param	*ap);		/* forward */

/*
 * Machine register set.
 */
struct db_variable db_regs[] = {
	{ "cs",	(int *)&ddb_regs.cs,  db_i386_reg_value },
	{ "ds",	(int *)&ddb_regs.ds,  db_i386_reg_value },
	{ "es",	(int *)&ddb_regs.es,  db_i386_reg_value },
	{ "fs",	(int *)&ddb_regs.fs,  db_i386_reg_value },
	{ "gs",	(int *)&ddb_regs.gs,  db_i386_reg_value },
	{ "ss",	(int *)&ddb_regs.ss,  db_i386_reg_value },
	{ "eax",(int *)&ddb_regs.eax, db_i386_reg_value },
	{ "ecx",(int *)&ddb_regs.ecx, db_i386_reg_value },
	{ "edx",(int *)&ddb_regs.edx, db_i386_reg_value },
	{ "ebx",(int *)&ddb_regs.ebx, db_i386_reg_value },
	{ "esp",(int *)&ddb_regs.uesp,db_i386_reg_value },
	{ "ebp",(int *)&ddb_regs.ebp, db_i386_reg_value },
	{ "esi",(int *)&ddb_regs.esi, db_i386_reg_value },
	{ "edi",(int *)&ddb_regs.edi, db_i386_reg_value },
	{ "eip",(int *)&ddb_regs.eip, db_i386_reg_value },
	{ "efl",(int *)&ddb_regs.efl, db_i386_reg_value },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Stack trace.
 */
#define	INKERNEL(va)	(((vm_offset_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

struct i386_frame {
	struct i386_frame	*f_frame;
	int			f_retaddr;
	int			f_arg0;
};

#define	TRAP		1
#define	INTERRUPT	2
#define SYSCALL		3

db_addr_t	db_user_trap_symbol_value = 0;
db_addr_t	db_kernel_trap_symbol_value = 0;
db_addr_t	db_interrupt_symbol_value = 0;
db_addr_t	db_return_to_iret_symbol_value = 0;
db_addr_t	db_syscall_symbol_value = 0;
boolean_t	db_trace_symbols_found = FALSE;

struct i386_kregs {
	char	*name;
	int	offset;
} i386_kregs[] = {
	{ "ebx", (int)(&((struct i386_kernel_state *)0)->k_ebx) },
	{ "esp", (int)(&((struct i386_kernel_state *)0)->k_esp) },
	{ "ebp", (int)(&((struct i386_kernel_state *)0)->k_ebp) },
	{ "edi", (int)(&((struct i386_kernel_state *)0)->k_edi) },
	{ "esi", (int)(&((struct i386_kernel_state *)0)->k_esi) },
	{ "eip", (int)(&((struct i386_kernel_state *)0)->k_eip) },
	{ 0 },
};

int *
db_lookup_i386_kreg(
	char	*name,
	int	*kregp)
{
	register struct i386_kregs *kp;

	for (kp = i386_kregs; kp->name; kp++) {
	    if (strcmp(name, kp->name) == 0)
		return (int *)((int)kregp + kp->offset);
	}
	return 0;
}
	
db_i386_reg_value(
	struct	db_variable	*vp,
	db_expr_t		*valuep,
	int			flag,
	db_var_aux_param_t	ap)
{
	int			*dp = 0;
	db_expr_t		null_reg = 0;
	register thread_t	thread = ap->thread;
	extern unsigned		int_stack_high;

	if (db_option(ap->modif, 'u')) {
	    if (thread == THREAD_NULL) {
		if ((thread = current_thread()) == THREAD_NULL)
		    db_error("no user registers\n");
	    }
	    if (thread == current_thread()) {
		if (ddb_regs.cs & 0x3)
		    dp = vp->valuep;
		else if (ddb_regs.ebp < int_stack_high)
		    db_error("cannot get/set user registers in nested interrupt\n");
	    }
	} else {
	    if (thread == THREAD_NULL || thread == current_thread()) {
		dp = vp->valuep;
	    } else if ((thread->state & TH_SWAPPED) == 0 && 
			thread->kernel_stack) {
		dp = db_lookup_i386_kreg(vp->name,
				(int *)(STACK_IKS(thread->kernel_stack)));
		if (dp == 0)
		    dp = &null_reg;
	    } else if ((thread->state & TH_SWAPPED) &&
			thread->swap_func != thread_exception_return) {
/*.....this breaks t/t $taskN.0...*/
		/* only EIP is valid */
		if (vp->valuep == (int *) &ddb_regs.eip) {
		    dp = (int *)(&thread->swap_func);
		} else {
		    dp = &null_reg;
		}
	    }
	}
	if (dp == 0) {
	    if (thread->pcb == 0)
		db_error("no pcb\n");
	    dp = (int *)((int)(&thread->pcb->iss) + 
		 		    ((int)vp->valuep - (int)&ddb_regs));
	}
	if (flag == DB_VAR_SET)
	    *dp = *valuep;
	else
	    *valuep = *dp;
}

void
db_find_trace_symbols(void)
{
	db_expr_t	value;
	if (db_value_of_name("_user_trap", &value))
	    db_user_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_kernel_trap", &value))
	    db_kernel_trap_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_interrupt", &value))
	    db_interrupt_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_return_to_iret", &value))
	    db_return_to_iret_symbol_value = (db_addr_t) value;
	if (db_value_of_name("_syscall", &value))
	    db_syscall_symbol_value = (db_addr_t) value;
	db_trace_symbols_found = TRUE;
}

/*
 * Figure out how many arguments were passed into the frame at "fp".
 */
int db_numargs_default = 5;

int
db_numargs(
	struct i386_frame *fp,
	task_t task)
{
	int	*argp;
	int	inst;
	int	args;
	extern char	etext[];

	argp = (int *)db_get_task_value((int)&fp->f_retaddr, 4, FALSE, task);
	if (argp < (int *)VM_MIN_KERNEL_ADDRESS || argp > (int *)etext)
	    args = db_numargs_default;
	else if (!DB_CHECK_ACCESS((int)argp, 4, task))
	    args = db_numargs_default;
	else {
	    inst = db_get_task_value((int)argp, 4, FALSE, task);
	    if ((inst & 0xff) == 0x59)	/* popl %ecx */
		args = 1;
	    else if ((inst & 0xffff) == 0xc483)	/* addl %n, %esp */
		args = ((inst >> 16) & 0xff) / 4;
	    else
		args = db_numargs_default;
	}
	return args;
}

struct interrupt_frame {
	struct i386_frame *if_frame;	/* point to next frame */
	int		  if_retaddr;	/* return address to _interrupt */
	int		  if_unit;	/* unit number */
	spl_t		  if_spl;	/* saved spl */
	int		  if_iretaddr;	/* _return_to_{iret,iret_i} */
	int		  if_edx;	/* old sp(iret) or saved edx(iret_i) */
	int		  if_ecx;	/* saved ecx(iret_i) */
	int		  if_eax;	/* saved eax(iret_i) */
	int		  if_eip;	/* saved eip(iret_i) */
	int		  if_cs;	/* saved cs(iret_i) */
	int		  if_efl;	/* saved efl(iret_i) */
};

/* 
 * Figure out the next frame up in the call stack.  
 * For trap(), we print the address of the faulting instruction and 
 *   proceed with the calling frame.  We return the ip that faulted.
 *   If the trap was caused by jumping through a bogus pointer, then
 *   the next line in the backtrace will list some random function as 
 *   being called.  It should get the argument list correct, though.  
 *   It might be possible to dig out from the next frame up the name
 *   of the function that faulted, but that could get hairy.
 */
void
db_nextframe(
	struct i386_frame **lfp,	/* in/out */
	struct i386_frame **fp,		/* in/out */
	db_addr_t	  *ip,		/* out */
	int 		  frame_type,	/* in */
	thread_t	  thread)	/* in */
{
	struct i386_saved_state *saved_regs;
	struct interrupt_frame *ifp;
	task_t task = (thread != THREAD_NULL)? thread->task: TASK_NULL;

	switch(frame_type) {
	case TRAP:
	    /*
	     * We know that trap() has 1 argument and we know that
	     * it is an (struct i386_saved_state *).
	     */
	    saved_regs = (struct i386_saved_state *)
			db_get_task_value((int)&((*fp)->f_arg0),4,FALSE,task);
	    db_printf(">>>>> %s (%d) at ",
			trap_name(saved_regs->trapno), saved_regs->trapno);
	    db_task_printsym(saved_regs->eip, DB_STGY_PROC, task);
	    db_printf(" <<<<<\n");
	    *fp = (struct i386_frame *)saved_regs->ebp;
	    *ip = (db_addr_t)saved_regs->eip;
	    break;
	case INTERRUPT:
	    if (*lfp == 0) {
		db_printf(">>>>> interrupt <<<<<\n");
		goto miss_frame;
	    }
	    db_printf(">>>>> interrupt at "); 
	    ifp = (struct interrupt_frame *)(*lfp);
	    *fp = ifp->if_frame;
	    if (ifp->if_iretaddr == db_return_to_iret_symbol_value)
		*ip = ((struct i386_interrupt_state *) ifp->if_edx)->eip;
	    else
		*ip = (db_addr_t) ifp->if_eip;
	    db_task_printsym(*ip, DB_STGY_PROC, task);
	    db_printf(" <<<<<\n");
	    break;
	case SYSCALL:
	    if (thread != THREAD_NULL && thread->pcb) {
		*ip = (db_addr_t) thread->pcb->iss.eip;
		*fp = (struct i386_frame *) thread->pcb->iss.ebp;
		break;
	    }
	    /* falling down for unknown case */
	default:
	miss_frame:
	    *ip = (db_addr_t)
		db_get_task_value((int)&(*fp)->f_retaddr, 4, FALSE, task);
	    *lfp = *fp;
	    *fp = (struct i386_frame *)
		db_get_task_value((int)&(*fp)->f_frame, 4, FALSE, task);
	    break;
	}
}

void
db_i386_stack_trace(
	thread_t	th,
	struct i386_frame *frame,
	db_addr_t	callpc,
	db_expr_t	count,
	int		flags);		/* forward */

#define	F_USER_TRACE	1
#define F_TRACE_THREAD	2

void
db_stack_trace_cmd(
	db_expr_t	addr,
	boolean_t	have_addr,
	db_expr_t	count,
	char		*modif)
{
	boolean_t	trace_thread = FALSE;
	struct i386_frame *frame;
	db_addr_t	callpc;
	int		flags = 0;
	thread_t	th;

	{
	    register char *cp = modif;
	    register char c;

	    while ((c = *cp++) != 0) {
		if (c == 't')
		    trace_thread = TRUE;
		if (c == 'u')
		    flags |= F_USER_TRACE;
	    }
	}

	if (!have_addr && !trace_thread) {
	    frame = (struct i386_frame *)ddb_regs.ebp;
	    callpc = (db_addr_t)ddb_regs.eip;
	    th = current_thread();
	} else if (trace_thread) {
	    if (have_addr) {
		th = (thread_t) addr;
		if (!db_check_thread_address_valid((db_addr_t)th))
		    return;
	    } else {
		th = db_default_thread;
		if (th == THREAD_NULL)
		   th = current_thread();
		if (th == THREAD_NULL) {
		   db_printf("no active thread\n");
		   return;
		}
	    }
	    if (th == current_thread()) {
	        frame = (struct i386_frame *)ddb_regs.ebp;
	        callpc = (db_addr_t)ddb_regs.eip;
	    } else {
		if (th->pcb == 0) {
		    db_printf("thread has no pcb\n");
		    return;
		}
		if ((th->state & TH_SWAPPED) || th->kernel_stack == 0) {
		    register struct i386_saved_state *iss = &th->pcb->iss;

		    db_printf("Continuation ");
		    db_task_printsym((db_expr_t)th->swap_func,
				      DB_STGY_PROC,
				      th->task);
		    db_printf("\n");

		    frame = (struct i386_frame *) (iss->ebp);
		    callpc = (db_addr_t) (iss->eip);
		} else {
		    register struct i386_kernel_state *iks;
		    iks = STACK_IKS(th->kernel_stack);
		    frame = (struct i386_frame *) (iks->k_ebp);
		    callpc = (db_addr_t) (iks->k_eip);
	        }
	    }
	} else {
	    frame = (struct i386_frame *)addr;
	    th = (db_default_thread)? db_default_thread: current_thread();
	    callpc = (db_addr_t)db_get_task_value((int)&frame->f_retaddr, 4, 
						  FALSE,
						  (th == THREAD_NULL) ? TASK_NULL : th->task);
	}

	db_i386_stack_trace( th, frame, callpc, count, flags );
}


void
db_i386_stack_trace(
	thread_t	th,
	struct i386_frame *frame,
	db_addr_t	callpc,
	db_expr_t	count,
	int		flags)
{
	task_t		task;
	boolean_t	kernel_only;
	int		*argp;
	int		user_frame = 0;
	struct i386_frame *lastframe;
	int		frame_type;
	char		*filename;
	int		linenum;
	extern unsigned	int db_maxoff;

	if (count == -1)
	    count = 65535;

	kernel_only = (flags & F_USER_TRACE) == 0;

	task = (th == THREAD_NULL) ? TASK_NULL : th->task;

	if (!db_trace_symbols_found)
	    db_find_trace_symbols();

	if (!INKERNEL((unsigned)callpc) && !INKERNEL((unsigned)frame)) {
	    db_printf(">>>>> user space <<<<<\n");
	    user_frame++;
	}

	lastframe = 0;
	while (count-- && frame != 0) {
	    register int narg;
	    char *	name;
	    db_expr_t	offset;

	    if (INKERNEL((unsigned)callpc) && user_frame == 0) {
		db_addr_t call_func = 0;

		db_symbol_values(0, db_search_task_symbol(callpc, 
				 	DB_STGY_XTRN, (db_addr_t *)&offset,
					TASK_NULL),
				 &name, (db_expr_t *)&call_func);
		if (call_func == db_user_trap_symbol_value ||
		    call_func == db_kernel_trap_symbol_value) {
		    frame_type = TRAP;
		    narg = 1;
		} else if (call_func == db_interrupt_symbol_value) {
		    frame_type = INTERRUPT;
		    goto next_frame;
		} else if (call_func == db_syscall_symbol_value) {
		    frame_type = SYSCALL;
		    goto next_frame;
		} else {
		    frame_type = 0;
		    narg = db_numargs(frame, task);
		}
	    } else if (INKERNEL((unsigned)callpc) ^ INKERNEL((unsigned)frame)) {
		frame_type = 0;
		narg = -1;
	    } else {
		frame_type = 0;
		narg = db_numargs(frame, task);
	    }

	    db_find_task_sym_and_offset(callpc, &name,
					(db_addr_t *)&offset, task);
	    if (name == 0 || offset > db_maxoff) {
		db_printf("0x%x(", callpc);
		offset = 0;
	    } else
	        db_printf("%s(", name);

	    argp = &frame->f_arg0;
	    while (narg > 0) {
		db_printf("%x", db_get_task_value((int)argp,4,FALSE,task));
		argp++;
		if (--narg != 0)
		    db_printf(",");
	    }
	    if (narg < 0)
		db_printf("...");
	    db_printf(")");
	    if (offset) {
		db_printf("+%x", offset);
            }
	    if (db_line_at_pc(0, &filename, &linenum, callpc)) {
		db_printf(" [%s", filename);
		if (linenum > 0)
		    db_printf(":%d", linenum);
		db_printf("]");
	    }
	    db_printf("\n");

	next_frame:
	    db_nextframe(&lastframe, &frame, &callpc, frame_type, th);

	    if (frame == 0) {
		/* end of chain */
		break;
	    }
	    if (!INKERNEL(lastframe) ||
		(!INKERNEL((unsigned)callpc) && !INKERNEL((unsigned)frame)))
		user_frame++;
	    if (user_frame == 1) {
		db_printf(">>>>> user space <<<<<\n");
		if (kernel_only)
		    break;
	    }
	    if (frame <= lastframe) {
		if (INKERNEL(lastframe) && !INKERNEL(frame))
		    continue;
		db_printf("Bad frame pointer: 0x%x\n", frame);
		break;
	    }
	}
}

#define	CTHREADS_SUPPORT	1

#if	CTHREADS_SUPPORT

thread_t
db_find_kthread(
	vm_offset_t	ustack_base,
	vm_size_t	ustack_top,
	task_t		task)
{
	thread_t thread;

	queue_iterate(&task->thread_list, thread, thread_t, thread_list) {
		vm_offset_t	usp = thread->pcb->iss.uesp/*ebp works*/;
		if (usp >= ustack_base && usp < ustack_top)
			return thread;
	}
	return THREAD_NULL;
}

static void db_cproc_state(
	int	state,
	char	s[4])
{
	if (state == 0) {
		*s++ = 'R';
	} else {
		if (state & 1) *s++ = 'S';
		if (state & 2) *s++ = 'B';
		if (state & 4) *s++ = 'C';
	}
	*s = 0;
}

/* offsets in a cproc structure */
int db_cproc_next_offset = 0 * 4;
int db_cproc_incarnation_offset = 1 * 4;
int db_cproc_list_offset = 2 * 4;
int db_cproc_wait_offset = 3 * 4;
int db_cproc_context_offset = 5 * 4;
int db_cproc_state_offset = 7 * 4;
int db_cproc_stack_base_offset = 10 * 4 + sizeof(mach_msg_header_t);
int db_cproc_stack_size_offset = 11 * 4 + sizeof(mach_msg_header_t);

/* offsets in a cproc_switch context structure */
int db_cprocsw_framep_offset = 3 * 4;
int db_cprocsw_pc_offset = 4 * 4;

#include <machine/setjmp.h>

extern jmp_buf_t *db_recover;

void db_trace_cproc(
	vm_offset_t	cproc,
	thread_t	thread)
{
	jmp_buf_t	db_jmpbuf;
	jmp_buf_t	*prev = db_recover;
	task_t		task;
	db_addr_t	pc, fp;

	task = (thread == THREAD_NULL)? TASK_NULL: thread->task;

	if (!_setjmp(db_recover = &db_jmpbuf)) {
		char pstate[4];
		unsigned int s, w, n, c, cth;

		s = db_get_task_value(cproc + db_cproc_state_offset, 4, FALSE, task);
		w = db_get_task_value(cproc + db_cproc_wait_offset, 4, FALSE, task);
		n = db_get_task_value(cproc + db_cproc_next_offset, 4, FALSE, task);
		c = db_get_task_value(cproc + db_cproc_context_offset, 4, FALSE, task);
		cth = db_get_task_value(cproc + db_cproc_incarnation_offset, 4, FALSE, task);

		db_cproc_state(s, pstate);

		db_printf("CThread %x (cproc %x) %s", cth, cproc, pstate);
		if (w) db_printf(" awaits %x", w);
		if (n) db_printf(" next %x", n);
		db_printf("\n");

		if ((s != 0) && (c != 0)) {
			pc = db_get_task_value(c + db_cprocsw_pc_offset, 4, FALSE, task);
			fp = c + db_cprocsw_framep_offset;
		} else {
			db_addr_t sb;
			vm_size_t ss;

			sb = db_get_task_value(cproc + db_cproc_stack_base_offset, sizeof(db_expr_t), FALSE, task);
			ss = db_get_task_value(cproc + db_cproc_stack_size_offset, sizeof(db_expr_t), FALSE, task);
			db_printf(" Stack base: %x\n", sb);
			/*
			 *  Lessee now..
			 */
			thread = db_find_kthread(sb, sb+ss, task);
			if (thread != THREAD_NULL) {
			    pc = thread->pcb->iss.eip;
			    fp = thread->pcb->iss.ebp;
			} else
			    fp = -1;
		}

		if (fp != -1)
			db_i386_stack_trace(thread, (struct i386_frame*)fp, pc,
						-1, F_USER_TRACE);
	}

	db_recover = prev;
}

void db_all_cprocs(
	task_t		task,
	db_expr_t	cproc_list)
{
	jmp_buf_t	db_jmpbuf;
	jmp_buf_t 	*prev = db_recover;
	thread_t	thread;
	db_expr_t	cproc, next;


	if (task != TASK_NULL) {
		thread = (thread_t) queue_first(&task->thread_list);
	} else
		thread = current_thread();

	if (cproc_list != 0)
		next = cproc_list;
	else
		if (!db_value_of_name("unix::cproc_list", &next)) {
			db_printf("No cprocs.\n");
			return;
		}


	while (next) {
		if (_setjmp(db_recover = &db_jmpbuf))
			break;

		cproc = db_get_task_value(next, 4, FALSE, TASK_NULL);
		if (cproc == 0) break;
		next = cproc + db_cproc_list_offset;

		db_trace_cproc(cproc, thread);
	}

	db_recover = prev;
}

#endif	/* CTHREADS_SUPPORT */

#endif MACH_KDB
