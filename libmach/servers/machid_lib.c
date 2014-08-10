/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * machid_lib.c,v
 * Revision 1.1.2.1  1994/10/15  23:30:10  baford
 * added from Johannes Helander's modified CMU USER collection
 *
 * Revision 2.2  92/01/22  23:10:47  rpd
 * 	Moved to lib/libmachid/.
 * 	[92/01/18            rpd]
 * 
 * Revision 2.2  91/08/29  15:42:09  rpd
 * 	Moved to libmach.
 * 
 * 	Added MACH_TYPE_DEFAULT_PAGER.
 * 	[91/08/15            rpd]
 * 
 * Revision 2.3  91/03/19  12:30:35  mrt
 * 	Changed to new copyright
 * 
 * Revision 2.2  90/09/12  16:31:46  rpd
 * 	Created.
 * 	[90/06/18            rpd]
 * 
 */

#include <servers/machid_types.h>
#include <servers/machid_lib.h>

char *
mach_type_string(type)
    mach_type_t type;
{
    switch (type) {
      case MACH_TYPE_NONE:
	return "none";
      case MACH_TYPE_TASK:
	return "task";
      case MACH_TYPE_THREAD:
	return "thread";
      case MACH_TYPE_PROCESSOR_SET:
	return "processor set";
      case MACH_TYPE_PROCESSOR_SET_NAME:
	return "processor set name";
      case MACH_TYPE_PROCESSOR:
	return "processor";
      case MACH_TYPE_HOST:
	return "host";
      case MACH_TYPE_HOST_PRIV:
	return "privileged host";
      case MACH_TYPE_OBJECT:
	return "memory object";
      case MACH_TYPE_OBJECT_CONTROL:
	return "memory object control";
      case MACH_TYPE_OBJECT_NAME:
	return "memory object name";
      case MACH_TYPE_MASTER_DEVICE:
	return "master device";
      case MACH_TYPE_DEFAULT_PAGER:
	return "default pager";
      default:
	return "unknown";
    }
}
