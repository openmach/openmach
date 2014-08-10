/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions of the socket-level I/O control calls.
 *
 * Version:	@(#)sockios.h	1.0.2	03/09/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_SOCKIOS_H
#define _LINUX_SOCKIOS_H

/* Routing table calls. */
#define SIOCADDRT	0x890B		/* add routing table entry	*/
#define SIOCDELRT	0x890C		/* delete routing table entry	*/

/* Socket configuration controls. */
#define SIOCGIFNAME	0x8910		/* get iface name		*/
#define SIOCSIFLINK	0x8911		/* set iface channel		*/
#define SIOCGIFCONF	0x8912		/* get iface list		*/
#define SIOCGIFFLAGS	0x8913		/* get flags			*/
#define SIOCSIFFLAGS	0x8914		/* set flags			*/
#define SIOCGIFADDR	0x8915		/* get PA address		*/
#define SIOCSIFADDR	0x8916		/* set PA address		*/
#define SIOCGIFDSTADDR	0x8917		/* get remote PA address	*/
#define SIOCSIFDSTADDR	0x8918		/* set remote PA address	*/
#define SIOCGIFBRDADDR	0x8919		/* get broadcast PA address	*/
#define SIOCSIFBRDADDR	0x891a		/* set broadcast PA address	*/
#define SIOCGIFNETMASK	0x891b		/* get network PA mask		*/
#define SIOCSIFNETMASK	0x891c		/* set network PA mask		*/
#define SIOCGIFMETRIC	0x891d		/* get metric			*/
#define SIOCSIFMETRIC	0x891e		/* set metric			*/
#define SIOCGIFMEM	0x891f		/* get memory address (BSD)	*/
#define SIOCSIFMEM	0x8920		/* set memory address (BSD)	*/
#define SIOCGIFMTU	0x8921		/* get MTU size			*/
#define SIOCSIFMTU	0x8922		/* set MTU size			*/
#define	SIOCSIFHWADDR	0x8924		/* set hardware address (NI)	*/
#define SIOCGIFENCAP	0x8925		/* get/set slip encapsulation   */
#define SIOCSIFENCAP	0x8926		
#define SIOCGIFHWADDR	0x8927		/* Get hardware address		*/
#define SIOCGIFSLAVE	0x8929		/* Driver slaving support	*/
#define SIOCSIFSLAVE	0x8930
/* begin multicast support change */
#define SIOCADDMULTI  0x8931
#define SIOCDELMULTI  0x8932
/* end multicast support change */

/* ARP cache control calls. */
#define OLD_SIOCDARP	0x8950		/* old delete ARP table entry	*/
#define OLD_SIOCGARP	0x8951		/* old get ARP table entry	*/
#define OLD_SIOCSARP	0x8952		/* old set ARP table entry	*/
#define SIOCDARP	0x8953		/* delete ARP table entry	*/
#define SIOCGARP	0x8954		/* get ARP table entry		*/
#define SIOCSARP	0x8955		/* set ARP table entry		*/

/* RARP cache control calls. */
#define SIOCDRARP	0x8960		/* delete RARP table entry	*/
#define SIOCGRARP	0x8961		/* get RARP table entry		*/
#define SIOCSRARP	0x8962		/* set RARP table entry		*/

/* Driver configuration calls */

#define SIOCGIFMAP	0x8970		/* Get device parameters	*/
#define SIOCSIFMAP	0x8971		/* Set device parameters	*/


/* Device private ioctl calls */

/*
 *	These 16 ioctls are available to devices via the do_ioctl() device
 *	vector. Each device should include this file and redefine these names
 *	as their own. Because these are device dependent it is a good idea
 *	_NOT_ to issue them to random objects and hope.
 */
 
#define SIOCDEVPRIVATE	0x89F0	/* to 89FF */

/*
 *	These 16 ioctl calls are protocol private
 */
 
#define SIOCPROTOPRIVATE 0x89E0 /* to 89EF */
#endif	/* _LINUX_SOCKIOS_H */
