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
 *	File: isdn_79c30_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Driver for the AMD 79c30 ISDN (Integrated Speech and
 *	Data Network) controller chip.
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

#include <isdn.h>
#if	NISDN > 0

#include <platforms.h>

#include <mach/std_types.h>
#include <machine/machspl.h>
#include <sys/ioctl.h>	/* for Sun compat */
#include <chips/busses.h>

#include <device/device_types.h>
#include <device/io_req.h>
#include <device/audio_status.h>
#include <chips/audio_defs.h>

#include <chips/isdn_79c30.h>

#include <chips/audio_config.h>		/* machdep config */

#define private static

/*
 * Autoconf info
 */
private int isdn_probe (vm_offset_t reg, struct bus_ctlr *ui);
private void isdn_attach ( struct bus_device *ui);

private vm_offset_t isdn_std[NISDN] = { 0 };
private struct bus_device *isdn_info[NISDN];

struct bus_driver isdn_driver =
       { isdn_probe, 0, isdn_attach, 0, isdn_std, "isdn", isdn_info, };


/*
 * Externally visible functions and data
 */
int	isdn_intr();


/*
 * Status bookeeping and globals
 */
typedef struct {
	amd79c30_padded_regs_t *regs;
	void		*audio_status;	/* for upcalls */
	struct mapreg	sc_map;		/* MAP status */
	/*
	 * keep track of levels so we don't have to convert back from
	 * MAP gain constants
	 */
	int	sc_rlevel;		/* record level */
	int	sc_plevel;		/* play level */
	int	sc_mlevel;		/* monitor level */
} isdn_softc_t;

isdn_softc_t	isdn_softc_data[NISDN];
isdn_softc_t	*isdn_softc[NISDN];

private int audio_default_level = 150;


/*
 * Forward decls
 */
audio_switch_t isdn_ops;

private void isdn_init( isdn_softc_t *sc );

private void isdn_set_mmr2(
		register amd79c30_padded_regs_t	*regs,
		register int			mmr2);

private void isdn_setgains(
		isdn_softc_t	*sc,
		int		pgain,
		int		rgain,
		int		mgain);

/*
 * Probe chip to see if it is there
 */
private isdn_probe(
	vm_offset_t	reg,
	struct bus_ctlr *ui)
{
	isdn_softc_t	*sc = &isdn_softc_data[ui->unit];

	isdn_softc[ui->unit] = sc;
	sc->regs = (amd79c30_padded_regs_t *)reg;

	return 1;
}

/*
 * Attach device to chip-indep driver(s)
 */
private void
isdn_attach(
	struct bus_device *ui)
{
	register isdn_softc_t	*sc = isdn_softc[ui->unit];
	register amd79c30_padded_regs_t *regs = sc->regs;

	/* disable interrupts */
	write_reg(regs->cr, AMDR_INIT);
	write_reg(regs->dr, AMD_INIT_PMS_ACTIVE | AMD_INIT_INT_DISABLE);

	/*
	 * Initialize the mux unit.  We use MCR3 to route audio (MAP)
	 * through channel Bb.  MCR1 and MCR2 are unused.
	 * Setting the INT enable bit in MCR4 will generate an interrupt
	 * on each converted audio sample.
	 */
	write_reg(regs->cr, AMDR_MUX_1_4);
 	write_reg(regs->dr, 0);
	write_reg(regs->dr, 0);
	write_reg(regs->dr, (AMD_MCRCHAN_BB << 4) | AMD_MCRCHAN_BA);
	write_reg(regs->dr, AMD_MCR4_INT_ENABLE);

	printf(" AMD 79C30A/79C32A");

	audio_attach( sc, &isdn_ops, &sc->audio_status );
}

/*
 * Chip re-initialization
 */
private void
isdn_init(
	isdn_softc_t	*sc)
{
	register amd79c30_padded_regs_t *regs;
	
	bzero((char *)&sc->sc_map, sizeof sc->sc_map);
	/* default to speaker */
	sc->sc_map.mr_mmr2 = AMD_MMR2_AINB | AMD_MMR2_LS;
	
	/* enable interrupts and set parameters established above */
	regs = sc->regs;
	isdn_set_mmr2 (regs, sc->sc_map.mr_mmr2);
	isdn_setgains (sc, audio_default_level, audio_default_level, 0);
	write_reg(regs->cr, AMDR_INIT);
	write_reg(regs->dr, AMD_INIT_PMS_ACTIVE);
}

/*
 * Chip shutdown
 */
private void
isdn_close(
	isdn_softc_t	*sc)
{
	register amd79c30_padded_regs_t *regs;

	regs = sc->regs;
	write_reg(regs->cr, AMDR_INIT);
	write_reg(regs->dr, AMD_INIT_PMS_ACTIVE | AMD_INIT_INT_DISABLE);
}

/*
 * Audio port selection
 */
private void
isdn_setport(
	isdn_softc_t	*sc,
	int		port)
{
	if (port == AUDIO_SPEAKER) {
		sc->sc_map.mr_mmr2 |= AMD_MMR2_LS;
		isdn_set_mmr2(sc->regs, sc->sc_map.mr_mmr2);
	} else if (port == AUDIO_HEADPHONE) {
		sc->sc_map.mr_mmr2 &=~ AMD_MMR2_LS;
		isdn_set_mmr2(sc->regs, sc->sc_map.mr_mmr2);
	}
}

private int
isdn_getport(
	isdn_softc_t	*sc)
{
	return (sc->sc_map.mr_mmr2 & AMD_MMR2_LS) ?
		AUDIO_SPEAKER : AUDIO_HEADPHONE;
}

/*
 * Volume control
 */
private void
isdn_setgains(
	isdn_softc_t	*sc,
	int		pgain,
	int		rgain,
	int		mgain)
{
	private void isdn_set_pgain(), isdn_set_rgain(), isdn_set_mgain();

	if (pgain != ~0)
		isdn_set_pgain(sc, pgain);
	if (rgain != ~0)
		isdn_set_rgain(sc, rgain);
	if (mgain != ~0)
		isdn_set_mgain(sc, mgain);

}

private void
isdn_getgains(
	isdn_softc_t	*sc,
	int		*pgain,
	int		*rgain,
	int		*mgain)
{
	*mgain = sc->sc_mlevel;
	*rgain = sc->sc_rlevel;
	*pgain = sc->sc_plevel;
}


/*
 * User control over MAP processor
 */
private io_return_t
isdn_setstate(
	isdn_softc_t	*sc,
	dev_flavor_t	flavor,
	register struct mapreg *map,
	natural_t	n_ints)
{
	register amd79c30_padded_regs_t *regs = sc->regs;
	register int i, v;
	spl_t s;

	/* Sun compat */
	if (flavor == AUDIOSETREG) {
		register struct audio_ioctl *a = (struct audio_ioctl *)map;
		s = splaudio();
		write_reg(regs->cr, (a->control >> 8) & 0xff);
		for (i = 0; i < (a->control & 0xff); i++) {
		    write_reg(regs->dr, a->data[i]);
		}
		splx(s);
		return D_SUCCESS;
	}

	if (flavor != AUDIO_SETMAP)
		return D_INVALID_OPERATION;

	if ((n_ints * sizeof(int)) < sizeof(*map))
		return D_INVALID_SIZE;

	bcopy(map, &sc->sc_map, sizeof(sc->sc_map));
	sc->sc_map.mr_mmr2 &= 0x7f;

	s = splaudio();
	write_reg(regs->cr, AMDR_MAP_1_10);
	for (i = 0; i < 8; i++) {
		v = map->mr_x[i];
		WAMD16(regs, v);
	}
	for (i = 0; i < 8; ++i) {
		v = map->mr_r[i];
		WAMD16(regs, v);
	}
	v = map->mr_gx; WAMD16(regs, v);
	v = map->mr_gr; WAMD16(regs, v);
	v = map->mr_ger; WAMD16(regs, v);
	v = map->mr_stgr; WAMD16(regs, v);
	v = map->mr_ftgr; WAMD16(regs, v);
	v = map->mr_atgr; WAMD16(regs, v);
	write_reg(regs->dr, map->mr_mmr1);
	write_reg(regs->dr, map->mr_mmr2);
	splx(s);
	return D_SUCCESS;
}

private io_return_t
isdn_getstate(
	isdn_softc_t	*sc,
	dev_flavor_t	flavor,
	register struct mapreg *map,
	natural_t	*count)
{
	register amd79c30_padded_regs_t *regs = sc->regs;
	spl_t s;
	int i;

	/* Sun compat */
	if (flavor == AUDIOGETREG) {
		register struct audio_ioctl *a = (struct audio_ioctl *)map;
		s = splaudio();
		write_reg(regs->cr, (a->control >> 8) & 0xff);
		for (i = 0; i < (a->control & 0xff); i++) {
		    read_reg(regs->dr,a->data[i]);
		}
		splx(s);
		*count = sizeof(*a) / sizeof(int);
		return D_SUCCESS;
	}

	if ( (*count * sizeof(int)) < sizeof(*map))
		return D_INVALID_SIZE;
	bcopy(&sc->sc_map, map, sizeof(sc->sc_map));
	*count = sizeof(*map) / sizeof(int);
	return D_SUCCESS;
}



/*
 * Set the mmr1 register and one other 16 bit register in the audio chip.
 * The other register is indicated by op and val.
 */
private void
isdn_set_mmr1(
	register amd79c30_padded_regs_t *regs,
	register int mmr1,
	register int op,
	register int val)
{
	register int s = splaudio();

	write_reg(regs->cr, AMDR_MAP_MMR1);
	write_reg(regs->dr, mmr1);
	write_reg(regs->cr, op);
	WAMD16(regs, val);
	splx(s);
}

/*
 * Set the mmr2 register.
 */
private void
isdn_set_mmr2(
	register amd79c30_padded_regs_t	*regs,
	register int			mmr2)
{
	register int s = splaudio();

	write_reg(regs->cr, AMDR_MAP_MMR2);
	write_reg(regs->dr, mmr2);
	splx(s);
}

/*
 * gx, gr & stg gains.  this table must contain 256 elements with
 * the 0th being "infinity" (the magic value 9008).  The remaining
 * elements match sun's gain curve (but with higher resolution):
 * -18 to 0dB in .16dB steps then 0 to 12dB in .08dB steps.
 */
private const unsigned short gx_coeff[256] = {
	0x9008, 0x8b7c, 0x8b51, 0x8b45, 0x8b42, 0x8b3b, 0x8b36, 0x8b33,
	0x8b32, 0x8b2a, 0x8b2b, 0x8b2c, 0x8b25, 0x8b23, 0x8b22, 0x8b22,
	0x9122, 0x8b1a, 0x8aa3, 0x8aa3, 0x8b1c, 0x8aa6, 0x912d, 0x912b,
	0x8aab, 0x8b12, 0x8aaa, 0x8ab2, 0x9132, 0x8ab4, 0x913c, 0x8abb,
	0x9142, 0x9144, 0x9151, 0x8ad5, 0x8aeb, 0x8a79, 0x8a5a, 0x8a4a,
	0x8b03, 0x91c2, 0x91bb, 0x8a3f, 0x8a33, 0x91b2, 0x9212, 0x9213,
	0x8a2c, 0x921d, 0x8a23, 0x921a, 0x9222, 0x9223, 0x922d, 0x9231,
	0x9234, 0x9242, 0x925b, 0x92dd, 0x92c1, 0x92b3, 0x92ab, 0x92a4,
	0x92a2, 0x932b, 0x9341, 0x93d3, 0x93b2, 0x93a2, 0x943c, 0x94b2,
	0x953a, 0x9653, 0x9782, 0x9e21, 0x9d23, 0x9cd2, 0x9c23, 0x9baa,
	0x9bde, 0x9b33, 0x9b22, 0x9b1d, 0x9ab2, 0xa142, 0xa1e5, 0x9a3b,
	0xa213, 0xa1a2, 0xa231, 0xa2eb, 0xa313, 0xa334, 0xa421, 0xa54b,
	0xada4, 0xac23, 0xab3b, 0xaaab, 0xaa5c, 0xb1a3, 0xb2ca, 0xb3bd,
	0xbe24, 0xbb2b, 0xba33, 0xc32b, 0xcb5a, 0xd2a2, 0xe31d, 0x0808,
	0x72ba, 0x62c2, 0x5c32, 0x52db, 0x513e, 0x4cce, 0x43b2, 0x4243,
	0x41b4, 0x3b12, 0x3bc3, 0x3df2, 0x34bd, 0x3334, 0x32c2, 0x3224,
	0x31aa, 0x2a7b, 0x2aaa, 0x2b23, 0x2bba, 0x2c42, 0x2e23, 0x25bb,
	0x242b, 0x240f, 0x231a, 0x22bb, 0x2241, 0x2223, 0x221f, 0x1a33,
	0x1a4a, 0x1acd, 0x2132, 0x1b1b, 0x1b2c, 0x1b62, 0x1c12, 0x1c32,
	0x1d1b, 0x1e71, 0x16b1, 0x1522, 0x1434, 0x1412, 0x1352, 0x1323,
	0x1315, 0x12bc, 0x127a, 0x1235, 0x1226, 0x11a2, 0x1216, 0x0a2a,
	0x11bc, 0x11d1, 0x1163, 0x0ac2, 0x0ab2, 0x0aab, 0x0b1b, 0x0b23,
	0x0b33, 0x0c0f, 0x0bb3, 0x0c1b, 0x0c3e, 0x0cb1, 0x0d4c, 0x0ec1,
	0x079a, 0x0614, 0x0521, 0x047c, 0x0422, 0x03b1, 0x03e3, 0x0333,
	0x0322, 0x031c, 0x02aa, 0x02ba, 0x02f2, 0x0242, 0x0232, 0x0227,
	0x0222, 0x021b, 0x01ad, 0x0212, 0x01b2, 0x01bb, 0x01cb, 0x01f6,
	0x0152, 0x013a, 0x0133, 0x0131, 0x012c, 0x0123, 0x0122, 0x00a2,
	0x011b, 0x011e, 0x0114, 0x00b1, 0x00aa, 0x00b3, 0x00bd, 0x00ba,
	0x00c5, 0x00d3, 0x00f3, 0x0062, 0x0051, 0x0042, 0x003b, 0x0033,
	0x0032, 0x002a, 0x002c, 0x0025, 0x0023, 0x0022, 0x001a, 0x0021,
	0x001b, 0x001b, 0x001d, 0x0015, 0x0013, 0x0013, 0x0012, 0x0012,
	0x000a, 0x000a, 0x0011, 0x0011, 0x000b, 0x000b, 0x000c, 0x000e,
};

/*
 * second stage play gain.
 */
private const unsigned short ger_coeff[] = {
	0x431f, /* 5. dB */
	0x331f, /* 5.5 dB */
	0x40dd, /* 6. dB */
	0x11dd, /* 6.5 dB */
	0x440f, /* 7. dB */
	0x411f, /* 7.5 dB */
	0x311f, /* 8. dB */
	0x5520, /* 8.5 dB */
	0x10dd, /* 9. dB */
	0x4211, /* 9.5 dB */
	0x410f, /* 10. dB */
	0x111f, /* 10.5 dB */
	0x600b, /* 11. dB */
	0x00dd, /* 11.5 dB */
	0x4210, /* 12. dB */
	0x110f, /* 13. dB */
	0x7200, /* 14. dB */
	0x2110, /* 15. dB */
	0x2200, /* 15.9 dB */
	0x000b, /* 16.9 dB */
	0x000f  /* 18. dB */
#define NGER (sizeof(ger_coeff) / sizeof(ger_coeff[0]))
};

private void
isdn_set_rgain(
	register isdn_softc_t	*sc,
	register int		level)
{
	level &= 0xff;
	sc->sc_rlevel = level;
	sc->sc_map.mr_mmr1 |= AMD_MMR1_GX;
	sc->sc_map.mr_gx = gx_coeff[level];
	isdn_set_mmr1(sc->regs, sc->sc_map.mr_mmr1,
		      AMDR_MAP_GX, sc->sc_map.mr_gx);
}

private void
isdn_set_pgain(
	register isdn_softc_t	*sc,
	register int		level)
{
	register int gi, s;
	register amd79c30_padded_regs_t *regs;

	level &= 0xff;
	sc->sc_plevel = level;
	sc->sc_map.mr_mmr1 |= AMD_MMR1_GER|AMD_MMR1_GR;
	level *= 256 + NGER;
	level >>= 8;
	if (level >= 256) {
		gi = level - 256;
		level = 255;
	} else
		gi = 0;
	sc->sc_map.mr_ger = ger_coeff[gi];
	sc->sc_map.mr_gr = gx_coeff[level];

	regs = sc->regs;
	s = splaudio();
	write_reg(regs->cr, AMDR_MAP_MMR1);
	write_reg(regs->dr, sc->sc_map.mr_mmr1);
	write_reg(regs->cr, AMDR_MAP_GR);
	gi =  sc->sc_map.mr_gr;
	WAMD16(regs, gi);
	write_reg(regs->cr, AMDR_MAP_GER);
	gi =  sc->sc_map.mr_ger;
	WAMD16(regs, gi);
	splx(s);
}

private void
isdn_set_mgain(
	register isdn_softc_t	*sc,
	register int		level)
{
	level &= 0xff;
	sc->sc_mlevel = level;
	sc->sc_map.mr_mmr1 |= AMD_MMR1_STG;
	sc->sc_map.mr_stgr = gx_coeff[level];
	isdn_set_mmr1(sc->regs, sc->sc_map.mr_mmr1,
		      AMDR_MAP_STG, sc->sc_map.mr_stgr);
}

/*
 * Interrupt routine
 */
#if	old
isdn_intr (unit, spllevel)
	spl_t	spllevel;
{
#ifdef	MAXINE
	xine_enable_interrupt(7, 0, 0);
#endif
#ifdef	FLAMINGO
	kn15aa_enable_interrupt(12, 0, 0);
#endif
	printf("ISDN interrupt");
}
#else
isdn_intr (unit, spllevel)
	spl_t	spllevel;
{
	isdn_softc_t	*sc = isdn_softc[unit];
	amd79c30_padded_regs_t *regs = sc->regs;
	register int i;
	unsigned int c;

	read_reg(regs->ir, i); mb();	/* clear interrupt, now */
#if mips
	splx(spllevel);		/* drop priority */
#endif

#if 0
	if (..this is an audio interrupt..)
#endif
	{
		read_reg(regs->bbrb, c);
		if (audio_hwintr(sc->audio_status, c, &c))
			write_reg(regs->bbtb, c);
	}
}
#endif



/*
 *  Standard operations vector
 */
audio_switch_t isdn_ops = {
	isdn_init,
	isdn_close,
	isdn_setport,
	isdn_getport,
	isdn_setgains,
	isdn_getgains,
	isdn_setstate,
	isdn_getstate
};

#if 1
write_an_int(int *where, int what) { *where = what;}
read_an_int(int *where) { return *where;}
#endif

#endif
