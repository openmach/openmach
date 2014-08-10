/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Holds initial configuration information for devices.
 *
 * NOTE:	This file is a nice idea, but its current format does not work
 *		well for drivers that support multiple units, like the SLIP
 *		driver.  We should actually have only one pointer to a driver
 *		here, with the driver knowing how many units it supports.
 *		Currently, the SLIP driver abuses the "base_addr" integer
 *		field of the 'device' structure to store the unit number...
 *		-FvK
 *
 * Version:	@(#)Space.c	1.0.7	08/12/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Donald J. Becker, <becker@super.org>
 *
 *	FIXME:
 *		Sort the device chain fastest first.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/netdevice.h>
#include <linux/errno.h>

#define	NEXT_DEV	NULL


/* A unified ethernet device probe.  This is the easiest way to have every
   ethernet adaptor have the name "eth[0123...]".
   */

extern int hp100_probe(struct device *dev);
extern int ultra_probe(struct device *dev);
extern int wd_probe(struct device *dev);
extern int el2_probe(struct device *dev);
extern int ne_probe(struct device *dev);
extern int hp_probe(struct device *dev);
extern int hp_plus_probe(struct device *dev);
extern int znet_probe(struct device *);
extern int express_probe(struct device *);
extern int eepro_probe(struct device *);
extern int el3_probe(struct device *);
extern int at1500_probe(struct device *);
extern int at1700_probe(struct device *);
extern int eth16i_probe(struct device *);
extern int depca_probe(struct device *);
extern int apricot_probe(struct device *);
extern int ewrk3_probe(struct device *);
extern int de4x5_probe(struct device *);
extern int el1_probe(struct device *);
#if	defined(CONFIG_WAVELAN)
extern int wavelan_probe(struct device *);
#endif	/* defined(CONFIG_WAVELAN) */
extern int el16_probe(struct device *);
extern int elplus_probe(struct device *);
extern int ac3200_probe(struct device *);
extern int e2100_probe(struct device *);
extern int ni52_probe(struct device *);
extern int ni65_probe(struct device *);
extern int SK_init(struct device *);
extern int seeq8005_probe(struct device *);
extern int tc59x_probe(struct device *);

/* Detachable devices ("pocket adaptors") */
extern int atp_init(struct device *);
extern int de600_probe(struct device *);
extern int de620_probe(struct device *);

static int
ethif_probe(struct device *dev)
{
    u_long base_addr = dev->base_addr;

    if ((base_addr == 0xffe0)  ||  (base_addr == 1))
	return 1;		/* ENXIO */

    if (1
#if defined(CONFIG_VORTEX)
	&& tc59x_probe(dev)
#endif
#if defined(CONFIG_SEEQ8005)
	&& seeq8005_probe(dev)
#endif
#if defined(CONFIG_HP100)
	&& hp100_probe(dev)
#endif	
#if defined(CONFIG_ULTRA)
	&& ultra_probe(dev)
#endif
#if defined(CONFIG_WD80x3) || defined(WD80x3)
	&& wd_probe(dev)
#endif
#if defined(CONFIG_EL2) || defined(EL2)	/* 3c503 */
	&& el2_probe(dev)
#endif
#if defined(CONFIG_HPLAN) || defined(HPLAN)
	&& hp_probe(dev)
#endif
#if 0
#if defined(CONFIG_HPLAN_PLUS)
	&& hp_plus_probe(dev)
#endif
#endif
#ifdef CONFIG_AC3200		/* Ansel Communications EISA 3200. */
	&& ac3200_probe(dev)
#endif
#ifdef CONFIG_E2100		/* Cabletron E21xx series. */
	&& e2100_probe(dev)
#endif
#if defined(CONFIG_NE2000) || defined(NE2000)
	&& ne_probe(dev)
#endif
#ifdef CONFIG_AT1500
	&& at1500_probe(dev)
#endif
#ifdef CONFIG_AT1700
	&& at1700_probe(dev)
#endif
#ifdef CONFIG_ETH16I
	&& eth16i_probe(dev)	/* ICL EtherTeam 16i/32 */
#endif
#ifdef CONFIG_EL3		/* 3c509 */
	&& el3_probe(dev)
#endif
#ifdef CONFIG_ZNET		/* Zenith Z-Note and some IBM Thinkpads. */
	&& znet_probe(dev)
#endif
#ifdef CONFIG_EEXPRESS		/* Intel EtherExpress */
	&& express_probe(dev)
#endif
#ifdef CONFIG_EEXPRESS_PRO	/* Intel EtherExpress Pro/10 */
	&& eepro_probe(dev)
#endif
#ifdef CONFIG_DEPCA		/* DEC DEPCA */
	&& depca_probe(dev)
#endif
#ifdef CONFIG_EWRK3             /* DEC EtherWORKS 3 */
        && ewrk3_probe(dev)
#endif
#ifdef CONFIG_DE4X5             /* DEC DE425, DE434, DE435 adapters */
        && de4x5_probe(dev)
#endif
#ifdef CONFIG_APRICOT		/* Apricot I82596 */
	&& apricot_probe(dev)
#endif
#ifdef CONFIG_EL1		/* 3c501 */
	&& el1_probe(dev)
#endif
#if	defined(CONFIG_WAVELAN)	/* WaveLAN */
	&& wavelan_probe(dev)
#endif	/* defined(CONFIG_WAVELAN) */
#ifdef CONFIG_EL16		/* 3c507 */
	&& el16_probe(dev)
#endif
#ifdef CONFIG_ELPLUS		/* 3c505 */
	&& elplus_probe(dev)
#endif
#ifdef CONFIG_DE600		/* D-Link DE-600 adapter */
	&& de600_probe(dev)
#endif
#ifdef CONFIG_DE620		/* D-Link DE-620 adapter */
	&& de620_probe(dev)
#endif
#if defined(CONFIG_SK_G16)
	&& SK_init(dev)
#endif
#ifdef CONFIG_NI52
	&& ni52_probe(dev)
#endif
#ifdef CONFIG_NI65
	&& ni65_probe(dev)
#endif
	&& 1 ) {
	return 1;	/* -ENODEV or -EAGAIN would be more accurate. */
    }
    return 0;
}


#ifdef CONFIG_NETROM
	extern int nr_init(struct device *);
	
	static struct device nr3_dev = { "nr3", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, nr_init, };
	static struct device nr2_dev = { "nr2", 0, 0, 0, 0, 0, 0, 0, 0, 0, &nr3_dev, nr_init, };
	static struct device nr1_dev = { "nr1", 0, 0, 0, 0, 0, 0, 0, 0, 0, &nr2_dev, nr_init, };
	static struct device nr0_dev = { "nr0", 0, 0, 0, 0, 0, 0, 0, 0, 0, &nr1_dev, nr_init, };

#   undef NEXT_DEV
#   define	NEXT_DEV	(&nr0_dev)
#endif

/* Run-time ATtachable (Pocket) devices have a different (not "eth#") name. */
#ifdef CONFIG_ATP		/* AT-LAN-TEC (RealTek) pocket adaptor. */
static struct device atp_dev = {
    "atp0", 0, 0, 0, 0, 0, 0, 0, 0, 0, NEXT_DEV, atp_init, /* ... */ };
#   undef NEXT_DEV
#   define NEXT_DEV	(&atp_dev)
#endif

#ifdef CONFIG_ARCNET
    extern int arcnet_probe(struct device *dev);
    static struct device arcnet_dev = {
	"arc0", 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NEXT_DEV, arcnet_probe, };
#   undef	NEXT_DEV
#   define	NEXT_DEV	(&arcnet_dev)
#endif

/* In Mach, by default allow at least 2 interfaces.  */
#ifdef MACH
#ifndef ETH1_ADDR
# define ETH1_ADDR 0
#endif
#ifndef ETH1_IRQ
# define ETH1_IRQ 0
#endif
#endif

/* The first device defaults to I/O base '0', which means autoprobe. */
#ifndef ETH0_ADDR
# define ETH0_ADDR 0
#endif
#ifndef ETH0_IRQ
# define ETH0_IRQ 0
#endif
/* "eth0" defaults to autoprobe (== 0), other use a base of 0xffe0 (== -0x20),
   which means "don't probe".  These entries exist to only to provide empty
   slots which may be enabled at boot-time. */

static struct device eth3_dev = {
    "eth3", 0,0,0,0,0xffe0 /* I/O base*/, 0,0,0,0, NEXT_DEV, ethif_probe };
static struct device eth2_dev = {
    "eth2", 0,0,0,0,0xffe0 /* I/O base*/, 0,0,0,0, &eth3_dev, ethif_probe };
#ifdef MACH
static struct device eth1_dev = {
    "eth1", 0,0,0,0,ETH1_ADDR, ETH1_IRQ,0,0,0, &eth2_dev, ethif_probe };
#else
static struct device eth1_dev = {
    "eth1", 0,0,0,0,0xffe0 /* I/O base*/, 0,0,0,0, &eth2_dev, ethif_probe };
#endif
static struct device eth0_dev = {
    "eth0", 0, 0, 0, 0, ETH0_ADDR, ETH0_IRQ, 0, 0, 0, &eth1_dev, ethif_probe };

#   undef NEXT_DEV
#   define NEXT_DEV	(&eth0_dev)

#if defined(PLIP) || defined(CONFIG_PLIP)
    extern int plip_init(struct device *);
    static struct device plip2_dev = {
	"plip2", 0, 0, 0, 0, 0x278, 2, 0, 0, 0, NEXT_DEV, plip_init, };
    static struct device plip1_dev = {
	"plip1", 0, 0, 0, 0, 0x378, 7, 0, 0, 0, &plip2_dev, plip_init, };
    static struct device plip0_dev = {
	"plip0", 0, 0, 0, 0, 0x3BC, 5, 0, 0, 0, &plip1_dev, plip_init, };
#   undef NEXT_DEV
#   define NEXT_DEV	(&plip0_dev)
#endif  /* PLIP */

#if defined(SLIP) || defined(CONFIG_SLIP)
	/* To be exact, this node just hooks the initialization
	   routines to the device structures.			*/
extern int slip_init_ctrl_dev(struct device *);
static struct device slip_bootstrap = {
  "slip_proto", 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NEXT_DEV, slip_init_ctrl_dev, };
#undef NEXT_DEV
#define NEXT_DEV (&slip_bootstrap)
#endif	/* SLIP */
  
#if defined(CONFIG_PPP)
extern int ppp_init(struct device *);
static struct device ppp_bootstrap = {
    "ppp_proto", 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NEXT_DEV, ppp_init, };
#undef NEXT_DEV
#define NEXT_DEV (&ppp_bootstrap)
#endif   /* PPP */

#ifdef CONFIG_DUMMY
    extern int dummy_init(struct device *dev);
    static struct device dummy_dev = {
	"dummy", 0x0, 0x0, 0x0, 0x0, 0, 0, 0, 0, 0, NEXT_DEV, dummy_init, };
#   undef	NEXT_DEV
#   define	NEXT_DEV	(&dummy_dev)
#endif

#ifdef CONFIG_EQUALIZER
extern int eql_init(struct device *dev);
struct device eql_dev = {
  "eql",			/* Master device for IP traffic load 
				   balancing */
  0x0, 0x0, 0x0, 0x0,		/* recv end/start; mem end/start */
  0,				/* base I/O address */
  0,				/* IRQ */
  0, 0, 0,			/* flags */
  NEXT_DEV,			/* next device */
  eql_init			/* set up the rest */
};
#   undef       NEXT_DEV
#   define      NEXT_DEV        (&eql_dev)
#endif

#ifdef CONFIG_IBMTR 

    extern int tok_probe(struct device *dev);
    static struct device ibmtr_dev1 = {
	"tr1",			/* IBM Token Ring (Non-DMA) Interface */
	0x0,			/* recv memory end			*/
	0x0,			/* recv memory start			*/
	0x0,			/* memory end				*/
	0x0,			/* memory start				*/
	0xa24,			/* base I/O address			*/
	0,			/* IRQ					*/
	0, 0, 0,		/* flags				*/
	NEXT_DEV,		/* next device				*/
	tok_probe		/* ??? Token_init should set up the rest	*/
    };
#   undef	NEXT_DEV
#   define	NEXT_DEV	(&ibmtr_dev1)


    static struct device ibmtr_dev0 = {
	"tr0",			/* IBM Token Ring (Non-DMA) Interface */
	0x0,			/* recv memory end			*/
	0x0,			/* recv memory start			*/
	0x0,			/* memory end				*/
	0x0,			/* memory start				*/
	0xa20,			/* base I/O address			*/
	0,			/* IRQ					*/
	0, 0, 0,		/* flags				*/
	NEXT_DEV,		/* next device				*/
	tok_probe		/* ??? Token_init should set up the rest	*/
    };
#   undef	NEXT_DEV
#   define	NEXT_DEV	(&ibmtr_dev0)

#endif 
#ifdef CONFIG_NET_IPIP
#ifdef CONFIG_IP_FORWARD
	extern int tunnel_init(struct device *);
	
	static struct device tunnel_dev1 = 
	{
		"tunl1",		/* IPIP tunnel  			*/
		0x0,			/* recv memory end			*/
		0x0,			/* recv memory start			*/
		0x0,			/* memory end				*/
		0x0,			/* memory start				*/
		0x0,			/* base I/O address			*/
		0,			/* IRQ					*/
		0, 0, 0,		/* flags				*/
		NEXT_DEV,		/* next device				*/
		tunnel_init		/* Fill in the details			*/
	};

	static struct device tunnel_dev0 = 
	{
		"tunl0",		/* IPIP tunnel  			*/
		0x0,			/* recv memory end			*/
		0x0,			/* recv memory start			*/
		0x0,			/* memory end				*/
		0x0,			/* memory start				*/
		0x0,			/* base I/O address			*/
		0,			/* IRQ					*/
		0, 0, 0,		/* flags				*/
		&tunnel_dev1,		/* next device				*/
		tunnel_init		/* Fill in the details			*/
    };
#   undef	NEXT_DEV
#   define	NEXT_DEV	(&tunnel_dev0)

#endif 
#endif

#ifdef MACH
struct device *dev_base = &eth0_dev;
#else	
extern int loopback_init(struct device *dev);
struct device loopback_dev = {
	"lo",			/* Software Loopback interface		*/
	0x0,			/* recv memory end			*/
	0x0,			/* recv memory start			*/
	0x0,			/* memory end				*/
	0x0,			/* memory start				*/
	0,			/* base I/O address			*/
	0,			/* IRQ					*/
	0, 0, 0,		/* flags				*/
	NEXT_DEV,		/* next device				*/
	loopback_init		/* loopback_init should set up the rest	*/
};

struct device *dev_base = &loopback_dev;
#endif
