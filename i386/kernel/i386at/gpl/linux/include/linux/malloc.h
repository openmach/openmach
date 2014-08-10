#ifndef _LINUX_MALLOC_H
#define _LINUX_MALLOC_H

#include <linux/mm.h>

#ifndef MACH_INCLUDE
#define kmalloc	linux_kmalloc
#define kfree	linux_kfree
#endif

void *linux_kmalloc(unsigned int size, int priority);
void linux_kfree(void * obj);

#define kfree_s(a,b) linux_kfree(a)

#endif /* _LINUX_MALLOC_H */
