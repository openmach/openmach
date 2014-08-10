/************************************************************
 *							    *
 *		    Linux EATA SCSI driver		    *
 *							    *
 *  based on the CAM document CAM/89-004 rev. 2.0c,	    *
 *  DPT's driver kit, some internal documents and source,   *
 *  and several other Linux scsi drivers and kernel docs.   *
 *							    *
 *  The driver currently:				    *
 *	-supports all ISA based EATA-DMA boards		    *
 *	-supports all EISA based EATA-DMA boards	    *
 *	-supports all PCI based EATA-DMA boards		    *
 *	-supports multiple HBAs with & without IRQ sharing  *
 *	-supports all SCSI channels on multi channel boards *
 *	-needs identical IDs on all channels of a HBA	    * 
 *	-can be loaded as module			    *
 *	-displays statistical and hardware information	    *
 *	 in /proc/scsi/eata_dma				    *
 *      -provides rudimentary latency measurement           * 
 *       possibilities via /proc/scsi/eata_dma/<hostnum>    *
 *							    *
 *  (c)1993,94,95 Michael Neuffer			    *
 *		  neuffer@goofy.zdv.uni-mainz.de	    *
 *							    *
 *  This program is free software; you can redistribute it  *
 *  and/or modify it under the terms of the GNU General	    *
 *  Public License as published by the Free Software	    *
 *  Foundation; either version 2 of the License, or	    *
 *  (at your option) any later version.			    *
 *							    *
 *  This program is distributed in the hope that it will be *
 *  useful, but WITHOUT ANY WARRANTY; without even the	    *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A    *
 *  PARTICULAR PURPOSE.	 See the GNU General Public License *
 *  for more details.					    *
 *							    *
 *  You should have received a copy of the GNU General	    *
 *  Public License along with this kernel; if not, write to *
 *  the Free Software Foundation, Inc., 675 Mass Ave,	    *
 *  Cambridge, MA 02139, USA.				    *
 *							    *
 * I have to thank DPT for their excellent support. I took  *
 * me almost a year and a stopover at their HQ, on my first *
 * trip to the USA, to get it, but since then they've been  *
 * very helpful and tried to give me all the infos and	    *
 * support I need.					    *
 *							    *
 * Thanks also to Greg Hosler who did a lot of testing and  *
 * found quite a number of bugs during the development.	    *
 ************************************************************
 *  last change: 95/11/29                 OS: Linux 1.3.45  *
 ************************************************************/

/* Look in eata_dma.h for configuration and revision information */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/malloc.h>
#include <linux/in.h>
#include <linux/bios32.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <asm/byteorder.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/blk.h>
#include "scsi.h"
#include "sd.h"
#include "hosts.h"
#include <linux/scsicam.h>
#include "eata_dma.h"
#include "eata_dma_proc.h" 

#include <linux/stat.h>
#include <linux/config.h>	/* for CONFIG_PCI */

struct proc_dir_entry proc_scsi_eata_dma = {
    PROC_SCSI_EATA, 8, "eata_dma",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};

static u32 ISAbases[] =
{0x1F0, 0x170, 0x330, 0x230};
static unchar EISAbases[] =
{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static uint registered_HBAs = 0;
static struct Scsi_Host *last_HBA = NULL;
static struct Scsi_Host *first_HBA = NULL;
static unchar reg_IRQ[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static unchar reg_IRQL[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static struct eata_sp *status = 0;   /* Statuspacket array   */
static void *dma_scratch = 0;

static struct eata_register *fake_int_base;
static int fake_int_result;
static int fake_int_happened;

static ulong int_counter = 0;
static ulong queue_counter = 0;

void eata_scsi_done (Scsi_Cmnd * scmd)
{
    scmd->request.rq_status = RQ_SCSI_DONE;

    if (scmd->request.sem != NULL)
	up(scmd->request.sem);
    
    return;
}   

void eata_fake_int_handler(s32 irq, struct pt_regs * regs)
{
    fake_int_result = inb((ulong)fake_int_base + HA_RSTATUS);
    fake_int_happened = TRUE;
    DBG(DBG_INTR3, printk("eata_fake_int_handler called irq%d base %p"
			  " res %#x\n", irq, fake_int_base, fake_int_result));
    return;
}

#ifdef MACH
#include "eata_dma_proc.src"
#else
#include "eata_dma_proc.c"
#endif

#ifdef MODULE
int eata_release(struct Scsi_Host *sh)
{
    uint i;
    if (sh->irq && reg_IRQ[sh->irq] == 1) free_irq(sh->irq);
    else reg_IRQ[sh->irq]--;
    
    scsi_init_free((void *)status, 512);
    scsi_init_free((void *)dma_scratch, 512);
    for (i = 0; i < sh->can_queue; i++){ /* Free all SG arrays */
	if(SD(sh)->ccb[i].sg_list != NULL)
	    scsi_init_free((void *) SD(sh)->ccb[i].sg_list, 
			   sh->sg_tablesize * sizeof(struct eata_sg_list));
    }
    
    if (SD(sh)->channel == 0) {
	if (sh->dma_channel != BUSMASTER) free_dma(sh->dma_channel);
	if (sh->io_port && sh->n_io_port)
	    release_region(sh->io_port, sh->n_io_port);
    }
    return(TRUE);
}
#endif


void eata_int_handler(int irq, struct pt_regs * regs)
{
    uint i, result = 0;
    uint hba_stat, scsi_stat, eata_stat;
    Scsi_Cmnd *cmd;
    struct eata_ccb *cp;
    struct eata_sp *sp;
    uint base;
    ulong flags;
    uint x;
    struct Scsi_Host *sh;

    save_flags(flags);
    cli();

    for (x = 1, sh = first_HBA; x <= registered_HBAs; x++, sh = SD(sh)->next) {
	if (sh->irq != irq)
	    continue;
	
	while(inb((uint)sh->base + HA_RAUXSTAT) & HA_AIRQ) {
	    
	    int_counter++;
	    
	    sp = &SD(sh)->sp;
	    cp = sp->ccb;
	    
	    if(cp == NULL) {
		eata_stat = inb((uint)sh->base + HA_RSTATUS);
		printk("eata_dma: int_handler, Spurious IRQ %d "
		       "received. CCB pointer not set.\n", irq);
		break;
	    }

	    cmd = cp->cmd;
	    base = (uint) cmd->host->base;
       	    hba_stat = sp->hba_stat;
	    
	    scsi_stat = (sp->scsi_stat >> 1) & 0x1f; 
	    
	    if (sp->EOC == FALSE) {
		eata_stat = inb(base + HA_RSTATUS);
		printk("eata_dma: int_handler, board: %x cmd %lx returned "
		       "unfinished.\nEATA: %x HBA: %x SCSI: %x spadr %lx "
		       "spadrirq %lx, irq%d\n", base, (long)cp, eata_stat, 
		       hba_stat, scsi_stat,(long)&status, (long)&status[irq], 
		       irq);
		DBG(DBG_DELAY, DEL2(800));
		break;
	    } 
	    
	    if (cp->status == LOCKED) {
		cp->status = FREE;
		eata_stat = inb(base + HA_RSTATUS);
		printk("eata_dma: int_handler, freeing locked queueslot\n");
		DBG(DBG_INTR && DBG_DELAY, DEL2(800));
		break;
	    }
	    
	    eata_stat = inb(base + HA_RSTATUS); 
	    DBG(DBG_INTR, printk("IRQ %d received, base %#.4x, pid %ld, "
				 "target: %x, lun: %x, ea_s: %#.2x, hba_s: "
				 "%#.2x \n", irq, base, cmd->pid, cmd->target,
				 cmd->lun, eata_stat, hba_stat));
	    
	    switch (hba_stat) {
	    case HA_NO_ERROR:	/* NO Error */
		if (scsi_stat == CONDITION_GOOD
		    && cmd->device->type == TYPE_DISK
		    && (HD(cmd)->t_state[cp->cp_channel][cp->cp_id] == RESET))
		    result = DID_BUS_BUSY << 16;	    
		else if (scsi_stat == GOOD) {
		    HD(cmd)->t_state[cp->cp_channel][cp->cp_id] = OK;
		    if(HD(cmd)->do_latency == TRUE && cp->timestamp) {
			uint time;
			time = jiffies - cp->timestamp;
			if((cp->rw_latency) == TRUE) { /* was WRITE */
			    if(HD(cmd)->writes_lat[cp->sizeindex][1] > time)
				HD(cmd)->writes_lat[cp->sizeindex][1] = time;
			    if(HD(cmd)->writes_lat[cp->sizeindex][2] < time)
				HD(cmd)->writes_lat[cp->sizeindex][2] = time;
			    HD(cmd)->writes_lat[cp->sizeindex][3] += time;
			    HD(cmd)->writes_lat[cp->sizeindex][0]++;
			} else {
			    if(HD(cmd)->reads_lat[cp->sizeindex][1] > time)
				HD(cmd)->reads_lat[cp->sizeindex][1] = time;
			    if(HD(cmd)->reads_lat[cp->sizeindex][2] < time)
				HD(cmd)->reads_lat[cp->sizeindex][2] = time;
			    HD(cmd)->reads_lat[cp->sizeindex][3] += time;
			    HD(cmd)->reads_lat[cp->sizeindex][0]++;
			}
		    }
		}
		else if (scsi_stat == CHECK_CONDITION
			 && cmd->device->type == TYPE_DISK
			 && (cmd->sense_buffer[2] & 0xf) == RECOVERED_ERROR)
		    result = DID_BUS_BUSY << 16;
		else
		    result = DID_OK << 16;
		HD(cmd)->t_timeout[cp->cp_channel][cp->cp_id] = OK;
		break;
	    case HA_ERR_SEL_TO:	        /* Selection Timeout */
		result = DID_BAD_TARGET << 16;  
		break;
	    case HA_ERR_CMD_TO:	        /* Command Timeout   */
		if (HD(cmd)->t_timeout[cp->cp_channel][cp->cp_id] > 1)
		    result = DID_ERROR << 16;
		else {
		    result = DID_TIME_OUT << 16;
		    HD(cmd)->t_timeout[cp->cp_channel][cp->cp_id]++;
		}
		break;
	    case HA_ERR_RESET:		/* SCSI Bus Reset Received */
	    case HA_INIT_POWERUP:	/* Initial Controller Power-up */
		if (cmd->device->type != TYPE_TAPE)
		    result = DID_BUS_BUSY << 16;
		else
		    result = DID_ERROR << 16;
		
		for (i = 0; i < MAXTARGET; i++)
		    HD(cmd)->t_state[cp->cp_channel][i] = RESET;
		break;
	    case HA_UNX_BUSPHASE:	/* Unexpected Bus Phase */
	    case HA_UNX_BUS_FREE:	/* Unexpected Bus Free */
	    case HA_BUS_PARITY:	        /* Bus Parity Error */
	    case HA_SCSI_HUNG:	        /* SCSI Hung */
	    case HA_UNX_MSGRJCT:	/* Unexpected Message Reject */
	    case HA_RESET_STUCK:        /* SCSI Bus Reset Stuck */
	    case HA_RSENSE_FAIL:        /* Auto Request-Sense Failed */
	    case HA_PARITY_ERR:	        /* Controller Ram Parity */
	    default:
		result = DID_ERROR << 16;
		break;
	    }
	    cmd->result = result | (scsi_stat << 1); 
	    
#if DBG_INTR2
	    if (scsi_stat || result || hba_stat || eata_stat != 0x50 
		|| cmd->scsi_done == NULL || cmd->device->id == 7) 
		printk("HBA: %d, channel %d, id: %d, lun %d, pid %ld:\n" 
		       "eata_stat %#x, hba_stat %#.2x, scsi_stat %#.2x, "
		       "sense_key: %#x, result: %#.8x\n", x, 
		       cmd->device->channel, cmd->device->id, cmd->device->lun,
		       cmd->pid, eata_stat, hba_stat, scsi_stat, 
		       cmd->sense_buffer[2] & 0xf, cmd->result); 
	    DBG(DBG_INTR&&DBG_DELAY,DEL2(800));
#endif
	    
	    cp->status = FREE;	    /* now we can release the slot  */
	    cmd->scsi_done(cmd);
	}
    }
    restore_flags(flags);
    
    return;
}

inline int eata_send_command(u32 addr, u32 base, u8 command)
{
    long loop = R_LIMIT;
    
    while (inb(base + HA_RAUXSTAT) & HA_ABUSY)
	if (--loop == 0)
	    return(FALSE);

    /* And now the address in nice little byte chunks */
    outb( addr & 0x000000ff,	    base + HA_WDMAADDR);
    outb((addr & 0x0000ff00) >> 8,  base + HA_WDMAADDR + 1);
    outb((addr & 0x00ff0000) >> 16, base + HA_WDMAADDR + 2);
    outb((addr & 0xff000000) >> 24, base + HA_WDMAADDR + 3);
    outb(command, base + HA_WCOMMAND);
    return(TRUE);
}

#if 0 
inline int eata_send_immediate(u32 addr, u32 base, u8 cmnd, u8 cmnd2, u8 id, 
			       u8 lun)
{
    if(addr){
	outb( addr & 0x000000ff,	base + HA_WDMAADDR);
	outb((addr & 0x0000ff00) >> 8,	base + HA_WDMAADDR + 1);
	outb((addr & 0x00ff0000) >> 16, base + HA_WDMAADDR + 2);
	outb((addr & 0xff000000) >> 24, base + HA_WDMAADDR + 3);
    } else {
	outb(id,  base + HA_WSUBCODE);
	outb(lun, base + HA_WSUBLUN);
    }
    
    outb(cmnd2, base + HA_WCOMMAND2);
    outb(cmnd,	base + HA_WCOMMAND);
    return(TRUE);
}
#endif

int eata_queue(Scsi_Cmnd * cmd, void (* done) (Scsi_Cmnd *))
{
    unsigned int i, x, y;
    u32 flags;
    hostdata *hd;
    struct Scsi_Host *sh;
    struct eata_ccb *cp;
    struct scatterlist *sl;
    
    save_flags(flags);
    cli();
    
    queue_counter++;

    hd = HD(cmd);
    sh = cmd->host;
    
    /* check for free slot */
    for (y = hd->last_ccb + 1, x = 0; x < sh->can_queue; x++, y++) { 
	if (y >= sh->can_queue)
	    y = 0;
	if (hd->ccb[y].status == FREE)
	    break;
    }
    
    hd->last_ccb = y;

    if (x >= sh->can_queue) { 
	uint z;
	
	printk(KERN_EMERG "eata_dma: run out of queue slots cmdno:%ld"
	       " intrno: %ld, can_queue: %d, x: %d, y: %d\n", 
	       queue_counter, int_counter, sh->can_queue, x, y);
	printk(KERN_EMERG "Status of queueslots:");
	for(z = 0; z < sh->can_queue; z +=2) {
	    switch(hd->ccb[z].status) {
	    case FREE:
		printk(KERN_EMERG "Slot %2d is FREE  \t", z);
		break;
	    case USED:
		printk(KERN_EMERG "Slot %2d is USED  \t", z);
		break;
	    case LOCKED:
		printk(KERN_EMERG "Slot %2d is LOCKED\t", z);
		break;
	    default:
		printk(KERN_EMERG "Slot %2d is UNKNOWN\t", z);
	    }
	    panic("\nSystem halted.\n");
	}
    }
    cp = &hd->ccb[y];
    
    memset(cp, 0, sizeof(struct eata_ccb) - sizeof(struct eata_sg_list *));
    
    cp->status = USED;			/* claim free slot */
    
    DBG(DBG_QUEUE, printk("eata_queue pid %ld, target: %x, lun: %x, y %d\n",
			  cmd->pid, cmd->target, cmd->lun, y));
    DBG(DBG_QUEUE && DBG_DELAY, DEL2(250));
    
    if(hd->do_latency == TRUE) {
	int x, z;
	short *sho;
	long *lon;
	x = 0;	/* just to keep GCC quiet */ 
	if (cmd->cmnd[0] == WRITE_6 || cmd->cmnd[0] == WRITE_10 || 
	    cmd->cmnd[0] == WRITE_12 || cmd->cmnd[0] == READ_6 || 
	    cmd->cmnd[0] == READ_10 || cmd->cmnd[0] == READ_12) {
	    
	    cp->timestamp = jiffies;	/* For latency measurements */
	    switch(cmd->cmnd[0]) {
	    case WRITE_6:   
	    case READ_6:    
		x = cmd->cmnd[4]/2; 
		break;
	    case WRITE_10:   
	    case READ_10:
		sho = (short *) &cmd->cmnd[7];
		x = ntohs(*sho)/2;	      
		break;
	    case WRITE_12:   
	    case READ_12:
		lon = (long *) &cmd->cmnd[6];
		x = ntohl(*lon)/2;	      
		break;
	    }

	    for(z = 0; (x > (1 << z)) && (z <= 11); z++) 
		/* nothing */;
	    cp->sizeindex = z;
	    if (cmd->cmnd[0] ==	WRITE_6 || cmd->cmnd[0] == WRITE_10 || 
		cmd->cmnd[0] ==	WRITE_12){
		cp->rw_latency = TRUE;
	    }
	}
    }
    cmd->scsi_done = (void *)done;
    
    switch (cmd->cmnd[0]) {
    case CHANGE_DEFINITION: case COMPARE:	  case COPY:
    case COPY_VERIFY:	    case LOG_SELECT:	  case MODE_SELECT:
    case MODE_SELECT_10:    case SEND_DIAGNOSTIC: case WRITE_BUFFER:
    case FORMAT_UNIT:	    case REASSIGN_BLOCKS: case RESERVE:
    case SEARCH_EQUAL:	    case SEARCH_HIGH:	  case SEARCH_LOW:
    case WRITE_6:	    case WRITE_10:	  case WRITE_VERIFY:
    case UPDATE_BLOCK:	    case WRITE_LONG:	  case WRITE_SAME:	
    case SEARCH_HIGH_12:    case SEARCH_EQUAL_12: case SEARCH_LOW_12:
    case WRITE_12:	    case WRITE_VERIFY_12: case SET_WINDOW: 
    case MEDIUM_SCAN:	    case SEND_VOLUME_TAG:	     
    case 0xea:	    /* alternate number for WRITE LONG */
	cp->DataOut = TRUE;	/* Output mode */
	break;
    case TEST_UNIT_READY:
    default:
	cp->DataIn = TRUE;	/* Input mode  */
    }

    /* FIXME: This will will have to be changed once the midlevel driver 
     *        allows different HBA IDs on every channel.
     */
    if (cmd->target == sh->this_id) 
	cp->Interpret = TRUE;	/* Interpret command */

    if (cmd->use_sg) {
	cp->scatter = TRUE;	/* SG mode     */
	if (cp->sg_list == NULL) {
	    cp->sg_list = kmalloc(sh->sg_tablesize * sizeof(struct eata_sg_list),
				  GFP_ATOMIC | GFP_DMA);
	}
	if (cp->sg_list == NULL)
	    panic("eata_dma: Run out of DMA memory for SG lists !\n");
	cp->cp_dataDMA = htonl(virt_to_bus(cp->sg_list)); 
	
	cp->cp_datalen = htonl(cmd->use_sg * sizeof(struct eata_sg_list));
	sl=(struct scatterlist *)cmd->request_buffer;
	for(i = 0; i < cmd->use_sg; i++, sl++){
	    cp->sg_list[i].data = htonl(virt_to_bus(sl->address));
	    cp->sg_list[i].len = htonl((u32) sl->length);
	}
    } else {
	cp->scatter = FALSE;
	cp->cp_datalen = htonl(cmd->request_bufflen);
	cp->cp_dataDMA = htonl(virt_to_bus(cmd->request_buffer));
    }
    
    cp->Auto_Req_Sen = TRUE;
    cp->cp_reqDMA = htonl(virt_to_bus(cmd->sense_buffer));
    cp->reqlen = sizeof(cmd->sense_buffer);
    
    cp->cp_id = cmd->target;
    cp->cp_channel = cmd->channel;
    cp->cp_lun = cmd->lun;
    cp->cp_dispri = TRUE;
    cp->cp_identify = TRUE;
    memcpy(cp->cp_cdb, cmd->cmnd, cmd->cmd_len);
    
    cp->cp_statDMA = htonl(virt_to_bus(&(hd->sp)));
    
    cp->cp_viraddr = cp; /* This will be passed thru, so we don't need to 
                          * convert it */
    cp->cmd = cmd;
    cmd->host_scribble = (char *)&hd->ccb[y];	
    
    if(eata_send_command((u32) cp, (u32) sh->base, EATA_CMD_DMA_SEND_CP) == FALSE) {
	cmd->result = DID_BUS_BUSY << 16;
	DBG(DBG_QUEUE && DBG_ABNORM, 
	    printk("eata_queue target %d, pid %ld, HBA busy, "
		   "returning DID_BUS_BUSY\n",cmd->target, cmd->pid));
	done(cmd);
	cp->status = FREE;    
	restore_flags(flags);
	return(0);
    }
    DBG(DBG_QUEUE, printk("Queued base %#.4x pid: %ld target: %x lun: %x "
			 "slot %d irq %d\n", (s32)sh->base, cmd->pid, 
			 cmd->target, cmd->lun, y, sh->irq));
    DBG(DBG_QUEUE && DBG_DELAY, DEL2(200));
    restore_flags(flags);
    return(0);
}


int eata_abort(Scsi_Cmnd * cmd)
{
    ulong loop = R_LIMIT;
    ulong flags;

    save_flags(flags);
    cli();

    DBG(DBG_ABNORM, printk("eata_abort called pid: %ld target: %x lun: %x"
			   " reason %x\n", cmd->pid, cmd->target, cmd->lun, 
			   cmd->abort_reason));
    DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));

    while (inb((u32)(cmd->host->base) + HA_RAUXSTAT) & HA_ABUSY) {
	if (--loop == 0) {
	    printk("eata_dma: abort, timeout error.\n");
	    restore_flags(flags);
	    DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	    return (SCSI_ABORT_ERROR);
	}
    }
    if (CD(cmd)->status == RESET) {
	restore_flags(flags);
	printk("eata_dma: abort, command reset error.\n");
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	return (SCSI_ABORT_ERROR);
    }
    if (CD(cmd)->status == LOCKED) {
	restore_flags(flags);
	DBG(DBG_ABNORM, printk("eata_dma: abort, queue slot locked.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	return (SCSI_ABORT_NOT_RUNNING);
    }
    if (CD(cmd)->status == USED) {
	DBG(DBG_ABNORM, printk("Returning: SCSI_ABORT_BUSY\n"));
	restore_flags(flags);
	return (SCSI_ABORT_BUSY);  /* SNOOZE */ 
    }
    if (CD(cmd)->status == FREE) {
	DBG(DBG_ABNORM, printk("Returning: SCSI_ABORT_NOT_RUNNING\n")); 
	restore_flags(flags);
	return (SCSI_ABORT_NOT_RUNNING);
    }
    restore_flags(flags);
    panic("eata_dma: abort: invalid slot status\n");
}

int eata_reset(Scsi_Cmnd * cmd)
{
    ushort x, z; 
    ulong time, limit = 0;
    ulong loop = R_LIMIT;
    ulong flags;
    unchar success = FALSE;
    Scsi_Cmnd *sp; 
    
    save_flags(flags);
    cli();
    
    DBG(DBG_ABNORM, printk("eata_reset called pid:%ld target: %x lun: %x"
			   " reason %x\n", cmd->pid, cmd->target, cmd->lun, 
			   cmd->abort_reason));
	
    if (HD(cmd)->state == RESET) {
	printk("eata_reset: exit, already in reset.\n");
	restore_flags(flags);
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	return (SCSI_RESET_ERROR);
    }
    
    while (inb((u32)(cmd->host->base) + HA_RAUXSTAT) & HA_ABUSY)
	if (--loop == 0) {
	    printk("eata_reset: exit, timeout error.\n");
	    restore_flags(flags);
	    DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	    return (SCSI_RESET_ERROR);
	}
 
    for (x = 0; x < MAXCHANNEL; x++) {
	for (z = 0; z < MAXTARGET; z++) {
	    HD(cmd)->t_state[x][z] = RESET;
	    HD(cmd)->t_timeout[x][z] = NO_TIMEOUT;
	}
    }

    for (x = 0; x < cmd->host->can_queue; x++) {
	if (HD(cmd)->ccb[x].status == FREE)
	    continue;
	
	if (HD(cmd)->ccb[x].status == LOCKED) {
	    HD(cmd)->ccb[x].status = FREE;
	    printk("eata_reset: locked slot %d forced free.\n", x);
	    DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	    continue;
	}
	sp = HD(cmd)->ccb[x].cmd;
	HD(cmd)->ccb[x].status = RESET;
	printk("eata_reset: slot %d in reset, pid %ld.\n", x, sp->pid);
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	
	if (sp == NULL)
	    panic("eata_reset: slot %d, sp==NULL.\n", x);
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	
	if (sp == cmd)
	    success = TRUE;
    }
    
    /* hard reset the HBA  */
    inb((u32) (cmd->host->base) + HA_RSTATUS);	/* This might cause trouble */
    eata_send_command(0, (u32) cmd->host->base, EATA_CMD_RESET);
    
    DBG(DBG_ABNORM, printk("eata_reset: board reset done, enabling interrupts.\n"));
    HD(cmd)->state = RESET;
    
    restore_flags(flags);
    
    time = jiffies;
    while (jiffies < (time + (3 * HZ)) || limit++ < 10000000)
	/* As time goes by... */;
    
    save_flags(flags);
    cli();
    
    DBG(DBG_ABNORM, printk("eata_reset: interrupts disabled, loops %ld.\n", 
			   limit));
    DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
    
    for (x = 0; x < cmd->host->can_queue; x++) {
	
	/* Skip slots already set free by interrupt */
	if (HD(cmd)->ccb[x].status != RESET)
	    continue;
	
	sp = HD(cmd)->ccb[x].cmd;
	sp->result = DID_RESET << 16;
	
	/* This mailbox is still waiting for its interrupt */
	HD(cmd)->ccb[x].status = LOCKED;
	
	printk("eata_reset: slot %d locked, DID_RESET, pid %ld done.\n",
	       x, sp->pid);
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	restore_flags(flags);
	sp->scsi_done(sp);
	cli();
    }
    
    HD(cmd)->state = FALSE;
    restore_flags(flags);
    
    if (success) {
	DBG(DBG_ABNORM, printk("eata_reset: exit, success.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	return (SCSI_RESET_SUCCESS);
    } else {
	DBG(DBG_ABNORM, printk("eata_reset: exit, wakeup.\n"));
	DBG(DBG_ABNORM && DBG_DELAY, DEL2(500));
	return (SCSI_RESET_PUNT);
    }
}

char * get_board_data(u32 base, u32 irq, u32 id)
{
    struct eata_ccb *cp;
    struct eata_sp  *sp;
    static char *buff;
    ulong i;
    ulong limit = 0;

    cp = (struct eata_ccb *) scsi_init_malloc(sizeof(struct eata_ccb),
					      GFP_ATOMIC | GFP_DMA);
    sp = (struct eata_sp *) scsi_init_malloc(sizeof(struct eata_sp), 
					     GFP_ATOMIC | GFP_DMA);

    buff = dma_scratch;
 
    memset(cp, 0, sizeof(struct eata_ccb));
    memset(sp, 0, sizeof(struct eata_sp));
    memset(buff, 0, 256);

    cp->DataIn = TRUE;	   
    cp->Interpret = TRUE;   /* Interpret command */
    cp->cp_dispri = TRUE;
    cp->cp_identify = TRUE;
 
    cp->cp_datalen = htonl(56);  
    cp->cp_dataDMA = htonl(virt_to_bus(buff));
    cp->cp_statDMA = htonl(virt_to_bus(sp));
    cp->cp_viraddr = cp;
    
    cp->cp_id = id;
    cp->cp_lun = 0;

    cp->cp_cdb[0] = INQUIRY;
    cp->cp_cdb[1] = 0;
    cp->cp_cdb[2] = 0;
    cp->cp_cdb[3] = 0;
    cp->cp_cdb[4] = 56;
    cp->cp_cdb[5] = 0;

    fake_int_base = (struct eata_register *) base;
    fake_int_result = FALSE;
    fake_int_happened = FALSE;

    eata_send_command((u32) cp, (u32) base, EATA_CMD_DMA_SEND_CP);
    
    i = jiffies + (3 * HZ);
    while (fake_int_happened == FALSE && jiffies <= i) 
	barrier();
    
    DBG(DBG_INTR3, printk("fake_int_result: %#x hbastat %#x scsistat %#x,"
			  " buff %p sp %p\n",
			  fake_int_result, (u32) (sp->hba_stat /*& 0x7f*/), 
			  (u32) sp->scsi_stat, buff, sp));

    scsi_init_free((void *)cp, sizeof(struct eata_ccb));
    scsi_init_free((void *)sp, sizeof(struct eata_sp));
    
    if ((fake_int_result & HA_SERROR) || jiffies > i){
	/* hard reset the HBA  */
	inb((u32) (base) + HA_RSTATUS);
	eata_send_command(0, base, EATA_CMD_RESET);
	i = jiffies;
	while (jiffies < (i + (3 * HZ)) && limit++ < 10000000) 
	    barrier();
	return (NULL);
    } else
	return (buff);
}
    
int check_blink_state(long base)
{
    ushort loops = 10;
    u32 blinkindicator;
    u32 state = 0x12345678;
    u32 oldstate = 0;

    blinkindicator = htonl(0x54504442);
    while ((loops--) && (state != oldstate)) {
	oldstate = state;
	state = inl((uint) base + 1);
    }

    DBG(DBG_BLINK, printk("Did Blink check. Status: %d\n",
	      (state == oldstate) && (state == blinkindicator)));

    if ((state == oldstate) && (state == blinkindicator))
	return(TRUE);
    else
	return (FALSE);
}

int get_conf_PIO(u32 base, struct get_conf *buf)
{
    ulong loop = R_LIMIT;
    u16 *p;

    if(check_region(base, 9)) 
	return (FALSE);
     
    memset(buf, 0, sizeof(struct get_conf));

    while (inb(base + HA_RSTATUS) & HA_SBUSY)
	if (--loop == 0) 
	    return (FALSE);
       
    DBG(DBG_PIO && DBG_PROBE,
	printk("Issuing PIO READ CONFIG to HBA at %#x\n", base));
    eata_send_command(0, base, EATA_CMD_PIO_READ_CONFIG);

    loop = R_LIMIT;
    for (p = (u16 *) buf; 
	 (long)p <= ((long)buf + (sizeof(struct get_conf) / 2)); p++) {
	while (!(inb(base + HA_RSTATUS) & HA_SDRQ))
	    if (--loop == 0)
		return (FALSE);

	loop = R_LIMIT;
	*p = inw(base + HA_RDATA);
    }

    if (!(inb(base + HA_RSTATUS) & HA_SERROR)) {	    /* Error ? */
	if (htonl(EATA_SIGNATURE) == buf->signature) {
	    DBG(DBG_PIO&&DBG_PROBE, printk("EATA Controller found at %x "
					   "EATA Level: %x\n", (uint) base, 
					   (uint) (buf->version)));
	    
	    while (inb(base + HA_RSTATUS) & HA_SDRQ) 
		inw(base + HA_RDATA);
	    return (TRUE);
	} 
    } else {
	DBG(DBG_PROBE, printk("eata_dma: get_conf_PIO, error during transfer "
		  "for HBA at %lx\n", (long)base));
    }
    return (FALSE);
}

void print_config(struct get_conf *gc)
{
    printk("LEN: %d ver:%d OCS:%d TAR:%d TRNXFR:%d MORES:%d DMAS:%d\n",
	   (u32) ntohl(gc->len), gc->version,
	   gc->OCS_enabled, gc->TAR_support, gc->TRNXFR, gc->MORE_support,
	   gc->DMA_support);
    printk("DMAV:%d HAAV:%d SCSIID0:%d ID1:%d ID2:%d QUEUE:%d SG:%d SEC:%d\n",
	   gc->DMA_valid, gc->HAA_valid, gc->scsi_id[3], gc->scsi_id[2],
	   gc->scsi_id[1], ntohs(gc->queuesiz), ntohs(gc->SGsiz), gc->SECOND);
    printk("IRQ:%d IRQT:%d DMAC:%d FORCADR:%d SG_64K:%d SG_UAE:%d MID:%d "
	   "MCH:%d MLUN:%d\n",
	   gc->IRQ, gc->IRQ_TR, (8 - gc->DMA_channel) & 7, gc->FORCADR, 
	   gc->SG_64K, gc->SG_UAE, gc->MAX_ID, gc->MAX_CHAN, gc->MAX_LUN); 
    printk("RIDQ:%d PCI:%d EISA:%d\n",
	   gc->ID_qest, gc->is_PCI, gc->is_EISA);
    DBG(DPT_DEBUG, DELAY(14));
}

short register_HBA(u32 base, struct get_conf *gc, Scsi_Host_Template * tpnt, 
		   u8 bustype)
{
    ulong size = 0;
    unchar dma_channel = 0;
    char *buff = 0;
    unchar bugs = 0;
    struct Scsi_Host *sh;
    hostdata *hd;
    int x;
    
    
    DBG(DBG_REGISTER, print_config(gc));

    if (gc->DMA_support == FALSE) {
	printk("The EATA HBA at %#.4x does not support DMA.\n" 
	       "Please use the EATA-PIO driver.\n", base);
	return (FALSE);
    }
    if(gc->HAA_valid == FALSE || ntohl(gc->len) < 0x22) 
	gc->MAX_CHAN = 0;
    
    if (reg_IRQ[gc->IRQ] == FALSE) {	/* Interrupt already registered ? */
	if (!request_irq(gc->IRQ, (void *) eata_fake_int_handler, SA_INTERRUPT,
			 "eata_dma")){
	    reg_IRQ[gc->IRQ]++;
	    if (!gc->IRQ_TR)
		reg_IRQL[gc->IRQ] = TRUE;   /* IRQ is edge triggered */
	} else {
	    printk("Couldn't allocate IRQ %d, Sorry.", gc->IRQ);
	    return (FALSE);
	}
    } else {		/* More than one HBA on this IRQ */
	if (reg_IRQL[gc->IRQ] == TRUE) {
	    printk("Can't support more than one HBA on this IRQ,\n"
		   "  if the IRQ is edge triggered. Sorry.\n");
	    return (FALSE);
	} else
	    reg_IRQ[gc->IRQ]++;
    }
    
    /* if gc->DMA_valid it must be an ISA HBA and we have to register it */
    dma_channel = BUSMASTER;
    if (gc->DMA_valid) {
	if (request_dma(dma_channel = (8 - gc->DMA_channel) & 7, "eata_dma")) {
	    printk("Unable to allocate DMA channel %d for ISA HBA at %#.4x.\n",
		   dma_channel, base);
	    reg_IRQ[gc->IRQ]--;
	    if (reg_IRQ[gc->IRQ] == 0)
		free_irq(gc->IRQ);
	    if (gc->IRQ_TR == FALSE)
		reg_IRQL[gc->IRQ] = FALSE; 
	    return (FALSE);
	}
    }
 
#if !(NEWSTUFF)
    if (bustype != IS_EISA && bustype != IS_ISA)
#endif
	buff = get_board_data(base, gc->IRQ, gc->scsi_id[3]);

    if (buff == NULL) {
#if !(NEWSTUFF)
	if (bustype == IS_EISA || bustype == IS_ISA) {
	    bugs = bugs || BROKEN_INQUIRY;
	} else {
#endif
	    if (gc->DMA_support == FALSE)
		printk("HBA at %#.4x doesn't support DMA. Sorry\n", base);
	    else
		printk("HBA at %#.4x does not react on INQUIRY. Sorry.\n", 
		       base);
	    if (gc->DMA_valid) 
		free_dma(dma_channel);
	    reg_IRQ[gc->IRQ]--;
	    if (reg_IRQ[gc->IRQ] == 0)
		free_irq(gc->IRQ);
	    if (gc->IRQ_TR == FALSE)
		reg_IRQL[gc->IRQ] = FALSE; 
	    return (FALSE);
#if !(NEWSTUFF)
	}
#endif
    }
    
    if (gc->DMA_support == FALSE && buff != NULL)  
	printk("HBA %.12sat %#.4x doesn't set the DMA_support flag correctly.\n",
	       &buff[16], base);
    
    request_region(base, 9, "eata_dma"); /* We already checked the 
					  * availability, so this
					  * should not fail.
					  */
    
    if(ntohs(gc->queuesiz) == 0) {
	gc->queuesiz = ntohs(64);
	printk("Warning: Queue size has to be corrected. Assuming 64 queueslots\n"
	       "         This might be a PM2012B with a defective Firmware\n");
    }

    size = sizeof(hostdata) + ((sizeof(struct eata_ccb) + sizeof(long)) 
			       * ntohs(gc->queuesiz));

    DBG(DBG_REGISTER, printk("scsi_register size: %ld\n", size));

    sh = scsi_register(tpnt, size);
    
    if(sh == NULL) {
	if (gc->DMA_valid) 
	    free_dma(dma_channel);
	
	reg_IRQ[gc->IRQ]--;
	if (reg_IRQ[gc->IRQ] == 0)
	    free_irq(gc->IRQ);
	if (gc->IRQ_TR == FALSE)
	    reg_IRQL[gc->IRQ] = FALSE; 
	return (FALSE);
    }
    
    hd = SD(sh);		   
    
    memset(hd->ccb, 0, sizeof(struct eata_ccb) * ntohs(gc->queuesiz));
    memset(hd->reads, 0, sizeof(u32) * 26); 

    hd->broken_INQUIRY = (bugs & BROKEN_INQUIRY);

    if(hd->broken_INQUIRY == TRUE) {
	strcpy(SD(sh)->vendor, "DPT");
	strcpy(SD(sh)->name, "??????????");
	strcpy(SD(sh)->revision, "???.?");
    } else {	
	strncpy(SD(sh)->vendor, &buff[8], 8);
	SD(sh)->vendor[8] = 0;
	strncpy(SD(sh)->name, &buff[16], 17);
	SD(sh)->name[17] = 0;
	SD(sh)->revision[0] = buff[32];
	SD(sh)->revision[1] = buff[33];
	SD(sh)->revision[2] = buff[34];
	SD(sh)->revision[3] = '.';
	SD(sh)->revision[4] = buff[35];
	SD(sh)->revision[5] = 0;
    }

    switch (ntohl(gc->len)) {
    case 0x1c:
	SD(sh)->EATA_revision = 'a';
	break;
    case 0x1e:
	SD(sh)->EATA_revision = 'b';
	break;
    case 0x22:
	SD(sh)->EATA_revision = 'c';
	break;
    case 0x24:
	SD(sh)->EATA_revision = 'z';		
    default:
	SD(sh)->EATA_revision = '?';
    }

    if(ntohl(gc->len) >= 0x22) {
	if (gc->is_PCI == TRUE)
	    hd->bustype = IS_PCI;
	else if (gc->is_EISA == TRUE)
	    hd->bustype = IS_EISA;
	else
	    hd->bustype = IS_ISA;
    } else if(hd->broken_INQUIRY == FALSE) {
	if (buff[21] == '4')
	    hd->bustype = IS_PCI;
	else if (buff[21] == '2')
	    hd->bustype = IS_EISA;
	else
	    hd->bustype = IS_ISA;
    } else 
	hd->bustype = bustype;
    
    if(ntohl(gc->len) >= 0x22) {
	sh->max_id = gc->MAX_ID + 1;
	sh->max_lun = gc->MAX_LUN + 1;
    } else {
	sh->max_id = 8;
	sh->max_lun = 8;
    }

    hd->channel = gc->MAX_CHAN;	    
    sh->max_channel = gc->MAX_CHAN; 
    sh->unique_id = base;
    sh->base = (char *) base;
    sh->io_port = base;
    sh->n_io_port = 9;
    sh->irq = gc->IRQ;
    sh->dma_channel = dma_channel;
    
    /* FIXME:
     * SCSI midlevel code should support different HBA ids on every channel
     */
    sh->this_id = gc->scsi_id[3];
    sh->can_queue = ntohs(gc->queuesiz);
    
    if (gc->OCS_enabled == TRUE) {
	if(hd->bustype != IS_ISA)
	    sh->cmd_per_lun = sh->can_queue/C_P_L_DIV; 
	else
	    sh->cmd_per_lun = 8; /* We artificially limit this to conserve 
				  * memory, which would be needed for ISA 
				  * bounce buffers */
    } else 
	sh->cmd_per_lun = 1;
    
    /* FIXME:
     * SG should be allocated more dynamically 
     */
    /*
     * If we are using a ISA board, we can't use extended SG,
     * because we would need exessive amounts of memory for
     * bounce buffers.
     */
    if (gc->SG_64K == TRUE && ntohs(gc->SGsiz) == 64 && hd->bustype != IS_ISA){
	sh->sg_tablesize = SG_SIZE_BIG;
	sh->use_clustering = FALSE;
    } else {
	sh->sg_tablesize = ntohs(gc->SGsiz);
	sh->use_clustering = TRUE;
	if (sh->sg_tablesize > SG_SIZE || sh->sg_tablesize == 0) {
	    sh->sg_tablesize = SG_SIZE;
	    if (ntohs(gc->SGsiz) == 0)
		printk("Warning: SG size had to be corrected.\n"
		       "This might be a PM2012 with a defective Firmware\n");
	}
    }

    if (gc->SECOND)
	hd->primary = FALSE;
    else
	hd->primary = TRUE;
    
    sh->wish_block = FALSE;	   
    
    if (hd->bustype != IS_ISA) {
	sh->unchecked_isa_dma = FALSE;
    } else {
	sh->unchecked_isa_dma = TRUE;	/* We're doing ISA DMA */
    }
    
    for(x = 0; x <= 11; x++){		 /* Initialize min. latency */
	hd->writes_lat[x][1] = 0xffffffff;
	hd->reads_lat[x][1] = 0xffffffff;
    }

    hd->next = NULL;	/* build a linked list of all HBAs */
    hd->prev = last_HBA;
    if(hd->prev != NULL)
	SD(hd->prev)->next = sh;
    last_HBA = sh;
    if (first_HBA == NULL)
	first_HBA = sh;
    registered_HBAs++;
    
    return (TRUE);
}


void find_EISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    u32 base;
    int i;
    
#if CHECKPAL
    u8 pal1, pal2, pal3;
#endif
    
    for (i = 0; i < MAXEISA; i++) {
	if (EISAbases[i] == TRUE) { /* Still a possibility ?	      */
	    
	    base = 0x1c88 + (i * 0x1000);
#if CHECKPAL
	    pal1 = inb((u16)base - 8);
	    pal2 = inb((u16)base - 7);
	    pal3 = inb((u16)base - 6);
	    
	    if (((pal1 == DPT_ID1) && (pal2 == DPT_ID2)) ||
		((pal1 == NEC_ID1) && (pal2 == NEC_ID2) && (pal3 == NEC_ID3))||
		((pal1 == ATT_ID1) && (pal2 == ATT_ID2) && (pal3 == ATT_ID3))){
		DBG(DBG_PROBE, printk("EISA EATA id tags found: %x %x %x \n",
				      (int)pal1, (int)pal2, (int)pal3));
#endif
		if (get_conf_PIO(base, buf) == TRUE) {
		    if (buf->IRQ) {  
			DBG(DBG_EISA, printk("Registering EISA HBA\n"));
			register_HBA(base, buf, tpnt, IS_EISA);
		    } else
			printk("eata_dma: No valid IRQ. HBA removed from list\n");
		} else {
		    if (check_blink_state(base)) 
			printk("HBA is in BLINK state. Consult your HBAs "
			       "Manual to correct this.\n");
		} 
		/* Nothing found here so we take it from the list */
		EISAbases[i] = 0;  
#if CHECKPAL
	    } 
#endif
	}
    }
    return; 
}

void find_ISA(struct get_conf *buf, Scsi_Host_Template * tpnt)
{
    int i;
    
    for (i = 0; i < MAXISA; i++) {  
	if (ISAbases[i]) {  
	    if (get_conf_PIO(ISAbases[i],buf) == TRUE){
		DBG(DBG_ISA, printk("Registering ISA HBA\n"));
		register_HBA(ISAbases[i], buf, tpnt, IS_ISA);
	    } else {
		if (check_blink_state(ISAbases[i])) 
		    printk("HBA is in BLINK state. Consult your HBAs "
			   "Manual to correct this.\n");
	    }
	    ISAbases[i] = 0;
	}
    }
    return;
}

void find_PCI(struct get_conf *buf, Scsi_Host_Template * tpnt)
{

#ifndef CONFIG_PCI
    printk("eata_dma: kernel PCI support not enabled. Skipping scan for PCI HBAs.\n");
#else
    
    u8 pci_bus, pci_device_fn;
    static s16 pci_index = 0;	/* Device index to PCI BIOS calls */
    u32 base = 0;
    u16 com_adr;
    u16 rev_device;
    u32 error, i, x;
    u8 pal1, pal2, pal3;

    if (pcibios_present()) {
	for (i = 0; i <= MAXPCI; ++i, ++pci_index) {
	    if (pcibios_find_device(PCI_VENDOR_ID_DPT, PCI_DEVICE_ID_DPT, 
				    pci_index, &pci_bus, &pci_device_fn))
		break;
	    DBG(DBG_PROBE && DBG_PCI, 
		printk("eata_dma: find_PCI, HBA at bus %d, device %d,"
		       " function %d, index %d\n", (s32)pci_bus, 
		       (s32)((pci_device_fn & 0xf8) >> 3),
		       (s32)(pci_device_fn & 7), pci_index));
	    
	    if (!(error = pcibios_read_config_word(pci_bus, pci_device_fn, 
				       PCI_CLASS_DEVICE, &rev_device))) {
		if (rev_device == PCI_CLASS_STORAGE_SCSI) {
		    if (!(error = pcibios_read_config_word(pci_bus, 
					       pci_device_fn, PCI_COMMAND, 
					       (u16 *) & com_adr))) {
			if (!((com_adr & PCI_COMMAND_IO) && 
			      (com_adr & PCI_COMMAND_MASTER))) {
			    printk("eata_dma: find_PCI, HBA has IO or BUSMASTER mode disabled\n");
			    continue;
			}
		    } else
			printk("eata_dma: find_PCI, error %x while reading "
			       "PCI_COMMAND\n", error);
		} else
		    printk("eata_dma: find_PCI, DEVICECLASSID %x didn't match\n", 
			   rev_device);
	    } else {
		printk("eata_dma: find_PCI, error %x while reading PCI_CLASS_BASE\n", 
		       error);
		continue;
	    }
	    
	    if (!(error = pcibios_read_config_dword(pci_bus, pci_device_fn,
				       PCI_BASE_ADDRESS_0, (int *) &base))){
		
		/* Check if the address is valid */
		if (base & 0x01) {
		    base &= 0xfffffffe;
                    /* EISA tag there ? */
		    pal1 = inb(base);
		    pal2 = inb(base + 1);
		    pal3 = inb(base + 2);
		    if (((pal1 == DPT_ID1) && (pal2 == DPT_ID2)) ||
			((pal1 == NEC_ID1) && (pal2 == NEC_ID2) && 
			 (pal3 == NEC_ID3)) ||
			((pal1 == ATT_ID1) && (pal2 == ATT_ID2) && 
			 (pal3 == ATT_ID3)))
			base += 0x08;
		    else
			base += 0x10;   /* Now, THIS is the real address */

		    if (base != 0x1f8) {
			/* We didn't find it in the primary search */
			if (get_conf_PIO(base, buf) == TRUE) {

			    /* OK. We made it till here, so we can go now  
			     * and register it. We  only have to check and 
			     * eventually remove it from the EISA and ISA list 
			     */
			    DBG(DBG_PCI, printk("Registering PCI HBA\n"));
			    register_HBA(base, buf, tpnt, IS_PCI);
			    
			    if (base < 0x1000) {
				for (x = 0; x < MAXISA; ++x) {
				    if (ISAbases[x] == base) {
					ISAbases[x] = 0;
					break;
				    }
				}
			    } else if ((base & 0x0fff) == 0x0c88) 
				EISAbases[(base >> 12) & 0x0f] = 0;
			    continue;  /* break; */
			} else if (check_blink_state(base) == TRUE) {
			    printk("eata_dma: HBA is in BLINK state.\n"
				   "Consult your HBAs Manual to correct this.\n");
			}
		    }
		}
	    } else {
		printk("eata_dma: error %x while reading "
		       "PCI_BASE_ADDRESS_0\n", error);
	    }
	}
    } else {
	printk("eata_dma: No BIOS32 extensions present. This driver release "
	       "still depends on it.\n"
	       "	  Skipping scan for PCI HBAs. \n");
    }
#endif /* #ifndef CONFIG_PCI */
    return;
}

int eata_detect(Scsi_Host_Template * tpnt)
{
    struct Scsi_Host *HBA_ptr;
    struct get_conf gc;
    int i;
    
    DBG((DBG_PROBE && DBG_DELAY) || DPT_DEBUG,
	printk("Using lots of delays to let you read the debugging output\n"));

    tpnt->proc_dir = &proc_scsi_eata_dma;

    status = scsi_init_malloc(512, GFP_ATOMIC | GFP_DMA);
    dma_scratch = scsi_init_malloc(512, GFP_ATOMIC | GFP_DMA);

    if(status == NULL || dma_scratch == NULL) {
	printk("eata_dma: can't allocate enough memory to probe for hosts !\n");
	return(0);
    }

    find_PCI(&gc, tpnt);
    
    find_EISA(&gc, tpnt);
    
    find_ISA(&gc, tpnt);
    
    for (i = 0; i <= MAXIRQ; i++) { /* Now that we know what we have, we     */
	if (reg_IRQ[i]){            /* exchange the interrupt handler which  */
	    free_irq(i);            /* we used for probing with the real one */
	    request_irq(i, (void *)(eata_int_handler), SA_INTERRUPT, "eata_dma");
	}
    }
    HBA_ptr = first_HBA;
    
    if (registered_HBAs != 0) {
	printk("EATA (Extended Attachment) driver version: %d.%d%s\n"
	       "developed in co-operation with DPT\n"		  
	       "(c) 1993-95 Michael Neuffer, neuffer@goofy.zdv.uni-mainz.de\n",
	       VER_MAJOR, VER_MINOR, VER_SUB);
	printk("Registered HBAs:");
	printk("\nHBA no. Boardtype: Revis: EATA: Bus: BaseIO: IRQ: DMA: Ch: "
	       "ID: Pr: QS: SG: CPL:\n");
	for (i = 1; i <= registered_HBAs; i++) {
	    printk("scsi%-2d: %.10s v%s 2.0%c  %s %#.4x   %2d",
		   HBA_ptr->host_no, SD(HBA_ptr)->name, SD(HBA_ptr)->revision,
		   SD(HBA_ptr)->EATA_revision, (SD(HBA_ptr)->bustype == 'P')? 
		   "PCI ":(SD(HBA_ptr)->bustype == 'E')?"EISA":"ISA ",
		   (u32) HBA_ptr->base, HBA_ptr->irq);
	    if(HBA_ptr->dma_channel != BUSMASTER)
		printk("   %2x ", HBA_ptr->dma_channel);
	    else
		printk("  %s", "BMST");
	    printk("  %d   %d   %c  %2d  %2d   %2d\n", SD(HBA_ptr)->channel, 
		   HBA_ptr->this_id, (SD(HBA_ptr)->primary == TRUE)?'Y':'N', 
		   HBA_ptr->can_queue, HBA_ptr->sg_tablesize, HBA_ptr->cmd_per_lun);
	    HBA_ptr = SD(HBA_ptr)->next;
	}
    } else {
	scsi_init_free((void *)status, 512);
    }

    scsi_init_free((void *)dma_scratch, 512);

    DBG(DPT_DEBUG, DELAY(12));

    return(registered_HBAs);
}

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = EATA_DMA;
#include "scsi_module.c"
#endif

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * tab-width: 8
 * End:
 */
