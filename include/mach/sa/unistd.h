#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

__DECL(int,close(int fd));
__DECL(int,read(int fd, void *buf, unsigned int n));
__DECL(int,write(int fd, const void *buf, unsigned int n));
__DECL(off_t,lseek(int fd, off_t offset, int whence));
__DECL(int,rename(const char *oldpath, const char *newpath));
__DECL(void *,sbrk(int size));

__END_DECLS

#endif /* _UNISTD_H_ */
