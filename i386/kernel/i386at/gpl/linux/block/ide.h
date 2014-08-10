/*
 *  linux/drivers/block/ide.h
 *
 *  Copyright (C) 1994, 1995  Linus Torvalds & authors
 */

#include <linux/config.h>

/*
 * This is the multiple IDE interface driver, as evolved from hd.c.  
 * It supports up to four IDE interfaces, on one or more IRQs (usually 14 & 15).
 * There can be up to two drives per interface, as per the ATA-2 spec.
 *
 * Primary i/f:    ide0: major=3;  (hda)         minor=0; (hdb)         minor=64
 * Secondary i/f:  ide1: major=22; (hdc or hd1a) minor=0; (hdd or hd1b) minor=64
 * Tertiary i/f:   ide2: major=33; (hde)         minor=0; (hdf)         minor=64
 * Quaternary i/f: ide3: major=34; (hdg)         minor=0; (hdh)         minor=64
 */

/******************************************************************************
 * IDE driver configuration options (play with these as desired):
 * 
 * REALLY_SLOW_IO can be defined in ide.c and ide-cd.c, if necessary
 */
#undef REALLY_FAST_IO			/* define if ide ports are perfect */
#define INITIAL_MULT_COUNT	0	/* off=0; on=2,4,8,16,32, etc.. */

#ifndef SUPPORT_VLB_SYNC		/* 1 to support weird 32-bit chips */
#define SUPPORT_VLB_SYNC	1	/* 0 to reduce kernel size */
#endif
#ifndef DISK_RECOVERY_TIME		/* off=0; on=access_delay_time */
#define DISK_RECOVERY_TIME	0	/*  for hardware that needs it */
#endif
#ifndef OK_TO_RESET_CONTROLLER		/* 1 needed for good error recovery */
#define OK_TO_RESET_CONTROLLER	1	/* 0 for use with AH2372A/B interface */
#endif
#ifndef FAKE_FDISK_FOR_EZDRIVE		/* 1 to help linux fdisk with EZDRIVE */
#define FAKE_FDISK_FOR_EZDRIVE 	1	/* 0 to reduce kernel size */
#endif
#ifndef FANCY_STATUS_DUMPS		/* 1 for human-readable drive errors */
#define FANCY_STATUS_DUMPS	1	/* 0 to reduce kernel size */
#endif

#if defined(CONFIG_BLK_DEV_IDECD) || defined(CONFIG_BLK_DEV_IDETAPE)
#define CONFIG_BLK_DEV_IDEATAPI 1
#endif

/*
 * IDE_DRIVE_CMD is used to implement many features of the hdparm utility
 */
#define IDE_DRIVE_CMD		99	/* (magic) undef to reduce kernel size*/

/*
 *  "No user-serviceable parts" beyond this point  :)
 *****************************************************************************/

typedef unsigned char	byte;	/* used everywhere */

/*
 * Probably not wise to fiddle with these
 */
#define ERROR_MAX	8	/* Max read/write errors per sector */
#define ERROR_RESET	3	/* Reset controller every 4th retry */
#define ERROR_RECAL	1	/* Recalibrate every 2nd retry */

/*
 * Ensure that various configuration flags have compatible settings
 */
#ifdef REALLY_SLOW_IO
#undef REALLY_FAST_IO
#endif

/*
 * Definitions for accessing IDE controller registers
 */

#define HWIF(drive)		((ide_hwif_t *)drive->hwif)
#define HWGROUP(drive)		((ide_hwgroup_t *)(HWIF(drive)->hwgroup))

#define IDE_DATA_OFFSET		(0)
#define IDE_ERROR_OFFSET	(1)
#define IDE_NSECTOR_OFFSET	(2)
#define IDE_SECTOR_OFFSET	(3)
#define IDE_LCYL_OFFSET		(4)
#define IDE_HCYL_OFFSET		(5)
#define IDE_SELECT_OFFSET	(6)
#define IDE_STATUS_OFFSET	(7)
#define IDE_FEATURE_OFFSET	IDE_ERROR_OFFSET
#define IDE_COMMAND_OFFSET	IDE_STATUS_OFFSET

#define IDE_DATA_REG		(HWIF(drive)->io_base+IDE_DATA_OFFSET)
#define IDE_ERROR_REG		(HWIF(drive)->io_base+IDE_ERROR_OFFSET)
#define IDE_NSECTOR_REG		(HWIF(drive)->io_base+IDE_NSECTOR_OFFSET)
#define IDE_SECTOR_REG		(HWIF(drive)->io_base+IDE_SECTOR_OFFSET)
#define IDE_LCYL_REG		(HWIF(drive)->io_base+IDE_LCYL_OFFSET)
#define IDE_HCYL_REG		(HWIF(drive)->io_base+IDE_HCYL_OFFSET)
#define IDE_SELECT_REG		(HWIF(drive)->io_base+IDE_SELECT_OFFSET)
#define IDE_STATUS_REG		(HWIF(drive)->io_base+IDE_STATUS_OFFSET)
#define IDE_CONTROL_REG		(HWIF(drive)->ctl_port)
#define IDE_FEATURE_REG		IDE_ERROR_REG
#define IDE_COMMAND_REG		IDE_STATUS_REG
#define IDE_ALTSTATUS_REG	IDE_CONTROL_REG

#ifdef REALLY_FAST_IO
#define OUT_BYTE(b,p)		outb((b),p)
#define IN_BYTE(p)		(byte)inb(p)
#else
#define OUT_BYTE(b,p)		outb_p((b),p)
#define IN_BYTE(p)		(byte)inb_p(p)
#endif /* REALLY_FAST_IO */

#define GET_ERR()		IN_BYTE(IDE_ERROR_REG)
#define GET_STAT()		IN_BYTE(IDE_STATUS_REG)
#define OK_STAT(stat,good,bad)	(((stat)&((good)|(bad)))==(good))
#define BAD_R_STAT		(BUSY_STAT   | ERR_STAT)
#define BAD_W_STAT		(BAD_R_STAT  | WRERR_STAT)
#define BAD_STAT		(BAD_R_STAT  | DRQ_STAT)
#define DRIVE_READY		(READY_STAT  | SEEK_STAT)
#define DATA_READY		(DRIVE_READY | DRQ_STAT)

/*
 * Some more useful definitions
 */
#define IDE_MAJOR_NAME	"ide"	/* the same for all i/f; see also genhd.c */
#define MAJOR_NAME	IDE_MAJOR_NAME
#define PARTN_BITS	6	/* number of minor dev bits for partitions */
#define PARTN_MASK	((1<<PARTN_BITS)-1)	/* a useful bit mask */
#define MAX_DRIVES	2	/* per interface; 2 assumed by lots of code */
#define MAX_HWIFS	4	/* an arbitrary, but realistic limit */
#define SECTOR_WORDS	(512 / 4)	/* number of 32bit words per sector */

/*
 * Timeouts for various operations:
 */
#define WAIT_DRQ	(5*HZ/100)	/* 50msec - spec allows up to 20ms */
#define WAIT_READY	(3*HZ/100)	/* 30msec - should be instantaneous */
#define WAIT_PIDENTIFY	(1*HZ)	/* 1sec   - should be less than 3ms (?) */
#define WAIT_WORSTCASE	(30*HZ)	/* 30sec  - worst case when spinning up */
#define WAIT_CMD	(10*HZ)	/* 10sec  - maximum wait for an IRQ to happen */

#ifdef CONFIG_BLK_DEV_IDETAPE
#include "ide-tape.h"
#endif /* CONFIG_BLK_DEV_IDETAPE */

#ifdef CONFIG_BLK_DEV_IDECD

struct atapi_request_sense {
  unsigned char error_code : 7;
  unsigned char valid      : 1;
  byte reserved1;
  unsigned char sense_key  : 4;
  unsigned char reserved2  : 1;
  unsigned char ili        : 1;
  unsigned char reserved3  : 2;
  byte info[4];
  byte sense_len;
  byte command_info[4];
  byte asc;
  byte ascq;
  byte fru;
  byte sense_key_specific[3];
};

struct packet_command {
  char *buffer;
  int buflen;
  int stat;
  struct atapi_request_sense *sense_data;
  unsigned char c[12];
};

/* Space to hold the disk TOC. */

#define MAX_TRACKS 99
struct atapi_toc_header {
  unsigned short toc_length;
  byte first_track;
  byte last_track;
};

struct atapi_toc_entry {
  byte reserved1;
  unsigned control : 4;
  unsigned adr     : 4;
  byte track;
  byte reserved2;
  union {
    unsigned lba;
    struct {
      byte reserved3;
      byte m;
      byte s;
      byte f;
    } msf;
  } addr;
};

struct atapi_toc {
  int    last_session_lba;
  int    xa_flag;
  unsigned capacity;
  struct atapi_toc_header hdr;
  struct atapi_toc_entry  ent[MAX_TRACKS+1];  /* One extra for the leadout. */
};


/* This structure is annoyingly close to, but not identical with,
   the cdrom_subchnl structure from cdrom.h. */
struct atapi_cdrom_subchnl 
{
  u_char  acdsc_reserved;
  u_char  acdsc_audiostatus;
  u_short acdsc_length;
  u_char  acdsc_format;

  u_char  acdsc_adr:	4;
  u_char  acdsc_ctrl:	4;
  u_char  acdsc_trk;
  u_char  acdsc_ind;
  union
  {
    struct 			
    {
      u_char	minute;
      u_char	second;
      u_char	frame;
    } msf;
    int	lba;
  } acdsc_absaddr;
  union 
  {
    struct 
    {
      u_char	minute;
      u_char	second;
      u_char	frame;
    } msf;
    int	lba;
  } acdsc_reladdr;
};


/* Extra per-device info for cdrom drives. */
struct cdrom_info {

  /* Buffer for table of contents.  NULL if we haven't allocated
     a TOC buffer for this device yet. */

  struct atapi_toc *toc;

  /* Sector buffer.  If a read request wants only the first part of a cdrom
     block, we cache the rest of the block here, in the expectation that that
     data is going to be wanted soon.  SECTOR_BUFFERED is the number of the
     first buffered sector, and NSECTORS_BUFFERED is the number of sectors
     in the buffer.  Before the buffer is allocated, we should have
     SECTOR_BUFFER == NULL and NSECTORS_BUFFERED == 0. */

  unsigned long sector_buffered;
  unsigned long nsectors_buffered;
  char *sector_buffer;

  /* The result of the last successful request sense command
     on this device. */
  struct atapi_request_sense sense_data;
};

#endif /* CONFIG_BLK_DEV_IDECD */

/*
 * Now for the data we need to maintain per-drive:  ide_drive_t
 */

typedef enum {ide_disk, ide_cdrom, ide_tape} ide_media_t;

typedef union {
	unsigned all			: 8;	/* all of the bits together */
	struct {
		unsigned set_geometry	: 1;	/* respecify drive geometry */
		unsigned recalibrate	: 1;	/* seek to cyl 0      */
		unsigned set_multmode	: 1;	/* set multmode count */
		unsigned set_pio	: 1;	/* set pio mode */
		unsigned reserved	: 4;	/* unused */
		} b;
	} special_t;

typedef union {
	unsigned all			: 8;	/* all of the bits together */
	struct {
		unsigned head		: 4;	/* always zeros here */
		unsigned unit		: 1;	/* drive select number, 0 or 1 */
		unsigned bit5		: 1;	/* always 1 */
		unsigned lba		: 1;	/* using LBA instead of CHS */
		unsigned bit7		: 1;	/* always 1 */
	} b;
	} select_t;

typedef struct ide_drive_s {
	special_t	special;	/* special action flags */
	unsigned present	: 1;	/* drive is physically present */
	unsigned noprobe 	: 1;	/* from:  hdx=noprobe */
	unsigned keep_settings  : 1;	/* restore settings after drive reset */
	unsigned busy		: 1;	/* currently doing revalidate_disk() */
	unsigned removeable	: 1;	/* 1 if need to do check_media_change */
	unsigned using_dma	: 1;	/* disk is using dma for read/write */
	unsigned forced_geom	: 1;	/* 1 if hdx=c,h,s was given at boot */
	unsigned unmask		: 1;	/* flag: okay to unmask other irqs */
	unsigned autotune	: 2;	/* 1=autotune, 2=noautotune, 0=default */
#if FAKE_FDISK_FOR_EZDRIVE
	unsigned remap_0_to_1	: 1;	/* flag: partitioned with ezdrive */
#endif /* FAKE_FDISK_FOR_EZDRIVE */
	ide_media_t	media;		/* disk, cdrom, tape */
	select_t	select;		/* basic drive/head select reg value */
	byte		ctl;		/* "normal" value for IDE_CONTROL_REG */
	byte		ready_stat;	/* min status value for drive ready */
	byte		mult_count;	/* current multiple sector setting */
	byte 		mult_req;	/* requested multiple sector setting */
	byte 		pio_req;	/* requested multiple sector setting */
	byte		io_32bit;	/* 0=16-bit, 1=32-bit, 2/3=32bit+sync */
	byte		bad_wstat;	/* used for ignoring WRERR_STAT */
	byte		sect0;		/* offset of first sector for DM6:DDO */
	byte 		usage;		/* current "open()" count for drive */
	byte 		head;		/* "real" number of heads */
	byte		sect;		/* "real" sectors per track */
	byte		bios_head;	/* BIOS/fdisk/LILO number of heads */
	byte		bios_sect;	/* BIOS/fdisk/LILO sectors per track */
	unsigned short	bios_cyl;	/* BIOS/fdisk/LILO number of cyls */
	unsigned short	cyl;		/* "real" number of cyls */
	void		  *hwif;	/* actually (ide_hwif_t *) */
	struct wait_queue *wqueue;	/* used to wait for drive in open() */
	struct hd_driveid *id;		/* drive model identification info */
	struct hd_struct  *part;	/* drive partition table */
	char		name[4];	/* drive name, such as "hda" */
#ifdef CONFIG_BLK_DEV_IDECD
	struct cdrom_info cdrom_info;	/* for ide-cd.c */
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
	idetape_tape_t	tape;		/* for ide-tape.c */
#endif /* CONFIG_BLK_DEV_IDETAPE */

	} ide_drive_t;

/*
 * An ide_dmaproc_t() initiates/aborts DMA read/write operations on a drive.
 *
 * The caller is assumed to have selected the drive and programmed the drive's
 * sector address using CHS or LBA.  All that remains is to prepare for DMA
 * and then issue the actual read/write DMA/PIO command to the drive.
 *
 * Returns 0 if all went well.
 * Returns 1 if DMA read/write could not be started, in which case the caller
 * should either try again later, or revert to PIO for the current request.
 */
typedef enum {	ide_dma_read = 0,	ide_dma_write = 1,
		ide_dma_abort = 2,	ide_dma_check = 3,
		ide_dma_status_bad = 4,	ide_dma_transferred = 5,
		ide_dma_begin = 6 }
	ide_dma_action_t;

typedef int (ide_dmaproc_t)(ide_dma_action_t, ide_drive_t *);


/*
 * An ide_tuneproc_t() is used to set the speed of an IDE interface
 * to a particular PIO mode.  The "byte" parameter is used
 * to select the PIO mode by number (0,1,2,3,4,5), and a value of 255
 * indicates that the interface driver should "auto-tune" the PIO mode
 * according to the drive capabilities in drive->id;
 *
 * Not all interface types support tuning, and not all of those
 * support all possible PIO settings.  They may silently ignore
 * or round values as they see fit.
 */
typedef void (ide_tuneproc_t)(ide_drive_t *, byte);

/*
 * This is used to provide HT6560B interface support.
 * It will probably also be used by the DC4030VL driver.
 */
typedef void (ide_selectproc_t) (ide_drive_t *);

/*
 * hwif_chipset_t is used to keep track of the specific hardware
 * chipset used by each IDE interface, if known.
 */
typedef enum {	ide_unknown,	ide_generic,	ide_triton,
		ide_cmd640,	ide_dtc2278,	ide_ali14xx,
		ide_qd6580,	ide_umc8672,	ide_ht6560b }
	hwif_chipset_t;

typedef struct hwif_s {
	struct hwif_s	*next;		/* for linked-list in ide_hwgroup_t */
	void		*hwgroup;	/* actually (ide_hwgroup_t *) */
	unsigned short	io_base;	/* base io port addr */
	unsigned short	ctl_port;	/* usually io_base+0x206 */
	ide_drive_t	drives[MAX_DRIVES];	/* drive info */
	struct gendisk	*gd;		/* gendisk structure */
	ide_tuneproc_t	*tuneproc;	/* routine to tune PIO mode for drives */
#ifdef CONFIG_BLK_DEV_HT6560B
	ide_selectproc_t *selectproc;	/* tweaks hardware to select drive */
#endif /* CONFIG_BLK_DEV_HT6560B */
	ide_dmaproc_t	*dmaproc;	/* dma read/write/abort routine */
	unsigned long	*dmatable;	/* dma physical region descriptor table */
	unsigned short	dma_base;	/* base addr for dma ports (triton) */
	byte		irq;		/* our irq number */
	byte		major;		/* our major number */
	char 		name[5];	/* name of interface, eg. "ide0" */
	byte		index;		/* 0 for ide0; 1 for ide1; ... */
	hwif_chipset_t	chipset;	/* sub-module for tuning.. */
	unsigned	noprobe : 1;	/* don't probe for this interface */
	unsigned	present : 1;	/* this interface exists */
	unsigned	serialized : 1;	/* valid only for ide_hwifs[0] */
	unsigned	no_unmask : 1;	/* disallow setting unmask bits */
#if (DISK_RECOVERY_TIME > 0)
	unsigned long	last_time;	/* time when previous rq was done */
#endif
#ifdef CONFIG_BLK_DEV_IDECD
	struct request request_sense_request;	/* from ide-cd.c */
	struct packet_command request_sense_pc;	/* from ide-cd.c */
#endif /* CONFIG_BLK_DEV_IDECD */
#ifdef CONFIG_BLK_DEV_IDETAPE
	ide_drive_t	*tape_drive;	/* Pointer to the tape on this interface */
#endif /* CONFIG_BLK_DEV_IDETAPE */
	} ide_hwif_t;

/*
 *  internal ide interrupt handler type
 */
typedef void (ide_handler_t)(ide_drive_t *);

typedef struct hwgroup_s {
	ide_handler_t		*handler;/* irq handler, if active */
	ide_drive_t		*drive;	/* current drive */
	ide_hwif_t		*hwif;	/* ptr to current hwif in linked-list */
	struct request		*rq;	/* current request */
	struct timer_list	timer;	/* failsafe timer */
	struct request		wrq;	/* local copy of current write rq */
	unsigned long		poll_timeout;	/* timeout value during long polls */
	} ide_hwgroup_t;

/*
 * ide_hwifs[] is the master data structure used to keep track
 * of just about everything in ide.c.  Whenever possible, routines
 * should be using pointers to a drive (ide_drive_t *) or 
 * pointers to a hwif (ide_hwif_t *), rather than indexing this
 * structure directly (the allocation/layout may change!).
 */
#ifdef _IDE_C
	ide_hwif_t	ide_hwifs[MAX_HWIFS];	/* master data repository */
#else
extern	ide_hwif_t	ide_hwifs[];
#endif

/*
 * One final include file, which references some of the data/defns from above
 */
#define IDE_DRIVER	/* "parameter" for blk.h */
#include <linux/blk.h>

#if (DISK_RECOVERY_TIME > 0)
void ide_set_recovery_timer (ide_hwif_t *);
#define SET_RECOVERY_TIMER(drive) ide_set_recovery_timer (drive)
#else
#define SET_RECOVERY_TIMER(drive)
#endif

/*
 * This is used for (nearly) all data transfers from the IDE interface
 */
void ide_input_data (ide_drive_t *drive, void *buffer, unsigned int wcount);

/*
 * This is used for (nearly) all data transfers to the IDE interface
 */
void ide_output_data (ide_drive_t *drive, void *buffer, unsigned int wcount);

/*
 * This is used on exit from the driver, to designate the next irq handler
 * and also to start the safety timer.
 */
void ide_set_handler (ide_drive_t *drive, ide_handler_t *handler, unsigned int timeout);

/*
 * Error reporting, in human readable form (luxurious, but a memory hog).
 */
byte ide_dump_status (ide_drive_t *drive, const char *msg, byte stat);

/*
 * ide_error() takes action based on the error returned by the controller.
 * The calling function must return afterwards, to restart the request.
 */
void ide_error (ide_drive_t *drive, const char *msg, byte stat);

/*
 * ide_fixstring() cleans up and (optionally) byte-swaps a text string,
 * removing leading/trailing blanks and compressing internal blanks.
 * It is primarily used to tidy up the model name/number fields as
 * returned by the WIN_[P]IDENTIFY commands.
 */
void ide_fixstring (byte *s, const int bytecount, const int byteswap);

/*
 * This routine busy-waits for the drive status to be not "busy".
 * It then checks the status for all of the "good" bits and none
 * of the "bad" bits, and if all is okay it returns 0.  All other
 * cases return 1 after invoking ide_error() -- caller should return.
 *
 */
int ide_wait_stat (ide_drive_t *drive, byte good, byte bad, unsigned long timeout);

/*
 * This routine is called from the partition-table code in genhd.c
 * to "convert" a drive to a logical geometry with fewer than 1024 cyls.
 *
 * The second parameter, "xparm", determines exactly how the translation
 * will be handled:
 *		 0 = convert to CHS with fewer than 1024 cyls
 *			using the same method as Ontrack DiskManager.
 *		 1 = same as "0", plus offset everything by 63 sectors.
 *		-1 = similar to "0", plus redirect sector 0 to sector 1.
 *		>1 = convert to a CHS geometry with "xparm" heads.
 *
 * Returns 0 if the translation was not possible, if the device was not
 * an IDE disk drive, or if a geometry was "forced" on the commandline.
 * Returns 1 if the geometry translation was successful.
 */
int ide_xlate_1024 (kdev_t, int, const char *);

/*
 * Start a reset operation for an IDE interface.
 * The caller should return immediately after invoking this.
 */
void ide_do_reset (ide_drive_t *);

/*
 * This function is intended to be used prior to invoking ide_do_drive_cmd().
 */
void ide_init_drive_cmd (struct request *rq);

/*
 * "action" parameter type for ide_do_drive_cmd() below.
 */
typedef enum
	{ide_wait,	/* insert rq at end of list, and wait for it */
	 ide_next,	/* insert rq immediately after current request */
	 ide_preempt,	/* insert rq in front of current request */
	 ide_end}	/* insert rq at end of list, but don't wait for it */
 ide_action_t;

/*
 * This function issues a special IDE device request
 * onto the request queue.
 *
 * If action is ide_wait, then then rq is queued at the end of
 * the request queue, and the function sleeps until it has been
 * processed.  This is for use when invoked from an ioctl handler.
 *
 * If action is ide_preempt, then the rq is queued at the head of
 * the request queue, displacing the currently-being-processed
 * request and this function returns immediately without waiting
 * for the new rq to be completed.  This is VERY DANGEROUS, and is
 * intended for careful use by the ATAPI tape/cdrom driver code.
 *
 * If action is ide_next, then the rq is queued immediately after
 * the currently-being-processed-request (if any), and the function
 * returns without waiting for the new rq to be completed.  As above,
 * This is VERY DANGEROUS, and is intended for careful use by the 
 * ATAPI tape/cdrom driver code.
 *
 * If action is ide_end, then the rq is queued at the end of the
 * request queue, and the function returns immediately without waiting
 * for the new rq to be completed. This is again intended for careful
 * use by the ATAPI tape/cdrom driver code. (Currently used by ide-tape.c,
 * when operating in the pipelined operation mode).
 */
int ide_do_drive_cmd (ide_drive_t *drive, struct request *rq, ide_action_t action);
 
/*
 * Clean up after success/failure of an explicit drive cmd.
 * stat/err are used only when (HWGROUP(drive)->rq->cmd == IDE_DRIVE_CMD).
 */
void ide_end_drive_cmd (ide_drive_t *drive, byte stat, byte err);

#ifdef CONFIG_BLK_DEV_IDECD
/*
 * These are routines in ide-cd.c invoked from ide.c
 */
void ide_do_rw_cdrom (ide_drive_t *, unsigned long);
int ide_cdrom_ioctl (ide_drive_t *, struct inode *, struct file *, unsigned int, unsigned long);
int ide_cdrom_check_media_change (ide_drive_t *);
int ide_cdrom_open (struct inode *, struct file *, ide_drive_t *);
void ide_cdrom_release (struct inode *, struct file *, ide_drive_t *);
void ide_cdrom_setup (ide_drive_t *);
#endif /* CONFIG_BLK_DEV_IDECD */

#ifdef CONFIG_BLK_DEV_IDETAPE

/*
 *	Functions in ide-tape.c which are invoked from ide.c:
 */

/*
 *	idetape_identify_device is called during device probing stage to
 *	probe for an ide atapi tape drive and to initialize global variables
 *	in ide-tape.c which provide the link between the character device
 *	and the correspoding block device.
 *
 *	Returns 1 if an ide tape was detected and is supported.
 *	Returns 0 otherwise.
 */
 
int idetape_identify_device (ide_drive_t *drive,struct hd_driveid *id);

/*
 *	idetape_setup is called a bit later than idetape_identify_device,
 *	during the search for disk partitions, to initialize various tape
 *	state variables in ide_drive_t *drive.
 */
 
void idetape_setup (ide_drive_t *drive);

/*
 *	idetape_do_request is our request function. It is called by ide.c
 *	to process a new request.
 */

void idetape_do_request (ide_drive_t *drive, struct request *rq, unsigned long block);

/*
 *	idetape_end_request is used to finish servicing a request, and to
 *	insert a pending pipeline request into the main device queue.
 */
 
void idetape_end_request (byte uptodate, ide_hwgroup_t *hwgroup);

/*
 *	Block device interface functions.
 */
  
int idetape_blkdev_ioctl (ide_drive_t *drive, struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg);
int idetape_blkdev_open (struct inode *inode, struct file *filp, ide_drive_t *drive);
void idetape_blkdev_release (struct inode *inode, struct file *filp, ide_drive_t *drive);

/*
 *	idetape_register_chrdev initializes the character device interface to
 *	the ide tape drive.
 */
 
void idetape_register_chrdev (void);

#endif /* CONFIG_BLK_DEV_IDETAPE */

#ifdef CONFIG_BLK_DEV_TRITON
void ide_init_triton (byte, byte);
#endif /* CONFIG_BLK_DEV_TRITON */
