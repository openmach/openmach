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
/*
 *	File: scsi_jukebox.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	10/90
 *
 *	Middle layer of the SCSI driver: SCSI protocol implementation
 *
 * This file contains code for SCSI commands for MEDIA CHANGER devices.
 */

#include <scsi.h>
#if  (NSCSI > 0)

#include <mach/std_types.h>

char *scjb_name(internal)
	boolean_t	internal;
{
	return internal ? "jz" : "jukebox";
}

#if 0
scsi_exchange_medium
scsi_init_element_status
scsi_move_medium
scsi_position_to_element
scsi_read_element_status
scsi_request_volume_address
scsi_send_volume_tag
#endif
#endif  /* NSCSI > 0 */

