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
/*
 *	File: lk201.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Routines for the LK201 Keyboard Driver
 */

#include <lk.h>
#if	NLK > 0
#include <bm.h>

#include <mach_kdb.h>

#include <mach/std_types.h>
#include <device/device_types.h>
#include <machine/machspl.h>		/* spl definitions */
#include <sys/ioctl.h>
#include <machine/machspl.h>
#include <chips/lk201.h>
#include <chips/serial_defs.h>
#include <chips/screen_defs.h>


/*
 * Structures describing the keyboard status
 */
typedef struct {
	unsigned short	kbd_flags;
	unsigned short	kbd_previous;
	char		kbd_gen_shift;
	char		kbd_ctrl;
	char		kbd_lock;
	char		kbd_meta;
	char		kbd_shift;
} lk201_kbd_state_t;

#define	mapLOCKtoCNTRL	0x1		/* flags */

typedef struct {
	char	led_active;
	char	led_pattern;
	char	led_increasing;
	char	led_lights;
	int	led_interval;
	int	led_light_count;
} lk201_led_state_t;


/*
 *	Keyboard state
 */

struct lk201_softc {
	lk201_kbd_state_t	kbd;
	lk201_led_state_t	led;
	int			sl_unit;
} lk201_softc_data[NLK];

typedef struct lk201_softc	*lk201_softc_t;

lk201_softc_t	lk201_softc[NLK];

/*
 * Forward decls
 */
io_return_t
lk201_translation(
	int	key,
	char	c,
	int	tabcode );

int
lk201_input(
	int		unit,
	unsigned short	data);


/*
 * Autoconf (sort-of)
 */
lk201_probe(
	int	unit)
{
	lk201_softc[unit] = &lk201_softc_data[unit];
	return 1;
}

void lk201_attach(
	int	unit,
	int	sl_unit)
{
	lk201_softc[unit]->sl_unit = sl_unit;
	lk201_selftest(unit);
}

/*
 *	Keyboard initialization
 */

static unsigned char lk201_reset_string[] = {
	LK_CMD_LEDS_ON, LK_PARAM_LED_MASK(0xf),	/* show we are resetting */
	LK_CMD_SET_DEFAULTS,
	LK_CMD_MODE(LK_MODE_RPT_DOWN,1),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,2),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,3),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,4),
	LK_CMD_MODE(LK_MODE_DOWN_UP,5),
	LK_CMD_MODE(LK_MODE_DOWN_UP,6),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,7),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,8),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,9),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,10),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,11),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,12),
	LK_CMD_MODE(LK_MODE_DOWN,13),
	LK_CMD_MODE(LK_MODE_RPT_DOWN,14),
	LK_CMD_ENB_RPT,
/*	LK_CMD_ENB_KEYCLK, LK_PARAM_VOLUME(4),	*/
	LK_CMD_DIS_KEYCLK,
	LK_CMD_RESUME,
	LK_CMD_ENB_BELL, LK_PARAM_VOLUME(4),
	LK_CMD_LEDS_OFF, LK_PARAM_LED_MASK(0xf)
};

void
lk201_reset(
	int	unit)
{
	register int    i, sl;
	register spl_t	s;
	lk201_softc_t	lk;

	lk = lk201_softc[unit];
	sl = lk->sl_unit;
	s = spltty();
	for (i = 0; i < sizeof(lk201_reset_string); i++) {
		(*console_putc)(sl,
				SCREEN_LINE_KEYBOARD,
				lk201_reset_string[i]);
		delay(100);
	}
	/* zero any state associated with previous keypresses */
	bzero(lk, sizeof(*lk));
	lk->sl_unit = sl;
	splx(s);
}

lk201_selftest(
	int	unit)
{
	int	messg[4], sl;
	spl_t	s;

	sl = lk201_softc[unit]->sl_unit;
	s = spltty();
	(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_REQ_ID);
	delay(10000);/* arbitrary */
	messg[0] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	messg[1] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	splx(s);

	printf("( lk201 id %x.%x", messg[0], messg[1]);

	s = spltty();
	(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_POWER_UP);

	/* cannot do this, waiting too long might cause receiver overruns */
/*	delay(80000);/* spec says 70 msecs or less */

	messg[0] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	messg[1] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	messg[2] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	messg[3] = (*console_getc)(sl, SCREEN_LINE_KEYBOARD, TRUE, TRUE);
	splx(s);

	printf(", self-test ");
	if (messg[0] != 0x01 || messg[1] || messg[2] || messg[3])
		printf("bad [%x %x %x %x]",
			messg[0], messg[1], messg[2], messg[3]);
	else
		printf("ok )");

	lk201_reset(unit);
}

/*
 *	Tinkerbell
 */
void
lk201_ring_bell(
	int	unit)
{
	spl_t s = spltty();
	(*console_putc)(lk201_softc[unit]->sl_unit, SCREEN_LINE_KEYBOARD, LK_CMD_BELL);
	splx(s);
}

/*
 *	Here is your LED toy, Bob
 */
void
lk201_lights(
	int		unit,
	boolean_t	on)
{
	unsigned int sl;
	spl_t	s;

	sl = lk201_softc[unit]->sl_unit;
	s = spltty();
	(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_OFF);
	(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(0xf));
	if (on < 16 && on > 0) {
		(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_ON);
		(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(on));
	}
	splx(s);
}


lk201_led(
	int	unit)
{
	lk201_led_state_t	*leds = &lk201_softc[unit]->led;
	unsigned int		sl;
	spl_t			s;

	sl = lk201_softc[unit]->sl_unit;
	if (leds->led_interval) {		/* leds are on */
	    if (leds->led_light_count <= 0) {	/* hit this lights */

	        if (leds->led_lights <= 0) leds->led_lights= 1;	/* sanity */
	        if (leds->led_lights > 16) leds->led_lights = 16;/* sanity */
	        leds->led_light_count = leds->led_interval;	/* reset */
		s = spltty();
	        (*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_OFF);
	        (*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(leds->led_lights));
	        switch (leds->led_pattern) {
	        case LED_OFF:
	            leds->led_interval = 0;	/* since you can now set */
	            break;			/* the interval even if off */
	    	case LED_COUNT:
	            leds->led_lights++;
	            if (leds->led_lights > 16) leds->led_lights = 1;
	            break;
	    	case LED_ROTATE:
	            leds->led_lights <<= 1;
	            if (leds->led_lights > 8) leds->led_lights = 1;
	            break;
	    	case LED_CYLON:
	            if (leds->led_increasing) {
	            	leds->led_lights <<= 1;
	                if (leds->led_lights > 8) {
	                    leds->led_lights >>= 2;
	                    leds->led_increasing = 0;
	                }
	            } else {
	            	leds->led_lights >>= 1;
	                if (leds->led_lights <= 0) {
	                    leds->led_lights = 2;
	                    leds->led_increasing = 1;
	                }
	            }
	            break;
	        }
	        (*console_putc)( sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_ON);
	        (*console_putc)( sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(leds->led_lights));
		splx(s);
	    }
	    leds->led_light_count--;
	} else {
	    if (leds->led_lights) {
		s = spltty();
	        (*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_OFF);
	        (*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(0xf));
	        leds->led_lights = 0;
		splx(s);
	    }
	    leds->led_active = 0;
#if	NBM > 0
	    screen_enable_vretrace(unit, 0); /* interrupts off */
#endif
	}
}


/*
 *	Special user-visible ops
 */
io_return_t
lk201_set_status(
	int		unit,
	dev_flavor_t	flavor,
	dev_status_t	status,
	natural_t	status_count)
{
	lk201_led_state_t	*leds = &lk201_softc[unit]->led;
	lk201_kbd_state_t	*kbd  = &lk201_softc[unit]->kbd;

	switch( flavor ) {
	    case LK201_SEND_CMD:{
		register lk201_cmd_t	*cmd = (lk201_cmd_t*)status;
		unsigned int		 cnt, s, sl;

		if ((status_count < (sizeof(*cmd)/sizeof(int))) ||
		    ((cnt = cmd->len) > 2))
			return D_INVALID_SIZE;

		if (cnt == 0)
			cmd->command |= LK_PARAM;
		else
			cmd->params[cnt-1] |= LK_PARAM;
		sl = lk201_softc[unit]->sl_unit;
		s = spltty();
		(*console_putc)(sl, SCREEN_LINE_KEYBOARD, cmd->command);
		if (cnt > 0)
			(*console_putc)(sl, SCREEN_LINE_KEYBOARD, cmd->params[0]);
		if (cnt > 1)
			(*console_putc)(sl, SCREEN_LINE_KEYBOARD, cmd->params[1]);
		splx(s);
		return D_SUCCESS;
	    }
	    case LK201_LED_PATTERN:{
		register int ptn = * (int *) status;
		if (ptn != LED_OFF && ptn != LED_COUNT &&
		    ptn != LED_ROTATE && ptn != LED_CYLON ) {
			return -1;
		} else {
			leds->led_pattern = ptn;
		}
		break;
	    }
	    case LK201_LED_INTERVAL:{
		register int lcnt = * (int *) status;
		if (lcnt < 0)
			lcnt = 1;
		leds->led_interval = lcnt;
		break;
	    }
	    case LK201_mapLOCKtoCNTRL:{
		boolean_t	enable = * (boolean_t*) status;
		if (enable)
			kbd->kbd_flags |= mapLOCKtoCNTRL;
		else
			kbd->kbd_flags &= ~mapLOCKtoCNTRL;
		return D_SUCCESS;
	    }
	    case LK201_REMAP_KEY:{
		register KeyMap *k = (KeyMap *) status;
		int		 mode;

		if (status_count < (sizeof(KeyMap)/sizeof(int)))
			return D_INVALID_SIZE;

		mode = k->shifted ? 1 : 0;
		if (k->meta) mode += 2;
		return lk201_translation( k->in_keyval,
					  k->out_keyval,
					  mode );
	    }
	    default:
		return D_INVALID_OPERATION;
	}
	leds->led_lights = 1;
	leds->led_active = 1;
#if	NBM > 0
	screen_enable_vretrace(unit, 1); /* interrupts on */
#endif
	return D_SUCCESS;
}

/*
 *	Keycode translation tables
 *
 *	NOTE: these tables have been compressed a little bit
 *	because the lk201 cannot generate very small codes.
 */

unsigned char	lk201_xlate_key[] = {
  /*  86 */	 0  ,0
  /*  88 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /*  96 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 104 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 112 */	,0  ,0x1b  ,0x08  ,'\n' ,0  ,0  ,0  ,0
  /* 120 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 128 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 136 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 144 */	,0  ,0  ,'0',0  ,'.','\r','1','2'
  /* 152 */	,'3','4','5','6',',','7','8','9'
  /* 160 */	,'-',0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 168 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 176 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 184 */	,0  ,0  ,0  ,0  ,0x7f,'\r','\t','`'
  /* 192 */	,'1'   ,'q'   ,'a'   ,'z'   ,0  ,'2'   ,'w'   ,'s'
  /* 200 */	,'x'   ,'<'   ,0  ,'3'   ,'e'   ,'d'   ,'c'   ,0
  /* 208 */	,'4'   ,'r'   ,'f'   ,'v'   ,' '   ,0  ,'5'   ,'t'
  /* 216 */	,'g'   ,'b'   ,0  ,'6'   ,'y'   ,'h'   ,'n'   ,0
  /* 224 */	,'7'   ,'u'   ,'j'   ,'m'   ,0  ,'8'   ,'i'   ,'k'
  /* 232 */	,','   ,0  ,'9'   ,'o'   ,'l'   ,'.'   ,0  ,'0'
  /* 240 */	,'p'   ,0  ,';'   ,'/'   ,0  ,'='   ,']'   ,'\\'
  /* 248 */	,0  ,'-'   ,'['   ,'\''  ,0  ,0  ,0  ,0
};

unsigned char	lk201_xlate_shifted[] = {
  /*  86 */	 0  ,0
  /*  88 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /*  96 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 104 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 112 */	,0  ,0x1b  ,0x08  ,'\n'  ,0  ,0  ,0  ,0
  /* 120 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 128 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 136 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 144 */	,0  ,0  ,'0',0  ,'.','\r','1','2'
  /* 152 */	,'3','4','5','6',',','7','8','9'
  /* 160 */	,'-',0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 168 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 176 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 184 */	,0  ,0  ,0  ,0  ,0x7f  ,'\r'  ,'\t'  ,'~'
  /* 192 */	,'!'   ,'Q'   ,'A'   ,'Z'   ,0  ,'@'   ,'W'   ,'S'
  /* 200 */	,'X'   ,'>'   ,0  ,'#'   ,'E'   ,'D'   ,'C'   ,0
  /* 208 */	,'$'   ,'R'   ,'F'   ,'V'   ,' '   ,0  ,'%'   ,'T'
  /* 216 */	,'G'   ,'B'   ,0  ,'^'   ,'Y'   ,'H'   ,'N'   ,0
  /* 224 */	,'&'   ,'U'   ,'J'   ,'M'   ,0  ,'*'   ,'I'   ,'K'
  /* 232 */	,'<'   ,0  ,'('   ,'O'   ,'L'   ,'>'   ,0  ,')'
  /* 240 */	,'P'   ,0  ,':'   ,'?'   ,0  ,'+'   ,'}'   ,'|'
  /* 248 */	,0  ,'_'   ,'{'   ,'"'   ,0  ,0  ,0  ,0
};

unsigned char	lk201_xlate_meta[] = {
  /*  86 */	 0  ,0
  /*  88 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /*  96 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 104 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 112 */	,0  ,0x1b  ,0x08  ,'\n'  ,0  ,0  ,0  ,0
  /* 120 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 128 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 136 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 144 */	,0  ,0  ,'0',0  ,'.','\r','1','2'
  /* 152 */	,'3','4','5','6',',','7','8','9'
  /* 160 */	,'-',0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 168 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 176 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 184 */	,0  ,0  ,0  ,0  ,0x7f  ,'\r'  ,'\t'  ,'~'
  /* 192 */	,'!'   ,'Q'   ,'A'   ,'Z'   ,0  ,'@'   ,'W'   ,'S'
  /* 200 */	,'X'   ,'>'   ,0  ,'#'   ,'E'   ,'D'   ,'C'   ,0
  /* 208 */	,'$'   ,'R'   ,'F'   ,'V'   ,' '   ,0  ,'%'   ,'T'
  /* 216 */	,'G'   ,'B'   ,0  ,'^'   ,'Y'   ,'H'   ,'N'   ,0
  /* 224 */	,'&'   ,'U'   ,'J'   ,'M'   ,0  ,'*'   ,'I'   ,'K'
  /* 232 */	,'<'   ,0  ,'('   ,'O'   ,'L'   ,'>'   ,0  ,')'
  /* 240 */	,'P'   ,0  ,':'   ,'?'   ,0  ,'+'   ,'}'   ,'|'
  /* 248 */	,0  ,'_'   ,'{'   ,'"'   ,0  ,0  ,0  ,0
};

unsigned char	lk201_xlate_shifted_meta[] = {
  /*  86 */	 0  ,0
  /*  88 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /*  96 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 104 */ 	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 112 */	,0  ,0x1b  ,0x08  ,'\n'  ,0  ,0  ,0  ,0
  /* 120 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 128 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 136 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 144 */	,0  ,0  ,'0',0  ,'.','\r','1','2'
  /* 152 */	,'3','4','5','6',',','7','8','9'
  /* 160 */	,'-',0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 168 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 176 */	,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0
  /* 184 */	,0  ,0  ,0  ,0  ,0x7f  ,'\r'  ,'\t'  ,'~'
  /* 192 */	,'!'   ,'Q'   ,'A'   ,'Z'   ,0  ,'@'   ,'W'   ,'S'
  /* 200 */	,'X'   ,'>'   ,0  ,'#'   ,'E'   ,'D'   ,'C'   ,0
  /* 208 */	,'$'   ,'R'   ,'F'   ,'V'   ,' '   ,0  ,'%'   ,'T'
  /* 216 */	,'G'   ,'B'   ,0  ,'^'   ,'Y'   ,'H'   ,'N'   ,0
  /* 224 */	,'&'   ,'U'   ,'J'   ,'M'   ,0  ,'*'   ,'I'   ,'K'
  /* 232 */	,'<'   ,0  ,'('   ,'O'   ,'L'   ,'>'   ,0  ,')'
  /* 240 */	,'P'   ,0  ,':'   ,'?'   ,0  ,'+'   ,'}'   ,'|'
  /* 248 */	,0  ,'_'   ,'{'   ,'"'   ,0  ,0  ,0  ,0
};


io_return_t
lk201_translation(
	int	key,
	char	c,
	int	tabcode )
{
	unsigned char *table;

	if ((key &= 0xff) < LK_MINCODE)
		return D_INVALID_OPERATION;

	switch (tabcode) {
	    case 3:
		table = lk201_xlate_shifted_meta;
		break;
	    case 2:
		table = lk201_xlate_meta;
		break;
	    case 1:
		table = lk201_xlate_shifted;
		break;
	    case 0:
	    default:
		table = lk201_xlate_key;
		break;
	}
	table[key - LK_MINCODE] = c;
	return D_SUCCESS;
}

/*
 *	Input character processing
 */

lk201_rint(
	int		unit,
	unsigned short	data,
	boolean_t	handle_shift,
	boolean_t	from_kernel)
{
	int             c;
	lk201_kbd_state_t	*kbd = &lk201_softc[unit]->kbd;

	/*
	 * Keyboard touched, clean char to 8 bits.
	 */
#if	NBM > 0
	ssaver_bump(unit);
#endif

	data &= 0xff;

	/* Translate keycode into ASCII */
	if ((c = lk201_input(unit, data)) == -1)
		return -1;

#if	NBM > 0
	/*
	 * Notify X, unless we are called from inside kernel
	 */
	if (!from_kernel &&
	    screen_keypress_event(unit, DEV_KEYBD, data, EVT_BUTTON_RAW))
		return -1;
#endif

	/* Handle shifting if need to */
	if (kbd->kbd_gen_shift)
		return (handle_shift) ? cngetc() : -1;

	return c;
}

/*
 * Routine to grock a character from LK201
 */
#if	MACH_KDB
int	lk201_allow_kdb = 1;
#endif

int lk201_debug = 0;

lk201_input(
	int		unit,
	unsigned short	data)
{
	int			c, sl;
	lk201_kbd_state_t	*kbd = &lk201_softc[unit]->kbd;

	kbd->kbd_gen_shift = 0;

#if	MACH_KDB
	if (lk201_allow_kdb && (data == LK_DO)) {
		kdb_kintr();
		return -2;
	}
#endif

        /*
         * Sanity checks
         */

	if (data == LK_INPUT_ERR || data == LK_OUTPUT_ERR) {
		printf(" Keyboard error, code = %x\n",data);
		return -1;
	}
	if (data < LK_MINCODE)
		return -1;

        /*
	 * Check special keys: shifts, ups, ..
         */

	if (data == LK_LOCK && (kbd->kbd_flags&mapLOCKtoCNTRL))
		data = LK_CNTRL;

	switch (data) {
	case LK_LOCK:
		kbd->kbd_lock ^= 1;
		kbd->kbd_gen_shift = 1;
		sl = lk201_softc[unit]->sl_unit;
		/* called from interrupt, no need for spl */
		if (kbd->kbd_lock)
			(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_ON);
		else
			(*console_putc)(sl,SCREEN_LINE_KEYBOARD, LK_CMD_LEDS_OFF);
		(*console_putc)(sl, SCREEN_LINE_KEYBOARD, LK_PARAM_LED_MASK(0x4));
		return 0;

	case LK_ALT:
	case LK_L_ALT:
	case LK_R_ALT:
	case LK_R_COMPOSE:
		kbd->kbd_meta ^= 1;
		kbd->kbd_gen_shift = 1;
		return 0;

	case LK_SHIFT:
	case LK_R_SHIFT:
		kbd->kbd_shift ^= 1;
		kbd->kbd_gen_shift = 1;
		return 0;

	case LK_CNTRL:
		kbd->kbd_ctrl ^= 1;
		kbd->kbd_gen_shift = 1;
		return 0;

	case LK_ALLUP:
		kbd->kbd_ctrl  = 0;
		kbd->kbd_shift = 0;
		kbd->kbd_meta  = 0;
		kbd->kbd_gen_shift = 1;
		return 0;

	case LK_REPEAT:
		c = kbd->kbd_previous;
		break;

	default:

                /*
		 * Do the key translation to ASCII
                 */
		if (kbd->kbd_ctrl || kbd->kbd_lock || kbd->kbd_shift) {
		    c = ((kbd->kbd_meta) ? 
			    lk201_xlate_shifted_meta : lk201_xlate_shifted)
			[data - LK_MINCODE];
		    if (kbd->kbd_ctrl)
			    c &= 0x1f;
		} else
		    c = ((kbd->kbd_meta) ?
		    	    lk201_xlate_meta : lk201_xlate_key)
			[data-LK_MINCODE];
		break;

	}

	kbd->kbd_previous = c;

        /*
	 * DEBUG code DEBUG
         */
	if (lk201_debug && (c == 0)) {
	    printf("lk201: [%x]\n", data);
	}

	return c;
}

#endif	/* NLK > 0 */
