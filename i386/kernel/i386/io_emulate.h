/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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

#ifndef	_I386_IO_EMULATE_H_
#define	_I386_IO_EMULATE_H_

/*
 * Return codes from IO emulation.
 */
extern int	emulate_io(/*
			struct i386_saved_state *regs,
			int	opcode,
			int	io_port
			   */);

#define	EM_IO_DONE	0	/* IO instruction executed, proceed */
#define	EM_IO_RETRY	1	/* IO port mapped, retry instruction */
#define	EM_IO_ERROR	2	/* IO port not mapped */

#endif	/* _I386_IO_EMULATE_H_ */
