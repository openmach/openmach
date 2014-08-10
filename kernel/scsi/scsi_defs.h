/* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
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
 *	File: scsi_defs.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	9/90
 *
 *	Controller-independent definitions for the SCSI driver
 */

#ifndef	_SCSI_SCSI_DEFS_H_
#define	_SCSI_SCSI_DEFS_H_

#include <kern/queue.h>
#include <kern/lock.h>

#include <rz_labels.h>

#define	await(event)	sleep(event,0)
extern	void wakeup();

typedef	vm_offset_t	opaque_t;	/* should be elsewhere */

/*
 * Internal error codes, and return values
 * XXX use the mach/error.h scheme XXX
 */
typedef unsigned int		scsi_ret_t;

#define	SCSI_ERR_GRAVITY(x)	((unsigned)(x)&0xf0000000U)
#define	SCSI_ERR_GRAVE		0x80000000U
#define SCSI_ERR_BAD		0x40000000

#define	SCSI_ERR_CLASS(x)	((unsigned)(x)&0x0fffffffU)
#define	SCSI_ERR_STATUS		0x00000001
#define	SCSI_ERR_SENSE		0x00000002
#define SCSI_ERR_MSEL		0x00000004

extern	void	scsi_error(/* target_info_t *, unsigned, unsigned */);

#define	SCSI_RET_IN_PROGRESS	0x00
#define	SCSI_RET_SUCCESS	0x01
#define	SCSI_RET_RETRY		0x02
#define SCSI_RET_NEED_SENSE	0x04
#define SCSI_RET_ABORTED	0x08
#define	SCSI_RET_DEVICE_DOWN	0x10

/*
 * Device-specific information kept by driver
 */
#define MAX_SCSI_PARTS 32	/* maximum partitions on a disk;can be larger */
typedef struct {
	struct disklabel	l;  /* NOT accurate. partitions stored below */
	struct {
	    unsigned int	badblockno;
	    unsigned int	save_rec;
	    char		*save_addr;
	    int			save_count;
	    int			save_resid;
	    int			retry_count;
	} b;
#if 0 /* no longer used by partition code */
	int			labelsector;
	int			labeloffset;
#endif 0
	struct diskpart scsi_array[MAX_SCSI_PARTS]; /* new partition info */
} scsi_disk_info_t;

typedef struct {
	boolean_t	read_only;
	unsigned int	speed;
	unsigned int	density;
	unsigned int	maxreclen;
	boolean_t	fixed_size;
} scsi_tape_info_t;

typedef struct {
	char		req_pending;
	char		req_id;
	char		req_lun;
	char		req_cmd;
	unsigned int	req_len;
	/* more later */
} scsi_processor_info_t;

typedef struct {
	void		*result;
	boolean_t	result_available;
	int		result_size;
	struct red_list	*violates_standards;
} scsi_cdrom_info_t;

typedef struct {
#	define SCSI_MAX_COMM_TTYS	16
	struct tty	*tty[SCSI_MAX_COMM_TTYS];
	io_req_t	ior;
} scsi_comm_info_t;

/*
 * Device descriptor
 */

#define	SCSI_TARGET_NAME_LEN	8+16+4+8	/* our way to keep it */

typedef struct target_info {
	queue_chain_t	links;			/* to queue for bus */
	io_req_t	ior;			/* what we are doing */

	unsigned int	flags;
#define	TGT_DID_SYNCH		0x00000001	/* finished the synch neg */
#define	TGT_TRY_SYNCH		0x00000002	/* do the synch negotiation */
#define	TGT_FULLY_PROBED	0x00000004	/* can sleep to wait */
#define	TGT_ONLINE		0x00000008	/* did the once-only stuff */
#define	TGT_ALIVE		0x00000010
#define	TGT_BBR_ACTIVE		0x00000020	/* bad block replc in progress */
#define	TGT_DISCONNECTED	0x00000040	/* waiting for reconnect */
#define	TGT_WRITTEN_TO		0x00000080	/* tapes: needs a filemark on close */
#define	TGT_REWIND_ON_CLOSE	0x00000100	/* tapes: rewind */
#define	TGT_BIG			0x00000200	/* disks: > 1Gb, use long R/W */
#define	TGT_REMOVABLE_MEDIA	0x00000400	/* e.g. floppy, cd-rom,.. */
#define	TGT_READONLY		0x00000800	/* cd-rom, scanner, .. */
#define	TGT_OPTIONAL_CMD	0x00001000	/* optional cmd, ignore errors */
#define TGT_WRITE_LABEL		0x00002000	/* disks: enable overwriting of label */
#define	TGT_US			0x00004000	/* our desc, when target role */

#define	TGT_HW_SPECIFIC_BITS	0xffff0000U	/* see specific HBA */
	char		*hw_state;		/* opaque */
	char		*dma_ptr;
	char		*cmd_ptr;
	struct scsi_devsw_struct	*dev_ops;	/* circularity */
	struct target_info	*next_lun;	/* if multi-LUN */
	char		target_id;
	char		unit_no;
	unsigned char	sync_period;
	unsigned char	sync_offset;
	decl_simple_lock_data(,target_lock)
#ifdef	MACH_KERNEL
#else	/*MACH_KERNEL*/
	struct fdma	fdma;
#endif	/*MACH_KERNEL*/
	/*
	 * State info kept while waiting to seize bus, either for first
	 * selection or while in disconnected state
	 */
	struct {
	    struct script	*script;
	    int			(*handler)();
	    unsigned int	out_count;
	    unsigned int	in_count;
	    unsigned int	copy_count;	/* optional */
	    unsigned int	dma_offset;
	    unsigned char	identify;
	    unsigned char	cmd_count;
	    unsigned char	hba_dep[2];
	} transient_state;
	unsigned int	block_size;
	volatile char	done;
	unsigned char	cur_cmd;
	unsigned char	lun;
	char		masterno;
	char		tgt_name[SCSI_TARGET_NAME_LEN];
	union {
	    scsi_disk_info_t	disk;
	    scsi_tape_info_t	tape;
	    scsi_cdrom_info_t	cdrom;
	    scsi_processor_info_t	cpu;
	    scsi_comm_info_t	comm;
	} dev_info;
} target_info_t;


/*
 * Device-specific operations
 */
typedef struct scsi_devsw_struct {
	char		*(*driver_name)(boolean_t);	  /* my driver's name */
	void		(*optimize)(target_info_t *);	  /* tune up internal params */
	scsi_ret_t	(*open)(target_info_t *,io_req_t);/* open time ops */
	scsi_ret_t	(*close)(target_info_t *);	  /* close time ops */
	int		(*strategy)(io_req_t);	          /* sort/start routine */
	void		(*restart)(target_info_t *,
				   boolean_t);		  /* completion routine */
	io_return_t	(*get_status)(int, 
				      target_info_t *,
				      dev_flavor_t,
				      dev_status_t,
				      natural_t *);	  /* specialization */
	io_return_t	(*set_status)(int, 
				      target_info_t *,
				      dev_flavor_t,
				      dev_status_t,
				      natural_t);	  /* specialization */
} scsi_devsw_t;

#define SCSI_OPTIMIZE_NULL ((void (*)(target_info_t *)) 0)
#define SCSI_OPEN_NULL ((scsi_ret_t (*)(target_info_t *,io_req_t)) 0)
#define SCSI_CLOSE_NULL ((scsi_ret_t (*)(target_info_t *)) 0)

extern scsi_devsw_t	scsi_devsw[];

/*
 * HBA descriptor
 */

typedef struct {
	/* initiator (us) state */
	unsigned char	initiator_id;
	unsigned char	masterno;
	unsigned int	max_dma_data;
	char		*hw_state;		/* opaque */
	int		(*go)();
	void		(*watchdog)();
	boolean_t	(*probe)();
	/* per-target state */
	target_info_t		*target[8];
} scsi_softc_t;

extern scsi_softc_t	*scsi_softc[];
extern scsi_softc_t	*scsi_master_alloc(/* int unit */);
extern target_info_t	*scsi_slave_alloc(/* int unit, int slave, char *hw */);

#define	BGET(d,mid,id)	(d[mid] & (1 << id))		/* bitmap ops */
#define BSET(d,mid,id)	d[mid] |= (1 << id)
#define BCLR(d,mid,id)	d[mid] &= ~(1 << id)

extern unsigned char	scsi_no_synchronous_xfer[];	/* one bitmap per ctlr */
extern unsigned char	scsi_use_long_form[];		/* one bitmap per ctlr */
extern unsigned char	scsi_might_disconnect[];	/* one bitmap per ctlr */
extern unsigned char	scsi_should_disconnect[];	/* one bitmap per ctlr */
extern unsigned char	scsi_initiator_id[];		/* one id per ctlr */

extern boolean_t	scsi_exabyte_filemarks;
extern boolean_t	scsi_no_automatic_bbr;
extern int		scsi_bbr_retries;
extern int		scsi_watchdog_period;
extern int		scsi_delay_after_reset;
extern unsigned int	scsi_per_target_virtual;	/* 2.5 only */

extern int		scsi_debug;

/*
 * HBA-independent Watchdog
 */
typedef struct {

	unsigned short	reset_count;
	char		nactive;

	char		watchdog_state;

#define SCSI_WD_INACTIVE	0
#define	SCSI_WD_ACTIVE		1
#define SCSI_WD_EXPIRED		2

	int		(*reset)();

} watchdog_t;

extern void scsi_watchdog( watchdog_t* );

#endif	_SCSI_SCSI_DEFS_H_
