/* 
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
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
 *	File: dtop_hdw.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	1/92
 *
 *	Hardware-level operations for the Desktop serial line
 *	bus (i2c aka ACCESS).
 */

#include <dtop.h>
#if	NDTOP > 0
#include <bm.h>
#include <platforms.h>

#include <machine/machspl.h>		/* spl definitions */
#include <mach/std_types.h>
#include <device/io_req.h>
#include <device/tty.h>

#include <chips/busses.h>
#include <chips/serial_defs.h>
#include <chips/screen_defs.h>
#include <chips/lk201.h>
#include <mips/PMAX/tc.h>

#include <chips/dtop.h>

#define	DTOP_MAX_POLL	0x7fff		/* about half a sec */

#ifdef	MAXINE

typedef volatile unsigned int	*data_reg_t;	/* uC  */
#define	DTOP_GET_BYTE(data)	(((*(data)) >> 8) & 0xff)
#define	DTOP_PUT_BYTE(data,c)	{ *(data) = (c) << 8; }

typedef volatile unsigned int	*poll_reg_t;	/* SIR */
#define	DTOP_RX_AVAIL(poll)	(*(poll) & 1)
#define	DTOP_TX_AVAIL(poll)	(*(poll) & 2)

#else

define how to get/put DTOP packets on this box

#endif

/*
 * Driver status
 */

struct dtop_softc {
	data_reg_t	data;
	poll_reg_t	poll;
	char		polling_mode;
	char		probed_once;
	short		bad_pkts;

	struct dtop_ds {
		int		(*handler)(dtop_device_t,
					   dtop_message_t, 
					   int, 
					   unsigned char);
		dtop_device	status;
	} device[(DTOP_ADDR_DEFAULT - DTOP_ADDR_FIRST) >> 1];

#	define	DTOP_DEVICE_NO(address)	(((address)-DTOP_ADDR_FIRST)>>1)

} dtop_softc_data[NDTOP];

typedef struct dtop_softc *dtop_softc_t;

dtop_softc_t	dtop_softc[NDTOP];

/*
 * Definition of the driver for the auto-configuration program.
 */

int	dtop_probe(), dtop_intr();
static void	dtop_attach();

vm_offset_t	dtop_std[NDTOP] = { 0 };
struct	bus_device *dtop_info[NDTOP];
struct	bus_driver dtop_driver = 
        { dtop_probe, 0, dtop_attach, 0, dtop_std, "dtop", dtop_info,};


int dtop_print_debug = 0;

/*
 * Adapt/Probe/Attach functions
 */

set_dtop_address( dtopunit, poll_reg)
	data_reg_t	poll_reg;
{
	int i;

	extern int	dtop_probe(), dtop_param(), dtop_start(),
			dtop_putc(), dtop_getc(),
			dtop_pollc(), dtop_mctl(), dtop_softCAR();

	dtop_std[dtopunit] = (vm_offset_t)poll_reg;

	/* Do this here */
	console_probe		= dtop_probe;
	console_param		= dtop_param;
	console_start		= dtop_start;
	console_putc		= dtop_putc;
	console_getc		= dtop_getc;
	console_pollc		= dtop_pollc;
	console_mctl		= dtop_mctl;
	console_softCAR		= dtop_softCAR;

}

dtop_probe( data_reg, ui)
	data_reg_t		data_reg;
	struct bus_device	*ui;
{
	int		dtopunit = ui->unit, i;
	dtop_softc_t    dtop;

	dtop = &dtop_softc_data[dtopunit];
	dtop_softc[dtopunit] = dtop;

	dtop->poll = (poll_reg_t)dtop_std[dtopunit];
	dtop->data = data_reg;

	for (i = 0; i < DTOP_MAX_DEVICES; i++)
		dtop->device[i].handler = dtop_null_device_handler;

	/* a lot more needed here, fornow: */
	dtop->device[DTOP_DEVICE_NO(0x6a)].handler = dtop_locator_handler;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.type =
		DEV_MOUSE;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.relative =
		1;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.button_code[0] =
		KEY_LEFT_BUTTON;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.button_code[1] =
		KEY_RIGHT_BUTTON;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.button_code[2] =
		KEY_MIDDLE_BUTTON;
	dtop->device[DTOP_DEVICE_NO(0x6a)].status.locator.n_coords =
		2;

	dtop->device[DTOP_DEVICE_NO(0x6c)].handler = dtop_keyboard_handler;
	dtop->device[DTOP_DEVICE_NO(0x6c)].status.keyboard.poll_frequency =
		(hz * 5) / 100; /* x0.01 secs */
	dtop->device[DTOP_DEVICE_NO(0x6c)].status.keyboard.bell_volume =
		DTOP_CLICK_VOLUME_MAX;

	return 1;
}

static void
dtop_attach(ui)
	struct bus_device	*ui;
{
	int             i;

	/* Initialize all the console ttys */
	for (i = 0; i < 4; i++)
		ttychars(console_tty[i]);
	/* Mark keyboard and mouse present */
	for (i = 0; i < 2; i++)
		console_tty[i]->t_addr = (char*)1;
}

/*
 * Polled I/O (debugger)
 */
dtop_pollc(unit, on)
	boolean_t		on;
{
	dtop_softc_t		dtop;

	dtop = dtop_softc[unit];
	if (on) {
		dtop->polling_mode++;
#if	NBM > 0
		screen_on_off(unit, TRUE);
#endif	NBM > 0
	} else
		dtop->polling_mode--;
}

/*
 * Interrupt routine
 */
dtop_intr (unit, spllevel, recvd)
	spl_t		spllevel;
	boolean_t	recvd;
{

	if (recvd) {
		dtop_message	msg;
		int		devno;
		dtop_softc_t	dtop;

		ssaver_bump(unit);

#ifdef	mips
		splx(spllevel);
#endif

		dtop = dtop_softc[unit];
		if (dtop_get_packet(dtop, &msg) < 0) {
		  if (dtop_print_debug)
		    printf("%s", "dtop: overrun (or stray)\n");
		  return;
		}

		devno = DTOP_DEVICE_NO(msg.src_address);
		if (devno < 0 || devno > 15) return;	/* sanity */

		(void) (*dtop->device[devno].handler)
				(&dtop->device[devno].status, &msg,
				 DTOP_EVENT_RECEIVE_PACKET, 0);

	} else {
		/* fornow xmit is not intr based */
		(*tc_enable_interrupt)( dtop_info[unit]->adaptor, FALSE, TRUE);
	}
}

boolean_t
dtop_start(tp)
	struct tty *tp;
{
	register int		line, temp;

	/* no, we do not need a char out first */
	return FALSE;
}

dtop_w_test(n, a,b,c,d,e,f,g,h)
{
	int *p = (int*)0xbc2a0000;

	if (n <= 0) return;

	a <<= 8; *p = a;
	if (--n == 0) goto out;
	delay(20);
	b <<= 8; *p = b;
	if (--n == 0) goto out;
	delay(20);
	c <<= 8; *p = c;
	if (--n == 0) goto out;
	delay(20);
	d <<= 8; *p = d;
	if (--n == 0) goto out;
	delay(20);
	e <<= 8; *p = e;
	if (--n == 0) goto out;
	delay(20);
	f <<= 8; *p = f;
	if (--n == 0) goto out;
	delay(20);
	g <<= 8; *p = g;
	if (--n == 0) goto out;
	delay(20);
	h <<= 8; *p = h;
out:
	delay(10000);
	{
		int buf[100];

		delay(20);
		a = *p;
		buf[0] = a;
		c = 1;
		for (n = 0; n < 100; n++) {
			delay(20);
			b = *p;
			if (b != a) {
				buf[c++] = b;
				b = a;
			}
		}
		for (n = 0; n < c; n++)
			db_printf("%x ", ((buf[n])>>8)&0xff);
	}
	return c;
}

/*
 * Take a packet off dtop interface
 * A packet MUST be there, this is not checked for.
 */
#define	DTOP_ESC_CHAR		0xf8
dtop_escape(c)
{
	/* I donno much about this stuff.. */
	switch (c) {
	case 0xe8:	return 0xf8;
	case 0xe9:	return 0xf9;
	case 0xea:	return 0xfa;
	case 0xeb:	return 0xfb;
	default:	/* printf("{esc %x}", c); */
			return c;
	}
}

dtop_get_packet(dtop, pkt)
	dtop_softc_t	dtop;
	dtop_message_t	pkt;
{
	register poll_reg_t	poll;
	register data_reg_t	data;
	register int		max, i, len;
	register unsigned char	c;

	poll = dtop->poll;
	data = dtop->data;

	/*
	 * The interface does not handle us the first byte,
	 * which is our address and cannot ever be anything
	 * else but 0x50.  This is a good thing, it makes
	 * the average packet exactly one word long, too.
	 */
	pkt->src_address = DTOP_GET_BYTE(data);

	for (max = 0; (max < DTOP_MAX_POLL) && !DTOP_RX_AVAIL(poll); max++)
		delay(16);
	if (max == DTOP_MAX_POLL) goto bad;
	pkt->code.bits = DTOP_GET_BYTE(data);

	/*
	 * Now get data and checksum
	 */
	len = pkt->code.val.len + 1;
	c = 0;
	for (i = 0; i < len; i++) {

again:		for (max = 0; (max < DTOP_MAX_POLL) && !DTOP_RX_AVAIL(poll); max++)
			delay(16);
		if (max == DTOP_MAX_POLL) goto bad;
		if (c == DTOP_ESC_CHAR) {
			c = dtop_escape(DTOP_GET_BYTE(data) & 0xff);
		} else {
			c = DTOP_GET_BYTE(data);
			if (c == DTOP_ESC_CHAR)
				goto again;
		}

		pkt->body[i] = c;
	}
	return len;
bad:
	dtop->bad_pkts++;
	return -1;
}

/* Conversely... */
dtop_put_packet(dtop, pkt)
	dtop_softc_t	dtop;
	dtop_message_t	pkt;
{
	register int i, max;
	register unsigned char *cp;
	register unsigned int spl;
	register unsigned char c;
	
	spl = spltty();
	pkt->src_address = pkt->dest_address;
	i = 0;
	cp = (unsigned char *)&pkt->src_address;
	while (i < pkt->code.val.len + 2) {
		for (max = 0; max < DTOP_MAX_POLL && !DTOP_TX_AVAIL(dtop->poll);
		     max++);
		if (max == DTOP_MAX_POLL)
			goto bad;
		DTOP_PUT_BYTE(dtop->data, *cp);
		cp++;
		i++;
	}
	for (max = 0; (max < DTOP_MAX_POLL) && !DTOP_RX_AVAIL(dtop->poll); max++)
		delay(16);
	if (max == DTOP_MAX_POLL)
		goto bad;
	c = DTOP_GET_BYTE(dtop->data);
	if (c == DTOP_ESC_CHAR) {
		for (max = 0; (max < DTOP_MAX_POLL)
		     && !DTOP_RX_AVAIL(dtop->poll); max++)
			delay(16);
		if (max == DTOP_MAX_POLL)
			goto bad;
		c = DTOP_GET_BYTE(dtop->data);
	}
	splx(spl);
	switch (c) {
	case 0xfb:		/* XMT, ok */
		break;
	default:
		return 0;
	}
	return 1;
 bad:
	splx(spl);
	return 0;
}


/*
 * Get a char from a specific DTOP line
 * [this is only used for console&screen purposes]
 */
dtop_getc( unit, line, wait, raw )
	boolean_t	wait;
	boolean_t	raw;
{
	register int c;
	dtop_softc_t	dtop;

	dtop = dtop_softc[unit];
again:
	c = -1;

	/*
	 * Try rconsole first
	 */
	if (rcline && line == SCREEN_LINE_KEYBOARD) {
		c = scc_getc( 0, rcline, FALSE, raw);
		if (c != -1) return c;
	}

	/*
	 * Now check keyboard
	 */
	if (DTOP_RX_AVAIL(dtop->poll)) {

		dtop_message	msg;
		struct dtop_ds	*ds;

		if (dtop_get_packet(dtop, &msg) >= 0) {

		    ds = &dtop->device[DTOP_DEVICE_NO(msg.src_address)];
		    if (ds->handler == dtop_keyboard_handler) {

			c = dtop_keyboard_handler(
					&ds->status, &msg,
					DTOP_EVENT_RECEIVE_PACKET, -1);

			if (c > 0) return c;

			c = -1;
		    }
		}
	}

	if (wait && (c == -1)) {
		delay(100);
		goto again;
	}

	return c;
}

/*
 * Put a char on a specific DTOP line
 */
dtop_putc( unit, line, c )
{
	if (rcline && line == rcline) {
		scc_putc(0, rcline, c);
	}
/*	dprintf("%c", c); */
}

dtop_param(tp, line)
	struct tty	*tp;
{
	if (tp->t_ispeed == 0)
		ttymodem(tp, 0);
	else
		/* called too early to invoke ttymodem, sigh */
		tp->t_state |= TS_CARR_ON;
}
 
/*
 * Modem control functions, we don't need 'em
 */
dtop_mctl(dev, bits, how)
	int dev;
	int bits, how;
{
	return 0;
}

dtop_softCAR(unit, line, on)
{
}

/* Some keyboard specific stuff, probably belongs elsewhere */

dtop_kbd_probe(unit)
{
	if (dtop_std[unit]) {
		lk201_probe(unit);
		return 1;
	}
	return 0;
}

io_return_t 
dtop_set_status(unit, flavor, status, status_count)
	int		unit;
	int		flavor;
	dev_status_t	status;
	unsigned int	status_count;
{
	dtop_device_t dev;

	dev = &dtop_softc[unit]->device[DTOP_DEVICE_NO(0x6c)].status;

	switch (flavor) {
	case LK201_SEND_CMD: {
		register lk201_cmd_t	*cmd = (lk201_cmd_t *)status;
		unsigned int		cnt;
		
		if ((status_count < (sizeof(*cmd)/sizeof(int))) ||
		    ((cnt = cmd->len) > 2))
			return D_INVALID_SIZE;
		switch (cmd->command) {
		case LK_CMD_ENB_BELL:
			cmd->params[0] ^= 0x7;
			if (dtop_print_debug)
				printf("LK_CMD_ENB_BELL %d\n", cmd->params[0]);
			dev->keyboard.bell_volume = cmd->params[0] & 0x7;
			break;
		case LK_CMD_DIS_BELL:
			dev->keyboard.bell_volume = 0;
			break;
		case LK_CMD_BELL:
			dtop_ring_bell(unit);
			break;
		case LK_CMD_LEDS_ON:
			cmd->params[0] &= ~0x80;
			if (dtop_print_debug)
				printf("LK_CMD_LEDS_ON %d %x\n",
				       cmd->params[0], cmd->params[0]);
			dev->keyboard.led_status |= cmd->params[0];
			dtop_leds(unit, dev->keyboard.led_status);
			break;
		case LK_CMD_LEDS_OFF:
			cmd->params[0] &= ~0x80;
			dev->keyboard.led_status &= ~cmd->params[0];
			dtop_leds(unit, dev->keyboard.led_status);
			break;
		case LK_CMD_ENB_KEYCLK:
		case LK_CMD_DIS_KEYCLK:
		case LK_CMD_SOUND_CLK:
		case LK_CMD_DIS_CTLCLK:
		case LK_CMD_ENB_CTLCLK:
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
	return lk201_set_status(unit, flavor, status, status_count);
}

dtop_kbd_reset(unit)
{
	return lk201_reset(unit);
}

#define DTOP_BITS(p, len)	(((p) << 7) | (len))

dtop_ring_bell(unit)
{
	dtop_message msg;
	dtop_device_t dev;
	int vol;

	dev = &dtop_softc[unit]->device[DTOP_DEVICE_NO(0x6c)].status;
	vol = dev->keyboard.bell_volume;

	if (dtop_print_debug)
		printf("dtop_ring_bell: %d\n", vol);
	msg.dest_address = DTOP_ADDR_KBD;
	msg.code.bits = DTOP_BITS(1, 2);
	msg.body[0] = DTOP_KMSG_BELL;
	msg.body[1] = vol;
	if (!dtop_put_packet(dtop_softc[unit], &msg)) {
	  if (dtop_print_debug)
	    printf("dtop_ring_bell: dtop_put_packet failed\n");
	  return -1;
	}
	return 0;
}

dtop_leds(unit, mask)
{
	dtop_message msg;

	if (dtop_print_debug)
		printf("dtop_leds %x\n", mask);
	msg.dest_address = DTOP_ADDR_KBD;
	msg.code.bits = DTOP_BITS(1, 2);
	msg.body[0] = DTOP_KMSG_LED;
	msg.body[1] = mask;
	if (!dtop_put_packet(dtop_softc[unit], &msg)) {
	  if (dtop_print_debug)
	    printf("dtop_leds: dtop_put_packet failed\n");
	  return -1;
	}
	return 0;
}



#endif	NDTOP > 0
