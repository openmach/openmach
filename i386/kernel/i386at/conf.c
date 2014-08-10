/* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 * Device switch for i386 AT bus.
 */

#include <mach/machine/vm_types.h>
#include <device/conf.h>

extern vm_offset_t block_io_mmap();

extern int	timeopen(), timeclose();
extern vm_offset_t timemmap();
#define	timename		"time"

#include <hd.h>
#if	NHD > 0
extern int	hdopen(), hdclose(), hdread(), hdwrite();
extern int	hdgetstat(), hdsetstat(), hddevinfo();
#define	hdname			"hd"

#if 0
extern int      pchdopen(),pchdread(),pchdwrite(),pchdgetstat(),pchdsetstat();
#define pchdname                "pchd"
#endif

#endif	NHD > 0

#include <aha.h>
#if	NAHA > 0
int     rz_open(), rz_close(), rz_read(), rz_write();
int     rz_get_status(), rz_set_status(), rz_devinfo();
int	cd_open(), cd_close(), cd_read(), cd_write();
#define rzname "sd"
#define	tzname "st"
#define	scname "sc"	/* processors */
#define cdname	"cd_audio"	/* CD-ROM DA */

#endif	/*NAHA > 0*/

#include <fd.h>
#if	NFD > 0
extern int	fdopen(), fdclose(), fdread(), fdwrite();
extern int	fdgetstat(), fdsetstat(), fddevinfo();
#define	fdname			"fd"
#endif	NFD > 0

#include <wt.h>
#if	NWT > 0
extern int	wtopen(), wtread(), wtwrite(), wtclose();
#define	wtname			"wt"
#endif	NWT > 0

#include <pc586.h>
#if	NPC586 > 0
extern int	pc586open(), pc586output(), pc586getstat(), pc586setstat(),
		pc586setinput();
#define	pc586name		"pc"
#endif NPC586 > 0

#include <ne.h>
#if     NNE > 0
extern int      neopen(), neoutput(), negetstat(), nesetstat(), nesetinput();
#ifdef FIPC
extern int      nefoutput();
#endif /* FIPC */
#define nename                  "ne"
#endif  NNE > 0

#include <ns8390.h>
#if	NNS8390 > 0
extern int	wd8003open(), eliiopen();
extern int	ns8390output(), ns8390getstat(), ns8390setstat(), 
		ns8390setinput();
#define	ns8390wdname		"wd"
#define	ns8390elname		"el"
#endif NNS8390 > 0

#include <at3c501.h>
#if	NAT3C501 > 0
extern int	at3c501open(), at3c501output(),
		at3c501getstat(), at3c501setstat(),
		at3c501setinput();
#define	at3c501name		"et"
#endif NAT3C501 > 0

#include <ul.h>
#if NUL > 0
extern int    ulopen(), uloutput(), ulgetstat(), ulsetstat(),
               ulsetinput();
#define ulname                        "ul"
#endif NUL > 0

#include <wd.h>
#if NWD > 0
extern int    wdopen(), wdoutput(), wdgetstat(), wdsetstat(),
              wdsetinput();
#define wdname                        "wd"
#endif NWD > 0

#include <hpp.h>
#if NHPP > 0
extern int    hppopen(), hppoutput(), hppgetstat(), hppsetstat(),
               hppsetinput();
#define hppname                       "hpp"
#endif /* NHPP > 0 */

#include <par.h>
#if	NPAR > 0
extern int	paropen(), paroutput(), pargetstat(), parsetstat(),
		parsetinput();
#define	parname		"par"
#endif NPAR > 0

#include <de6c.h>
#if	NDE6C > 0
extern int	de6copen(), de6coutput(), de6cgetstat(), de6csetstat(),
		de6csetinput();
#define	de6cname		"de"
#endif NDE6C > 0

extern int	kdopen(), kdclose(), kdread(), kdwrite();
extern int	kdgetstat(), kdsetstat(), kdportdeath();
extern vm_offset_t kdmmap();
#define	kdname			"kd"

#include <com.h>
#if	NCOM > 0
extern int	comopen(), comclose(), comread(), comwrite();
extern int	comgetstat(), comsetstat(), comportdeath();
#define	comname			"com"
#endif	NCOM > 0

#include <lpr.h>
#if	NLPR > 0
extern int	lpropen(), lprclose(), lprread(), lprwrite();
extern int	lprgetstat(), lprsetstat(), lprportdeath();
#define	lprname			"lpr"
#endif	NLPR > 0

#include <blit.h>
#if NBLIT > 0
extern int	blitopen(), blitclose(), blit_get_stat();
extern vm_offset_t blitmmap();
#define	blitname		"blit"

extern int	mouseinit(), mouseopen(), mouseclose();
extern int	mouseioctl(), mouseselect(), mouseread();
#endif

extern int	kbdopen(), kbdclose(), kbdread();
extern int	kbdgetstat(), kbdsetstat();
#define	kbdname			"kbd"

extern int	mouseopen(), mouseclose(), mouseread();
#define	mousename		"mouse"

extern int	ioplopen(), ioplclose();
extern vm_offset_t ioplmmap();
#define	ioplname		"iopl"

/*
 * List of devices - console must be at slot 0
 */
struct dev_ops	dev_name_list[] =
{
	/*name,		open,		close,		read,
	  write,	getstat,	setstat,	mmap,
	  async_in,	reset,		port_death,	subdev,
	  dev_info */

	/* We don't assign a console here, when we find one via
	   cninit() we stick something appropriate here through the
	   indirect list */
	{ "cn",		nulldev,	nulldev,	nulldev,
	  nulldev,	nulldev,	nulldev,	nulldev,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ kdname,	kdopen,		kdclose,	kdread,
	  kdwrite,	kdgetstat,	kdsetstat,	kdmmap,
	  nodev,	nulldev,	kdportdeath,	0,
	  nodev },

	{ timename,	timeopen,	timeclose,	nulldev,
	  nulldev,	nulldev,	nulldev,	timemmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#ifndef LINUX_DEV
#if	NHD > 0
	{ hdname,	hdopen,		hdclose,	hdread,
	  hdwrite,	hdgetstat,	hdsetstat,	nomap,
	  nodev,	nulldev,	nulldev,	1024,
	  hddevinfo },
#endif	NHD > 0

#if	NAHA > 0
	{ rzname,	rz_open,	rz_close,	rz_read,
	  rz_write,	rz_get_status,	rz_set_status,	nomap,
	  nodev,	nulldev,	nulldev,	1024, /* 8 */
	  rz_devinfo },

	{ tzname,	rz_open,	rz_close,	rz_read,
	  rz_write,	rz_get_status,	rz_set_status,	nomap,
	  nodev,	nulldev,	nulldev,	8,
	  nodev },

	{ cdname,	cd_open,	cd_close,	cd_read,
	  cd_write,	nodev,		nodev,		nomap,
	  nodev,	nulldev,	nulldev,	8,
	  nodev },

	{ scname,	rz_open,	rz_close,	rz_read,
	  rz_write,	rz_get_status,	rz_set_status,	nomap,
	  nodev,	nulldev,	nulldev,	8,
	  nodev },

#endif	/*NAHA > 0*/

#if	NFD > 0
	{ fdname,	fdopen,		fdclose,	fdread,
	  fdwrite,	fdgetstat,	fdsetstat,	nomap,
	  nodev,	nulldev,	nulldev,	64,
	  fddevinfo },
#endif	NFD > 0

#if	NWT > 0
	{ wtname,	wtopen,		wtclose,	wtread,
	  wtwrite,	nulldev,	nulldev,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },
#endif	NWT > 0

#if	NPC586 > 0
	{ pc586name,	pc586open,	nulldev,	nulldev,
	  pc586output,	pc586getstat,	pc586setstat,	nomap,
	  pc586setinput,nulldev,	nulldev, 	0,
	  nodev },
#endif

#if     NNE > 0
        { nename,       neopen,         nulldev,        nulldev,
          neoutput,     negetstat,      nesetstat,      nulldev,
#ifdef FIPC
          nesetinput,   nulldev,        nefoutput,      0,
#else
          nesetinput,   nulldev,        nulldev,        0,
#endif /* FIPC */
          nodev },
#endif

#if	NAT3C501 > 0
	{ at3c501name,	at3c501open,	nulldev,	nulldev,
	  at3c501output,at3c501getstat,	at3c501setstat,	nomap,
	  at3c501setinput, nulldev,	nulldev, 	0,
	  nodev },
#endif

#if	NNS8390 > 0
	{ ns8390wdname,	wd8003open,	nulldev,	nulldev,
	  ns8390output, ns8390getstat,	ns8390setstat,	nomap,
	  ns8390setinput, nulldev,	nulldev,	0,
	  nodev },

	{ ns8390elname,	eliiopen,	nulldev,	nulldev,
	  ns8390output, ns8390getstat,	ns8390setstat,	nomap,
	  ns8390setinput, nulldev,	nulldev,	0,
	  nodev },
#endif

#if   	NUL > 0
        { ulname,       ulopen,         nulldev,        nulldev,
          uloutput,     ulgetstat,      ulsetstat,      nulldev,
          ulsetinput,   nulldev,        nulldev,        0,
          nodev },
#endif

#if   	NWD > 0
	{ wdname,       wdopen,         nulldev,        nulldev,
          wdoutput,     wdgetstat,      wdsetstat,      nulldev,
          wdsetinput,   nulldev,        nulldev,        0,
	  nodev },
#endif

#if   	NHPP > 0
	{ hppname,      hppopen,        nulldev,        nulldev,
	  hppoutput,  hppgetstat,     hppsetstat,     nulldev,
	  hppsetinput,  nulldev,        nulldev,        0,
          nodev },
#endif

#if	NPAR > 0
	{ parname,	paropen,	nulldev,	nulldev,
	  paroutput,	pargetstat,	parsetstat,	nomap,
	  parsetinput,	nulldev,	nulldev, 	0,
	  nodev },
#endif

#if	NDE6C > 0
	{ de6cname,	de6copen,	nulldev,	nulldev,
	  de6coutput,	de6cgetstat,	de6csetstat,	nomap,
	  de6csetinput,	nulldev,	nulldev, 	0,
	  nodev },
#endif
#endif /* ! LINUX_DEV */

#if	NCOM > 0
	{ comname,	comopen,	comclose,	comread,
	  comwrite,	comgetstat,	comsetstat,	nomap,
	  nodev,	nulldev,	comportdeath,	0,
	  nodev },
#endif

#ifndef LINUX_DEV
#if	NLPR > 0
	{ lprname,	lpropen,	lprclose,	lprread,
	  lprwrite,	lprgetstat,	lprsetstat,	nomap,
	  nodev,	nulldev,	lprportdeath,	0,
	  nodev },
#endif
#endif /* ! LINUX_DEV */

#if	NBLIT > 0
	{ blitname,	blitopen,	blitclose,	nodev,
	  nodev,	blit_get_stat,	nodev,		blitmmap,
	  nodev,	nodev,		nodev,		0,
	  nodev },
#endif

	{ mousename,	mouseopen,	mouseclose,	mouseread,
	  nodev,	nulldev,	nulldev,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ kbdname,	kbdopen,	kbdclose,	kbdread,
	  nodev,	kbdgetstat,	kbdsetstat,	nomap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

	{ ioplname,	ioplopen,	ioplclose,	nodev,
	  nodev,	nodev,		nodev,		ioplmmap,
	  nodev,	nulldev,	nulldev,	0,
	  nodev },

#if 0
#if	NHD > 0
	{ pchdname,     pchdopen,      	hdclose,	pchdread,
	  pchdwrite,	pchdgetstat,	pchdsetstat,	nomap,
	  nodev,	nulldev,	nulldev,	16,
	  hddevinfo },
#endif	NHD > 0
#endif

#if 0
#if     NHD > 0
        { hdname,       hdopen,         hdclose,        hdread,
          hdwrite,      hdgetstat,      hdsetstat,      nomap,
          nodev,        nulldev,        nulldev,        16,
          hddevinfo },
#endif  NHD > 0
#endif 0 /* Kevin doesn't know why this was here. */

};
int	dev_name_count = sizeof(dev_name_list)/sizeof(dev_name_list[0]);

/*
 * Indirect list.
 */
struct dev_indirect dev_indirect_list[] = {

	/* console */
	{ "console",	&dev_name_list[0],	0 }
};
int	dev_indirect_count = sizeof(dev_indirect_list)
				/ sizeof(dev_indirect_list[0]);
