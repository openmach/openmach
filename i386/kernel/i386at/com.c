/* 
 * Mach Operating System
 * Copyright (c) 1994,1993,1991,1990 Carnegie Mellon University
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

#include <com.h>
#if NCOM > 0

#include <mach/std_types.h>
#include <sys/types.h>
#include <sys/time.h>
#include <device/conf.h>
#include <device/errno.h>
#include <device/tty.h>
#include <device/io_req.h>

#include <i386/ipl.h>
#include <i386/pio.h>
#include <i386/machspl.h>
#include <chips/busses.h>
#include <i386at/comreg.h>

#include <rc.h>
#include <cons.h>

extern void timeout(), ttrstrt();

int comprobe(), comintr(), comstart(), commctl();
void comattach();
static void comparam();
int comstop(), comgetstat(), comsetstat();

static vm_offset_t com_std[NCOM] = { 0 };
struct bus_device *cominfo[NCOM];
struct bus_driver comdriver = {
	comprobe, 0, comattach, 0, com_std, "com", cominfo, 0, 0, 0};

struct tty com_tty[NCOM];
int commodem[NCOM];
int comcarrier[NCOM] = {0, 0,};
boolean_t comfifo[NCOM];
boolean_t comtimer_active;
int comtimer_state[NCOM];

#if RCLINE >= 0
#define RCBAUD B9600
static struct bus_device *comcndev;
int comcnprobe(struct consdev *cp);
int comcninit(struct consdev *cp);
int comcngetc(dev_t dev, int wait);
int comcnputc(dev_t dev, int c);
#endif

#ifndef	PORTSELECTOR
#define ISPEED	B9600
#define IFLAGS	(EVENP|ODDP|ECHO|CRMOD)
#else
#define ISPEED	B4800
#define IFLAGS	(EVENP|ODDP)
#endif

u_short divisorreg[] = {
	0,	2304,	1536,	1047,		/*     0,    50,    75,   110*/
	857,	 768,	 576,	 384,	 192,	/*   134.5, 150,   200,   300,   600*/
	 96,	  64,		  48,		/*  1200,  1800,  2000,  2400 */
      		  24,	  	  12,		/*  3600,  4800,  7200,  9600 */
	  6,	   3,	   2};			/* 19200, 38400, 56000 */


/*
 *
 * Probes are called during kernel boot: return 1 to mean that
 * the relevant device is present today.
 *
 */
int
comprobe_general(struct bus_device *dev, int noisy)
{
	u_short	addr = dev->address;
	int	unit = dev->unit;
	int     oldctl, oldmsb;
	char    *type = "8250";
	int     i;

	if ((unit < 0) || (unit > NCOM)) {
		printf("com %d out of range\n", unit);
		return(0);
	}
	oldctl = inb(LINE_CTL(addr));	 /* Save old value of LINE_CTL */
	oldmsb = inb(BAUD_MSB(addr));	 /* Save old value of BAUD_MSB */
	outb(LINE_CTL(addr), 0);	 /* Select INTR_ENAB */    
	outb(BAUD_MSB(addr), 0);
	if (inb(BAUD_MSB(addr)) != 0)
	  {
	    outb(LINE_CTL(addr), oldctl);
	    outb(BAUD_MSB(addr), oldmsb);
	    return 0;
	  }
	outb(LINE_CTL(addr), iDLAB);	 /* Select BAUD_MSB */
	outb(BAUD_MSB(addr), 255);
	if (inb(BAUD_MSB(addr)) != 255)
	  {
	    outb(LINE_CTL(addr), oldctl);
	    outb(BAUD_MSB(addr), oldmsb);
	    return 0;
	  }
	outb(LINE_CTL(addr), 0);	 /* Select INTR_ENAB */
	if (inb(BAUD_MSB(addr)) != 0)	 /* Check that it has kept its value*/
	  {
	    outb(LINE_CTL(addr), oldctl);
	    outb(BAUD_MSB(addr), oldmsb);
	    return 0;
	  }

	/* Com port found, now check what chip it has */
	
	for(i = 0; i < 256; i++)	 /* Is there Scratch register */
	  {
	    outb(SCR(addr), i);
	    if (inb(SCR(addr)) != i)
	      break;
	  }
	if (i == 256)
	  {				 /* Yes == 450 or 460 */
	    outb(SCR(addr), 0);
	    type = "82450 or 16450";
	    outb(FIFO_CTL(addr), iFIFOENA | iFIFO14CH);	 /* Enable fifo */
	    if ((inb(FIFO_CTL(addr)) & iFIFO14CH) != 0)
	      {				 /* Was it successfull */
		/* if both bits are not set then broken xx550 */
		if ((inb(FIFO_CTL(addr)) & iFIFO14CH) == iFIFO14CH)
		  {
		    type = "82550 or 16550";
		    comfifo[unit] = TRUE;
		  }
		else
		  {
		    type = "82550 or 16550 with non-working FIFO";
		  }
		outb(INTR_ID(addr), 0x00); /* Disable fifos */
	      }
	  }
	if (noisy)
		printf("com%d: %s chip.\n", unit, type);
	return 1;
}

/*
 * Probe routine for use during kernel startup when it is probing
 * all of bus_device_init
 */
int
comprobe(int port, struct bus_device *dev)
{
	return comprobe_general(dev, /*noisy*/ 1);
}

#if RCLINE >= 0
/*
 * Probe routine for use by the console
 */
int
comcnprobe(struct consdev *cp)
{
	struct	bus_device *b;
	int	maj, unit, pri;

	maj = 0;
	unit = -1;
	pri = CN_DEAD;

	for (b = bus_device_init; b->driver; b++)
		if (strcmp(b->name, "com") == 0
		    && b->unit == RCLINE
		    && comprobe_general(b, /*quiet*/ 0))
		{
			/* Found one */
			comcndev = b;
			unit = b->unit;
			pri = CN_REMOTE;
			break;
		}

	cp->cn_dev = makedev(maj, unit);
	cp->cn_pri = pri;
}
#endif


/*
 *
 * Device Attach's are called during kernel boot, but only if the matching
 * device Probe returned a 1.
 *
 */
void
comattach(struct bus_device *dev)
{
	u_char	unit = dev->unit;
	u_short	addr = dev->address;

	take_dev_irq(dev);
	printf(", port = %x, spl = %d, pic = %d. (DOS COM%d)",
	       dev->address, dev->sysdep, dev->sysdep1, unit+1);

/*	comcarrier[unit] = addr->flags;*/
	commodem[unit] = 0;

	outb(INTR_ENAB(addr), 0);
	outb(MODEM_CTL(addr), 0);
	while (!(inb(INTR_ID(addr))&1)) {
		(void) inb(LINE_STAT (addr)); 	/* reset overrun error etc */
		(void) inb(TXRX      (addr)); 	/* reset data-ready */
		(void) inb(MODEM_STAT(addr)); 	/* reset modem status reg */
	}
}

#if RCLINE >= 0
/*
 * Attach/init routine for console.  This isn't called by
 * configure_bus_device which sets the alive, adaptor, and minfo
 * fields of the bus_device struct (comattach is), therefore we do
 * that by hand.
 */
int
comcninit(struct consdev *cp)
{
	u_char	unit = comcndev->unit;
	u_short	addr = comcndev->address;
		      
	take_dev_irq(comcndev);

	comcndev->alive = 1;
	comcndev->adaptor = 0;
	cominfo[minor(cp->cn_dev)] = comcndev;

	outb(LINE_CTL(addr), iDLAB);
	outb(BAUD_LSB(addr), divisorreg[RCBAUD] & 0xff);
	outb(BAUD_MSB(addr), divisorreg[RCBAUD] >>8);
	outb(LINE_CTL(addr), i7BITS|iPEN);
	outb(INTR_ENAB(addr), 0);
	outb(MODEM_CTL(addr), iDTR|iRTS|iOUT2);

	{
		char	msg[128];
		volatile unsigned char *p = (volatile unsigned char *)0xb8000;
		int	i;

		sprintf(msg, "    **** using COM port %d for console ****",
			unit+1);
		for (i = 0; msg[i]; i++) {
			p[2*i] = msg[i];
			p[2*i+1] = (0<<7)	/* blink */
				   | (0x0<<4)	/* bg */
				   | (1<<3)	/* hi-intensity */
				   | 0x4;	/* fg */
		}
	}

}
#endif

/*
 *	Probe for COM<dev> after autoconfiguration.
 *	Used to handle PCMCIA modems, which may appear
 *	at any time.
 */
boolean_t com_reprobe(
	int	unit)
{
	struct bus_device	*device;

	/*
	 *	Look for COM device <unit> in the device
	 *	initialization list.  It must not be alive
	 *	(otherwise we would have opened it already).
	 */
	for (device = bus_device_init; device->driver; device++) {
	    if (device->driver == &comdriver && device->unit == unit &&
		!device->alive && device->ctlr == (char)-1)
	    {
		/*
		 *	Found an entry for com port <unit>.
		 *	Probe it.
		 */
		if (configure_bus_device(device->name,
					 device->address,
					 device->phys_address,
					 0,
					 "atbus"))
		    return TRUE;
	    }
	}
	return FALSE;
}

io_return_t comopen(
	int dev,
	int flag,
	io_req_t ior)
{
	int		unit = minor(dev);
	u_short		addr;
	struct bus_device	*isai;
	struct tty	*tp;
	spl_t		s;
	io_return_t	result;

	if (unit >= NCOM)
	    return ENXIO;	/* no such device */
	if ((isai = cominfo[unit]) == 0 || isai->alive == 0) {
	    /*
	     *	Try to probe it again
	     */
	    if (!com_reprobe(unit))
		return ENXIO;
	}
	tp = &com_tty[unit];

	if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0) {
		ttychars(tp);
		tp->t_addr = (char *)isai->address;
		tp->t_dev = dev;
		tp->t_oproc = comstart;
		tp->t_stop = comstop;
		tp->t_mctl = commctl;
		tp->t_getstat = comgetstat;
		tp->t_setstat = comsetstat;
#ifndef	PORTSELECTOR
		if (tp->t_ispeed == 0) {
#else
			tp->t_state |= TS_HUPCLS;
#endif	/* PORTSELECTOR */
			tp->t_ispeed = ISPEED;
			tp->t_ospeed = ISPEED;
			tp->t_flags = IFLAGS;
			tp->t_state &= ~TS_BUSY;
#ifndef	PORTSELECTOR
		}
#endif	/* PORTSELECTOR */
	}
/*rvb	tp->t_state |= TS_WOPEN; */
	if ((tp->t_state & TS_ISOPEN) == 0)
		comparam(unit);
	addr = (int)tp->t_addr;

	s = spltty();
	if (!comcarrier[unit])	/* not originating */
		tp->t_state |= TS_CARR_ON;
	else {
		int modem_stat = inb(MODEM_STAT(addr));
		if (modem_stat & iRLSD)
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &= ~TS_CARR_ON;
		fix_modem_state(unit, modem_stat);
	} 
	splx(s);

	result = char_open(dev, tp, flag, ior);

	if (!comtimer_active) {
		comtimer_active = TRUE;
		comtimer();
	}

	s = spltty();
	while(!(inb(INTR_ID(addr))&1)) { /* while pending interrupts */
		(void) inb(LINE_STAT (addr)); /* reset overrun error  */
		(void) inb(TXRX      (addr)); /* reset data-ready	    */
		(void) inb(MODEM_STAT(addr)); /* reset modem status   */
	}
	splx(s);
	return result;
}

io_return_t comclose(dev, flag)
int dev;
int flag;
{
	struct tty	*tp = &com_tty[minor(dev)];
	u_short		addr = (int)tp->t_addr;

	ttyclose(tp);
	if (tp->t_state&TS_HUPCLS || (tp->t_state&TS_ISOPEN)==0) { 
		outb(INTR_ENAB(addr), 0);
		outb(MODEM_CTL(addr), 0);
		tp->t_state &= ~TS_BUSY;
		commodem[minor(dev)] = 0;
		if (comfifo[minor(dev)] != 0)
			outb(INTR_ID(addr), 0x00); /* Disable fifos */
	}
	return 0;
}

io_return_t comread(dev, ior)
int	dev;
io_req_t ior;
{
	return char_read(&com_tty[minor(dev)], ior);
}

io_return_t comwrite(dev, ior)
int	dev;
io_req_t ior;
{
	return char_write(&com_tty[minor(dev)], ior);
}

io_return_t comportdeath(dev, port)
dev_t		dev;
mach_port_t	port;
{
	return (tty_portdeath(&com_tty[minor(dev)], port));
}

io_return_t
comgetstat(dev, flavor, data, count)
dev_t		dev;
int		flavor;
int		*data;		/* pointer to OUT array */
unsigned int	*count;		/* out */
{
	io_return_t	result = D_SUCCESS;
	int		unit = minor(dev);

	switch (flavor) {
	case TTY_MODEM:
		fix_modem_state(unit, inb(MODEM_STAT(cominfo[unit]->address)));
		*data = commodem[unit];
		*count = 1;
		break;
	default:
		result = tty_get_status(&com_tty[unit], flavor, data, count);
		break;
	}
	return (result);
}

io_return_t
comsetstat(dev, flavor, data, count)
dev_t		dev;
int		flavor;
int *		data;
unsigned int	count;
{
	io_return_t	result = D_SUCCESS;
	int 		unit = minor(dev);
	struct tty	*tp = &com_tty[unit];

	switch (flavor) {
	case TTY_SET_BREAK:
		commctl(tp, TM_BRK, DMBIS);
		break;
	case TTY_CLEAR_BREAK:
		commctl(tp, TM_BRK, DMBIC);
		break;
	case TTY_MODEM:
		commctl(tp, *data, DMSET);
		break;
	default:
		result = tty_set_status(&com_tty[unit], flavor, data, count);
		if (result == D_SUCCESS && flavor == TTY_STATUS)
			comparam(unit);
		return (result);
	}
	return (D_SUCCESS);
}

comintr(unit)
int unit;
{
	register struct tty	*tp = &com_tty[unit];
	u_short			addr = cominfo[unit]->address;
	static char 		comoverrun = 0;
	char			c, line, intr_id;
	int			line_stat;

	while (! ((intr_id=(inb(INTR_ID(addr))&MASKi)) & 1))
	    switch (intr_id) { 
		case MODi: 
		    /* modem change */
			commodem_intr(unit, inb(MODEM_STAT(addr)));
			break;

		case TRAi:
			comtimer_state[unit] = 0;
			tp->t_state &= ~(TS_BUSY|TS_FLUSH);
			tt_write_wakeup(tp);
			(void) comstart(tp);
			break;
		case RECi:
		case CTIi:         /* Character timeout indication */
			if (tp->t_state&TS_ISOPEN) {
				while ((line = inb(LINE_STAT(addr))) & iDR) {
					c = inb(TXRX(addr));
					ttyinput(c, tp);
				}
			} else
				tt_open_wakeup(tp);
			break;
		case LINi: 
			line_stat = inb(LINE_STAT(addr));

			if ((line_stat & iPE) &&
			    ((tp->t_flags&(EVENP|ODDP)) == EVENP ||
			     (tp->t_flags&(EVENP|ODDP)) == ODDP)) {
				/* parity error */;
			} else 	if (line&iOR && !comoverrun) {
				printf("com%d: overrun\n", unit);
				comoverrun = 1;
			} else if (line_stat & (iFE | iBRKINTR)) {
				/* framing error or break */
				ttyinput(tp->t_breakc, tp);
			}
			break;
		}
}

static void
comparam(unit)
register int unit;
{
	struct tty	*tp = &com_tty[unit];
	u_short		addr = (int)tp->t_addr;
	spl_t		s = spltty();
	int		mode;

        if (tp->t_ispeed == B0) {
		tp->t_state |= TS_HUPCLS;
		outb(MODEM_CTL(addr), iOUT2);
		commodem[unit] = 0;
		splx(s);
		return;
	}

	/* Do input buffering */
	if (tp->t_ispeed >= B300)
		tp->t_state |= TS_MIN;

	outb(LINE_CTL(addr), iDLAB);
	outb(BAUD_LSB(addr), divisorreg[tp->t_ispeed] & 0xff);
	outb(BAUD_MSB(addr), divisorreg[tp->t_ispeed] >> 8);

	if (tp->t_flags & (RAW|LITOUT|PASS8))
		mode = i8BITS;
	else
		mode = i7BITS | iPEN;
	if (tp->t_flags & EVENP)
		mode |= iEPS;
	if (tp->t_ispeed == B110)
		/*
		 * 110 baud uses two stop bits -
		 * all other speeds use one
		 */
		mode |= iSTB;

	outb(LINE_CTL(addr), mode);

	outb(INTR_ENAB(addr), iTX_ENAB|iRX_ENAB|iMODEM_ENAB|iERROR_ENAB);
	if (comfifo[unit])
		outb(FIFO_CTL(addr), iFIFOENA|iFIFO14CH);
	outb(MODEM_CTL(addr), iDTR|iRTS|iOUT2);
	commodem[unit] |= (TM_DTR|TM_RTS);
        splx(s);
}

comparm(int unit, int baud, int intr, int mode, int modem)
{
	u_short addr = (u_short)(cominfo[unit]->address);
	spl_t	s = spltty();

	if (unit != 0 && unit != 1) {
		printf("comparm(unit, baud, mode, intr, modem)\n");
		splx(s);
		return;
	}
	outb(LINE_CTL(addr), iDLAB);
	outb(BAUD_LSB(addr), divisorreg[baud] & 0xff);
	outb(BAUD_MSB(addr), divisorreg[baud] >> 8);
	outb(LINE_CTL(addr), mode);
	outb(INTR_ENAB(addr), intr);
	outb(MODEM_CTL(addr), modem);
	splx(s);
}

int comst_1, comst_2, comst_3, comst_4, comst_5 = 14;

int
comstart(tp)
struct tty *tp;
{
	char nch;
	int i;

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) {
comst_1++;
		return(0);
	}
	if ((!queue_empty(&tp->t_delayed_write)) &&
	    (tp->t_outq.c_cc <= TTLOWAT(tp))) {
comst_2++;
		tt_write_wakeup(tp);
	}
	if (!tp->t_outq.c_cc) {
comst_3++;
		return(0);
	}

#if 0
	i = (comfifo[minor(tp->t_dev)]) ? /*14*/comst_5 : 1;

	tp->t_state |= TS_BUSY;
	while (i-- > 0) {
		nch = getc(&tp->t_outq);
		if (nch == -1) break;
		if ((nch & 0200) && ((tp->t_flags & LITOUT) == 0)) {
		    timeout(ttrstrt, (char *)tp, (nch & 0x7f) + 6);
		    tp->t_state |= TS_TIMEOUT;
comst_4++;
		    return(0);
		}
		outb(TXRX((int)tp->t_addr), nch);
	}
#else
	nch = getc(&tp->t_outq);
	if ((nch & 0200) && ((tp->t_flags & LITOUT) == 0)) {
	    timeout(ttrstrt, (char *)tp, (nch & 0x7f) + 6);
	    tp->t_state |= TS_TIMEOUT;
comst_4++;
	    return(0);
	}
	outb(TXRX((int)tp->t_addr), nch);
	tp->t_state |= TS_BUSY;
#endif
	return(0);
}

/* Check for stuck xmitters */
int comtimer_interval = 5;

comtimer()
{
	spl_t	s = spltty();
	struct tty *tp = com_tty;
	int i, nch;

	for (i = 0; i < NCOM; i++, tp++) {
		if ((tp->t_state & TS_ISOPEN) == 0)
			continue;
		if (!tp->t_outq.c_cc)
			continue;
		if (++comtimer_state[i] < 2)
			continue;
		/* Its stuck */
printf("Tty %x was stuck\n", tp);
		nch = getc(&tp->t_outq);
		outb(TXRX((int)tp->t_addr), nch);
	}

	splx(s);
	timeout(comtimer, 0, comtimer_interval*hz);
}

/*
 * Set receive modem state from modem status register.
 */
fix_modem_state(unit, modem_stat)
int	unit, modem_stat;
{
	int	stat = 0;

	if (modem_stat & iCTS)
	    stat |= TM_CTS;	/* clear to send */
	if (modem_stat & iDSR)
	    stat |= TM_DSR;	/* data set ready */
	if (modem_stat & iRI)
	    stat |= TM_RNG;	/* ring indicator */
	if (modem_stat & iRLSD)
	    stat |= TM_CAR;	/* carrier? */

	commodem[unit] = (commodem[unit] & ~(TM_CTS|TM_DSR|TM_RNG|TM_CAR))
				| stat;
}

/*
 * Modem change (input signals)
 */
commodem_intr(
	int	unit,
	int	stat)
{
	int	changed;

	changed = commodem[unit];
	fix_modem_state(unit, stat);
	stat = commodem[unit];

	/* Assumption: if the other party can handle
	   modem signals then it should handle all
	   the necessary ones. Else fix the cable. */

	changed ^= stat;	/* what changed ? */

	if (changed & TM_CTS)
		tty_cts( &com_tty[unit], stat & TM_CTS );

#if 0
	if (changed & TM_CAR)
		ttymodem( &com_tty[unit], stat & TM_CAR );
#endif

}

/*
 * Set/get modem bits
 */
commctl(
	register struct tty	*tp,
	int	bits,
	int	how)
{
	spl_t		s;
	int		unit;
	vm_offset_t	dev_addr;
	register int	b;

	unit = minor(tp->t_dev);

	if (bits == TM_HUP) { /* close line (internal) */
		bits = TM_DTR | TM_RTS;
		how = DMBIC;
	}

	if (how == DMGET) return commodem[unit];

	dev_addr = cominfo[unit]->address;

	s = spltty();

	switch (how) {
	case DMSET:
		b = bits; break;
	case DMBIS:
		b = commodem[unit] | bits; break;
	case DMBIC:
		b = commodem[unit] & ~bits; break;
	}
	commodem[unit] = b;

	if (bits & TM_BRK) {
		if (b & TM_BRK) {
			outb(LINE_CTL(dev_addr), inb(LINE_CTL(dev_addr)) | iSETBREAK);
		} else {
			outb(LINE_CTL(dev_addr), inb(LINE_CTL(dev_addr)) & ~iSETBREAK);
		}
	}

#if 0
	/* do I need to do something on this ? */
	if (bits & TM_LE) {	/* line enable */
	}
#endif
#if 0
	/* Unsupported */
	if (bits & TM_ST) {	/* secondary transmit */
	}
	if (bits & TM_SR) {	/* secondary receive */
	}
#endif
	if (bits & (TM_DTR|TM_RTS)) {	/* data terminal ready, request to send */
		how = iOUT2;
		if (b & TM_DTR) how |= iDTR;
		if (b & TM_RTS) how |= iRTS;
		outb(MODEM_CTL(dev_addr), how);
	}

	splx(s);

	/* the rest are inputs */
	return commodem[unit];
}

comstop(tp, flags)
register struct tty *tp;
int	flags;
{
	if ((tp->t_state & TS_BUSY) && (tp->t_state & TS_TTSTOP) == 0)
	    tp->t_state |= TS_FLUSH;
}

/*
 *
 * Code to be called from debugger.
 *
 */
void compr_addr(addr)
{
	/* The two line_stat prints may show different values, since
	*  touching some of the registers constitutes changing them.
	*/
	printf("LINE_STAT(%x) %x\n",
		LINE_STAT(addr), inb(LINE_STAT(addr)));

	printf("TXRX(%x) %x, INTR_ENAB(%x) %x, INTR_ID(%x) %x, LINE_CTL(%x) %x,\n\
MODEM_CTL(%x) %x, LINE_STAT(%x) %x, MODEM_STAT(%x) %x\n",
	TXRX(addr), 	 inb(TXRX(addr)),
	INTR_ENAB(addr), inb(INTR_ENAB(addr)),
	INTR_ID(addr), 	 inb(INTR_ID(addr)),
	LINE_CTL(addr),  inb(LINE_CTL(addr)),
	MODEM_CTL(addr), inb(MODEM_CTL(addr)),
	LINE_STAT(addr), inb(LINE_STAT(addr)),
	MODEM_STAT(addr),inb(MODEM_STAT(addr)));
}

int compr(unit)
{
	compr_addr(cominfo[unit]->address);
	return(0);
}

int
comgetc(int unit)
{
	u_short	addr = (u_short)(cominfo[unit]->address);
	spl_t	s = spltty();
	int	c;

	while((inb(LINE_STAT(addr)) & iDR) == 0) ;

	c = inb(TXRX(addr));
	splx(s);
	return c;
}

#if RCLINE >= 0
/*
 * Routines for the console
 */
int
comcnputc(dev_t dev, int c)
{
	u_short addr = (u_short)(cominfo[minor(dev)]->address);

	/* Wait for transmitter to empty */
	while((inb(LINE_STAT(addr)) & iTHRE) == 0)
		continue;

	/* send the char */
	if (c == '\n')
		comcnputc(dev, '\r');
	outb(addr, c);
}

int
comcngetc(dev_t dev, int wait)
{
	u_short	addr = (u_short)(cominfo[minor(dev)]->address);
	int	c;

	while((inb(LINE_STAT(addr)) & iDR) == 0)
		if (! wait)
			return 0;

	c = inb(TXRX(addr));
	return c & 0x7f;
}
#endif /* RCLINE */

#endif	/* NCOM */
