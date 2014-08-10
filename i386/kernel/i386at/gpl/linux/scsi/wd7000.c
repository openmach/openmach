/* $Id: wd7000.c,v 1.1 1996/03/25 20:25:56 goel Exp $
 *  linux/drivers/scsi/wd7000.c
 *
 *  Copyright (C) 1992  Thomas Wuensche
 *	closely related to the aha1542 driver from Tommy Thorn
 *	( as close as different hardware allows on a lowlevel-driver :-) )
 *
 *  Revised (and renamed) by John Boyd <boyd@cis.ohio-state.edu> to
 *  accommodate Eric Youngdale's modifications to scsi.c.  Nov 1992.
 *
 *  Additional changes to support scatter/gather.  Dec. 1992.  tw/jb
 *
 *  No longer tries to reset SCSI bus at boot (it wasn't working anyway).
 *  Rewritten to support multiple host adapters.
 *  Miscellaneous cleanup.
 *  So far, still doesn't do reset or abort correctly, since I have no idea
 *  how to do them with this board (8^(.                      Jan 1994 jb
 *
 * This driver now supports both of the two standard configurations (per
 * the 3.36 Owner's Manual, my latest reference) by the same method as
 * before; namely, by looking for a BIOS signature.  Thus, the location of
 * the BIOS signature determines the board configuration.  Until I have
 * time to do something more flexible, users should stick to one of the
 * following:
 *
 * Standard configuration for single-adapter systems:
 *    - BIOS at CE00h
 *    - I/O base address 350h
 *    - IRQ level 15
 *    - DMA channel 6
 * Standard configuration for a second adapter in a system:
 *    - BIOS at C800h
 *    - I/O base address 330h
 *    - IRQ level 11
 *    - DMA channel 5
 *
 * Anyone who can recompile the kernel is welcome to add others as need
 * arises, but unpredictable results may occur if there are conflicts.
 * In any event, if there are multiple adapters in a system, they MUST
 * use different I/O bases, IRQ levels, and DMA channels, since they will be
 * indistinguishable (and in direct conflict) otherwise.
 *
 *   As a point of information, the NO_OP command toggles the CMD_RDY bit
 * of the status port, and this fact could be used as a test for the I/O
 * base address (or more generally, board detection).  There is an interrupt
 * status port, so IRQ probing could also be done.  I suppose the full
 * DMA diagnostic could be used to detect the DMA channel being used.  I
 * haven't done any of this, though, because I think there's too much of
 * a chance that such explorations could be destructive, if some other
 * board's resources are used inadvertently.  So, call me a wimp, but I
 * don't want to try it.  The only kind of exploration I trust is memory
 * exploration, since it's more certain that reading memory won't be
 * destructive.
 *
 * More to my liking would be a LILO boot command line specification, such
 * as is used by the aha152x driver (and possibly others).  I'll look into
 * it, as I have time...
 *
 *   I get mail occasionally from people who either are using or are
 * considering using a WD7000 with Linux.  There is a variety of
 * nomenclature describing WD7000's.  To the best of my knowledge, the
 * following is a brief summary (from an old WD doc - I don't work for
 * them or anything like that):
 *
 * WD7000-FASST2: This is a WD7000 board with the real-mode SST ROM BIOS
 *        installed.  Last I heard, the BIOS was actually done by Columbia
 *        Data Products.  The BIOS is only used by this driver (and thus
 *        by Linux) to identify the board; none of it can be executed under
 *        Linux.
 *
 * WD7000-ASC: This is the original adapter board, with or without BIOS.
 *        The board uses a WD33C93 or WD33C93A SBIC, which in turn is
 *        controlled by an onboard Z80 processor.  The board interface
 *        visible to the host CPU is defined effectively by the Z80's
 *        firmware, and it is this firmware's revision level that is
 *        determined and reported by this driver.  (The version of the
 *        on-board BIOS is of no interest whatsoever.)  The host CPU has
 *        no access to the SBIC; hence the fact that it is a WD33C93 is
 *        also of no interest to this driver.
 *
 * WD7000-AX:
 * WD7000-MX:
 * WD7000-EX: These are newer versions of the WD7000-ASC.  The -ASC is
 *        largely built from discrete components; these boards use more
 *        integration.  The -AX is an ISA bus board (like the -ASC),
 *        the -MX is an MCA (i.e., PS/2) bus board), and the -EX is an
 *        EISA bus board.
 *
 *  At the time of my documentation, the -?X boards were "future" products,
 *  and were not yet available.  However, I vaguely recall that Thomas
 *  Wuensche had an -AX, so I believe at least it is supported by this
 *  driver.  I have no personal knowledge of either -MX or -EX boards.
 *
 *  P.S. Just recently, I've discovered (directly from WD and Future
 *  Domain) that all but the WD7000-EX have been out of production for
 *  two years now.  FD has production rights to the 7000-EX, and are
 *  producing it under a new name, and with a new BIOS.  If anyone has
 *  one of the FD boards, it would be nice to come up with a signature
 *  for it.
 *                                                           J.B. Jan 1994.
 */

#ifdef MODULE
#include <linux/module.h>
#endif

#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/head.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <asm/system.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"

#define ANY2SCSI_INLINE    /* undef this to use old macros */
#undef DEBUG

#include "wd7000.h"

#include<linux/stat.h>

struct proc_dir_entry proc_scsi_wd7000 = {
    PROC_SCSI_7000FASST, 6, "wd7000",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};


/*
 *  Mailbox structure sizes.
 *  I prefer to keep the number of ICMBs much larger than the number of
 *  OGMBs.  OGMBs are used very quickly by the driver to start one or
 *  more commands, while ICMBs are used by the host adapter per command.
 */
#define OGMB_CNT	16
#define ICMB_CNT	32

/*
 *  Scb's are shared by all active adapters.  So, if they all become busy,
 *  callers may be made to wait in alloc_scbs for them to free.  That can
 *  be avoided by setting MAX_SCBS to NUM_CONFIG * WD7000_Q.  If you'd
 *  rather conserve memory, use a smaller number (> 0, of course) - things
 *  will should still work OK.
 */
#define MAX_SCBS        32

/*
 *  WD7000-specific mailbox structure
 *
 */
typedef volatile struct mailbox{
  unchar status;
  unchar scbptr[3];             /* SCSI-style - MSB first (big endian) */
} Mailbox;

/*
 *  This structure should contain all per-adapter global data.  I.e., any
 *  new global per-adapter data should put in here.
 *
 */
typedef struct adapter {
  struct Scsi_Host *sh;             /* Pointer to Scsi_Host structure */
  int iobase;                       /* This adapter's I/O base address */
  int irq;                          /* This adapter's IRQ level */
  int dma;                          /* This adapter's DMA channel */
  struct {                          /* This adapter's mailboxes */
    Mailbox ogmb[OGMB_CNT];             /* Outgoing mailboxes */
    Mailbox icmb[ICMB_CNT];             /* Incoming mailboxes */
  } mb;
  int next_ogmb;                    /* to reduce contention at mailboxes */
  unchar control;                   /* shadows CONTROL port value */
  unchar rev1, rev2;                /* filled in by wd7000_revision */
} Adapter;

/*
 * The following is set up by wd7000_detect, and used thereafter by
 * wd7000_intr_handle to map the irq level to the corresponding Adapter.
 * Note that if SA_INTERRUPT is not used, wd7000_intr_handle must be
 * changed to pick up the IRQ level correctly.
 */
Adapter *irq2host[16] = {NULL};  /* Possible IRQs are 0-15 */

/*
 *  Standard Adapter Configurations - used by wd7000_detect
 */
typedef struct {
  const void *bios;             /* (linear) base address for ROM BIOS */
  int iobase;                   /* I/O ports base address */
  int irq;                      /* IRQ level */
  int dma;                      /* DMA channel */
} Config;

static const Config configs[] = {
  {(void *) 0xce000, 0x350, 15, 6},  /* defaults for single adapter */
  {(void *) 0xc8000, 0x330, 11, 5},  /* defaults for second adapter */
  {(void *) 0xd8000, 0x350, 15, 6},  /* Arghhh.... who added this ? */
};
#define NUM_CONFIGS (sizeof(configs)/sizeof(Config))

/*
 *  The following list defines strings to look for in the BIOS that identify
 *  it as the WD7000-FASST2 SST BIOS.  I suspect that something should be
 *  added for the Future Domain version.
 */
typedef struct signature {
    const void *sig;           /* String to look for */
    unsigned    ofs;           /* offset from BIOS base address */
    unsigned    len;           /* length of string */
} Signature;

static const Signature signatures[] = {
  {"SSTBIOS",0x0000d,7}                  /* "SSTBIOS" @ offset 0x0000d */
};
#define NUM_SIGNATURES (sizeof(signatures)/sizeof(Signature))


/*
 *  I/O Port Offsets and Bit Definitions
 *  4 addresses are used.  Those not defined here are reserved.
 */
#define ASC_STAT        0       /* Status,  Read */
#define ASC_COMMAND     0       /* Command, Write */
#define ASC_INTR_STAT   1       /* Interrupt Status, Read */
#define ASC_INTR_ACK    1       /* Acknowledge, Write */
#define ASC_CONTROL     2       /* Control, Write */

/* ASC Status Port
 */
#define INT_IM		0x80		/* Interrupt Image Flag */
#define CMD_RDY		0x40		/* Command Port Ready */
#define CMD_REJ		0x20		/* Command Port Byte Rejected */
#define ASC_INIT        0x10		/* ASC Initialized Flag */
#define ASC_STATMASK    0xf0		/* The lower 4 Bytes are reserved */

/* COMMAND opcodes
 *
 *  Unfortunately, I have no idea how to properly use some of these commands,
 *  as the OEM manual does not make it clear.  I have not been able to use
 *  enable/disable unsolicited interrupts or the reset commands with any
 *  discernible effect whatsoever.  I think they may be related to certain
 *  ICB commands, but again, the OEM manual doesn't make that clear.
 */
#define NO_OP             0     /* NO-OP toggles CMD_RDY bit in ASC_STAT */
#define INITIALIZATION    1     /* initialization (10 bytes) */
#define DISABLE_UNS_INTR  2     /* disable unsolicited interrupts */
#define ENABLE_UNS_INTR   3     /* enable unsolicited interrupts */
#define INTR_ON_FREE_OGMB 4     /* interrupt on free OGMB */
#define SOFT_RESET        5     /* SCSI bus soft reset */
#define HARD_RESET_ACK    6     /* SCSI bus hard reset acknowledge */
#define START_OGMB        0x80  /* start command in OGMB (n) */
#define SCAN_OGMBS        0xc0  /* start multiple commands, signature (n) */
				/*    where (n) = lower 6 bits */
/* For INITIALIZATION:
 */
typedef struct initCmd {
  unchar op;                   /* command opcode (= 1) */
  unchar ID;                   /* Adapter's SCSI ID */
  unchar bus_on;               /* Bus on time, x 125ns (see below) */
  unchar bus_off;              /* Bus off time, ""         ""      */
  unchar rsvd;                 /* Reserved */
  unchar mailboxes[3];         /* Address of Mailboxes, MSB first  */
  unchar ogmbs;                /* Number of outgoing MBs, max 64, 0,1 = 1 */
  unchar icmbs;                /* Number of incoming MBs,   ""       ""   */
} InitCmd;

#define BUS_ON            64    /* x 125ns = 8000ns (BIOS default) */
#define BUS_OFF           15    /* x 125ns = 1875ns (BIOS default) */
 
/* Interrupt Status Port - also returns diagnostic codes at ASC reset
 *
 * if msb is zero, the lower bits are diagnostic status
 * Diagnostics:
 * 01	No diagnostic error occurred
 * 02	RAM failure
 * 03	FIFO R/W failed
 * 04   SBIC register read/write failed
 * 05   Initialization D-FF failed
 * 06   Host IRQ D-FF failed
 * 07   ROM checksum error
 * Interrupt status (bitwise):
 * 10NNNNNN   outgoing mailbox NNNNNN is free
 * 11NNNNNN   incoming mailbox NNNNNN needs service
 */
#define MB_INTR	 0xC0		/* Mailbox Service possible/required */
#define IMB_INTR 0x40		/* 1 Incoming / 0 Outgoing */
#define MB_MASK  0x3f           /* mask for mailbox number */

/* CONTROL port bits
 */
#define INT_EN		0x08	/* Interrupt Enable	*/
#define DMA_EN		0x04	/* DMA Enable		*/
#define SCSI_RES	0x02	/* SCSI Reset		*/
#define ASC_RES		0x01	/* ASC Reset		*/

/*
   Driver data structures:
   - mb and scbs are required for interfacing with the host adapter.
     An SCB has extra fields not visible to the adapter; mb's
     _cannot_ do this, since the adapter assumes they are contiguous in
     memory, 4 bytes each, with ICMBs following OGMBs, and uses this fact
     to access them.
   - An icb is for host-only (non-SCSI) commands.  ICBs are 16 bytes each;
     the additional bytes are used only by the driver.
   - For now, a pool of SCBs are kept in global storage by this driver,
     and are allocated and freed as needed.

  The 7000-FASST2 marks OGMBs empty as soon as it has _started_ a command,
  not when it has finished.  Since the SCB must be around for completion,
  problems arise when SCBs correspond to OGMBs, which may be reallocated
  earlier (or delayed unnecessarily until a command completes).
  Mailboxes are used as transient data structures, simply for
  carrying SCB addresses to/from the 7000-FASST2.

  Note also since SCBs are not "permanently" associated with mailboxes,
  there is no need to keep a global list of Scsi_Cmnd pointers indexed
  by OGMB.   Again, SCBs reference their Scsi_Cmnds directly, so mailbox
  indices need not be involved.
*/

/*
 *  WD7000-specific scatter/gather element structure
 */
typedef struct sgb {
    unchar len[3];
    unchar ptr[3];              /* Also SCSI-style - MSB first */
} Sgb;

typedef struct scb {		/* Command Control Block 5.4.1 */
  unchar op;			/* Command Control Block Operation Code */
  unchar idlun;			/* op=0,2:Target Id, op=1:Initiator Id */
				/* Outbound data transfer, length is checked*/
				/* Inbound data transfer, length is checked */
				/* Logical Unit Number */
  unchar cdb[12];		/* SCSI Command Block */
  volatile unchar status;       /* SCSI Return Status */
  volatile unchar vue;		/* Vendor Unique Error Code */
  unchar maxlen[3];		/* Maximum Data Transfer Length */
  unchar dataptr[3];		/* SCSI Data Block Pointer */
  unchar linkptr[3];		/* Next Command Link Pointer */
  unchar direc;			/* Transfer Direction */
  unchar reserved2[6];		/* SCSI Command Descriptor Block */
				/* end of hardware SCB */
  Scsi_Cmnd *SCpnt;             /* Scsi_Cmnd using this SCB */
  Sgb sgb[WD7000_SG];           /* Scatter/gather list for this SCB */
  Adapter *host;                /* host adapter */
  struct scb *next;             /* for lists of scbs */
} Scb;

/*
 *  This driver is written to allow host-only commands to be executed.
 *  These use a 16-byte block called an ICB.  The format is extended by the
 *  driver to 18 bytes, to support the status returned in the ICMB and
 *  an execution phase code.
 *
 *  There are other formats besides these; these are the ones I've tried
 *  to use.  Formats for some of the defined ICB opcodes are not defined
 *  (notably, get/set unsolicited interrupt status) in my copy of the OEM
 *  manual, and others are ambiguous/hard to follow.
 */
#define ICB_OP_MASK             0x80  /* distinguishes scbs from icbs */
#define ICB_OP_OPEN_RBUF        0x80  /* open receive buffer */
#define ICB_OP_RECV_CMD         0x81  /* receive command from initiator */
#define ICB_OP_RECV_DATA        0x82  /* receive data from initiator */
#define ICB_OP_RECV_SDATA       0x83  /* receive data with status from init. */
#define ICB_OP_SEND_DATA        0x84  /* send data with status to initiator */
#define ICB_OP_SEND_STAT        0x86  /* send command status to initiator */
			     /* 0x87 is reserved */
#define ICB_OP_READ_INIT        0x88  /* read initialization bytes */
#define ICB_OP_READ_ID          0x89  /* read adapter's SCSI ID */
#define ICB_OP_SET_UMASK        0x8A  /* set unsolicited interrupt mask */
#define ICB_OP_GET_UMASK        0x8B  /* read unsolicited interrupt mask */
#define ICB_OP_GET_REVISION     0x8C  /* read firmware revision level */
#define ICB_OP_DIAGNOSTICS      0x8D  /* execute diagnostics */
#define ICB_OP_SET_EPARMS       0x8E  /* set execution parameters */
#define ICB_OP_GET_EPARMS       0x8F  /* read execution parameters */

typedef struct icbRecvCmd {
  unchar op;
  unchar IDlun;                 /* Initiator SCSI ID/lun */
  unchar len[3];                /* command buffer length */
  unchar ptr[3];                /* command buffer address */
  unchar rsvd[7];               /* reserved */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbRecvCmd;

typedef struct icbSendStat {
  unchar op;
  unchar IDlun;                 /* Target SCSI ID/lun */
  unchar stat;                  /* (outgoing) completion status byte 1 */
  unchar rsvd[12];              /* reserved */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbSendStat;

typedef struct icbRevLvl {
  unchar op;
  volatile unchar primary;      /* primary revision level (returned) */
  volatile unchar secondary;    /* secondary revision level (returned) */
  unchar rsvd[12];              /* reserved */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbRevLvl;

typedef struct icbUnsMask {     /* I'm totally guessing here */
  unchar op;
  volatile unchar mask[14];     /* mask bits */
#ifdef 0
  unchar rsvd[12];              /* reserved */
#endif
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbUnsMask;

typedef struct icbDiag {
  unchar op;
  unchar type;                  /* diagnostics type code (0-3) */
  unchar len[3];                /* buffer length */
  unchar ptr[3];                /* buffer address */
  unchar rsvd[7];               /* reserved */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbDiag;

#define ICB_DIAG_POWERUP        0     /* Power-up diags only */
#define ICB_DIAG_WALKING        1     /* walking 1's pattern */
#define ICB_DIAG_DMA            2     /* DMA - system memory diags */
#define ICB_DIAG_FULL           3     /* do both 1 & 2 */

typedef struct icbParms {
  unchar op;
  unchar rsvd1;                 /* reserved */
  unchar len[3];                /* parms buffer length */
  unchar ptr[3];                /* parms buffer address */
  unchar idx[2];                /* index (MSB-LSB) */
  unchar rsvd2[5];              /* reserved */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbParms;

typedef struct icbAny {
  unchar op;
  unchar data[14];              /* format-specific data */
  volatile unchar vue;          /* vendor-unique error code */
  volatile unchar status;       /* returned (icmb) status */
  volatile unchar phase;        /* used by interrupt handler */
} IcbAny;

typedef union icb {
  unchar op;                    /* ICB opcode */
  IcbRecvCmd recv_cmd;          /* format for receive command */
  IcbSendStat send_stat;        /* format for send status */
  IcbRevLvl rev_lvl;            /* format for get revision level */
  IcbDiag diag;                 /* format for execute diagnostics */
  IcbParms eparms;              /* format for get/set exec parms */
  IcbAny icb;                   /* generic format */
  unchar data[18];
} Icb;


/*
 *  Driver SCB structure pool.
 *
 *  The SCBs declared here are shared by all host adapters; hence, this
 *  structure is not part of the Adapter structure.
 */
static Scb scbs[MAX_SCBS];
static Scb *scbfree = NULL;      /* free list */
static int freescbs = MAX_SCBS;  /* free list counter */

/*
 *  END of data/declarations - code follows.
 */


#ifdef ANY2SCSI_INLINE
/*
   Since they're used a lot, I've redone the following from the macros
   formerly in wd7000.h, hopefully to speed them up by getting rid of
   all the shifting (it may not matter; GCC might have done as well anyway).

   xany2scsi and xscsi2int were not being used, and are no longer defined.
   (They were simply 4-byte versions of these routines).
*/

typedef union {  /* let's cheat... */
  int i;
  unchar u[sizeof(int)];  /* the sizeof(int) makes it more portable */
} i_u;


static inline void any2scsi( unchar *scsi, int any )
{
    *scsi++ = ((i_u) any).u[2];
    *scsi++ = ((i_u) any).u[1];
    *scsi++ = ((i_u) any).u[0];
}


static inline int scsi2int( unchar *scsi )
{
    i_u result;

    result.i = 0;  /* clears unused bytes */
    *(result.u+2) = *scsi++;
    *(result.u+1) = *scsi++;
      *(result.u) = *scsi++;
    return result.i;
}
#else
/*
   These are the old ones - I've just moved them here...
*/
#undef any2scsi
#define any2scsi(up, p)			\
(up)[0] = (((unsigned long)(p)) >> 16);		\
(up)[1] = ((unsigned long)(p)) >> 8;		\
(up)[2] = ((unsigned long)(p));

#undef scsi2int
#define scsi2int(up) ( (((unsigned long)*(up)) << 16) + \
 (((unsigned long)(up)[1]) << 8) + ((unsigned long)(up)[2]) )
#endif

    
static inline void wd7000_enable_intr(Adapter *host)
{
    host->control |= INT_EN;
    outb(host->control, host->iobase+ASC_CONTROL);
}


static inline void wd7000_enable_dma(Adapter *host)
{
    host->control |= DMA_EN;
    outb(host->control,host->iobase+ASC_CONTROL);
    set_dma_mode(host->dma, DMA_MODE_CASCADE);
    enable_dma(host->dma);
}


#define WAITnexttimeout 200  /* 2 seconds */

#define WAIT(port, mask, allof, noneof)					\
 { register volatile unsigned WAITbits; 				\
   register unsigned long WAITtimeout = jiffies + WAITnexttimeout;	\
   while (1) {								\
     WAITbits = inb(port) & (mask);					\
     if ((WAITbits & (allof)) == (allof) && ((WAITbits & (noneof)) == 0)) \
       break;                                                         	\
     if (jiffies > WAITtimeout) goto fail;				\
   }									\
 }


static inline void delay( unsigned how_long )
{
     register unsigned long time = jiffies + how_long;

     while (jiffies < time);
}


static inline int command_out(Adapter *host, unchar *cmd, int len)
{
    WAIT(host->iobase+ASC_STAT,ASC_STATMASK,CMD_RDY,0);
    while (len--)  {
	do  {
	    outb(*cmd, host->iobase+ASC_COMMAND);
	    WAIT(host->iobase+ASC_STAT, ASC_STATMASK, CMD_RDY, 0);
	}  while (inb(host->iobase+ASC_STAT) & CMD_REJ);
	cmd++;
    }
    return 1;

fail:
    printk("wd7000 command_out: WAIT failed(%d)\n", len+1);
    return 0;
}


/*
 *  This version of alloc_scbs is in preparation for supporting multiple
 *  commands per lun and command chaining, by queueing pending commands.
 *  We will need to allocate Scbs in blocks since they will wait to be
 *  executed so there is the possibility of deadlock otherwise.
 *  Also, to keep larger requests from being starved by smaller requests,
 *  we limit access to this routine with an internal busy flag, so that
 *  the satisfiability of a request is not dependent on the size of the
 *  request.
 */
static inline Scb *alloc_scbs(int needed)
{
    register Scb *scb, *p;
    register unsigned long flags;
    register unsigned long timeout = jiffies + WAITnexttimeout;
    register unsigned long now;
    static int busy = 0;
    int i;

    if (needed <= 0)  return NULL;  /* sanity check */

    save_flags(flags);
    cli();
    while (busy)  { /* someone else is allocating */
	sti();	/* Yes this is really needed here */
	now = jiffies;  while (jiffies == now)  /* wait a jiffy */;
	cli();
    }
    busy = 1;          /* not busy now; it's our turn */

    while (freescbs < needed)  {
	timeout = jiffies + WAITnexttimeout;
	do {
	    sti();	/* Yes this is really needed here */
	    now = jiffies;   while (jiffies == now) /* wait a jiffy */;
	    cli();
	}  while (freescbs < needed && jiffies <= timeout);
	/*
	 *  If we get here with enough free Scbs, we can take them.
	 *  Otherwise, we timed out and didn't get enough.
	 */
	if (freescbs < needed)  {
	    busy = 0;
	    panic("wd7000: can't get enough free SCBs.\n");
	    restore_flags(flags);
	    return NULL;
	}
    }
    scb = scbfree;  freescbs -= needed;
    for (i = 0; i < needed; i++)  { p = scbfree;  scbfree = p->next; }
    p->next = NULL;
    
    busy = 0;   /* we're done */

    restore_flags(flags);

    return scb;
}


static inline void free_scb( Scb *scb )
{
    register unsigned long flags;

    save_flags(flags);
    cli();

    memset(scb, 0, sizeof(Scb));
    scb->next = scbfree;  scbfree = scb;
    freescbs++;

    restore_flags(flags);
}


static inline void init_scbs(void)
{
    int i;
    unsigned long flags;

    save_flags(flags);
    cli();

    scbfree = &(scbs[0]);
    memset(scbs, 0, sizeof(scbs));
    for (i = 0;  i < MAX_SCBS-1;  i++)  {
      scbs[i].next = &(scbs[i+1]);  scbs[i].SCpnt = NULL;
    }
    scbs[MAX_SCBS-1].next = NULL;
    scbs[MAX_SCBS-1].SCpnt = NULL;

    restore_flags(flags);
}    
    

static int mail_out( Adapter *host, Scb *scbptr )
/*
 *  Note: this can also be used for ICBs; just cast to the parm type.
 */
{
    register int i, ogmb;
    register unsigned long flags;
    unchar start_ogmb;
    Mailbox *ogmbs = host->mb.ogmb;
    int *next_ogmb = &(host->next_ogmb);
#ifdef DEBUG
    printk("wd7000 mail_out: %06x",(unsigned int) scbptr);
#endif
    /* We first look for a free outgoing mailbox */
    save_flags(flags);
    cli();
    ogmb = *next_ogmb;
    for (i = 0; i < OGMB_CNT; i++) {
	if (ogmbs[ogmb].status == 0)  {
#ifdef DEBUG
	    printk(" using OGMB %x",ogmb);
#endif
	    ogmbs[ogmb].status = 1;
	    any2scsi((unchar *) ogmbs[ogmb].scbptr, (int) scbptr);

	    *next_ogmb = (ogmb+1) % OGMB_CNT;
	    break;
	}  else
	    ogmb = (++ogmb) % OGMB_CNT;
    }
    restore_flags(flags);
#ifdef DEBUG
    printk(", scb is %x",(unsigned int) scbptr);
#endif
    if (i >= OGMB_CNT) {
	/*
	 *  Alternatively, we might issue the "interrupt on free OGMB",
	 *  and sleep, but it must be ensured that it isn't the init
	 *  task running.  Instead, this version assumes that the caller
	 *  will be persistent, and try again.  Since it's the adapter
	 *  that marks OGMB's free, waiting even with interrupts off
	 *  should work, since they are freed very quickly in most cases.
	 */
	#ifdef DEBUG
	printk(", no free OGMBs.\n");
#endif
	return 0;
    }

    wd7000_enable_intr(host); 

    start_ogmb = START_OGMB | ogmb;
    command_out( host, &start_ogmb, 1 );
#ifdef DEBUG
    printk(", awaiting interrupt.\n");
#endif
    return 1;
}


int make_code(unsigned hosterr, unsigned scsierr)
{   
#ifdef DEBUG
    int in_error = hosterr;
#endif

    switch ((hosterr>>8)&0xff){
	case 0:	/* Reserved */
		hosterr = DID_ERROR;
		break;
	case 1:	/* Command Complete, no errors */
		hosterr = DID_OK;
		break;
	case 2: /* Command complete, error logged in scb status (scsierr) */ 
		hosterr = DID_OK;
		break;
	case 4:	/* Command failed to complete - timeout */
		hosterr = DID_TIME_OUT;
		break;
	case 5:	/* Command terminated; Bus reset by external device */
		hosterr = DID_RESET;
		break;
	case 6:	/* Unexpected Command Received w/ host as target */
		hosterr = DID_BAD_TARGET;
		break;
	case 80: /* Unexpected Reselection */
	case 81: /* Unexpected Selection */
		hosterr = DID_BAD_INTR;
		break;
	case 82: /* Abort Command Message  */
		hosterr = DID_ABORT;
		break;
	case 83: /* SCSI Bus Software Reset */
	case 84: /* SCSI Bus Hardware Reset */
		hosterr = DID_RESET;
		break;
	default: /* Reserved */
		hosterr = DID_ERROR;
		break;
	}
#ifdef DEBUG
    if (scsierr||hosterr)
	printk("\nSCSI command error: SCSI %02x host %04x return %d",
	       scsierr,in_error,hosterr);
#endif
    return scsierr | (hosterr << 16);
}


static void wd7000_scsi_done(Scsi_Cmnd * SCpnt)
{
#ifdef DEBUG
    printk("wd7000_scsi_done: %06x\n",(unsigned int) SCpnt);
#endif
    SCpnt->SCp.phase = 0;
}


#define wd7000_intr_ack(host)  outb(0,host->iobase+ASC_INTR_ACK)

void wd7000_intr_handle(int irq, struct pt_regs * regs)
{
    register int flag, icmb, errstatus, icmb_status;
    register int host_error, scsi_error;
    register Scb *scb;             /* for SCSI commands */
    register IcbAny *icb;          /* for host commands */
    register Scsi_Cmnd *SCpnt;
    Adapter *host = irq2host[irq];  /* This MUST be set!!! */
    Mailbox *icmbs = host->mb.icmb;

#ifdef DEBUG
    printk("wd7000_intr_handle: irq = %d, host = %06x\n", irq, host);
#endif

    flag = inb(host->iobase+ASC_INTR_STAT);
#ifdef DEBUG
    printk("wd7000_intr_handle: intr stat = %02x\n",flag);
#endif

    if (!(inb(host->iobase+ASC_STAT) & INT_IM))  {
	/* NB: these are _very_ possible if IRQ 15 is being used, since
	   it's the "garbage collector" on the 2nd 8259 PIC.  Specifically,
	   any interrupt signal into the 8259 which can't be identified
	   comes out as 7 from the 8259, which is 15 to the host.  Thus, it
	   is a good thing the WD7000 has an interrupt status port, so we
	   can sort these out.  Otherwise, electrical noise and other such
	   problems would be indistinguishable from valid interrupts...
	*/
#ifdef DEBUG 
	printk("wd7000_intr_handle: phantom interrupt...\n");
#endif
	wd7000_intr_ack(host);
	return; 
    }

    if (flag & MB_INTR)  {
	/* The interrupt is for a mailbox */
	if (!(flag & IMB_INTR)) {
#ifdef DEBUG
	    printk("wd7000_intr_handle: free outgoing mailbox");
#endif
	    /*
	     * If sleep_on() and the "interrupt on free OGMB" command are
	     * used in mail_out(), wake_up() should correspondingly be called
	     * here.  For now, we don't need to do anything special.
	     */
	    wd7000_intr_ack(host);
	    return;
	}  else  {
	    /* The interrupt is for an incoming mailbox */
	    icmb = flag & MB_MASK;
	    icmb_status = icmbs[icmb].status;
	    if (icmb_status & 0x80)  {  /* unsolicited - result in ICMB */
#ifdef DEBUG
 		printk("wd7000_intr_handle: unsolicited interrupt %02xh\n",
		       icmb_status);
#endif
		wd7000_intr_ack(host);
		return;
	    }
	    scb = (struct scb *) scsi2int((unchar *)icmbs[icmb].scbptr);
	    icmbs[icmb].status = 0;
	    if (!(scb->op & ICB_OP_MASK))  {   /* an SCB is done */
		SCpnt = scb->SCpnt;
		if (--(SCpnt->SCp.phase) <= 0)  {  /* all scbs are done */
		    host_error = scb->vue | (icmb_status << 8);
		    scsi_error = scb->status;
		    errstatus = make_code(host_error,scsi_error);    
		    SCpnt->result = errstatus;

		    free_scb(scb);

		    SCpnt->scsi_done(SCpnt);
		}
	    }  else  {    /* an ICB is done */
		icb = (IcbAny *) scb;
		icb->status = icmb_status;
		icb->phase  = 0;
	    }
	}  /* incoming mailbox */
    }

    wd7000_intr_ack(host);
    return;
}


int wd7000_queuecommand(Scsi_Cmnd * SCpnt, void (*done)(Scsi_Cmnd *))
{
    register Scb *scb;
    register Sgb *sgb;
    register unchar *cdb = (unchar *) SCpnt->cmnd;
    register unchar idlun;
    register short cdblen;
    Adapter *host = (Adapter *) SCpnt->host->hostdata;

    cdblen = SCpnt->cmd_len;
    idlun = ((SCpnt->target << 5) & 0xe0) | (SCpnt->lun & 7);
    SCpnt->scsi_done = done;
    SCpnt->SCp.phase = 1;
    scb = alloc_scbs(1);
    scb->idlun = idlun;
    memcpy(scb->cdb, cdb, cdblen);
    scb->direc = 0x40;		/* Disable direction check */

    scb->SCpnt = SCpnt;         /* so we can find stuff later */
    SCpnt->host_scribble = (unchar *) scb;
    scb->host = host;

    if (SCpnt->use_sg)  {
	struct scatterlist *sg = (struct scatterlist *) SCpnt->request_buffer;
	unsigned i;

	if (SCpnt->host->sg_tablesize == SG_NONE)  {
	    panic("wd7000_queuecommand: scatter/gather not supported.\n");
	}
#ifdef DEBUG
 	printk("Using scatter/gather with %d elements.\n",SCpnt->use_sg);
#endif

	sgb = scb->sgb;
 	scb->op = 1;
 	any2scsi(scb->dataptr, (int) sgb);
 	any2scsi(scb->maxlen, SCpnt->use_sg * sizeof (Sgb) );

	for (i = 0;  i < SCpnt->use_sg;  i++)  {
 	    any2scsi(sgb[i].ptr, (int) sg[i].address);
 	    any2scsi(sgb[i].len, sg[i].length);
	}
    }  else  {
	scb->op = 0;
	any2scsi(scb->dataptr, (int) SCpnt->request_buffer);
	any2scsi(scb->maxlen, SCpnt->request_bufflen);
    }
    while (!mail_out(host, scb)) /* keep trying */;

    return 1;
}


int wd7000_command(Scsi_Cmnd *SCpnt)
{
    wd7000_queuecommand(SCpnt, wd7000_scsi_done);

    while (SCpnt->SCp.phase > 0) barrier();  /* phase counts scbs down to 0 */

    return SCpnt->result;
}


int wd7000_diagnostics( Adapter *host, int code )
{
    static IcbDiag icb = {ICB_OP_DIAGNOSTICS};
    static unchar buf[256];
    unsigned long timeout;

    icb.type = code;
    any2scsi(icb.len, sizeof(buf));
    any2scsi(icb.ptr, (int) &buf);
    icb.phase = 1;
    /*
     * This routine is only called at init, so there should be OGMBs
     * available.  I'm assuming so here.  If this is going to
     * fail, I can just let the timeout catch the failure.
     */
    mail_out(host, (struct scb *) &icb);
    timeout = jiffies + WAITnexttimeout;  /* wait up to 2 seconds */
    while (icb.phase && jiffies < timeout)
    	barrier(); /* wait for completion */

    if (icb.phase)  {
	printk("wd7000_diagnostics: timed out.\n");
	return 0;
    }
    if (make_code(icb.vue|(icb.status << 8),0))  {
	printk("wd7000_diagnostics: failed (%02x,%02x)\n",
	       icb.vue, icb.status);
	return 0;
    }

    return 1;
}


int wd7000_init( Adapter *host )
{
    InitCmd init_cmd = {
	INITIALIZATION, 7, BUS_ON, BUS_OFF, 0, {0,0,0}, OGMB_CNT, ICMB_CNT
    };
    int diag;

    /*
       Reset the adapter - only.  The SCSI bus was initialized at power-up,
       and we need to do this just so we control the mailboxes, etc.
    */
    outb(ASC_RES, host->iobase+ASC_CONTROL);
    delay(1);  /* reset pulse: this is 10ms, only need 25us */
    outb(0,host->iobase+ASC_CONTROL);
    host->control = 0;   /* this must always shadow ASC_CONTROL */
    WAIT(host->iobase+ASC_STAT, ASC_STATMASK, CMD_RDY, 0);

    if ((diag = inb(host->iobase+ASC_INTR_STAT)) != 1)  {
	printk("wd7000_init: ");
	switch (diag)  {
	case 2:
	  printk("RAM failure.\n");
	  break;
	case 3:
	  printk("FIFO R/W failed\n");
	  break;
	case 4:
	  printk("SBIC register R/W failed\n");
	  break;
	case 5:
	  printk("Initialization D-FF failed.\n");
	  break;
	case 6:
	  printk("Host IRQ D-FF failed.\n");
	  break;
	case 7:
	  printk("ROM checksum error.\n");
	  break;
	default:
	  printk("diagnostic code %02Xh received.\n", diag);
	  break;
	}
	return 0;
    }
    
    /* Clear mailboxes */
    memset(&(host->mb), 0, sizeof(host->mb));

    /* Execute init command */
    any2scsi((unchar *) &(init_cmd.mailboxes), (int) &(host->mb));
    if (!command_out(host, (unchar *) &init_cmd, sizeof(init_cmd)))  {
	printk("wd7000_init: adapter initialization failed.\n"); 
	return 0;
    }
    WAIT(host->iobase+ASC_STAT, ASC_STATMASK, ASC_INIT, 0);

    if (request_irq(host->irq, wd7000_intr_handle, SA_INTERRUPT, "wd7000")) {
	printk("wd7000_init: can't get IRQ %d.\n", host->irq);
	return 0;
    }
    if (request_dma(host->dma,"wd7000"))  {
	printk("wd7000_init: can't get DMA channel %d.\n", host->dma);
	free_irq(host->irq);
	return 0;
    }
    wd7000_enable_dma(host);
    wd7000_enable_intr(host);

    if (!wd7000_diagnostics(host,ICB_DIAG_FULL))  {
	free_dma(host->dma);
	free_irq(host->irq);
	return 0;
    }

    return 1;

  fail:
    printk("wd7000_init: WAIT timed out.\n"); 
    return 0;					/* 0 = not ok */
}


void wd7000_revision(Adapter *host)
{
    static IcbRevLvl icb = {ICB_OP_GET_REVISION};

    icb.phase = 1;
    /*
     * Like diagnostics, this is only done at init time, in fact, from
     * wd7000_detect, so there should be OGMBs available.  If it fails,
     * the only damage will be that the revision will show up as 0.0,
     * which in turn means that scatter/gather will be disabled.
     */
    mail_out(host, (struct scb *) &icb);
    while (icb.phase)
    	barrier(); /* wait for completion */
    host->rev1 = icb.primary;
    host->rev2 = icb.secondary;
}


int wd7000_detect(Scsi_Host_Template * tpnt)
/* 
 *  Returns the number of adapters this driver is supporting.
 *
 *  The source for hosts.c says to wait to call scsi_register until 100%
 *  sure about an adapter.  We need to do it a little sooner here; we
 *  need the storage set up by scsi_register before wd7000_init, and
 *  changing the location of an Adapter structure is more trouble than
 *  calling scsi_unregister.
 *
 */
{
    int i,j, present = 0;
    const Config *cfg;
    const Signature *sig;
    Adapter *host = NULL;
    struct Scsi_Host *sh;

    tpnt->proc_dir = &proc_scsi_wd7000;

    /* Set up SCB free list, which is shared by all adapters */
    init_scbs();

    cfg = configs;
    for (i = 0; i < NUM_CONFIGS; i++)  {
	sig = signatures;
	for (j = 0; j < NUM_SIGNATURES; j++)  {
	    if (!memcmp(cfg->bios+sig->ofs, sig->sig, sig->len))  {
		/* matched this one */
#ifdef DEBUG
		printk("WD-7000 SST BIOS detected at %04X: checking...\n",
		       (int) cfg->bios);
#endif
		/*
		 *  We won't explicitly test the configuration (in this
		 *  version); instead, we'll just see if it works to
		 *  setup the adapter; if it does, we'll use it.
		 */
		if (check_region(cfg->iobase, 4))  {  /* ports in use */
		    printk("IO %xh already in use.\n", host->iobase);
		    continue;
		}
		/*
		 *  We register here, to get a pointer to the extra space,
		 *  which we'll use as the Adapter structure (host) for
		 *  this adapter.  It is located just after the registered
		 *  Scsi_Host structure (sh), and is located by the empty
		 *  array hostdata.
		 */
		sh = scsi_register(tpnt, sizeof(Adapter) );
		host = (Adapter *) sh->hostdata;
#ifdef DEBUG
		printk("wd7000_detect: adapter allocated at %06x\n",
		       (int)host);
#endif
		memset( host, 0, sizeof(Adapter) );
		host->sh = sh;
		host->irq = cfg->irq;
		host->iobase = cfg->iobase;
		host->dma = cfg->dma;
		irq2host[host->irq] = host;

		if (!wd7000_init(host))  {  /* Initialization failed */
		    scsi_unregister (sh);
		    continue;
		}

		/*
		 *  OK from here - we'll use this adapter/configuration.
		 */
		wd7000_revision(host);   /* important for scatter/gather */

		printk("Western Digital WD-7000 (%d.%d) ",
		       host->rev1, host->rev2);
		printk("using IO %xh IRQ %d DMA %d.\n",
		       host->iobase, host->irq, host->dma);

		request_region(host->iobase, 4,"wd7000"); /* Register our ports */
		/*
		 *  For boards before rev 6.0, scatter/gather isn't supported.
		 */
		if (host->rev1 < 6)  sh->sg_tablesize = SG_NONE;

		present++;                      /* count it */
		break;                          /* don't try any more sigs */
	    }
	    sig++;  /* try next signature with this configuration */
	}
	cfg++;      /* try next configuration */
    }

    return present;
}


/*
 *  I have absolutely NO idea how to do an abort with the WD7000...
 */
int wd7000_abort(Scsi_Cmnd * SCpnt)
{
    Adapter *host = (Adapter *) SCpnt->host->hostdata;

    if (inb(host->iobase+ASC_STAT) & INT_IM)  {
	printk("wd7000_abort: lost interrupt\n");
	wd7000_intr_handle(host->irq, NULL);
	return SCSI_ABORT_SUCCESS;
    }

    return SCSI_ABORT_SNOOZE;
}


/*
 *  I also have no idea how to do a reset...
 */
int wd7000_reset(Scsi_Cmnd * SCpnt)
{
    return SCSI_RESET_PUNT;
}


/*
 *  This was borrowed directly from aha1542.c, but my disks are organized
 *  this way, so I think it will work OK.  Someone who is ambitious can
 *  borrow a newer or more complete version from another driver.
 */
int wd7000_biosparam(Disk * disk, kdev_t dev, int* ip)
{
  int size = disk->capacity;
  ip[0] = 64;
  ip[1] = 32;
  ip[2] = size >> 11;
/*  if (ip[2] >= 1024) ip[2] = 1024; */
  return 0;
}

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = WD7000;

#include "scsi_module.c"
#endif
