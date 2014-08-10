/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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

#include <mach_kdb.h>
#include <norma_ipc.h>
#include <cpus.h>

#include "cpu_number.h"
#include <kern/lock.h>
#include <sys/varargs.h>
#include <kern/thread.h>



extern void cnputc();
void Debugger();

#if	MACH_KDB
extern int db_breakpoints_inserted;
#endif

#if NCPUS>1
simple_lock_data_t Assert_print_lock; /* uninited, we take our chances */
#endif

void
Assert(char *exp, char *file, int line)
{
#if NCPUS > 1
  	simple_lock(&Assert_print_lock);
	printf("{%d} Assertion failed: file \"%s\", line %d\n", 
	       cpu_number(), file, line);
	simple_unlock(&Assert_print_lock);
#else
	printf("Assertion `%s' failed in file \"%s\", line %d\n",
		exp, file, line);
#endif

#if	MACH_KDB
	if (db_breakpoints_inserted)
#endif
	Debugger("assertion failure");
}

void Debugger(message)
	char *	message;
{
#if	!MACH_KDB
	panic("Debugger invoked, but there isn't one!");
#endif

#ifdef	lint
	message++;
#endif	/* lint */

#if	defined(vax) || defined(PC532)
	asm("bpt");
#endif	/* vax */

#ifdef	sun3
	current_thread()->pcb->flag |= TRACE_KDB;
	asm("orw  #0x00008000,sr");
#endif	/* sun3 */
#ifdef	sun4
	current_thread()->pcb->pcb_flag |= TRACE_KDB;
	asm("ta 0x81");
#endif	/* sun4 */

#if	defined(mips ) || defined(luna88k) || defined(i860) || defined(alpha)
	gimmeabreak();
#endif

#ifdef	i386
	asm("int3");
#endif
}

/* Be prepared to panic anytime,
   even before panic_init() gets called from the "normal" place in kern/startup.c.
   (panic_init() still needs to be called from there
   to make sure we get initialized before starting multiple processors.)  */
boolean_t		panic_lock_initialized = FALSE;
decl_simple_lock_data(,	panic_lock)

char			*panicstr;
int			paniccpu;

void
panic_init()
{
	if (!panic_lock_initialized)
	{
		panic_lock_initialized = TRUE;
		simple_lock_init(&panic_lock);
	}
}

/*VARARGS1*/
void
panic(s, va_alist)
	char *	s;
	va_dcl
{
	va_list	listp;
#if	NORMA_IPC
	extern int _node_self;	/* node_self() may not be callable yet */
#endif	/* NORMA_IPC */

	panic_init();

	simple_lock(&panic_lock);
	if (panicstr) {
	    if (cpu_number() != paniccpu) {
		simple_unlock(&panic_lock);
		halt_cpu();
		/* NOTREACHED */
	    }
	}
	else {
	    panicstr = s;
	    paniccpu = cpu_number();
	}
	simple_unlock(&panic_lock);
	printf("panic");
#if	NORMA_IPC
	printf("(node %U)", _node_self);
#endif
#if	NCPUS > 1
	printf("(cpu %U)", paniccpu);
#endif
	printf(": ");
	va_start(listp);
	_doprnt(s, &listp, cnputc, 0);
	va_end(listp);
	printf("\n");

#if	MACH_KDB
	Debugger("panic");
#else
	halt_cpu();
#endif
}

/*
 * We'd like to use BSD's log routines here...
 */
/*VARARGS2*/
void
log(level, fmt, va_alist)
	int	level;
	char *	fmt;
	va_dcl
{
	va_list	listp;

#ifdef lint
	level++;
#endif
	va_start(listp);
	_doprnt(fmt, &listp, cnputc, 0);
	va_end(listp);
}
