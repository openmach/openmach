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

void i16_exit(int rc)
{
	/* Clean up properly.  */
	i16_ext_mem_shutdown();
	i16_xms_shutdown();
#ifdef ENABLE_VCPI
	i16_vcpi_shutdown();
#endif
#ifdef ENABLE_DPMI
	i16_dpmi_shutdown();
#endif
#ifdef ENABLE_CODE_CHECK
	i16_code_check();
#endif

	/* Call the DOS exit function.  */
	i16_dos_exit(rc);
}

