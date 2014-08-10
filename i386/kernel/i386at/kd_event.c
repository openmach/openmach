/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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
/* **********************************************************************
 File:         kd_event.c
 Description:  Driver for event interface to keyboard.

 $ Header: $

 Copyright Ing. C. Olivetti & C. S.p.A. 1989.  All rights reserved.
********************************************************************** */
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <mach/boolean.h>
#include <sys/types.h>
#ifdef	MACH_KERNEL
#include <device/errno.h>
#include <device/io_req.h>
#else	MACH_KERNEL
#include <sys/file.h>
#include <sys/errno.h>
#include <kern/thread.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#endif	MACH_KERNEL
#include <i386/machspl.h>
#include <i386at/kd.h>
#include <i386at/kd_queue.h>

/*
 * Code for /dev/kbd.   The interrupt processing is done in kd.c,
 * which calls into this module to enqueue scancode events when
 * the keyboard is in Event mode.
 */

/*
 * Note: These globals are protected by raising the interrupt level
 * via SPLKD.
 */

kd_event_queue kbd_queue;		/* queue of keyboard events */
#ifdef	MACH_KERNEL
queue_head_t	kbd_read_queue = { &kbd_read_queue, &kbd_read_queue };
#else	MACH_KERNEL
struct proc *kbd_sel = 0;		/* selecting process, if any */
short kbdpgrp = 0;		/* process group leader when dev is open */

int kbdflag = 0;
#define KBD_COLL	1		/* select collision */
#define KBD_ASYNC	2		/* user wants asynch notification */
#define KBD_NBIO	4		/* user wants non-blocking I/O */
#endif	MACH_KERNEL


void kbd_enqueue();
#ifdef	MACH_KERNEL
io_return_t X_kdb_enter_init();
io_return_t X_kdb_exit_init();
#endif	MACH_KERNEL

static boolean_t initialized = FALSE;


/*
 * kbdinit - set up event queue.
 */

kbdinit()
{
	spl_t s = SPLKD();
	
	if (!initialized) {
		kdq_reset(&kbd_queue);
		initialized = TRUE;
	}
	splx(s);
}


/*
 * kbdopen - Verify that open is read-only and remember process
 * group leader.
 */

/*ARGSUSED*/
kbdopen(dev, flags)
	dev_t dev;
	int flags;
{
	kbdinit();

#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	if (flags & FWRITE)
		return(ENODEV);
	
	if (kbdpgrp == 0)
		kbdpgrp = u.u_procp->p_pgrp;
#endif	MACH_KERNEL
	return(0);
}


/*
 * kbdclose - Make sure that the kd driver is in Ascii mode and
 * reset various flags.
 */

/*ARGSUSED*/
kbdclose(dev, flags)
	dev_t dev;
	int flags;
{
	spl_t s = SPLKD();

	kb_mode = KB_ASCII;
#ifdef	MACH_KERNEL
#else	MACH_KERNEL
	kbdpgrp = 0;
	kbdflag = 0;
	kbd_sel = 0;
#endif	MACH_KERNEL
	kdq_reset(&kbd_queue);
	splx(s);
}


#ifdef	MACH_KERNEL
io_return_t kbdgetstat(dev, flavor, data, count)
	dev_t		dev;
	int		flavor;
	int *		data;		/* pointer to OUT array */
	unsigned int	*count;		/* OUT */
{
	io_return_t	result;

	switch (flavor) {
	    case KDGKBDTYPE:
		*data = KB_VANILLAKB;
		*count = 1;
		break;
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

io_return_t kbdsetstat(dev, flavor, data, count)
	dev_t		dev;
	int		flavor;
	int *		data;
	unsigned int	count;
{
	io_return_t	result;

	switch (flavor) {
	    case KDSKBDMODE:
		kb_mode = *data;
		/* XXX - what to do about unread events? */
		/* XXX - should check that 'data' contains an OK valud */
		break;
	    case K_X_KDB_ENTER:
		return X_kdb_enter_init(data, count);
	    case K_X_KDB_EXIT:
		return X_kdb_exit_init(data, count);
	    default:
		return (D_INVALID_OPERATION);
	}
	return (D_SUCCESS);
}

#else	MACH_KERNEL
/*
 * kbdioctl - handling for asynch & non-blocking I/O.
 */

/*ARGSUSED*/
kbdioctl(dev, cmd, data, flag)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
{
	spl_t s = SPLKD();
	int err = 0;

	switch (cmd) {
	case KDSKBDMODE:
		kb_mode = *(int *)data;
		/* XXX - what to do about unread events? */
		/* XXX - should check that "data" contains an OK value */
		break;
	case KDGKBDTYPE:
		*(int *)data = KB_VANILLAKB;
		break;
	case K_X_KDB_ENTER:
		X_kdb_enter_init((struct X_kdb *) data);
		break;
	case K_X_KDB_EXIT:
		X_kdb_exit_init( (struct X_kdb *) data);
		break;
	case FIONBIO:
		if (*(int *)data)
			kbdflag |= KBD_NBIO;
		else
			kbdflag &= ~KBD_NBIO;
		break;
	case FIOASYNC:
		if (*(int *)data)
			kbdflag |= KBD_ASYNC;
		else
			kbdflag &= ~KBD_ASYNC;
		break;
	default:
		err = ENOTTY;
		break;
	}

	splx(s);
	return(err);
}


/*
 * kbdselect
 */

/*ARGSUSED*/
kbdselect(dev, rw)
{
	spl_t s = SPLKD();

	if (!kdq_empty(&kbd_queue)) {
		splx(s);
		return(1);
	}

	if (kbd_sel)
		kbdflag |= KBD_COLL;
	else
		kbd_sel = (struct proc *)current_thread();
					/* eeeyuck */
	
	splx(s);
	return(0);
}
#endif	MACH_KERNEL


/*
 * kbdread - dequeue and return any queued events.
 */

#ifdef	MACH_KERNEL
boolean_t	kbd_read_done();	/* forward */

kbdread(dev, ior)
	dev_t	dev;
	register io_req_t	ior;
{
	register int	err, count;
	register spl_t	s;

	err = device_read_alloc(ior, (vm_size_t)ior->io_count);
	if (err != KERN_SUCCESS)
	    return (err);

	s = SPLKD();
	if (kdq_empty(&kbd_queue)) {
	    if (ior->io_mode & D_NOWAIT) {
		splx(s);
		return (D_WOULD_BLOCK);
	    }
	    ior->io_done = kbd_read_done;
	    enqueue_tail(&kbd_read_queue, (queue_entry_t) ior);
	    splx(s);
	    return (D_IO_QUEUED);
	}
	count = 0;
	while (!kdq_empty(&kbd_queue) && count < ior->io_count) {
	    register kd_event *ev;

	    ev = kdq_get(&kbd_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);
	ior->io_residual = ior->io_count - count;
	return (D_SUCCESS);
}

boolean_t kbd_read_done(ior)
	register io_req_t	ior;
{
	register int	count;
	register spl_t	s;

	s = SPLKD();
	if (kdq_empty(&kbd_queue)) {
	    ior->io_done = kbd_read_done;
	    enqueue_tail(&kbd_read_queue, (queue_entry_t)ior);
	    splx(s);
	    return (FALSE);
	}

	count = 0;
	while (!kdq_empty(&kbd_queue) && count < ior->io_count) {
	    register kd_event *ev;

	    ev = kdq_get(&kbd_queue);
	    *(kd_event *)(&ior->io_data[count]) = *ev;
	    count += sizeof(kd_event);
	}
	splx(s);

	ior->io_residual = ior->io_count - count;
	ds_read_done(ior);

	return (TRUE);
}

#else	MACH_KERNEL
/*ARGSUSED*/
kbdread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	int s = SPLKD();
	int err = 0;
	kd_event *ev;
	int i;
	char *cp;

	if (kdq_empty(&kbd_queue))
		if (kbdflag & KBD_NBIO) {
			err = EWOULDBLOCK;
			goto done;
		} else 
			while (kdq_empty(&kbd_queue)) {
				splx(s);
				sleep((caddr_t)&kbd_queue, TTIPRI);
				s = SPLKD();
			}

	while (!kdq_empty(&kbd_queue) && uio->uio_resid >= sizeof(kd_event)) {
		ev = kdq_get(&kbd_queue);
		for (cp = (char *)ev, i = 0; i < sizeof(kd_event);
		     ++i, ++cp) {
			err = ureadc(*cp, uio);
			if (err)
				goto done;
		}
	}

done:
	splx(s);
	return(err);
}
#endif	MACH_KERNEL


/*
 * kd_enqsc - enqueue a scancode.  Should be called at SPLKD.
 */

void
kd_enqsc(sc)
	Scancode sc;
{
	kd_event ev;

	ev.type = KEYBD_EVENT;
	ev.time = time;
	ev.value.sc = sc;
	kbd_enqueue(&ev);
}


/*
 * kbd_enqueue - enqueue an event and wake up selecting processes, if
 * any.  Should be called at SPLKD.
 */

void
kbd_enqueue(ev)
	kd_event *ev;
{
	if (kdq_full(&kbd_queue))
		printf("kbd: queue full\n");
	else
		kdq_put(&kbd_queue, ev);

#ifdef	MACH_KERNEL
	{
	    register io_req_t	ior;
	    while ((ior = (io_req_t)dequeue_head(&kbd_read_queue)) != 0)
		iodone(ior);
	}
#else	MACH_KERNEL
	if (kbd_sel) {
		selwakeup(kbd_sel, kbdflag & KBD_COLL);
		kbd_sel = 0;
		kbdflag &= ~KBD_COLL;
	}
	if (kbdflag & KBD_ASYNC)
		gsignal(kbdpgrp, SIGIO);
	wakeup((caddr_t)&kbd_queue);
#endif	MACH_KERNEL
}

u_int X_kdb_enter_str[512], X_kdb_exit_str[512];
int   X_kdb_enter_len = 0,  X_kdb_exit_len = 0;

kdb_in_out(p)
u_int *p;
{
register int t = p[0];

	switch (t & K_X_TYPE) {
		case K_X_IN|K_X_BYTE:
			inb(t & K_X_PORT);
			break;

		case K_X_IN|K_X_WORD:
			inw(t & K_X_PORT);
			break;

		case K_X_IN|K_X_LONG:
			inl(t & K_X_PORT);
			break;

		case K_X_OUT|K_X_BYTE:
			outb(t & K_X_PORT, p[1]);
			break;

		case K_X_OUT|K_X_WORD:
			outw(t & K_X_PORT, p[1]);
			break;

		case K_X_OUT|K_X_LONG:
			outl(t & K_X_PORT, p[1]);
			break;
	}
}

X_kdb_enter()
{
register u_int *u_ip, *endp;

	for (u_ip = X_kdb_enter_str, endp = &X_kdb_enter_str[X_kdb_enter_len];
	     u_ip < endp;
	     u_ip += 2)
	    kdb_in_out(u_ip);
}

X_kdb_exit()
{
register u_int *u_ip, *endp;

	for (u_ip = X_kdb_exit_str, endp = &X_kdb_exit_str[X_kdb_exit_len];
	     u_ip < endp;
	     u_ip += 2)
	   kdb_in_out(u_ip);
}

#ifdef	MACH_KERNEL
io_return_t
X_kdb_enter_init(data, count)
    u_int *data;
    u_int count;
{
    if (count * sizeof X_kdb_enter_str[0] > sizeof X_kdb_enter_str)
	return D_INVALID_OPERATION;

    bcopy(data, X_kdb_enter_str, count * sizeof X_kdb_enter_str[0]);
    X_kdb_enter_len = count;
    return D_SUCCESS;
}

io_return_t
X_kdb_exit_init(data, count)
    u_int *data;
    u_int count;
{
    if (count * sizeof X_kdb_exit_str[0] > sizeof X_kdb_exit_str)
	return D_INVALID_OPERATION;

    bcopy(data, X_kdb_exit_str, count * sizeof X_kdb_exit_str[0]);
    X_kdb_exit_len = count;
    return D_SUCCESS;
}
#else	MACH_KERNEL
X_kdb_enter_init(kp)
struct X_kdb *kp;
{
	if (kp->size > sizeof X_kdb_enter_str)
		u.u_error = ENOENT;
	else if(copyin(kp->ptr, X_kdb_enter_str, kp->size) == EFAULT)
		u.u_error = EFAULT;

	X_kdb_enter_len = kp->size>>2;
}

X_kdb_exit_init(kp)
struct X_kdb *kp;
{
	if (kp->size > sizeof X_kdb_exit_str)
		u.u_error = ENOENT;
	else if(copyin(kp->ptr, X_kdb_exit_str, kp->size) == EFAULT)
		u.u_error = EFAULT;

	X_kdb_exit_len = kp->size>>2;
}
#endif	MACH_KERNEL
