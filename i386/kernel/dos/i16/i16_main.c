/* 
 * Copyright (c) 1995-1994 The University of Utah and
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

#include <mach/machine/code16.h>

#include "config.h"
#include "i16_dos.h"


CODE16

int argc;
char **argv;

void i16_main(int _argc, char **_argv)
{
	argc = _argc;
	argv = _argv;

	i16_init();

	/* Make sure we're running on a good enough DOS version.  */
	if (i16_dos_version() < 0x300)
		i16_die("DOS 3.00 or higher required.");

	/* See if we're running in a DPMI or VCPI environment.
	   If either of these are successful, they don't return.  */
	i16_dos_mem_check();
#ifdef ENABLE_DPMI
	i16_dpmi_check();
#endif
	i16_xms_check();
	i16_ext_mem_check();
#ifdef ENABLE_VCPI
	i16_vcpi_check();
#endif

	i16_raw_start();
}

