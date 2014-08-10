#ifndef _LINUX_KERNEL_STAT_H
#define _LINUX_KERNEL_STAT_H

#include <asm/irq.h>

/*
 * 'kernel_stat.h' contains the definitions needed for doing
 * some kernel statistics (cpu usage, context switches ...),
 * used by rstatd/perfmeter
 */

#define DK_NDRIVE 4

struct kernel_stat {
	unsigned int cpu_user, cpu_nice, cpu_system;
	unsigned int dk_drive[DK_NDRIVE];
	unsigned int dk_drive_rio[DK_NDRIVE];
	unsigned int dk_drive_wio[DK_NDRIVE];
	unsigned int dk_drive_rblk[DK_NDRIVE];
	unsigned int dk_drive_wblk[DK_NDRIVE];
	unsigned int pgpgin, pgpgout;
	unsigned int pswpin, pswpout;
	unsigned int interrupts[NR_IRQS];
	unsigned int ipackets, opackets;
	unsigned int ierrors, oerrors;
	unsigned int collisions;
	unsigned int context_swtch;
};

extern struct kernel_stat kstat;

#endif /* _LINUX_KERNEL_STAT_H */
