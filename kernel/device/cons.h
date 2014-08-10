/* 
 * Copyright (c) 1988-1994, The University of Utah and
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
 *	Utah $Hdr: cons.h 1.10 94/12/14$
 */

struct consdev {
#ifdef MACH_KERNEL
	char	*cn_name;	/* name of device in dev_name_list */
#endif
	int	(*cn_probe)();	/* probe hardware and fill in consdev info */
	int	(*cn_init)();	/* turn on as console */
	int	(*cn_getc)();	/* kernel getchar interface */
	int	(*cn_putc)();	/* kernel putchar interface */
	dev_t	cn_dev;		/* major/minor of device */
	short	cn_pri;		/* pecking order; the higher the better */
};

/* values for cn_pri - reflect our policy for console selection */
#define	CN_DEAD		0	/* device doesn't exist */
#define CN_NORMAL	1	/* device exists but is nothing special */
#define CN_INTERNAL	2	/* "internal" bit-mapped display */
#define CN_REMOTE	3	/* serial interface with remote bit set */

/* XXX */
#define	CONSMAJOR	0

#define CONSBUFSIZE	1024

#ifdef KERNEL
extern	struct consdev constab[];
extern	struct consdev *cn_tab;
#endif
