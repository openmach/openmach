/*
 * AT&T GIS (nee NCR) WaveLAN card:
 *	An Ethernet-like radio transceiver
 *	controlled by an Intel 82586 coprocessor.
 */

#include	<linux/module.h>

#include	<linux/kernel.h>
#include	<linux/sched.h>
#include	<linux/types.h>
#include	<linux/fcntl.h>
#include	<linux/interrupt.h>
#include	<linux/stat.h>
#include	<linux/ptrace.h>
#include	<linux/ioport.h>
#include	<linux/in.h>
#include	<linux/string.h>
#include	<linux/delay.h>
#include	<asm/system.h>
#include	<asm/bitops.h>
#include	<asm/io.h>
#include	<asm/dma.h>
#include	<linux/errno.h>
#include	<linux/netdevice.h>
#include	<linux/etherdevice.h>
#include	<linux/skbuff.h>
#include	<linux/malloc.h>
#include	<linux/timer.h>
#include	<linux/proc_fs.h>
#define	STRUCT_CHECK	1
#ifdef MACH
#include	<net/i82586.h>
#else
#include	"i82586.h"
#endif
#include	"wavelan.h"

#ifndef WAVELAN_DEBUG
#define WAVELAN_DEBUG			0
#endif	/* WAVELAN_DEBUG */

#define	WATCHDOG_JIFFIES		512	/* TODO: express in HZ. */
#define	ENABLE_FULL_PROMISCUOUS		0x10000

#define	nels(a)				(sizeof(a) / sizeof(a[0]))

typedef struct device		device;
typedef struct enet_statistics	en_stats;
typedef struct net_local	net_local;
typedef struct timer_list	timer_list;

struct net_local
{
	en_stats	stats;
	unsigned int	tx_n_in_use;
	unsigned char	nwid[2];
	unsigned short	hacr;
	unsigned short	rx_head;
	unsigned short	rx_last;
	unsigned short	tx_first_free;
	unsigned short	tx_first_in_use;
	unsigned int	nresets;
	unsigned int	correct_nwid;
	unsigned int	wrong_nwid;
	unsigned int	promiscuous;
	unsigned int	full_promiscuous;
	timer_list	watchdog;
	device		*dev;
	net_local	*prev;
	net_local	*next;
};

extern int		wavelan_probe(device *);	/* See Space.c */

static const char	*version	= "wavelan.c:v7 95/4/8\n";

/*
 * Entry point forward declarations.
 */
static int		wavelan_probe1(device *, unsigned short);
static int		wavelan_open(device *);
static int		wavelan_send_packet(struct sk_buff *, device *);
static void		wavelan_interrupt(int, struct pt_regs *);
static int		wavelan_close(device *);
static en_stats		*wavelan_get_stats(device *);
static void		wavelan_set_multicast_list(device *);
static int		wavelan_get_info(char*, char**, off_t, int, int);

/*
 * Other forward declarations.
 */
static void		wavelan_cu_show_one(device *, net_local *, int, unsigned short);
static void		wavelan_cu_start(device *);
static void		wavelan_ru_start(device *);
static void		wavelan_watchdog(unsigned long);
#if	0
static void		wavelan_psa_show(psa_t *);
static void		wavelan_mmc_show(unsigned short);
#endif	/* 0 */
static void		wavelan_scb_show(unsigned short);
static void		wavelan_ru_show(device *);
static void		wavelan_cu_show(device *);
static void		wavelan_dev_show(device *);
static void		wavelan_local_show(device *);

static unsigned int	wavelan_debug	= WAVELAN_DEBUG;
static net_local	*first_wavelan	= (net_local *)0;

static
unsigned long
wavelan_splhi(void)
{
	unsigned long	flags;

	save_flags(flags);
	cli();

	return flags;
}

static
void
wavelan_splx(unsigned long flags)
{
	restore_flags(flags);
}

static
unsigned short
hasr_read(unsigned short ioaddr)
{
	return inw(HASR(ioaddr));
}

static
void
hacr_write(unsigned short ioaddr, int hacr)
{
	outw(hacr, HACR(ioaddr));
}

static
void
hacr_write_slow(unsigned short ioaddr, int hacr)
{
	hacr_write(ioaddr, hacr);
	/* delay might only be needed sometimes */
	udelay(1000);
}

/*
 * Set the channel attention bit.
 */
static
void
set_chan_attn(unsigned short ioaddr, unsigned short current_hacr)
{
	hacr_write(ioaddr, current_hacr | HACR_CA);
}

/*
 * Reset, and then set host adaptor into default mode.
 */
static
void
wavelan_reset(unsigned short ioaddr)
{
	hacr_write_slow(ioaddr, HACR_RESET);
	hacr_write(ioaddr, HACR_DEFAULT);
}

static
void
wavelan_16_off(unsigned short ioaddr, unsigned short hacr)
{
	hacr &= ~HACR_16BITS;

	hacr_write(ioaddr, hacr);
}

static
void
wavelan_16_on(unsigned short ioaddr, unsigned short hacr)
{
	hacr |= HACR_16BITS;

	hacr_write(ioaddr, hacr);
}

static
void
wavelan_ints_off(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned long	x;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	x = wavelan_splhi();

	lp->hacr &= ~HACR_INTRON;
	hacr_write(ioaddr, lp->hacr);

	wavelan_splx(x);
}

static
void
wavelan_ints_on(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned long	x;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	x = wavelan_splhi();

	lp->hacr |= HACR_INTRON;
	hacr_write(ioaddr, lp->hacr);

	wavelan_splx(x);
}

/*
 * Read bytes from the PSA.
 */
static
void
psa_read(unsigned short ioaddr, unsigned short hacr, int o, unsigned char *b, int n)
{
	wavelan_16_off(ioaddr, hacr);

	while (n-- > 0)
	{
		outw(o, PIOR2(ioaddr));
		o++;
		*b++ = inb(PIOP2(ioaddr));
	}

	wavelan_16_on(ioaddr, hacr);
}

#if	defined(IRQ_SET_WORKS)
/*
 * Write bytes to the PSA.
 */
static
void
psa_write(unsigned short ioaddr, unsigned short hacr, int o, unsigned char *b, int n)
{
	wavelan_16_off(ioaddr, hacr);

	while (n-- > 0)
	{
		outw(o, PIOR2(ioaddr));
		o++;
		outb(*b, PIOP2(ioaddr));
		b++;
	}

	wavelan_16_on(ioaddr, hacr);
}
#endif	/* defined(IRQ_SET_WORKS) */

/*
 * Read bytes from the on-board RAM.
 */
static
void
obram_read(unsigned short ioaddr, unsigned short o, unsigned char *b, int n)
{
	n = (n + 1) / (sizeof(unsigned short) / sizeof(unsigned char));

	outw(o, PIOR1(ioaddr));

	insw(PIOP1(ioaddr), (unsigned short *)b, n);
}

/*
 * Write bytes to the on-board RAM.
 */
static
void
obram_write(unsigned short ioaddr, unsigned short o, unsigned char *b, int n)
{
	n = (n + 1) / (sizeof(unsigned short) / sizeof(unsigned char));

	outw(o, PIOR1(ioaddr));

	outsw(PIOP1(ioaddr), (unsigned short *)b, n);
}

/*
 * Read bytes from the MMC.
 */
static
void
mmc_read(unsigned short ioaddr, unsigned short o, unsigned char *b, int n)
{
	while (n-- > 0)
	{
		while (inw(HASR(ioaddr)) & HASR_MMC_BUSY)
			;

		outw(o << 1, MMCR(ioaddr));
		o++;

		while (inw(HASR(ioaddr)) & HASR_MMC_BUSY)
			;

		*b++ = (unsigned char)(inw(MMCR(ioaddr)) >> 8);
	}
}

/*
 * Write bytes to the MMC.
 */
static
void
mmc_write(unsigned short ioaddr, unsigned short o, unsigned char *b, int n)
{
	while (n-- > 0)
	{
		while (inw(HASR(ioaddr)) & HASR_MMC_BUSY)
			;

		outw((unsigned short)(((unsigned short)*b << 8) | (o << 1) | 1), MMCR(ioaddr));
		b++;
		o++;
	}
}

static int	irqvals[]	=
{
	   0,    0,    0, 0x01,
	0x02, 0x04,    0, 0x08,
	   0,    0, 0x10, 0x20,
	0x40,    0,    0, 0x80,
};

#if	defined(IRQ_SET_WORKS)
static
int
wavelan_unmap_irq(int irq, unsigned char *irqval)
{
	if (irq < 0 || irq >= nels(irqvals) || irqvals[irq] == 0)
		return -1;
	
	*irqval = (unsigned char)irqvals[irq];

	return 0;
}
#endif	/* defined(IRQ_SET_WORKS) */

/*
 * Map values from the irq parameter register to irq numbers.
 */
static
int
wavelan_map_irq(unsigned char irqval)
{
	int	irq;

	for (irq = 0; irq < nels(irqvals); irq++)
	{
		if (irqvals[irq] == (int)irqval)
			return irq;
	}

	return -1;
}

/*
 * Initialize the Modem Management Controller.
 */
static
void
wavelan_mmc_init(device *dev, psa_t *psa)
{
	unsigned short	ioaddr;
	net_local	*lp;
	mmw_t		m;
	int		configured;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;
	memset(&m, 0x00, sizeof(m));

	/*
	 *	configured = psa->psa_conf_status & 1;
	 *
	 * For now we use the persistent PSA
	 * information as little as possible, thereby
	 * allowing us to return to the same known state
	 * during a hardware reset.
	 */
	configured = 0;
	
	/*
	 * Set default modem control parameters.
	 * See NCR document 407-0024326 Rev. A.
	 */
	m.mmw_jabber_enable = 0x01;
	m.mmw_anten_sel = MMW_ANTEN_SEL_ALG_EN;
	m.mmw_ifs = 0x20;
	m.mmw_mod_delay = 0x04;
	m.mmw_jam_time = 0x38;

	m.mmw_encr_enable = 0;
	m.mmw_des_io_invert = 0;
	m.mmw_freeze = 0;
	m.mmw_decay_prm = 0;
	m.mmw_decay_updat_prm = 0;

	if (configured)
	{
		/*
		 * Use configuration defaults from parameter storage area.
		 */
		if (psa->psa_undefined & 1)
			m.mmw_loopt_sel = 0x00;
		else
			m.mmw_loopt_sel = MMW_LOOPT_SEL_UNDEFINED;

		m.mmw_thr_pre_set = psa->psa_thr_pre_set & 0x3F;
		m.mmw_quality_thr = psa->psa_quality_thr & 0x0F;
	}
	else
	{
		if (lp->promiscuous && lp->full_promiscuous)
			m.mmw_loopt_sel = MMW_LOOPT_SEL_UNDEFINED;
		else
			m.mmw_loopt_sel = 0x00;

		/*
		 * 0x04 for AT,
		 * 0x01 for MCA.
		 */
		if (psa->psa_comp_number & 1)
			m.mmw_thr_pre_set = 0x01;
		else
			m.mmw_thr_pre_set = 0x04;

		m.mmw_quality_thr = 0x03;
	}

	m.mmw_netw_id_l = lp->nwid[1];
	m.mmw_netw_id_h = lp->nwid[0];

	mmc_write(ioaddr, 0, (unsigned char *)&m, sizeof(m));
}

static
void
wavelan_ack(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	scb_cs;
	int		i;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	obram_read(ioaddr, scboff(OFFSET_SCB, scb_status), (unsigned char *)&scb_cs, sizeof(scb_cs));
	scb_cs &= SCB_ST_INT;

	if (scb_cs == 0)
		return;

	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));

	set_chan_attn(ioaddr, lp->hacr);

	for (i = 1000; i > 0; i--)
	{
		obram_read(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));
		if (scb_cs == 0)
			break;

		udelay(1000);
	}

	if (i <= 0)
		printk("%s: wavelan_ack(): board not accepting command.\n", dev->name);
}

/*
 * Set channel attention bit and busy wait until command has
 * completed, then acknowledge the command completion.
 */
static
int
wavelan_synchronous_cmd(device *dev, const char *str)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	scb_cmd;
	ach_t		cb;
	int		i;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	scb_cmd = SCB_CMD_CUC & SCB_CMD_CUC_GO;
	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cmd, sizeof(scb_cmd));

	set_chan_attn(ioaddr, lp->hacr);

	for (i = 64; i > 0; i--)
	{
		obram_read(ioaddr, OFFSET_CU, (unsigned char *)&cb, sizeof(cb));
		if (cb.ac_status & AC_SFLD_C)
			break;

		udelay(1000);
	}

	if (i <= 0 || !(cb.ac_status & AC_SFLD_OK))
	{
		printk("%s: %s failed; status = 0x%x\n", dev->name, str, cb.ac_status);
		wavelan_scb_show(ioaddr);
		return -1;
	}

	wavelan_ack(dev);

	return 0;
}

static
int
wavelan_hardware_reset(device *dev)
{
	unsigned short	ioaddr;
	psa_t		psa;
	net_local	*lp;
	scp_t		scp;
	iscp_t		iscp;
	scb_t		scb;
	ach_t		cb;
	int		i;
	ac_cfg_t	cfg;
	ac_ias_t	ias;

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_hardware_reset(dev=0x%x)\n", dev->name, (unsigned int)dev);

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	lp->nresets++;

	wavelan_reset(ioaddr);
	lp->hacr = HACR_DEFAULT;

	/*
	 * Clear the onboard RAM.
	 */
	{
		unsigned char	zeroes[512];

		memset(&zeroes[0], 0x00, sizeof(zeroes));

		for (i = 0; i < I82586_MEMZ; i += sizeof(zeroes))
			obram_write(ioaddr, i, &zeroes[0], sizeof(zeroes));
	}

	psa_read(ioaddr, lp->hacr, 0, (unsigned char *)&psa, sizeof(psa));

	wavelan_mmc_init(dev, &psa);

	/*
	 * Construct the command unit structures:
	 * scp, iscp, scb, cb.
	 */
	memset(&scp, 0x00, sizeof(scp));
	scp.scp_sysbus = SCP_SY_16BBUS;
	scp.scp_iscpl = OFFSET_ISCP;
	obram_write(ioaddr, OFFSET_SCP, (unsigned char *)&scp, sizeof(scp));

	memset(&iscp, 0x00, sizeof(iscp));
	iscp.iscp_busy = 1;
	iscp.iscp_offset = OFFSET_SCB;
	obram_write(ioaddr, OFFSET_ISCP, (unsigned char *)&iscp, sizeof(iscp));

	memset(&scb, 0x00, sizeof(scb));
	scb.scb_command = SCB_CMD_RESET;
	scb.scb_cbl_offset = OFFSET_CU;
	scb.scb_rfa_offset = OFFSET_RU;
	obram_write(ioaddr, OFFSET_SCB, (unsigned char *)&scb, sizeof(scb));

	set_chan_attn(ioaddr, lp->hacr);

	for (i = 1000; i > 0; i--)
	{
		obram_read(ioaddr, OFFSET_ISCP, (unsigned char *)&iscp, sizeof(iscp));

		if (iscp.iscp_busy == (unsigned short)0)
			break;

		udelay(1000);
	}

	if (i <= 0)
	{
		printk("%s: wavelan_hardware_reset(): iscp_busy timeout.\n", dev->name);
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);
		return -1;
	}

	for (i = 15; i > 0; i--)
	{
		obram_read(ioaddr, OFFSET_SCB, (unsigned char *)&scb, sizeof(scb));

		if (scb.scb_status == (SCB_ST_CX | SCB_ST_CNA))
			break;

		udelay(1000);
	}

	if (i <= 0)
	{
		printk("%s: wavelan_hardware_reset(): status: expected 0x%02x, got 0x%02x.\n", dev->name, SCB_ST_CX | SCB_ST_CNA, scb.scb_status);
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);
		return -1;
	}

	wavelan_ack(dev);

	memset(&cb, 0x00, sizeof(cb));
	cb.ac_command = AC_CFLD_EL | (AC_CFLD_CMD & acmd_diagnose);
	cb.ac_link = OFFSET_CU;
	obram_write(ioaddr, OFFSET_CU, (unsigned char *)&cb, sizeof(cb));

	if (wavelan_synchronous_cmd(dev, "diag()") == -1)
	{
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);
		return -1;
	}

	obram_read(ioaddr, OFFSET_CU, (unsigned char *)&cb, sizeof(cb));
	if (cb.ac_status & AC_SFLD_FAIL)
	{
		printk("%s: wavelan_hardware_reset(): i82586 Self Test failed.\n", dev->name);
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);
		return -1;
	}

	memset(&cfg, 0x00, sizeof(cfg));

#if	0
	/*
	 * The default board configuration.
	 */
	cfg.fifolim_bytecnt 	= 0x080c;
	cfg.addrlen_mode  	= 0x2600;
	cfg.linprio_interframe	= 0x7820;	/* IFS=120, ACS=2 */
	cfg.slot_time      	= 0xf00c;	/* slottime=12    */
	cfg.hardware	     	= 0x0008;	/* tx even w/o CD */
	cfg.min_frame_len   	= 0x0040;
#endif	/* 0 */

	/*
	 * For Linux we invert AC_CFG_ALOC(..) so as to conform
	 * to the way that net packets reach us from above.
	 * (See also ac_tx_t.)
	 */
	cfg.cfg_byte_cnt = AC_CFG_BYTE_CNT(sizeof(ac_cfg_t) - sizeof(ach_t));
	cfg.cfg_fifolim = AC_CFG_FIFOLIM(8);
 	cfg.cfg_byte8 = AC_CFG_SAV_BF(0) |
			AC_CFG_SRDY(0);
 	cfg.cfg_byte9 = AC_CFG_ELPBCK(0) |
			AC_CFG_ILPBCK(0) |
			AC_CFG_PRELEN(AC_CFG_PLEN_2) |
			AC_CFG_ALOC(1) |
			AC_CFG_ADDRLEN(WAVELAN_ADDR_SIZE);
	cfg.cfg_byte10 = AC_CFG_BOFMET(0) |
			AC_CFG_ACR(0) |
			AC_CFG_LINPRIO(0);
	cfg.cfg_ifs = 32;
	cfg.cfg_slotl = 0;
	cfg.cfg_byte13 = AC_CFG_RETRYNUM(15) |
			AC_CFG_SLTTMHI(2);
	cfg.cfg_byte14 = AC_CFG_FLGPAD(0) |
			AC_CFG_BTSTF(0) |
			AC_CFG_CRC16(0) |
			AC_CFG_NCRC(0) |
			AC_CFG_TNCRS(1) |
			AC_CFG_MANCH(0) |
			AC_CFG_BCDIS(0) |
			AC_CFG_PRM(lp->promiscuous);
	cfg.cfg_byte15 = AC_CFG_ICDS(0) |
			AC_CFG_CDTF(0) |
			AC_CFG_ICSS(0) |
			AC_CFG_CSTF(0);
/*
	cfg.cfg_min_frm_len = AC_CFG_MNFRM(64);
*/
	cfg.cfg_min_frm_len = AC_CFG_MNFRM(8);

	cfg.cfg_h.ac_command = AC_CFLD_EL | (AC_CFLD_CMD & acmd_configure);
	cfg.cfg_h.ac_link = OFFSET_CU;
	obram_write(ioaddr, OFFSET_CU, (unsigned char *)&cfg, sizeof(cfg));

	if (wavelan_synchronous_cmd(dev, "reset()-configure") == -1)
	{
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);

		return -1;
	}

	memset(&ias, 0x00, sizeof(ias));
	ias.ias_h.ac_command = AC_CFLD_EL | (AC_CFLD_CMD & acmd_ia_setup);
	ias.ias_h.ac_link = OFFSET_CU;
	memcpy(&ias.ias_addr[0], (unsigned char *)&dev->dev_addr[0], sizeof(ias.ias_addr));
	obram_write(ioaddr, OFFSET_CU, (unsigned char *)&ias, sizeof(ias));

	if (wavelan_synchronous_cmd(dev, "reset()-address") == -1)
	{
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_hardware_reset(): -1\n", dev->name);

		return -1;
	}

	wavelan_ints_on(dev);

	if (wavelan_debug > 4)
		wavelan_scb_show(ioaddr);

	wavelan_ru_start(dev);
	wavelan_cu_start(dev);

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_hardware_reset(): 0\n", dev->name);

	return 0;
}

#if	STRUCT_CHECK == 1

static
const char	*
wavelan_struct_check(void)
{
#define	SC(t,s,n)	if (sizeof(t) != s) return n
	SC(psa_t, PSA_SIZE, "psa_t");
	SC(mmw_t, MMW_SIZE, "mmw_t");
	SC(mmr_t, MMR_SIZE, "mmr_t");
	SC(ha_t, HA_SIZE, "ha_t");
#undef	SC

	return (char *)0;
}

#endif	/* STRUCT_CHECK == 1 */

/*
 * Check for a network adaptor of this type.
 * Return '0' iff one exists.
 * (There seem to be different interpretations of
 * the initial value of dev->base_addr.
 * We follow the example in drivers/net/ne.c.)
 */
int
wavelan_probe(device *dev)
{
	int			i;
	int			r;
	short			base_addr;
	static unsigned short	iobase[]	=
	{
#if	0
		Leave out 0x3C0 for now -- seems to clash
		with some video controllers.
		Leave out the others too -- we will always
		use 0x390 and leave 0x300 for the Ethernet device.
		0x300, 0x390, 0x3E0, 0x3C0,
#endif	/* 0 */
		0x390,
	};

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_probe(dev=0x%x (base_addr=0x%x))\n", dev->name, (unsigned int)dev, (unsigned int)dev->base_addr);

#if	STRUCT_CHECK == 1
	if (wavelan_struct_check() != (char *)0)
	{
		printk("%s: structure/compiler botch: \"%s\"\n", dev->name, wavelan_struct_check());

		if (wavelan_debug > 0)
			printk("%s: <-wavelan_probe(): ENODEV\n", dev->name);

		return ENODEV;
	}
#endif	/* STRUCT_CHECK == 1 */

	base_addr = dev->base_addr;

	if (base_addr < 0)
	{
		/*
		 * Don't probe at all.
		 */
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_probe(): ENXIO\n", dev->name);
		return ENXIO;
	}

	if (base_addr > 0x100)
	{
		/*
		 * Check a single specified location.
		 */
		r = wavelan_probe1(dev, base_addr);
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_probe(): %d\n", dev->name, r);
		return r;
	}

	for (i = 0; i < nels(iobase); i++)
	{
		if (check_region(iobase[i], sizeof(ha_t)))
			continue;

		if (wavelan_probe1(dev, iobase[i]) == 0)
		{
			if (wavelan_debug > 0)
				printk("%s: <-wavelan_probe(): 0\n", dev->name);
			proc_net_register(&(struct proc_dir_entry) {
				PROC_NET_WAVELAN, 7, "wavelan",
				S_IFREG | S_IRUGO, 1, 0, 0,
				0, &proc_net_inode_operations,
				wavelan_get_info
			});

			return 0;
		}
	}

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_probe(): ENODEV\n", dev->name);

	return ENODEV;
}

static
int
wavelan_probe1(device *dev, unsigned short ioaddr)
{
	psa_t		psa;
	int		irq;
	int		i;
	net_local	*lp;
	int		enable_full_promiscuous;

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_probe1(dev=0x%x, ioaddr=0x%x)\n", dev->name, (unsigned int)dev, ioaddr);

	wavelan_reset(ioaddr);

	psa_read(ioaddr, HACR_DEFAULT, 0, (unsigned char *)&psa, sizeof(psa));

	/*
	 * Check the first three octets of the MAC address
	 * for the manufacturer's code.
	 */ 
	if
	(
		psa.psa_univ_mac_addr[0] != SA_ADDR0
		||
		psa.psa_univ_mac_addr[1] != SA_ADDR1
		||
		psa.psa_univ_mac_addr[2] != SA_ADDR2
	)
	{
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_probe1(): ENODEV\n", dev->name);
		return ENODEV;
	}

	printk("%s: WaveLAN at %#x,", dev->name, ioaddr);

	if (dev->irq != 0)
	{
		printk("[WARNING: explicit IRQ value %d ignored: using PSA value instead]", dev->irq);
#if	defined(IRQ_SET_WORKS)
Leave this out until I can get it to work -- BJ.
		if (wavelan_unmap_irq(dev->irq, &psa.psa_int_req_no) == -1)
		{
			printk(" could not wavelan_unmap_irq(%d, ..) -- ignored.\n", dev->irq);
			dev->irq = 0;
		}
		else
		{
			psa_write(ioaddr, HACR_DEFAULT, (char *)&psa.psa_int_req_no - (char *)&psa, (unsigned char *)&psa.psa_int_req_no, sizeof(psa.psa_int_req_no));
			wavelan_reset(ioaddr);
		}
#endif	/* defined(IRQ_SET_WORKS) */
	}

	if ((irq = wavelan_map_irq(psa.psa_int_req_no)) == -1)
	{
		printk(" could not wavelan_map_irq(%d).\n", psa.psa_int_req_no);
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_probe1(): EAGAIN\n", dev->name);
		return EAGAIN;
	}

	dev->irq = irq;

	request_region(ioaddr, sizeof(ha_t), "wavelan");
	dev->base_addr = ioaddr;

	/*
	 * The third numeric argument to LILO's
	 * `ether=' control line arrives here as `dev->mem_start'.
	 *
	 * If bit 16 of dev->mem_start is non-zero we enable
	 * full promiscuity.
	 *
	 * If either of the least significant two bytes of
	 * dev->mem_start are non-zero we use them instead
	 * of the PSA NWID.
	 */
	enable_full_promiscuous = (dev->mem_start & ENABLE_FULL_PROMISCUOUS) == ENABLE_FULL_PROMISCUOUS;
	dev->mem_start &= ~ENABLE_FULL_PROMISCUOUS;

	if (dev->mem_start != 0)
	{
		psa.psa_nwid[0] = (dev->mem_start >> 8) & 0xFF;
		psa.psa_nwid[1] = (dev->mem_start >> 0) & 0xFF;
	}

	dev->mem_start = 0x0000;
	dev->mem_end = 0x0000;
	dev->if_port = 0;

	memcpy(&dev->dev_addr[0], &psa.psa_univ_mac_addr[0], WAVELAN_ADDR_SIZE);

	for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
		printk("%s%02x", (i == 0) ? " " : ":", dev->dev_addr[i]);

	printk(", IRQ %d", dev->irq);
	if (enable_full_promiscuous)
		printk(", promisc");
	printk(", nwid 0x%02x%02x", psa.psa_nwid[0], psa.psa_nwid[1]);

	printk(", PC");
	switch (psa.psa_comp_number)
	{
	case PSA_COMP_PC_AT_915:
	case PSA_COMP_PC_AT_2400:
		printk("-AT");
		break;

	case PSA_COMP_PC_MC_915:
	case PSA_COMP_PC_MC_2400:
		printk("-MC");
		break;

	case PSA_COMP_PCMCIA_915:
		printk("MCIA");
		break;

	default:
		printk("???");
		break;
	}

	printk(", ");
	switch (psa.psa_subband)
	{
	case PSA_SUBBAND_915:
		printk("915");
		break;

	case PSA_SUBBAND_2425:
		printk("2425");
		break;

	case PSA_SUBBAND_2460:
		printk("2460");
		break;

	case PSA_SUBBAND_2484:
		printk("2484");
		break;

	case PSA_SUBBAND_2430_5:
		printk("2430.5");
		break;

	default:
		printk("???");
		break;
	}
	printk(" MHz");

	printk("\n");

	if (wavelan_debug > 0)
		printk(version);

	dev->priv = kmalloc(sizeof(net_local), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0x00, sizeof(net_local));
	lp = (net_local *)dev->priv;

	if (first_wavelan == (net_local *)0)
	{
		first_wavelan = lp;
		lp->prev = lp;
		lp->next = lp;
	}
	else
	{
		lp->prev = first_wavelan->prev;
		lp->next = first_wavelan;
		first_wavelan->prev->next = lp;
		first_wavelan->prev = lp;
	}
	lp->dev = dev;

	lp->hacr = HACR_DEFAULT;

	lp->full_promiscuous = enable_full_promiscuous;
	lp->nwid[0] = psa.psa_nwid[0];
	lp->nwid[1] = psa.psa_nwid[1];

	lp->watchdog.function = wavelan_watchdog;
	lp->watchdog.data = (unsigned long)dev;

	dev->open = wavelan_open;
	dev->stop = wavelan_close;
	dev->hard_start_xmit = wavelan_send_packet;
	dev->get_stats = wavelan_get_stats;
	dev->set_multicast_list = &wavelan_set_multicast_list;

	/*
	 * Fill in the fields of the device structure
	 * with ethernet-generic values.
	 */
	ether_setup(dev);

	dev->flags &= ~IFF_MULTICAST;		/* Not yet supported */
	
	dev->mtu = WAVELAN_MTU;

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_probe1(): 0\n", dev->name);

	return 0;
}

/*
 * Construct the fd and rbd structures.
 * Start the receive unit.
 */
static
void
wavelan_ru_start(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	scb_cs;
	fd_t		fd;
	rbd_t		rbd;
	unsigned short	rx;
	unsigned short	rx_next;
	int		i;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	obram_read(ioaddr, scboff(OFFSET_SCB, scb_status), (unsigned char *)&scb_cs, sizeof(scb_cs));
	if ((scb_cs & SCB_ST_RUS) == SCB_ST_RUS_RDY)
		return;

	lp->rx_head = OFFSET_RU;

	for (i = 0, rx = lp->rx_head; i < NRXBLOCKS; i++, rx = rx_next)
	{
		rx_next = (i == NRXBLOCKS - 1) ? lp->rx_head : rx + RXBLOCKZ;

		fd.fd_status = 0;
		fd.fd_command = (i == NRXBLOCKS - 1) ? FD_COMMAND_EL : 0;
		fd.fd_link_offset = rx_next;
		fd.fd_rbd_offset = rx + sizeof(fd);
		obram_write(ioaddr, rx, (unsigned char *)&fd, sizeof(fd));

		rbd.rbd_status = 0;
		rbd.rbd_next_rbd_offset = I82586NULL;
		rbd.rbd_bufl = rx + sizeof(fd) + sizeof(rbd);
		rbd.rbd_bufh = 0;
		rbd.rbd_el_size = RBD_EL | (RBD_SIZE & MAXDATAZ);
		obram_write(ioaddr, rx + sizeof(fd), (unsigned char *)&rbd, sizeof(rbd));

		lp->rx_last = rx;
	}

	obram_write(ioaddr, scboff(OFFSET_SCB, scb_rfa_offset), (unsigned char *)&lp->rx_head, sizeof(lp->rx_head));

	scb_cs = SCB_CMD_RUC_GO;
	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));

	set_chan_attn(ioaddr, lp->hacr);

	for (i = 1000; i > 0; i--)
	{
		obram_read(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));
		if (scb_cs == 0)
			break;

		udelay(1000);
	}

	if (i <= 0)
		printk("%s: wavelan_ru_start(): board not accepting command.\n", dev->name);
}

/*
 * Initialise the transmit blocks.
 * Start the command unit executing the NOP
 * self-loop of the first transmit block.
 */
static
void
wavelan_cu_start(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	int		i;
	unsigned short	txblock;
	unsigned short	first_nop;
	unsigned short	scb_cs;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	lp->tx_first_free = OFFSET_CU;
	lp->tx_first_in_use = I82586NULL;

	for
	(
		i = 0, txblock = OFFSET_CU;
		i < NTXBLOCKS;
		i++, txblock += TXBLOCKZ
	)
	{
		ac_tx_t		tx;
		ac_nop_t	nop;
		tbd_t		tbd;
		unsigned short	tx_addr;
		unsigned short	nop_addr;
		unsigned short	tbd_addr;
		unsigned short	buf_addr;

		tx_addr = txblock;
		nop_addr = tx_addr + sizeof(tx);
		tbd_addr = nop_addr + sizeof(nop);
		buf_addr = tbd_addr + sizeof(tbd);

		tx.tx_h.ac_status = 0;
		tx.tx_h.ac_command = acmd_transmit | AC_CFLD_I;
		tx.tx_h.ac_link = nop_addr;
		tx.tx_tbd_offset = tbd_addr;
		obram_write(ioaddr, tx_addr, (unsigned char *)&tx, sizeof(tx));

		nop.nop_h.ac_status = 0;
		nop.nop_h.ac_command = acmd_nop;
		nop.nop_h.ac_link = nop_addr;
		obram_write(ioaddr, nop_addr, (unsigned char *)&nop, sizeof(nop));

		tbd.tbd_status = TBD_STATUS_EOF;
		tbd.tbd_next_bd_offset = I82586NULL;
		tbd.tbd_bufl = buf_addr;
		tbd.tbd_bufh = 0;
		obram_write(ioaddr, tbd_addr, (unsigned char *)&tbd, sizeof(tbd));
	}

	first_nop = OFFSET_CU + (NTXBLOCKS - 1) * TXBLOCKZ + sizeof(ac_tx_t);
	obram_write(ioaddr, scboff(OFFSET_SCB, scb_cbl_offset), (unsigned char *)&first_nop, sizeof(first_nop));

	scb_cs = SCB_CMD_CUC_GO;
	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));

	set_chan_attn(ioaddr, lp->hacr);

	for (i = 1000; i > 0; i--)
	{
		obram_read(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cs, sizeof(scb_cs));
		if (scb_cs == 0)
			break;

		udelay(1000);
	}

	if (i <= 0)
		printk("%s: wavelan_cu_start(): board not accepting command.\n", dev->name);

	lp->tx_n_in_use = 0;
	dev->tbusy = 0;
}

static
int
wavelan_open(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned long	x;
	int		r;

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_open(dev=0x%x)\n", dev->name, (unsigned int)dev);

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	if (dev->irq == 0)
	{
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_open(): -ENXIO\n", dev->name);
		return -ENXIO;
	}

	if
	(
		irq2dev_map[dev->irq] != (device *)0
		/* This is always true, but avoid the false IRQ. */
		||
		(irq2dev_map[dev->irq] = dev) == (device *)0
		||
		request_irq(dev->irq, &wavelan_interrupt, 0, "WaveLAN") != 0
	)
	{
		irq2dev_map[dev->irq] = (device *)0;
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_open(): -EAGAIN\n", dev->name);
		return -EAGAIN;
	}

	x = wavelan_splhi();
	if ((r = wavelan_hardware_reset(dev)) != -1)
	{
		dev->interrupt = 0;
		dev->start = 1;
	}
	wavelan_splx(x);

	if (r == -1)
	{
		free_irq(dev->irq);
		irq2dev_map[dev->irq] = (device *)0;
		if (wavelan_debug > 0)
			printk("%s: <-wavelan_open(): -EAGAIN(2)\n", dev->name);
		return -EAGAIN;
	}

	MOD_INC_USE_COUNT;

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_open(): 0\n", dev->name);

	return 0;
}

static
void
hardware_send_packet(device *dev, void *buf, short length)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	txblock;
	unsigned short	txpred;
	unsigned short	tx_addr;
	unsigned short	nop_addr;
	unsigned short	tbd_addr;
	unsigned short	buf_addr;
	ac_tx_t		tx;
	ac_nop_t	nop;
	tbd_t		tbd;
	unsigned long	x;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	x = wavelan_splhi();

	txblock = lp->tx_first_free;
	txpred = txblock - TXBLOCKZ;
	if (txpred < OFFSET_CU)
		txpred += NTXBLOCKS * TXBLOCKZ;
	lp->tx_first_free += TXBLOCKZ;
	if (lp->tx_first_free >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
		lp->tx_first_free -= NTXBLOCKS * TXBLOCKZ;

/*
if (lp->tx_n_in_use > 0)
	printk("%c", "0123456789abcdefghijk"[lp->tx_n_in_use]);
*/

	lp->tx_n_in_use++;

	tx_addr = txblock;
	nop_addr = tx_addr + sizeof(tx);
	tbd_addr = nop_addr + sizeof(nop);
	buf_addr = tbd_addr + sizeof(tbd);

	/*
	 * Transmit command.
	 */
	tx.tx_h.ac_status = 0;
	obram_write(ioaddr, toff(ac_tx_t, tx_addr, tx_h.ac_status), (unsigned char *)&tx.tx_h.ac_status, sizeof(tx.tx_h.ac_status));

	/*
	 * NOP command.
	 */
	nop.nop_h.ac_status = 0;
	obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status), (unsigned char *)&nop.nop_h.ac_status, sizeof(nop.nop_h.ac_status));
	nop.nop_h.ac_link = nop_addr;
	obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link), (unsigned char *)&nop.nop_h.ac_link, sizeof(nop.nop_h.ac_link));

	/*
	 * Transmit buffer descriptor. 
	 */
	tbd.tbd_status = TBD_STATUS_EOF | (TBD_STATUS_ACNT & length);
	tbd.tbd_next_bd_offset = I82586NULL;
	tbd.tbd_bufl = buf_addr;
	tbd.tbd_bufh = 0;
	obram_write(ioaddr, tbd_addr, (unsigned char *)&tbd, sizeof(tbd));

	/*
	 * Data.
	 */
	obram_write(ioaddr, buf_addr, buf, length);

	/*
	 * Overwrite the predecessor NOP link
	 * so that it points to this txblock.
	 */
	nop_addr = txpred + sizeof(tx);
	nop.nop_h.ac_status = 0;
	obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_status), (unsigned char *)&nop.nop_h.ac_status, sizeof(nop.nop_h.ac_status));
	nop.nop_h.ac_link = txblock;
	obram_write(ioaddr, toff(ac_nop_t, nop_addr, nop_h.ac_link), (unsigned char *)&nop.nop_h.ac_link, sizeof(nop.nop_h.ac_link));

	if (lp->tx_first_in_use == I82586NULL)
		lp->tx_first_in_use = txblock;

	if (lp->tx_n_in_use < NTXBLOCKS - 1)
		dev->tbusy = 0;

	dev->trans_start = jiffies;

	if (lp->watchdog.next == (timer_list *)0)
		wavelan_watchdog((unsigned long)dev);

	wavelan_splx(x);

	if (wavelan_debug > 4)
	{
		unsigned char	*a;

		a = (unsigned char *)buf;

		printk
		(
			"%s: tx: dest %02x:%02x:%02x:%02x:%02x:%02x, length %d, tbd.tbd_bufl 0x%x.\n",
			dev->name,
			a[0], a[1], a[2], a[3], a[4], a[5],
			length,
			buf_addr
		);
	}
}

static
int
wavelan_send_packet(struct sk_buff *skb, device *dev)
{
	unsigned short	ioaddr;

	ioaddr = dev->base_addr;

	if (dev->tbusy)
	{
		/*
		 * If we get here, some higher level
		 * has decided we are broken.
		 */
		int	tickssofar;

		tickssofar = jiffies - dev->trans_start;

		/*
		 * But for the moment, we will rely on wavelan_watchdog()
		 * instead as it allows finer control over exactly when we
		 * make the determination of failure.
		 *
		if (tickssofar < 5)
		 */
			return 1;

		wavelan_scb_show(ioaddr);
		wavelan_ru_show(dev);
		wavelan_cu_show(dev);
		wavelan_dev_show(dev);
		wavelan_local_show(dev);

		printk("%s: transmit timed out -- resetting board.\n", dev->name);

		(void)wavelan_hardware_reset(dev);
	}

	/*
	 * If some higher layer thinks we've missed
	 * a tx-done interrupt we are passed NULL.
	 * Caution: dev_tint() handles the cli()/sti() itself.
	 */
	if (skb == (struct sk_buff *)0)
	{
		dev_tint(dev);
		return 0;
	}

	/*
	 * Block a timer-based transmit from overlapping.
	 */
	if (set_bit(0, (void *)&dev->tbusy) == 0)
	{
		short		length;
		unsigned char	*buf;

		length = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;
		buf = skb->data;

		hardware_send_packet(dev, buf, length);
	}
	else
		printk("%s: Transmitter access conflict.\n", dev->name);

	dev_kfree_skb(skb, FREE_WRITE);

	return 0;
}

#if	0
static
int
addrcmp(unsigned char *a0, unsigned char *a1)
{
	int	i;

	for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
	{
		if (a0[i] != a1[i])
			return a0[i] - a1[i];
	}

	return 0;
}
#endif	/* 0 */

/*
 * Transfer as many packets as we can
 * from the device RAM.
 * Called by the interrupt handler.
 */
static
void
wavelan_receive(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	int		nreaped;

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;
	nreaped = 0;

	for (;;)
	{
		fd_t		fd;
		rbd_t		rbd;
		ushort		pkt_len;
		int		sksize;
		struct sk_buff	*skb;

		obram_read(ioaddr, lp->rx_head, (unsigned char *)&fd, sizeof(fd));

		if ((fd.fd_status & FD_STATUS_C) != FD_STATUS_C)
			break;

		nreaped++;

		if
		(
			(fd.fd_status & (FD_STATUS_B | FD_STATUS_OK))
			!=
			(FD_STATUS_B | FD_STATUS_OK)
		)
		{
			/*
			 * Not sure about this one -- it does not seem
			 * to be an error so we will keep quiet about it.
			if ((fd.fd_status & FD_STATUS_B) != FD_STATUS_B)
				printk("%s: frame not consumed by RU.\n", dev->name);
			 */

			if ((fd.fd_status & FD_STATUS_OK) != FD_STATUS_OK)
				printk("%s: frame not received successfully.\n", dev->name);
		}

		if ((fd.fd_status & (FD_STATUS_S6 | FD_STATUS_S7 | FD_STATUS_S8 | FD_STATUS_S9 | FD_STATUS_S10 | FD_STATUS_S11)) != 0)
		{
			lp->stats.rx_errors++;

			if ((fd.fd_status & FD_STATUS_S6) != 0)
				printk("%s: no EOF flag.\n", dev->name);

			if ((fd.fd_status & FD_STATUS_S7) != 0)
			{
				lp->stats.rx_length_errors++;
				printk("%s: frame too short.\n", dev->name);
			}

			if ((fd.fd_status & FD_STATUS_S8) != 0)
			{
				lp->stats.rx_over_errors++;
				printk("%s: rx DMA overrun.\n", dev->name);
			}

			if ((fd.fd_status & FD_STATUS_S9) != 0)
			{
				lp->stats.rx_fifo_errors++;
				printk("%s: ran out of resources.\n", dev->name);
			}

			if ((fd.fd_status & FD_STATUS_S10) != 0)
			{
				lp->stats.rx_frame_errors++;
				printk("%s: alignment error.\n", dev->name);
			}

			if ((fd.fd_status & FD_STATUS_S11) != 0)
			{
				lp->stats.rx_crc_errors++;
				printk("%s: CRC error.\n", dev->name);
			}
		}

		if (fd.fd_rbd_offset == I82586NULL)
			printk("%s: frame has no data.\n", dev->name);
		else
		{
			obram_read(ioaddr, fd.fd_rbd_offset, (unsigned char *)&rbd, sizeof(rbd));

			if ((rbd.rbd_status & RBD_STATUS_EOF) != RBD_STATUS_EOF)
				printk("%s: missing EOF flag.\n", dev->name);

			if ((rbd.rbd_status & RBD_STATUS_F) != RBD_STATUS_F)
				printk("%s: missing F flag.\n", dev->name);

			pkt_len = rbd.rbd_status & RBD_STATUS_ACNT;

#if	0
			{
				unsigned char		addr[WAVELAN_ADDR_SIZE];
				int			i;
				static unsigned char	toweraddr[WAVELAN_ADDR_SIZE]	=
				{
					0x08, 0x00, 0x0e, 0x20, 0x3e, 0xd3,
				};

				obram_read(ioaddr, rbd.rbd_bufl + sizeof(addr), &addr[0], sizeof(addr));
				if
				(
					/*
					addrcmp(&addr[0], &dev->dev_addr[0]) != 0
					&&
					*/
					addrcmp(&addr[0], toweraddr) != 0
				)
				{
					printk("%s: foreign MAC source addr=", dev->name);
					for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
						printk("%s%02x", (i == 0) ? "" : ":", addr[i]);
					printk("\n");
				}
			}
#endif	/* 0 */

			if (wavelan_debug > 5)
			{
				unsigned char	addr[WAVELAN_ADDR_SIZE];
				unsigned short	ltype;
				int		i;

#if	0
				printk("%s: fd_dest=", dev->name);
				for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
					printk("%s%02x", (i == 0) ? "" : ":", fd.fd_dest[i]);
				printk("\n");

				printk("%s: fd_src=", dev->name);
				for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
					printk("%s%02x", (i == 0) ? "" : ":", fd.fd_src[i]);
				printk("\n");
				printk("%s: fd_length=%d\n", dev->name, fd.fd_length);
#endif	/* 0 */

				obram_read(ioaddr, rbd.rbd_bufl, &addr[0], sizeof(addr));
				printk("%s: dest=", dev->name);
				for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
					printk("%s%02x", (i == 0) ? "" : ":", addr[i]);
				printk("\n");

				obram_read(ioaddr, rbd.rbd_bufl + sizeof(addr), &addr[0], sizeof(addr));
				printk("%s: src=", dev->name);
				for (i = 0; i < WAVELAN_ADDR_SIZE; i++)
					printk("%s%02x", (i == 0) ? "" : ":", addr[i]);
				printk("\n");

				obram_read(ioaddr, rbd.rbd_bufl + sizeof(addr) * 2, (unsigned char *)&ltype, sizeof(ltype));
				printk("%s: ntohs(length/type)=0x%04x\n", dev->name, ntohs(ltype));
			}

			sksize = pkt_len;

			if ((skb = dev_alloc_skb(sksize)) == (struct sk_buff *)0)
			{
				printk("%s: could not alloc_skb(%d, GFP_ATOMIC).\n", dev->name, sksize);
				lp->stats.rx_dropped++;
			}
			else
			{
				skb->dev = dev;

				obram_read(ioaddr, rbd.rbd_bufl, skb_put(skb,pkt_len), pkt_len);

				if (wavelan_debug > 5)
				{
					int	i;
					int	maxi;

					printk("%s: pkt_len=%d, data=\"", dev->name, pkt_len);

					if ((maxi = pkt_len) > 16)
						maxi = 16;
				
					for (i = 0; i < maxi; i++)
					{
						unsigned char	c;

						c = skb->data[i];
						if (c >= ' ' && c <= '~')
							printk(" %c", skb->data[i]);
						else
							printk("%02x", skb->data[i]);
					}

					if (maxi < pkt_len)
						printk("..");
				
					printk("\"\n\n");
				}
			
				skb->protocol=eth_type_trans(skb,dev);
				netif_rx(skb);

				lp->stats.rx_packets++;
			}
		}

		fd.fd_status = 0;
		obram_write(ioaddr, fdoff(lp->rx_head, fd_status), (unsigned char *)&fd.fd_status, sizeof(fd.fd_status));

		fd.fd_command = FD_COMMAND_EL;
		obram_write(ioaddr, fdoff(lp->rx_head, fd_command), (unsigned char *)&fd.fd_command, sizeof(fd.fd_command));

		fd.fd_command = 0;
		obram_write(ioaddr, fdoff(lp->rx_last, fd_command), (unsigned char *)&fd.fd_command, sizeof(fd.fd_command));

		lp->rx_last = lp->rx_head;
		lp->rx_head = fd.fd_link_offset;
	}

/*
	if (nreaped > 1)
		printk("r%d", nreaped);
*/
}

/*
 * Command completion interrupt.
 * Reclaim as many freed tx buffers as we can.
 */
static
int
wavelan_complete(device *dev, unsigned short ioaddr, net_local *lp)
{
	int	nreaped;

	nreaped = 0;

	for (;;)
	{
		unsigned short	tx_status;

		if (lp->tx_first_in_use == I82586NULL)
			break;

		obram_read(ioaddr, acoff(lp->tx_first_in_use, ac_status), (unsigned char *)&tx_status, sizeof(tx_status));

		if ((tx_status & AC_SFLD_C) == 0)
			break;

		nreaped++;

		--lp->tx_n_in_use;

/*
if (lp->tx_n_in_use > 0)
	printk("%c", "0123456789abcdefghijk"[lp->tx_n_in_use]);
*/

		if (lp->tx_n_in_use <= 0)
			lp->tx_first_in_use = I82586NULL;
		else
		{
			lp->tx_first_in_use += TXBLOCKZ;
			if (lp->tx_first_in_use >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
				lp->tx_first_in_use -= NTXBLOCKS * TXBLOCKZ;
		}

		if (tx_status & AC_SFLD_OK)
		{
			int	ncollisions;

			lp->stats.tx_packets++;
			ncollisions = tx_status & AC_SFLD_MAXCOL;
			lp->stats.collisions += ncollisions;
			/*
			if (ncollisions > 0)
				printk("%s: tx completed after %d collisions.\n", dev->name, ncollisions);
			*/
		}
		else
		{
			lp->stats.tx_errors++;
			if (tx_status & AC_SFLD_S10)
			{
				lp->stats.tx_carrier_errors++;
				if (wavelan_debug > 0)
					printk("%s:     tx error: no CS.\n", dev->name);
			}
			if (tx_status & AC_SFLD_S9)
			{
				lp->stats.tx_carrier_errors++;
				printk("%s:     tx error: lost CTS.\n", dev->name);
			}
			if (tx_status & AC_SFLD_S8)
			{
				lp->stats.tx_fifo_errors++;
				printk("%s:     tx error: slow DMA.\n", dev->name);
			}
			if (tx_status & AC_SFLD_S6)
			{
				lp->stats.tx_heartbeat_errors++;
				if (wavelan_debug > 0)
					printk("%s:     tx error: heart beat.\n", dev->name);
			}
			if (tx_status & AC_SFLD_S5)
			{
				lp->stats.tx_aborted_errors++;
				if (wavelan_debug > 0)
					printk("%s:     tx error: too many collisions.\n", dev->name);
			}
		}

		if (wavelan_debug > 5)
			printk("%s:     tx completed, tx_status 0x%04x.\n", dev->name, tx_status);
	}

/*
	if (nreaped > 1)
		printk("c%d", nreaped);
*/

	/*
	 * Inform upper layers.
	 */
	if (lp->tx_n_in_use < NTXBLOCKS - 1)
	{
		dev->tbusy = 0;
		mark_bh(NET_BH);
	}

	return nreaped;
}

static
void
wavelan_watchdog(unsigned long a)
{
	device		*dev;
	net_local	*lp;
	unsigned short	ioaddr;
	unsigned long	x;
	unsigned int	nreaped;

	x = wavelan_splhi();

	dev = (device *)a;
	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	if (lp->tx_n_in_use <= 0)
	{
		wavelan_splx(x);
		return;
	}

	lp->watchdog.expires = jiffies+WATCHDOG_JIFFIES;
	add_timer(&lp->watchdog);

	if (jiffies - dev->trans_start < WATCHDOG_JIFFIES)
	{
		wavelan_splx(x);
		return;
	}

	nreaped = wavelan_complete(dev, ioaddr, lp);

	printk("%s: warning: wavelan_watchdog(): %d reaped, %d remain.\n", dev->name, nreaped, lp->tx_n_in_use);
	/*
	wavelan_scb_show(ioaddr);
	wavelan_ru_show(dev);
	wavelan_cu_show(dev);
	wavelan_dev_show(dev);
	wavelan_local_show(dev);
	*/

	wavelan_splx(x);
}

static
void
wavelan_interrupt(int irq, struct pt_regs *regs)
{
	device		*dev;
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	hasr;
	unsigned short	status;
	unsigned short	ack_cmd;

	if ((dev = (device *)(irq2dev_map[irq])) == (device *)0)
	{
		printk("wavelan_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	dev->interrupt = 1;

	if ((hasr = hasr_read(ioaddr)) & HASR_MMC_INTR)
	{
		unsigned char	dce_status;

		/*
		 * Interrupt from the modem management controller.
		 * This will clear it -- ignored for now.
		 */
		mmc_read(ioaddr, mmroff(0, mmr_dce_status), &dce_status, sizeof(dce_status));
		if (wavelan_debug > 0)
			printk("%s: warning: wavelan_interrupt(): unexpected mmc interrupt: status 0x%04x.\n", dev->name, dce_status);
	}

	if ((hasr & HASR_82586_INTR) == 0)
	{
		dev->interrupt = 0;
		if (wavelan_debug > 0)
			printk("%s: warning: wavelan_interrupt() but (hasr & HASR_82586_INTR) == 0.\n", dev->name);
		return;
	}

	obram_read(ioaddr, scboff(OFFSET_SCB, scb_status), (unsigned char *)&status, sizeof(status));

	/*
	 * Acknowledge the interrupt(s).
	 */
	ack_cmd = status & SCB_ST_INT;

	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&ack_cmd, sizeof(ack_cmd));

	set_chan_attn(ioaddr, lp->hacr);

	if (wavelan_debug > 5)
		printk("%s: interrupt, status 0x%04x.\n", dev->name, status);

	if ((status & SCB_ST_CX) == SCB_ST_CX)
	{
		/*
		 * Command completed.
		 */
		if (wavelan_debug > 5)
			printk("%s: command completed.\n", dev->name);
		(void)wavelan_complete(dev, ioaddr, lp);
	}

	if ((status & SCB_ST_FR) == SCB_ST_FR)
	{
		/*
		 * Frame received.
		 */
		if (wavelan_debug > 5)
			printk("%s: received packet.\n", dev->name);
		wavelan_receive(dev);
	}

	if
	(
		(status & SCB_ST_CNA) == SCB_ST_CNA
		||
		(((status & SCB_ST_CUS) != SCB_ST_CUS_ACTV) && dev->start)
	)
	{
		printk("%s: warning: CU inactive -- restarting.\n", dev->name);

		(void)wavelan_hardware_reset(dev);
	}

	if
	(
		(status & SCB_ST_RNR) == SCB_ST_RNR
		||
		(((status & SCB_ST_RUS) != SCB_ST_RUS_RDY) && dev->start)
	)
	{
		printk("%s: warning: RU not ready -- restarting.\n", dev->name);

		(void)wavelan_hardware_reset(dev);
	}

	dev->interrupt = 0;
}

static
int
wavelan_close(device *dev)
{
	unsigned short	ioaddr;
	net_local	*lp;
	unsigned short	scb_cmd;

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_close(dev=0x%x)\n", dev->name, (unsigned int)dev);

	ioaddr = dev->base_addr;
	lp = (net_local *)dev->priv;

	dev->tbusy = 1;
	dev->start = 0;

	/*
	 * Flush the Tx and disable Rx.
	 */
	scb_cmd = (SCB_CMD_CUC & SCB_CMD_CUC_SUS) | (SCB_CMD_RUC & SCB_CMD_RUC_SUS);
	obram_write(ioaddr, scboff(OFFSET_SCB, scb_command), (unsigned char *)&scb_cmd, sizeof(scb_cmd));
	set_chan_attn(ioaddr, lp->hacr);

	wavelan_ints_off(dev);

	free_irq(dev->irq);
	irq2dev_map[dev->irq] = (device *)0;

	/*
	 * Release the ioport-region.
	 */
	release_region(ioaddr, sizeof(ha_t));

	MOD_DEC_USE_COUNT;

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_close(): 0\n", dev->name);

	return 0;
}

/*
 * Get the current statistics.
 * This may be called with the card open or closed.
 */
static
en_stats	*
wavelan_get_stats(device *dev)
{
	net_local	*lp;

	lp = (net_local *)dev->priv;

	return &lp->stats;
}

static
void
wavelan_set_multicast_list(device *dev)
{
	net_local	*lp;
	unsigned long	x;

	if (wavelan_debug > 0)
		printk("%s: ->wavelan_set_multicast_list(dev=0x%x)", dev->name, dev);

	lp = (net_local *)dev->priv;

	if(dev->flags&IFF_PROMISC)
	{
		/*
		 * Promiscuous mode: receive all packets.
		 */
		lp->promiscuous = 1;
		x = wavelan_splhi();
		(void)wavelan_hardware_reset(dev);
		wavelan_splx(x);
	}
#if MULTICAST_IS_ADDED	
	else if((dev->flags&IFF_ALLMULTI)||dev->mc_list)
	{
			
	
	}
#endif	
	else	
	{
		/*
		 * Normal mode: disable promiscuous mode,
		 * clear multicast list.
		 */
		lp->promiscuous = 0;
		x = wavelan_splhi();
		(void)wavelan_hardware_reset(dev);
		wavelan_splx(x);
	}

	if (wavelan_debug > 0)
		printk("%s: <-wavelan_set_multicast_list()\n", dev->name);
}

/*
 * Extra WaveLAN-specific device data.
 * "cat /proc/net/wavelan" -- see fs/proc/net.c.
 */
static
int
sprintf_stats(char *buffer, device *dev)
{
	net_local	*lp;
	unsigned char	v;
	mmr_t		m;

	lp = (net_local *)dev->priv;

	if (lp == (net_local *)0)
		return sprintf(buffer, "%6s: No statistics available.\n", dev->name);

	v = (unsigned char)1;
	mmc_write(dev->base_addr, mmwoff(0, mmw_freeze), &v, sizeof(v));

	mmc_read(dev->base_addr, mmroff(0, mmr_dce_status), &m.mmr_dce_status, sizeof(m.mmr_dce_status));
	mmc_read(dev->base_addr, mmroff(0, mmr_correct_nwid_h), &m.mmr_correct_nwid_h, sizeof(m.mmr_correct_nwid_h));
	mmc_read(dev->base_addr, mmroff(0, mmr_correct_nwid_l), &m.mmr_correct_nwid_l, sizeof(m.mmr_correct_nwid_l));
	mmc_read(dev->base_addr, mmroff(0, mmr_wrong_nwid_h), &m.mmr_wrong_nwid_h, sizeof(m.mmr_wrong_nwid_h));
	mmc_read(dev->base_addr, mmroff(0, mmr_wrong_nwid_l), &m.mmr_wrong_nwid_l, sizeof(m.mmr_wrong_nwid_l));
	mmc_read(dev->base_addr, mmroff(0, mmr_signal_lvl), &m.mmr_signal_lvl, sizeof(m.mmr_signal_lvl));
	mmc_read(dev->base_addr, mmroff(0, mmr_silence_lvl), &m.mmr_silence_lvl, sizeof(m.mmr_silence_lvl));
	mmc_read(dev->base_addr, mmroff(0, mmr_sgnl_qual), &m.mmr_sgnl_qual, sizeof(m.mmr_sgnl_qual));

	v = (unsigned char)0;
	mmc_write(dev->base_addr, mmwoff(0, mmw_freeze), &v, sizeof(v));

	lp->correct_nwid += (m.mmr_correct_nwid_h << 8) | m.mmr_correct_nwid_l;
	lp->wrong_nwid += (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l;

	return sprintf
	(
		buffer,
		"%6s:   %02x %08x %08x   %02x   %02x   %02x   %02x    %u\n",
		dev->name,
		m.mmr_dce_status,
		lp->correct_nwid,
		lp->wrong_nwid,
		m.mmr_signal_lvl,
		m.mmr_silence_lvl,
		m.mmr_sgnl_qual,
		lp->tx_n_in_use,
		lp->nresets
	);
}

static int
wavelan_get_info(char *buffer, char **start, off_t offset, int length, int dummy)
{
	int		len;
	off_t		begin;
	off_t		pos;
	int		size;
	unsigned long	x;

	len = 0;
	begin = 0;
	pos = 0;

	size = sprintf(buffer, "%s", "Iface |  dce    +nwid    -nwid  lvl slnc qual ntxq nrst\n");

	pos += size;
	len += size;
	
	x = wavelan_splhi();

	if (first_wavelan != (net_local *)0)
	{
		net_local	*lp;

		lp = first_wavelan;
		do
		{
			size = sprintf_stats(buffer + len, lp->dev);

			len += size;
			pos = begin + len;
					
			if (pos < offset)
			{
				len = 0;
				begin = pos;
			}

			if (pos > offset + length)
				break;
		}
			while ((lp = lp->next) != first_wavelan);
	}

	wavelan_splx(x);

	*start = buffer + (offset - begin);	/* Start of wanted data */
	len -= (offset - begin);		/* Start slop */
	if (len > length)
		len = length;			/* Ending slop */

	return len;
}

#if	defined(MODULE)
static char		devicename[9]		= { 0, };
static struct device	dev_wavelan		=
{
	devicename, /* device name is inserted by linux/drivers/net/net_init.c */
	0, 0, 0, 0,
	0, 0,
	0, 0, 0, NULL, wavelan_probe
};

static int io = 0x390; /* Default from above.. */
static int irq = 0;

int
init_module(void)
{
	dev_wavelan.base_addr = io;
	dev_wavelan.irq       = irq;
	if (register_netdev(&dev_wavelan) != 0)
		return -EIO;

	return 0;
}

void
cleanup_module(void)
{
	proc_net_unregister(PROC_NET_WAVELAN);
	unregister_netdev(&dev_wavelan);
	kfree_s(dev_wavelan.priv, sizeof(struct net_local));
	dev_wavelan.priv = NULL;
}
#endif	/* defined(MODULE) */

static
void
wavelan_cu_show_one(device *dev, net_local *lp, int i, unsigned short p)
{
	unsigned short	ioaddr;
	ac_tx_t		actx;

	ioaddr = dev->base_addr;

	printk("%d: 0x%x:", i, p);

	obram_read(ioaddr, p, (unsigned char *)&actx, sizeof(actx));
	printk(" status=0x%x,", actx.tx_h.ac_status);
	printk(" command=0x%x,", actx.tx_h.ac_command);

/*
	{
		tbd_t	tbd;

		obram_read(ioaddr, actx.tx_tbd_offset, (unsigned char *)&tbd, sizeof(tbd));
		printk(" tbd_status=0x%x,", tbd.tbd_status);
	}
*/

	printk("|");
}

#if	0
static
void
wavelan_psa_show(psa_t *p)
{
	printk("psa:");

	printk("psa_io_base_addr_1: 0x%02x,", p->psa_io_base_addr_1);
	printk("psa_io_base_addr_2: 0x%02x,", p->psa_io_base_addr_2);
	printk("psa_io_base_addr_3: 0x%02x,", p->psa_io_base_addr_3);
	printk("psa_io_base_addr_4: 0x%02x,", p->psa_io_base_addr_4);
	printk("psa_rem_boot_addr_1: 0x%02x,", p->psa_rem_boot_addr_1);
	printk("psa_rem_boot_addr_2: 0x%02x,", p->psa_rem_boot_addr_2);
	printk("psa_rem_boot_addr_3: 0x%02x,", p->psa_rem_boot_addr_3);
	printk("psa_holi_params: 0x%02x,", p->psa_holi_params);
	printk("psa_int_req_no: %d,", p->psa_int_req_no);
	printk
	(
		"psa_univ_mac_addr[]: %02x:%02x:%02x:%02x:%02x:%02x,",
		p->psa_univ_mac_addr[0],
		p->psa_univ_mac_addr[1],
		p->psa_univ_mac_addr[2],
		p->psa_univ_mac_addr[3],
		p->psa_univ_mac_addr[4],
		p->psa_univ_mac_addr[5]
	);
	printk
	(
		"psa_local_mac_addr[]: %02x:%02x:%02x:%02x:%02x:%02x,",
		p->psa_local_mac_addr[0],
		p->psa_local_mac_addr[1],
		p->psa_local_mac_addr[2],
		p->psa_local_mac_addr[3],
		p->psa_local_mac_addr[4],
		p->psa_local_mac_addr[5]
	);
	printk("psa_univ_local_sel: %d,", p->psa_univ_local_sel);
	printk("psa_comp_number: %d,", p->psa_comp_number);
	printk("psa_thr_pre_set: 0x%02x,", p->psa_thr_pre_set);
	printk("psa_feature_select/decay_prm: 0x%02x,", p->psa_feature_select);
	printk("psa_subband/decay_update_prm: %d,", p->psa_subband);
	printk("psa_quality_thr: 0x%02x,", p->psa_quality_thr);
	printk("psa_mod_delay: 0x%02x,", p->psa_mod_delay);
	printk("psa_nwid: 0x%02x%02x,", p->psa_nwid[0], p->psa_nwid[1]);
	printk("psa_undefined: %d,", p->psa_undefined);
	printk("psa_encryption_select: %d,", p->psa_encryption_select);
	printk
	(
		"psa_encryption_key[]: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x,",
		p->psa_encryption_key[0],
		p->psa_encryption_key[1],
		p->psa_encryption_key[2],
		p->psa_encryption_key[3],
		p->psa_encryption_key[4],
		p->psa_encryption_key[5],
		p->psa_encryption_key[6],
		p->psa_encryption_key[7]
	);
	printk("psa_databus_width: %d,", p->psa_databus_width);
	printk("psa_call_code/auto_squelch: 0x%02x,", p->psa_call_code);
	printk("psa_no_of_retries: %d,", p->psa_no_of_retries);
	printk("psa_acr: %d,", p->psa_acr);
	printk("psa_dump_count: %d,", p->psa_dump_count);
	printk("psa_nwid_prefix: 0x%02x,", p->psa_nwid_prefix);
	printk("psa_conf_status: %d,", p->psa_conf_status);
	printk("psa_crc: 0x%02x%02x,", p->psa_crc[0], p->psa_crc[1]);
	printk("psa_crc_status: 0x%02x,", p->psa_crc_status);

	printk("\n");
}

static
void
wavelan_mmc_show(unsigned short ioaddr)
{
	mmr_t	m;

	mmc_read(ioaddr, 0, (unsigned char *)&m, sizeof(m));

	printk("mmr:");
	printk(" des_status: 0x%x", m.mmr_des_status);
	printk(" des_avail: 0x%x", m.mmr_des_avail);
	printk(" des_io_invert: 0x%x", m.mmr_des_io_invert);
	printk
	(
		" dce_status: 0x%x[%s%s%s%s]",
		m.mmr_dce_status & 0x0F,
		(m.mmr_dce_status & MMR_DCE_STATUS_ENERG_DET) ? "energy detected," : "",
		(m.mmr_dce_status & MMR_DCE_STATUS_LOOPT_IND) ? "loop test indicated," : "",
		(m.mmr_dce_status & MMR_DCE_STATUS_XMTITR_IND) ? "transmitter on," : "",
		(m.mmr_dce_status & MMR_DCE_STATUS_JBR_EXPIRED) ? "jabber timer expired," : ""
	);
	printk(" correct_nwid: %d", m.mmr_correct_nwid_h << 8 | m.mmr_correct_nwid_l);
	printk(" wrong_nwid: %d", (m.mmr_wrong_nwid_h << 8) | m.mmr_wrong_nwid_l);
	printk(" thr_pre_set: 0x%x", m.mmr_thr_pre_set);
	printk(" signal_lvl: %d", m.mmr_signal_lvl);
	printk(" silence_lvl: %d", m.mmr_silence_lvl);
	printk(" sgnl_qual: 0x%x", m.mmr_sgnl_qual);
	printk(" netw_id_l: %x", m.mmr_netw_id_l);

	printk("\n");
}
#endif	/* 0 */

static
void
wavelan_scb_show(unsigned short ioaddr)
{
	scb_t	scb;

	obram_read(ioaddr, OFFSET_SCB, (unsigned char *)&scb, sizeof(scb));   

	printk("scb:");

	printk(" status:");
	printk
	(
		" stat 0x%x[%s%s%s%s]",
		(scb.scb_status & (SCB_ST_CX | SCB_ST_FR | SCB_ST_CNA | SCB_ST_RNR)) >> 12,
		(scb.scb_status & SCB_ST_CX) ? "cmd completion interrupt," : "",
		(scb.scb_status & SCB_ST_FR) ? "frame received," : "",
		(scb.scb_status & SCB_ST_CNA) ? "cmd unit not active," : "",
		(scb.scb_status & SCB_ST_RNR) ? "rcv unit not ready," : ""
	);
	printk
	(
		" cus 0x%x[%s%s%s]",
		(scb.scb_status & SCB_ST_CUS) >> 8,
		((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_IDLE) ? "idle" : "",
		((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_SUSP) ? "suspended" : "",
		((scb.scb_status & SCB_ST_CUS) == SCB_ST_CUS_ACTV) ? "active" : ""
	);
	printk
	(
		" rus 0x%x[%s%s%s%s]",
		(scb.scb_status & SCB_ST_RUS) >> 4,
		((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_IDLE) ? "idle" : "",
		((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_SUSP) ? "suspended" : "",
		((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_NRES) ? "no resources" : "",
		((scb.scb_status & SCB_ST_RUS) == SCB_ST_RUS_RDY) ? "ready" : ""
	);

	printk(" command:");
	printk
	(
		" ack 0x%x[%s%s%s%s]",
		(scb.scb_command & (SCB_CMD_ACK_CX | SCB_CMD_ACK_FR | SCB_CMD_ACK_CNA | SCB_CMD_ACK_RNR)) >> 12,
		(scb.scb_command & SCB_CMD_ACK_CX) ? "ack cmd completion," : "",
		(scb.scb_command & SCB_CMD_ACK_FR) ? "ack frame received," : "",
		(scb.scb_command & SCB_CMD_ACK_CNA) ? "ack CU not active," : "",
		(scb.scb_command & SCB_CMD_ACK_RNR) ? "ack RU not ready," : ""
	);
	printk
	(
		" cuc 0x%x[%s%s%s%s%s]",
		(scb.scb_command & SCB_CMD_CUC) >> 8,
		((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_NOP) ? "nop" : "",
		((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_GO) ? "start cbl_offset" : "",
		((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_RES) ? "resume execution" : "",
		((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_SUS) ? "suspend execution" : "",
		((scb.scb_command & SCB_CMD_CUC) == SCB_CMD_CUC_ABT) ? "abort execution" : ""
	);
	printk
	(
		" ruc 0x%x[%s%s%s%s%s]",
		(scb.scb_command & SCB_CMD_RUC) >> 4,
		((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_NOP) ? "nop" : "",
		((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_GO) ? "start rfa_offset" : "",
		((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_RES) ? "resume reception" : "",
		((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_SUS) ? "suspend reception" : "",
		((scb.scb_command & SCB_CMD_RUC) == SCB_CMD_RUC_ABT) ? "abort reception" : ""
	);

	printk(" cbl_offset 0x%x", scb.scb_cbl_offset);
	printk(" rfa_offset 0x%x", scb.scb_rfa_offset);

	printk(" crcerrs %d", scb.scb_crcerrs);
	printk(" alnerrs %d", scb.scb_alnerrs);
	printk(" rscerrs %d", scb.scb_rscerrs);
	printk(" ovrnerrs %d", scb.scb_ovrnerrs);

	printk("\n");
}

static
void
wavelan_ru_show(device *dev)
{
	net_local	*lp;

	lp = (net_local *)dev->priv;

	printk("ru:");
	/*
	 * Not implemented yet...
	 */
	printk("\n");
}

static
void
wavelan_cu_show(device *dev)
{
	net_local	*lp;
	unsigned int	i;
	unsigned short	p;

	lp = (net_local *)dev->priv;

	printk("cu:");
	printk("\n");

	for (i = 0, p = lp->tx_first_in_use; i < NTXBLOCKS; i++)
	{
		wavelan_cu_show_one(dev, lp, i, p);

		p += TXBLOCKZ;
		if (p >= OFFSET_CU + NTXBLOCKS * TXBLOCKZ)
			p -= NTXBLOCKS * TXBLOCKZ;
	}
}

static
void
wavelan_dev_show(device *dev)
{
	printk("dev:");
	printk(" start=%d,", dev->start);
	printk(" tbusy=%ld,", dev->tbusy);
	printk(" interrupt=%d,", dev->interrupt);
	printk(" trans_start=%ld,", dev->trans_start);
	printk(" flags=0x%x,", dev->flags);
	printk("\n");
}

static
void
wavelan_local_show(device *dev)
{
	net_local	*lp;

	lp = (net_local *)dev->priv;

	printk("local:");
	printk(" tx_n_in_use=%d,", lp->tx_n_in_use);
	printk(" hacr=0x%x,", lp->hacr);
	printk(" rx_head=0x%x,", lp->rx_head);
	printk(" rx_last=0x%x,", lp->rx_last);
	printk(" tx_first_free=0x%x,", lp->tx_first_free);
	printk(" tx_first_in_use=0x%x,", lp->tx_first_in_use);
	printk("\n");
}

/*
 * This software may only be used and distributed
 * according to the terms of the GNU Public License.
 *
 * This software was developed as a component of the
 * Linux operating system.
 * It is based on other device drivers and information
 * either written or supplied by:
 *	Ajay Bakre (bakre@paul.rutgers.edu),
 *	Donald Becker (becker@cesdis.gsfc.nasa.gov),
 *	Loeke Brederveld (Loeke.Brederveld@Utrecht.NCR.com),
 *	Anders Klemets (klemets@it.kth.se),
 *	Vladimir V. Kolpakov (w@stier.koenig.ru),
 *	Marc Meertens (Marc.Meertens@Utrecht.NCR.com),
 *	Pauline Middelink (middelin@polyware.iaf.nl),
 *	Robert Morris (rtm@das.harvard.edu),
 *	Girish Welling (welling@paul.rutgers.edu),
 *
 * Thanks go also to:
 *	James Ashton (jaa101@syseng.anu.edu.au),
 *	Alan Cox (iialan@iiit.swan.ac.uk),
 *	Allan Creighton (allanc@cs.usyd.edu.au),
 *	Matthew Geier (matthew@cs.usyd.edu.au),
 *	Remo di Giovanni (remo@cs.usyd.edu.au),
 *	Eckhard Grah (grah@wrcs1.urz.uni-wuppertal.de),
 *	Vipul Gupta (vgupta@cs.binghamton.edu),
 *	Mark Hagan (mhagan@wtcpost.daytonoh.NCR.COM),
 *	Tim Nicholson (tim@cs.usyd.edu.au),
 *	Ian Parkin (ian@cs.usyd.edu.au),
 *	John Rosenberg (johnr@cs.usyd.edu.au),
 *	George Rossi (george@phm.gov.au),
 *	Arthur Scott (arthur@cs.usyd.edu.au),
 *	Peter Storey,
 * for their assistance and advice.
 *
 * Please send bug reports, updates, comments to:
 *
 * Bruce Janson                                    Email:  bruce@cs.usyd.edu.au
 * Basser Department of Computer Science           Phone:  +61-2-351-3423
 * University of Sydney, N.S.W., 2006, AUSTRALIA   Fax:    +61-2-351-3838
 */
