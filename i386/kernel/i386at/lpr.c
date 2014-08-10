/* 
 * Mach Operating System
 * Copyright (c) 1993-1990 Carnegie Mellon University
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
 *	Parallel port printer driver v1.0
 *	All rights reserved.
 */ 
  
#include <lpr.h>
#if NLPR > 0
#include <par.h>
#include <de6c.h>
  
#ifdef	MACH_KERNEL
#include <mach/std_types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <device/conf.h>
#include <device/errno.h>
#include <device/tty.h>
#include <device/io_req.h>
#else	MACH_KERNEL
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/file.h>
#endif	MACH_KERNEL
  
#include <i386/ipl.h>
#include <i386/pio.h>
#include <chips/busses.h>
#include <i386at/lprreg.h>
  
#if NPAR > 0
extern int	parattach();
#endif

#if NDE6C > 0
extern int	de6cattach();
#endif

extern void 	splx();
extern spl_t	spltty();
extern void 	timeout();
extern void 	ttrstrt();

/* 
 * Driver information for auto-configuration stuff.
 */

int 	lprprobe(), lprintr(), lprstart(), lprstop();
void	lprattach(struct bus_device *);
#ifdef	MACH_KERNEL
int lprstop(), lprgetstat(), lprsetstat();
#endif	MACH_KERNEL

struct bus_device *lprinfo[NLPR];	/* ??? */

static vm_offset_t lpr_std[NLPR] = { 0 };
static struct bus_device *lpr_info[NLPR];
struct bus_driver lprdriver = {
	lprprobe, 0, lprattach, 0, lpr_std, "lpr", lpr_info, 0, 0, 0};

struct tty	lpr_tty[NLPR];

int lpr_alive[NLPR];

lprprobe(port, dev)
struct bus_device *dev;
{
	u_short	addr = (u_short) dev->address;
	int	unit = dev->unit;
	int ret;

	if ((unit < 0) || (unit > NLPR)) {
		printf("com %d out of range\n", unit);
		return(0);
	}

	outb(INTR_ENAB(addr),0x07);
	outb(DATA(addr),0xaa);
	ret = inb(DATA(addr)) == 0xaa;
	if (ret) {
		if (lpr_alive[unit]) {
			printf("lpr: Multiple alive entries for unit %d.\n", unit);
			printf("lpr: Ignoring entry with address = %x .\n", addr);
			ret = 0;
		} else
			lpr_alive[unit]++;
	}
	return(ret);
}

void lprattach(struct bus_device *dev)
{
	u_char		unit = dev->unit;
	u_short		addr = (u_short) dev->address;
	struct tty	*tp = &lpr_tty[unit];

	take_dev_irq(dev);
	printf(", port = %x, spl = %d, pic = %d.",
	       dev->address, dev->sysdep, dev->sysdep1);
	lprinfo[unit] = dev;
  
	outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) & 0x0f);

#if NPAR > 0
	parattach(dev);
#endif

#if NDE6C > 0 && !defined(LINUX_DEV)
	de6cattach(dev);
#endif
	return;
}

lpropen(dev, flag, ior)
int dev;
int flag;
#ifdef	MACH_KERNEL
io_req_t ior;
#endif	MACH_KERNEL
{
int unit = minor(dev);
struct bus_device *isai;
struct tty *tp;
u_short addr;
  
	if (unit >= NLPR || (isai = lprinfo[unit]) == 0 || isai->alive == 0)
		return(ENXIO);
	tp = &lpr_tty[unit];
#ifndef	MACH_KERNEL
	if (tp->t_state & TS_XCLUDE && u.u_uid != 0)
		return(EBUSY);
#endif	MACH_KERNEL
	addr = (u_short) isai->address;
	tp->t_dev = dev;
	tp->t_addr = *(caddr_t *)&addr;
	tp->t_oproc = lprstart;
	tp->t_state |= TS_WOPEN;
#ifdef	MACH_KERNEL
	tp->t_stop = lprstop;
	tp->t_getstat = lprgetstat;
	tp->t_setstat = lprsetstat;
#endif	MACH_KERNEL
	if ((tp->t_state & TS_ISOPEN) == 0)
		ttychars(tp);
	outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) | 0x10);
	tp->t_state |= TS_CARR_ON;
	return (char_open(dev, tp, flag, ior));
}

lprclose(dev, flag)
int dev;
int flag;
{
int 		unit = minor(dev);
struct	tty	*tp = &lpr_tty[unit];
u_short		addr = 	(u_short) lprinfo[unit]->address;
  
#ifndef	MACH_KERNEL
  	(*linesw[tp->t_line].l_close)(tp);
#endif	MACH_KERNEL
	ttyclose(tp);
	if (tp->t_state&TS_HUPCLS || (tp->t_state&TS_ISOPEN)==0) {
		outb(INTR_ENAB(addr), inb(INTR_ENAB(addr)) & 0x0f);
		tp->t_state &= ~TS_BUSY;
	} 
}

#ifdef	MACH_KERNEL
lprread(dev, ior)
int	dev;
io_req_t ior;
{
	return char_read(&lpr_tty[minor(dev)], ior);
}

lprwrite(dev, ior)
int	dev;
io_req_t ior;
{
	return char_write(&lpr_tty[minor(dev)], ior);
}

lprportdeath(dev, port)
dev_t		dev;
mach_port_t	port;
{
	return (tty_portdeath(&lpr_tty[minor(dev)], port));
}

io_return_t
lprgetstat(dev, flavor, data, count)
dev_t		dev;
int		flavor;
int		*data;		/* pointer to OUT array */
unsigned int	*count;		/* out */
{
	io_return_t	result = D_SUCCESS;
	int		unit = minor(dev);

	switch (flavor) {
	default:
		result = tty_get_status(&lpr_tty[unit], flavor, data, count);
		break;
	}
	return (result);
}

io_return_t
lprsetstat(dev, flavor, data, count)
dev_t		dev;
int		flavor;
int *		data;
unsigned int	count;
{
	io_return_t	result = D_SUCCESS;
	int 		unit = minor(dev);
	u_short		dev_addr = (u_short) lprinfo[unit]->address;
	int		s;

	switch (flavor) {
	default:
		result = tty_set_status(&lpr_tty[unit], flavor, data, count);
/*		if (result == D_SUCCESS && flavor == TTY_STATUS)
			lprparam(unit);
*/		return (result);
	}
	return (D_SUCCESS);
}
#else	MACH_KERNEL
int lprwrite(dev, uio)
     int dev;
     struct uio *uio;
{
  struct tty *tp= &lpr_tty[minor(dev)];
  
  return ((*linesw[tp->t_line].l_write)(tp, uio));
}

int lprioctl(dev, cmd, addr, mode)
     int dev;
     int cmd;
     caddr_t addr;
     int mode;
{
  int error;
  spl_t s;
  int unit = minor(dev);
  struct tty *tp = &lpr_tty[unit];
  
  error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, addr,mode);
  if (error >= 0)
    return(error);
  error = ttioctl(tp, cmd, addr,mode);
  if (error >= 0)
    return (error);
  s = spltty();
  switch (cmd) {
  default:
    splx(s);
    return(ENOTTY);
  }
  splx(s);
  return(0);
}
#endif	MACH_KERNEL

int lprintr(unit)
int unit;
{
	register struct tty *tp = &lpr_tty[unit];

	if ((tp->t_state & TS_ISOPEN) == 0)
	  return;

	tp->t_state &= ~TS_BUSY;
	if (tp->t_state&TS_FLUSH)
		tp->t_state &=~TS_FLUSH;
	tt_write_wakeup(tp);
	lprstart(tp);
}   

int lprstart(tp)
struct tty *tp;
{
	spl_t s = spltty();
	u_short addr = (natural_t) tp->t_addr;
	int status = inb(STATUS(addr));
	char nch;

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) {
		splx(s);
		return(0);
	}

	if (status & 0x20) {
		printf("Printer out of paper!\n");
		splx(s);
		return(0);
	}

	if (tp->t_outq.c_cc <= TTLOWAT(tp)) {
#ifdef	MACH_KERNEL
		tt_write_wakeup(tp);
#else	MACH_KERNEL
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup ((caddr_t)&tp->t_outq);
		}
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
#endif	MACH_KERNEL
	}
	if (tp->t_outq.c_cc == 0) {
		splx(s);
		return(0);
	}
#ifdef	MACH_KERNEL
	nch = getc(&tp->t_outq);
	if ((tp->t_flags & LITOUT) == 0 && (nch & 0200)) {
		timeout(ttrstrt, (char *)tp, (nch & 0x7f) + 6);
		tp->t_state |= TS_TIMEOUT;
		return;
	}
	outb(DATA(addr), nch);
	outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) | 0x01);
	outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) & 0x1e);
	tp->t_state |= TS_BUSY;
#else	MACH_KERNEL
	if (tp->t_flags & (RAW|LITOUT))
		nch = ndqb(&tp->t_outq,0);
	else {
		nch = ndqb(&tp->t_outq, 0200);
		if (nch == 0) {
		    nch = getc(&tp->t_outq);
		    timeout(ttrstrt,(caddr_t)tp,(nch&0x7f)+6);
		    tp->t_state |= TS_TIMEOUT;
		    splx(s);
		    return(0);
		}
	}
	if (nch) {
		nch=getc(&tp->t_outq);
		outb(DATA(addr), nch);
		outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) | 0x01);
		outb(INTR_ENAB(addr),inb(INTR_ENAB(addr)) & 0x1e);
		tp->t_state |= TS_BUSY;
	}
#endif	MACH_KERNEL
	splx(s);
	return(0);
}

#ifdef	MACH_KERNEL
lprstop(tp, flags)
register struct tty *tp;
int	flags;
{
	if ((tp->t_state & TS_BUSY) && (tp->t_state & TS_TTSTOP) == 0)
		tp->t_state |= TS_FLUSH;
}
#else	MACH_KERNEL
int lprstop(tp, flag)
struct tty *tp;
{
	int s = spltty();
  
	if ((tp->t_state&TS_BUSY) && (!(tp->t_state&TS_TTSTOP)))
		tp->t_state |= TS_FLUSH;
	splx(s);
}
#endif	MACH_KERNEL
lprpr(unit)
{
	lprpr_addr(lprinfo[unit]->address);
	return 0;
}

lprpr_addr(addr)
{
	printf("DATA(%x) %x, STATUS(%x) %x, INTR_ENAB(%x) %x\n",
		DATA(addr), inb(DATA(addr)),
		STATUS(addr), inb(STATUS(addr)),
		INTR_ENAB(addr), inb(INTR_ENAB(addr)));
}
#endif NLPR
