/*
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
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
 *      Utah $Hdr: cons.c 1.14 94/12/14$
 */

#ifdef MACH_KERNEL
#include <sys/types.h>
#include <device/conf.h>
#include <mach/boolean.h>
#include <cons.h>
#else
#include <sys/param.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <hpdev/cons.h>
#endif

static	int cn_inited = 0;
static	struct consdev *cn_tab = 0;	/* physical console device info */
#ifndef MACH_KERNEL
static	struct tty *constty = 0;	/* virtual console output device */
#endif

/*
 * ROM getc/putc primitives.
 * On some architectures, the boot ROM provides basic character input/output
 * routines that can be used before devices are configured or virtual memory
 * is enabled.  This can be useful to debug (or catch panics from) code early
 * in the bootstrap procedure.
 */
int	(*romgetc)() = 0;
void	(*romputc)() = 0;

#if CONSBUFSIZE > 0
/*
 * Temporary buffer to store console output before a console is selected.
 * This is statically allocated so it can be called before malloc/kmem_alloc
 * have been initialized.  It is initialized so it won't be clobbered as
 * part of the zeroing of BSS (on PA/Mach).
 */
static	char consbuf[CONSBUFSIZE] = { 0 };
static	char *consbp = consbuf;
static	int consbufused = 0;
#endif

cninit()
{
	struct consdev *cp;
#ifdef MACH_KERNEL
	dev_ops_t cn_ops;
	int x;
#endif

	if (cn_inited)
		return;

	/*
	 * Collect information about all possible consoles
	 * and find the one with highest priority
	 */
	for (cp = constab; cp->cn_probe; cp++) {
		(*cp->cn_probe)(cp);
		if (cp->cn_pri > CN_DEAD &&
		    (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri))
			cn_tab = cp;
	}
	/*
	 * Found a console, initialize it.
	 */
	if (cp = cn_tab) { 
		/*
		 * Initialize as console
		 */
		(*cp->cn_init)(cp);
#ifdef MACH_KERNEL
		/*
		 * Look up its dev_ops pointer in the device table and
		 * place it in the device indirection table.
		 */
		if (dev_name_lookup(cp->cn_name, &cn_ops, &x) == FALSE)
			panic("cninit: dev_name_lookup failed");
		dev_set_indirection("console", cn_ops, minor(cp->cn_dev));
#endif
#if CONSBUFSIZE > 0
		/*
		 * Now that the console is initialized, dump any chars in
		 * the temporary console buffer.
		 */
		if (consbufused) {
			char *cbp = consbp;
			do {
				if (*cbp)
					cnputc(*cbp);
				if (++cbp == &consbuf[CONSBUFSIZE])
					cbp = consbuf;
			} while (cbp != consbp);
			consbufused = 0;
		}
#endif
		cn_inited = 1;
		return;
	}
	/*
	 * No console device found, not a problem for BSD, fatal for Mach
	 */
#ifdef MACH_KERNEL
	panic("can't find a console device");
#endif
}

#ifndef MACH_KERNEL
cnopen(dev, flag)
	dev_t dev;
{
	if (cn_tab == NULL)
		return(0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_open)(dev, flag));
}
 
cnclose(dev, flag)
	dev_t dev;
{
	if (cn_tab == NULL)
		return(0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_close)(dev, flag));
}
 
cnread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	if (cn_tab == NULL)
		return(0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_read)(dev, uio));
}
 
cnwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	if (cn_tab == NULL)
		return(0);
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_write)(dev, uio));
}
 
cnioctl(dev, cmd, data, flag)
	dev_t dev;
	caddr_t data;
{
	if (cn_tab == NULL)
		return(0);
	/*
	 * Superuser can always use this to wrest control of console
	 * output from the "virtual" console.
	 */
	if (cmd == TIOCCONS && constty) {
		if (!suser())
			return(EPERM);
		constty = NULL;
		return(0);
	}
	dev = cn_tab->cn_dev;
	return ((*cdevsw[major(dev)].d_ioctl)(dev, cmd, data, flag));
}

cnselect(dev, rw)
	dev_t dev;
	int rw;
{
	if (cn_tab == NULL)
		return(1);
	return(ttselect(cn_tab->cn_dev, rw));
}

#ifndef hp300
/*
 * XXX Should go away when the new CIO MUX driver is in place
 */
#define	d_control	d_mmap
cncontrol(dev, cmd, data)
	dev_t dev;
	int cmd;
	int data;
{
	if (cn_tab == NULL)
		return(0);
	dev = cn_tab->cn_dev;
	return((*cdevsw[major(dev)].d_control)(dev, cmd, data));
}
#undef	d_control
#endif
#endif

cngetc()
{
	if (cn_tab)
		return ((*cn_tab->cn_getc)(cn_tab->cn_dev, 1));
	if (romgetc)
		return ((*romgetc)(1));
	return (0);
}

#ifdef MACH_KERNEL
cnmaygetc()
{
	if (cn_tab)
		return((*cn_tab->cn_getc)(cn_tab->cn_dev, 0));
	if (romgetc)
		return ((*romgetc)(0));
	return (0);
}
#endif

cnputc(c)
	int c;
{
	if (c == 0)
		return;

	if (cn_tab) {
		(*cn_tab->cn_putc)(cn_tab->cn_dev, c);
		if (c == '\n')
			(*cn_tab->cn_putc)(cn_tab->cn_dev, '\r');
	} else if (romputc) {
		(*romputc)(c);
		if (c == '\n')
			(*romputc)('\r');
	}
#if CONSBUFSIZE > 0
	else {
		if (consbufused == 0) {
			consbp = consbuf;
			consbufused = 1;
			bzero(consbuf, CONSBUFSIZE);
		}
		*consbp++ = c;
		if (consbp >= &consbuf[CONSBUFSIZE])
			consbp = consbuf;
	}
#endif
}
