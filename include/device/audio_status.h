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
/*
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

#ifndef _DEVICE_AUDIO_STATUS_H_
#define _DEVICE_AUDIO_STATUS_H_

/*
 * 	Access to ADC devices, such as the AMD 79C30A/32A.
 */

/*
 * Programmable gains, see tables in device drivers
 * for detailed mapping to device specifics.
 */
#define AUDIO_MIN_GAIN (0)
#define AUDIO_MAX_GAIN (255)

/*
 * Encoding of audio samples
 */
#define AUDIO_ENCODING_ULAW (1)
#define AUDIO_ENCODING_ALAW (2)

/*
 * Selection of input/output jack
 */
#define	AUDIO_MIKE		1

#define AUDIO_SPEAKER   	1
#define AUDIO_HEADPHONE		2

/*
 * Programming information from/to user application.
 * Only portions of this might be available on any given chip.
 */
struct audio_prinfo {
	unsigned int	sample_rate;
	unsigned int	channels;
	unsigned int	precision;
	unsigned int	encoding;
	unsigned int	gain;
	unsigned int	port;		/* input/output jack */
	unsigned int	seek;		/* BSD extension */
	unsigned int	ispare[3];
	unsigned int	samples;
	unsigned int	eof;

	unsigned char	pause;
	unsigned char	error;
	unsigned char	waiting;
	unsigned char	cspare[3];
	unsigned char	open;
	unsigned char	active;

};

struct audio_info {
	struct	audio_prinfo play;
	struct	audio_prinfo record;
	unsigned int	monitor_gain;
	/* BSD extensions */
	unsigned int	blocksize;	/* input blocking threshold */
	unsigned int	hiwat;		/* output high water mark */
	unsigned int	lowat;		/* output low water mark */
	unsigned int	backlog;	/* samples of output backlog to gen. */
};

typedef struct audio_info audio_info_t;

#define AUDIO_INITINFO(p)\
	(void)memset((void *)(p), 0xff, sizeof(struct audio_info))

#define AUDIO_GETINFO	_IOR('A', 21, audio_info_t)
#define AUDIO_SETINFO	_IOWR('A', 22, audio_info_t)
#define AUDIO_DRAIN	_IO('A', 23)
#define AUDIO_FLUSH	_IO('A', 24)
#define AUDIO_WSEEK	_IOR('A', 25, unsigned int)
#define AUDIO_RERROR	_IOR('A', 26, int)
#define AUDIO_WERROR	_IOR('A', 27, int)

/*
 * Low level interface to the amd79c30.
 * Internal registers of the MAP block,
 * the Main Audio Processor.
 */
struct mapreg {
	unsigned short	mr_x[8];
	unsigned short	mr_r[8];
	unsigned short	mr_gx;
	unsigned short	mr_gr;
	unsigned short	mr_ger;
	unsigned short	mr_stgr;
	unsigned short	mr_ftgr;
	unsigned short	mr_atgr;
	unsigned char	mr_mmr1;
	unsigned char	mr_mmr2;
};

#define AUDIO_GETMAP	_IOR('A', 27, struct mapreg)
#define	AUDIO_SETMAP	_IOW('A', 28, struct mapreg)

/*
 * Compatibility with Sun interface
 */
struct	audio_ioctl {
	short	control;
	unsigned char	data[46];
};

#define AUDIOGETREG		_IOWR('i',1,struct audio_ioctl)
#define AUDIOSETREG		_IOW('i',2,struct audio_ioctl)

#endif /* _DEVICE_AUDIO_STATUS_H_ */
