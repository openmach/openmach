/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 *  Here platform specific code to define sample_t & co
 *  [to cope with weird DMA engines], and other customs
 */
#ifdef	FLAMINGO
#define splaudio	splbio
#define	sample_t	unsigned char	/* later */
#define	samples_to_chars	bcopy
#define	chars_to_samples	bcopy
/* Sparse space ! */
typedef struct {
	volatile unsigned long	cr;	/* command register (wo) */
/*#define ir cr				/* interrupt register (ro) */
	volatile unsigned long	dr;	/* data register (rw) */
	volatile unsigned long	dsr1;	/* D-channel status register 1 (ro) */
	volatile unsigned long	der;	/* D-channel error register (ro) */
	volatile unsigned long	dctb;	/* D-channel transmit register (wo) */
/*#define dcrb dctb			/* D-channel receive register (ro) */
	volatile unsigned long	bbtb;	/* Bb-channel transmit register (wo) */
/*#define bbrb bbtb			/* Bb-channel receive register (ro) */
	volatile unsigned long	bctb;	/* Bc-channel transmit register (wo)*/
/*#define bcrb bctb			/* Bc-channel receive register (ro) */
	volatile unsigned long	dsr2;	/* D-channel status register 2 (ro) */
} amd79c30_padded_regs_t;

/* give the chip 400ns in between accesses */
#define	read_reg(r,v)				\
	{ (v) = ((r) >> 8) & 0xff; delay(1); }

#define write_reg(r,v)				\
	{ (r) = (((v) & 0xff) << 8) |		\
		 0x200000000L; /*bytemask*/	\
		delay(1); wbflush();		\
	}

/* Write 16 bits of data from variable v to the data port of the audio chip */
#define	WAMD16(regs, v)				\
	{ write_reg((regs)->dr,v);		\
	  write_reg((regs)->dr,v>>8); }

#define mb() wbflush()

#endif	/* FLAMINGO */


#ifdef	MAXINE
#define splaudio	splhigh
typedef struct {
	volatile unsigned char	cr;	/* command register (wo) */
/*#define ir cr				/* interrupt register (ro) */
	char				pad0[63];
	volatile unsigned char	dr;	/* data register (rw) */
	char				pad1[63];
	volatile unsigned char	dsr1;	/* D-channel status register 1 (ro) */
	char				pad2[63];
	volatile unsigned char	der;	/* D-channel error register (ro) */
	char				pad3[63];
	volatile unsigned char	dctb;	/* D-channel transmit register (wo) */
/*#define dcrb dctb			/* D-channel receive register (ro) */
	char				pad4[63];
	volatile unsigned char	bbtb;	/* Bb-channel transmit register (wo) */
/*#define bbrb bbtb			/* Bb-channel receive register (ro) */
	char				pad5[63];
	volatile unsigned char	bctb;	/* Bc-channel transmit register (wo)*/
/*#define bcrb bctb			/* Bc-channel receive register (ro) */
	char				pad6[63];
	volatile unsigned char	dsr2;	/* D-channel status register 2 (ro) */
	char				pad7[63];
} amd79c30_padded_regs_t;

/* give the chip 400ns in between accesses */
#define	read_reg(r,v)				\
	{ (v) = (r); delay(1); }

#define write_reg(r,v)				\
	{ (r) = (v); delay(1); wbflush(); }

/* Write 16 bits of data from variable v to the data port of the audio chip */
#define	WAMD16(regs, v)				\
	{ write_reg((regs)->dr,v);		\
	  write_reg((regs)->dr,v>>8); }

#define mb()

#endif	/* MAXINE */


#ifndef	sample_t
#define	sample_t	unsigned char
#define	samples_to_chars	bcopy
#define	chars_to_samples	bcopy
#endif

/*
 * More architecture-specific customizations
 */
#ifdef	alpha
#define sample_rpt_int(x)	(((x)<<24)|((x)<<16)|((x)<<8)|((x)<<0))
#define sample_rpt_long(x)	((sample_rpt_int(x)<<32)|sample_rpt_int(x))
#endif

#ifndef	sample_rpt_long
#define sample_rpt_long(x)	(((x)<<24)|((x)<<16)|((x)<<8)|((x)<<0))
#endif

