/*
 * linux/drivers/block/ide-cd.c
 *
 * 1.00  Oct 31, 1994 -- Initial version.
 * 1.01  Nov  2, 1994 -- Fixed problem with starting request in
 *                       cdrom_check_status.
 * 1.03  Nov 25, 1994 -- leaving unmask_intr[] as a user-setting (as for disks)
 * (from mlord)       -- minor changes to cdrom_setup()
 *                    -- renamed ide_dev_s to ide_drive_t, enable irq on command
 * 2.00  Nov 27, 1994 -- Generalize packet command interface;
 *                       add audio ioctls.
 * 2.01  Dec  3, 1994 -- Rework packet command interface to handle devices
 *                       which send an interrupt when ready for a command.
 * 2.02  Dec 11, 1994 -- Cache the TOC in the driver.
 *                       Don't use SCMD_PLAYAUDIO_TI; it's not included
 *                       in the current version of ATAPI.
 *                       Try to use LBA instead of track or MSF addressing
 *                       when possible.
 *                       Don't wait for READY_STAT.
 * 2.03  Jan 10, 1995 -- Rewrite block read routines to handle block sizes
 *                       other than 2k and to move multiple sectors in a
 *                       single transaction.
 * 2.04  Apr 21, 1995 -- Add work-around for Creative Labs CD220E drives.
 *                       Thanks to Nick Saw <cwsaw@pts7.pts.mot.com> for
 *                       help in figuring this out.  Ditto for Acer and
 *                       Aztech drives, which seem to have the same problem.
 * 2.04b May 30, 1995 -- Fix to match changes in ide.c version 3.16 -ml
 * 2.05  Jun  8, 1995 -- Don't attempt to retry after an illegal request
 *                        or data protect error.
 *                       Use HWIF and DEV_HWIF macros as in ide.c.
 *                       Always try to do a request_sense after
 *                        a failed command.
 *                       Include an option to give textual descriptions
 *                        of ATAPI errors.
 *                       Fix a bug in handling the sector cache which
 *                        showed up if the drive returned data in 512 byte
 *                        blocks (like Pioneer drives).  Thanks to
 *                        Richard Hirst <srh@gpt.co.uk> for diagnosing this.
 *                       Properly supply the page number field in the
 *                        MODE_SELECT command.
 *                       PLAYAUDIO12 is broken on the Aztech; work around it.
 * 2.05x Aug 11, 1995 -- lots of data structure renaming/restructuring in ide.c
 *                       (my apologies to Scott, but now ide-cd.c is independent)
 * 3.00  Aug 22, 1995 -- Implement CDROMMULTISESSION ioctl.
 *                       Implement CDROMREADAUDIO ioctl (UNTESTED).
 *                       Use input_ide_data() and output_ide_data().
 *                       Add door locking.
 *                       Fix usage count leak in cdrom_open, which happened
 *                        when a read-write mount was attempted.
 *                       Try to load the disk on open.
 *                       Implement CDROMEJECT_SW ioctl (off by default).
 *                       Read total cdrom capacity during open.
 *                       Rearrange logic in cdrom_decode_status.  Issue
 *                        request sense commands for failed packet commands
 *                        from here instead of from cdrom_queue_packet_command.
 *                        Fix a race condition in retrieving error information.
 *                       Suppress printing normal unit attention errors and
 *                        some drive not ready errors.
 *                       Implement CDROMVOLREAD ioctl.
 *                       Implement CDROMREADMODE1/2 ioctls.
 *                       Fix race condition in setting up interrupt handlers
 *                        when the `serialize' option is used.
 * 3.01  Sep  2, 1995 -- Fix ordering of reenabling interrupts in
 *                        cdrom_queue_request.
 *                       Another try at using ide_[input,output]_data.
 * 3.02  Sep 16, 1995 -- Stick total disk capacity in partition table as well.
 *                       Make VERBOSE_IDE_CD_ERRORS dump failed command again.
 *                       Dump out more information for ILLEGAL REQUEST errs.
 *                       Fix handling of errors occuring before the
 *                        packet command is transferred.
 *                       Fix transfers with odd bytelengths.
 * 3.03  Oct 27, 1995 -- Some Creative drives have an id of just `CD'.
 *                       `DCI-2S10' drives are broken too.
 * 3.04  Nov 20, 1995 -- So are Vertos drives.
 * 3.05  Dec  1, 1995 -- Changes to go with overhaul of ide.c and ide-tape.c
 * 3.06  Dec 16, 1995 -- Add support needed for partitions.
 *                       More workarounds for Vertos bugs (based on patches
 *                        from Holger Dietze <dietze@aix520.informatik.uni-leipzig.de>).
 *                       Try to eliminate byteorder assumptions.
 *                       Use atapi_cdrom_subchnl struct definition.
 *                       Add STANDARD_ATAPI compilation option.
 * 3.07  Jan 29, 1996 -- More twiddling for broken drives: Sony 55D,
 *                        Vertos 300.
 *                       Add NO_DOOR_LOCKING configuration option.
 *                       Handle drive_cmd requests w/NULL args (for hdparm -t).
 *                       Work around sporadic Sony55e audio play problem.
 * 3.07a Feb 11, 1996 -- check drive->id for NULL before dereferencing, to fix
 *                        problem with "hde=cdrom" with no drive present.  -ml
 *
 * NOTE: Direct audio reads will only work on some types of drive.
 * So far, i've received reports of success for Sony and Toshiba drives.
 *
 * ATAPI cd-rom driver.  To be used with ide.c.
 *
 * Copyright (C) 1994, 1995, 1996  scott snyder  <snyder@fnald0.fnal.gov>
 * May be copied or modified under the terms of the GNU General Public License
 * (../../COPYING).
 */


/***************************************************************************/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/malloc.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/cdrom.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/segment.h>
#ifdef __alpha__
# include <asm/unaligned.h>
#endif

#include "ide.h"



/* Turn this on to have the driver print out the meanings of the
   ATAPI error codes.  This will use up additional kernel-space
   memory, though. */

#ifndef VERBOSE_IDE_CD_ERRORS
#define VERBOSE_IDE_CD_ERRORS 0
#endif


/* Turning this on will remove code to work around various nonstandard
   ATAPI implementations.  If you know your drive follows the standard,
   this will give you a slightly smaller kernel. */

#ifndef STANDARD_ATAPI
#define STANDARD_ATAPI 0
#endif


/* Turning this on will disable the door-locking functionality.
   This is apparently needed for supermount. */

#ifndef NO_DOOR_LOCKING
#define NO_DOOR_LOCKING 0
#endif


/************************************************************************/

#define SECTOR_SIZE 512
#define SECTOR_BITS 9
#define SECTORS_PER_FRAME (CD_FRAMESIZE / SECTOR_SIZE)

#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* special command codes for strategy routine. */
#define PACKET_COMMAND        4315
#define REQUEST_SENSE_COMMAND 4316
#define RESET_DRIVE_COMMAND   4317

/* Some ATAPI command opcodes (just like SCSI).
   (Some other cdrom-specific codes are in cdrom.h.) */
#define TEST_UNIT_READY         0x00
#define REQUEST_SENSE           0x03
#define START_STOP              0x1b
#define ALLOW_MEDIUM_REMOVAL    0x1e
#define READ_CAPACITY		0x25
#define READ_10                 0x28
#define MODE_SENSE_10           0x5a
#define MODE_SELECT_10          0x55
#define READ_CD                 0xbe


/* ATAPI sense keys (mostly copied from scsi.h). */

#define NO_SENSE                0x00
#define RECOVERED_ERROR         0x01
#define NOT_READY               0x02
#define MEDIUM_ERROR            0x03
#define HARDWARE_ERROR          0x04
#define ILLEGAL_REQUEST         0x05
#define UNIT_ATTENTION          0x06
#define DATA_PROTECT            0x07
#define ABORTED_COMMAND         0x0b
#define MISCOMPARE              0x0e

/* We want some additional flags for cd-rom drives.
   To save space in the ide_drive_t struct, use some fields which
   doesn't make sense for cd-roms -- `bios_sect' and `bios_head'. */

/* Configuration flags.  These describe the capabilities of the drive.
   They generally do not change after initialization, unless we learn
   more about the drive from stuff failing. */
struct ide_cd_config_flags {
  __u8 drq_interrupt : 1; /* Device sends an interrupt when ready
                                 for a packet command. */
  __u8 no_doorlock   : 1; /* Drive cannot lock the door. */
#if ! STANDARD_ATAPI
  __u8 no_playaudio12: 1; /* The PLAYAUDIO12 command is not supported. */
 
  __u8 no_lba_toc    : 1; /* Drive cannot return TOC info in LBA format. */
  __u8 playmsf_uses_bcd : 1; /* Drive uses BCD in PLAYAUDIO_MSF. */
  __u8 old_readcd    : 1; /* Drive uses old READ CD opcode. */
  __u8 vertos_lossage: 1; /* Drive is a Vertos 300,
				 and likes to speak BCD. */
#endif  /* not STANDARD_ATAPI */
  __u8 reserved : 1;
};
#define CDROM_CONFIG_FLAGS(drive) ((struct ide_cd_config_flags *)&((drive)->bios_sect))

 
/* State flags.  These give information about the current state of the
   drive, and will change during normal operation. */
struct ide_cd_state_flags {
  __u8 media_changed : 1; /* Driver has noticed a media change. */
  __u8 toc_valid     : 1; /* Saved TOC information is current. */
  __u8 door_locked   : 1; /* We think that the drive door is locked. */
  __u8 eject_on_close: 1; /* Drive should eject when device is closed. */
  __u8 reserved : 4;
};
#define CDROM_STATE_FLAGS(drive)  ((struct ide_cd_state_flags *)&((drive)->bios_head))


#define SECTOR_BUFFER_SIZE CD_FRAMESIZE



/****************************************************************************
 * Routines to read and write data from/to the drive, using
 * the routines input_ide_data() and output_ide_data() from ide.c.
 *
 * These routines will round up any request for an odd number of bytes,
 * so if an odd bytecount is specified, be sure that there's at least one
 * extra byte allocated for the buffer.
 */


static inline
void cdrom_in_bytes (ide_drive_t *drive, void *buffer, uint bytecount)
{
  ++bytecount;
  ide_input_data (drive, buffer, bytecount / 4);
  if ((bytecount & 0x03) >= 2)
    {
      insw (IDE_DATA_REG, ((byte *)buffer) + (bytecount & ~0x03), 1);
    }
}


static inline
void cdrom_out_bytes (ide_drive_t *drive, void *buffer, uint bytecount)
{
  ++bytecount;
  ide_output_data (drive, buffer, bytecount / 4);
  if ((bytecount & 0x03) >= 2)
    {
      outsw (IDE_DATA_REG, ((byte *)buffer) + (bytecount & ~0x03), 1);
    }
}



/****************************************************************************
 * Descriptions of ATAPI error codes.
 */

#define ARY_LEN(a) ((sizeof(a) / sizeof(a[0])))

#if VERBOSE_IDE_CD_ERRORS

/* From Table 124 of the ATAPI 1.2 spec. */

char *sense_key_texts[16] = {
  "No sense data",
  "Recovered error",
  "Not ready",
  "Medium error",
  "Hardware error",
  "Illegal request",
  "Unit attention",
  "Data protect",
  "(reserved)",
  "(reserved)",
  "(reserved)",
  "Aborted command",
  "(reserved)",
  "(reserved)",
  "Miscompare",
  "(reserved)",
};


/* From Table 125 of the ATAPI 1.2 spec. */

struct {
  short asc_ascq;
  char *text;
} sense_data_texts[] = {
  { 0x0000, "No additional sense information" },
  { 0x0011, "Audio play operation in progress" },
  { 0x0012, "Audio play operation paused" },
  { 0x0013, "Audio play operation successfully completed" },
  { 0x0014, "Audio play operation stopped due to error" },
  { 0x0015, "No current audio status to return" },

  { 0x0200, "No seek complete" },

  { 0x0400, "Logical unit not ready - cause not reportable" },
  { 0x0401, "Logical unit not ready - in progress (sic) of becoming ready" },
  { 0x0402, "Logical unit not ready - initializing command required" },
  { 0x0403, "Logical unit not ready - manual intervention required" },

  { 0x0600, "No reference position found" },

  { 0x0900, "Track following error" },
  { 0x0901, "Tracking servo failure" },
  { 0x0902, "Focus servo failure" },
  { 0x0903, "Spindle servo failure" },

  { 0x1100, "Unrecovered read error" },
  { 0x1106, "CIRC unrecovered error" },

  { 0x1500, "Random positioning error" },
  { 0x1501, "Mechanical positioning error" },
  { 0x1502, "Positioning error detected by read of medium" },

  { 0x1700, "Recovered data with no error correction applied" },
  { 0x1701, "Recovered data with retries" },
  { 0x1702, "Recovered data with positive head offset" },
  { 0x1703, "Recovered data with negative head offset" },
  { 0x1704, "Recovered data with retries and/or CIRC applied" },
  { 0x1705, "Recovered data using previous sector ID" },

  { 0x1800, "Recovered data with error correction applied" },
  { 0x1801, "Recovered data with error correction and retries applied" },
  { 0x1802, "Recovered data - the data was auto-reallocated" },
  { 0x1803, "Recovered data with CIRC" },
  { 0x1804, "Recovered data with L-EC" },
  { 0x1805, "Recovered data - recommend reassignment" },
  { 0x1806, "Recovered data - recommend rewrite" },

  { 0x1a00, "Parameter list length error" },

  { 0x2000, "Invalid command operation code" },

  { 0x2100, "Logical block address out of range" },

  { 0x2400, "Invalid field in command packet" },

  { 0x2600, "Invalid field in parameter list" },
  { 0x2601, "Parameter not supported" },
  { 0x2602, "Parameter value invalid" },
  { 0x2603, "Threshold parameters not supported" },

  { 0x2800, "Not ready to ready transition, medium may have changed" },

  { 0x2900, "Power on, reset or bus device reset occurred" },

  { 0x2a00, "Parameters changed" },
  { 0x2a01, "Mode parameters changed" },

  { 0x3000, "Incompatible medium installed" },
  { 0x3001, "Cannot read medium - unknown format" },
  { 0x3002, "Cannot read medium - incompatible format" },

  { 0x3700, "Rounded parameter" },

  { 0x3900, "Saving parameters not supported" },

  { 0x3a00, "Medium not present" },

  { 0x3f00, "ATAPI CD-ROM drive operating conditions have changed" },
  { 0x3f01, "Microcode has been changed" },
  { 0x3f02, "Changed operating definition" },
  { 0x3f03, "Inquiry data has changed" },

  { 0x4000, "Diagnostic failure on component (ASCQ)" },

  { 0x4400, "Internal ATAPI CD-ROM drive failure" },

  { 0x4e00, "Overlapped commands attempted" },

  { 0x5300, "Media load or eject failed" },
  { 0x5302, "Medium removal prevented" },

  { 0x5700, "Unable to recover table of contents" },

  { 0x5a00, "Operator request or state change input (unspecified)" },
  { 0x5a01, "Operator medium removal request" },

  { 0x5b00, "Threshold condition met" },

  { 0x5c00, "Status change" },

  { 0x6300, "End of user area encountered on this track" },

  { 0x6400, "Illegal mode for this track" },

  { 0xbf00, "Loss of streaming" },
};
#endif



/****************************************************************************
 * Generic packet command support and error handling routines.
 */


static
void cdrom_analyze_sense_data (ide_drive_t *drive, 
			       struct atapi_request_sense *reqbuf,
			       struct packet_command *failed_command)
{
  /* Don't print not ready or unit attention errors for READ_SUBCHANNEL.
     Workman (and probably other programs) uses this command to poll
     the drive, and we don't want to fill the syslog with useless errors. */
  if (failed_command &&
      failed_command->c[0] == SCMD_READ_SUBCHANNEL &&
      (reqbuf->sense_key == NOT_READY || reqbuf->sense_key == UNIT_ATTENTION))
    return;

#if VERBOSE_IDE_CD_ERRORS
  {
    int i;
    char *s;
    char buf[80];

    printk ("ATAPI device %s:\n", drive->name);

    printk ("  Error code: 0x%02x\n", reqbuf->error_code);

    if (reqbuf->sense_key >= 0 &&
	reqbuf->sense_key < ARY_LEN (sense_key_texts))
      s = sense_key_texts[reqbuf->sense_key];
    else
      s = "(bad sense key)";

    printk ("  Sense key: 0x%02x - %s\n", reqbuf->sense_key, s);

    if (reqbuf->asc == 0x40) {
      sprintf (buf, "Diagnostic failure on component 0x%02x", reqbuf->ascq);
      s = buf;
    }

    else {
      int lo, hi;
      int key = (reqbuf->asc << 8);
      if ( ! (reqbuf->ascq >= 0x80 && reqbuf->ascq <= 0xdd) )
	key |= reqbuf->ascq;

      lo = 0;
      hi = ARY_LEN (sense_data_texts);
      s = NULL;

      while (hi > lo) {
	int mid = (lo + hi) / 2;
	if (sense_data_texts[mid].asc_ascq == key) {
	  s = sense_data_texts[mid].text;
	  break;
	}
	else if (sense_data_texts[mid].asc_ascq > key)
	  hi = mid;
	else
	  lo = mid+1;
      }
    }

    if (s == NULL) {
      if (reqbuf->asc > 0x80)
	s = "(vendor-specific error)";
      else
	s = "(reserved error code)";
    }

    printk ("  Additional sense data: 0x%02x, 0x%02x  - %s\n",
	    reqbuf->asc, reqbuf->ascq, s);

    if (failed_command != NULL) {
      printk ("  Failed packet command: ");
      for (i=0; i<sizeof (failed_command->c); i++)
	printk ("%02x ", failed_command->c[i]);
      printk ("\n");
    }

    if (reqbuf->sense_key == ILLEGAL_REQUEST &&
	(reqbuf->sense_key_specific[0] & 0x80) != 0)
      {
	printk ("  Error in %s byte %d",
		(reqbuf->sense_key_specific[0] & 0x40) != 0
		  ? "command packet"
		  : "command data",
		(reqbuf->sense_key_specific[1] << 8) +
		reqbuf->sense_key_specific[2]);

	if ((reqbuf->sense_key_specific[0] & 0x40) != 0)
	  {
	    printk (" bit %d", reqbuf->sense_key_specific[0] & 0x07);
	  }

	printk ("\n");
      }
  }

#else

  /* Suppress printing unit attention and `in progress of becoming ready'
     errors when we're not being verbose. */

  if (reqbuf->sense_key == UNIT_ATTENTION ||
      (reqbuf->sense_key == NOT_READY && (reqbuf->asc == 4 ||
					  reqbuf->asc == 0x3a)))
    return;

  printk ("%s: code: 0x%02x  key: 0x%02x  asc: 0x%02x  ascq: 0x%02x\n",
	  drive->name,
	  reqbuf->error_code, reqbuf->sense_key, reqbuf->asc, reqbuf->ascq);
#endif
}


/* Fix up a possibly partially-processed request so that we can
   start it over entirely, or even put it back on the request queue. */
static void restore_request (struct request *rq)
{
  if (rq->buffer != rq->bh->b_data)
    {
      int n = (rq->buffer - rq->bh->b_data) / SECTOR_SIZE;
      rq->buffer = rq->bh->b_data;
      rq->nr_sectors += n;
      rq->sector -= n;
    }
  rq->current_nr_sectors = rq->bh->b_size >> SECTOR_BITS;
}


static void cdrom_queue_request_sense (ide_drive_t *drive, 
				       struct semaphore *sem,
				       struct atapi_request_sense *reqbuf,
				       struct packet_command *failed_command)
{
  struct request *rq;
  struct packet_command *pc;
  int len;

  /* If the request didn't explicitly specify where to put the sense data,
     use the statically allocated structure. */
  if (reqbuf == NULL)
    reqbuf = &drive->cdrom_info.sense_data;

  /* Make up a new request to retrieve sense information. */

  pc = &HWIF(drive)->request_sense_pc;
  memset (pc, 0, sizeof (*pc));

  /* The request_sense structure has an odd number of (16-bit) words,
     which won't work well with 32-bit transfers.  However, we don't care
     about the last two bytes, so just truncate the structure down
     to an even length. */
  len = sizeof (*reqbuf) / 4;
  len *= 4;

  pc->c[0] = REQUEST_SENSE;
  pc->c[4] = len;
  pc->buffer = (char *)reqbuf;
  pc->buflen = len;
  pc->sense_data = (struct atapi_request_sense *)failed_command;

  /* stuff the sense request in front of our current request */

  rq = &HWIF(drive)->request_sense_request;
  ide_init_drive_cmd (rq);
  rq->cmd = REQUEST_SENSE_COMMAND;
  rq->buffer = (char *)pc;
  rq->sem = sem;
  (void) ide_do_drive_cmd (drive, rq, ide_preempt);
}


static void cdrom_end_request (int uptodate, ide_drive_t *drive)
{
  struct request *rq = HWGROUP(drive)->rq;

  /* The code in blk.h can screw us up on error recovery if the block
     size is larger than 1k.  Fix that up here. */
  if (!uptodate && rq->bh != 0)
    {
      int adj = rq->current_nr_sectors - 1;
      rq->current_nr_sectors -= adj;
      rq->sector += adj;
    }

  if (rq->cmd == REQUEST_SENSE_COMMAND && uptodate)
    {
      struct packet_command *pc = (struct packet_command *)rq->buffer;
      cdrom_analyze_sense_data (drive,
				(struct atapi_request_sense *)(pc->buffer - pc->c[4]), 
				(struct packet_command *)pc->sense_data);
    }

  ide_end_request (uptodate, HWGROUP(drive));
}


/* Mark that we've seen a media change, and invalidate our internal
   buffers. */
static void cdrom_saw_media_change (ide_drive_t *drive)
{
  CDROM_STATE_FLAGS (drive)->media_changed = 1;
  CDROM_STATE_FLAGS (drive)->toc_valid = 0;
  drive->cdrom_info.nsectors_buffered = 0;
}


/* Returns 0 if the request should be continued.
   Returns 1 if the request was ended. */
static int cdrom_decode_status (ide_drive_t *drive, int good_stat, int *stat_ret)
{
  struct request *rq = HWGROUP(drive)->rq;
  int stat, err, sense_key, cmd;

  /* Check for errors. */
  stat = GET_STAT();
  *stat_ret = stat;

  if (OK_STAT (stat, good_stat, BAD_R_STAT))
    return 0;

  /* Got an error. */
  err = IN_BYTE (IDE_ERROR_REG);
  sense_key = err >> 4;

  if (rq == NULL)
    printk ("%s : missing request in cdrom_decode_status\n", drive->name);
  else
    {
      cmd = rq->cmd;

      if (cmd == REQUEST_SENSE_COMMAND)
	{
	  /* We got an error trying to get sense info from the drive
	     (probably while trying to recover from a former error).
	     Just give up. */

	  struct packet_command *pc = (struct packet_command *)rq->buffer;
	  pc->stat = 1;
	  cdrom_end_request (1, drive);
	  ide_error (drive, "request sense failure", stat);
	  return 1;
	}

      else if (cmd == PACKET_COMMAND)
	{
	  /* All other functions, except for READ. */

	  struct packet_command *pc = (struct packet_command *)rq->buffer;
	  struct semaphore *sem = NULL;

	  /* Check for tray open. */
	  if (sense_key == NOT_READY)
	    {
	      cdrom_saw_media_change (drive);

	      /* Print an error message to the syslog.
		 Exception: don't print anything if this is a read subchannel
		 command.  This is because workman constantly polls the drive
		 with this command, and we don't want to uselessly fill up
		 the syslog. */
	      if (pc->c[0] != SCMD_READ_SUBCHANNEL)
		printk ("%s : tray open or drive not ready\n", drive->name);
	    }

	  /* Check for media change. */
	  else if (sense_key == UNIT_ATTENTION)
	    {
	      cdrom_saw_media_change (drive);
	      printk ("%s: media changed\n", drive->name);
	    }

	  /* Otherwise, print an error. */
	  else
	    {
	      ide_dump_status (drive, "packet command error", stat);
	    }

	  /* Set the error flag and complete the request.
	     Then, if we have a CHECK CONDITION status, queue a request
	     sense command.  We must be careful, though: we don't want
	     the thread in cdrom_queue_packet_command to wake up until
	     the request sense has completed.  We do this by transferring
	     the semaphore from the packet command request to the
	     request sense request. */

	  if ((stat & ERR_STAT) != 0)
	    {
	      sem = rq->sem;
	      rq->sem = NULL;
	    }

	  pc->stat = 1;
	  cdrom_end_request (1, drive);

	  if ((stat & ERR_STAT) != 0)
	    cdrom_queue_request_sense (drive, sem, pc->sense_data, pc);
	}

      else
	{
	  /* Handle errors from READ requests. */

	  /* Check for tray open. */
	  if (sense_key == NOT_READY)
	    {
	      cdrom_saw_media_change (drive);

	      /* Fail the request. */
	      printk ("%s : tray open\n", drive->name);
	      cdrom_end_request (0, drive);
	    }

	  /* Check for media change. */
	  else if (sense_key == UNIT_ATTENTION)
	    {
	      cdrom_saw_media_change (drive);

	      /* Arrange to retry the request.
	         But be sure to give up if we've retried too many times. */
	      if (++rq->errors > ERROR_MAX)
		{
		  cdrom_end_request (0, drive);
		}
	    }
	  /* No point in retrying after an illegal request or
	     data protect error.*/
	  else if (sense_key == ILLEGAL_REQUEST || sense_key == DATA_PROTECT)
	    {
	      ide_dump_status (drive, "command error", stat);
	      cdrom_end_request (0, drive);
	    }

	  /* If there were other errors, go to the default handler. */
	  else if ((err & ~ABRT_ERR) != 0)
	    {
	      ide_error (drive, "cdrom_decode_status", stat);
	      return 1;
	    }

	  /* Else, abort if we've racked up too many retries. */
	  else if ((++rq->errors > ERROR_MAX))
	    {
	      cdrom_end_request (0, drive);
	    }

	  /* If we got a CHECK_CONDITION status, queue a request sense
	     command. */
	  if ((stat & ERR_STAT) != 0)
	    cdrom_queue_request_sense (drive, NULL, NULL, NULL);
	}
    }

  /* Retry, or handle the next request. */
  return 1;
}


/* Set up the device registers for transferring a packet command on DEV,
   expecting to later transfer XFERLEN bytes.  HANDLER is the routine
   which actually transfers the command to the drive.  If this is a
   drq_interrupt device, this routine will arrange for HANDLER to be
   called when the interrupt from the drive arrives.  Otherwise, HANDLER
   will be called immediately after the drive is prepared for the transfer. */

static int cdrom_start_packet_command (ide_drive_t *drive, int xferlen,
				       ide_handler_t *handler)
{
  /* Wait for the controller to be idle. */
  if (ide_wait_stat (drive, 0, BUSY_STAT, WAIT_READY)) return 1;

  /* Set up the controller registers. */
  OUT_BYTE (0, IDE_FEATURE_REG);
  OUT_BYTE (0, IDE_NSECTOR_REG);
  OUT_BYTE (0, IDE_SECTOR_REG);

  OUT_BYTE (xferlen & 0xff, IDE_LCYL_REG);
  OUT_BYTE (xferlen >> 8  , IDE_HCYL_REG);
  OUT_BYTE (drive->ctl, IDE_CONTROL_REG);

  if (CDROM_CONFIG_FLAGS (drive)->drq_interrupt)
    {
      ide_set_handler (drive, handler, WAIT_CMD);
      OUT_BYTE (WIN_PACKETCMD, IDE_COMMAND_REG); /* packet command */
    }
  else
    {
      OUT_BYTE (WIN_PACKETCMD, IDE_COMMAND_REG); /* packet command */
      (*handler) (drive);
    }

  return 0;
}


/* Send a packet command to DRIVE described by CMD_BUF and CMD_LEN.
   The device registers must have already been prepared
   by cdrom_start_packet_command.
   HANDLER is the interrupt handler to call when the command completes
   or there's data ready. */
static int cdrom_transfer_packet_command (ide_drive_t *drive,
                                          char *cmd_buf, int cmd_len,
					  ide_handler_t *handler)
{
  if (CDROM_CONFIG_FLAGS (drive)->drq_interrupt)
    {
      /* Here we should have been called after receiving an interrupt
         from the device.  DRQ should how be set. */
      int stat_dum;

      /* Check for errors. */
      if (cdrom_decode_status (drive, DRQ_STAT, &stat_dum)) return 1;
    }
  else
    {
      /* Otherwise, we must wait for DRQ to get set. */
      if (ide_wait_stat (drive, DRQ_STAT, BUSY_STAT, WAIT_READY)) return 1;
    }

  /* Arm the interrupt handler. */
  ide_set_handler (drive, handler, WAIT_CMD);

  /* Send the command to the device. */
  cdrom_out_bytes (drive, cmd_buf, cmd_len);

  return 0;
}



/****************************************************************************
 * Block read functions.
 */

/*
 * Buffer up to SECTORS_TO_TRANSFER sectors from the drive in our sector
 * buffer.  Once the first sector is added, any subsequent sectors are
 * assumed to be continuous (until the buffer is cleared).  For the first
 * sector added, SECTOR is its sector number.  (SECTOR is then ignored until
 * the buffer is cleared.)
 */
static void cdrom_buffer_sectors (ide_drive_t *drive, unsigned long sector,
                                  int sectors_to_transfer)
{
  struct cdrom_info *info = &drive->cdrom_info;

  /* Number of sectors to read into the buffer. */
  int sectors_to_buffer = MIN (sectors_to_transfer,
                               (SECTOR_BUFFER_SIZE >> SECTOR_BITS) -
                                 info->nsectors_buffered);

  char *dest;

  /* If we don't yet have a sector buffer, try to allocate one.
     If we can't get one atomically, it's not fatal -- we'll just throw
     the data away rather than caching it. */
  if (info->sector_buffer == NULL)
    {
      info->sector_buffer = (char *) kmalloc (SECTOR_BUFFER_SIZE, GFP_ATOMIC);

      /* If we couldn't get a buffer, don't try to buffer anything... */
      if (info->sector_buffer == NULL)
        sectors_to_buffer = 0;
    }

  /* If this is the first sector in the buffer, remember its number. */
  if (info->nsectors_buffered == 0)
    info->sector_buffered = sector;

  /* Read the data into the buffer. */
  dest = info->sector_buffer + info->nsectors_buffered * SECTOR_SIZE;
  while (sectors_to_buffer > 0)
    {
      cdrom_in_bytes (drive, dest, SECTOR_SIZE);
      --sectors_to_buffer;
      --sectors_to_transfer;
      ++info->nsectors_buffered;
      dest += SECTOR_SIZE;
    }

  /* Throw away any remaining data. */
  while (sectors_to_transfer > 0)
    {
      char dum[SECTOR_SIZE];
      cdrom_in_bytes (drive, dum, sizeof (dum));
      --sectors_to_transfer;
    }
}


/*
 * Check the contents of the interrupt reason register from the cdrom
 * and attempt to recover if there are problems.  Returns  0 if everything's
 * ok; nonzero if the request has been terminated.
 */
static inline
int cdrom_read_check_ireason (ide_drive_t *drive, int len, int ireason)
{
  ireason &= 3;
  if (ireason == 2) return 0;

  if (ireason == 0)
    {
      /* Whoops... The drive is expecting to receive data from us! */
      printk ("%s: cdrom_read_intr: "
              "Drive wants to transfer data the wrong way!\n",
              drive->name);

      /* Throw some data at the drive so it doesn't hang
         and quit this request. */
      while (len > 0)
        {
          int dum = 0;
	  cdrom_out_bytes (drive, &dum, sizeof (dum));
          len -= sizeof (dum);
        }
    }

  else
    {
      /* Drive wants a command packet, or invalid ireason... */
      printk ("%s: cdrom_read_intr: bad interrupt reason %d\n",
              drive->name, ireason);
    }

  cdrom_end_request (0, drive);
  return -1;
}


/*
 * Interrupt routine.  Called when a read request has completed.
 */
static void cdrom_read_intr (ide_drive_t *drive)
{
  int stat;
  int ireason, len, sectors_to_transfer, nskip;

  struct request *rq = HWGROUP(drive)->rq;

  /* Check for errors. */
  if (cdrom_decode_status (drive, 0, &stat)) return;

  /* Read the interrupt reason and the transfer length. */
  ireason = IN_BYTE (IDE_NSECTOR_REG);
  len = IN_BYTE (IDE_LCYL_REG) + 256 * IN_BYTE (IDE_HCYL_REG);

  /* If DRQ is clear, the command has completed. */
  if ((stat & DRQ_STAT) == 0)
    {
      /* If we're not done filling the current buffer, complain.
         Otherwise, complete the command normally. */
      if (rq->current_nr_sectors > 0)
        {
          printk ("%s: cdrom_read_intr: data underrun (%ld blocks)\n",
                  drive->name, rq->current_nr_sectors);
          cdrom_end_request (0, drive);
        }
      else
        cdrom_end_request (1, drive);

      return;
    }

  /* Check that the drive is expecting to do the same thing that we are. */
  if (cdrom_read_check_ireason (drive, len, ireason)) return;

  /* Assume that the drive will always provide data in multiples of at least
     SECTOR_SIZE, as it gets hairy to keep track of the transfers otherwise. */
  if ((len % SECTOR_SIZE) != 0)
    {
      printk ("%s: cdrom_read_intr: Bad transfer size %d\n",
              drive->name, len);
      printk ("  This drive is not supported by this version of the driver\n");
      cdrom_end_request (0, drive);
      return;
    }

  /* The number of sectors we need to read from the drive. */
  sectors_to_transfer = len / SECTOR_SIZE;

  /* First, figure out if we need to bit-bucket any of the leading sectors. */
  nskip = MIN ((int)(rq->current_nr_sectors - (rq->bh->b_size >> SECTOR_BITS)),
               sectors_to_transfer);

  while (nskip > 0)
    {
      /* We need to throw away a sector. */
      char dum[SECTOR_SIZE];
      cdrom_in_bytes (drive, dum, sizeof (dum));

      --rq->current_nr_sectors;
      --nskip;
      --sectors_to_transfer;
    }

  /* Now loop while we still have data to read from the drive. */
  while (sectors_to_transfer > 0)
    {
      int this_transfer;

      /* If we've filled the present buffer but there's another chained
         buffer after it, move on. */
      if (rq->current_nr_sectors == 0 &&
          rq->nr_sectors > 0)
        cdrom_end_request (1, drive);

      /* If the buffers are full, cache the rest of the data in our
         internal buffer. */
      if (rq->current_nr_sectors == 0)
        {
          cdrom_buffer_sectors (drive, rq->sector, sectors_to_transfer);
          sectors_to_transfer = 0;
        }
      else
        {
          /* Transfer data to the buffers.
             Figure out how many sectors we can transfer
             to the current buffer. */
          this_transfer = MIN (sectors_to_transfer,
                               rq->current_nr_sectors);

          /* Read this_transfer sectors into the current buffer. */
          while (this_transfer > 0)
            {
              cdrom_in_bytes (drive, rq->buffer, SECTOR_SIZE);
              rq->buffer += SECTOR_SIZE;
              --rq->nr_sectors;
              --rq->current_nr_sectors;
              ++rq->sector;
              --this_transfer;
              --sectors_to_transfer;
            }
        }
    }

  /* Done moving data!
     Wait for another interrupt. */
  ide_set_handler (drive, &cdrom_read_intr, WAIT_CMD);
}


/*
 * Try to satisfy some of the current read request from our cached data.
 * Returns nonzero if the request has been completed, zero otherwise.
 */
static int cdrom_read_from_buffer (ide_drive_t *drive)
{
  struct cdrom_info *info = &drive->cdrom_info;
  struct request *rq = HWGROUP(drive)->rq;

  /* Can't do anything if there's no buffer. */
  if (info->sector_buffer == NULL) return 0;

  /* Loop while this request needs data and the next block is present
     in our cache. */
  while (rq->nr_sectors > 0 &&
         rq->sector >= info->sector_buffered &&
         rq->sector < info->sector_buffered + info->nsectors_buffered)
    {
      if (rq->current_nr_sectors == 0)
        cdrom_end_request (1, drive);

      memcpy (rq->buffer,
              info->sector_buffer +
                (rq->sector - info->sector_buffered) * SECTOR_SIZE,
              SECTOR_SIZE);
      rq->buffer += SECTOR_SIZE;
      --rq->current_nr_sectors;
      --rq->nr_sectors;
      ++rq->sector;
    }

  /* If we've satisfied the current request, terminate it successfully. */
  if (rq->nr_sectors == 0)
    {
      cdrom_end_request (1, drive);
      return -1;
    }

  /* Move on to the next buffer if needed. */
  if (rq->current_nr_sectors == 0)
    cdrom_end_request (1, drive);

  /* If this condition does not hold, then the kluge i use to
     represent the number of sectors to skip at the start of a transfer
     will fail.  I think that this will never happen, but let's be
     paranoid and check. */
  if (rq->current_nr_sectors < (rq->bh->b_size >> SECTOR_BITS) &&
      (rq->sector % SECTORS_PER_FRAME) != 0)
    {
      printk ("%s: cdrom_read_from_buffer: buffer botch (%ld)\n",
              drive->name, rq->sector);
      cdrom_end_request (0, drive);
      return -1;
    }

  return 0;
}



/*
 * Routine to send a read packet command to the drive.
 * This is usually called directly from cdrom_start_read.
 * However, for drq_interrupt devices, it is called from an interrupt
 * when the drive is ready to accept the command.
 */
static void cdrom_start_read_continuation (ide_drive_t *drive)
{
  struct packet_command pc;
  struct request *rq = HWGROUP(drive)->rq;

  int nsect, sector, nframes, frame, nskip;

  /* Number of sectors to transfer. */
  nsect = rq->nr_sectors;

  /* Starting sector. */
  sector = rq->sector;

  /* If the requested sector doesn't start on a cdrom block boundary,
     we must adjust the start of the transfer so that it does,
     and remember to skip the first few sectors.  If the CURRENT_NR_SECTORS
     field is larger than the size of the buffer, it will mean that
     we're to skip a number of sectors equal to the amount by which
     CURRENT_NR_SECTORS is larger than the buffer size. */
  nskip = (sector % SECTORS_PER_FRAME);
  if (nskip > 0)
    {
      /* Sanity check... */
      if (rq->current_nr_sectors != (rq->bh->b_size >> SECTOR_BITS))
        {
          printk ("%s: cdrom_start_read_continuation: buffer botch (%ld)\n",
                  drive->name, rq->current_nr_sectors);
          cdrom_end_request (0, drive);
          return;
        }

      sector -= nskip;
      nsect += nskip;
      rq->current_nr_sectors += nskip;
    }

  /* Convert from sectors to cdrom blocks, rounding up the transfer
     length if needed. */
  nframes = (nsect + SECTORS_PER_FRAME-1) / SECTORS_PER_FRAME;
  frame = sector / SECTORS_PER_FRAME;

  /* Largest number of frames was can transfer at once is 64k-1. */
  nframes = MIN (nframes, 65535);

  /* Set up the command */
  memset (&pc.c, 0, sizeof (pc.c));
  pc.c[0] = READ_10;
  pc.c[7] = (nframes >> 8);
  pc.c[8] = (nframes & 0xff);
#ifdef __alpha__
  stl_u (htonl (frame), (unsigned int *) &pc.c[2]);
#else
  *(int *)(&pc.c[2]) = htonl (frame);
#endif

  /* Send the command to the drive and return. */
  (void) cdrom_transfer_packet_command (drive, pc.c, sizeof (pc.c),
					&cdrom_read_intr);
}


/*
 * Start a read request from the CD-ROM.
 */
static void cdrom_start_read (ide_drive_t *drive, unsigned int block)
{
  struct request *rq = HWGROUP(drive)->rq;
  int minor = MINOR (rq->rq_dev);

  /* If the request is relative to a partition, fix it up to refer to the
     absolute address.  */
  if ((minor & PARTN_MASK) != 0) {
    rq->sector = block;
    minor &= ~PARTN_MASK;
    rq->rq_dev = MKDEV (MAJOR(rq->rq_dev), minor);
  }

  /* We may be retrying this request after an error.
     Fix up any weirdness which might be present in the request packet. */
  restore_request (rq);

  /* Satisfy whatever we can of this request from our cached sector. */
  if (cdrom_read_from_buffer (drive))
    return;

  /* Clear the local sector buffer. */
  drive->cdrom_info.nsectors_buffered = 0;

  /* Start sending the read request to the drive. */
  cdrom_start_packet_command (drive, 32768, cdrom_start_read_continuation);
}




/****************************************************************************
 * Execute all other packet commands.
 */

/* Forward declarations. */
static int
cdrom_lockdoor (ide_drive_t *drive, int lockflag,
		struct atapi_request_sense *reqbuf);



/* Interrupt routine for packet command completion. */
static void cdrom_pc_intr (ide_drive_t *drive)
{
  int ireason, len, stat, thislen;
  struct request *rq = HWGROUP(drive)->rq;
  struct packet_command *pc = (struct packet_command *)rq->buffer;

  /* Check for errors. */
  if (cdrom_decode_status (drive, 0, &stat)) return;

  /* Read the interrupt reason and the transfer length. */
  ireason = IN_BYTE (IDE_NSECTOR_REG);
  len = IN_BYTE (IDE_LCYL_REG) + 256 * IN_BYTE (IDE_HCYL_REG);

  /* If DRQ is clear, the command has completed.
     Complain if we still have data left to transfer. */
  if ((stat & DRQ_STAT) == 0)
    {
      /* Some of the trailing request sense fields are optional, and
	 some drives don't send them.  Sigh. */
      if (pc->c[0] == REQUEST_SENSE && pc->buflen > 0 && pc->buflen <= 5) {
	while (pc->buflen > 0) {
	  *pc->buffer++ = 0;
	  --pc->buflen;
	}
      }

      if (pc->buflen == 0)
        cdrom_end_request (1, drive);
      else
        {
          printk ("%s: cdrom_pc_intr: data underrun %d\n",
                  drive->name, pc->buflen);
          pc->stat = 1;
          cdrom_end_request (1, drive);
        }
      return;
    }

  /* Figure out how much data to transfer. */
  thislen = pc->buflen;
  if (thislen < 0) thislen = -thislen;
  if (thislen > len) thislen = len;

  /* The drive wants to be written to. */
  if ((ireason & 3) == 0)
    {
      /* Check that we want to write. */
      if (pc->buflen > 0)
        {
          printk ("%s: cdrom_pc_intr: Drive wants to transfer data the wrong way!\n",
                  drive->name);
          pc->stat = 1;
          thislen = 0;
        }

      /* Transfer the data. */
      cdrom_out_bytes (drive, pc->buffer, thislen);

      /* If we haven't moved enough data to satisfy the drive,
         add some padding. */
      while (len > thislen)
        {
          int dum = 0;
	  cdrom_out_bytes (drive, &dum, sizeof (dum));
          len -= sizeof (dum);
        }

      /* Keep count of how much data we've moved. */
      pc->buffer += thislen;
      pc->buflen += thislen;
    }

  /* Same drill for reading. */
  else if ((ireason & 3) == 2)
    {
      /* Check that we want to read. */
      if (pc->buflen < 0)
        {
          printk ("%s: cdrom_pc_intr: Drive wants to transfer data the wrong way!\n",
                  drive->name);
          pc->stat = 1;
          thislen = 0;
        }

      /* Transfer the data. */
      cdrom_in_bytes (drive, pc->buffer, thislen);

      /* If we haven't moved enough data to satisfy the drive,
         add some padding. */
      while (len > thislen)
        {
          int dum = 0;
	  cdrom_in_bytes (drive, &dum, sizeof (dum));
          len -= sizeof (dum);
        }

      /* Keep count of how much data we've moved. */
      pc->buffer += thislen;
      pc->buflen -= thislen;
    }

  else
    {
      printk ("%s: cdrom_pc_intr: The drive appears confused (ireason = 0x%2x)\n",
              drive->name, ireason);
      pc->stat = 1;
    }

  /* Now we wait for another interrupt. */
  ide_set_handler (drive, &cdrom_pc_intr, WAIT_CMD);
}


static void cdrom_do_pc_continuation (ide_drive_t *drive)
{
  struct request *rq = HWGROUP(drive)->rq;
  struct packet_command *pc = (struct packet_command *)rq->buffer;

  /* Send the command to the drive and return. */
  cdrom_transfer_packet_command (drive, pc->c, sizeof (pc->c), &cdrom_pc_intr);
}


static void cdrom_do_packet_command (ide_drive_t *drive)
{
  int len;
  struct request *rq = HWGROUP(drive)->rq;
  struct packet_command *pc = (struct packet_command *)rq->buffer;

  len = pc->buflen;
  if (len < 0) len = -len;

  pc->stat = 0;

  /* Start sending the command to the drive. */
  cdrom_start_packet_command (drive, len, cdrom_do_pc_continuation);
}

#ifndef MACH
/* Sleep for TIME jiffies.
   Not to be called from an interrupt handler. */
static
void cdrom_sleep (int time)
{
  current->state = TASK_INTERRUPTIBLE;
  current->timeout = jiffies + time;
  schedule ();
}
#endif

static
int cdrom_queue_packet_command (ide_drive_t *drive, struct packet_command *pc)
{
  struct atapi_request_sense my_reqbuf;
  int retries = 10;
  struct request req;

  /* If our caller has not provided a place to stick any sense data,
     use our own area. */
  if (pc->sense_data == NULL)
    pc->sense_data = &my_reqbuf;
  pc->sense_data->sense_key = 0;

  /* Start of retry loop. */
  do {
    ide_init_drive_cmd (&req);
    req.cmd = PACKET_COMMAND;
    req.buffer = (char *)pc;
    (void) ide_do_drive_cmd (drive, &req, ide_wait);

    if (pc->stat != 0)
      {
	/* The request failed.  Retry if it was due to a unit attention status
	   (usually means media was changed). */
	struct atapi_request_sense *reqbuf = pc->sense_data;

	if (reqbuf->sense_key == UNIT_ATTENTION)
	  ;

	/* Also retry if the drive is in the process of loading a disk.
	   This time, however, wait a little between retries to give
	   the drive time. */
	else if (reqbuf->sense_key == NOT_READY && reqbuf->asc == 4)
	  {
	    cdrom_sleep (HZ);
	  }

	/* Otherwise, don't retry. */
	else
	  retries = 0;

	--retries;
      }

    /* End of retry loop. */
  } while (pc->stat != 0 && retries >= 0);


  /* Return an error if the command failed. */
  if (pc->stat != 0)
    return -EIO;

  else
    {
      /* The command succeeded.  If it was anything other than a request sense,
	 eject, or door lock command, and we think that the door is presently
	 unlocked, lock it again.  (The door was probably unlocked via
	 an explicit CDROMEJECT ioctl.) */
      if (CDROM_STATE_FLAGS (drive)->door_locked == 0 &&
	  (pc->c[0] != REQUEST_SENSE &&
	   pc->c[0] != ALLOW_MEDIUM_REMOVAL &&
	   pc->c[0] != START_STOP))
	{
	  (void) cdrom_lockdoor (drive, 1, NULL);
	}
      return 0;
    }
}



/****************************************************************************
 * drive_cmd handling.
 *
 * Most of the functions accessed via drive_cmd are not valid for ATAPI
 * devices.  Only attempt to execute those which actually should be valid.
 */

static
void cdrom_do_drive_cmd (ide_drive_t *drive)
{
  struct request *rq = HWGROUP(drive)->rq;
  byte *args = rq->buffer;

  if (args)
    {
#if 0  /* This bit isn't done yet... */
      if (args[0] == WIN_SETFEATURES &&
	  (args[2] == 0x66 || args[2] == 0xcc || args[2] == 0x02 ||
	   args[2] == 0xdd || args[2] == 0x5d))
	{
	  OUT_BYTE (args[2], io_base + IDE_FEATURE_OFFSET);
	  <send cmd>
	}
      else
#endif
	{
	  printk ("%s: Unsupported drive command %02x %02x %02x\n",
		  drive->name, args[0], args[1], args[2]);
	  rq->errors = 1;
	}
    }

  cdrom_end_request (1, drive);
}



/****************************************************************************
 * cdrom driver request routine.
 */

void ide_do_rw_cdrom (ide_drive_t *drive, unsigned long block)
{
  struct request *rq = HWGROUP(drive)->rq;

  if (rq -> cmd == PACKET_COMMAND || rq -> cmd == REQUEST_SENSE_COMMAND)
    cdrom_do_packet_command (drive);

  else if (rq -> cmd == RESET_DRIVE_COMMAND)
    {
      cdrom_end_request (1, drive);
      ide_do_reset (drive);
      return;
    }

  else if (rq -> cmd == IDE_DRIVE_CMD)
    cdrom_do_drive_cmd (drive);

  else if (rq -> cmd != READ)
    {
      printk ("ide-cd: bad cmd %d\n", rq -> cmd);
      cdrom_end_request (0, drive);
    }
  else
    cdrom_start_read (drive, block);
}



/****************************************************************************
 * Ioctl handling.
 *
 * Routines which queue packet commands take as a final argument a pointer
 * to an atapi_request_sense struct.  If execution of the command results
 * in an error with a CHECK CONDITION status, this structure will be filled
 * with the results of the subsequent request sense command.  The pointer
 * can also be NULL, in which case no sense information is returned.
 */

#if ! STANDARD_ATAPI
static
int bin2bcd (int x)
{
  return (x%10) | ((x/10) << 4);
}


static
int bcd2bin (int x)
{
  return (x >> 4) * 10 + (x & 0x0f);
}
#endif /* not STANDARD_ATAPI */


static inline
void lba_to_msf (int lba, byte *m, byte *s, byte *f)
{
  lba += CD_BLOCK_OFFSET;
  lba &= 0xffffff;  /* negative lbas use only 24 bits */
  *m = lba / (CD_SECS * CD_FRAMES);
  lba %= (CD_SECS * CD_FRAMES);
  *s = lba / CD_FRAMES;
  *f = lba % CD_FRAMES;
}


static inline
int msf_to_lba (byte m, byte s, byte f)
{
  return (((m * CD_SECS) + s) * CD_FRAMES + f) - CD_BLOCK_OFFSET;
}


static int
cdrom_check_status (ide_drive_t  *drive,
		    struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));

  pc.sense_data = reqbuf;
  pc.c[0] = TEST_UNIT_READY;

  return cdrom_queue_packet_command (drive, &pc);
}


/* Lock the door if LOCKFLAG is nonzero; unlock it otherwise. */
static int
cdrom_lockdoor (ide_drive_t *drive, int lockflag,
		struct atapi_request_sense *reqbuf)
{
  struct atapi_request_sense my_reqbuf;
  int stat;
  struct packet_command pc;

  if (reqbuf == NULL)
    reqbuf = &my_reqbuf;

  /* If the drive cannot lock the door, just pretend. */
  if (CDROM_CONFIG_FLAGS (drive)->no_doorlock)
    stat = 0;
  else
    {
      memset (&pc, 0, sizeof (pc));
      pc.sense_data = reqbuf;

      pc.c[0] = ALLOW_MEDIUM_REMOVAL;
      pc.c[4] = (lockflag != 0);
      stat = cdrom_queue_packet_command (drive, &pc);
    }

  if (stat == 0)
    CDROM_STATE_FLAGS (drive)->door_locked = lockflag;
  else
    {
      /* If we got an illegal field error, the drive
	 probably cannot lock the door. */
      if (reqbuf->sense_key == ILLEGAL_REQUEST && reqbuf->asc == 0x24)
	{
	  printk ("%s: door locking not supported\n", drive->name);
	  CDROM_CONFIG_FLAGS (drive)->no_doorlock = 1;
	  stat = 0;
	  CDROM_STATE_FLAGS (drive)->door_locked = lockflag;
	}
    }
  return stat;
}


/* Eject the disk if EJECTFLAG is 0.
   If EJECTFLAG is 1, try to reload the disk. */
static int
cdrom_eject (ide_drive_t *drive, int ejectflag,
	     struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = START_STOP;
  pc.c[4] = 2 + (ejectflag != 0);
  return cdrom_queue_packet_command (drive, &pc);
}


static int
cdrom_pause (ide_drive_t *drive, int pauseflag,
	     struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = SCMD_PAUSE_RESUME;
  pc.c[8] = !pauseflag;
  return cdrom_queue_packet_command (drive, &pc);
}


static int
cdrom_startstop (ide_drive_t *drive, int startflag,
		 struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = START_STOP;
  pc.c[1] = 1;
  pc.c[4] = startflag;
  return cdrom_queue_packet_command (drive, &pc);
}


static int
cdrom_read_capacity (ide_drive_t *drive, unsigned *capacity,
		     struct atapi_request_sense *reqbuf)
{
  struct {
    unsigned lba;
    unsigned blocklen;
  } capbuf;

  int stat;
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = READ_CAPACITY;
  pc.buffer = (char *)&capbuf;
  pc.buflen = sizeof (capbuf);

  stat = cdrom_queue_packet_command (drive, &pc);
  if (stat == 0)
    {
      *capacity = ntohl (capbuf.lba);
    }

  return stat;
}


static int
cdrom_read_tocentry (ide_drive_t *drive, int trackno, int msf_flag,
                     int format, char *buf, int buflen,
		     struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.buffer =  buf;
  pc.buflen = buflen;
  pc.c[0] = SCMD_READ_TOC;
  pc.c[6] = trackno;
  pc.c[7] = (buflen >> 8);
  pc.c[8] = (buflen & 0xff);
  pc.c[9] = (format << 6);
  if (msf_flag) pc.c[1] = 2;
  return cdrom_queue_packet_command (drive, &pc);
}


/* Try to read the entire TOC for the disk into our internal buffer. */
static int
cdrom_read_toc (ide_drive_t *drive,
		struct atapi_request_sense *reqbuf)
{
  int msf_flag;
  int stat, ntracks, i;
  struct atapi_toc *toc = drive->cdrom_info.toc;
  struct {
    struct atapi_toc_header hdr;
    struct atapi_toc_entry  ent;
  } ms_tmp;

  if (toc == NULL)
    {
      /* Try to allocate space. */
      toc = (struct atapi_toc *) kmalloc (sizeof (struct atapi_toc),
                                          GFP_KERNEL);
      drive->cdrom_info.toc = toc;
    }

  if (toc == NULL)
    {
      printk ("%s: No cdrom TOC buffer!\n", drive->name);
      return -EIO;
    }

  /* Check to see if the existing data is still valid.
     If it is, just return. */
  if (CDROM_STATE_FLAGS (drive)->toc_valid)
    (void) cdrom_check_status (drive, NULL);

  if (CDROM_STATE_FLAGS (drive)->toc_valid) return 0;

#if STANDARD_ATAPI
  msf_flag = 0;
#else  /* not STANDARD_ATAPI */
  /* Some drives can't return TOC data in LBA format. */
  msf_flag = (CDROM_CONFIG_FLAGS (drive)->no_lba_toc);
#endif  /* not STANDARD_ATAPI */

  /* First read just the header, so we know how long the TOC is. */
  stat = cdrom_read_tocentry (drive, 0, msf_flag, 0, (char *)&toc->hdr,
                              sizeof (struct atapi_toc_header) +
                              sizeof (struct atapi_toc_entry),
			      reqbuf);
  if (stat) return stat;

#if ! STANDARD_ATAPI
  if (CDROM_CONFIG_FLAGS (drive)->vertos_lossage)
    {
      toc->hdr.first_track = bcd2bin (toc->hdr.first_track);
      toc->hdr.last_track  = bcd2bin (toc->hdr.last_track);
      /* hopefully the length is not BCD, too ;-| */
    }
#endif  /* not STANDARD_ATAPI */

  ntracks = toc->hdr.last_track - toc->hdr.first_track + 1;
  if (ntracks <= 0) return -EIO;
  if (ntracks > MAX_TRACKS) ntracks = MAX_TRACKS;

  /* Now read the whole schmeer. */
  stat = cdrom_read_tocentry (drive, 0, msf_flag, 0, (char *)&toc->hdr,
                              sizeof (struct atapi_toc_header) +
                              (ntracks+1) * sizeof (struct atapi_toc_entry),
			      reqbuf);
  if (stat) return stat;
  toc->hdr.toc_length = ntohs (toc->hdr.toc_length);

#if ! STANDARD_ATAPI
  if (CDROM_CONFIG_FLAGS (drive)->vertos_lossage)
    {
      toc->hdr.first_track = bcd2bin (toc->hdr.first_track);
      toc->hdr.last_track  = bcd2bin (toc->hdr.last_track);
      /* hopefully the length is not BCD, too ;-| */
    }
#endif  /* not STANDARD_ATAPI */

  for (i=0; i<=ntracks; i++)
    {
#if ! STANDARD_ATAPI
      if (msf_flag)
	{
	  if (CDROM_CONFIG_FLAGS (drive)->vertos_lossage)
	    {
	      toc->ent[i].track = bcd2bin (toc->ent[i].track);
	      toc->ent[i].addr.msf.m = bcd2bin (toc->ent[i].addr.msf.m);
	      toc->ent[i].addr.msf.s = bcd2bin (toc->ent[i].addr.msf.s);
	      toc->ent[i].addr.msf.f = bcd2bin (toc->ent[i].addr.msf.f);
	    }
	  toc->ent[i].addr.lba = msf_to_lba (toc->ent[i].addr.msf.m,
					     toc->ent[i].addr.msf.s,
					     toc->ent[i].addr.msf.f);
	}
      else
#endif  /* not STANDARD_ATAPI */
	toc->ent[i].addr.lba = ntohl (toc->ent[i].addr.lba);
    }

  /* Read the multisession information. */
  stat = cdrom_read_tocentry (drive, 0, msf_flag, 1,
			      (char *)&ms_tmp, sizeof (ms_tmp),
			      reqbuf);
  if (stat) return stat;
#if ! STANDARD_ATAPI
  if (msf_flag)
    toc->last_session_lba = msf_to_lba (ms_tmp.ent.addr.msf.m,
					ms_tmp.ent.addr.msf.s,
					ms_tmp.ent.addr.msf.f);
  else
#endif  /* not STANDARD_ATAPI */
    toc->last_session_lba = ntohl (ms_tmp.ent.addr.lba);

  toc->xa_flag = (ms_tmp.hdr.first_track != ms_tmp.hdr.last_track);

  /* Now try to get the total cdrom capacity. */
  stat = cdrom_read_capacity (drive, &toc->capacity, reqbuf);
  if (stat) toc->capacity = 0x1fffff;

  HWIF(drive)->gd->sizes[drive->select.b.unit << PARTN_BITS]
    = toc->capacity * SECTORS_PER_FRAME;
  drive->part[0].nr_sects = toc->capacity * SECTORS_PER_FRAME;

  /* Remember that we've read this stuff. */
  CDROM_STATE_FLAGS (drive)->toc_valid = 1;

  return 0;
}


static int
cdrom_read_subchannel (ide_drive_t *drive,
                       char *buf, int buflen,
		       struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.buffer =  buf;
  pc.buflen = buflen;
  pc.c[0] = SCMD_READ_SUBCHANNEL;
  pc.c[2] = 0x40;  /* request subQ data */
  pc.c[3] = 0x01;  /* Format 1: current position */
  pc.c[7] = (buflen >> 8);
  pc.c[8] = (buflen & 0xff);
  return cdrom_queue_packet_command (drive, &pc);
}


/* modeflag: 0 = current, 1 = changeable mask, 2 = default, 3 = saved */
static int
cdrom_mode_sense (ide_drive_t *drive, int pageno, int modeflag,
                  char *buf, int buflen,
		  struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.buffer =  buf;
  pc.buflen = buflen;
  pc.c[0] = MODE_SENSE_10;
  pc.c[2] = pageno | (modeflag << 6);
  pc.c[7] = (buflen >> 8);
  pc.c[8] = (buflen & 0xff);
  return cdrom_queue_packet_command (drive, &pc);
}


static int
cdrom_mode_select (ide_drive_t *drive, int pageno, char *buf, int buflen,
		   struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.buffer =  buf;
  pc.buflen = - buflen;
  pc.c[0] = MODE_SELECT_10;
  pc.c[1] = 0x10;
  pc.c[2] = pageno;
  pc.c[7] = (buflen >> 8);
  pc.c[8] = (buflen & 0xff);
  return cdrom_queue_packet_command (drive, &pc);
}


static int
cdrom_play_lba_range_play12 (ide_drive_t *drive, int lba_start, int lba_end,
			     struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = SCMD_PLAYAUDIO12;
#ifdef __alpha__
  stq_u(((long) htonl (lba_end - lba_start) << 32) | htonl(lba_start),
	(unsigned long *) &pc.c[2]);
#else
  *(int *)(&pc.c[2]) = htonl (lba_start);
  *(int *)(&pc.c[6]) = htonl (lba_end - lba_start);
#endif

  return cdrom_queue_packet_command (drive, &pc);
}


#if !  STANDARD_ATAPI
static int
cdrom_play_lba_range_msf (ide_drive_t *drive, int lba_start, int lba_end,
			  struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.c[0] = SCMD_PLAYAUDIO_MSF;
  lba_to_msf (lba_start, &pc.c[3], &pc.c[4], &pc.c[5]);
  lba_to_msf (lba_end-1, &pc.c[6], &pc.c[7], &pc.c[8]);

  if (CDROM_CONFIG_FLAGS (drive)->playmsf_uses_bcd)
    {
      pc.c[3] = bin2bcd (pc.c[3]);
      pc.c[4] = bin2bcd (pc.c[4]);
      pc.c[5] = bin2bcd (pc.c[5]);
      pc.c[6] = bin2bcd (pc.c[6]);
      pc.c[7] = bin2bcd (pc.c[7]);
      pc.c[8] = bin2bcd (pc.c[8]);
    }

  return cdrom_queue_packet_command (drive, &pc);
}
#endif  /* not STANDARD_ATAPI */


static int
cdrom_play_lba_range_1 (ide_drive_t *drive, int lba_start, int lba_end,
			struct atapi_request_sense *reqbuf)
{
  /* This is rather annoying.
     My NEC-260 won't recognize group 5 commands such as PLAYAUDIO12;
     the only way to get it to play more than 64k of blocks at once
     seems to be the PLAYAUDIO_MSF command.  However, the parameters
     the NEC 260 wants for the PLAYMSF command are incompatible with
     the new version of the spec.

     So what i'll try is this.  First try for PLAYAUDIO12.  If it works,
     great.  Otherwise, if the drive reports an illegal command code,
     try PLAYAUDIO_MSF using the NEC 260-style bcd parameters. */

#if ! STANDARD_ATAPI
  if (CDROM_CONFIG_FLAGS (drive)->no_playaudio12)
    return cdrom_play_lba_range_msf (drive, lba_start, lba_end, reqbuf);
  else
#endif  /* not STANDARD_ATAPI */
    {
      int stat;
      struct atapi_request_sense my_reqbuf;

      if (reqbuf == NULL)
	reqbuf = &my_reqbuf;

      stat = cdrom_play_lba_range_play12 (drive, lba_start, lba_end, reqbuf);
      if (stat == 0) return 0;

#if ! STANDARD_ATAPI
      /* It failed.  Try to find out why. */
      if (reqbuf->sense_key == ILLEGAL_REQUEST && reqbuf->asc == 0x20)
        {
          /* The drive didn't recognize the command.
             Retry with the MSF variant. */
          printk ("%s: Drive does not support PLAYAUDIO12; "
                  "trying PLAYAUDIO_MSF\n", drive->name);
          CDROM_CONFIG_FLAGS (drive)->no_playaudio12 = 1;
          CDROM_CONFIG_FLAGS (drive)->playmsf_uses_bcd = 1;
          return cdrom_play_lba_range_msf (drive, lba_start, lba_end, reqbuf);
        }
#endif  /* not STANDARD_ATAPI */

      /* Failed for some other reason.  Give up. */
      return stat;
    }
}


/* Play audio starting at LBA LBA_START and finishing with the
   LBA before LBA_END. */
static int
cdrom_play_lba_range (ide_drive_t *drive, int lba_start, int lba_end,
		      struct atapi_request_sense *reqbuf)
{
  int i, stat;
  struct atapi_request_sense my_reqbuf;

  if (reqbuf == NULL)
    reqbuf = &my_reqbuf;

  /* Some drives, will, for certain audio cds,
     give an error if you ask them to play the entire cd using the
     values which are returned in the TOC.  The play will succeed, however,
     if the ending address is adjusted downwards by a few frames. */
  for (i=0; i<75; i++)
    {
      stat = cdrom_play_lba_range_1 (drive, lba_start, lba_end, reqbuf);

      if (stat == 0 ||
          !(reqbuf->sense_key == ILLEGAL_REQUEST && reqbuf->asc == 0x24))
	return stat;

      --lba_end;
      if (lba_end <= lba_start) break;
    }

  return stat;
}


static
int cdrom_get_toc_entry (ide_drive_t *drive, int track,
                         struct atapi_toc_entry **ent,
			 struct atapi_request_sense *reqbuf)
{
  int stat, ntracks;
  struct atapi_toc *toc;

  /* Make sure our saved TOC is valid. */
  stat = cdrom_read_toc (drive, reqbuf);
  if (stat) return stat;

  toc = drive->cdrom_info.toc;

  /* Check validity of requested track number. */
  ntracks = toc->hdr.last_track - toc->hdr.first_track + 1;
  if (track == CDROM_LEADOUT)
    *ent = &toc->ent[ntracks];
  else if (track < toc->hdr.first_track ||
           track > toc->hdr.last_track)
    return -EINVAL;
  else
    *ent = &toc->ent[track - toc->hdr.first_track];

  return 0;
}


static int
cdrom_read_block (ide_drive_t *drive, int format, int lba,
		  char *buf, int buflen,
		  struct atapi_request_sense *reqbuf)
{
  struct packet_command pc;
  struct atapi_request_sense my_reqbuf;
  int stat;

  if (reqbuf == NULL)
    reqbuf = &my_reqbuf;

  memset (&pc, 0, sizeof (pc));
  pc.sense_data = reqbuf;

  pc.buffer = buf;
  pc.buflen = buflen;

#if ! STANDARD_ATAPI
  if (CDROM_CONFIG_FLAGS (drive)->old_readcd)
    pc.c[0] = 0xd4;
  else
#endif  /* not STANDARD_ATAPI */
    pc.c[0] = READ_CD;

  pc.c[1] = (format << 2);
#ifdef __alpha__
  stl_u(htonl (lba), (unsigned int *) &pc.c[2]);
#else
  *(int *)(&pc.c[2]) = htonl (lba);
#endif
  pc.c[8] = 1;  /* one block */
  pc.c[9] = 0x10;

  stat = cdrom_queue_packet_command (drive, &pc);

#if ! STANDARD_ATAPI
  /* If the drive doesn't recognize the READ CD opcode, retry the command
     with an older opcode for that command. */
  if (stat && reqbuf->sense_key == ILLEGAL_REQUEST && reqbuf->asc == 0x20 &&
      CDROM_CONFIG_FLAGS (drive)->old_readcd == 0)
    {
      printk ("%s: Drive does not recognize READ_CD; trying opcode 0xd4\n",
	      drive->name);
      CDROM_CONFIG_FLAGS (drive)->old_readcd = 1;
      return cdrom_read_block (drive, format, lba, buf, buflen, reqbuf);
    }
#endif  /* not STANDARD_ATAPI */

  return stat;
}


int ide_cdrom_ioctl (ide_drive_t *drive, struct inode *inode,
		     struct file *file, unsigned int cmd, unsigned long arg)
{
  switch (cmd)
    {
    case CDROMEJECT:
      {
	int stat;

	if (drive->usage > 1)
	  return -EBUSY;

	stat = cdrom_lockdoor (drive, 0, NULL);
	if (stat) return stat;

	return cdrom_eject (drive, 0, NULL);
      }

    case CDROMEJECT_SW:
      {
	CDROM_STATE_FLAGS (drive)->eject_on_close = arg;
	return 0;
      }

    case CDROMPAUSE:
      return cdrom_pause (drive, 1, NULL);

    case CDROMRESUME:
      return cdrom_pause (drive, 0, NULL);

    case CDROMSTART:
      return cdrom_startstop (drive, 1, NULL);

    case CDROMSTOP:
      {
	int stat;

	stat = cdrom_startstop (drive, 0, NULL);
	if (stat) return stat;
	/* pit says the Dolphin needs this. */
	return cdrom_eject (drive, 1, NULL);
      }

    case CDROMPLAYMSF:
      {
        struct cdrom_msf msf;
        int stat, lba_start, lba_end;

        stat = verify_area (VERIFY_READ, (void *)arg, sizeof (msf));
        if (stat) return stat;

        memcpy_fromfs (&msf, (void *) arg, sizeof(msf));

        lba_start = msf_to_lba (msf.cdmsf_min0, msf.cdmsf_sec0,
                                msf.cdmsf_frame0);
        lba_end = msf_to_lba (msf.cdmsf_min1, msf.cdmsf_sec1,
                              msf.cdmsf_frame1) + 1;

        if (lba_end <= lba_start) return -EINVAL;

        return cdrom_play_lba_range (drive, lba_start, lba_end, NULL);
      }

    /* Like just about every other Linux cdrom driver, we ignore the
       index part of the request here. */
    case CDROMPLAYTRKIND:
      {
        int stat, lba_start, lba_end;
        struct cdrom_ti ti;
        struct atapi_toc_entry *first_toc, *last_toc;

        stat = verify_area (VERIFY_READ, (void *)arg, sizeof (ti));
        if (stat) return stat;

        memcpy_fromfs (&ti, (void *) arg, sizeof(ti));

        stat = cdrom_get_toc_entry (drive, ti.cdti_trk0, &first_toc, NULL);
        if (stat) return stat;
        stat = cdrom_get_toc_entry (drive, ti.cdti_trk1, &last_toc, NULL);
        if (stat) return stat;

        if (ti.cdti_trk1 != CDROM_LEADOUT) ++last_toc;
        lba_start = first_toc->addr.lba;
        lba_end   = last_toc->addr.lba;

        if (lba_end <= lba_start) return -EINVAL;

        return cdrom_play_lba_range (drive, lba_start, lba_end, NULL);
      }

    case CDROMREADTOCHDR:
      {
        int stat;
        struct cdrom_tochdr tochdr;
        struct atapi_toc *toc;

        stat = verify_area (VERIFY_WRITE, (void *) arg, sizeof (tochdr));
        if (stat) return stat;

        /* Make sure our saved TOC is valid. */
        stat = cdrom_read_toc (drive, NULL);
        if (stat) return stat;

        toc = drive->cdrom_info.toc;
        tochdr.cdth_trk0 = toc->hdr.first_track;
        tochdr.cdth_trk1 = toc->hdr.last_track;

        memcpy_tofs ((void *) arg, &tochdr, sizeof (tochdr));

        return stat;
      }

    case CDROMREADTOCENTRY:
      {
        int stat;
        struct cdrom_tocentry tocentry;
        struct atapi_toc_entry *toce;

        stat = verify_area (VERIFY_READ, (void *) arg, sizeof (tocentry));
        if (stat) return stat;
        stat = verify_area (VERIFY_WRITE, (void *) arg, sizeof (tocentry));
        if (stat) return stat;

        memcpy_fromfs (&tocentry, (void *) arg, sizeof (tocentry));

        stat = cdrom_get_toc_entry (drive, tocentry.cdte_track, &toce, NULL);
        if (stat) return stat;

        tocentry.cdte_ctrl = toce->control;
        tocentry.cdte_adr  = toce->adr;

        if (tocentry.cdte_format == CDROM_MSF)
          {
            /* convert to MSF */
            lba_to_msf (toce->addr.lba,
                        &tocentry.cdte_addr.msf.minute,
                        &tocentry.cdte_addr.msf.second,
                        &tocentry.cdte_addr.msf.frame);
          }
        else
          tocentry.cdte_addr.lba = toce->addr.lba;

        memcpy_tofs ((void *) arg, &tocentry, sizeof (tocentry));

        return stat;
      }

    case CDROMSUBCHNL:
      {
        struct atapi_cdrom_subchnl scbuf;
        int stat, abs_lba, rel_lba;
        struct cdrom_subchnl subchnl;

        stat = verify_area (VERIFY_WRITE, (void *) arg, sizeof (subchnl));
        if (stat) return stat;
        stat = verify_area (VERIFY_READ, (void *) arg, sizeof (subchnl));
        if (stat) return stat;

        memcpy_fromfs (&subchnl, (void *) arg, sizeof (subchnl));

        stat = cdrom_read_subchannel (drive, (char *)&scbuf, sizeof (scbuf),
				      NULL);
        if (stat) return stat;

#if ! STANDARD_ATAPI
	if (CDROM_CONFIG_FLAGS (drive)->vertos_lossage)
	  {
	    abs_lba = msf_to_lba (bcd2bin (scbuf.acdsc_absaddr.msf.minute),
				  bcd2bin (scbuf.acdsc_absaddr.msf.second),
				  bcd2bin (scbuf.acdsc_absaddr.msf.frame));
	    rel_lba = msf_to_lba (bcd2bin (scbuf.acdsc_reladdr.msf.minute),
				  bcd2bin (scbuf.acdsc_reladdr.msf.second),
				  bcd2bin (scbuf.acdsc_reladdr.msf.frame));
	    scbuf.acdsc_trk = bcd2bin (scbuf.acdsc_trk);
	  }
	else
#endif /* not STANDARD_ATAPI */
	  {
	    abs_lba = ntohl (scbuf.acdsc_absaddr.lba);
	    rel_lba = ntohl (scbuf.acdsc_reladdr.lba);
	  }

        if (subchnl.cdsc_format == CDROM_MSF)
          {
            lba_to_msf (abs_lba,
                        &subchnl.cdsc_absaddr.msf.minute,
                        &subchnl.cdsc_absaddr.msf.second,
                        &subchnl.cdsc_absaddr.msf.frame);
            lba_to_msf (rel_lba,
                        &subchnl.cdsc_reladdr.msf.minute,
                        &subchnl.cdsc_reladdr.msf.second,
                        &subchnl.cdsc_reladdr.msf.frame);
          }
        else
          {
            subchnl.cdsc_absaddr.lba = abs_lba;
            subchnl.cdsc_reladdr.lba = rel_lba;
          }

        subchnl.cdsc_audiostatus = scbuf.acdsc_audiostatus;
        subchnl.cdsc_ctrl = scbuf.acdsc_ctrl;
        subchnl.cdsc_trk  = scbuf.acdsc_trk;
        subchnl.cdsc_ind  = scbuf.acdsc_ind;

        memcpy_tofs ((void *) arg, &subchnl, sizeof (subchnl));

        return stat;
      }

    case CDROMVOLCTRL:
      {
        struct cdrom_volctrl volctrl;
        char buffer[24], mask[24];
        int stat;

        stat = verify_area (VERIFY_READ, (void *) arg, sizeof (volctrl));
        if (stat) return stat;
        memcpy_fromfs (&volctrl, (void *) arg, sizeof (volctrl));

        stat = cdrom_mode_sense (drive, 0x0e, 0, buffer, sizeof (buffer),NULL);
        if (stat) return stat;
        stat = cdrom_mode_sense (drive, 0x0e, 1, mask  , sizeof (buffer),NULL);
        if (stat) return stat;

        buffer[1] = buffer[2] = 0;

        buffer[17] = volctrl.channel0 & mask[17];
        buffer[19] = volctrl.channel1 & mask[19];
        buffer[21] = volctrl.channel2 & mask[21];
        buffer[23] = volctrl.channel3 & mask[23];

        return cdrom_mode_select (drive, 0x0e, buffer, sizeof (buffer), NULL);
      }

    case CDROMVOLREAD:
      {
        struct cdrom_volctrl volctrl;
        char buffer[24];
        int stat;

        stat = verify_area (VERIFY_WRITE, (void *) arg, sizeof (volctrl));
        if (stat) return stat;

        stat = cdrom_mode_sense (drive, 0x0e, 0, buffer, sizeof (buffer), NULL);
        if (stat) return stat;

        volctrl.channel0 = buffer[17];
        volctrl.channel1 = buffer[19];
        volctrl.channel2 = buffer[21];
        volctrl.channel3 = buffer[23];

        memcpy_tofs ((void *) arg, &volctrl, sizeof (volctrl));

	return 0;
      }

    case CDROMMULTISESSION:
      {
	struct cdrom_multisession ms_info;
	struct atapi_toc *toc;
	int stat;

	stat = verify_area (VERIFY_READ,  (void *)arg, sizeof (ms_info));
        if (stat) return stat;
	stat = verify_area (VERIFY_WRITE, (void *)arg, sizeof (ms_info));
        if (stat) return stat;

	memcpy_fromfs (&ms_info, (void *)arg, sizeof (ms_info));

	/* Make sure the TOC information is valid. */
	stat = cdrom_read_toc (drive, NULL);
	if (stat) return stat;

	toc = drive->cdrom_info.toc;

	if (ms_info.addr_format == CDROM_MSF)
	  lba_to_msf (toc->last_session_lba,
		      &ms_info.addr.msf.minute,
		      &ms_info.addr.msf.second,
		      &ms_info.addr.msf.frame);

	else if (ms_info.addr_format == CDROM_LBA)
	  ms_info.addr.lba = toc->last_session_lba;

	else
	  return -EINVAL;

	ms_info.xa_flag = toc->xa_flag;

	memcpy_tofs ((void *)arg, &ms_info, sizeof (ms_info));

	return 0;
      }

    /* Read 2352 byte blocks from audio tracks. */
    case CDROMREADAUDIO:
      {
	int stat, lba;
	struct atapi_toc *toc;
	struct cdrom_read_audio ra;
	char buf[CD_FRAMESIZE_RAW];

	/* Make sure the TOC is up to date. */
	stat = cdrom_read_toc (drive, NULL);
	if (stat) return stat;

	toc = drive->cdrom_info.toc;

	stat = verify_area (VERIFY_READ, (char *)arg, sizeof (ra));
	if (stat) return stat;

	memcpy_fromfs (&ra, (void *)arg, sizeof (ra));

	if (ra.nframes < 0 || ra.nframes > toc->capacity)
	  return -EINVAL;
	else if (ra.nframes == 0)
	  return 0;

	stat = verify_area (VERIFY_WRITE, (char *)ra.buf,
			                  ra.nframes * CD_FRAMESIZE_RAW);
	if (stat) return stat;

	if (ra.addr_format == CDROM_MSF)
	  lba = msf_to_lba (ra.addr.msf.minute, ra.addr.msf.second,
			    ra.addr.msf.frame);
	
	else if (ra.addr_format == CDROM_LBA)
	  lba = ra.addr.lba;

	else
	  return -EINVAL;

	if (lba < 0 || lba >= toc->capacity)
	  return -EINVAL;

	while (ra.nframes > 0)
	  {
	    stat = cdrom_read_block (drive, 1, lba, buf,
				     CD_FRAMESIZE_RAW, NULL);
	    if (stat) return stat;
	    memcpy_tofs (ra.buf, buf, CD_FRAMESIZE_RAW);
	    ra.buf += CD_FRAMESIZE_RAW;
	    --ra.nframes;
	    ++lba;
	  }

	return 0;
      }

    case CDROMREADMODE1:
    case CDROMREADMODE2:
      {
	struct cdrom_msf msf;
	int blocksize, format, stat, lba;
	struct atapi_toc *toc;
	char buf[CD_FRAMESIZE_RAW0];

	if (cmd == CDROMREADMODE1)
	  {
	    blocksize = CD_FRAMESIZE;
	    format = 2;
	  }
	else
	  {
	    blocksize = CD_FRAMESIZE_RAW0;
	    format = 3;
	  }

	stat = verify_area (VERIFY_READ, (char *)arg, sizeof (msf));
	if (stat) return stat;
	stat = verify_area (VERIFY_WRITE, (char *)arg, blocksize);
	if (stat) return stat;

	memcpy_fromfs (&msf, (void *)arg, sizeof (msf));

	lba = msf_to_lba (msf.cdmsf_min0, msf.cdmsf_sec0, msf.cdmsf_frame0);
	
	/* Make sure the TOC is up to date. */
	stat = cdrom_read_toc (drive, NULL);
	if (stat) return stat;

	toc = drive->cdrom_info.toc;

	if (lba < 0 || lba >= toc->capacity)
	  return -EINVAL;

	stat = cdrom_read_block (drive, format, lba, buf, blocksize, NULL);
	if (stat) return stat;

	memcpy_tofs ((char *)arg, buf, blocksize);
	return 0;
      }

#if 0 /* Doesn't work reliably yet. */
    case CDROMRESET:
      {
	struct request req;
	ide_init_drive_cmd (&req);
	req.cmd = RESET_DRIVE_COMMAND;
	return ide_do_drive_cmd (drive, &req, ide_wait);
      }
#endif

 
#ifdef TEST
    case 0x1234:
      {
        int stat;
        struct packet_command pc;
	int len, lena;

        memset (&pc, 0, sizeof (pc));

        stat = verify_area (VERIFY_READ, (void *) arg, sizeof (pc.c));
        if (stat) return stat;
        memcpy_fromfs (&pc.c, (void *) arg, sizeof (pc.c));
	arg += sizeof (pc.c);

	stat = verify_area (VERIFY_READ, (void *) arg, sizeof (len));
        if (stat) return stat;
        memcpy_fromfs (&len, (void *) arg , sizeof (len));
	arg += sizeof (len);

	if (len > 0) {
	  stat = verify_area (VERIFY_WRITE, (void *) arg, len);
	  if (stat) return stat;
	}

	lena = len;
	if (lena  < 0) lena = 0;

	{
	  char buf[lena];
	  if (len > 0) {
	    pc.buflen = len;
	    pc.buffer = buf;
	  }

	  stat = cdrom_queue_packet_command (drive, &pc);

	  if (len > 0)
	    memcpy_tofs ((void *)arg, buf, len);
	}

        return stat;
      }
#endif

    default:
      return -EPERM;
    }

}



/****************************************************************************
 * Other driver requests (open, close, check media change).
 */

int ide_cdrom_check_media_change (ide_drive_t *drive)
{
  int retval;

  (void) cdrom_check_status (drive, NULL);

  retval = CDROM_STATE_FLAGS (drive)->media_changed;
  CDROM_STATE_FLAGS (drive)->media_changed = 0;

  return retval;
}


int ide_cdrom_open (struct inode *ip, struct file *fp, ide_drive_t *drive)
{
  /* no write access */
  if (fp->f_mode & 2)
    {
      --drive->usage;
      return -EROFS;
    }

  /* If this is the first open, check the drive status. */
  if (drive->usage == 1)
    {
      int stat;
      struct atapi_request_sense my_reqbuf;
      my_reqbuf.sense_key = 0;

      /* Get the drive status. */
      stat = cdrom_check_status (drive, &my_reqbuf);

      /* If the tray is open, try to close it. */
      if (stat && my_reqbuf.sense_key == NOT_READY)
	{
	  cdrom_eject (drive, 1, &my_reqbuf);
	  stat = cdrom_check_status (drive, &my_reqbuf);
	}

      /* Return an error if there are still problems. */
      if (stat && my_reqbuf.sense_key != UNIT_ATTENTION)
	{
	  --drive->usage;
	  return -ENXIO;
	}

      /* Now lock the door. */
      (void) cdrom_lockdoor (drive, 1, &my_reqbuf);

      /* And try to read the TOC information now. */
      (void) cdrom_read_toc (drive, &my_reqbuf);
    }

  return 0;
}


/*
 * Close down the device.  Invalidate all cached blocks.
 */

void ide_cdrom_release (struct inode *inode, struct file *file, ide_drive_t *drive)
{
  if (drive->usage == 0)
    {
      invalidate_buffers (inode->i_rdev);

      /* Unlock the door. */
      (void) cdrom_lockdoor (drive, 0, NULL);

      /* Do an eject if we were requested to do so. */
      if (CDROM_STATE_FLAGS (drive)->eject_on_close)
	(void) cdrom_eject (drive, 0, NULL);
    }
}



/****************************************************************************
 * Device initialization.
 */

void ide_cdrom_setup (ide_drive_t *drive)
{
  blksize_size[HWIF(drive)->major][drive->select.b.unit << PARTN_BITS] = CD_FRAMESIZE;

  drive->special.all = 0;
  drive->ready_stat = 0;

  CDROM_STATE_FLAGS (drive)->media_changed = 0;
  CDROM_STATE_FLAGS (drive)->toc_valid     = 0;
  CDROM_STATE_FLAGS (drive)->door_locked   = 0;

  /* Turn this off by default, since many people don't like it. */
  CDROM_STATE_FLAGS (drive)->eject_on_close= 0;

#if NO_DOOR_LOCKING
  CDROM_CONFIG_FLAGS (drive)->no_doorlock = 1;
#else
  CDROM_CONFIG_FLAGS (drive)->no_doorlock = 0;
#endif

  if (drive->id != NULL) {
    CDROM_CONFIG_FLAGS (drive)->drq_interrupt =
      ((drive->id->config & 0x0060) == 0x20);
  } else {
    CDROM_CONFIG_FLAGS (drive)->drq_interrupt = 0;
  }

#if ! STANDARD_ATAPI
  CDROM_CONFIG_FLAGS (drive)->no_playaudio12 = 0;
  CDROM_CONFIG_FLAGS (drive)->old_readcd = 0;
  CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 0;
  CDROM_CONFIG_FLAGS (drive)->playmsf_uses_bcd = 0;
  CDROM_CONFIG_FLAGS (drive)->vertos_lossage = 0;

  if (drive->id != NULL) {
    /* Accommodate some broken drives... */
    if (strcmp (drive->id->model, "CD220E") == 0 ||
        strcmp (drive->id->model, "CD")     == 0)        /* Creative Labs */
      CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

    else if (strcmp (drive->id->model, "TO-ICSLYAL") == 0 ||  /* Acer CD525E */
             strcmp (drive->id->model, "OTI-SCYLLA") == 0)
      CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

    /* I don't know who makes this.
       Francesco Messineo <sidera@ccii.unipi.it> says this one's broken too. */
    else if (strcmp (drive->id->model, "DCI-2S10") == 0)
      CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

    else if (strcmp (drive->id->model, "CDA26803I SE") == 0) /* Aztech */
      {
        CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

        /* This drive _also_ does not implement PLAYAUDIO12 correctly. */
        CDROM_CONFIG_FLAGS (drive)->no_playaudio12 = 1;
      }

    /* Vertos 300.
       There seem to be at least two different, incompatible versions
       of this drive floating around.  Luckily, they appear to return their
       id strings with different byte orderings. */
    else if (strcmp (drive->id->model, "V003S0DS") == 0)
      {
        CDROM_CONFIG_FLAGS (drive)->vertos_lossage = 1;
        CDROM_CONFIG_FLAGS (drive)->playmsf_uses_bcd = 1;
        CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;
      }
    else if (strcmp (drive->id->model, "0V300SSD") == 0 ||
  	   strcmp (drive->id->model, "V003M0DP") == 0)
      CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

    /* Vertos 400. */
    else if (strcmp (drive->id->model, "V004E0DT") == 0 ||
  	   strcmp (drive->id->model, "0V400ETD") == 0)
      CDROM_CONFIG_FLAGS (drive)->no_lba_toc = 1;

    else if ( strcmp (drive->id->model, "CD-ROM CDU55D") == 0) /*sony cdu55d */
      CDROM_CONFIG_FLAGS (drive)->no_playaudio12 = 1;

   else if (strcmp (drive->id->model, "CD-ROM CDU55E") == 0)
  	CDROM_CONFIG_FLAGS (drive)->no_playaudio12 = 1;
  } /* drive-id != NULL */
#endif  /* not STANDARD_ATAPI */

  drive->cdrom_info.toc               = NULL;
  drive->cdrom_info.sector_buffer     = NULL;
  drive->cdrom_info.sector_buffered   = 0;
  drive->cdrom_info.nsectors_buffered = 0;
}



/*
 * TODO:
 *  CDROM_GET_UPC
 *  CDROMRESET
 *  Lock the door when a read request completes successfully and the
 *   door is not already locked.  Also try to reorganize to reduce
 *   duplicated functionality between read and ioctl paths?
 *  Establish interfaces for an IDE port driver, and break out the cdrom
 *   code into a loadable module.
 *  Support changers.
 *  Write some real documentation.
 */
