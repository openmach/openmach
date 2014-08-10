/* aha152x.c -- Adaptec AHA-152x driver
 * Author: Juergen E. Fischer, fischer@et-inf.fho-emden.de
 * Copyright 1993, 1994, 1995 Juergen E. Fischer
 *
 *
 * This driver is based on
 *   fdomain.c -- Future Domain TMC-16x0 driver
 * which is
 *   Copyright 1992, 1993 Rickard E. Faith (faith@cs.unc.edu)
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 *
 * $Id: aha152x.c,v 1.1 1996/03/25 20:25:17 goel Exp $
 *
 * $Log: aha152x.c,v $
 * Revision 1.1  1996/03/25  20:25:17  goel
 * Linux driver merge.
 *
 * Revision 1.14  1996/01/17  15:11:20  fischer
 * - fixed lockup in MESSAGE IN phase after reconnection
 *
 * Revision 1.13  1996/01/09  02:15:53  fischer
 * - some cleanups
 * - moved request_irq behind controller initialization
 *   (to avoid spurious interrupts)
 *
 * Revision 1.12  1995/12/16  12:26:07  fischer
 * - barrier()'s added
 * - configurable RESET delay added
 *
 * Revision 1.11  1995/12/06  21:18:35  fischer
 * - some minor updates
 *
 * Revision 1.10  1995/07/22  19:18:45  fischer
 * - support for 2 controllers
 * - started synchronous data transfers (not working yet)
 *
 * Revision 1.9  1995/03/18  09:20:24  root
 * - patches for PCMCIA and modules
 *
 * Revision 1.8  1995/01/21  22:07:19  root
 * - snarf_region => request_region
 * - aha152x_intr interface change
 *
 * Revision 1.7  1995/01/02  23:19:36  root
 * - updated COMMAND_SIZE to cmd_len
 * - changed sti() to restore_flags()
 * - fixed some #ifdef which generated warnings
 *
 * Revision 1.6  1994/11/24  20:35:27  root
 * - problem with odd number of bytes in fifo fixed
 *
 * Revision 1.5  1994/10/30  14:39:56  root
 * - abort code fixed
 * - debugging improved
 *
 * Revision 1.4  1994/09/12  11:33:01  root
 * - irqaction to request_irq
 * - abortion updated
 *
 * Revision 1.3  1994/08/04  13:53:05  root
 * - updates for mid-level-driver changes
 * - accept unexpected BUSFREE phase as error condition
 * - parity check now configurable
 *
 * Revision 1.2  1994/07/03  12:56:36  root
 * - cleaned up debugging code
 * - more tweaking on reset delays
 * - updated abort/reset code (pretty untested...)
 *
 * Revision 1.1  1994/05/28  21:18:49  root
 * - update for mid-level interface change (abort-reset)
 * - delays after resets adjusted for some slow devices
 *
 * Revision 1.0  1994/03/25  12:52:00  root
 * - Fixed "more data than expected" problem
 * - added new BIOS signatures
 *
 * Revision 0.102  1994/01/31  20:44:12  root
 * - minor changes in insw/outsw handling
 *
 * Revision 0.101  1993/12/13  01:16:27  root
 * - fixed STATUS phase (non-GOOD stati were dropped sometimes;
 *   fixes problems with CD-ROM sector size detection & media change)
 *
 * Revision 0.100  1993/12/10  16:58:47  root
 * - fix for unsuccessful selections in case of non-continuous id assignments
 *   on the scsi bus.
 *
 * Revision 0.99  1993/10/24  16:19:59  root
 * - fixed DATA IN (rare read errors gone)
 *
 * Revision 0.98  1993/10/17  12:54:44  root
 * - fixed some recent fixes (shame on me)
 * - moved initialization of scratch area to aha152x_queue
 *
 * Revision 0.97  1993/10/09  18:53:53  root
 * - DATA IN fixed. Rarely left data in the fifo.
 *
 * Revision 0.96  1993/10/03  00:53:59  root
 * - minor changes on DATA IN
 *
 * Revision 0.95  1993/09/24  10:36:01  root
 * - change handling of MSGI after reselection
 * - fixed sti/cli
 * - minor changes
 *
 * Revision 0.94  1993/09/18  14:08:22  root
 * - fixed bug in multiple outstanding command code
 * - changed detection
 * - support for kernel command line configuration
 * - reset corrected
 * - changed message handling
 *
 * Revision 0.93  1993/09/15  20:41:19  root
 * - fixed bugs with multiple outstanding commands
 *
 * Revision 0.92  1993/09/13  02:46:33  root
 * - multiple outstanding commands work (no problems with IBM drive)
 *
 * Revision 0.91  1993/09/12  20:51:46  root
 * added multiple outstanding commands
 * (some problem with this $%&? IBM device remain)
 *
 * Revision 0.9  1993/09/12  11:11:22  root
 * - corrected auto-configuration
 * - changed the auto-configuration (added some '#define's)
 * - added support for dis-/reconnection
 *
 * Revision 0.8  1993/09/06  23:09:39  root
 * - added support for the drive activity light
 * - minor changes
 *
 * Revision 0.7  1993/09/05  14:30:15  root
 * - improved phase detection
 * - now using the new snarf_region code of 0.99pl13
 *
 * Revision 0.6  1993/09/02  11:01:38  root
 * first public release; added some signatures and biosparam()
 *
 * Revision 0.5  1993/08/30  10:23:30  root
 * fixed timing problems with my IBM drive
 *
 * Revision 0.4  1993/08/29  14:06:52  root
 * fixed some problems with timeouts due incomplete commands
 *
 * Revision 0.3  1993/08/28  15:55:03  root
 * writing data works too.  mounted and worked on a dos partition
 *
 * Revision 0.2  1993/08/27  22:42:07  root
 * reading data works.  Mounted a msdos partition.
 *
 * Revision 0.1  1993/08/25  13:38:30  root
 * first "damn thing doesn't work" version
 *
 * Revision 0.0  1993/08/14  19:54:25  root
 * empty function bodies; detect() works.
 *
 *
 **************************************************************************


 
 DESCRIPTION:

 This is the Linux low-level SCSI driver for Adaptec AHA-1520/1522
 SCSI host adapters.


 PER-DEFINE CONFIGURABLE OPTIONS:

 AUTOCONF:
   use configuration the controller reports (only 152x)

 SKIP_BIOSTEST:
   Don't test for BIOS signature (AHA-1510 or disabled BIOS)

 SETUP0	{ IOPORT, IRQ, SCSI_ID, RECONNECT, PARITY, SYNCHRONOUS, DELAY }:
   override for the first controller
   
 SETUP1	{ IOPORT, IRQ, SCSI_ID, RECONNECT, PARITY, SYNCHRONOUS, DELAY }:
   override for the second controller


 LILO COMMAND LINE OPTIONS:

 aha152x=<IOPORT>[,<IRQ>[,<SCSI-ID>[,<RECONNECT>[,<PARITY>[,<SYNCHRONOUS>[,<DELAY>]]]]]]

 The normal configuration can be overridden by specifying a command line.
 When you do this, the BIOS test is skipped. Entered values have to be
 valid (known). Don't use values that aren't supported under normal operation.
 If you think that you need other values: contact me.  For two controllers
 use the aha152x statement twice.


 REFERENCES USED:

 "AIC-6260 SCSI Chip Specification", Adaptec Corporation.

 "SCSI COMPUTER SYSTEM INTERFACE - 2 (SCSI-2)", X3T9.2/86-109 rev. 10h

 "Writing a SCSI device driver for Linux", Rik Faith (faith@cs.unc.edu)

 "Kernel Hacker's Guide", Michael K. Johnson (johnsonm@sunsite.unc.edu)

 "Adaptec 1520/1522 User's Guide", Adaptec Corporation.
 
 Michael K. Johnson (johnsonm@sunsite.unc.edu)

 Drew Eckhardt (drew@cs.colorado.edu)

 Eric Youngdale (ericy@cais.com) 

 special thanks to Eric Youngdale for the free(!) supplying the
 documentation on the chip.

 **************************************************************************/

#ifdef PCMCIA
#define MODULE
#endif

#include <linux/module.h>

#ifdef PCMCIA
#undef MODULE
#endif

#include <linux/sched.h>
#include <asm/io.h>
#include <linux/blk.h>
#include "scsi.h"
#include "sd.h"
#include "hosts.h"
#include "constants.h"
#include <asm/system.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>

#include "aha152x.h"
#include <linux/stat.h>

struct proc_dir_entry proc_scsi_aha152x = {
    PROC_SCSI_AHA152X, 7, "aha152x",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};

/* DEFINES */

#ifdef MACH
#define AUTOCONF
#endif

/* For PCMCIA cards, always use AUTOCONF */
#if defined(PCMCIA) || defined(MODULE)
#if !defined(AUTOCONF)
#define AUTOCONF
#endif
#endif

#if !defined(AUTOCONF) && !defined(SETUP0)
#error define AUTOCONF or SETUP0
#endif

#if defined(DEBUG_AHA152X)

#undef  SKIP_PORTS              /* don't display ports */

#undef  DEBUG_QUEUE             /* debug queue() */
#undef  DEBUG_RESET             /* debug reset() */
#undef  DEBUG_INTR              /* debug intr() */
#undef  DEBUG_SELECTION         /* debug selection part in intr() */
#undef  DEBUG_MSGO              /* debug message out phase in intr() */
#undef  DEBUG_MSGI              /* debug message in phase in intr() */
#undef  DEBUG_STATUS            /* debug status phase in intr() */
#undef  DEBUG_CMD               /* debug command phase in intr() */
#undef  DEBUG_DATAI             /* debug data in phase in intr() */
#undef  DEBUG_DATAO             /* debug data out phase in intr() */
#undef  DEBUG_ABORT             /* debug abort() */
#undef  DEBUG_DONE              /* debug done() */
#undef  DEBUG_BIOSPARAM         /* debug biosparam() */

#undef  DEBUG_RACE              /* debug race conditions */
#undef  DEBUG_PHASES            /* debug phases (useful to trace) */
#undef  DEBUG_QUEUES            /* debug reselection */

/* recently used for debugging */
#if 0
#endif

#define DEBUG_SELECTION
#define DEBUG_PHASES
#define DEBUG_RESET
#define DEBUG_ABORT

#define DEBUG_DEFAULT (debug_reset|debug_abort)

#endif

/* END OF DEFINES */

extern long loops_per_sec;

#define DELAY_DEFAULT 100

/* some additional "phases" for getphase() */
#define P_BUSFREE  1
#define P_PARITY   2

/* possible irq range */
#define IRQ_MIN 9
#define IRQ_MAX 12
#define IRQS    IRQ_MAX-IRQ_MIN+1

enum {
  not_issued   = 0x0001,
  in_selection = 0x0002,
  disconnected = 0x0004,
  aborted      = 0x0008,
  sent_ident   = 0x0010,
  in_other     = 0x0020,
  in_sync      = 0x0040,
  sync_ok      = 0x0080,
};

/* set by aha152x_setup according to the command line */
static int  setup_count=0;
static struct aha152x_setup {
  int io_port;
  int irq;
  int scsiid;
  int reconnect;
  int parity;
  int synchronous;
  int delay;
#ifdef DEBUG_AHA152X
  int debug;
#endif
  char *conf;
} setup[2];

static struct Scsi_Host *aha152x_host[IRQS];

#define HOSTDATA(shpnt)   ((struct aha152x_hostdata *) &shpnt->hostdata)
#define CURRENT_SC	  (HOSTDATA(shpnt)->current_SC)
#define ISSUE_SC	  (HOSTDATA(shpnt)->issue_SC)
#define DISCONNECTED_SC	  (HOSTDATA(shpnt)->disconnected_SC)
#define DELAY             (HOSTDATA(shpnt)->delay)
#define SYNCRATE	  (HOSTDATA(shpnt)->syncrate[CURRENT_SC->target])
#define MSG(i)            (HOSTDATA(shpnt)->message[i])
#define MSGLEN            (HOSTDATA(shpnt)->message_len)
#define ADDMSG(x)	  (MSG(MSGLEN++)=x)

struct aha152x_hostdata {
  Scsi_Cmnd     *issue_SC;
  Scsi_Cmnd     *current_SC;
  Scsi_Cmnd     *disconnected_SC;
  int           aborting;
  int           abortion_complete;
  int           abort_result;
  int           commands;
  
  int           reconnect;
  int           parity;
  int           synchronous;
  int           delay;
 
  unsigned char syncrate[8];
  
  unsigned char message[256];
  int           message_len;

#ifdef DEBUG_AHA152X
  int           debug;
#endif
};

void aha152x_intr(int irq, struct pt_regs *);
void aha152x_done(struct Scsi_Host *shpnt, int error);
void aha152x_setup(char *str, int *ints);
int aha152x_checksetup(struct aha152x_setup *setup);

static void aha152x_reset_ports(struct Scsi_Host *shpnt);
static void aha152x_panic(struct Scsi_Host *shpnt, char *msg);

static void disp_ports(struct Scsi_Host *shpnt);
static void show_command(Scsi_Cmnd *ptr);
static void show_queues(struct Scsi_Host *shpnt);
static void disp_enintr(struct Scsi_Host *shpnt);

#if defined(DEBUG_RACE)
static void enter_driver(const char *);
static void leave_driver(const char *);
#endif

/* possible i/o addresses for the AIC-6260 */
static unsigned short ports[] =
{
  0x340,      /* default first */
  0x140
};
#define PORT_COUNT (sizeof(ports) / sizeof(unsigned short))

#if !defined(SKIP_BIOSTEST)
/* possible locations for the Adaptec BIOS */
static void *addresses[] =
{
  (void *) 0xdc000,   /* default first */
  (void *) 0xc8000,
  (void *) 0xcc000,
  (void *) 0xd0000,
  (void *) 0xd4000,
  (void *) 0xd8000,
  (void *) 0xe0000,
  (void *) 0xeb800,   /* VTech Platinum SMP */
  (void *) 0xf0000,
};
#define ADDRESS_COUNT (sizeof(addresses) / sizeof(void *))

/* signatures for various AIC-6[23]60 based controllers.
   The point in detecting signatures is to avoid useless
   and maybe harmful probes on ports. I'm not sure that
   all listed boards pass auto-configuration. For those
   which fail the BIOS signature is obsolete, because
   user intervention to supply the configuration is 
   needed anyway. */
static struct signature {
  char *signature;
  int  sig_offset;
  int  sig_length;
} signatures[] =
{
  { "Adaptec AHA-1520 BIOS",      0x102e, 21 },  /* Adaptec 152x */
  { "Adaptec ASW-B626 BIOS",      0x1029, 21 },  /* on-board controller */
  { "Adaptec BIOS: ASW-B626",       0x0f, 22 },  /* on-board controller */
  { "Adaptec ASW-B626 S2",        0x2e6c, 19 },  /* on-board controller */
  { "Adaptec BIOS:AIC-6360",         0xc, 21 },  /* on-board controller */
  { "ScsiPro SP-360 BIOS",        0x2873, 19 },  /* ScsiPro-Controller  */
  { "GA-400 LOCAL BUS SCSI BIOS", 0x102e, 26 },  /* Gigabyte Local-Bus-SCSI */
  { "Adaptec BIOS:AVA-282X",         0xc, 21 },  /* Adaptec 282x */
  { "Adaptec IBM Dock II SCSI",   0x2edd, 24 },  /* IBM Thinkpad Dock II */
  { "Adaptec BIOS:AHA-1532P",       0x1c, 22 },  /* IBM Thinkpad Dock II SCSI */
};
#define SIGNATURE_COUNT (sizeof(signatures) / sizeof(struct signature))
#endif


static void do_pause(unsigned amount) /* Pause for amount*10 milliseconds */
{
   unsigned long the_time = jiffies + amount; /* 0.01 seconds per jiffy */

   while (jiffies < the_time)
    barrier();
}

/*
 *  queue services:
 */
static inline void append_SC(Scsi_Cmnd **SC, Scsi_Cmnd *new_SC)
{
  Scsi_Cmnd *end;

  new_SC->host_scribble = (unsigned char *) NULL;
  if(!*SC)
    *SC=new_SC;
  else
    {
      for(end=*SC; end->host_scribble; end = (Scsi_Cmnd *) end->host_scribble)
	;
      end->host_scribble = (unsigned char *) new_SC;
    }
}

static inline Scsi_Cmnd *remove_first_SC(Scsi_Cmnd **SC)
{
  Scsi_Cmnd *ptr;

  ptr=*SC;
  if(ptr)
    *SC= (Scsi_Cmnd *) (*SC)->host_scribble;
  return ptr;
}

static inline Scsi_Cmnd *remove_SC(Scsi_Cmnd **SC, int target, int lun)
{
  Scsi_Cmnd *ptr, *prev;

  for(ptr=*SC, prev=NULL;
       ptr && ((ptr->target!=target) || (ptr->lun!=lun));
      prev = ptr, ptr = (Scsi_Cmnd *) ptr->host_scribble)
    ;

  if(ptr)
    if(prev)
      prev->host_scribble = ptr->host_scribble;
    else
      *SC= (Scsi_Cmnd *) ptr->host_scribble;
  return ptr;
}

/*
 * read inbound byte and wait for ACK to get low
 */
static void make_acklow(struct Scsi_Host *shpnt)
{
  SETPORT(SXFRCTL0, CH1|SPIOEN);
  GETPORT(SCSIDAT);
  SETPORT(SXFRCTL0, CH1);

  while(TESTHI(SCSISIG, ACKI))
    barrier();
}

/*
 * detect current phase more reliable:
 * phase is valid, when the target asserts REQ after we've deasserted ACK.
 *
 * return value is a valid phase or an error code.
 *
 * errorcodes:
 *   P_BUSFREE   BUS FREE phase detected
 *   P_PARITY    parity error in DATA phase
 */
static int getphase(struct Scsi_Host *shpnt)
{
  int phase, sstat1;
  
  while(1)
    {
      do
	{
          while(!((sstat1 = GETPORT(SSTAT1)) & (BUSFREE|SCSIRSTI|REQINIT)))
            barrier();
          if(sstat1 & BUSFREE)
	    return P_BUSFREE;
          if(sstat1 & SCSIRSTI)
	    {
	      printk("aha152x: RESET IN\n");
              SETPORT(SSTAT1, SCSIRSTI);
	    }
	}
      while(TESTHI(SCSISIG, ACKI) || TESTLO(SSTAT1, REQINIT));

      SETPORT(SSTAT1, CLRSCSIPERR);
  
      phase = GETPORT(SCSISIG) & P_MASK ;

      if(TESTHI(SSTAT1, SCSIPERR))
	{
          if((phase & (CDO|MSGO))==0)                        /* DATA phase */
	    return P_PARITY;

          make_acklow(shpnt);
	}
      else
	return phase;
    }
}

/* called from init/main.c */
void aha152x_setup(char *str, int *ints)
{
  if(setup_count>2)
    panic("aha152x: you can only configure up to two controllers\n");

  setup[setup_count].conf        = str;
  setup[setup_count].io_port     = ints[0] >= 1 ? ints[1] : 0x340;
  setup[setup_count].irq         = ints[0] >= 2 ? ints[2] : 11;
  setup[setup_count].scsiid      = ints[0] >= 3 ? ints[3] : 7;
  setup[setup_count].reconnect   = ints[0] >= 4 ? ints[4] : 1;
  setup[setup_count].parity      = ints[0] >= 5 ? ints[5] : 1;
  setup[setup_count].synchronous = ints[0] >= 6 ? ints[6] : 0 /* FIXME: 1 */;
  setup[setup_count].delay       = ints[0] >= 7 ? ints[7] : DELAY_DEFAULT;
#ifdef DEBUG_AHA152X
  setup[setup_count].debug       = ints[0] >= 8 ? ints[8] : DEBUG_DEFAULT;
  if(ints[0]>8)
    { 
      printk("aha152x: usage: aha152x=<IOBASE>[,<IRQ>[,<SCSI ID>"
 	         "[,<RECONNECT>[,<PARITY>[,<SYNCHRONOUS>[,<DELAY>[,<DEBUG>]]]]]]]\n");
#else
  if(ints[0]>7)
    {
      printk("aha152x: usage: aha152x=<IOBASE>[,<IRQ>[,<SCSI ID>"
             "[,<RECONNECT>[,<PARITY>[,<SYNCHRONOUS>[,<DELAY>]]]]]]\n");
#endif
    }
  else 
    setup_count++;
}

/*
   Test, if port_base is valid.
 */
static int aha152x_porttest(int io_port)
{
  int i;

  if(check_region(io_port, IO_RANGE))
    return 0;

  SETPORT(io_port+O_DMACNTRL1, 0);          /* reset stack pointer */
  for(i=0; i<16; i++)
    SETPORT(io_port+O_STACK, i);

  SETPORT(io_port+O_DMACNTRL1, 0);          /* reset stack pointer */
  for(i=0; i<16 && GETPORT(io_port+O_STACK)==i; i++)
    ;

  return(i==16);
}

int aha152x_checksetup(struct aha152x_setup *setup)
{
  int i;
  
#ifndef PCMCIA
  for(i=0; i<PORT_COUNT && (setup->io_port != ports[i]); i++)
    ;
  
  if(i==PORT_COUNT)
    return 0;
#endif
  
  if(!aha152x_porttest(setup->io_port))
    return 0;
  
  if((setup->irq < IRQ_MIN) && (setup->irq > IRQ_MAX))
    return 0;
  
  if((setup->scsiid < 0) || (setup->scsiid > 7))
    return 0;
  
  if((setup->reconnect < 0) || (setup->reconnect > 1))
    return 0;
  
  if((setup->parity < 0) || (setup->parity > 1))
    return 0;
  
  if((setup->synchronous < 0) || (setup->synchronous > 1))
    return 0;
  
  return 1;
}


int aha152x_detect(Scsi_Host_Template * tpnt)
{
  int                 i, j, ok;
#if defined(AUTOCONF)
  aha152x_config      conf;
#endif
  
  tpnt->proc_dir = &proc_scsi_aha152x;

  for(i=0; i<IRQS; i++)
    aha152x_host[i] = (struct Scsi_Host *) NULL;
  
  if(setup_count)
    {
      printk("aha152x: processing commandline: ");
   
      for(i=0; i<setup_count; i++)
        if(!aha152x_checksetup(&setup[i]))
	{
                printk("\naha152x: %s\n", setup[i].conf);
                printk("aha152x: invalid line (controller=%d)\n", i+1);
	}

      printk("ok\n");
	}

#ifdef SETUP0
  if(setup_count<2)
	{
      struct aha152x_setup override = SETUP0;

      if(setup_count==0 || (override.io_port != setup[0].io_port))
        if(!aha152x_checksetup(&override))
	{
                printk("\naha152x: SETUP0 (0x%x, %d, %d, %d, %d, %d, %d) invalid\n",
        	      override.io_port,
        	      override.irq,
        	      override.scsiid,
        	      override.reconnect,
        	      override.parity,
        	      override.synchronous,
        	      override.delay);
	}
        else
          setup[setup_count++] = override;
	}
#endif

#ifdef SETUP1
  if(setup_count<2)
	{
      struct aha152x_setup override = SETUP1;

      if(setup_count==0 || (override.io_port != setup[0].io_port))
        if(!aha152x_checksetup(&override))
	{
                printk("\naha152x: SETUP1 (0x%x, %d, %d, %d, %d, %d, %d) invalid\n",
        	       override.io_port,
        	       override.irq,
        	       override.scsiid,
        	       override.reconnect,
        	       override.parity,
        	       override.synchronous,
        	       override.delay);
    }
  else
          setup[setup_count++] = override;
    }
#endif
  
#if defined(AUTOCONF)
  if(setup_count<2)
    {
#if !defined(SKIP_BIOSTEST)
      ok=0;
      for(i=0; i < ADDRESS_COUNT && !ok; i++)
            for(j=0; (j < SIGNATURE_COUNT) && !ok; j++)
	  ok=!memcmp((void *) addresses[i]+signatures[j].sig_offset,
		     (void *) signatures[j].signature,
		     (int) signatures[j].sig_length);

      if(!ok && setup_count==0)
	return 0;

      printk("aha152x: BIOS test: passed, ");
#else
      printk("aha152x: ");
#endif /* !SKIP_BIOSTEST */
 
      for(i=0; i<PORT_COUNT && setup_count<2; i++)
            {
              if((setup_count==1) && (setup[0].io_port == ports[i]))
                continue;

              if(aha152x_porttest(ports[i]))
	{
                  setup[setup_count].io_port = ports[i];
              
                  conf.cf_port =
        	        (GETPORT(ports[i]+O_PORTA)<<8) + GETPORT(ports[i]+O_PORTB);
              
                  setup[setup_count].irq         = IRQ_MIN + conf.cf_irq;
                  setup[setup_count].scsiid      = conf.cf_id;
                  setup[setup_count].reconnect   = conf.cf_tardisc;
                  setup[setup_count].parity      = !conf.cf_parity;
                  setup[setup_count].synchronous = 0 /* FIXME: conf.cf_syncneg */;
                  setup[setup_count].delay       = DELAY_DEFAULT;
#ifdef DEBUG_AHA152X
                  setup[setup_count].debug       = DEBUG_DEFAULT;
#endif
                  setup_count++;
                }
	}

      printk("auto configuration: ok, ");
    }
#endif

  printk("detection complete\n");

  for(i=0; i<setup_count; i++)
    {
      struct Scsi_Host        *shpnt;

      shpnt = aha152x_host[setup[i].irq-IRQ_MIN] =
        scsi_register(tpnt, sizeof(struct aha152x_hostdata));

      shpnt->io_port                     = setup[i].io_port;
      shpnt->n_io_port                   = IO_RANGE;
      shpnt->irq                         = setup[i].irq;

      ISSUE_SC                           = (Scsi_Cmnd *) NULL;
      CURRENT_SC                         = (Scsi_Cmnd *) NULL;
      DISCONNECTED_SC                    = (Scsi_Cmnd *) NULL;

      HOSTDATA(shpnt)->reconnect         = setup[i].reconnect;
      HOSTDATA(shpnt)->parity            = setup[i].parity;
      HOSTDATA(shpnt)->synchronous       = setup[i].synchronous;
      HOSTDATA(shpnt)->delay             = setup[i].delay;
#ifdef DEBUG_AHA152X
      HOSTDATA(shpnt)->debug             = setup[i].debug;
#endif

      HOSTDATA(shpnt)->aborting          = 0;
      HOSTDATA(shpnt)->abortion_complete = 0;
      HOSTDATA(shpnt)->abort_result      = 0;
      HOSTDATA(shpnt)->commands          = 0;

      HOSTDATA(shpnt)->message_len       = 0;

      for(j=0; j<8; j++)
        HOSTDATA(shpnt)->syncrate[j] = 0;
 
      SETPORT(SCSIID, setup[i].scsiid << 4);
      shpnt->this_id=setup[i].scsiid;
  
      if(setup[i].reconnect)
        shpnt->hostt->can_queue=AHA152X_MAXQUEUE;

  /* RESET OUT */
      SETBITS(SCSISEQ, SCSIRSTO);
  do_pause(30);
      CLRBITS(SCSISEQ, SCSIRSTO);
      do_pause(setup[i].delay);

      aha152x_reset_ports(shpnt);
      
      printk("aha152x%d: vital data: PORTBASE=0x%03x, IRQ=%d, SCSI ID=%d,"
             " reconnect=%s, parity=%s, synchronous=%s, delay=%d\n",
      	     i,
             shpnt->io_port,
             shpnt->irq,
             shpnt->this_id,
             HOSTDATA(shpnt)->reconnect ? "enabled" : "disabled",
             HOSTDATA(shpnt)->parity ? "enabled" : "disabled",
             HOSTDATA(shpnt)->synchronous ? "enabled" : "disabled",
             HOSTDATA(shpnt)->delay);

      request_region(shpnt->io_port, IO_RANGE, "aha152x");  /* Register */
  
  /* not expecting any interrupts */
  SETPORT(SIMODE0, 0);
  SETPORT(SIMODE1, 0);

      SETBITS(DMACNTRL0, INTEN);

      ok = request_irq(setup[i].irq, aha152x_intr, SA_INTERRUPT, "aha152x");
      
      if(ok<0)
        {
          if(ok == -EINVAL)
            {
              printk("aha152x%d: bad IRQ %d.\n", i, setup[i].irq);
              printk("          Contact author.\n");
            }
          else
            if(ok == -EBUSY)
              printk("aha152x%d: IRQ %d already in use. Configure another.\n",
        	     i, setup[i].irq);
            else
              {
                printk("\naha152x%d: Unexpected error code on"
        	       " requesting IRQ %d.\n", i, setup[i].irq);
                printk("          Contact author.\n");
              }
          printk("aha152x: driver needs an IRQ.\n");
          continue;
        }
    }
  
  return (setup_count>0);
}

/* 
 *  Queue a command and setup interrupts for a free bus.
 */
int aha152x_queue(Scsi_Cmnd * SCpnt, void (*done)(Scsi_Cmnd *))
{
  struct Scsi_Host *shpnt = SCpnt->host;
  unsigned long flags;

#if defined(DEBUG_RACE)
  enter_driver("queue");
#else
#if defined(DEBUG_QUEUE)
  if(HOSTDATA(shpnt)->debug & debug_queue)
    printk("aha152x: queue(), ");
#endif
#endif

#if defined(DEBUG_QUEUE)
  if(HOSTDATA(shpnt)->debug & debug_queue)
  {
      printk("SCpnt (target = %d lun = %d cmnd = ",
             SCpnt->target, SCpnt->lun);
    print_command(SCpnt->cmnd);
      printk(", cmd_len=%d, pieces = %d size = %u), ",
             SCpnt->cmd_len, SCpnt->use_sg, SCpnt->request_bufflen);
      disp_ports(shpnt);
  }
#endif

  SCpnt->scsi_done =       done;

  /* setup scratch area
     SCp.ptr              : buffer pointer
     SCp.this_residual    : buffer length
     SCp.buffer           : next buffer
     SCp.buffers_residual : left buffers in list
     SCp.phase            : current state of the command */
  SCpnt->SCp.phase = not_issued;
  if (SCpnt->use_sg)
    {
      SCpnt->SCp.buffer =
        (struct scatterlist *) SCpnt->request_buffer;
      SCpnt->SCp.ptr              = SCpnt->SCp.buffer->address;
      SCpnt->SCp.this_residual    = SCpnt->SCp.buffer->length;
      SCpnt->SCp.buffers_residual = SCpnt->use_sg - 1;
    }
  else
    {
      SCpnt->SCp.ptr              = (char *)SCpnt->request_buffer;
      SCpnt->SCp.this_residual    = SCpnt->request_bufflen;
      SCpnt->SCp.buffer           = NULL;
      SCpnt->SCp.buffers_residual = 0;
    }
	  
  SCpnt->SCp.Status              = CHECK_CONDITION;
  SCpnt->SCp.Message             = 0;
  SCpnt->SCp.have_data_in        = 0;
  SCpnt->SCp.sent_command        = 0;

  /* Turn led on, when this is the first command. */
  save_flags(flags);
  cli();
  HOSTDATA(shpnt)->commands++;
  if(HOSTDATA(shpnt)->commands==1)
    SETPORT(PORTA, 1);

#if defined(DEBUG_QUEUES)
  if(HOSTDATA(shpnt)->debug & debug_queues)
    printk("i+ (%d), ", HOSTDATA(shpnt)->commands);
#endif
  append_SC(&ISSUE_SC, SCpnt);
  
  /* Enable bus free interrupt, when we aren't currently on the bus */
  if(!CURRENT_SC)
    {
      SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
      SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);
    }
  restore_flags(flags);

#if defined(DEBUG_RACE)
  leave_driver("queue");
#endif

  return 0;
}

/*
 *  We only support commands in interrupt-driven fashion
 */
int aha152x_command(Scsi_Cmnd *SCpnt)
{
  printk("aha152x: interrupt driven driver; use aha152x_queue()\n");
  return -1;
}

/*
 *  Abort a queued command
 *  (commands that are on the bus can't be aborted easily)
 */
int aha152x_abort(Scsi_Cmnd *SCpnt)
{
  struct Scsi_Host *shpnt = SCpnt->host;
  unsigned long flags;
  Scsi_Cmnd *ptr, *prev;

  save_flags(flags);
  cli();

#if defined(DEBUG_ABORT)
  if(HOSTDATA(shpnt)->debug & debug_abort)
  { 
      printk("aha152x: abort(), SCpnt=0x%08x, ", (unsigned int) SCpnt);
      show_queues(shpnt);
  }
#endif

  /* look for command in issue queue */
  for(ptr=ISSUE_SC, prev=NULL;
       ptr && ptr!=SCpnt;
       prev=ptr, ptr=(Scsi_Cmnd *) ptr->host_scribble)
    ;

  if(ptr)
    {
      /* dequeue */
      if(prev)
	prev->host_scribble = ptr->host_scribble;
      else
        ISSUE_SC = (Scsi_Cmnd *) ptr->host_scribble;
      restore_flags(flags);

      ptr->host_scribble = NULL;
      ptr->result = DID_ABORT << 16;
      ptr->scsi_done(ptr);
      return SCSI_ABORT_SUCCESS;
    }

  /* if the bus is busy or a command is currently processed,
     we can't do anything more */
  if (TESTLO(SSTAT1, BUSFREE) || (CURRENT_SC && CURRENT_SC!=SCpnt))
    {
      /* fail abortion, if bus is busy */

      if(!CURRENT_SC)
	printk("bus busy w/o current command, ");
 
      restore_flags(flags);
      return SCSI_ABORT_BUSY;
    }

  /* bus is free */

  if(CURRENT_SC)
  { 
    /* target entered bus free before COMMAND COMPLETE, nothing to abort */
    restore_flags(flags);
      CURRENT_SC->result = DID_ERROR << 16;
      CURRENT_SC->scsi_done(CURRENT_SC);
      CURRENT_SC = (Scsi_Cmnd *) NULL;
    return SCSI_ABORT_SUCCESS;
  }

  /* look for command in disconnected queue */
  for(ptr=DISCONNECTED_SC, prev=NULL;
       ptr && ptr!=SCpnt;
       prev=ptr, ptr=(Scsi_Cmnd *) ptr->host_scribble)
    ;

  if(ptr)
    if(!HOSTDATA(shpnt)->aborting)
      {
	/* dequeue */
	if(prev)
	  prev->host_scribble = ptr->host_scribble;
	else
          DISCONNECTED_SC = (Scsi_Cmnd *) ptr->host_scribble;
  
	/* set command current and initiate selection,
	   let the interrupt routine take care of the abortion */
        CURRENT_SC     = ptr;
	ptr->SCp.phase = in_selection|aborted;
        SETPORT(SCSIID, (shpnt->this_id << OID_) | CURRENT_SC->target);
        
        ADDMSG(ABORT);
  
	/* enable interrupts for SELECTION OUT DONE and SELECTION TIME OUT */
        SETPORT(SIMODE0, ENSELDO | (DISCONNECTED_SC ? ENSELDI : 0));
        SETPORT(SIMODE1, ENSELTIMO);
  
	/* Enable SELECTION OUT sequence */
        SETBITS(SCSISEQ, ENSELO | ENAUTOATNO);
  
        SETBITS(DMACNTRL0, INTEN);
        HOSTDATA(shpnt)->abort_result=SCSI_ABORT_SUCCESS;
        HOSTDATA(shpnt)->aborting++;
        HOSTDATA(shpnt)->abortion_complete=0;

	sti();  /* Hi Eric, guess what ;-) */
  
	/* sleep until the abortion is complete */
        while(!HOSTDATA(shpnt)->abortion_complete)
	  barrier();
        HOSTDATA(shpnt)->aborting=0;
        return HOSTDATA(shpnt)->abort_result;
      }
    else
      {
	/* we're already aborting a command */
	restore_flags(flags);
	return SCSI_ABORT_BUSY;
      }

  /* command wasn't found */
  printk("command not found\n");
  restore_flags(flags);
  return SCSI_ABORT_NOT_RUNNING;
}

/*
 *  Restore default values to the AIC-6260 registers and reset the fifos
 */
static void aha152x_reset_ports(struct Scsi_Host *shpnt)
{
  /* disable interrupts */
  SETPORT(DMACNTRL0, RSTFIFO);

  SETPORT(SCSISEQ, 0);

  SETPORT(SXFRCTL1, 0);
  SETPORT(SCSISIG, 0);
  SETPORT(SCSIRATE, 0);

  /* clear all interrupt conditions */
  SETPORT(SSTAT0, 0x7f);
  SETPORT(SSTAT1, 0xef);

  SETPORT(SSTAT4, SYNCERR|FWERR|FRERR);

  SETPORT(DMACNTRL0, 0);
  SETPORT(DMACNTRL1, 0);

  SETPORT(BRSTCNTRL, 0xf1);

  /* clear SCSI fifo and transfer count */
  SETPORT(SXFRCTL0, CH1|CLRCH1|CLRSTCNT);
  SETPORT(SXFRCTL0, CH1);

  /* enable interrupts */
  SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
  SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);
}

/*
 *  Reset registers, reset a hanging bus and
 *  kill active and disconnected commands for target w/o soft reset
 */
int aha152x_reset(Scsi_Cmnd *SCpnt)
{
  struct Scsi_Host *shpnt = SCpnt->host;
  unsigned long flags;
  Scsi_Cmnd *ptr, *prev, *next;

  aha152x_reset_ports(shpnt);

  /* Reset, if bus hangs */
  if(TESTLO(SSTAT1, BUSFREE))
    {
       CLRBITS(DMACNTRL0, INTEN);

#if defined(DEBUG_RESET)
       if(HOSTDATA(shpnt)->debug & debug_reset)
  {
       printk("aha152x: reset(), bus not free: SCSI RESET OUT\n");
           show_queues(shpnt);
  }
#endif

       ptr=CURRENT_SC;
       if(ptr && !ptr->device->soft_reset)
	 {
           ptr->host_scribble = NULL;
           ptr->result = DID_RESET << 16;
           ptr->scsi_done(CURRENT_SC);
           CURRENT_SC=NULL;
	 }

       save_flags(flags);
       cli();
       prev=NULL; ptr=DISCONNECTED_SC;
       while(ptr)
	 {
	   if(!ptr->device->soft_reset)
	     {
	       if(prev)
		 prev->host_scribble = ptr->host_scribble;
	       else
        	 DISCONNECTED_SC = (Scsi_Cmnd *) ptr->host_scribble;

	       next = (Scsi_Cmnd *) ptr->host_scribble;
  
	       ptr->host_scribble = NULL;
	       ptr->result        = DID_RESET << 16;
               ptr->scsi_done(ptr);
  
	       ptr = next; 
	     }
	   else
	     {
	       prev=ptr;
	       ptr = (Scsi_Cmnd *) ptr->host_scribble;
	     }
	 }
       restore_flags(flags);

#if defined(DEBUG_RESET)
       if(HOSTDATA(shpnt)->debug & debug_reset)
       {
	 printk("commands on targets w/ soft-resets:\n");
           show_queues(shpnt);
       }
#endif

       /* RESET OUT */
       SETPORT(SCSISEQ, SCSIRSTO);
       do_pause(30);
       SETPORT(SCSISEQ, 0);
       do_pause(DELAY);

       SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
       SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);

       SETPORT(DMACNTRL0, INTEN);
    }

  return SCSI_RESET_SUCCESS;
}

/*
 * Return the "logical geometry"
 */
int aha152x_biosparam(Scsi_Disk * disk, kdev_t dev, int *info_array)
{
  int size = disk->capacity;

#if defined(DEBUG_BIOSPARAM)
  if(HOSTDATA(shpnt)->debug & debug_biosparam)
    printk("aha152x_biosparam: dev=%s, size=%d, ", kdevname(dev), size);
#endif
  
/* I took this from other SCSI drivers, since it provides
   the correct data for my devices. */
  info_array[0]=64;
  info_array[1]=32;
  info_array[2]=size>>11;

#if defined(DEBUG_BIOSPARAM)
  if(HOSTDATA(shpnt)->debug & debug_biosparam)
  {
    printk("bios geometry: head=%d, sec=%d, cyl=%d\n",
	   info_array[0], info_array[1], info_array[2]);
    printk("WARNING: check, if the bios geometry is correct.\n");
  }
#endif

  return 0;
}

/*
 *  Internal done function
 */
void aha152x_done(struct Scsi_Host *shpnt, int error)
{
  unsigned long flags;
  Scsi_Cmnd *done_SC;

#if defined(DEBUG_DONE)
  if(HOSTDATA(shpnt)->debug & debug_done)
  {
    printk("\naha152x: done(), ");
    disp_ports(shpnt);
  }
#endif

  if (CURRENT_SC)
    {
#if defined(DEBUG_DONE)
      if(HOSTDATA(shpnt)->debug & debug_done)
	printk("done(%x), ", error);
#endif

      save_flags(flags);
      cli();

      done_SC = CURRENT_SC;
      CURRENT_SC = NULL;

      /* turn led off, when no commands are in the driver */
      HOSTDATA(shpnt)->commands--;
      if(!HOSTDATA(shpnt)->commands)
        SETPORT(PORTA, 0);                                  /* turn led off */

#if defined(DEBUG_QUEUES)
      if(HOSTDATA(shpnt)->debug & debug_queues) 
        printk("ok (%d), ", HOSTDATA(shpnt)->commands);
#endif
      restore_flags(flags);

      SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
      SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);

#if defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & debug_phases)
	printk("BUS FREE loop, ");
#endif
      while(TESTLO(SSTAT1, BUSFREE))
        barrier();
#if defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & debug_phases)
	printk("BUS FREE\n");
#endif

      done_SC->result = error;
      if(done_SC->scsi_done)
	{
#if defined(DEBUG_DONE)
          if(HOSTDATA(shpnt)->debug & debug_done)
	    printk("calling scsi_done, ");
#endif
          done_SC->scsi_done(done_SC);
#if defined(DEBUG_DONE)
          if(HOSTDATA(shpnt)->debug & debug_done)
	    printk("done returned, ");
#endif
	}
      else
        panic("aha152x: current_SC->scsi_done() == NULL");
    }
  else
    aha152x_panic(shpnt, "done() called outside of command");
}

/*
 * Interrupts handler (main routine of the driver)
 */
void aha152x_intr(int irqno, struct pt_regs * regs)
{
  struct Scsi_Host *shpnt = aha152x_host[irqno-IRQ_MIN];
  unsigned int flags;
  int done=0, phase;

#if defined(DEBUG_RACE)
  enter_driver("intr");
#else
#if defined(DEBUG_INTR)
  if(HOSTDATA(shpnt)->debug & debug_intr)
    printk("\naha152x: intr(), ");
#endif
#endif

  /* no more interrupts from the controller, while we busy.
     INTEN has to be restored, when we're ready to leave
     intr(). To avoid race conditions we have to return
     immediately afterwards. */
  CLRBITS(DMACNTRL0, INTEN);
  sti();  /* Yes, sti() really needs to be here */

  /* disconnected target is trying to reconnect.
     Only possible, if we have disconnected nexuses and
     nothing is occupying the bus.
  */
  if(TESTHI(SSTAT0, SELDI) &&
      DISCONNECTED_SC &&
      (!CURRENT_SC || (CURRENT_SC->SCp.phase & in_selection)) )
    {
      int identify_msg, target, i;

      /* Avoid conflicts when a target reconnects
	 while we are trying to connect to another. */
      if(CURRENT_SC)
	{
#if defined(DEBUG_QUEUES)
          if(HOSTDATA(shpnt)->debug & debug_queues)
	  printk("i+, ");
#endif
	  save_flags(flags);
	  cli();
          append_SC(&ISSUE_SC, CURRENT_SC);
          CURRENT_SC=NULL;
	  restore_flags(flags);
	}

      /* disable sequences */
      SETPORT(SCSISEQ, 0);
      SETPORT(SSTAT0, CLRSELDI);
      SETPORT(SSTAT1, CLRBUSFREE);

#if defined(DEBUG_QUEUES) || defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & (debug_queues|debug_phases))
	printk("reselected, ");
#endif

      i = GETPORT(SELID) & ~(1 << shpnt->this_id);
      target=0;
      if(i)
        for(; (i & 1)==0; target++, i>>=1)
	  ;
      else
        aha152x_panic(shpnt, "reconnecting target unknown");

#if defined(DEBUG_QUEUES)
      if(HOSTDATA(shpnt)->debug & debug_queues)
        printk("SELID=%02x, target=%d, ", GETPORT(SELID), target);
#endif
      SETPORT(SCSIID, (shpnt->this_id << OID_) | target);
      SETPORT(SCSISEQ, ENRESELI);

      if(TESTLO(SSTAT0, SELDI))
        aha152x_panic(shpnt, "RESELI failed");

      SETPORT(SCSIRATE, HOSTDATA(shpnt)->syncrate[target]&0x7f);

      SETPORT(SCSISIG, P_MSGI);

      /* Get identify message */
      if((i=getphase(shpnt))!=P_MSGI)
	{
	  printk("target doesn't enter MSGI to identify (phase=%02x)\n", i);
          aha152x_panic(shpnt, "unknown lun");
	}
      SETPORT(SCSISEQ, 0);

      SETPORT(SXFRCTL0, CH1);

      identify_msg = GETPORT(SCSIBUS);

      if(!(identify_msg & IDENTIFY_BASE))
	{
	  printk("target=%d, inbound message (%02x) != IDENTIFY\n",
		 target, identify_msg);
          aha152x_panic(shpnt, "unknown lun");
	}


#if defined(DEBUG_QUEUES)
      if(HOSTDATA(shpnt)->debug & debug_queues)
        printk("identify=%02x, lun=%d, ", identify_msg, identify_msg & 0x3f);
#endif

      save_flags(flags);
      cli();

#if defined(DEBUG_QUEUES)
      if(HOSTDATA(shpnt)->debug & debug_queues)
	printk("d-, ");
#endif
      CURRENT_SC = remove_SC(&DISCONNECTED_SC,
			      target,
        		     identify_msg & 0x3f);

      if(!CURRENT_SC)
	{
          printk("lun=%d, ", identify_msg & 0x3f);
          aha152x_panic(shpnt, "no disconnected command for that lun");
	}

      CURRENT_SC->SCp.phase &= ~disconnected;
      restore_flags(flags);

      make_acklow(shpnt);
      if(getphase(shpnt)!=P_MSGI) {
      SETPORT(SIMODE0, 0);
      SETPORT(SIMODE1, ENPHASEMIS|ENBUSFREE);
#if defined(DEBUG_RACE)
      leave_driver("(reselected) intr");
#endif
      SETBITS(DMACNTRL0, INTEN);
      return;
    }
    }
  
  /* Check, if we aren't busy with a command */
  if(!CURRENT_SC)
    {
      /* bus is free to issue a queued command */
      if(TESTHI(SSTAT1, BUSFREE) && ISSUE_SC)
	{
	  save_flags(flags);
	  cli();
#if defined(DEBUG_QUEUES)
          if(HOSTDATA(shpnt)->debug & debug_queues)
	    printk("i-, ");
#endif
          CURRENT_SC = remove_first_SC(&ISSUE_SC);
	  restore_flags(flags);

#if defined(DEBUG_INTR) || defined(DEBUG_SELECTION) || defined(DEBUG_PHASES)
          if(HOSTDATA(shpnt)->debug & (debug_intr|debug_selection|debug_phases))
	    printk("issuing command, ");
#endif
          CURRENT_SC->SCp.phase = in_selection;

#if defined(DEBUG_INTR) || defined(DEBUG_SELECTION) || defined(DEBUG_PHASES)
          if(HOSTDATA(shpnt)->debug & (debug_intr|debug_selection|debug_phases))
            printk("selecting %d, ", CURRENT_SC->target); 
#endif
          SETPORT(SCSIID, (shpnt->this_id << OID_) | CURRENT_SC->target);

	  /* Enable interrupts for SELECTION OUT DONE and SELECTION OUT INITIATED */
          SETPORT(SXFRCTL1, HOSTDATA(shpnt)->parity ? (ENSPCHK|ENSTIMER) : ENSTIMER);

	  /* enable interrupts for SELECTION OUT DONE and SELECTION TIME OUT */
          SETPORT(SIMODE0, ENSELDO | (DISCONNECTED_SC ? ENSELDI : 0));
          SETPORT(SIMODE1, ENSELTIMO);

	  /* Enable SELECTION OUT sequence */
          SETBITS(SCSISEQ, ENSELO | ENAUTOATNO);
	
        }
      else
        {
          /* No command we are busy with and no new to issue */
          printk("aha152x: ignoring spurious interrupt, nothing to do\n");
          if(TESTHI(DMACNTRL0, SWINT)) {
            printk("aha152x: SWINT is set!  Why?\n");
            CLRBITS(DMACNTRL0, SWINT);
          }
          show_queues(shpnt);
        }

#if defined(DEBUG_RACE)
	  leave_driver("(selecting) intr");
#endif
          SETBITS(DMACNTRL0, INTEN);
	  return;
	}

  /* the bus is busy with something */

#if defined(DEBUG_INTR)
  if(HOSTDATA(shpnt)->debug & debug_intr)
    disp_ports(shpnt);
#endif

  /* we are waiting for the result of a selection attempt */
  if(CURRENT_SC->SCp.phase & in_selection)
    {
      if(TESTLO(SSTAT1, SELTO))
	/* no timeout */
        if(TESTHI(SSTAT0, SELDO))
	  {
	    /* clear BUS FREE interrupt */
            SETPORT(SSTAT1, CLRBUSFREE);

	    /* Disable SELECTION OUT sequence */
            CLRBITS(SCSISEQ, ENSELO|ENAUTOATNO);

	    /* Disable SELECTION OUT DONE interrupt */
	    CLRBITS(SIMODE0, ENSELDO);
	    CLRBITS(SIMODE1, ENSELTIMO);

            if(TESTLO(SSTAT0, SELDO))
	      {
		printk("aha152x: passing bus free condition\n");

#if defined(DEBUG_RACE)
		leave_driver("(passing bus free) intr");
#endif
        	SETBITS(DMACNTRL0, INTEN);

        	if(CURRENT_SC->SCp.phase & aborted)
		  {
        	    HOSTDATA(shpnt)->abort_result=SCSI_ABORT_ERROR;
        	    HOSTDATA(shpnt)->abortion_complete++;
		  }

        	aha152x_done(shpnt, DID_NO_CONNECT << 16);
		return;
	      }
#if defined(DEBUG_SELECTION) || defined(DEBUG_PHASES)
            if(HOSTDATA(shpnt)->debug & (debug_selection|debug_phases))
	      printk("SELDO (SELID=%x), ", GETPORT(SELID));
#endif

	    /* selection was done */
            SETPORT(SSTAT0, CLRSELDO);

#if defined(DEBUG_ABORT)
            if((HOSTDATA(shpnt)->debug & debug_abort) && (CURRENT_SC->SCp.phase & aborted))
	      printk("(ABORT) target selected, ");
#endif

            CURRENT_SC->SCp.phase &= ~in_selection;
            CURRENT_SC->SCp.phase |= in_other;

            ADDMSG(IDENTIFY(HOSTDATA(shpnt)->reconnect,CURRENT_SC->lun));

            if(!(SYNCRATE&0x80) && HOSTDATA(shpnt)->synchronous)
              {
                ADDMSG(EXTENDED_MESSAGE);
                ADDMSG(3);
                ADDMSG(EXTENDED_SDTR);
        	ADDMSG(50);
                ADDMSG(8);

                printk("outbound SDTR: ");
                print_msg(&MSG(MSGLEN-5));

        	SYNCRATE=0x80;
                CURRENT_SC->SCp.phase |= in_sync;
              }

#if defined(DEBUG_RACE)
	    leave_driver("(SELDO) intr");
#endif
            SETPORT(SCSIRATE, SYNCRATE&0x7f);

            SETPORT(SCSISIG, P_MSGO);

            SETPORT(SIMODE0, 0);
            SETPORT(SIMODE1, ENREQINIT|ENBUSFREE);
            SETBITS(DMACNTRL0, INTEN);
	    return;
	  }
	else
          aha152x_panic(shpnt, "neither timeout nor selection\007");
      else
	{
#if defined(DEBUG_SELECTION) || defined(DEBUG_PHASES)
          if(HOSTDATA(shpnt)->debug & (debug_selection|debug_phases))
	  printk("SELTO, ");
#endif
	  /* end selection attempt */
          CLRBITS(SCSISEQ, ENSELO|ENAUTOATNO);

	  /* timeout */
          SETPORT(SSTAT1, CLRSELTIMO);

          SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
          SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);
          SETBITS(DMACNTRL0, INTEN);
#if defined(DEBUG_RACE)
	  leave_driver("(SELTO) intr");
#endif

          if(CURRENT_SC->SCp.phase & aborted)
	    {
#if defined(DEBUG_ABORT)
              if(HOSTDATA(shpnt)->debug & debug_abort)
		printk("(ABORT) selection timeout, ");
#endif
              HOSTDATA(shpnt)->abort_result=SCSI_ABORT_ERROR;
              HOSTDATA(shpnt)->abortion_complete++;
	    }

          if(TESTLO(SSTAT0, SELINGO))
	    /* ARBITRATION not won */
            aha152x_done(shpnt, DID_BUS_BUSY << 16);
	  else
	    /* ARBITRATION won, but SELECTION failed */
            aha152x_done(shpnt, DID_NO_CONNECT << 16);

	  return;
	}
    }

  /* enable interrupt, when target leaves current phase */
  phase = getphase(shpnt);
  if(!(phase & ~P_MASK))                                      /* "real" phase */
    SETPORT(SCSISIG, phase);
  SETPORT(SSTAT1, CLRPHASECHG);
  CURRENT_SC->SCp.phase =
    (CURRENT_SC->SCp.phase & ~((P_MASK|1)<<16)) | (phase << 16);

  /* information transfer phase */
  switch(phase)
    {
    case P_MSGO:                                               /* MESSAGE OUT */
      {
        int i, identify=0, abort=0;

#if defined(DEBUG_INTR) || defined(DEBUG_MSGO) || defined(DEBUG_PHASES)
        if(HOSTDATA(shpnt)->debug & (debug_intr|debug_msgo|debug_phases))
	  printk("MESSAGE OUT, ");
#endif
        if(MSGLEN==0)
	  {
            ADDMSG(MESSAGE_REJECT);
#if defined(DEBUG_MSGO)
            if(HOSTDATA(shpnt)->debug & debug_msgo)
              printk("unexpected MSGO; rejecting, ");
#endif
	  }
        
        
        CLRBITS(SXFRCTL0, ENDMA);
        
        SETPORT(SIMODE0, 0);
        SETPORT(SIMODE1, ENPHASEMIS|ENREQINIT|ENBUSFREE);
        
        /* wait for data latch to become ready or a phase change */
        while(TESTLO(DMASTAT, INTSTAT))
          barrier();
        
#if defined(DEBUG_MSGO)
        if(HOSTDATA(shpnt)->debug & debug_msgo)
          {
            int i;
            
            printk("messages (");
            for(i=0; i<MSGLEN; i+=print_msg(&MSG(i)), printk(" "))
              ;
            printk("), ");
	    }
#endif
        
        for(i=0; i<MSGLEN && TESTLO(SSTAT1, PHASEMIS); i++)
	    {
#if defined(DEBUG_MSGO)
            if(HOSTDATA(shpnt)->debug & debug_msgo)
              printk("%x ", MSG(i));
#endif
            if(i==MSGLEN-1)
              {
                /* Leave MESSAGE OUT after transfer */
                SETPORT(SSTAT1, CLRATNO);
	    }
	  
            SETPORT(SCSIDAT, MSG(i));

            make_acklow(shpnt);
            getphase(shpnt);

            if(MSG(i)==IDENTIFY(HOSTDATA(shpnt)->reconnect,CURRENT_SC->lun))
              identify++;

            if(MSG(i)==ABORT)
              abort++;

          }

        MSGLEN=0;

        if(identify)
          CURRENT_SC->SCp.phase |= sent_ident;

        if(abort)
	  {
	    /* revive abort(); abort() enables interrupts */
            HOSTDATA(shpnt)->abort_result=SCSI_ABORT_SUCCESS;
            HOSTDATA(shpnt)->abortion_complete++;

            CURRENT_SC->SCp.phase &= ~(P_MASK<<16);

	    /* exit */
            SETBITS(DMACNTRL0, INTEN);
#if defined(DEBUG_RACE)
	    leave_driver("(ABORT) intr");
#endif
            aha152x_done(shpnt, DID_ABORT<<16);
	    return;
	  }
      }
      break;

    case P_CMD:                                          /* COMMAND phase */
#if defined(DEBUG_INTR) || defined(DEBUG_CMD) || defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & (debug_intr|debug_cmd|debug_phases))
	printk("COMMAND, ");
#endif
      if(!(CURRENT_SC->SCp.sent_command))
	{
          int i;

          CLRBITS(SXFRCTL0, ENDMA);

          SETPORT(SIMODE0, 0);
          SETPORT(SIMODE1, ENPHASEMIS|ENREQINIT|ENBUSFREE);
  
          /* wait for data latch to become ready or a phase change */
          while(TESTLO(DMASTAT, INTSTAT))
            barrier();
  
          for(i=0; i<CURRENT_SC->cmd_len && TESTLO(SSTAT1, PHASEMIS); i++)
	  {
              SETPORT(SCSIDAT, CURRENT_SC->cmnd[i]);

              make_acklow(shpnt);
              getphase(shpnt);
	  }

          if(i<CURRENT_SC->cmd_len && TESTHI(SSTAT1, PHASEMIS))
            aha152x_panic(shpnt, "target left COMMAND");

          CURRENT_SC->SCp.sent_command++;
	}
      else
        aha152x_panic(shpnt, "Nothing to send while in COMMAND");
      break;

    case P_MSGI:                                          /* MESSAGE IN phase */
      {
        int start_sync=0;
        
#if defined(DEBUG_INTR) || defined(DEBUG_MSGI) || defined(DEBUG_PHASES)
        if(HOSTDATA(shpnt)->debug & (debug_intr|debug_msgi|debug_phases))
	printk("MESSAGE IN, ");
#endif
        SETPORT(SXFRCTL0, CH1);

        SETPORT(SIMODE0, 0);
        SETPORT(SIMODE1, ENBUSFREE);
  
        while(phase == P_MSGI) 
	{
            CURRENT_SC->SCp.Message = GETPORT(SCSIDAT);
            switch(CURRENT_SC->SCp.Message)
	    {
	    case DISCONNECT:
#if defined(DEBUG_MSGI) || defined(DEBUG_PHASES)
        	if(HOSTDATA(shpnt)->debug & (debug_msgi|debug_phases))
		printk("target disconnected, ");
#endif
        	CURRENT_SC->SCp.Message = 0;
        	CURRENT_SC->SCp.phase   |= disconnected;
        	if(!HOSTDATA(shpnt)->reconnect)
        	  aha152x_panic(shpnt, "target was not allowed to disconnect");
	      break;
	
	    case COMMAND_COMPLETE:
#if defined(DEBUG_MSGI) || defined(DEBUG_PHASES)
        	if(HOSTDATA(shpnt)->debug & (debug_msgi|debug_phases))
        	  printk("inbound message (COMMAND COMPLETE), ");
#endif
	      done++;
	      break;

	    case MESSAGE_REJECT:
        	if(CURRENT_SC->SCp.phase & in_sync)
        	  { 
        	    CURRENT_SC->SCp.phase &= ~in_sync;
        	    SYNCRATE=0x80;
        	    printk("synchronous rejected, ");
        	  }
        	else
        	  printk("inbound message (MESSAGE REJECT), ");
#if defined(DEBUG_MSGI)
        	if(HOSTDATA(shpnt)->debug & debug_msgi)
        	  printk("inbound message (MESSAGE REJECT), ");
#endif
	      break;

	    case SAVE_POINTERS:
#if defined(DEBUG_MSGI)
        	if(HOSTDATA(shpnt)->debug & debug_msgi)
        	  printk("inbound message (SAVE DATA POINTERS), ");
#endif
	      break;

	    case EXTENDED_MESSAGE:
	      { 
        	  char buffer[16];
        	  int  i;

#if defined(DEBUG_MSGI)
        	  if(HOSTDATA(shpnt)->debug & debug_msgi)
        	    printk("inbound message (EXTENDED MESSAGE), ");
#endif
        	  make_acklow(shpnt);
        	  if(getphase(shpnt)!=P_MSGI)
		  break;
  
        	  buffer[0]=EXTENDED_MESSAGE;
        	  buffer[1]=GETPORT(SCSIDAT);
        	  
        	  for(i=0; i<buffer[1] &&
        	      (make_acklow(shpnt), getphase(shpnt)==P_MSGI); i++)
        	    buffer[2+i]=GETPORT(SCSIDAT);

#if defined(DEBUG_MSGI)
        	  if(HOSTDATA(shpnt)->debug & debug_msgi)
        	    print_msg(buffer);
#endif

        	  switch(buffer [2])
        	    {
        	    case EXTENDED_SDTR:
        	      {
        		long ticks;
        		
        		if(buffer[1]!=3)
        		  aha152x_panic(shpnt, "SDTR message length != 3");
        		
        		if(!HOSTDATA(shpnt)->synchronous)
		  break;

        		printk("inbound SDTR: "); print_msg(buffer);
        		
        		ticks=(buffer[3]*4+49)/50;

        		if(CURRENT_SC->SCp.phase & in_sync)
		  {
        		    /* we initiated SDTR */
        		    if(ticks>9 || buffer[4]<1 || buffer[4]>8)
        		      aha152x_panic(shpnt, "received SDTR invalid");
        		    
        		    SYNCRATE |= ((ticks-2)<<4) + buffer[4];
        		  }
        		else if(ticks<=9 && buffer[4]>=1)
        		  {
        		    if(buffer[4]>8)
        		      buffer[4]=8;
        		    
        		    ADDMSG(EXTENDED_MESSAGE);
        		    ADDMSG(3);
        		    ADDMSG(EXTENDED_SDTR);
        		    if(ticks<4)
        		      {
        			    ticks=4;
        			    ADDMSG(50);
        		      }
		      else
        		      ADDMSG(buffer[3]);
        		    
        		    ADDMSG(buffer[4]);
        		    
        		    printk("outbound SDTR: ");
                            print_msg(&MSG(MSGLEN-5));
        		    
        		    CURRENT_SC->SCp.phase |= in_sync;
        		    
                            SYNCRATE |= ((ticks-2)<<4) + buffer[4];
        		    
        		    start_sync++;
		  }
        		else
		  {
        		    /* requested SDTR is too slow, do it asynchronously */
        		    ADDMSG(MESSAGE_REJECT);
                            SYNCRATE = 0;
        		  } 
        		
        		SETPORT(SCSIRATE, SYNCRATE&0x7f);
        	      }
        	      break;
        	      
        	    case EXTENDED_MODIFY_DATA_POINTER:
        	    case EXTENDED_EXTENDED_IDENTIFY:
        	    case EXTENDED_WDTR:
        	    default:
        	      ADDMSG(MESSAGE_REJECT);
        	      break;
		  }
	      }
	      break;
       
	    default:
        	printk("unsupported inbound message %x, ", 
        	       CURRENT_SC->SCp.Message);
	      break;

	    }

            make_acklow(shpnt);
            phase=getphase(shpnt);
	} 

        if(start_sync)
          CURRENT_SC->SCp.phase |= in_sync;
        else
          CURRENT_SC->SCp.phase &= ~in_sync;
        
        if(MSGLEN>0)
          SETPORT(SCSISIG, P_MSGI|ATNO);
        
      /* clear SCSI fifo on BUSFREE */
      if(phase==P_BUSFREE)
	SETPORT(SXFRCTL0, CH1|CLRCH1);

        if(CURRENT_SC->SCp.phase & disconnected)
	{
	  save_flags(flags);
	  cli();
#if defined(DEBUG_QUEUES)
            if(HOSTDATA(shpnt)->debug & debug_queues)
	    printk("d+, ");
#endif
            append_SC(&DISCONNECTED_SC, CURRENT_SC);
            CURRENT_SC->SCp.phase |= 1<<16;
            CURRENT_SC = NULL;
	  restore_flags(flags);

            SETBITS(SCSISEQ, ENRESELI);

            SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
            SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);

            SETBITS(DMACNTRL0, INTEN);
	  return;
	}
      }
      break;

    case P_STATUS:                                         /* STATUS IN phase */
#if defined(DEBUG_STATUS) || defined(DEBUG_INTR) || defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & (debug_status|debug_intr|debug_phases))
	printk("STATUS, ");
#endif
      SETPORT(SXFRCTL0, CH1);

      SETPORT(SIMODE0, 0);
      SETPORT(SIMODE1, ENREQINIT|ENBUSFREE);

      if(TESTHI(SSTAT1, PHASEMIS))
	printk("aha152x: passing STATUS phase");
	
      CURRENT_SC->SCp.Status = GETPORT(SCSIBUS);
      make_acklow(shpnt);
      getphase(shpnt);

#if defined(DEBUG_STATUS)
      if(HOSTDATA(shpnt)->debug & debug_status)
      {
	printk("inbound status ");
          print_status(CURRENT_SC->SCp.Status);
	printk(", ");
      }
#endif
      break;

    case P_DATAI:                                            /* DATA IN phase */
      {
	int fifodata, data_count, done;

#if defined(DEBUG_DATAI) || defined(DEBUG_INTR) || defined(DEBUG_PHASES)
        if(HOSTDATA(shpnt)->debug & (debug_datai|debug_intr|debug_phases))
	  printk("DATA IN, ");
#endif

#if 0
	if(GETPORT(FIFOSTAT) || GETPORT(SSTAT2) & (SFULL|SFCNT))
	  printk("aha152x: P_DATAI: %d(%d) bytes left in FIFO, resetting\n",
		 GETPORT(FIFOSTAT), GETPORT(SSTAT2) & (SFULL|SFCNT));
#endif

	/* reset host fifo */
	SETPORT(DMACNTRL0, RSTFIFO);
	SETPORT(DMACNTRL0, RSTFIFO|ENDMA);

        SETPORT(SXFRCTL0, CH1|SCSIEN|DMAEN);

        SETPORT(SIMODE0, 0);
        SETPORT(SIMODE1, ENPHASEMIS|ENBUSFREE);

	/* done is set when the FIFO is empty after the target left DATA IN */
	done=0;
      
	/* while the target stays in DATA to transfer data */
        while (!done) 
	  {
#if defined(DEBUG_DATAI)
            if(HOSTDATA(shpnt)->debug & debug_datai)
	      printk("expecting data, ");
#endif
	    /* wait for PHASEMIS or full FIFO */
            while(TESTLO (DMASTAT, DFIFOFULL|INTSTAT))
              barrier();

#if defined(DEBUG_DATAI)
            if(HOSTDATA(shpnt)->debug & debug_datai)
              printk("ok, ");
#endif
            
            if(TESTHI(DMASTAT, DFIFOFULL))
	      fifodata=GETPORT(FIFOSTAT);
	    else
	      {
		/* wait for SCSI fifo to get empty */
        	while(TESTLO(SSTAT2, SEMPTY))
        	  barrier();

		/* rest of data in FIFO */
		fifodata=GETPORT(FIFOSTAT);
#if defined(DEBUG_DATAI)
        	if(HOSTDATA(shpnt)->debug & debug_datai)
		  printk("last transfer, ");
#endif
		done=1;
	      }
  
#if defined(DEBUG_DATAI)
            if(HOSTDATA(shpnt)->debug & debug_datai)
	      printk("fifodata=%d, ", fifodata);
#endif

            while(fifodata && CURRENT_SC->SCp.this_residual)
	      {
		data_count=fifodata;
  
		/* limit data transfer to size of first sg buffer */
        	if (data_count > CURRENT_SC->SCp.this_residual)
        	  data_count = CURRENT_SC->SCp.this_residual;
  
		fifodata -= data_count;

#if defined(DEBUG_DATAI)
        	if(HOSTDATA(shpnt)->debug & debug_datai)
		  printk("data_count=%d, ", data_count);
#endif
  
		if(data_count&1)
		  {
		    /* get a single byte in byte mode */
        	    SETBITS(DMACNTRL0, _8BIT);
        	    *CURRENT_SC->SCp.ptr++ = GETPORT(DATAPORT);
        	    CURRENT_SC->SCp.this_residual--;
		  }
		if(data_count>1)
		  {
        	    CLRBITS(DMACNTRL0, _8BIT);
		    data_count >>= 1; /* Number of words */
        	    insw(DATAPORT, CURRENT_SC->SCp.ptr, data_count);
#if defined(DEBUG_DATAI)
        	    if(HOSTDATA(shpnt)->debug & debug_datai)
        	      /* show what comes with the last transfer */
		      if(done)
			{
#ifdef 0
			  int           i;
			  unsigned char *data;
#endif
  
        		  printk("data on last transfer (%d bytes) ",
        			 2*data_count);
#ifdef 0
			  printk("data on last transfer (%d bytes: ",
				 2*data_count);
        		  data = (unsigned char *) CURRENT_SC->SCp.ptr;
        		  for(i=0; i<2*data_count; i++)
			    printk("%2x ", *data++);
			  printk("), ");
#endif
			}
#endif
        	    CURRENT_SC->SCp.ptr           += 2 * data_count;
        	    CURRENT_SC->SCp.this_residual -= 2 * data_count;
		  }
	      
		/* if this buffer is full and there are more buffers left */
        	if (!CURRENT_SC->SCp.this_residual &&
        	    CURRENT_SC->SCp.buffers_residual)
		  {
		    /* advance to next buffer */
        	    CURRENT_SC->SCp.buffers_residual--;
        	    CURRENT_SC->SCp.buffer++;
        	    CURRENT_SC->SCp.ptr =
        	      CURRENT_SC->SCp.buffer->address;
        	    CURRENT_SC->SCp.this_residual =
        	      CURRENT_SC->SCp.buffer->length;
		  } 
	      }
 
	    /*
	     * Fifo should be empty
	     */
	    if(fifodata>0)
	      {
		printk("aha152x: more data than expected (%d bytes)\n",
		       GETPORT(FIFOSTAT));
        	SETBITS(DMACNTRL0, _8BIT);
        	printk("aha152x: data (");
		while(fifodata--)
        	  printk("%2x ", GETPORT(DATAPORT));
		printk(")\n");
	      }

#if defined(DEBUG_DATAI)
            if(HOSTDATA(shpnt)->debug & debug_datai)
	      if(!fifodata)
		printk("fifo empty, ");
	      else
		printk("something left in fifo, ");
#endif
	  }

#if defined(DEBUG_DATAI)
        if((HOSTDATA(shpnt)->debug & debug_datai) &&
           (CURRENT_SC->SCp.buffers_residual ||
            CURRENT_SC->SCp.this_residual))
	  printk("left buffers (buffers=%d, bytes=%d), ",
        	 CURRENT_SC->SCp.buffers_residual, 
        	 CURRENT_SC->SCp.this_residual);
#endif
	/* transfer can be considered ended, when SCSIEN reads back zero */
	CLRBITS(SXFRCTL0, SCSIEN|DMAEN);
        while(TESTHI(SXFRCTL0, SCSIEN))
          barrier();
        CLRBITS(DMACNTRL0, ENDMA);

#if defined(DEBUG_DATAI) || defined(DEBUG_INTR)
        if(HOSTDATA(shpnt)->debug & (debug_datai|debug_intr))
	  printk("got %d bytes, ", GETSTCNT());
#endif

        CURRENT_SC->SCp.have_data_in++;
      }
      break;

    case P_DATAO:                                           /* DATA OUT phase */
      {
	int data_count;

#if defined(DEBUG_DATAO) || defined(DEBUG_INTR) || defined(DEBUG_PHASES)
        if(HOSTDATA(shpnt)->debug & (debug_datao|debug_intr|debug_phases))
	  printk("DATA OUT, ");
#endif
#if defined(DEBUG_DATAO)
        if(HOSTDATA(shpnt)->debug & debug_datao)
	  printk("got data to send (bytes=%d, buffers=%d), ",
        	 CURRENT_SC->SCp.this_residual,
        	 CURRENT_SC->SCp.buffers_residual);
#endif

        if(GETPORT(FIFOSTAT) || GETPORT(SSTAT2) & (SFULL|SFCNT))
	  {
            printk("%d(%d) left in FIFO, ",
        	   GETPORT(FIFOSTAT), GETPORT(SSTAT2) & (SFULL|SFCNT));
            aha152x_panic(shpnt, "FIFO should be empty");
	  }

        SETPORT(SXFRCTL0, CH1|CLRSTCNT|CLRCH1);
        SETPORT(SXFRCTL0, SCSIEN|DMAEN|CH1);
        
	SETPORT(DMACNTRL0, WRITE_READ|RSTFIFO);
	SETPORT(DMACNTRL0, ENDMA|WRITE_READ);

        SETPORT(SIMODE0, 0);
        SETPORT(SIMODE1, ENPHASEMIS|ENBUSFREE);

	/* while current buffer is not empty or
	   there are more buffers to transfer */
        while(TESTLO(SSTAT1, PHASEMIS) &&
              (CURRENT_SC->SCp.this_residual ||
               CURRENT_SC->SCp.buffers_residual))
	  {
#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
	      printk("sending data (left: bytes=%d, buffers=%d), waiting, ",
        	     CURRENT_SC->SCp.this_residual,
        	     CURRENT_SC->SCp.buffers_residual);
#endif
	    /* transfer rest of buffer, but max. 128 byte */
            data_count =
              CURRENT_SC->SCp.this_residual > 128 ?
              128 : CURRENT_SC->SCp.this_residual ;

#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
	      printk("data_count=%d, ", data_count);
#endif
  
	    if(data_count&1)
	      {
		/* put a single byte in byte mode */
        	SETBITS(DMACNTRL0, _8BIT);
        	SETPORT(DATAPORT, *CURRENT_SC->SCp.ptr++);
        	CURRENT_SC->SCp.this_residual--;
	      }
	    if(data_count>1)
	      {
        	CLRBITS(DMACNTRL0, _8BIT);
        	data_count >>= 1; /* number of words */
        	outsw(DATAPORT, CURRENT_SC->SCp.ptr, data_count);
        	CURRENT_SC->SCp.ptr           += 2 * data_count;
        	CURRENT_SC->SCp.this_residual -= 2 * data_count;
	      }

	    /* wait for FIFO to get empty */
            while(TESTLO(DMASTAT, DFIFOEMP|INTSTAT))
              barrier();

#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
	      printk("fifo (%d bytes), transfered (%d bytes), ",
        	     GETPORT(FIFOSTAT), GETSTCNT());
#endif

	    /* if this buffer is empty and there are more buffers left */
            if (TESTLO(SSTAT1, PHASEMIS) &&
        	!CURRENT_SC->SCp.this_residual &&
        	CURRENT_SC->SCp.buffers_residual)
	      {
		 /* advance to next buffer */
        	CURRENT_SC->SCp.buffers_residual--;
        	CURRENT_SC->SCp.buffer++;
        	CURRENT_SC->SCp.ptr =
        	  CURRENT_SC->SCp.buffer->address;
        	CURRENT_SC->SCp.this_residual =
        	  CURRENT_SC->SCp.buffer->length;
	      }
	  }

        if (CURRENT_SC->SCp.this_residual || CURRENT_SC->SCp.buffers_residual)
	  {
	    /* target leaves DATA OUT for an other phase
	       (perhaps disconnect) */

	    /* data in fifos has to be resend */
	    data_count = GETPORT(SSTAT2) & (SFULL|SFCNT);

	    data_count += GETPORT(FIFOSTAT) ;
            CURRENT_SC->SCp.ptr           -= data_count;
            CURRENT_SC->SCp.this_residual += data_count;
#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
              printk("left data (bytes=%d, buffers=%d), fifos (bytes=%d), "
        	     "transfer incomplete, resetting fifo, ",
        	     CURRENT_SC->SCp.this_residual,
        	     CURRENT_SC->SCp.buffers_residual,
        	     data_count);
#endif
	    SETPORT(DMACNTRL0, WRITE_READ|RSTFIFO);
            CLRBITS(SXFRCTL0, SCSIEN|DMAEN);
	    CLRBITS(DMACNTRL0, ENDMA);
	  }
	else
	  {
#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
	      printk("waiting for SCSI fifo to get empty, ");
#endif
	    /* wait for SCSI fifo to get empty */
            while(TESTLO(SSTAT2, SEMPTY))
              barrier();
#if defined(DEBUG_DATAO)
            if(HOSTDATA(shpnt)->debug & debug_datao)
	      printk("ok, left data (bytes=%d, buffers=%d) ",
        	     CURRENT_SC->SCp.this_residual,
        	     CURRENT_SC->SCp.buffers_residual);
#endif
	    CLRBITS(SXFRCTL0, SCSIEN|DMAEN);

	    /* transfer can be considered ended, when SCSIEN reads back zero */
            while(TESTHI(SXFRCTL0, SCSIEN))
              barrier();

	    CLRBITS(DMACNTRL0, ENDMA);
	  }

#if defined(DEBUG_DATAO) || defined(DEBUG_INTR)
        if(HOSTDATA(shpnt)->debug & (debug_datao|debug_intr))
          printk("sent %d data bytes, ", GETSTCNT());
#endif
      }
      break;

    case P_BUSFREE:                                                /* BUSFREE */
#if defined(DEBUG_RACE)
      leave_driver("(BUSFREE) intr");
#endif
#if defined(DEBUG_PHASES)
      if(HOSTDATA(shpnt)->debug & debug_phases)
	printk("unexpected BUS FREE, ");
#endif
      CURRENT_SC->SCp.phase &= ~(P_MASK<<16);

      aha152x_done(shpnt, DID_ERROR << 16);         /* Don't know any better */
      return;
      break;

    case P_PARITY:                              /* parity error in DATA phase */
#if defined(DEBUG_RACE)
      leave_driver("(DID_PARITY) intr");
#endif
      printk("PARITY error in DATA phase, ");

      CURRENT_SC->SCp.phase &= ~(P_MASK<<16);

      SETBITS(DMACNTRL0, INTEN);
      aha152x_done(shpnt, DID_PARITY << 16);
      return;
      break;

    default:
      printk("aha152x: unexpected phase\n");
      break;
    }

  if(done)
    {
#if defined(DEBUG_INTR)
      if(HOSTDATA(shpnt)->debug & debug_intr)
	printk("command done.\n");
#endif
#if defined(DEBUG_RACE)
      leave_driver("(done) intr");
#endif

      SETPORT(SIMODE0, DISCONNECTED_SC ? ENSELDI : 0);
      SETPORT(SIMODE1, ISSUE_SC ? ENBUSFREE : 0);
      SETPORT(SCSISEQ, DISCONNECTED_SC ? ENRESELI : 0);
      
      SETBITS(DMACNTRL0, INTEN);
      
      aha152x_done(shpnt,
        	   (CURRENT_SC->SCp.Status  & 0xff)
        	   | ((CURRENT_SC->SCp.Message & 0xff) << 8)
        	   | (DID_OK << 16));

#if defined(DEBUG_RACE)
      printk("done returned (DID_OK: Status=%x; Message=%x).\n",
             CURRENT_SC->SCp.Status, CURRENT_SC->SCp.Message);
#endif
      return;
    }

  if(CURRENT_SC)
    CURRENT_SC->SCp.phase |= 1<<16 ;

  SETPORT(SIMODE0, 0);
  SETPORT(SIMODE1, ENPHASEMIS|ENBUSFREE);
#if defined(DEBUG_INTR)
  if(HOSTDATA(shpnt)->debug & debug_intr)
    disp_enintr(shpnt);
#endif
#if defined(DEBUG_RACE)
  leave_driver("(PHASEEND) intr");
#endif

  SETBITS(DMACNTRL0, INTEN);
  return;
}

/* 
 * Dump the current driver status and panic...
 */
static void aha152x_panic(struct Scsi_Host *shpnt, char *msg)
{
  printk("\naha152x: %s\n", msg);
  show_queues(shpnt);
  panic("aha152x panic");
}

/*
 * Display registers of AIC-6260
 */
static void disp_ports(struct Scsi_Host *shpnt)
{
#ifdef DEBUG_AHA152X
  int s;

#ifdef SKIP_PORTS
  if(HOSTDATA(shpnt)->debug & debug_skipports)
	return;
#endif

  printk("\n%s: ", CURRENT_SC ? "on bus" : "waiting");

  s=GETPORT(SCSISEQ);
  printk("SCSISEQ (");
  if(s & TEMODEO)     printk("TARGET MODE ");
  if(s & ENSELO)      printk("SELO ");
  if(s & ENSELI)      printk("SELI ");
  if(s & ENRESELI)    printk("RESELI ");
  if(s & ENAUTOATNO)  printk("AUTOATNO ");
  if(s & ENAUTOATNI)  printk("AUTOATNI ");
  if(s & ENAUTOATNP)  printk("AUTOATNP ");
  if(s & SCSIRSTO)    printk("SCSIRSTO ");
  printk(");");

  printk(" SCSISIG (");
  s=GETPORT(SCSISIG);
  switch(s & P_MASK)
    {
    case P_DATAO:
      printk("DATA OUT");
      break;
    case P_DATAI:
      printk("DATA IN");
      break;
    case P_CMD:
      printk("COMMAND"); 
      break;
    case P_STATUS:
      printk("STATUS"); 
      break;
    case P_MSGO:
      printk("MESSAGE OUT");
      break;
    case P_MSGI:
      printk("MESSAGE IN");
      break;
    default:
      printk("*illegal*");
      break;
    }
  
  printk("); ");

  printk("INTSTAT (%s); ", TESTHI(DMASTAT, INTSTAT) ? "hi" : "lo");

  printk("SSTAT (");
  s=GETPORT(SSTAT0);
  if(s & TARGET)   printk("TARGET ");
  if(s & SELDO)    printk("SELDO ");
  if(s & SELDI)    printk("SELDI ");
  if(s & SELINGO)  printk("SELINGO ");
  if(s & SWRAP)    printk("SWRAP ");
  if(s & SDONE)    printk("SDONE ");
  if(s & SPIORDY)  printk("SPIORDY ");
  if(s & DMADONE)  printk("DMADONE ");

  s=GETPORT(SSTAT1);
  if(s & SELTO)     printk("SELTO ");
  if(s & ATNTARG)   printk("ATNTARG ");
  if(s & SCSIRSTI)  printk("SCSIRSTI ");
  if(s & PHASEMIS)  printk("PHASEMIS ");
  if(s & BUSFREE)   printk("BUSFREE ");
  if(s & SCSIPERR)  printk("SCSIPERR ");
  if(s & PHASECHG)  printk("PHASECHG ");
  if(s & REQINIT)   printk("REQINIT ");
  printk("); ");


  printk("SSTAT (");

  s=GETPORT(SSTAT0) & GETPORT(SIMODE0);

  if(s & TARGET)    printk("TARGET ");
  if(s & SELDO)     printk("SELDO ");
  if(s & SELDI)     printk("SELDI ");
  if(s & SELINGO)   printk("SELINGO ");
  if(s & SWRAP)     printk("SWRAP ");
  if(s & SDONE)     printk("SDONE ");
  if(s & SPIORDY)   printk("SPIORDY ");
  if(s & DMADONE)   printk("DMADONE ");

  s=GETPORT(SSTAT1) & GETPORT(SIMODE1);

  if(s & SELTO)     printk("SELTO ");
  if(s & ATNTARG)   printk("ATNTARG ");
  if(s & SCSIRSTI)  printk("SCSIRSTI ");
  if(s & PHASEMIS)  printk("PHASEMIS ");
  if(s & BUSFREE)   printk("BUSFREE ");
  if(s & SCSIPERR)  printk("SCSIPERR ");
  if(s & PHASECHG)  printk("PHASECHG ");
  if(s & REQINIT)   printk("REQINIT ");
  printk("); ");

  printk("SXFRCTL0 (");

  s=GETPORT(SXFRCTL0);
  if(s & SCSIEN)    printk("SCSIEN ");
  if(s & DMAEN)     printk("DMAEN ");
  if(s & CH1)       printk("CH1 ");
  if(s & CLRSTCNT)  printk("CLRSTCNT ");
  if(s & SPIOEN)    printk("SPIOEN ");
  if(s & CLRCH1)    printk("CLRCH1 ");
  printk("); ");

  printk("SIGNAL (");

  s=GETPORT(SCSISIG);
  if(s & ATNI)  printk("ATNI ");
  if(s & SELI)  printk("SELI ");
  if(s & BSYI)  printk("BSYI ");
  if(s & REQI)  printk("REQI ");
  if(s & ACKI)  printk("ACKI ");
  printk("); ");

  printk("SELID (%02x), ", GETPORT(SELID));

  printk("SSTAT2 (");

  s=GETPORT(SSTAT2);
  if(s & SOFFSET)  printk("SOFFSET ");
  if(s & SEMPTY)   printk("SEMPTY ");
  if(s & SFULL)    printk("SFULL ");
  printk("); SFCNT (%d); ", s & (SFULL|SFCNT));

  s=GETPORT(SSTAT3);
  printk("SCSICNT (%d), OFFCNT(%d), ", (s&0xf0)>>4, s&0x0f);
  
  printk("SSTAT4 (");
  s=GETPORT(SSTAT4);
  if(s & SYNCERR)   printk("SYNCERR ");
  if(s & FWERR)     printk("FWERR ");
  if(s & FRERR)     printk("FRERR ");
  printk("); ");

  printk("DMACNTRL0 (");
  s=GETPORT(DMACNTRL0);
  printk("%s ", s & _8BIT      ? "8BIT"  : "16BIT");
  printk("%s ", s & DMA        ? "DMA"   : "PIO"  );
  printk("%s ", s & WRITE_READ ? "WRITE" : "READ" );
  if(s & ENDMA)    printk("ENDMA ");
  if(s & INTEN)    printk("INTEN ");
  if(s & RSTFIFO)  printk("RSTFIFO ");
  if(s & SWINT)    printk("SWINT ");
  printk("); ");


#if 0
  printk("DMACNTRL1 (");

  s=GETPORT(DMACNTRL1);
  if(s & PWRDWN)    printk("PWRDN ");
  printk("); ");


  printk("STK (%d); ", s & 0xf);
  
#endif

  printk("DMASTAT (");
  s=GETPORT(DMASTAT);
  if(s & ATDONE)     printk("ATDONE ");
  if(s & WORDRDY)    printk("WORDRDY ");
  if(s & DFIFOFULL)  printk("DFIFOFULL ");
  if(s & DFIFOEMP)   printk("DFIFOEMP ");
  printk(")");

  printk("\n");
#endif
}

/*
 * display enabled interrupts
 */
static void disp_enintr(struct Scsi_Host *shpnt)
{
  int s;

  printk("enabled interrupts (");
  
  s=GETPORT(SIMODE0);
  if(s & ENSELDO)    printk("ENSELDO ");
  if(s & ENSELDI)    printk("ENSELDI ");
  if(s & ENSELINGO)  printk("ENSELINGO ");
  if(s & ENSWRAP)    printk("ENSWRAP ");
  if(s & ENSDONE)    printk("ENSDONE ");
  if(s & ENSPIORDY)  printk("ENSPIORDY ");
  if(s & ENDMADONE)  printk("ENDMADONE ");

  s=GETPORT(SIMODE1);
  if(s & ENSELTIMO)    printk("ENSELTIMO ");
  if(s & ENATNTARG)    printk("ENATNTARG ");
  if(s & ENPHASEMIS)   printk("ENPHASEMIS ");
  if(s & ENBUSFREE)    printk("ENBUSFREE ");
  if(s & ENSCSIPERR)   printk("ENSCSIPERR ");
  if(s & ENPHASECHG)   printk("ENPHASECHG ");
  if(s & ENREQINIT)    printk("ENREQINIT ");
  printk(")\n");
}

#if defined(DEBUG_RACE)

static const char *should_leave;
static int in_driver=0;

/*
 * Only one routine can be in the driver at once.
 */
static void enter_driver(const char *func)
{
  unsigned long flags;

  save_flags(flags);
  cli();
  printk("aha152x: entering %s() (%x)\n", func, jiffies);
  if(in_driver)
    {
      printk("%s should leave first.\n", should_leave);
      panic("aha152x: already in driver\n");
    }

  in_driver++;
  should_leave=func;
  restore_flags(flags);
}

static void leave_driver(const char *func)
{
  unsigned long flags;

  save_flags(flags);
  cli();
  printk("\naha152x: leaving %s() (%x)\n", func, jiffies);
  if(!in_driver)
    {
      printk("aha152x: %s already left.\n", should_leave);
      panic("aha152x: %s already left driver.\n");
    }

  in_driver--;
  should_leave=func;
  restore_flags(flags);
}
#endif

/*
 * Show the command data of a command
 */
static void show_command(Scsi_Cmnd *ptr)
{
  printk("0x%08x: target=%d; lun=%d; cmnd=(",
	 (unsigned int) ptr, ptr->target, ptr->lun);
  
  print_command(ptr->cmnd);

  printk("); residual=%d; buffers=%d; phase |",
	 ptr->SCp.this_residual, ptr->SCp.buffers_residual);

  if(ptr->SCp.phase & not_issued  )  printk("not issued|");
  if(ptr->SCp.phase & in_selection)  printk("in selection|");
  if(ptr->SCp.phase & disconnected)  printk("disconnected|");
  if(ptr->SCp.phase & aborted     )  printk("aborted|");
  if(ptr->SCp.phase & sent_ident  )  printk("send_ident|");
  if(ptr->SCp.phase & in_other)
    { 
      printk("; in other(");
      switch((ptr->SCp.phase >> 16) & P_MASK)
	{
	case P_DATAO:
	  printk("DATA OUT");
	  break;
	case P_DATAI:
	  printk("DATA IN");
	  break;
	case P_CMD:
	  printk("COMMAND");
	  break;
	case P_STATUS:
	  printk("STATUS");
	  break;
	case P_MSGO:
	  printk("MESSAGE OUT");
	  break;
	case P_MSGI:
	  printk("MESSAGE IN");
	  break;
	default: 
	  printk("*illegal*");
	  break;
	}
      printk(")");
      if(ptr->SCp.phase & (1<<16))
	printk("; phaseend");
    }
  printk("; next=0x%08x\n", (unsigned int) ptr->host_scribble);
}
 
/*
 * Dump the queued data
 */
static void show_queues(struct Scsi_Host *shpnt)
{
  unsigned long flags;
  Scsi_Cmnd *ptr;

  save_flags(flags);
  cli();
  printk("QUEUE STATUS:\nissue_SC:\n");
  for(ptr=ISSUE_SC; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
    show_command(ptr);

  printk("current_SC:\n");
  if(CURRENT_SC)
    show_command(CURRENT_SC);
  else
    printk("none\n");

  printk("disconnected_SC:\n");
  for(ptr=DISCONNECTED_SC; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
    show_command(ptr);

  disp_ports(shpnt);
  disp_enintr(shpnt);
  restore_flags(flags);
}

int aha152x_set_info(char *buffer, int length, struct Scsi_Host *shpnt)
{
  return(-ENOSYS);  /* Currently this is a no-op */
}

#undef SPRINTF
#define SPRINTF(args...) pos += sprintf(pos, ## args)

static int get_command(char *pos, Scsi_Cmnd *ptr)
{
  char *start = pos;
  int i;
  
  SPRINTF("0x%08x: target=%d; lun=%d; cmnd=(",
          (unsigned int) ptr, ptr->target, ptr->lun);
  
  for(i=0; i<COMMAND_SIZE(ptr->cmnd[0]); i++)
    SPRINTF("0x%02x", ptr->cmnd[i]);
  
  SPRINTF("); residual=%d; buffers=%d; phase |",
          ptr->SCp.this_residual, ptr->SCp.buffers_residual);
  
  if(ptr->SCp.phase & not_issued  )  SPRINTF("not issued|");
  if(ptr->SCp.phase & in_selection)  SPRINTF("in selection|");
  if(ptr->SCp.phase & disconnected)  SPRINTF("disconnected|");
  if(ptr->SCp.phase & aborted     )  SPRINTF("aborted|");
  if(ptr->SCp.phase & sent_ident  )  SPRINTF("send_ident|");
  if(ptr->SCp.phase & in_other)
    { 
      SPRINTF("; in other(");
      switch((ptr->SCp.phase >> 16) & P_MASK)
        {
        case P_DATAO:
          SPRINTF("DATA OUT");
          break;
        case P_DATAI:
          SPRINTF("DATA IN");
          break;
        case P_CMD:
          SPRINTF("COMMAND");
          break;
        case P_STATUS:
          SPRINTF("STATUS");
          break;
        case P_MSGO:
          SPRINTF("MESSAGE OUT");
          break;
        case P_MSGI:
          SPRINTF("MESSAGE IN");
          break;
        default: 
          SPRINTF("*illegal*");
          break;
        }
      SPRINTF(")");
      if(ptr->SCp.phase & (1<<16))
        SPRINTF("; phaseend");
    }
  SPRINTF("; next=0x%08x\n", (unsigned int) ptr->host_scribble);
  
  return(pos-start);
}

#undef SPRINTF
#define SPRINTF(args...) do { if(pos < buffer + length) pos += sprintf(pos, ## args); } while(0)

int aha152x_proc_info(
        	      char *buffer,
        	      char **start,
        	      off_t offset,
        	      int length,
        	      int hostno,
        	      int inout
        	      )
{
  int i;
  char *pos = buffer;
  Scsi_Device *scd;
  struct Scsi_Host *shpnt;
  unsigned long flags;
  Scsi_Cmnd *ptr;
  
  for(i=0, shpnt= (struct Scsi_Host *) NULL; i<IRQS; i++)
    if(aha152x_host[i] && aha152x_host[i]->host_no == hostno)
      shpnt=aha152x_host[i];
  
  if(!shpnt)
    return(-ESRCH);
  
  if(inout) /* Has data been written to the file ? */ 
    return(aha152x_set_info(buffer, length, shpnt));
  
  SPRINTF(AHA152X_REVID "\n");
  
  save_flags(flags);
  cli();
  
  SPRINTF("vital data:\nioports 0x%04x to 0x%04x\n",
          shpnt->io_port, shpnt->io_port+shpnt->n_io_port-1);
  SPRINTF("interrupt 0x%02x\n", shpnt->irq);
  SPRINTF("disconnection/reconnection %s\n", 
          HOSTDATA(shpnt)->reconnect ? "enabled" : "disabled");
  SPRINTF("parity checking %s\n", 
          HOSTDATA(shpnt)->parity ? "enabled" : "disabled");
  SPRINTF("synchronous transfers %s\n", 
          HOSTDATA(shpnt)->synchronous ? "enabled" : "disabled");
  SPRINTF("current queued %d commands\n",
          HOSTDATA(shpnt)->commands);
  
#if 0
  SPRINTF("synchronously operating targets (tick=%ld ns):\n",
          250000000/loops_per_sec);
  for(i=0; i<8; i++)
    if(HOSTDATA(shpnt)->syncrate[i]&0x7f)
      SPRINTF("target %d: period %dT/%ldns; req/ack offset %d\n",
              i,
              (((HOSTDATA(shpnt)->syncrate[i]&0x70)>>4)+2),
        	       (((HOSTDATA(shpnt)->syncrate[i]&0x70)>>4)+2)*
        		 250000000/loops_per_sec,
        	       HOSTDATA(shpnt)->syncrate[i]&0x0f);
#else
  SPRINTF("synchronously operating targets (tick=50 ns):\n");
  for(i=0; i<8; i++)
    if(HOSTDATA(shpnt)->syncrate[i]&0x7f)
      SPRINTF("target %d: period %dT/%dns; req/ack offset %d\n",
              i,
              (((HOSTDATA(shpnt)->syncrate[i]&0x70)>>4)+2),
              (((HOSTDATA(shpnt)->syncrate[i]&0x70)>>4)+2)*50,
              HOSTDATA(shpnt)->syncrate[i]&0x0f);
#endif
  
#ifdef DEBUG_AHA152X
#define PDEBUG(flags,txt) if(HOSTDATA(shpnt)->debug & flags) SPRINTF("(%s) ", txt);
  
  SPRINTF("enabled debugging options:\n");
  
  PDEBUG(debug_skipports, "skip ports");
  PDEBUG(debug_queue, "queue");
  PDEBUG(debug_intr, "interrupt");
  PDEBUG(debug_selection, "selection");
  PDEBUG(debug_msgo, "message out");
  PDEBUG(debug_msgi, "message in");
  PDEBUG(debug_status, "status");
  PDEBUG(debug_cmd, "command");
  PDEBUG(debug_datai, "data in");
  PDEBUG(debug_datao, "data out");
  PDEBUG(debug_abort, "abort");
  PDEBUG(debug_done, "done");
  PDEBUG(debug_biosparam, "bios parameters");
  PDEBUG(debug_phases, "phases");
  PDEBUG(debug_queues, "queues");
  PDEBUG(debug_reset, "reset");
  
  SPRINTF("\n");
#endif
  
  SPRINTF("queue status:\nnot yet issued commands:\n");
  for(ptr=ISSUE_SC; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
    pos += get_command(pos, ptr);
  
  if(CURRENT_SC)
    {
      SPRINTF("current command:\n");
      pos += get_command(pos, CURRENT_SC);
    }
  
  SPRINTF("disconnected commands:\n");
  for(ptr=DISCONNECTED_SC; ptr; ptr = (Scsi_Cmnd *) ptr->host_scribble)
    pos += get_command(pos, ptr);
  
  restore_flags(flags);
  
  scd = scsi_devices;
  
  SPRINTF("Attached devices: %s\n", (scd)?"":"none");
  
  while (scd) {
    if (scd->host == shpnt) {
      
      SPRINTF("Channel: %02d Id: %02d Lun: %02d\n  Vendor: ",
              scd->channel, scd->id, scd->lun);
      for (i=0; i<8; i++) {
        if (scd->vendor[i] >= 0x20)
          SPRINTF("%c", scd->vendor[i]);
        else
          SPRINTF(" ");
      }
      SPRINTF(" Model: ");
      for (i = 0; i < 16; i++) {
        if (scd->model[i] >= 0x20)
          SPRINTF("%c", scd->model[i]);
        else
          SPRINTF(" ");
      }
      SPRINTF(" Rev: ");
      for (i = 0; i < 4; i++) {
        if (scd->rev[i] >= 0x20)
          SPRINTF("%c", scd->rev[i]);
        else
          SPRINTF(" ");
      }
      SPRINTF("\n");
      
      SPRINTF("  Type:   %d ", scd->type);
      SPRINTF("               ANSI SCSI revision: %02x",
              (scd->scsi_level < 3)?1:2);
      
      if (scd->scsi_level == 2)
        SPRINTF(" CCS\n");
      else
        SPRINTF("\n");
    }
    scd = scd->next;
  }
  
  *start=buffer+offset;
  if (pos - buffer < offset)
    return 0;
  else if (pos - buffer - offset < length)
    return pos - buffer - offset;
  else
    return length;
}

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = AHA152X;

#include "scsi_module.c"
#endif
