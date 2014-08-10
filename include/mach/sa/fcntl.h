#ifndef _MACH_SA_FCNTL_H_
#define _MACH_SA_FCNTL_H_

#include <sys/cdefs.h>

#define O_ACCMODE	0x0003
#define O_RDONLY	0x0000
#define O_WRONLY	0x0001
#define O_RDWR		0x0002

#define O_CREAT		0x0010
#define O_TRUNC		0x0020
#define O_APPEND	0x0040
#define O_EXCL		0x0080

__BEGIN_DECLS

int open(const char *__name, int __mode, ...);

__END_DECLS

#endif /* _MACH_SA_FCNTL_H_ */
