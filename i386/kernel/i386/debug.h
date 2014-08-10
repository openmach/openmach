/* 
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _I386_DEBUG_
#define _I386_DEBUG_


#ifdef DEBUG


/* Maximum number of entries in a debug trace.
   If the buffer overflows, the oldest entries are forgotten.  */
#define DEBUG_TRACE_LEN 512

/* Add the caller's current position to the debug trace buffer.
   Only the kernel stack needs to be valid;
   the other data segment registers are not needed
   and all registers are saved.  */
#ifndef ASSEMBLER

/* Dump a saved state.
   Probably a good idea to have this around
   even when DEBUG isn't turned on.  */
void dump_ss(struct i386_saved_state *st);

#define DEBUG_TRACE _debug_trace(__FILE__,__LINE__)

/* Reset the debug trace buffer so it contains no valid entries.  */
void debug_trace_reset(void);

/* Dump the contents of the trace buffer to the console.
   Also clears the trace buffer.  */
void debug_trace_dump(void);

#else	ASSEMBLER

#define DEBUG_TRACE				\
	pushl	$__LINE__			;\
	pushl	$9f				;\
	call	__debug_trace			;\
	addl	$8,%esp				;\
	.data					;\
9:	.ascii	__FILE__"\0"			;\
	.text

#endif ASSEMBLER


#endif DEBUG

/* XXX #include_next "debug.h" */

#endif _I386_DEBUG_
