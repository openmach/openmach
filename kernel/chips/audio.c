/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
/*-
 * Copyright (c) 1991, 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 * 	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. The name of the Laboratory may not be used to endorse or promote 
 *    products derived from this software without specific prior written 
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <audio.h>
#if NAUDIO > 0

#include <mach_kdb.h>
#include <platforms.h>

#include <mach/std_types.h>
#include <machine/machspl.h>
#include <kern/kalloc.h>
#include <kern/sched_prim.h>
#include <chips/busses.h>

#include <device/device_types.h>
#include <device/io_req.h>
#include <device/ds_routines.h>
#include <device/audio_status.h>	/* user interface */
#include <chips/audio_defs.h>		/* chip interface */
#include <chips/audio_config.h>		/* machdep config */

#define private static

/*
 * Exported functions and data structures
 * [see header file for listing]
 */
int audio_blocksize = DEFBLKSIZE;	/* patchable */
int audio_backlog = 400;		/* 50ms in samples */

/*
 * Software state, per AMD79C30 audio chip.
 */
private
struct audio_softc {
	void		*hw;		/* chip status */
	audio_switch_t	*ops;		/* chip operations */
	au_io_t 	*sc_au;		/* recv and xmit buffers, etc */


	unsigned int	sc_wseek;	/* timestamp of last frame written */
	unsigned int	sc_rseek;	/* timestamp of last frame read */
#if 0
	struct	selinfo sc_wsel;	/* write selector */
	struct	selinfo sc_rsel;	/* read selector */
#endif

} audio_softc_data[NAUDIO];

#define unit_to_softc(u)	&audio_softc_data[u]


/* forward declarations */
private int	audio_sleep (au_cb_t *cb, int thresh);
private void	audio_swintr (struct audio_softc *sc);

/*
 * Audio chip found.
 */
void
audio_attach(
	void		*hw,		/* IN, chip status */
	audio_switch_t	*ops,
	void		**audio_status)	/* OUT, audio status */
{
	register struct audio_softc *sc;
	static int	next = 0;

	if (next >= NAUDIO)
		panic("Please configure more than %d audio devices\n", NAUDIO);
	sc = &audio_softc_data[next++];

	printf(" audio");

	sc->hw	= hw;
	sc->ops	= ops;

	*audio_status	= (void *)sc;
}


private int audio_setinfo (struct audio_softc *, audio_info_t *);
private int audio_getinfo (struct audio_softc *, audio_info_t *);

io_return_t
audio_open(
	int		unit,
	int		mode,
	io_req_t 	req)
{
	register struct audio_softc *sc;
	register au_io_t *au;

	sc = unit_to_softc(unit);
	if (unit > NAUDIO || (!sc->hw))
		return (D_NO_SUCH_DEVICE);

	if (!sc->sc_au) {
		sc->sc_au = (au_io_t *) kalloc(sizeof(au_io_t));
		bzero(sc->sc_au, sizeof(au_io_t));
	}
	au = sc->sc_au;

	au->au_lowat = audio_blocksize;
	au->au_hiwat = AUCB_SIZE - au->au_lowat;
	au->au_blksize = audio_blocksize;
	au->au_backlog = audio_backlog;

	/* set up read and write blocks and `dead sound' zero value. */
	AUCB_INIT(&au->au_rb);
	au->au_rb.cb_thresh = AUCB_SIZE;
	AUCB_INIT(&au->au_wb);
	au->au_wb.cb_thresh = -1;

	/* nothing read or written yet */
	sc->sc_rseek = 0;
	sc->sc_wseek = 0;

	(*sc->ops->init)(sc->hw);

	return (0);
}

private int
audio_drain(
	register au_io_t *au)
{
	register int error;

	while (!AUCB_EMPTY(&au->au_wb))
		if ((error = audio_sleep(&au->au_wb, 0)) != 0)
			return (error);
	return (0);
}

/*
 * Close an audio chip.
 */
/* ARGSUSED */
io_return_t
audio_close(
	int		unit)
{
	register struct audio_softc *sc = unit_to_softc(unit);
	register au_cb_t *cb;
	register spl_t s;

	/*
	 * Block until output drains, but allow ^C interrupt.
	 */
	sc->sc_au->au_lowat = 0;	/* avoid excessive wakeups */

	/*
	 * If there is pending output, let it drain (unless
	 * the output is paused).
	 */
	cb = &sc->sc_au->au_wb;
	s = splaudio();
	if (!AUCB_EMPTY(cb) && !cb->cb_pause)
		(void)audio_drain(sc->sc_au);
	/*
	 * Disable interrupts, and done.
	 */
	(*sc->ops->close)(sc->hw);
	splx(s);
	return (D_SUCCESS);
}

private int
audio_sleep(
	register au_cb_t *cb,
	register int thresh)
{
	register spl_t s = splaudio();

	cb->cb_thresh = thresh;
	assert_wait((event_t)cb, TRUE);
	splx(s);
	thread_block((void (*)()) 0);
	return (0);	/* XXXX */
}

io_return_t
audio_read(
	int		unit,
	io_req_t	ior)
{
	register struct audio_softc *sc = unit_to_softc(unit);
	register au_cb_t *cb;
	register int n, head, taildata;
	register int blocksize = sc->sc_au->au_blksize;
	io_return_t	rc;
	unsigned char	*data;

	/*
	 * Allocate read buffer
	 */
	rc = device_read_alloc(ior, (vm_size_t)ior->io_count);
	if (rc != KERN_SUCCESS)
	    return rc;
	data = (unsigned char *) ior->io_data;
	ior->io_residual = ior->io_count;

	cb = &sc->sc_au->au_rb;
	cb->cb_drops = 0;
	sc->sc_rseek = sc->sc_au->au_stamp - AUCB_LEN(cb);
	do {
		while (AUCB_LEN(cb) < blocksize) {

			if (ior->io_mode & D_NODELAY)
				return (D_WOULD_BLOCK);

			if ((rc = audio_sleep(cb, blocksize)) != 0)
				return(rc);
		}
		/*
		 * The space calculation can only err on the short
		 * side if an interrupt occurs during processing:
		 * only cb_tail is altered in the interrupt code.
		 */
		head = cb->cb_head;
		if ((n = AUCB_LEN(cb)) > ior->io_residual)
			n = ior->io_residual;
		taildata = AUCB_SIZE - head;

		if (n > taildata) {
			bcopy(cb->cb_data + head, data, taildata);
			bcopy(cb->cb_data, data + taildata, n - taildata);
		} else
			bcopy(cb->cb_data + head, data, n);
		data += n;
		ior->io_residual -= n;

		head = AUCB_MOD(head + n);
		cb->cb_head = head;
	} while (ior->io_residual >= blocksize);

	return (rc);
}

io_return_t
audio_write(
	int		unit,
	io_req_t	ior)
{
	register struct audio_softc *sc = unit_to_softc(unit);
	register au_io_t *au = sc->sc_au;
	register au_cb_t *cb = &au->au_wb;
	register int n, tail, tailspace, first, watermark;
	io_return_t rc;
	unsigned char *data;
	vm_offset_t addr = 0;

	if (!(ior->io_op & IO_INBAND)) {
	    /*
	     * Copy out-of-line data into kernel address space.
	     * Since data is copied as page list, it will be
	     * accessible.
	     */
	    vm_map_copy_t copy = (vm_map_copy_t) ior->io_data;
	    kern_return_t kr;

	    kr = vm_map_copyout(device_io_map, &addr, copy);
	    if (kr != KERN_SUCCESS)
		return kr;
	    data = (unsigned char *) addr;
	} else
	    data = (unsigned char *) ior->io_data;
	ior->io_residual = ior->io_count;

	rc = D_SUCCESS;
	first = 1;
	while (ior->io_residual > 0) {
		watermark = au->au_hiwat;
		while (AUCB_LEN(cb) > watermark) {

			if (ior->io_mode & D_NODELAY) {
				rc = D_WOULD_BLOCK;
				goto out;
			}

			if ((rc = audio_sleep(cb, watermark)) != 0)
				goto out;

			watermark = au->au_lowat;
		}
		/*
		 * The only value that can change on an interrupt is
		 * cb->cb_head.  We only pull that out once to decide
		 * how much to write into cb_data; if we lose a race
		 * and cb_head changes, we will merely be overly
		 * conservative.  For a legitimate time stamp,
		 * however, we need to synchronize the accesses to
		 * au_stamp and cb_head at a high ipl below.
		 */
		tail = cb->cb_tail;
		if ((n = (AUCB_SIZE - 1) - AUCB_LEN(cb)) > ior->io_residual) {
			n = ior->io_residual;
			if (cb->cb_head == tail &&
			    n <= au->au_blksize &&
			    au->au_stamp - sc->sc_wseek > 400) {
				/*
				 * the write is 'small', the buffer is empty
				 * and we have been silent for at least 50ms
				 * so we might be dealing with an application
				 * that writes frames synchronously with
				 * reading them.  If so, we need an output
				 * backlog to cover scheduling delays or
				 * there will be gaps in the sound output.
				 * Also take this opportunity to reset the
				 * buffer pointers in case we ended up on
				 * a bad boundary (odd byte, blksize bytes
				 * from end, etc.).
				 */
				register unsigned long *ip;
				register unsigned long muzero;
				spl_t s;
				register int i;

				s = splaudio();
				cb->cb_head = cb->cb_tail = 0;
				splx(s);

				tail = au->au_backlog;
				ip = (unsigned long *)cb->cb_data;
				muzero = sample_rpt_long(0x7fL);
				for (i = tail / sizeof muzero; --i >= 0; )
					*ip++ = muzero;
			}
		}
		tailspace = AUCB_SIZE - tail;
		if (n > tailspace) {
			/* write first part at tail and rest at head */
			bcopy(data, cb->cb_data + tail, tailspace);
			bcopy(data + tailspace, cb->cb_data,
					 n - tailspace);
		} else
			bcopy(data, cb->cb_data + tail, n);
		data += n;
		ior->io_residual -= n;

		tail = AUCB_MOD(tail + n);
		if (first) {
			register spl_t s = splaudio();
			sc->sc_wseek = AUCB_LEN(cb) + au->au_stamp + 1;
			/* 
			 * To guarantee that a write is contiguous in the
			 * sample space, we clear the drop count the first
			 * time through.  If we later get drops, we will
			 * break out of the loop below, before writing
			 * a new frame.
			 */
			cb->cb_drops = 0;
			cb->cb_tail = tail;
			splx(s);
			first = 0;
		} else {
#if 0
			if (cb->cb_drops != 0)
				break;
#endif
			cb->cb_tail = tail;
		}
	}
out:
	if (!(ior->io_op & IO_INBAND))
	    (void) vm_deallocate(device_io_map, addr, ior->io_count);
	return (rc);
}

#include <sys/ioctl.h>

io_return_t
audio_get_status(
	int		unit,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	*status_count)
{
	register struct audio_softc *sc = unit_to_softc(unit);
	register au_io_t *au = sc->sc_au;
	io_return_t rc = D_SUCCESS;
	spl_t	s;

	switch (flavor) {

	case AUDIO_GETMAP:
	case AUDIOGETREG:
		rc = (*sc->ops->getstate)(sc->hw, flavor,
				(void *)status, status_count);
		break;

	/*
	 * Number of read samples dropped.  We don't know where or
	 * when they were dropped.
	 */
	case AUDIO_RERROR:
		*(int *)status = au->au_rb.cb_drops;
		*status_count = 1;
		break;

	case AUDIO_WERROR:
		*(int *)status = au->au_wb.cb_drops;
		*status_count = 1;
		break;

	/*
	 * How many samples will elapse until mike hears the first
	 * sample of what we last wrote?
	 */
	case AUDIO_WSEEK:
		s = splaudio();
		*(unsigned int *)status = sc->sc_wseek - au->au_stamp
				  + AUCB_LEN(&au->au_rb);
		splx(s);
		*status_count = 1;
		break;

	case AUDIO_GETINFO:
		rc = audio_getinfo(sc, (audio_info_t *)status);
		*status_count = sizeof(audio_info_t) / sizeof(int);
		break;

	default:
		rc = D_INVALID_OPERATION;
		break;
	}
	return (rc);
}

io_return_t
audio_set_status(
	int		unit,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	status_count)
{
	register struct audio_softc *sc = unit_to_softc(unit);
	register au_io_t *au = sc->sc_au;
	io_return_t rc = D_SUCCESS;
	spl_t	s;

	switch (flavor) {

	case AUDIO_SETMAP:
	case AUDIOSETREG:
		rc = (*sc->ops->setstate)(sc->hw, flavor,
				(void *)status, status_count);
		break;

	case AUDIO_FLUSH:
		s = splaudio();
		AUCB_INIT(&au->au_rb);
		AUCB_INIT(&au->au_wb);
		au->au_stamp = 0;
		splx(s);
		sc->sc_wseek = 0;
		sc->sc_rseek = 0;
		break;

	case AUDIO_SETINFO:
		rc = audio_setinfo(sc, (audio_info_t *)status);
		break;

	case AUDIO_DRAIN:
		rc = audio_drain(au);
		break;

	default:
		rc = D_INVALID_OPERATION;
		break;
	}
	return (rc);
}


/*
 * Interrupt routine
 */
boolean_t
audio_hwintr(
	void			*status,
	unsigned int		s_in,
	unsigned int		*s_out)
{
	register au_io_t *au = ((struct audio_softc *) status)->sc_au;
	register au_cb_t *cb;
	register int h, t, k;
	register boolean_t	wakeit = FALSE;

	++au->au_stamp;

	/* receive incoming data */
	cb = &au->au_rb;
	h = cb->cb_head;
	t = cb->cb_tail;
	k = AUCB_MOD(t + 1);
	if (h == k)
		cb->cb_drops++;
	else if  (cb->cb_pause != 0)
		cb->cb_pdrops++;
	else {
		cb->cb_data[t] = s_in;
		cb->cb_tail = t = k;
	}
	if (AUCB_MOD(t - h) >= cb->cb_thresh) {
		cb->cb_thresh = AUCB_SIZE;
		cb->cb_waking = 1;
		wakeit = TRUE;
	}
	/* send outgoing data */
	cb = &au->au_wb;
	h = cb->cb_head;
	t = cb->cb_tail;
	k = 0;
	if (h == t)
		cb->cb_drops++;
	else if (cb->cb_pause != 0)
		cb->cb_pdrops++;
	else {
		cb->cb_head = h = AUCB_MOD(h + 1);
		*s_out = cb->cb_data[h];
		k = 1;
	}
	if (AUCB_MOD(t - h) <= cb->cb_thresh) {
		cb->cb_thresh = -1;
		cb->cb_waking = 1;
		wakeit = TRUE;
	}
	if (wakeit)
		audio_swintr((struct audio_softc *) status);
	return (k == 1);
}

private void
audio_swintr(
	register struct audio_softc *sc)
{
	register au_io_t *au = sc->sc_au;

	if (au->au_rb.cb_waking != 0) {
		au->au_rb.cb_waking = 0;
		wakeup(&au->au_rb);
	}
	if (au->au_wb.cb_waking != 0) {
		au->au_wb.cb_waking = 0;
		wakeup(&au->au_wb);
	}
}

private int
audio_setinfo(
	struct audio_softc *sc,
	audio_info_t *ai)
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	register int bsize;
	register au_io_t	*au = sc->sc_au;
	spl_t s;

	(*sc->ops->setgains)(sc->hw, p->gain, r->gain, ai->monitor_gain );

	if (p->pause != (unsigned char)~0)
		au->au_wb.cb_pause = p->pause;
	if (r->pause != (unsigned char)~0)
		au->au_rb.cb_pause = r->pause;

	if (p->port != ~0)
		(*sc->ops->setport)(sc->hw, p->port);

	if (ai->blocksize != ~0) {
		if (ai->blocksize == 0)
			bsize = ai->blocksize = DEFBLKSIZE;
		else if (ai->blocksize > MAXBLKSIZE)
			bsize = ai->blocksize = MAXBLKSIZE;
		else
			bsize = ai->blocksize;

		s = splaudio();
		au->au_blksize = bsize;
		/* AUDIO_FLUSH */
		AUCB_INIT(&au->au_rb);
		AUCB_INIT(&au->au_wb);
		splx(s);

	}
	if (ai->hiwat != ~0 && (unsigned)ai->hiwat < AUCB_SIZE)
		au->au_hiwat = ai->hiwat;
	if (ai->lowat != ~0 && ai->lowat < AUCB_SIZE)
		au->au_lowat = ai->lowat;
	if (ai->backlog != ~0 && ai->backlog < (AUCB_SIZE/2))
		au->au_backlog = ai->backlog;

	return (0);
}

private int
audio_getinfo(
	struct audio_softc *sc,
	audio_info_t *ai)
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	register au_io_t	*au = sc->sc_au;

	p->sample_rate = r->sample_rate = 8000;
	p->channels = r->channels = 1;
	p->precision = r->precision = 8;
	p->encoding = r->encoding = AUDIO_ENCODING_ULAW;

	(*sc->ops->getgains)(sc->hw, &p->gain, &r->gain, &ai->monitor_gain );

	r->port = AUDIO_MIKE;
	p->port = (*sc->ops->getport)(sc->hw);

	p->pause = au->au_wb.cb_pause;
	r->pause = au->au_rb.cb_pause;
	p->error = au->au_wb.cb_drops != 0;
	r->error = au->au_rb.cb_drops != 0;

	/* Now this is funny. If you got here it means you must have
	   opened the device, so how could it possibly be closed ?
	   Unless we upgrade the berkeley code to check if the chip
	   is currently playing and/or recording... Later. */
	p->open = TRUE;
	r->open = TRUE;

	p->samples = au->au_stamp - au->au_wb.cb_pdrops;
	r->samples = au->au_stamp - au->au_rb.cb_pdrops;

	p->seek = sc->sc_wseek;
	r->seek = sc->sc_rseek;

	ai->blocksize = au->au_blksize;
	ai->hiwat = au->au_hiwat;
	ai->lowat = au->au_lowat;
	ai->backlog = au->au_backlog;

	return (0);
}

#if	MACH_KDB
#include <ddb/db_output.h>

void audio_queue_status( au_cb_t *cb, char *logo)
{
	db_printf("%s ring status:\n", logo);
	db_printf("   h %x t %x sh %x w %d p %d d %x pd %x\n",
		  cb->cb_head, cb->cb_tail, cb->cb_thresh,
		  cb->cb_waking, cb->cb_pause, (long)cb->cb_drops,
		  (long)cb->cb_pdrops);
}

int audio_status(int unit)
{
	struct audio_softc *sc = unit_to_softc(unit);
	au_io_t	*au;

	if (!sc) {
		db_printf("No such thing\n");
		return 0;
	}
	db_printf("@%lx: wseek %d rseek %d, au @%lx\n",
		sc, sc->sc_wseek, sc->sc_rseek, sc->sc_au);
	if (!(au = sc->sc_au)) return 0;

	db_printf("au: stamp %x lo %x hi %x blk %x blg %x\n",
		au->au_stamp, au->au_lowat, au->au_hiwat,
		au->au_blksize, au->au_backlog);
	audio_queue_status(&au->au_rb, "read");
	audio_queue_status(&au->au_wb, "write");

	return 0;
}
#endif	/* MACH_KDB */

#endif	/* NAUDIO > 0 */

