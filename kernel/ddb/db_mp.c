/* 
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
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

#include <cpus.h>

#if	NCPUS > 1

#include <mach/boolean.h>
#include <mach/machine.h>

#include <kern/cpu_number.h>
#include <kern/lock.h>

#include <machine/db_machdep.h>

#include <ddb/db_command.h>
#include <ddb/db_run.h>

/*
 * Routines to interlock access to the kernel debugger on
 * multiprocessors.
 */

decl_simple_lock_data(,db_lock)		/* lock to enter debugger */
volatile int	db_cpu = -1;		/* CPU currently in debugger */
					/* -1 if none */
int	db_active[NCPUS] = { 0 };	/* count recursive entries
					   into debugger */
int	db_slave[NCPUS] = { 0 };	/* nonzero if cpu interrupted
					   by another cpu in debugger */

int	db_enter_debug = 0;

void	remote_db();		/* forward */
void	lock_db();
void	unlock_db();


/*
 * Called when entering kernel debugger.
 * Takes db lock. If we were called remotely (slave state) we just
 * wait for db_cpu to be equal to cpu_number(). Otherwise enter debugger
 * if not active on another cpu
 */

boolean_t
db_enter()
{
	int	mycpu = cpu_number();

	/*
	 * Count recursive entries to debugger.
	 */
	db_active[mycpu]++;

	/*
	 * Wait for other CPUS to leave debugger.
	 */
	lock_db();

	if (db_enter_debug)
	    db_printf(
		"db_enter: cpu %d[%d], master %d, db_cpu %d, run mode %d\n",
		mycpu, db_slave[mycpu], master_cpu, db_cpu, db_run_mode);

	/*
	 * If no CPU in debugger, and I am not being stopped,
	 * enter the debugger.
	 */
	if (db_cpu == -1 && !db_slave[mycpu]) {
	    remote_db();	/* stop other cpus */
	    db_cpu = mycpu;
	    return TRUE;
	}
	/*
	 * If I am already in the debugger (recursive entry
	 * or returning from single step), enter debugger.
	 */
	else if (db_cpu == mycpu)
	    return TRUE;
	/*
	 * Otherwise, cannot enter debugger.
	 */
	else
	    return FALSE;
}

/*
 * Leave debugger.
 */
void
db_leave()
{
	int	mycpu = cpu_number();

	/*
	 * If continuing, give up debugger
	 */
	if (db_run_mode == STEP_CONTINUE)
	    db_cpu = -1;

	/*
	 * If I am a slave, drop my slave count.
	 */
	if (db_slave[mycpu])
	    db_slave[mycpu]--;
	if (db_enter_debug)
	    db_printf("db_leave: cpu %d[%d], db_cpu %d, run_mode %d\n",
		      mycpu, db_slave[mycpu], db_cpu, db_run_mode);
	/*
	 * Unlock debugger.
	 */
	unlock_db();

	/*
	 * Drop recursive entry count.
	 */
	db_active[mycpu]--;
}


/*
 * invoke kernel debugger on slave processors 
 */

void
remote_db() {
	int	my_cpu = cpu_number();
	register int	i;

	for (i = 0; i < NCPUS; i++) {
	    if (i != my_cpu &&
		machine_slot[i].is_cpu &&
		machine_slot[i].running)
	    {
		cpu_interrupt_to_db(i);
	    }
	}
}

/*
 * Save and restore DB global registers.
 *
 * DB_SAVE_CTXT must be at the start of a block, and
 * DB_RESTORE_CTXT must be in the same block.
 */

#ifdef	__STDC__
#define DB_SAVE(type, name) extern type name; type name##_save = name
#define DB_RESTORE(name) name = name##_save
#else	/* __STDC__ */
#define DB_SAVE(type, name) extern type name; type name/**/_save = name
#define DB_RESTORE(name) name = name/**/_save
#endif	/* __STDC__ */

#define DB_SAVE_CTXT() \
	DB_SAVE(int, db_run_mode); \
	DB_SAVE(boolean_t, db_sstep_print); \
	DB_SAVE(int, db_loop_count); \
	DB_SAVE(int, db_call_depth); \
	DB_SAVE(int, db_inst_count); \
	DB_SAVE(int, db_last_inst_count); \
	DB_SAVE(int, db_load_count); \
	DB_SAVE(int, db_store_count); \
	DB_SAVE(boolean_t, db_cmd_loop_done); \
	DB_SAVE(jmp_buf_t *, db_recover); \
	DB_SAVE(db_addr_t, db_dot); \
	DB_SAVE(db_addr_t, db_last_addr); \
	DB_SAVE(db_addr_t, db_prev); \
	DB_SAVE(db_addr_t, db_next); \
	SAVE_DDB_REGS

#define DB_RESTORE_CTXT() \
	DB_RESTORE(db_run_mode); \
	DB_RESTORE(db_sstep_print); \
	DB_RESTORE(db_loop_count); \
	DB_RESTORE(db_call_depth); \
	DB_RESTORE(db_inst_count); \
	DB_RESTORE(db_last_inst_count); \
	DB_RESTORE(db_load_count); \
	DB_RESTORE(db_store_count); \
	DB_RESTORE(db_cmd_loop_done); \
	DB_RESTORE(db_recover); \
	DB_RESTORE(db_dot); \
	DB_RESTORE(db_last_addr); \
	DB_RESTORE(db_prev); \
	DB_RESTORE(db_next); \
	RESTORE_DDB_REGS

/*
 * switch to another cpu
 */
void
db_on(cpu)
	int	cpu;
{
	/*
	 * Save ddb global variables
	 */
	DB_SAVE_CTXT();

	/*
	 * Don`t do if bad CPU number.
	 * CPU must also be spinning in db_entry.
	 */
	if (cpu < 0 || cpu >= NCPUS || !db_active[cpu])
	    return;

	/*
	 * Give debugger to that CPU
	 */
	db_cpu = cpu;
	unlock_db();

	/*
	 * Wait for it to come back again
	 */
	lock_db();

	/*
	 * Restore ddb globals
	 */
	DB_RESTORE_CTXT();

	if (db_cpu == -1) /* someone continued */
	    db_continue_cmd(0, 0, 0, "");
}

/*
 * Called by interprocessor interrupt when one CPU is
 * in kernel debugger and wants to stop other CPUs
 */
void
remote_db_enter()
{
	db_slave[cpu_number()]++;
	kdb_kintr();
}

/*
 * Acquire kernel debugger.
 * Conditional code for forwarding characters from slave to console
 * if console on master only.
 */

/*
 * As long as db_cpu is not -1 or cpu_number(), we know that debugger
 * is active on another cpu.
 */
void
lock_db()
{
	int	my_cpu = cpu_number();

	for (;;) {
#if	CONSOLE_ON_MASTER
	    if (my_cpu == master_cpu) {
		db_console();
	    }
#endif
	    if (db_cpu != -1 && db_cpu != my_cpu)
		continue;

#if	CONSOLE_ON_MASTER
	    if (my_cpu == master_cpu) {
		if (!simple_lock_try(&db_lock))
		    continue;
	    }
	    else {
		simple_lock(&db_lock);
	    }
#else
	    simple_lock(&db_lock);
#endif
	    if (db_cpu == -1 || db_cpu == my_cpu)
		break;
	    simple_unlock(&db_lock);
	}
}

void
unlock_db()
{
	simple_unlock(&db_lock);
}

#ifdef sketch
void
db_console()
{
			if (i_bit(CBUS_PUT_CHAR, my_word)) {
				volatile u_char c = cbus_ochar;
				i_bit_clear(CBUS_PUT_CHAR, my_word);
				cnputc(c);
			} else if (i_bit(CBUS_GET_CHAR, my_word)) {
				if (cbus_wait_char)
					cbus_ichar = cngetc();
				else
					cbus_ichar = cnmaygetc();
				i_bit_clear(CBUS_GET_CHAR, my_word);
#ifndef	notdef
			} else if (!cnmaygetc()) {
#else	/* notdef */
			} else if (com_is_char() && !com_getc(TRUE)) {
#endif	/* notdef */
				simple_unlock(&db_lock);
				db_cpu = my_cpu;
			}
}
#endif	/* sketch */

#endif	/* NCPUS > 1 */

#endif MACH_KDB
