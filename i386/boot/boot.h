#ifndef _boot_h_
#define _boot_h_

#include <mach/machine/multiboot.h>

extern void *boot_kern_image;
extern struct multiboot_header boot_kern_hdr;

extern struct multiboot_info *boot_info;

/* malloc-related helper functions for boot code to use.  */
void malloc_reserve(vm_offset_t rstart, vm_offset_t rend);
void *mustmalloc(vm_size_t size);
void *mustcalloc(vm_size_t size);

#endif _boot_h_
