/*
 * portio.h	Definitions of routines for detecting, reserving and
 *		allocating system resources.
 *
 * Version:	0.01	8/30/93
 *
 * Author:	Donald Becker (becker@super.org)
 */

#ifndef _LINUX_PORTIO_H
#define _LINUX_PORTIO_H

#define HAVE_PORTRESERVE
/*
 * Call check_region() before probing for your hardware.
 * Once you have found you hardware, register it with request_region().
 * If you unload the driver, use release_region to free ports.
 */
extern void reserve_setup(char *str, int *ints);
extern int check_region(unsigned int from, unsigned int extent);
extern void request_region(unsigned int from, unsigned int extent,const char *name);
extern void release_region(unsigned int from, unsigned int extent);
extern int get_ioport_list(char *);


#define HAVE_AUTOIRQ
extern void *irq2dev_map[16];		/* Use only if you own the IRQ. */
extern int autoirq_setup(int waittime);
extern int autoirq_report(int waittime);

#endif	/* _LINUX_PORTIO_H */
