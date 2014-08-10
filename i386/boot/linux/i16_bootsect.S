
#include <linux/config.h>

#define SETUPSECS	4			/* nr of setup-sectors */
#define SYSSIZE		DEF_SYSSIZE

/* ROOT_DEV & SWAP_DEV are now written by "build". */
#define ROOT_DEV 0
#define SWAP_DEV 0
#ifndef SVGA_MODE
#define SVGA_MODE ASK_VGA
#endif
#ifndef RAMDISK
#define RAMDISK 0
#endif 
#ifndef CONFIG_ROOT_RDONLY
#define CONFIG_ROOT_RDONLY 0
#endif

	.text
	.code16
	.globl	_start
_start:
	/* XXX at least we could print a message...  */
	jmp	_start


	.org 497
setup_sects:
	.byte SETUPSECS
root_flags:
	.word CONFIG_ROOT_RDONLY
syssize:
	.word SYSSIZE
swap_dev:
	.word SWAP_DEV
ram_size:
	.word RAMDISK
vid_mode:
	.word SVGA_MODE
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55
