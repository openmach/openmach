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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	7/90
 *
 * 	Compatibility TTY structure for existing TTY device drivers.
 */

#ifndef	_DEVICE_TTY_H_
#define	_DEVICE_TTY_H_

#include <kern/lock.h>
#include <kern/queue.h>
#include <mach/port.h>

#include <device/device_types.h>
#include <device/tty_status.h>
#include <device/cirbuf.h>
#include <device/io_req.h>

#ifdef	luna88k
#include <luna88k/jtermio.h>
#endif

struct tty {
	decl_simple_lock_data(,t_lock)
	struct cirbuf	t_inq;		/* input buffer */
	struct cirbuf	t_outq;		/* output buffer */
	char *		t_addr;		/* device pointer */
	int		t_dev;		/* device number */
	int		(*t_start)(struct tty *);
					/* routine to start output */
#define	t_oproc	t_start
	int		(*t_stop)(struct tty *, int);
					/* routine to stop output */
	int		(*t_mctl)(struct tty *, int, int);
					/* (optional) routine to control
					   modem signals */
	char		t_ispeed;	/* input speed */
	char		t_ospeed;	/* output speed */
	char		t_breakc;	/* character to deliver when 'break'
					   condition received */
	int		t_flags;	/* mode flags */
	int		t_state;	/* current state */
	int		t_line;		/* fake line discipline number,
					   for old drivers - always 0 */
	queue_head_t	t_delayed_read;	/* pending read requests */
	queue_head_t	t_delayed_write;/* pending write requests */
	queue_head_t	t_delayed_open;	/* pending open requests */

/*
 * Items beyond this point should be removed to device-specific
 * extension structures.
 */
	int		(*t_getstat)();	/* routine to get status */
	int		(*t_setstat)();	/* routine to set status */
	dev_ops_t	t_tops;		/* another device to possibly
					   push through */
};
typedef struct tty	*tty_t;

/*
 * Common TTY service routines
 */
extern io_return_t char_open(
	int		dev,
	struct tty *	tp,
	dev_mode_t	mode,
	io_req_t	ior);

extern io_return_t char_read(
	struct tty *	tp,
	io_req_t	ior);

extern io_return_t char_write(
	struct tty *	tp,
	io_req_t	ior);

extern void ttyinput(
	unsigned int	c,
	struct tty *	tp);

extern boolean_t ttymodem(
	struct tty *	tp,
	boolean_t	carrier_up);

extern void tty_queue_completion(
	queue_t		queue);
#define	tt_open_wakeup(tp) \
	(tty_queue_completion(&(tp)->t_delayed_open))
#define	tt_write_wakeup(tp) \
	(tty_queue_completion(&(tp)->t_delayed_write))

extern void ttychars(
	struct tty *	tp);

#define	TTMINBUF	90

short	tthiwat[NSPEEDS], ttlowat[NSPEEDS];
#define	TTHIWAT(tp)	tthiwat[(tp)->t_ospeed]
#define	TTLOWAT(tp)	ttlowat[(tp)->t_ospeed]

/* internal state bits */
#define	TS_INIT		0x00000001	/* tty structure initialized */
#define	TS_TIMEOUT	0x00000002	/* delay timeout in progress */
#define	TS_WOPEN	0x00000004	/* waiting for open to complete */
#define	TS_ISOPEN	0x00000008	/* device is open */
#define	TS_FLUSH	0x00000010	/* outq has been flushed during DMA */
#define	TS_CARR_ON	0x00000020	/* software copy of carrier-present */
#define	TS_BUSY		0x00000040	/* output in progress */
#define	TS_ASLEEP	0x00000080	/* wakeup when output done */

#define	TS_TTSTOP	0x00000100	/* output stopped by ctl-s */
#define	TS_HUPCLS	0x00000200	/* hang up upon last close */
#define	TS_TBLOCK	0x00000400	/* tandem queue blocked */

#define	TS_NBIO		0x00001000	/* tty in non-blocking mode */
#define	TS_ONDELAY	0x00002000	/* device is open; software copy of 
 					 * carrier is not present */
#define	TS_MIN		0x00004000	/* buffer input chars, if possible */
#define	TS_MIN_TO	0x00008000	/* timeout for the above is active */

#define TS_OUT          0x00010000	/* tty in use for dialout only */
#define	TS_RTS_DOWN	0x00020000	/* modem pls stop */

#define TS_TRANSLATE	0x00100000	/* translation device enabled */
#define TS_KDB		0x00200000	/* should enter kdb on ALT */

#define	TS_MIN_TO_RCV	0x00400000	/* character recived during 
					   receive timeout interval */

/* flags - old names defined in terms of new ones */

#define	TANDEM		TF_TANDEM
#define	ODDP		TF_ODDP
#define	EVENP		TF_EVENP
#define	ANYP		(ODDP|EVENP)
#define	MDMBUF		TF_MDMBUF
#define	LITOUT		TF_LITOUT
#define	NOHANG		TF_NOHANG

#define	ECHO		TF_ECHO
#define	CRMOD		TF_CRMOD
#define	XTABS		TF_XTABS

/* these are here only to let old code compile - they are never set */
#define	RAW		LITOUT
#define	PASS8		LITOUT

/*
 * Hardware bits.
 * SHOULD NOT BE HERE.
 */
#define	DONE	0200
#define	IENABLE	0100

/*
 * Modem control commands.
 */
#define	DMSET		0
#define	DMBIS		1
#define	DMBIC		2
#define	DMGET		3

/*
 * Fake 'line discipline' switch, for the benefit of old code
 * that wants to call through it.
 */
struct ldisc_switch {
	int	(*l_read) (struct tty *, io_req_t);	/* read */
	int	(*l_write)(struct tty *, io_req_t);	/* write */
	void	(*l_rint) (unsigned int, struct tty *);	/* character input */
	boolean_t (*l_modem)(struct tty *, boolean_t);	/* modem change */
	void	(*l_start)(struct tty *);		/* start output */
};

extern struct ldisc_switch	linesw[];

#endif	/* _DEVICE_TTY_H_ */
