/* AM53/79C974 (PCscsi) driver release 0.5
 *
 * The architecture and much of the code of this device
 * driver was originally developed by Drew Eckhardt for
 * the NCR5380. The following copyrights apply:
 *  For the architecture and all parts similar to the NCR5380:
 *    Copyright 1993, Drew Eckhardt
 *	Visionary Computing 
 *	(Unix and Linux consulting and custom programming)
 * 	drew@colorado.edu
 *	+1 (303) 666-5836
 *
 *  The AM53C974_nobios_detect code was origininally developed by
 *   Robin Cutshaw (robin@xfree86.org) and is used here in a 
 *   modified form.
 *
 *  For the other parts:
 *    Copyright 1994, D. Frieauff
 *    EMail: fri@rsx42sun0.dofn.de
 *    Phone: x49-7545-8-2256 , x49-7541-42305
 */

/*
 * $Log: AM53C974.h,v $
 * Revision 1.1  1996/03/25  20:25:06  goel
 * Linux driver merge.
 *
 */

#ifndef AM53C974_H
#define AM53C974_H

#include <linux/scsicam.h>

/***************************************************************************************
* Default setting of the controller's SCSI id. Edit and uncomment this only if your    *
* BIOS does not correctly initialize the controller's SCSI id.                         *
* If you don't get a warning during boot, it is correctly initialized.                 *
****************************************************************************************/
/* #define AM53C974_SCSI_ID 7 */

/***************************************************************************************
* Default settings for sync. negotiation enable, transfer rate and sync. offset.       *
* These settings can be replaced by LILO overrides (append) with the following syntax:          *
* AM53C974=host-scsi-id, target-scsi-id, max-rate, max-offset                          *
* Sync. negotiation is disabled by default and will be enabled for those targets which *
* are specified in the LILO override                                                   *
****************************************************************************************/
#define DEFAULT_SYNC_NEGOTIATION_ENABLED 0 /* 0 or 1 */
#define DEFAULT_RATE			 5 /* MHz, min: 3; max: 10 */
#define DEFAULT_SYNC_OFFSET		 0 /* bytes, min: 0; max: 15; use 0 for async. mode */


/* --------------------- don't edit below here  --------------------- */

#define AM53C974_DRIVER_REVISION_MAJOR 0
#define AM53C974_DRIVER_REVISION_MINOR 5
#define SEPARATOR_LINE  \
"--------------------------------------------------------------------------\n"

/* debug control */
/* #define AM53C974_DEBUG */
/* #define AM53C974_DEBUG_MSG */
/* #define AM53C974_DEBUG_KEYWAIT */
/* #define AM53C974_DEBUG_INIT */
/* #define AM53C974_DEBUG_QUEUE */
/* #define AM53C974_DEBUG_INFO */
/* #define AM53C974_DEBUG_LINKED */
/* #define VERBOSE_AM53C974_DEBUG */
/* #define AM53C974_DEBUG_INTR */
/* #define AM53C974_DEB_RESEL */
#define AM53C974_DEBUG_ABORT
/* #define AM53C974_OPTION_DEBUG_PROBE_ONLY */

/* special options/constants */
#define DEF_CLK                 40   /* chip clock freq. in MHz */
#define MIN_PERIOD               4   /* for negotiation: min. number of clocks per cycle */
#define MAX_PERIOD              13   /* for negotiation: max. number of clocks per cycle */
#define MAX_OFFSET              15   /* for negotiation: max. offset (0=async) */

#define DEF_SCSI_TIMEOUT        245  /* STIMREG value, 40 Mhz */
#define DEF_STP                 8    /* STPREG value assuming 5.0 MB/sec, FASTCLK, FASTSCSI */
#define DEF_SOF_RAD             0    /* REQ/ACK deassertion delay */
#define DEF_SOF_RAA             0    /* REQ/ACK assertion delay */
#define DEF_ETM                 0    /* CNTLREG1, ext. timing mode */
#define DEF_PERE                1    /* CNTLREG1, parity error reporting */
#define DEF_CLKF                0    /* CLKFREG,  0=40 Mhz */
#define DEF_ENF                 1    /* CNTLREG2, enable features */
#define DEF_ADIDCHK             0    /* CNTLREG3, additional ID check */
#define DEF_FASTSCSI            1    /* CNTLREG3, fast SCSI */
#define DEF_FASTCLK             1    /* CNTLREG3, fast clocking, 5 MB/sec at 40MHz chip clk */
#define DEF_GLITCH              1    /* CNTLREG4, glitch eater, 0=12ns, 1=35ns, 2=25ns, 3=off */
#define DEF_PWD                 0    /* CNTLREG4, reduced power feature */
#define DEF_RAE                 0    /* CNTLREG4, RAE active negation on REQ, ACK only */
#define DEF_RADE                1    /* 1CNTLREG4, active negation on REQ, ACK and data */

/*** PCI block ***/
/* standard registers are defined in <linux/pci.h> */
#ifndef PCI_VENDOR_ID_AMD
#define PCI_VENDOR_ID_AMD	0x1022
#define PCI_DEVICE_ID_AMD_SCSI  0x2020
#endif
#define PCI_BASE_MASK           0xFFFFFFE0
#define PCI_COMMAND_PERREN      0x40
#define PCI_SCRATCH_REG_0	0x40	/* 16 bits */
#define PCI_SCRATCH_REG_1	0x42	/* 16 bits */
#define PCI_SCRATCH_REG_2	0x44	/* 16 bits */
#define PCI_SCRATCH_REG_3	0x46	/* 16 bits */
#define PCI_SCRATCH_REG_4	0x48	/* 16 bits */
#define PCI_SCRATCH_REG_5	0x4A	/* 16 bits */
#define PCI_SCRATCH_REG_6	0x4C	/* 16 bits */
#define PCI_SCRATCH_REG_7	0x4E	/* 16 bits */

/*** SCSI block ***/
#define CTCLREG		    	0x00	/* r	current transf. count, low byte    */
#define CTCMREG		   	0x04	/* r 	current transf. count, middle byte */
#define CTCHREG		    	0x38	/* r	current transf. count, high byte   */
#define STCLREG		    	0x00	/* w	start transf. count, low byte      */
#define STCMREG		    	0x04	/* w	start transf. count, middle byte   */
#define STCHREG		    	0x38	/* w 	start transf. count, high byte     */
#define FFREG		    	0x08	/* rw	SCSI FIFO reg.			   */
#define STIMREG		    	0x14	/* w	SCSI timeout reg.		   */

#define SDIDREG		    	0x10	/* w	SCSI destination ID reg.	   */
#define SDIREG_MASK		0x07	/* mask					   */

#define STPREG		    	0x18	/* w	synchronous transf. period reg.	   */
#define STPREG_STP		0x1F	/* synchr. transfer period		   */

#define CLKFREG		    	0x24	/* w	clock factor reg.		   */
#define CLKFREG_MASK		0x07	/* mask					   */

#define CMDREG		    	0x0C	/* rw	SCSI command reg.		   */
#define CMDREG_DMA         	0x80    /* set DMA mode (set together with opcodes below) */
#define CMDREG_IT          	0x10    /* information transfer 		   */
#define CMDREG_ICCS		0x11	/* initiator command complete steps 	   */
#define CMDREG_MA		0x12	/* message accepted 			   */
#define CMDREG_TPB		0x98	/* transfer pad bytes, DMA mode only 	   */
#define CMDREG_SATN		0x1A	/* set ATN 				   */
#define CMDREG_RATN		0x1B	/* reset ATN 				   */
#define CMDREG_SOAS		0x41	/* select without ATN steps 		   */
#define CMDREG_SAS		0x42	/* select with ATN steps (1 msg byte)	   */
#define CMDREG_SASS		0x43	/* select with ATN and stop steps 	   */
#define CMDREG_ESR		0x44	/* enable selection/reselection 	   */
#define CMDREG_DSR		0x45	/* disable selection/reselection 	   */
#define CMDREG_SA3S		0x46	/* select with ATN 3 steps  (3 msg bytes)  */
#define CMDREG_NOP		0x00	/* no operation 			   */
#define CMDREG_CFIFO		0x01	/* clear FIFO 				   */
#define CMDREG_RDEV		0x02	/* reset device 			   */
#define CMDREG_RBUS		0x03	/* reset SCSI bus 			   */

#define STATREG		    	0x10	/* r 	SCSI status reg.		   */
#define STATREG_INT		0x80	/* SCSI interrupt condition detected	   */
#define STATREG_IOE		0x40	/* SCSI illegal operation error detected   */
#define STATREG_PE		0x20	/* SCSI parity error detected		   */
#define STATREG_CTZ		0x10	/* CTC reg decremented to zero		   */
#define STATREG_MSG		0x04	/* SCSI MSG phase (latched?)		   */
#define STATREG_CD		0x02	/* SCSI C/D phase (latched?)		   */
#define STATREG_IO		0x01	/* SCSI I/O phase (latched?)		   */
#define STATREG_PHASE           0x07    /* SCSI phase mask 			   */

#define INSTREG		    	0x14	/* r	interrupt status reg.		   */
#define INSTREG_SRST		0x80	/* SCSI reset detected			   */
#define INSTREG_ICMD		0x40	/* SCSI invalid command detected	   */
#define INSTREG_DIS		0x20	/* target disconnected or sel/resel timeout*/
#define INSTREG_SR		0x10	/* device on bus has service request       */
#define INSTREG_SO		0x08	/* successful operation			   */
#define INSTREG_RESEL		0x04	/* device reselected as initiator	   */

#define ISREG		    	0x18	/* r	internal state reg.		   */
#define ISREG_SOF		0x08	/* synchronous offset flag (act. low)	   */
#define ISREG_IS		0x07	/* status of intermediate op.		   */
#define ISREG_OK_NO_STOP        0x04    /* selection successful                    */
#define ISREG_OK_STOP           0x01    /* selection successful                    */

#define CFIREG		    	0x1C	/* r	current FIFO/internal state reg.   */
#define CFIREG_IS		0xE0	/* status of intermediate op.		   */
#define CFIREG_CF		0x1F	/* number of bytes in SCSI FIFO		   */

#define SOFREG		    	0x1C	/* w	synchr. offset reg.		   */
#define SOFREG_RAD		0xC0	/* REQ/ACK deassertion delay (sync.)	   */
#define SOFREG_RAA		0x30	/* REQ/ACK assertion delay (sync.)	   */
#define SOFREG_SO		0x0F	/* synch. offset (sync.)		   */

#define CNTLREG1	    	0x20	/* rw	control register one		   */
#define CNTLREG1_ETM		0x80	/* set extended timing mode		   */
#define CNTLREG1_DISR		0x40	/* disable interrupt on SCSI reset	   */
#define CNTLREG1_PERE		0x10	/* enable parity error reporting	   */
#define CNTLREG1_SID		0x07	/* host adapter SCSI ID			   */

#define CNTLREG2	    	0x2C	/* rw	control register two		   */
#define CNTLREG2_ENF		0x40	/* enable features			   */

#define CNTLREG3	    	0x30	/* rw	control register three		   */ 
#define CNTLREG3_ADIDCHK	0x80	/* additional ID check			   */
#define CNTLREG3_FASTSCSI	0x10	/* fast SCSI				   */
#define CNTLREG3_FASTCLK	0x08	/* fast SCSI clocking			   */

#define CNTLREG4	    	0x34	/* rw	control register four		   */ 
#define CNTLREG4_GLITCH		0xC0	/* glitch eater				   */
#define CNTLREG4_PWD		0x20	/* reduced power feature		   */
#define CNTLREG4_RAE		0x08	/* write only, active negot. ctrl.	   */
#define CNTLREG4_RADE		0x04	/* active negot. ctrl.			   */
#define CNTLREG4_RES		0x10	/* reserved bit, must be 1		   */

/*** DMA block ***/
#define DMACMD		    	0x40	/* rw	command				   */
#define DMACMD_DIR		0x80	/* transfer direction (1=read from device) */
#define DMACMD_INTE_D		0x40	/* DMA transfer interrupt enable 	   */
#define DMACMD_INTE_P		0x20	/* page transfer interrupt enable 	   */
#define DMACMD_MDL		0x10	/* map to memory descriptor list 	   */
#define DMACMD_DIAG		0x04	/* diagnostics, set to 0		   */
#define DMACMD_IDLE 		0x00	/* idle cmd			 	   */
#define DMACMD_BLAST		0x01	/* flush FIFO to memory		 	   */
#define DMACMD_ABORT		0x02	/* terminate DMA		 	   */
#define DMACMD_START		0x03	/* start DMA			 	   */

#define DMASTATUS	      	0x54	/* r	status register			   */
#define DMASTATUS_BCMPLT	0x20	/* BLAST complete			   */
#define DMASTATUS_SCSIINT	0x10	/* SCSI interrupt pending		   */
#define DMASTATUS_DONE		0x08	/* DMA transfer terminated		   */
#define DMASTATUS_ABORT		0x04	/* DMA transfer aborted			   */
#define DMASTATUS_ERROR		0x02	/* DMA transfer error			   */
#define DMASTATUS_PWDN		0x02	/* power down indicator			   */

#define DMASTC		    	0x44	/* rw	starting transfer count		   */
#define DMASPA		    	0x48	/* rw	starting physical address	   */
#define DMAWBC		    	0x4C	/* r	working byte counter		   */
#define DMAWAC		    	0x50	/* r	working address counter		   */
#define DMASMDLA	    	0x58	/* rw	starting MDL address		   */
#define DMAWMAC		    	0x5C	/* r	working MDL counter		   */

/*** SCSI phases ***/
#define PHASE_MSGIN             0x07
#define PHASE_MSGOUT            0x06
#define PHASE_RES_1             0x05
#define PHASE_RES_0             0x04
#define PHASE_STATIN            0x03
#define PHASE_CMDOUT            0x02
#define PHASE_DATAIN            0x01
#define PHASE_DATAOUT           0x00

struct AM53C974_hostdata {
    volatile unsigned       in_reset:1;          /* flag, says bus reset pending */
    volatile unsigned       aborted:1;           /* flag, says aborted */
    volatile unsigned       selecting:1;         /* selection started, but not yet finished */
    volatile unsigned       disconnecting: 1;    /* disconnection started, but not yet finished */
    volatile unsigned       dma_busy:1;          /* dma busy when service request for info transfer received */
    volatile unsigned  char msgout[10];          /* message to output in MSGOUT_PHASE */
    volatile unsigned  char last_message[10];	/* last message OUT */
    volatile Scsi_Cmnd      *issue_queue;	/* waiting to be issued */
    volatile Scsi_Cmnd      *disconnected_queue;	/* waiting for reconnect */
    volatile Scsi_Cmnd      *sel_cmd;            /* command for selection */
    volatile Scsi_Cmnd      *connected;		/* currently connected command */
    volatile unsigned  char busy[8];		/* index = target, bit = lun */
    unsigned  char sync_per[8];         /* synchronous transfer period (in effect) */
    unsigned  char sync_off[8];         /* synchronous offset (in effect) */
    unsigned  char sync_neg[8];         /* sync. negotiation performed (in effect) */
    unsigned  char sync_en[8];          /* sync. negotiation performed (in effect) */
    unsigned  char max_rate[8];         /* max. transfer rate (setup) */
    unsigned  char max_offset[8];       /* max. sync. offset (setup), only valid if corresponding sync_en is nonzero */
    };

#define AM53C974 { \
    NULL,              		/* pointer to next in list                      */  \
    NULL,			/* long * usage_count				*/  \
    NULL,                       /* struct proc_dir_entry *proc_dir              */ \
    NULL,                       /* int (*proc_info)(char *, char **, off_t, int, int, int); */ \
    "AM53C974",        		/* name                                         */  \
    AM53C974_detect,   		/* int (* detect)(struct SHT *)                 */  \
    NULL,              		/* int (*release)(struct Scsi_Host *)           */  \
    AM53C974_info,     		/* const char *(* info)(struct Scsi_Host *)     */  \
    AM53C974_command,  		/* int (* command)(Scsi_Cmnd *)                 */  \
    AM53C974_queue_command,	/* int (* queuecommand)(Scsi_Cmnd *,                \
                                           void (*done)(Scsi_Cmnd *))           */  \
    AM53C974_abort,    		/* int (* abort)(Scsi_Cmnd *)                   */  \
    AM53C974_reset,    		/* int (* reset)(Scsi_Cmnd *)                   */  \
    NULL,                 	/* int (* slave_attach)(int, int)               */  \
    scsicam_bios_param,		/* int (* bios_param)(Disk *, int, int[])       */  \
    12,                 	/* can_queue                                    */  \
    -1,                         /* this_id                                      */  \
    SG_ALL,            		/* sg_tablesize                                 */  \
    1,                 		/* cmd_per_lun                                  */  \
    0,                 		/* present, i.e. how many adapters of this kind */  \
    0,                 		/* unchecked_isa_dma                            */  \
    DISABLE_CLUSTERING 		/* use_clustering                               */  \
    }

void AM53C974_setup(char *str, int *ints);
int AM53C974_detect(Scsi_Host_Template *tpnt);
int AM53C974_biosparm(Disk *disk, int dev, int *info_array);
const char *AM53C974_info(struct Scsi_Host *);
int AM53C974_command(Scsi_Cmnd *SCpnt);
int AM53C974_queue_command(Scsi_Cmnd *cmd, void (*done)(Scsi_Cmnd *));
int AM53C974_abort(Scsi_Cmnd *cmd);
int AM53C974_reset (Scsi_Cmnd *cmd);

#define AM53C974_local_declare()	unsigned long io_port
#define AM53C974_setio(instance)	io_port = instance->io_port
#define AM53C974_read_8(addr)           inb(io_port + (addr))
#define AM53C974_write_8(addr,x)        outb((x), io_port + (addr))
#define AM53C974_read_16(addr)          inw(io_port + (addr))
#define AM53C974_write_16(addr,x)       outw((x), io_port + (addr))
#define AM53C974_read_32(addr)          inl(io_port + (addr))
#define AM53C974_write_32(addr,x)       outl((x), io_port + (addr))

#define AM53C974_poll_int()             { do { statreg = AM53C974_read_8(STATREG); } \
                                             while (!(statreg & STATREG_INT)) ; \
                                          AM53C974_read_8(INSTREG) ; } /* clear int */
#define AM53C974_cfifo()		(AM53C974_read_8(CFIREG) & CFIREG_CF)

/* These are "special" values for the tag parameter passed to AM53C974_select. */
#define TAG_NEXT	-1 	/* Use next free tag */
#define TAG_NONE	-2	/* Establish I_T_L nexus instead of I_T_L_Q
				 * even on SCSI-II devices */

/************ LILO overrides *************/
typedef struct _override_t {
    int host_scsi_id;			/* SCSI id of the bus controller */
    int target_scsi_id;                 /* SCSI id of target */
    int max_rate;			/* max. transfer rate */
    int max_offset;	                /* max. sync. offset, 0 = asynchronous */
    } override_t;

/************ PCI stuff *************/
#define AM53C974_PCIREG_OPEN()                    outb(0xF1, 0xCF8); outb(0, 0xCFA)
#define AM53C974_PCIREG_CLOSE()                   outb(0, 0xCF8)
#define AM53C974_PCIREG_READ_BYTE(instance,a)     ( inb((a) + (instance)->io_port) )
#define AM53C974_PCIREG_READ_WORD(instance,a)     ( inw((a) + (instance)->io_port) )
#define AM53C974_PCIREG_READ_DWORD(instance,a)    ( inl((a) + (instance)->io_port) )
#define AM53C974_PCIREG_WRITE_BYTE(instance,x,a)  ( outb((x), (a) + (instance)->io_port) )
#define AM53C974_PCIREG_WRITE_WORD(instance,x,a)  ( outw((x), (a) + (instance)->io_port) )
#define AM53C974_PCIREG_WRITE_DWORD(instance,x,a) ( outl((x), (a) + (instance)->io_port) )

typedef struct _pci_config_t {
    /* start of official PCI config space header */
    union {
        unsigned int device_vendor;
	struct {
	  unsigned short vendor;
	  unsigned short device;
 	  } dv;
        } dv_id;
#define _device_vendor dv_id.device_vendor
#define _vendor dv_id.dv.vendor
#define _device dv_id.dv.device
    union {
        unsigned int status_command;
	struct {
	  unsigned short command;
	  unsigned short status;
	  } sc;
        } stat_cmd;
#define _status_command stat_cmd.status_command
#define _command stat_cmd.sc.command
#define _status  stat_cmd.sc.status
    union {
        unsigned int class_revision;
	struct {
	    unsigned char rev_id;
	    unsigned char prog_if;
	    unsigned char sub_class;
	    unsigned char base_class;
	} cr;
    } class_rev;
#define _class_revision class_rev.class_revision
#define _rev_id     class_rev.cr.rev_id
#define _prog_if    class_rev.cr.prog_if
#define _sub_class  class_rev.cr.sub_class
#define _base_class class_rev.cr.base_class
    union {
        unsigned int bist_header_latency_cache;
	struct {
	    unsigned char cache_line_size;
	    unsigned char latency_timer;
	    unsigned char header_type;
	    unsigned char bist;
	} bhlc;
    } bhlc;
#define _bist_header_latency_cache bhlc.bist_header_latency_cache
#define _cache_line_size bhlc.bhlc.cache_line_size
#define _latency_timer   bhlc.bhlc.latency_timer
#define _header_type     bhlc.bhlc.header_type
#define _bist            bhlc.bhlc.bist
    unsigned int _base0;
    unsigned int _base1;
    unsigned int _base2;
    unsigned int _base3;
    unsigned int _base4;
    unsigned int _base5;
    unsigned int rsvd1;
    unsigned int rsvd2;
    unsigned int _baserom;
    unsigned int rsvd3;
    unsigned int rsvd4;
    union {
        unsigned int max_min_ipin_iline;
	struct {
	    unsigned char int_line;
	    unsigned char int_pin;
	    unsigned char min_gnt;
	    unsigned char max_lat;
	} mmii;
    } mmii;
#define _max_min_ipin_iline mmii.max_min_ipin_iline
#define _int_line mmii.mmii.int_line
#define _int_pin  mmii.mmii.int_pin
#define _min_gnt  mmii.mmii.min_gnt
#define _max_lat  mmii.mmii.max_lat
    /* end of official PCI config space header */
    unsigned short _ioaddr; /* config type 1 - private I/O addr    */
    unsigned int _pcibus;  /* config type 2 - private bus id      */
    unsigned int _cardnum; /* config type 2 - private card number */
} pci_config_t;

#endif /* AM53C974_H */
