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
#ifndef _i386_kernel_dos_dos_io_h_
#define _i386_kernel_dos_dos_io_h_

#include <mach/machine/vm_types.h>

#include "real.h"
#include "debug.h"

struct stat;
struct timeval;
struct timezone;
struct termios;

typedef int dos_fd_t;

/* Maximum number of bytes we can read or write with one DOS call
   to or from memory not in the low 1MB accessible to DOS.
   Must be less than 64KB.
   Try to keep this size on a sector (512-byte) boundary for performance.  */
#ifndef DOS_BUF_SIZE
#define DOS_BUF_SIZE 0x1000
#endif

/* If DOS_BUF_DYNAMIC is set, then dos_buf is a pointer
   which must be provided and initialized by calling code.
   Otherwise, the dos_buf is a statically-allocated bss array.  */
#ifdef DOS_BUF_DYNAMIC
extern char *dos_buf;
#else
extern char dos_buf[DOS_BUF_SIZE];
#endif

int dos_check_err(struct real_call_data *rcd);

int dos_open(const char *s, int flags, int create_mode, dos_fd_t *out_fh);
int dos_close(dos_fd_t fd);
int dos_read(dos_fd_t fd, void *buf, vm_size_t size, vm_size_t *out_actual);
int dos_write(dos_fd_t fd, const void *buf, vm_size_t size, vm_size_t *out_actual);
int dos_seek(dos_fd_t fd, vm_offset_t offset, int whence, vm_offset_t *out_newpos);
int dos_fstat(dos_fd_t fd, struct stat *st);
int dos_tcgetattr(dos_fd_t fd, struct termios *t);
int dos_rename(const char *oldpath, const char *newpath);
int dos_unlink(const char *filename);

int dos_gettimeofday(struct timeval *tv, struct timezone *tz);

#define dos_init_rcd(rcd) real_call_data_init(rcd)

#define real_set_ds_dx(ptr)				\
	({ unsigned ofs = (unsigned)(ptr);		\
	   assert(ofs < 0x10000);			\
	   real_call_data.ds = real_cs;			\
	   real_call_data.edx = ofs;			\
	})
#define real_set_es_di(ptr)				\
	({ unsigned ofs = (unsigned)(ptr);		\
	   assert(ofs < 0x10000);			\
	   real_call_data.es = real_cs;			\
	   real_call_data.edi = ofs;			\
	})


#endif _i386_kernel_dos_dos_io_h_
