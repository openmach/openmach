/*
 *  This file is in2000.c, written and
 *  Copyright (C) 1993  Brad McLean
 *	Last edit 1/19/95 TZ
 * Disclaimer:
 * Note:  This is ugly.  I know it, I wrote it, but my whole
 * focus was on getting the damn thing up and out quickly.
 * Future stuff that would be nice:  Command chaining, and
 * a local queue of commands would speed stuff up considerably.
 * Disconnection needs some supporting code.  All of this
 * is beyond the scope of what I wanted to address, but if you
 * have time and patience, more power to you.
 * Also, there are some constants scattered throughout that
 * should have defines, and I should have built functions to
 * address the registers on the WD chip.
 * Oh well, I'm out of time for this project.
 * The one good thing to be said is that you can use the card.
 */

/*
 * This module was updated by Shaun Savage first on 5-13-93
 * At that time the write was fixed, irq detection, and some
 * timing stuff.  since that time other problems were fixed.
 * On 7-20-93 this file was updated for patch level 11
 * There are still problems with it but it work on 95% of
 * the machines.  There are still problems with it working with
 * IDE drives, as swap drive and HD that support reselection.
 * But for most people it will work.
 */
/* More changes by Bill Earnest, wde@aluxpo.att.com
 * through 4/07/94. Includes rewrites of FIFO routines,
 * length-limited commands to make swap partitions work.
 * Merged the changes released by Larry Doolittle, based on input
 * from Jon Luckey, Roger Sunshine, John Shifflett. The FAST_FIFO
 * doesn't work for me. Scatter-gather code from Eric. The change to
 * an IF stmt. in the interrupt routine finally made it stable.
 * Limiting swap request size patch to ll_rw_blk.c not needed now.
 * Please ignore the clutter of debug stmts., pretty can come later.
 */
/* Merged code from Matt Postiff improving the auto-sense validation
 * for all I/O addresses. Some reports of problems still come in, but
 * have been unable to reproduce or localize the cause. Some are from
 * LUN > 0 problems, but that is not host specific. Now 6/6/94.
 */
/* Changes for 1.1.28 kernel made 7/19/94, code not affected. (WDE)
 */
/* Changes for 1.1.43+ kernels made 8/25/94, code added to check for
 * new BIOS version, derived by jshiffle@netcom.com. (WDE)
 *
 * 1/7/95 Fix from Peter Lu (swift@world.std.com) for datalen vs. dataptr
 * logic, much more stable under load.
 *
 * 1/19/95 (zerucha@shell.portal.com) Added module and biosparam support for
 * larger SCSI hard drives (untested).
 */

#ifdef MODULE
#include <linux/module.h>
#endif

#include <linux/kernel.h>
#include <linux/head.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <asm/dma.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"

#include "in2000.h"
#include<linux/stat.h>

struct proc_dir_entry proc_scsi_in2000 = {
    PROC_SCSI_IN2000, 6, "in2000",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};

/*#define FAST_FIFO_IO*/

/*#define DEBUG*/
#ifdef DEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

/* These functions are based on include/asm/io.h */
#ifndef inw
inline static unsigned short inw( unsigned short port )
{
   unsigned short _v;
   
   __asm__ volatile ("inw %1,%0"
		     :"=a" (_v):"d" ((unsigned short) port));
   return _v;
}
#endif

#ifndef outw
inline static void outw( unsigned short value, unsigned short port )
{
   __asm__ volatile ("outw %0,%1"
			: /* no outputs */
			:"a" ((unsigned short) value),
			"d" ((unsigned short) port));
}
#endif

/* These functions are lifted from drivers/block/hd.c */

#define port_read(port,buf,nr) \
__asm__("cld;rep;insw": :"d" (port),"D" (buf),"c" (nr):"cx","di")

#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw": :"d" (port),"S" (buf),"c" (nr):"cx","si")

static unsigned int base;
static unsigned int ficmsk;
static unsigned char irq_level;
static int in2000_datalen;
static unsigned int in2000_nsegment;
static unsigned int in2000_current_segment;
static unsigned short *in2000_dataptr;
static char	in2000_datawrite;
static struct scatterlist * in2000_scatter;
static Scsi_Cmnd *in2000_SCptr = 0;

static void (*in2000_done)(Scsi_Cmnd *);

static int in2000_test_port(int index)
{
    static const int *bios_tab[] = {
	(int *) 0xc8000, (int *) 0xd0000, (int *) 0xd8000 };
    int	i;
    char    tmp;

    tmp = inb(INFLED);
	/* First, see if the DIP switch values are valid */
	/* The test of B7 may fail on some early boards, mine works. */
    if ( ((~tmp & 0x3) != index ) || (tmp & 0x80) || !(tmp & 0x4) )
    	return 0;
    printk("IN-2000 probe got dip setting of %02X\n", tmp);
    tmp = inb(INVERS);
/* Add some extra sanity checks here */
    for(i=0; i < 3; i++)
	if(*(bios_tab[i]+0x04) == 0x41564f4e ||
		*(bios_tab[i]+0xc) == 0x61776c41) {
	  printk("IN-2000 probe found hdw. vers. %02x, BIOS at %06x\n",
		tmp, (unsigned int)bios_tab[i]);
		return 1;
	}
    printk("in2000 BIOS not found.\n");
    return 0;
}


/*
 * retrieve the current transaction counter from the WD
 */

static unsigned in2000_txcnt(void)
{
    unsigned total=0;

    if(inb(INSTAT) & 0x20) return 0xffffff;	/* not readable now */
    outb(TXCNTH,INSTAT);	/* then autoincrement */
    total =  (inb(INDATA) & 0xff) << 16;
    outb(TXCNTM,INSTAT);
    total += (inb(INDATA) & 0xff) << 8;
    outb(TXCNTL,INSTAT);
    total += (inb(INDATA) & 0xff);
    return total;
}

/*
 * Note: the FIFO is screwy, and has a counter granularity of 16 bytes, so
 * we have to reconcile the FIFO counter, the transaction byte count from the
 * WD chip, and of course, our desired transaction size.  It may look strange,
 * and could probably use improvement, but it works, for now.
 */

static void in2000_fifo_out(void)	/* uses FIFOCNTR */
{
    unsigned count, infcnt, txcnt;

    infcnt = inb(INFCNT)& 0xfe;	/* FIFO counter */
    do {
	txcnt = in2000_txcnt();
/*DEB(printk("FIw:%d %02x %d\n", in2000_datalen, infcnt, txcnt));*/
	count = (infcnt << 3) - 32;	/* don't fill completely */
	if ( count > in2000_datalen )
	    count = in2000_datalen;	/* limit to actual data on hand */
	count >>= 1;		/* Words, not bytes */
#ifdef FAST_FIFO_IO
	if ( count ) {
		port_write(INFIFO, in2000_dataptr, count);
		in2000_datalen -= (count<<1);
	}
#else
	while ( count-- )
	    {
		outw(*in2000_dataptr++, INFIFO);
		in2000_datalen -= 2;
	    }
#endif
    } while((in2000_datalen > 0) && ((infcnt = (inb(INFCNT)) & 0xfe) >= 0x20) );
    /* If scatter-gather, go on to next segment */
    if( !in2000_datalen && ++in2000_current_segment < in2000_nsegment)
      {
      in2000_scatter++;
      in2000_datalen = in2000_scatter->length;
      in2000_dataptr = (unsigned short*)in2000_scatter->address;
      }
    if ( in2000_datalen <= 0 )
    {
	ficmsk = 0;
	count = 32;	/* Always says to use this much flush */
	while ( count-- )
	    outw(0, INFIFO);
	outb(2, ININTR); /* Mask FIFO Interrupts when done */
    }
}

static void in2000_fifo_in(void)	/* uses FIFOCNTR */
{
    unsigned fic, count, count2;

    count = inb(INFCNT) & 0xe1;
    do{
	count2 = count;
	count = (fic = inb(INFCNT)) & 0xe1;
    } while ( count != count2 );
DEB(printk("FIir:%d %02x %08x\n", in2000_datalen,fic,(unsigned int )in2000_dataptr));
    do {
	count2 = in2000_txcnt();	/* bytes yet to come over SCSI bus */
DEB(printk("FIr:%d %02x %08x %08x\n", in2000_datalen,fic,count2,(unsigned int)in2000_dataptr));
	if(count2 > 65536) count2 = 0;
	if(fic > 128) count = 1024;
	  else if(fic > 64) count = 512;
	    else if (fic > 32) count = 256;
	      else if ( count2 < in2000_datalen ) /* if drive has < what we want */
		count = in2000_datalen - count2;	/* FIFO has the rest */
	if ( count > in2000_datalen )	/* count2 is lesser of FIFO & rqst */
	    count2 = in2000_datalen >> 1;	/* converted to word count */
	else
	    count2 = count >> 1;
	count >>= 1;		/* also to words */
	count -= count2;	/* extra left over in FIFO */
#ifdef FAST_FIFO_IO
	if ( count2 ) {
		port_read(INFIFO, in2000_dataptr, count2);
		in2000_datalen -= (count2<<1);
	}
#else
	while ( count2-- )
	{
	    *in2000_dataptr++ = inw(INFIFO);
	    in2000_datalen -=2;
	}
#endif
    } while((in2000_datalen > 0) && (fic = inb(INFCNT)) );
DEB(printk("FIer:%d %02x %08x\n", in2000_datalen,fic,(unsigned int )in2000_dataptr));
/*    while ( count-- )
    	inw(INFIFO);*/	/* Throw away some extra stuff */
    if( !in2000_datalen && ++in2000_current_segment < in2000_nsegment)
      {
      in2000_scatter++;
      in2000_datalen = in2000_scatter->length;
      in2000_dataptr = (unsigned short*)in2000_scatter->address;
      }
    if ( ! in2000_datalen ){
	outb(2, ININTR); /* Mask FIFO Interrupts when done */
	ficmsk = 0;}
}

static void in2000_intr_handle(int irq, struct pt_regs *regs)
{
    int result=0;
    unsigned int count,auxstatus,scsistatus,cmdphase,scsibyte;
    int action=0;
    Scsi_Cmnd *SCptr;

  DEB(printk("INT:%d %02x %08x\n", in2000_datalen, inb(INFCNT),(unsigned int)in2000_dataptr));

    if (( (ficmsk & (count = inb(INFCNT))) == 0xfe ) ||
		( (inb(INSTAT) & 0x8c) == 0x80))
	{	/* FIFO interrupt or WD interrupt */
   	auxstatus = inb(INSTAT);	/* need to save now */
   	outb(SCSIST,INSTAT);
   	scsistatus = inb(INDATA); /* This clears the WD intrpt bit */
   	outb(TARGETU,INSTAT);	/* then autoincrement */
   	scsibyte = inb(INDATA);	/* Get the scsi status byte */
   	outb(CMDPHAS,INSTAT);
   	cmdphase = inb(INDATA);
   	DEB(printk("(int2000:%02x %02x %02x %02x %02x)\n",count,auxstatus,
		scsistatus,cmdphase,scsibyte));

	/* Why do we assume that we need to send more data here??? ERY */
   	if ( in2000_datalen )	/* data xfer pending */
   	    {
   	    if ( in2000_dataptr == NULL )
		printk("int2000: dataptr=NULL datalen=%d\n",
			in2000_datalen);
	    else if ( in2000_datawrite )
		in2000_fifo_out();
	    else
		in2000_fifo_in();
   	    } 
	if ( (auxstatus & 0x8c) == 0x80 )
	    {	/* There is a WD Chip interrupt & register read good */
	    outb(2,ININTR);	/* Disable fifo interrupts */
	    ficmsk = 0;
	    result = DID_OK << 16;
	    /* 16=Select & transfer complete, 85=got disconnect */
	    if ((scsistatus != 0x16) && (scsistatus != 0x85)
		&& (scsistatus != 0x42)){
/*	   	printk("(WDi2000:%02x %02x %02x %02x %02x)\n",count,auxstatus,
			scsistatus,cmdphase,scsibyte);*/
/*		printk("QDAT:%d %08x %02x\n",
		in2000_datalen,(unsigned int)in2000_dataptr,ficmsk);*/
		;
	    }
		switch ( scsistatus & 0xf0 )
		    {
		    case	0x00:	/* Card Reset Completed */
			action = 3;
			break;
		    case	0x10:	/* Successful Command Completion */
			if ( scsistatus & 0x8 )
		    	    action = 1;
			break;
		    case	0x20:	/* Command Paused or Aborted */
			if ( (scsistatus & 0x8) )
		    	    action = 1;
			else if ( (scsistatus & 7) < 2 )
		    		action = 2;
			     else
		    		result = DID_ABORT << 16;
			break;
		    case	0x40:	/* Terminated early */
			if ( scsistatus & 0x8 )
		     	    action = 1;
			else if ( (scsistatus & 7) > 2 )
		     		action = 2;
			     else
		    		result = DID_TIME_OUT << 16;
			break;
		    case	0x80:	/* Service Required from SCSI bus */
			if ( scsistatus & 0x8 )
			    action = 1;
			else
			    action = 2;
			break;
		    }		/* end switch(scsistatus) */
		outb(0,INFLED);
		switch ( action )
		    {
		    case	0x02:	/* Issue an abort */
			outb(COMMAND,INSTAT);
			outb(1,INDATA); 	/* ABORT COMMAND */
			result = DID_ABORT << 16;
		    case	0x00:	/* Basically all done */
			if ( ! in2000_SCptr )
			    return;
			in2000_SCptr->result = result | scsibyte;
			SCptr = in2000_SCptr;
			in2000_SCptr = 0;
			if ( in2000_done )
		     	    (*in2000_done)(SCptr);
			break;
		    case	0x01:	/* We need to reissue a command */
			outb(CMDPHAS,INSTAT);
			switch ( scsistatus & 7 )
			    {
			    case	0:	/* Data out phase */
		    	    case	1:	/* Data in phase */
		    	    case	4:	/* Unspec info out phase */
		    	    case	5:	/* Unspec info in phase */
		    	    case	6:	/* Message in phase */
		    	    case	7:	/* Message in phase */
				outb(0x41,INDATA); /* rdy to disconn */
				break;
		    	    case	2:	/* command phase */
				outb(0x30,INDATA); /* rdy to send cmd bytes */
				break;
		    	    case	3:	/* status phase */
				outb(0x45,INDATA); /* To go to status phase,*/
				outb(TXCNTH,INSTAT); /* elim. data, autoinc */
				outb(0,INDATA);
				outb(0,INDATA);
				outb(0,INDATA);
				in2000_datalen = 0;
				in2000_dataptr = 0;
				break;
			    }	/* end switch(scsistatus) */
			outb(COMMAND,INSTAT);
			outb(8,INDATA);	 /* RESTART THE COMMAND */
			break;
		    case	0x03:	/* Finish up a Card Reset */
			outb(TIMEOUT,INSTAT);	/* I got these values */
						/* by reverse Engineering */
			outb(IN2000_TMOUT,INDATA); /* the Always' bios. */
			outb(CONTROL,INSTAT);
			outb(0,INDATA);
			outb(SYNCTXR,INSTAT);
			outb(0x40,INDATA);	/* async, 4 cyc xfer per. */
			break;
		    }		/* end switch(action) */
	    }			/* end if auxstatus for WD int */
	}			/* end while intrpt active */
}

int in2000_queuecommand(Scsi_Cmnd * SCpnt, void (*done)(Scsi_Cmnd *))
{
    unchar direction;
    unchar *cmd = (unchar *) SCpnt->cmnd;
    unchar target = SCpnt->target;
    void *buff = SCpnt->request_buffer;
    unsigned long flags;
    int bufflen = SCpnt->request_bufflen;
    int timeout, size, loop;
    int i;

    /*
     * This SCSI command has no data phase, but unfortunately the mid-level
     * SCSI drivers ask for 256 bytes of data xfer.  Our card hangs if you
     * do this, so we protect against it here.  It would be nice if the mid-
     * level could be changed, but who knows if that would break other host
     * adapter drivers.
     */
    if ( *cmd == TEST_UNIT_READY )
	bufflen = 0;

    /*
     * What it looks like.  Boy did I get tired of reading its output.
     */
    if (*cmd == READ_10 || *cmd == WRITE_10) {
	i = xscsi2int((cmd+1));
    } else if (*cmd == READ_6 || *cmd == WRITE_6) {
	i = scsi2int((cmd+1));
    } else {
	i = -1;
    }
#ifdef DEBUG
    printk("in2000qcmd: pos %d len %d ", i, bufflen);
    printk("scsi cmd:");
    for (i = 0; i < SCpnt->cmd_len; i++) printk("%02x ", cmd[i]);
    printk("\n");
#endif
    direction = 1;	/* assume for most commands */
    if (*cmd == WRITE_10 || *cmd == WRITE_6)
	direction = 0;
    size = SCpnt->cmd_len;	/* CDB length */ 
    /*
     * Setup our current pointers
     * This is where you would allocate a control structure in a queue,
     * If you were going to upgrade this to do multiple issue.
     * Note that datalen and dataptr exist because we can change the
     * values during the course of the operation, while managing the
     * FIFO.
     * Note the nasty little first clause.  In theory, the mid-level
     * drivers should never hand us more than one command at a time,
     * but just in case someone gets cute in configuring the driver,
     * we'll protect them, although not very politely.
     */
    if ( in2000_SCptr )
    {
	printk("in2000_queue_command waiting for free command block!\n");
	while ( in2000_SCptr )
	    barrier();
    }
    for ( timeout = jiffies + 5; timeout > jiffies; )
    {
	if ( ! ( inb(INSTAT) & 0xb0 ) )
	{
	    timeout = 0;
	    break;
	}
	else
	{
	    inb(INSTAT);
	    outb(SCSIST,INSTAT);
	    inb(INDATA);
	    outb(TARGETU,INSTAT); 	/* then autoinc */
	    inb(INDATA);
	    inb(INDATA);
	}
    }
    if ( timeout )
    {
	printk("in2000_queue_command timeout!\n");
	SCpnt->result = DID_TIME_OUT << 16;
	(*done)(SCpnt);
	return 1;
    }
    /* Added for scatter-gather support */
    in2000_nsegment = SCpnt->use_sg;
    in2000_current_segment = 0;
    if(SCpnt->use_sg){
      in2000_scatter = (struct scatterlist *) buff;
      in2000_datalen = in2000_scatter->length;
      in2000_dataptr = (unsigned short*)in2000_scatter->address;
    } else {
      in2000_scatter = NULL;
      in2000_datalen = bufflen;
      in2000_dataptr = (unsigned short*) buff;
    };
    in2000_done = done;
    in2000_SCptr = SCpnt;
    /*
     * Write the CDB to the card, then the LUN, the length, and the target.
     */
    outb(TOTSECT, INSTAT);	/* start here then autoincrement */
    for ( loop=0; loop < size; loop++ )
	outb(cmd[loop],INDATA);
    outb(TARGETU,INSTAT);
    outb(SCpnt->lun & 7,INDATA);
    SCpnt->host_scribble = NULL;
    outb(TXCNTH,INSTAT);	/* then autoincrement */
    outb(bufflen>>16,INDATA);
    outb(bufflen>>8,INDATA);
    outb(bufflen,INDATA);
    outb(target&7,INDATA);
    /*
     * Set up the FIFO
     */
    save_flags(flags);
    cli();		/* so FIFO init waits till WD set */
    outb(0,INFRST);
    if ( direction == 1 )
    {
	in2000_datawrite = 0;
	outb(0,INFWRT);
    }
    else
    {
	in2000_datawrite = 1;
	for ( loop=16; --loop; ) /* preload the outgoing fifo */
	    {
		outw(*in2000_dataptr++,INFIFO);
		if(in2000_datalen > 0) in2000_datalen-=2;
	    }
    }
    ficmsk = 0xff;
    /*
     * Start it up
     */
    outb(CONTROL,INSTAT);	/* WD BUS Mode */
    outb(0x4C,INDATA);
    if ( in2000_datalen )		/* if data xfer cmd */
	outb(0,ININTR);		/* Enable FIFO intrpt some boards? */
    outb(COMMAND,INSTAT);
    outb(0,INNLED);
    outb(8,INDATA);		/* Select w/ATN & Transfer */
    restore_flags(flags);			/* let the intrpt rip */
    return 0;
}

static volatile int internal_done_flag = 0;
static volatile int internal_done_errcode = 0;

static void internal_done(Scsi_Cmnd * SCpnt)
{
    internal_done_errcode = SCpnt->result;
    ++internal_done_flag;
}

int in2000_command(Scsi_Cmnd * SCpnt)
{
    in2000_queuecommand(SCpnt, internal_done);

    while (!internal_done_flag);
    internal_done_flag = 0;
    return internal_done_errcode;
}

int in2000_detect(Scsi_Host_Template * tpnt)
{
/* Order chosen to reduce conflicts with some multi-port serial boards */
    int base_tab[] = { 0x220,0x200,0x110,0x100 };
    int int_tab[] = { 15,14,11,10 };
    struct Scsi_Host * shpnt;
    int loop, tmp;

    DEB(printk("in2000_detect: \n"));

    tpnt->proc_dir = &proc_scsi_in2000;

    for ( loop=0; loop < 4; loop++ )
    {
	base = base_tab[loop];
	if ( in2000_test_port(loop))  break;
    }
    if ( loop == 4 )
	return 0;

  /* Read the dip switch values again for miscellaneous checking and
     informative messages */
  tmp = inb(INFLED);

  /* Bit 2 tells us if interrupts are disabled */
  if ( (tmp & 0x4) == 0 ) {
    printk("The IN-2000 is not configured for interrupt operation\n");
    printk("Change the DIP switch settings to enable interrupt operation\n");
  }

  /* Bit 6 tells us about floppy controller */
  printk("IN-2000 probe found floppy controller on IN-2000 ");
  if ( (tmp & 0x40) == 0)
    printk("enabled\n");
  else
    printk("disabled\n");

  /* Bit 5 tells us about synch/asynch mode */
  printk("IN-2000 probe found IN-2000 in ");
  if ( (tmp & 0x20) == 0)
    printk("synchronous mode\n");
  else
    printk("asynchronous mode\n");

    irq_level = int_tab [ ((~inb(INFLED)>>3)&0x3) ];

    printk("Configuring IN2000 at IO:%x, IRQ %d"
#ifdef FAST_FIFO_IO
		" (using fast FIFO I/O code)"
#endif
		"\n",base, irq_level);

    outb(2,ININTR);	/* Shut off the FIFO first, so it won't ask for data.*/
    if (request_irq(irq_level,in2000_intr_handle, 0, "in2000"))
    {
	printk("in2000_detect: Unable to allocate IRQ.\n");
	return 0;
    }
    outb(0,INFWRT);	/* read mode so WD can intrpt */
    outb(SCSIST,INSTAT);
    inb(INDATA);	/* free status reg, clear WD intrpt */
    outb(OWNID,INSTAT);
    outb(0x7,INDATA);	/* we use addr 7 */
    outb(COMMAND,INSTAT);
    outb(0,INDATA);	/* do chip reset */
    shpnt = scsi_register(tpnt, 0);
    /* Set these up so that we can unload the driver properly. */
    shpnt->io_port = base;
    shpnt->n_io_port = 12;
    shpnt->irq = irq_level;
    request_region(base, 12,"in2000");  /* Prevent other drivers from using this space */
    return 1;
}

int in2000_abort(Scsi_Cmnd * SCpnt)
{
    DEB(printk("in2000_abort\n"));
    /*
     * Ask no stupid questions, just order the abort.
     */
    outb(COMMAND,INSTAT);
    outb(1,INDATA);	/* Abort Command */
    return 0;
}

static inline void delay( unsigned how_long )
{
    unsigned long time = jiffies + how_long;
    while (jiffies < time) ;
}

int in2000_reset(Scsi_Cmnd * SCpnt)
{
    DEB(printk("in2000_reset called\n"));
    /*
     * Note: this is finished off by an incoming interrupt
     */
    outb(0,INFWRT);	/* read mode so WD can intrpt */
    outb(SCSIST,INSTAT);
    inb(INDATA);
    outb(OWNID,INSTAT);
    outb(0x7,INDATA);	/* ID=7,noadv, no parity, clk div=2 (8-10Mhz clk) */
    outb(COMMAND,INSTAT);
    outb(0,INDATA);	/* reset WD chip */
    delay(2);
#ifdef SCSI_RESET_PENDING
    return SCSI_RESET_PENDING;
#else
    if(SCpnt) SCpnt->flags |= NEEDS_JUMPSTART;
    return 0;
#endif
}

int in2000_biosparam(Disk * disk, kdev_t dev, int* iinfo)
	{
	  int size = disk->capacity;
    DEB(printk("in2000_biosparam\n"));
    iinfo[0] = 64;
    iinfo[1] = 32;
    iinfo[2] = size >> 11;
/* This should approximate the large drive handling that the DOS ASPI manager
   uses.  Drives very near the boundaries may not be handled correctly (i.e.
   near 2.0 Gb and 4.0 Gb) */
    if (iinfo[2] > 1024) {
	iinfo[0] = 64;
	iinfo[1] = 63;
	iinfo[2] = disk->capacity / (iinfo[0] * iinfo[1]);
	}
    if (iinfo[2] > 1024) {
	iinfo[0] = 128;
	iinfo[1] = 63;
	iinfo[2] = disk->capacity / (iinfo[0] * iinfo[1]);
	}
    if (iinfo[2] > 1024) {
	iinfo[0] = 255;
	iinfo[1] = 63;
	iinfo[2] = disk->capacity / (iinfo[0] * iinfo[1]);
	if (iinfo[2] > 1023)
	    iinfo[2] = 1023;
	}
    return 0;
    }

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = IN2000;

#include "scsi_module.c"
#endif

