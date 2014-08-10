/* 
 * Mach Operating System
 * Copyright (c) 1993-1988 Carnegie Mellon University
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
 *	Date: 	8/88
 *
 * 	TTY io.
 * 	Compatibility with old TTY device drivers.
 */

#include <mach/kern_return.h>
#include <mach/mig_errors.h>
#include <mach/vm_param.h>
#include <machine/machspl.h>		/* spl definitions */

#include <ipc/ipc_port.h>

#include <kern/lock.h>
#include <kern/queue.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <device/device_types.h>
#include <device/io_req.h>
#include <device/ds_routines.h>
#include <device/device_reply.h>

#include <device/tty.h>

/* If you change these, check that tty_outq_size and tty_inq_size
 * is greater than largest tthiwat entry.
 */
short	tthiwat[16] =
   { 100,100,100,100,100,100,100,200,200,400,400,400,650,650,1300,2000 };
short	ttlowat[16] =
   {  30, 30, 30, 30, 30, 30, 30, 50, 50,120,120,120,125,125, 125, 125 };

/*
 * forward declarations
 */
void	queue_delayed_reply(
	queue_t, io_req_t, boolean_t (*)(io_req_t));
void	tty_output(struct tty *);
void	tty_flush(struct tty *, int);
boolean_t char_open_done(io_req_t);
boolean_t char_read_done(io_req_t);
boolean_t char_write_done(io_req_t);

/*
 * Fake 'line discipline' switch for the benefit of old code
 * that wants to call through it.
 */
struct ldisc_switch	linesw[] = {
	{
	    char_read,
	    char_write,
	    ttyinput,
	    ttymodem,
	    tty_output
	}
};

/*
 * Sizes for input and output circular buffers.
 */
int	tty_inq_size = 4096;	/* big nuf */
int	tty_outq_size = 2048;	/* Must be bigger that tthiwat */
int	pdma_default = 1;       /* turn pseudo dma on by default */

/*
 * compute pseudo-dma tables 
 */

int pdma_timeouts[NSPEEDS]; /* how many ticks in timeout */
int pdma_water_mark[NSPEEDS];


void chario_init(void)
{
  /* the basic idea with the timeouts is two allow enough
     time for a character to show up if data is coming in at full data rate
     plus a little slack. 2 ticks is considered slack
     Below 300 baud we just glob a character at a time */
#define _PR(x) ((hz/x) + 2)

  int i;

  for (i = B0; i < B300; i++)
    pdma_timeouts[i] = 0;
  
  pdma_timeouts[B300] = _PR(30);
  pdma_timeouts[B600] = _PR(60);
  pdma_timeouts[B1200] = _PR(120);
  pdma_timeouts[B1800] = _PR(180);
  pdma_timeouts[B2400] = _PR(240);
  pdma_timeouts[B4800] = _PR(480);
  pdma_timeouts[B9600] = _PR(960);
  pdma_timeouts[EXTA]  = _PR(1440); /* >14400 baud */
  pdma_timeouts[EXTB]  = _PR(1920); /* >19200 baud */

  for (i = B0; i < B300; i++)
    pdma_water_mark[i] = 0;

  /* for the slow speeds, we try to buffer 0.02 of the baud rate
     (20% of the character rate). For the faster lines,
     we try to buffer 1/2 the input queue size */

#undef _PR
#define _PR(x) (0.20 * x)

  pdma_water_mark[B300] = _PR(120);
  pdma_water_mark[B600] = _PR(120);
  pdma_water_mark[B1200] = _PR(120);
  pdma_water_mark[B1800] = _PR(180);
  pdma_water_mark[B2400] = _PR(240);
  pdma_water_mark[B4800] = _PR(480);
  i = tty_inq_size/2;
  pdma_water_mark[B9600] = i;
  pdma_water_mark[EXTA]  = i; /* >14400 baud */
  pdma_water_mark[EXTB]  = i; /* >19200 baud */

  return; 
}

/*
 * Open TTY, waiting for CARR_ON.
 * No locks may be held.
 * May run on any CPU.
 */
io_return_t char_open(
	int		dev,
	struct tty *	tp,
	dev_mode_t	mode,
	io_req_t	ior)
{
	spl_t	s;
	io_return_t	rc = D_SUCCESS;

	s = spltty();
	simple_lock(&tp->t_lock);

	tp->t_dev = dev;

	if (tp->t_mctl)
		(*tp->t_mctl)(tp, TM_DTR, DMSET);

	if (pdma_default)
	  tp->t_state |= TS_MIN;

	if ((tp->t_state & TS_CARR_ON) == 0) {
	    /*
	     * No carrier.
	     */
	    if (mode & D_NODELAY) {
		tp->t_state |= TS_ONDELAY;
	    }
	    else {
		/*
		 * Don`t return from open until carrier detected.
		 */
		tp->t_state |= TS_WOPEN;

		ior->io_dev_ptr = (char *)tp;

		queue_delayed_reply(&tp->t_delayed_open, ior, char_open_done);
		rc = D_IO_QUEUED;
		goto out;
	    }
	}
	tp->t_state |= TS_ISOPEN;
	if (tp->t_mctl)
		(*tp->t_mctl)(tp, TM_RTS, DMBIS);
out:
	simple_unlock(&tp->t_lock);
	splx(s);
	return rc;
}

/*
 * Retry wait for CARR_ON for open.
 * No locks may be held.
 * May run on any CPU.
 */
boolean_t char_open_done(
	io_req_t	ior)
{
	register struct tty *tp = (struct tty *)ior->io_dev_ptr;
	spl_t s = spltty();

	simple_lock(&tp->t_lock);
	if ((tp->t_state & TS_ISOPEN) == 0) {
	    queue_delayed_reply(&tp->t_delayed_open, ior, char_open_done);
	    simple_unlock(&tp->t_lock);
	    splx(s);
	    return FALSE;
	}

	tp->t_state |= TS_ISOPEN;
	tp->t_state &= ~TS_WOPEN;

	if (tp->t_mctl)
		(*tp->t_mctl)(tp, TM_RTS, DMBIS);

	simple_unlock(&tp->t_lock);
	splx(s);

	ior->io_error = D_SUCCESS;
	(void) ds_open_done(ior);
	return TRUE;
}

boolean_t tty_close_open_reply(
	io_req_t	ior)
{
	ior->io_error = D_DEVICE_DOWN;
	(void) ds_open_done(ior);
	return TRUE;
}

/*
 * Write to TTY.
 * No locks may be held.
 * Calls device start routine; must already be on master if
 * device needs to run on master.
 */
io_return_t char_write(
	register struct tty *	tp,
	register io_req_t	ior)
{
	spl_t		s;
	register int	count;
	register char	*data;
	vm_offset_t	addr;
	io_return_t	rc = D_SUCCESS;

	data  = ior->io_data;
	count = ior->io_count;
	if (count == 0)
	    return rc;

	if (!(ior->io_op & IO_INBAND)) {
	    /*
	     * Copy out-of-line data into kernel address space.
	     * Since data is copied as page list, it will be
	     * accessible.
	     */
	    vm_map_copy_t copy = (vm_map_copy_t) data;
	    kern_return_t kr;

	    kr = vm_map_copyout(device_io_map, &addr, copy);
	    if (kr != KERN_SUCCESS)
		return kr;
	    data = (char *) addr;
	}

	/*
	 * Check for tty operating.
	 */
	s = spltty();
	simple_lock(&tp->t_lock);

	if ((tp->t_state & TS_CARR_ON) == 0) {

	    if ((tp->t_state & TS_ONDELAY) == 0) {
		/*
		 * No delayed writes - tell caller that device is down
		 */
		rc = D_IO_ERROR;
		goto out;
	    }

	    if (ior->io_mode & D_NOWAIT) {
		rc = D_WOULD_BLOCK;
		goto out;
	    }
	}

	/*
	 * Copy data into the output buffer.
	 * Report the amount not copied.
	 */

	ior->io_residual = b_to_q(data, count, &tp->t_outq);

	/*
	 * Start hardware output.
	 */

	tp->t_state &= ~TS_TTSTOP;
	tty_output(tp);

	if (tp->t_outq.c_cc > TTHIWAT(tp) ||
	    (tp->t_state & TS_CARR_ON) == 0) {

	    /*
	     * Do not send reply until some characters have been sent.
	     */
	    ior->io_dev_ptr = (char *)tp;
	    queue_delayed_reply(&tp->t_delayed_write, ior, char_write_done);

	    rc = D_IO_QUEUED;
	}
out:
	simple_unlock(&tp->t_lock);
	splx(s);

	if (!(ior->io_op & IO_INBAND))
	    (void) vm_deallocate(device_io_map, addr, ior->io_count);
	return rc;
}

/*
 * Retry wait for output queue emptied, for write.
 * No locks may be held.
 * May run on any CPU.
 */
boolean_t char_write_done(
	register io_req_t	ior)
{
	register struct tty *tp = (struct tty *)ior->io_dev_ptr;
	register spl_t s = spltty();

	simple_lock(&tp->t_lock);
	if (tp->t_outq.c_cc > TTHIWAT(tp) ||
	    (tp->t_state & TS_CARR_ON) == 0) {

	    queue_delayed_reply(&tp->t_delayed_write, ior, char_write_done);
	    simple_unlock(&tp->t_lock);
	    splx(s);
	    return FALSE;
	}
	simple_unlock(&tp->t_lock);
	splx(s);

	if (IP_VALID(ior->io_reply_port)) {
	  (void) (*((ior->io_op & IO_INBAND) ?
		    ds_device_write_reply_inband :
		    ds_device_write_reply))(ior->io_reply_port,
					    ior->io_reply_port_type,
					    ior->io_error,
					    (int) (ior->io_total -
						   ior->io_residual));
	}
	mach_device_deallocate(ior->io_device);
	return TRUE;
}

boolean_t tty_close_write_reply(
	register io_req_t	ior)
{
	ior->io_residual = ior->io_count;
	ior->io_error = D_DEVICE_DOWN;
	(void) ds_write_done(ior);
	return TRUE;
}

/*
 * Read from TTY.
 * No locks may be held.
 * May run on any CPU - does not talk to device driver.
 */
io_return_t char_read(
	register struct tty *tp,
	register io_req_t ior)
{
	spl_t		s;
	kern_return_t	rc;

	/*
	 * Allocate memory for read buffer.
	 */
	rc = device_read_alloc(ior, (vm_size_t)ior->io_count);
	if (rc != KERN_SUCCESS)
	    return rc;

	s = spltty();
	simple_lock(&tp->t_lock);
	if ((tp->t_state & TS_CARR_ON) == 0) {

	    if ((tp->t_state & TS_ONDELAY) == 0) {
		/*
		 * No delayed writes - tell caller that device is down
		 */
		rc = D_IO_ERROR;
		goto out;
	    }

	    if (ior->io_mode & D_NOWAIT) {
		rc = D_WOULD_BLOCK;
		goto out;
	    }

	}

	if (tp->t_inq.c_cc <= 0 ||
	    (tp->t_state & TS_CARR_ON) == 0) {

	    ior->io_dev_ptr = (char *)tp;
	    queue_delayed_reply(&tp->t_delayed_read, ior, char_read_done);
	    rc = D_IO_QUEUED;
	    goto out;
	}
	
	ior->io_residual = ior->io_count - q_to_b(&tp->t_inq,
						  ior->io_data,
						  (int)ior->io_count);
	if (tp->t_state & TS_RTS_DOWN) {
	    (*tp->t_mctl)(tp, TM_RTS, DMBIS);
	    tp->t_state &= ~TS_RTS_DOWN;
	}

    out:
	simple_unlock(&tp->t_lock);
	splx(s);
	return rc;
}

/*
 * Retry wait for characters, for read.
 * No locks may be held.
 * May run on any CPU - does not talk to device driver.
 */
boolean_t char_read_done(
	register io_req_t	ior)
{
	register struct tty *tp = (struct tty *)ior->io_dev_ptr;
	register spl_t s = spltty();

	simple_lock(&tp->t_lock);

	if (tp->t_inq.c_cc <= 0 ||
	    (tp->t_state & TS_CARR_ON) == 0) {

	    queue_delayed_reply(&tp->t_delayed_read, ior, char_read_done);
	    simple_unlock(&tp->t_lock);
	    splx(s);
	    return FALSE;
	}

	ior->io_residual = ior->io_count - q_to_b(&tp->t_inq,
						  ior->io_data,
						  (int)ior->io_count);
	if (tp->t_state & TS_RTS_DOWN) {
	    (*tp->t_mctl)(tp, TM_RTS, DMBIS);
	    tp->t_state &= ~TS_RTS_DOWN;
	}

	simple_unlock(&tp->t_lock);
	splx(s);

	(void) ds_read_done(ior);
	return TRUE;
}

boolean_t tty_close_read_reply(
	register io_req_t	ior)
{
	ior->io_residual = ior->io_count;
	ior->io_error = D_DEVICE_DOWN;
	(void) ds_read_done(ior);
	return TRUE;
}

/*
 * Close the tty.
 * Tty must be locked (at spltty).
 * Iff modem control should run on master.
 */
void ttyclose(
	register struct tty *tp)
{
	register io_req_t	ior;

	/*
	 * Flush the read and write queues.  Signal
	 * the open queue so that those waiting for open
	 * to complete will see that the tty is closed.
	 */
	while ((ior = (io_req_t)dequeue_head(&tp->t_delayed_read)) != 0) {
	    ior->io_done = tty_close_read_reply;
	    iodone(ior);
	}
	while ((ior = (io_req_t)dequeue_head(&tp->t_delayed_write)) != 0) {
	    ior->io_done = tty_close_write_reply;
	    iodone(ior);
	}
	while ((ior = (io_req_t)dequeue_head(&tp->t_delayed_open)) != 0) {
	    ior->io_done = tty_close_open_reply;
	    iodone(ior);
	}

	/* Close down modem */
	if (tp->t_mctl) {
		(*tp->t_mctl)(tp, TM_BRK|TM_RTS, DMBIC);
		if ((tp->t_state&(TS_HUPCLS|TS_WOPEN)) || (tp->t_state&TS_ISOPEN)==0)
			(*tp->t_mctl)(tp, TM_HUP, DMSET);
	}

	/* only save buffering bit, and carrier */
	tp->t_state = tp->t_state & (TS_MIN|TS_CARR_ON);
}

/*
 * Port-death routine to clean up reply messages.
 */
boolean_t
tty_queue_clean(
	queue_t		q,
	ipc_port_t	port,
	boolean_t	(*routine)(io_req_t) )
{
	register io_req_t	ior;

	ior = (io_req_t)queue_first(q);
	while (!queue_end(q, (queue_entry_t)ior)) {
	    if (ior->io_reply_port == port) {
		remqueue(q, (queue_entry_t)ior);
		ior->io_done = routine;
		iodone(ior);
		return TRUE;
	    }
	    ior = ior->io_next;
	}
	return FALSE;
}

/*
 * Handle port-death (dead reply port) for tty.
 * No locks may be held.
 * May run on any CPU.
 */
boolean_t
tty_portdeath(
	struct tty *	tp,
	ipc_port_t	port)
{
	register spl_t	spl = spltty();
	register boolean_t	result;

	simple_lock(&tp->t_lock);

	/*
	 * The queues may never have been initialized
	 */
	if (tp->t_delayed_read.next == 0) {
	    result = FALSE;
	}
	else {
	    result =
		tty_queue_clean(&tp->t_delayed_read,  port,
				tty_close_read_reply)
	     || tty_queue_clean(&tp->t_delayed_write, port,
				tty_close_write_reply)
	     || tty_queue_clean(&tp->t_delayed_open,  port,
				tty_close_open_reply);
	}
	simple_unlock(&tp->t_lock);
	splx(spl);

	return result;
}

/*
 * Get TTY status.
 * No locks may be held.
 * May run on any CPU.
 */
io_return_t tty_get_status(
	register struct tty *tp,
	dev_flavor_t	flavor,
	int *		data,		/* pointer to OUT array */
	natural_t	*count)		/* out */
{
	spl_t		s;

	switch (flavor) {
	    case TTY_STATUS:
	    {
		register struct tty_status *tsp =
			(struct tty_status *) data;

               if (*count < TTY_STATUS_COUNT)
                   return (D_INVALID_OPERATION);

		s = spltty();
		simple_lock(&tp->t_lock);

		tsp->tt_ispeed = tp->t_ispeed;
		tsp->tt_ospeed = tp->t_ospeed;
		tsp->tt_breakc = tp->t_breakc;
		tsp->tt_flags  = tp->t_flags;
		if (tp->t_state & TS_HUPCLS)
		    tsp->tt_flags |= TF_HUPCLS;

		simple_unlock(&tp->t_lock);
		splx(s);

		*count = TTY_STATUS_COUNT;
		break;

	    }
	    default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}

/*
 * Set TTY status.
 * No locks may be held.
 * Calls device start or stop routines; must already be on master if
 * device needs to run on master.
 */
io_return_t tty_set_status(
	register struct tty *tp,
	dev_flavor_t	flavor,
	int *		data,
	natural_t	count)
{
	int	s;

	switch (flavor) {
	    case TTY_FLUSH:
	    {
		register int	flags;
		if (count < TTY_FLUSH_COUNT)
		    return D_INVALID_OPERATION;

		flags = *data;
		if (flags == 0)
		    flags = D_READ | D_WRITE;

		s = spltty();
		simple_lock(&tp->t_lock);
		tty_flush(tp, flags);
		simple_unlock(&tp->t_lock);
		splx(s);

		break;
	    }
	    case TTY_STOP:
		/* stop output */
		s = spltty();
		simple_lock(&tp->t_lock);
		if ((tp->t_state & TS_TTSTOP) == 0) {
		    tp->t_state |= TS_TTSTOP;
		    (*tp->t_stop)(tp, 0);
		}
		simple_unlock(&tp->t_lock);
		splx(s);
		break;

	    case TTY_START:
		/* start output */
		s = spltty();
		simple_lock(&tp->t_lock);
		if (tp->t_state & TS_TTSTOP) {
		    tp->t_state &= ~TS_TTSTOP;
		    tty_output(tp);
		}
		simple_unlock(&tp->t_lock);
		splx(s);
		break;

	    case TTY_STATUS:
		/* set special characters and speed */
	    {
		register struct tty_status *tsp;

		if (count < TTY_STATUS_COUNT)
		    return D_INVALID_OPERATION;

		tsp = (struct tty_status *)data;

		if (tsp->tt_ispeed < 0 ||
		    tsp->tt_ispeed >= NSPEEDS ||
		    tsp->tt_ospeed < 0 ||
		    tsp->tt_ospeed >= NSPEEDS)
		{
		    return D_INVALID_OPERATION;
		}

		s = spltty();
		simple_lock(&tp->t_lock);

		tp->t_ispeed = tsp->tt_ispeed;
		tp->t_ospeed = tsp->tt_ospeed;
		tp->t_breakc = tsp->tt_breakc;
		tp->t_flags  = tsp->tt_flags & ~TF_HUPCLS;
		if (tsp->tt_flags & TF_HUPCLS)
		    tp->t_state |= TS_HUPCLS;

		simple_unlock(&tp->t_lock);
		splx(s);
		break;
	    }
	    default:
		return D_INVALID_OPERATION;
	}
	return D_SUCCESS;
}


/*
 * [internal]
 * Queue IOR on reply queue, to wait for TTY operation.
 * TTY must be locked (at spltty).
 */
void queue_delayed_reply(
	queue_t		qh,
	io_req_t	ior,
	boolean_t	(*io_done)(io_req_t) )
{
	ior->io_done = io_done;
	enqueue_tail(qh, (queue_entry_t)ior);
}

/*
 * Retry delayed IO operations for TTY.
 * TTY containing queue must be locked (at spltty).
 */
void tty_queue_completion(
	register queue_t	qh)
{
	register io_req_t	ior;

	while ((ior = (io_req_t)dequeue_head(qh)) != 0) {
	    iodone(ior);
	}
}

/*
 * Set the default special characters.
 * Since this routine is called whenever a tty has never been opened,
 * we can initialize the queues here.
 */
void ttychars(
	register struct tty *tp)
{
	if ((tp->t_flags & TS_INIT) == 0) {
	    /*
	     * Initialize queues
	     */
	    queue_init(&tp->t_delayed_open);
	    queue_init(&tp->t_delayed_read);
	    queue_init(&tp->t_delayed_write);

	    /*
	     * Initialize character buffers
	     */
	    cb_alloc(&tp->t_inq,  tty_inq_size);

	    /* if we might do modem flow control */
	    if (tp->t_mctl && tp->t_inq.c_hog > 30)
	    	tp->t_inq.c_hog -= 30;

	    cb_alloc(&tp->t_outq, tty_outq_size);

	    /*
	     * Mark initialized
	     */
	    tp->t_state |= TS_INIT;
	}

	tp->t_breakc = 0;
}

/*
 * Flush all TTY queues.
 * Called at spltty, tty already locked.
 * Calls device STOP routine; must already be on master if
 * device needs to run on master.
 */
void tty_flush(
	register struct tty *tp,
	int	rw)
{
	if (rw & D_READ) {
	    cb_clear(&tp->t_inq);
	    tty_queue_completion(&tp->t_delayed_read);
	}
	if (rw & D_WRITE) {
	    tp->t_state &= ~TS_TTSTOP;
	    (*tp->t_stop)(tp, rw);
	    cb_clear(&tp->t_outq);
	    tty_queue_completion(&tp->t_delayed_write);
	}
}
		
/*
 * Restart character output after a delay timeout.
 * Calls device start routine - must be on master CPU.
 *
 *	Timeout routines are called only on master CPU.
 *	What if device runs on a different CPU?
 */
void ttrstrt(
	register struct tty *tp)
{
	register spl_t	s;

	s = spltty();
	simple_lock(&tp->t_lock);

	tp->t_state &= ~TS_TIMEOUT;
	ttstart (tp);

	simple_unlock(&tp->t_lock);
        splx(s);
}

/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 *
 * Called at spltty, tty already locked.
 * Must be on master CPU if device runs on master.
 */
void ttstart(tp)
	register struct tty *tp;
{
	if ((tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) == 0) {
	    /*
	     * Start up the hardware again
	     */
	    (*tp->t_start)(tp);

	    /*
	     * Wake up those waiting for write completion.
	     */
	    if (tp->t_outq.c_cc <= TTLOWAT(tp))
		tty_queue_completion(&tp->t_delayed_write);
	}
}

/*
 * Start character output, if the device is not busy or
 * stopped or waiting for a timeout.
 *
 * Called at spltty, tty already locked.
 * Must be on master CPU if device runs on master.
 */
void tty_output(
	register struct tty *tp)
{
	if ((tp->t_state & (TS_TIMEOUT|TS_TTSTOP|TS_BUSY)) == 0) {
	    /*
	     * Not busy.  Start output.
	     */
	    (*tp->t_start)(tp);

	    /*
	     * Wake up those waiting for write completion.
	     */
	    if (tp->t_outq.c_cc <= TTLOWAT(tp))
		tty_queue_completion(&tp->t_delayed_write);
	}
}

/*
 * Send any buffered recvd chars up to user
 */
void ttypush(
	register struct tty	*tp)
{
	spl_t	s = spltty();
	register int	state;

	simple_lock(&tp->t_lock);

	/*
	  The pdma timeout has gone off. 
	  If no character has been received since the timeout
	  was set, push any pending characters up.
	  If any characters were received in the last interval
	  then just reset the timeout and the character received bit.
	  */

	state = tp->t_state;

	if (state & TS_MIN_TO)
	  {
	    if (state & TS_MIN_TO_RCV)
	      { /* a character was received */
		tp->t_state = state & ~TS_MIN_TO_RCV;
		timeout(ttypush,tp,pdma_timeouts[tp->t_ispeed]);
	      }
	    else
	      {
		tp->t_state = state & ~TS_MIN_TO;
		if (tp->t_inq.c_cc) /* pending characters */
		  tty_queue_completion(&tp->t_delayed_read);
	      }
	  }
	else
	  {
	    tp->t_state = state & ~TS_MIN_TO_RCV;/* sanity */
	  }

	simple_unlock(&tp->t_lock);
	splx(s);
}

/*
 * Put input character on input queue.
 *
 * Called at spltty, tty already locked.
 */
void ttyinput(
	unsigned int	c,
	struct tty	*tp)
{
  if (tp->t_inq.c_cc >= tp->t_inq.c_hog) {
    /*
     * Do not want to overflow input queue
     */
    if (tp->t_mctl) {
	(*tp->t_mctl)(tp, TM_RTS, DMBIC);
	tp->t_state |= TS_RTS_DOWN;
    }
    tty_queue_completion(&tp->t_delayed_read);
    return;

  }

  c &= 0xff;

  (void) putc(c, &tp->t_inq);
  if ((tp->t_state & TS_MIN) == 0 ||
	tp->t_inq.c_cc > pdma_water_mark[tp->t_ispeed])
     {
	  /*
	   * No input buffering, or input minimum exceeded.
	   * Grab a request from input queue and queue it
	   * to io_done thread.
	   */
	  if (tp->t_state & TS_MIN_TO) {
	    tp->t_state &= ~(TS_MIN_TO|TS_MIN_TO_RCV);
	    untimeout(ttypush, tp);
	  }
	  tty_queue_completion(&tp->t_delayed_read);
      }
      else {
	/*
	 * Not enough characters. 
	 * If no timeout is set, initiate the timeout 
	 * Otherwise set the character received during timeout interval
	 * flag.
	 * One alternative approach would be just to reset the timeout
	 * into the future, but this involves making a timeout/untimeout
	 * call on every character.
	 */
	register int ptime = pdma_timeouts[tp->t_ispeed];
	if (ptime > 0)
	  {
	    if ((tp->t_state & TS_MIN_TO) == 0)
	      {
		tp->t_state |= TS_MIN_TO;
		timeout(ttypush, tp, ptime);
	      }
	    else
	      {
	        tp->t_state |= TS_MIN_TO_RCV;
	      }
	  }
      }
}

/*
 * Put many characters on input queue.
 *
 * Called at spltty, tty already locked.
 */
void ttyinput_many(
	struct tty	*tp,
	unsigned char	*chars,
	int		count)
{
	/*
	 * Do not want to overflow input queue 
	 */
	if (tp->t_inq.c_cc < tp->t_inq.c_hog)
		count -= b_to_q( chars, count, &tp->t_inq);

	tty_queue_completion(&tp->t_delayed_read);
}


/*
 * Handle modem control transition on a tty.
 * Flag indicates new state of carrier.
 * Returns FALSE if the line should be turned off.
 *
 * Called at spltty, tty already locked.
 */
boolean_t ttymodem(
	struct tty *	tp,
	boolean_t	carrier_up)
{
	if ((tp->t_state&TS_WOPEN) == 0 && (tp->t_flags & MDMBUF)) {
	    /*
	     * Flow control by carrier.  Carrier down stops
	     * output; carrier up restarts output.
	     */
	    if (carrier_up) {
		tp->t_state &= ~TS_TTSTOP;
		tty_output(tp);
	    }
	    else if ((tp->t_state&TS_TTSTOP) == 0) {
		tp->t_state |= TS_TTSTOP;
		(*tp->t_stop)(tp, 0);
	    }
	}
	else if (carrier_up) {
	    /*
	     * Carrier now on.
	     */
	    tp->t_state |= TS_CARR_ON;
	    tt_open_wakeup(tp);
	}
	else {
	    /*
	     * Lost carrier.
	     */
	    tp->t_state &= ~TS_CARR_ON;
	    if (tp->t_state & TS_ISOPEN &&
		(tp->t_flags & NOHANG) == 0)
	    {
		/*
		 * Hang up TTY if carrier drops.
		 * Need to alert users, somehow...
		 */
		tty_flush(tp, D_READ|D_WRITE);
		return FALSE;
	    }
	}
	return TRUE;
}

/*
 * Similarly, handle transitions on the ClearToSend
 * signal.  Nowadays, it is used by many modems as
 * a flow-control device: they turn it down to stop
 * us from sending more chars.  We do the same with
 * the RequestToSend signal. [Yes, that is exactly
 * why those signals are defined in the standard.]
 *
 * Tty must be locked and on master.
 */
tty_cts(
	struct tty *	tp,
	boolean_t	cts_up)
{
	if (tp->t_state & TS_ISOPEN){
		if (cts_up) {
			tp->t_state &= ~(TS_TTSTOP|TS_BUSY);
			tty_output(tp);
		} else {
			tp->t_state |= (TS_TTSTOP|TS_BUSY);
			(*tp->t_stop)(tp, D_WRITE);
		}
	}
}
