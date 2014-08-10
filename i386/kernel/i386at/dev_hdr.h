/*
 * Mach device definitions (i386at version).
 *
 * Copyright (c) 1996 The University of Utah and
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
 *      Author: Shantanu Goel, University of Utah CSL
 */

#ifndef _I386AT_DEV_HDR_H_
#define _I386AT_DEV_HDR_H_

struct device_emulation_ops;

/* This structure is associated with each open device port.
   The port representing the device points to this structure.  */
struct device
{
  struct device_emulation_ops *emul_ops;
  void *emul_data;
};

typedef struct device *device_t;

#define DEVICE_NULL	((device_t) 0)

#endif /* _I386AT_DEV_HDR_H_ */
