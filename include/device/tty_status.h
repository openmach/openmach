/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 *	Date: 	ll/90
 *
 * 	Status information for tty.
 */

struct tty_status {
	int	tt_ispeed;		/* input speed */
	int	tt_ospeed;		/* output speed */
	int	tt_breakc;		/* character to deliver when break
					   detected on line */
	int	tt_flags;		/* mode flags */
};
#define	TTY_STATUS_COUNT	(sizeof(struct tty_status)/sizeof(int))
#define	TTY_STATUS		(dev_flavor_t)(('t'<<16) + 1)

/*
 * Speeds
 */
#define B0	0
#define B50	1
#define B75	2
#define B110	3
#define B134	4
#define B150	5
#define B200	6
#define B300	7
#define B600	8
#define B1200	9
#define	B1800	10
#define B2400	11
#define B4800	12
#define B9600	13
#define EXTA	14 /* XX can we just get rid of EXTA and EXTB? */
#define EXTB	15
#define B19200	EXTA
#define B38400  EXTB

#define	NSPEEDS	16

/*
 * Flags
 */
#define	TF_TANDEM	0x00000001	/* send stop character when input
					   queue full */
#define	TF_ODDP		0x00000002	/* get/send odd parity */
#define	TF_EVENP	0x00000004	/* get/send even parity */
#define	TF_ANYP		(TF_ODDP|TF_EVENP)
					/* get any parity/send none */
#define	TF_LITOUT	0x00000008	/* output all 8 bits
					   otherwise, characters >= 0x80
					   are time delays	XXX */
#define	TF_MDMBUF	0x00000010	/* start/stop output on carrier
					   interrupt
					   otherwise, dropping carrier
					   hangs up line */
#define	TF_NOHANG	0x00000020	/* no hangup signal on carrier drop */
#define	TF_HUPCLS	0x00000040	/* hang up (outgoing) on last close */

/*
 * Read-only flags - information about device
 */
#define	TF_ECHO		0x00000080	/* device wants user to echo input */
#define	TF_CRMOD	0x00000100	/* device wants \r\n, not \n */
#define	TF_XTABS	0x00000200	/* device does not understand tabs */

/*
 * Modem control
 */
#define	TTY_MODEM_COUNT		(1)	/* one integer */
#define	TTY_MODEM		(dev_flavor_t)(('t'<<16) + 2)

#define	TM_LE		0x0001		/* line enable */
#define	TM_DTR		0x0002		/* data terminal ready */
#define	TM_RTS		0x0004		/* request to send */
#define	TM_ST		0x0008		/* secondary transmit */
#define	TM_SR		0x0010		/* secondary receive */
#define	TM_CTS		0x0020		/* clear to send */
#define	TM_CAR		0x0040		/* carrier detect */
#define	TM_RNG		0x0080		/* ring */
#define	TM_DSR		0x0100		/* data set ready */

#define	TM_BRK		0x0200		/* set line break (internal) */
#define	TM_HUP		0x0000		/* close line (internal) */

/*
 * Other controls
 */
#define	TTY_FLUSH_COUNT		(1)	/* one integer - D_READ|D_WRITE */
#define	TTY_FLUSH		(dev_flavor_t)(('t'<<16) + 3)
					/* flush input or output */
#define	TTY_STOP		(dev_flavor_t)(('t'<<16) + 4)
					/* stop output */
#define	TTY_START		(dev_flavor_t)(('t'<<16) + 5)
					/* start output */
#define	TTY_SET_BREAK		(dev_flavor_t)(('t'<<16) + 6)
					/* set break condition */
#define	TTY_CLEAR_BREAK		(dev_flavor_t)(('t'<<16) + 7)
					/* clear break condition */
#define TTY_SET_TRANSLATION	(dev_flavor_t)(('t'<<16) + 8)
					/* set translation table */
