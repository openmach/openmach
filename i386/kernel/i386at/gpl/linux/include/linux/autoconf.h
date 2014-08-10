/*
 * Automatically generated C config: don't edit
 */

/*
 * Loadable module support
 */
#undef CONFIG_MODULES
#undef  CONFIG_MODVERSIONS
#undef  CONFIG_KERNELD

/*
 * General setup
 */
#undef  CONFIG_MATH_EMULATION
#undef CONFIG_NET
#undef  CONFIG_MAX_16M
#define  CONFIG_PCI
#define CONFIG_SYSVIPC 1
#define CONFIG_BINFMT_AOUT 1
#define CONFIG_BINFMT_ELF 1
#undef  CONFIG_KERNEL_ELF
#undef  CONFIG_M386
#define CONFIG_M486 1
#undef  CONFIG_M586
#undef  CONFIG_M686

/*
 * Floppy, IDE, and other block devices
 */
#define CONFIG_BLK_DEV_FD 1
#define CONFIG_BLK_DEV_IDE 1

/*
 * Please see drivers/block/README.ide for help/info on IDE drives
 */
#define CONFIG_BLK_DEV_HD_IDE 1
#define CONFIG_BLK_DEV_IDEATAPI 1
#define CONFIG_BLK_DEV_IDECD 1
#undef  CONFIG_BLK_DEV_IDETAPE
#define CONFIG_BLK_DEV_CMD640 1
#define CONFIG_BLK_DEV_RZ1000 1
#define CONFIG_BLK_DEV_TRITON 1
#define CONFIG_IDE_CHIPSETS 1
#undef  CONFIG_BLK_DEV_RAM
#undef  CONFIG_BLK_DEV_LOOP
#undef  CONFIG_BLK_DEV_XD

/*
 * Networking options
 */
#undef  CONFIG_FIREWALL
#undef  CONFIG_NET_ALIAS
#define CONFIG_INET 1
#undef  CONFIG_IP_FORWARD
#undef  CONFIG_IP_MULTICAST
#undef  CONFIG_IP_ACCT

/*
 * (it is safe to leave these untouched)
 */
#undef  CONFIG_INET_PCTCP
#undef  CONFIG_INET_RARP
#undef  CONFIG_NO_PATH_MTU_DISCOVERY
#undef  CONFIG_TCP_NAGLE_OFF
#define CONFIG_IP_NOSR 1
#define CONFIG_SKB_LARGE 1

/*
 *  
 */
#undef  CONFIG_IPX
#undef  CONFIG_ATALK
#undef  CONFIG_AX25
#undef  CONFIG_NETLINK

/*
 * SCSI support
 */
#define CONFIG_SCSI 1

/*
 * SCSI support type (disk, tape, CDrom)
 */
#define CONFIG_BLK_DEV_SD 1
#undef  CONFIG_CHR_DEV_ST
#define CONFIG_BLK_DEV_SR 1
#undef  CONFIG_CHR_DEV_SG

/*
 * Some SCSI devices (e.g. CD jukebox) support multiple LUNs
 */
#undef  CONFIG_SCSI_MULTI_LUN
#undef  CONFIG_SCSI_CONSTANTS

/*
 * SCSI low-level drivers
 */
#undef  CONFIG_SCSI_ADVANSYS
#define CONFIG_SCSI_AHA152X 1
#define CONFIG_SCSI_AHA1542 1
#undef  CONFIG_SCSI_AHA1740
#define CONFIG_SCSI_AIC7XXX 1
#undef  CONFIG_SCSI_BUSLOGIC
#undef  CONFIG_SCSI_EATA_DMA
#undef  CONFIG_SCSI_EATA_PIO
#define CONFIG_SCSI_U14_34F 1
#undef  CONFIG_SCSI_FUTURE_DOMAIN
#undef  CONFIG_SCSI_GENERIC_NCR5380
#undef  CONFIG_SCSI_IN2000
#undef  CONFIG_SCSI_PAS16
#undef  CONFIG_SCSI_QLOGIC
#define CONFIG_SCSI_SEAGATE 1
#undef  CONFIG_SCSI_T128
#undef  CONFIG_SCSI_ULTRASTOR
#undef  CONFIG_SCSI_7000FASST
#undef  CONFIG_SCSI_EATA
#undef  CONFIG_SCSI_NCR53C406A
#undef  CONFIG_SCSI_AM53C974
#define CONFIG_SCSI_NCR53C7xx 1

/*
 * Network device support
 */
#undef CONFIG_NETDEVICES
#undef  CONFIG_DUMMY
#undef CONFIG_SLIP
#undef CONFIG_SLIP_COMPRESSED
#undef CONFIG_SLIP_SMART
#undef CONFIG_PPP

/*
 * CCP compressors for PPP are only built as modules.
 */
#undef  CONFIG_SCC
#undef  CONFIG_PLIP
#undef  CONFIG_EQUALIZER
#undef  CONFIG_NET_ALPHA
#define CONFIG_NET_VENDOR_SMC 1
#undef  CONFIG_LANCE
#undef  CONFIG_NET_VENDOR_3COM
#undef  CONFIG_EL1
#undef  CONFIG_EL2
#undef  CONFIG_EL3
#undef  CONFIG_VORTEX
#define CONFIG_NET_ISA 1
#undef  CONFIG_E2100
#undef  CONFIG_DEPCA
#undef  CONFIG_EWRK3
#define CONFIG_HPLAN_PLUS 1
#undef  CONFIG_HPLAN
#undef  CONFIG_HP100
#define CONFIG_NE2000 1
#undef  CONFIG_SK_G16
#undef  CONFIG_NET_EISA
#undef  CONFIG_NET_POCKET
#undef  CONFIG_TR
#undef  CONFIG_ARCNET
#define CONFIG_DE4X5 1
#define CONFIG_ULTRA 1

/*
 * CD-ROM drivers (not for SCSI or IDE/ATAPI drives)
 */
#undef  CONFIG_CD_NO_IDESCSI

/*
 * Filesystems
 */
#undef  CONFIG_QUOTA
#define CONFIG_MINIX_FS 1
#undef  CONFIG_EXT_FS
#define CONFIG_EXT2_FS 1
#undef  CONFIG_XIA_FS
#define CONFIG_FAT_FS 1
#define CONFIG_MSDOS_FS 1
#undef  CONFIG_VFAT_FS
#undef  CONFIG_UMSDOS_FS
#define CONFIG_PROC_FS 1
#define CONFIG_NFS_FS 1
#undef  CONFIG_ROOT_NFS
#undef  CONFIG_SMB_FS
#define CONFIG_ISO9660_FS 1
#undef  CONFIG_HPFS_FS
#undef  CONFIG_SYSV_FS

/*
 * Character devices
 */
#undef  CONFIG_CYCLADES
#undef  CONFIG_STALDRV
#define CONFIG_PRINTER 1
#undef  CONFIG_BUSMOUSE
#undef  CONFIG_PSMOUSE
#undef  CONFIG_MS_BUSMOUSE
#undef  CONFIG_ATIXL_BUSMOUSE
#undef  CONFIG_QIC02_TAPE
#undef  CONFIG_APM
#undef  CONFIG_WATCHDOG

/*
 * Sound
 */
#undef  CONFIG_SOUND

/*
 * Kernel hacking
 */
#undef  CONFIG_PROFILE
