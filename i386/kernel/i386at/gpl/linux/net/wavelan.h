#define WAVELAN_ADDR_SIZE	6	/* Size of a MAC address */
#define SA_ADDR0		0x08	/* First octet of WaveLAN MAC addresses */
#define SA_ADDR1		0x00	/* Second octet of WaveLAN MAC addresses */
#define SA_ADDR2		0x0E	/* Third octet of WaveLAN MAC addresses */
#define WAVELAN_MTU		1500	/* Maximum size of WaveLAN packet */

/*
 * Parameter Storage Area (PSA).
 */
typedef struct psa_t	psa_t;
struct psa_t
{
	unsigned char	psa_io_base_addr_1;	/* Base address 1 ??? */
	unsigned char	psa_io_base_addr_2;	/* Base address 2 */
	unsigned char	psa_io_base_addr_3;	/* Base address 3 */
	unsigned char	psa_io_base_addr_4;	/* Base address 4 */
	unsigned char	psa_rem_boot_addr_1;	/* Remote Boot Address 1 */
	unsigned char	psa_rem_boot_addr_2;	/* Remote Boot Address 2 */
	unsigned char	psa_rem_boot_addr_3;	/* Remote Boot Address 3 */
	unsigned char	psa_holi_params;	/* HOst Lan Interface (HOLI) Parameters */
	unsigned char	psa_int_req_no;		/* Interrupt Request Line */
	unsigned char	psa_unused0[7];		/* unused */
	unsigned char	psa_univ_mac_addr[WAVELAN_ADDR_SIZE];	/* Universal (factory) MAC Address */
	unsigned char	psa_local_mac_addr[WAVELAN_ADDR_SIZE];	/* Local MAC Address */
	unsigned char	psa_univ_local_sel;	/* Universal Local Selection */
#define		PSA_UNIVERSAL	0		/* Universal (factory) */
#define		PSA_LOCAL	1		/* Local */
	unsigned char	psa_comp_number;	/* Compatibility Number: */
#define		PSA_COMP_PC_AT_915	0 	/* PC-AT 915 MHz	*/
#define		PSA_COMP_PC_MC_915	1 	/* PC-MC 915 MHz	*/
#define		PSA_COMP_PC_AT_2400	2 	/* PC-AT 2.4 GHz	*/
#define		PSA_COMP_PC_MC_2400	3 	/* PC-MC 2.4 GHz	*/
#define		PSA_COMP_PCMCIA_915	4 	/* PCMCIA 915 MHz	*/
	unsigned char	psa_thr_pre_set;	/* Modem Threshold Preset */
	unsigned char	psa_feature_select;	/* ??? */
#if	0
	<alias for above>
	unsigned char	psa_decay_prm;		/* Modem Decay */
#endif	/* 0 */
	unsigned char	psa_subband;		/* Subband	*/
#define		PSA_SUBBAND_915		0	/* 915 MHz	*/
#define		PSA_SUBBAND_2425	1	/* 2425 MHz	*/
#define		PSA_SUBBAND_2460	2	/* 2460 MHz	*/
#define		PSA_SUBBAND_2484	3	/* 2484 MHz	*/
#define		PSA_SUBBAND_2430_5	4	/* 2430.5 MHz	*/
#if	0
	<alias for above>
	unsigned char	psa_decay_updat_prm;	/* Modem Decay Update ??? */
#endif	/* 0 */
	unsigned char	psa_quality_thr;	/* Modem Quality Threshold */
	unsigned char	psa_mod_delay;		/* Modem Delay ??? */
	unsigned char	psa_nwid[2];		/* Network ID */
	unsigned char	psa_undefined;		/* undefined */
	unsigned char	psa_encryption_select;	/* Encryption On Off */
	unsigned char	psa_encryption_key[8];	/* Encryption Key */
	unsigned char	psa_databus_width;	/* 8/16 bit bus width */
	unsigned char	psa_call_code;		/* ??? */
#if	0
	<alias for above>
	unsigned char	psa_auto_squelch;	/* Automatic Squelch level On off ??? */
#endif	/* 0 */
	unsigned char	psa_no_of_retries;	/* LAN Cont. No of retries */
	unsigned char	psa_acr;		/* LAN Cont. ACR */
	unsigned char	psa_dump_count;		/* number of Dump Commands in TFB */
	unsigned char	psa_unused1[4];		/* unused */
	unsigned char	psa_nwid_prefix;	/* ??? */
	unsigned char	psa_unused2[3];		/* unused */
	unsigned char	psa_conf_status;	/* Card Configuration Status */
	unsigned char	psa_crc[2];		/* CRC over PSA */
	unsigned char	psa_crc_status;		/* CRC Valid Flag */
};
#if	STRUCT_CHECK == 1
#define	PSA_SIZE	64
#endif	/* STRUCT_CHECK == 1 */

/*
 * Modem Management Controller (MMC) write structure.
 */
typedef struct mmw_t	mmw_t;
struct mmw_t
{
	unsigned char	mmw_encr_key[8];	/* encryption key */
	unsigned char	mmw_encr_enable;	/* enable/disable encryption */
	unsigned char	mmw_unused0[1];		/* unused */
	unsigned char	mmw_des_io_invert;	/* ??? */
	unsigned char	mmw_unused1[5];		/* unused */
	unsigned char	mmw_loopt_sel;		/* looptest selection */
#define		MMW_LOOPT_SEL_UNDEFINED	0x40	/* undefined */
#define		MMW_LOOPT_SEL_INT	0x20	/* activate Attention Request */
#define		MMW_LOOPT_SEL_LS	0x10	/* looptest without collision avoidance */
#define		MMW_LOOPT_SEL_LT3A	0x08	/* looptest 3a */
#define		MMW_LOOPT_SEL_LT3B	0x04	/* looptest 3b */
#define		MMW_LOOPT_SEL_LT3C	0x02	/* looptest 3c */
#define		MMW_LOOPT_SEL_LT3D	0x01	/* looptest 3d */
	unsigned char	mmw_jabber_enable;	/* jabber timer enable */
	unsigned char	mmw_freeze;		/* freeze / unfreeze signal level */
	unsigned char	mmw_anten_sel;		/* antenna selection */
#define		MMW_ANTEN_SEL_SEL	0x01	/* direct antenna selection */
#define		MMW_ANTEN_SEL_ALG_EN	0x02	/* antenna selection algorithm enable */
	unsigned char	mmw_ifs;		/* inter frame spacing */
	unsigned char	mmw_mod_delay;	 	/* modem delay */
	unsigned char	mmw_jam_time;		/* jamming time */
	unsigned char	mmw_unused2[1];		/* unused */
	unsigned char	mmw_thr_pre_set;	/* level threshold preset */
	unsigned char	mmw_decay_prm;		/* decay parameters */
	unsigned char	mmw_decay_updat_prm;	/* decay update parameters */
	unsigned char	mmw_quality_thr;	/* quality (z-quotient) threshold */
	unsigned char	mmw_netw_id_l;		/* NWID low order byte */
	unsigned char	mmw_netw_id_h;		/* NWID high order byte */
};
#if	STRUCT_CHECK == 1
#define	MMW_SIZE	30
#endif	/* STRUCT_CHECK == 1 */

#define	mmwoff(p,f) 	(unsigned short)((void *)(&((mmw_t *)((void *)0 + (p)))->f) - (void *)0)

/*
 * Modem Management Controller (MMC) read structure.
 */
typedef struct mmr_t	mmr_t;
struct mmr_t
{
	unsigned char	mmr_unused0[8];		/* unused */
	unsigned char	mmr_des_status;		/* encryption status */
	unsigned char	mmr_des_avail;		/* encryption available (0x55 read) */
	unsigned char	mmr_des_io_invert;	/* des I/O invert register */
	unsigned char	mmr_unused1[5];		/* unused */
	unsigned char	mmr_dce_status;		/* DCE status */
#define		MMR_DCE_STATUS_ENERG_DET	0x01	/* energy detected */
#define		MMR_DCE_STATUS_LOOPT_IND	0x02	/* loop test indicated */
#define		MMR_DCE_STATUS_XMTITR_IND	0x04	/* transmitter on */
#define		MMR_DCE_STATUS_JBR_EXPIRED	0x08	/* jabber timer expired */
	unsigned char	mmr_unused2[3];		/* unused */
	unsigned char	mmr_correct_nwid_l;	/* no. of correct NWID's rxd (low) */
	unsigned char	mmr_correct_nwid_h;	/* no. of correct NWID's rxd (high) */
	unsigned char	mmr_wrong_nwid_l;	/* count of wrong NWID's received (low) */
	unsigned char	mmr_wrong_nwid_h;	/* count of wrong NWID's received (high) */
	unsigned char	mmr_thr_pre_set;	/* level threshold preset */
	unsigned char	mmr_signal_lvl;		/* signal level */
	unsigned char	mmr_silence_lvl;	/* silence level */
	unsigned char	mmr_sgnl_qual;		/* signal quality */
#define		MMR_SGNL_QUAL_0		0x01	/* signal quality 0 */
#define		MMR_SGNL_QUAL_1		0x02	/* signal quality 1 */
#define		MMR_SGNL_QUAL_2		0x04	/* signal quality 2 */
#define		MMR_SGNL_QUAL_3		0x08	/* signal quality 3 */
#define		MMR_SGNL_QUAL_S_A	0x80	/* currently selected antenna */
	unsigned char	mmr_netw_id_l;		/* NWID low order byte ??? */
	unsigned char	mmr_unused3[1];		/* unused */
};
#if	STRUCT_CHECK == 1
#define	MMR_SIZE	30
#endif	/* STRUCT_CHECK == 1 */

#define	MMR_LEVEL_MASK	0x3F

#define	mmroff(p,f) 	(unsigned short)((void *)(&((mmr_t *)((void *)0 + (p)))->f) - (void *)0)

/*
 * Host Adaptor structure.
 * (base is board port address).
 */
typedef union hacs_u	hacs_u;
union hacs_u
{
	unsigned short	hu_command;		/* Command register */
#define		HACR_RESET		0x0001	/* Reset board */
#define		HACR_CA			0x0002	/* Set Channel Attention for 82586 */
#define		HACR_16BITS		0x0004	/* 16 bits operation (0 => 8bits) */
#define		HACR_OUT0		0x0008	/* General purpose output pin 0 */
						/* not used - must be 1 */
#define		HACR_OUT1		0x0010	/* General purpose output pin 1 */
						/* not used - must be 1 */
#define		HACR_82586_INT_ENABLE	0x0020	/* Enable 82586 interrupts */
#define		HACR_MMC_INT_ENABLE	0x0040	/* Enable MMC interrupts */
#define		HACR_INTR_CLR_ENABLE	0x0080	/* Enable interrupt status read/clear */
	unsigned short	hu_status;		/* Status Register */
#define		HASR_82586_INTR		0x0001	/* Interrupt request from 82586 */
#define		HASR_MMC_INTR		0x0002	/* Interrupt request from MMC */
#define		HASR_MMC_BUSY		0x0004	/* MMC busy indication */
#define		HASR_PSA_BUSY		0x0008	/* LAN parameter storage area busy */
};

typedef struct ha_t	ha_t;
struct ha_t
{
	hacs_u		ha_cs;		/* Command and status registers */
#define 		ha_command	ha_cs.hu_command
#define 		ha_status	ha_cs.hu_status
	unsigned short	ha_mmcr;	/* Modem Management Ctrl Register */
	unsigned short	ha_pior0;	/* Program I/O Address Register Port 0 */
	unsigned short	ha_piop0;	/* Program I/O Port 0 */
	unsigned short	ha_pior1;	/* Program I/O Address Register Port 1 */
	unsigned short	ha_piop1;	/* Program I/O Port 1 */
	unsigned short	ha_pior2;	/* Program I/O Address Register Port 2 */
	unsigned short	ha_piop2;	/* Program I/O Port 2 */
};
#if	STRUCT_CHECK == 1
#define HA_SIZE		16
#endif	/* STRUCT_CHECK == 1 */

#define	hoff(p,f) 	(unsigned short)((void *)(&((ha_t *)((void *)0 + (p)))->f) - (void *)0)
#define	HACR(p)		hoff(p, ha_command)
#define	HASR(p)		hoff(p, ha_status)
#define	MMCR(p)		hoff(p, ha_mmcr)
#define	PIOR0(p)	hoff(p, ha_pior0)
#define	PIOP0(p)	hoff(p, ha_piop0)
#define	PIOR1(p)	hoff(p, ha_pior1)
#define	PIOP1(p)	hoff(p, ha_piop1)
#define	PIOR2(p)	hoff(p, ha_pior2)
#define	PIOP2(p)	hoff(p, ha_piop2)

/*
 * Program I/O Mode Register values.
 */
#define STATIC_PIO		0	/* Mode 1: static mode */
					/* RAM access ??? */
#define AUTOINCR_PIO		1	/* Mode 2: auto increment mode */
					/* RAM access ??? */
#define AUTODECR_PIO		2	/* Mode 3: auto decrement mode */
					/* RAM access ??? */
#define PARAM_ACCESS_PIO	3	/* Mode 4: LAN parameter access mode */
					/* Parameter access. */
#define PIO_MASK		3	/* register mask */
#define PIOM(cmd,piono)		((u_short)cmd << 10 << (piono * 2))

#define	HACR_DEFAULT		(HACR_OUT0 | HACR_OUT1 | HACR_16BITS | PIOM(STATIC_PIO, 0) | PIOM(AUTOINCR_PIO, 1) | PIOM(PARAM_ACCESS_PIO, 2))
#define	HACR_INTRON		(HACR_82586_INT_ENABLE | HACR_MMC_INT_ENABLE | HACR_INTR_CLR_ENABLE)

#define	MAXDATAZ		(WAVELAN_ADDR_SIZE + WAVELAN_ADDR_SIZE + 2 + WAVELAN_MTU)

/*
 * Onboard 64k RAM layout.
 * (Offsets from 0x0000.)
 */
#define OFFSET_RU		0x0000
#define OFFSET_CU		0x8000
#define OFFSET_SCB		(OFFSET_ISCP - sizeof(scb_t))
#define OFFSET_ISCP		(OFFSET_SCP - sizeof(iscp_t))
#define OFFSET_SCP		I82586_SCP_ADDR

#define	RXBLOCKZ		(sizeof(fd_t) + sizeof(rbd_t) + MAXDATAZ)
#define	TXBLOCKZ		(sizeof(ac_tx_t) + sizeof(ac_nop_t) + sizeof(tbd_t) + MAXDATAZ)

#define	NRXBLOCKS		((OFFSET_CU - OFFSET_RU) / RXBLOCKZ)
#define	NTXBLOCKS		((OFFSET_SCB - OFFSET_CU) / TXBLOCKZ)

/*
 * This software may only be used and distributed
 * according to the terms of the GNU Public License.
 *
 * For more details, see wavelan.c.
 */
