
void swap_statistics(u8 *p)
{
    u32 y;
    u32 *lp, h_lp;
    u16 *sp, h_sp;
    u8 *bp;
    
    lp = (u32 *)p;
    sp = ((short *)lp) + 1;	    /* Convert Header */
    h_sp = *sp = ntohs(*sp);
    lp++;

    do {
	sp = (u16 *)lp;		  /* Convert SubHeader */
	*sp = ntohs(*sp);
	bp = (u8 *) lp;
	y = *(bp + 3);
	lp++;
	for (h_lp = (u32)lp; (u32)lp < h_lp + ((u32)*(bp + 3)); lp++)
	    *lp = ntohl(*lp);
    }while ((u32)lp < ((u32)p) + 4 + h_sp);

}

/*
 * eata_set_info
 * buffer : pointer to the data that has been written to the hostfile
 * length : number of bytes written to the hostfile
 * HBA_ptr: pointer to the Scsi_Host struct
 */
int eata_set_info(char *buffer, int length, struct Scsi_Host *HBA_ptr)
{
    int orig_length = length;

    if (length >= 8 && strncmp(buffer, "eata_dma", 8) == 0) {
	buffer += 9;
	length -= 9;
	if(length >= 8 && strncmp(buffer, "latency", 7) == 0) {
	    SD(HBA_ptr)->do_latency = TRUE;
	    return(orig_length);
	} 
	
	if(length >=10 && strncmp(buffer, "nolatency", 9) == 0) {
	    SD(HBA_ptr)->do_latency = FALSE;
	    return(orig_length);
	} 
	
	printk("Unknown command:%s length: %d\n", buffer, length);
    } else 
	printk("Wrong Signature:%10s\n", buffer);
    
    return(-EINVAL);
}

/*
 * eata_proc_info
 * inout : decides on the direction of the dataflow and the meaning of the 
 *	   variables
 * buffer: If inout==FALSE data is beeing written to it else read from it
 * *start: If inout==FALSE start of the valid data in the buffer
 * offset: If inout==FALSE offset from the beginning of the imaginary file 
 *	   from which we start writing into the buffer
 * length: If inout==FALSE max number of bytes to be written into the buffer 
 *	   else number of bytes in the buffer
 */
int eata_proc_info(char *buffer, char **start, off_t offset, int length, 
		   int hostno, int inout)
{

    Scsi_Device *scd, SDev;
    struct Scsi_Host *HBA_ptr;
    Scsi_Cmnd scmd;
    char cmnd[10];
    static u8 buff[512];
    static u8 buff2[512];
    hst_cmd_stat *rhcs, *whcs;
    coco	 *cc;
    scsitrans	 *st;
    scsimod	 *sm;
    hobu	 *hb;
    scbu	 *sb;
    boty	 *bt;
    memco	 *mc;
    firm	 *fm;
    subinf	 *si; 
    pcinf	 *pi;
    arrlim	 *al;
    int i, x; 
    int	  size, len = 0;
    off_t begin = 0;
    off_t pos = 0;
    scd = NULL;

    HBA_ptr = first_HBA;
    for (i = 1; i <= registered_HBAs; i++) {
	if (HBA_ptr->host_no == hostno)
	    break;
	HBA_ptr = SD(HBA_ptr)->next;
    }	     

    if(inout == TRUE) /* Has data been writen to the file ? */ 
	return(eata_set_info(buffer, length, HBA_ptr));

    if (offset == 0)
	memset(buff, 0, sizeof(buff));

    cc = (coco *)     (buff + 0x148);
    st = (scsitrans *)(buff + 0x164); 
    sm = (scsimod *)  (buff + 0x16c);
    hb = (hobu *)     (buff + 0x172);
    sb = (scbu *)     (buff + 0x178);
    bt = (boty *)     (buff + 0x17e);
    mc = (memco *)    (buff + 0x186);
    fm = (firm *)     (buff + 0x18e);
    si = (subinf *)   (buff + 0x196); 
    pi = (pcinf *)    (buff + 0x19c);
    al = (arrlim *)   (buff + 0x1a2);

    size = sprintf(buffer+len, "EATA (Extended Attachment) driver version: "
		   "%d.%d%s\n",VER_MAJOR, VER_MINOR, VER_SUB);
    len += size; pos = begin + len;
    size = sprintf(buffer + len, "queued commands:     %10ld\n"
		   "processed interrupts:%10ld\n", queue_counter, int_counter);
    len += size; pos = begin + len;

    size = sprintf(buffer + len, "\nscsi%-2d: HBA %.10s\n",
		   HBA_ptr->host_no, SD(HBA_ptr)->name);
    len += size; 
    pos = begin + len;
    size = sprintf(buffer + len, "Firmware revision: v%s\n", 
		   SD(HBA_ptr)->revision);
    len += size;
    pos = begin + len;
    size = sprintf(buffer + len, "Hardware Configuration:\n");
    len += size; 
    pos = begin + len;
    
    if(SD(HBA_ptr)->broken_INQUIRY == TRUE) {
	if (HBA_ptr->dma_channel == BUSMASTER)
	    size = sprintf(buffer + len, "DMA: BUSMASTER\n");
	else
	    size = sprintf(buffer + len, "DMA: %d\n", HBA_ptr->dma_channel);
	len += size; 
	pos = begin + len;

	size = sprintf(buffer + len, "Base IO : %#.4x\n", (u32) HBA_ptr->base);
	len += size; 
	pos = begin + len;

	size = sprintf(buffer + len, "Host Bus: EISA\n"); 
	len += size; 
	pos = begin + len;

    } else {
	memset(&SDev, 0, sizeof(Scsi_Device));
	memset(&scmd, 0, sizeof(Scsi_Cmnd));

	SDev.host = HBA_ptr;
	SDev.id = HBA_ptr->this_id;
	SDev.lun = 0;
	SDev.channel = 0;

	cmnd[0] = LOG_SENSE;
	cmnd[1] = 0;
	cmnd[2] = 0x33 + (3<<6);
	cmnd[3] = 0;
	cmnd[4] = 0;
	cmnd[5] = 0;
        cmnd[6] = 0;
	cmnd[7] = 0x00;
	cmnd[8] = 0x66;
	cmnd[9] = 0;

	scmd.cmd_len = 10;
	
	scmd.host = HBA_ptr; 
	scmd.device = &SDev;
	scmd.target = HBA_ptr->this_id; 
	scmd.lun = 0; 
	scmd.channel = 0;
	scmd.use_sg = 0;

	/*
	 * Do the command and wait for it to finish.
	 */	
	{
	    struct semaphore sem = MUTEX_LOCKED;
	    scmd.request.rq_status = RQ_SCSI_BUSY;
	    scmd.request.sem = &sem;
	    scsi_do_cmd (&scmd, cmnd, buff + 0x144, 0x66,  
			 eata_scsi_done, 1 * HZ, 1);
	    down(&sem);
	}

	size = sprintf(buffer + len, "IRQ: %2d, %s triggered\n", cc->interrupt,
		       (cc->intt == TRUE)?"level":"edge");
	len += size; 
	pos = begin + len;
	if (HBA_ptr->dma_channel == 0xff)
	    size = sprintf(buffer + len, "DMA: BUSMASTER\n");
	else
	    size = sprintf(buffer + len, "DMA: %d\n", HBA_ptr->dma_channel);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "CPU: MC680%02d %dMHz\n", bt->cpu_type,
		       bt->cpu_speed);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "Base IO : %#.4x\n", (u32) HBA_ptr->base);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "Host Bus: %s\n", 
		       (SD(HBA_ptr)->bustype == IS_PCI)?"PCI ":
		       (SD(HBA_ptr)->bustype == IS_EISA)?"EISA":"ISA ");
	
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "SCSI Bus:%s%s Speed: %sMB/sec. %s\n",
		       (sb->wide == TRUE)?" WIDE":"", 
		       (sb->dif == TRUE)?" DIFFERENTIAL":"",
		       (sb->speed == 0)?"5":(sb->speed == 1)?"10":"20",
		       (sb->ext == TRUE)?"With external cable detection":"");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "SCSI channel expansion Module: %s present\n",
		       (bt->sx1 == TRUE)?"SX1 (one channel)":
		       ((bt->sx2 == TRUE)?"SX2 (two channels)":"not"));
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "SmartRAID hardware: %spresent.\n",
		       (cc->srs == TRUE)?"":"not ");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "    Type: %s\n",
		       ((cc->key == TRUE)?((bt->dmi == TRUE)?"integrated"
					   :((bt->dm4 == TRUE)?"DM401X"
					   :(bt->dm4k == TRUE)?"DM4000"
					   :"-"))
					   :"-"));
	len += size; 
	pos = begin + len;
	
	size = sprintf(buffer + len, "    Max array groups:              %d\n",
		       (al->code == 0x0e)?al->max_groups:7);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "    Max drives per RAID 0 array:   %d\n",
		       (al->code == 0x0e)?al->raid0_drv:7);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "    Max drives per RAID 3/5 array: %d\n",
		       (al->code == 0x0e)?al->raid35_drv:7);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "Cache Module: %spresent.\n",
		       (cc->csh)?"":"not ");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "    Type: %s\n",
		       ((cc->csh == TRUE)?((bt->cmi == TRUE)?"integrated"
					 :((bt->cm4 == TRUE)?"CM401X"
					 :((bt->cm4k == TRUE)?"CM4000"
					 :"-")))
					 :"-"));
	len += size; 
	pos = begin + len;
	for (x = 0; x <= 3; x++) {
	    size = sprintf(buffer + len, "    Bank%d: %dMB with%s ECC\n",x,
			   mc->banksize[x] & 0x7f, 
			   (mc->banksize[x] & 0x80)?"":"out");
	    len += size; 
	    pos = begin + len;	    
	}   
	size = sprintf(buffer + len, "Timer Mod.: %spresent\n",
		       (cc->tmr == TRUE)?"":"not ");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "NVRAM     : %spresent\n",
		       (cc->nvr == TRUE)?"":"not ");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "SmartROM  : %sabled\n",
		       (bt->srom == TRUE)?"dis":"en");
	len += size; 
	pos = begin + len;
	size = sprintf(buffer + len, "Alarm     : %s\n",
		       (bt->alrm == TRUE)?"on":"off");
	len += size; 
	pos = begin + len;
	
	if (pos < offset) {
	    len = 0;
	    begin = pos;
	}
	if (pos > offset + length)
	    goto stop_output; 
	
	cmnd[0] = LOG_SENSE;
	cmnd[1] = 0;
	cmnd[2] = 0x32 + (3<<6); 
	cmnd[3] = 0;
	cmnd[4] = 0;
	cmnd[5] = 0;
	cmnd[6] = 0;
	cmnd[7] = 0x01;
	cmnd[8] = 0x44;
	cmnd[9] = 0;
 
	scmd.cmd_len = 10;

	/*
	 * Do the command and wait for it to finish.
	 */	
	{
	    struct semaphore sem = MUTEX_LOCKED;
	    scmd.request.rq_status = RQ_SCSI_BUSY;
	    scmd.request.sem = &sem;
	    scsi_do_cmd (&scmd, cmnd, buff2, 0x144,
			 eata_scsi_done, 1 * HZ, 1);
	    down(&sem);
	}

	swap_statistics(buff2);
	rhcs = (hst_cmd_stat *)(buff2 + 0x2c); 
	whcs = (hst_cmd_stat *)(buff2 + 0x8c);		 
	
	for (x = 0; x <= 11; x++) {
	    SD(HBA_ptr)->reads[x] += rhcs->sizes[x];
	    SD(HBA_ptr)->writes[x] += whcs->sizes[x];
	    SD(HBA_ptr)->reads[12] += rhcs->sizes[x];
	    SD(HBA_ptr)->writes[12] += whcs->sizes[x];
	}
	size = sprintf(buffer + len, "Host<->Disk command statistics:\n"
		       "         Reads:	     Writes:\n");
	len += size; 
	pos = begin + len;
	for (x = 0; x <= 10; x++) {
	    size = sprintf(buffer+len,"%5dk:%12u %12u\n", 1 << x,
			   SD(HBA_ptr)->reads[x], 
			   SD(HBA_ptr)->writes[x]);
	    len += size; 
	    pos = begin + len;
	}
	size = sprintf(buffer+len,">1024k:%12u %12u\n",
		       SD(HBA_ptr)->reads[11], 
		       SD(HBA_ptr)->writes[11]);
	len += size; 
	pos = begin + len;
	size = sprintf(buffer+len,"Sum   :%12u %12u\n",
		       SD(HBA_ptr)->reads[12], 
		       SD(HBA_ptr)->writes[12]);
	len += size; 
	pos = begin + len;
    }
    
    if (pos < offset) {
	len = 0;
	begin = pos;
    }
    if (pos > offset + length)
	goto stop_output;

    if(SD(HBA_ptr)->do_latency == TRUE) {
	size = sprintf(buffer + len, "Host Latency Command Statistics:\n"
		       "Current timer resolution: 10ms\n"
		       "         Reads:	      Min:(ms)     Max:(ms)     Ave:(ms)\n");
	len += size; 
	pos = begin + len;
	for (x = 0; x <= 10; x++) {
	    size = sprintf(buffer+len,"%5dk:%12u %12u %12u %12u\n", 
			   1 << x,
			   SD(HBA_ptr)->reads_lat[x][0], 
			   (SD(HBA_ptr)->reads_lat[x][1] == 0xffffffff) 
			   ? 0:(SD(HBA_ptr)->reads_lat[x][1] * 10), 
			   SD(HBA_ptr)->reads_lat[x][2] * 10, 
			   SD(HBA_ptr)->reads_lat[x][3] * 10 /
			   ((SD(HBA_ptr)->reads_lat[x][0])
			    ? SD(HBA_ptr)->reads_lat[x][0]:1));
	    len += size; 
	    pos = begin + len;
	}
	size = sprintf(buffer+len,">1024k:%12u %12u %12u %12u\n",
			   SD(HBA_ptr)->reads_lat[11][0], 
			   (SD(HBA_ptr)->reads_lat[11][1] == 0xffffffff)
			   ? 0:(SD(HBA_ptr)->reads_lat[11][1] * 10), 
			   SD(HBA_ptr)->reads_lat[11][2] * 10, 
			   SD(HBA_ptr)->reads_lat[11][3] * 10 /
			   ((SD(HBA_ptr)->reads_lat[x][0])
			    ? SD(HBA_ptr)->reads_lat[x][0]:1));
	len += size; 
	pos = begin + len;

	if (pos < offset) {
	    len = 0;
	    begin = pos;
	}
	if (pos > offset + length)
	    goto stop_output;

	size = sprintf(buffer + len,
		       "         Writes:      Min:(ms)     Max:(ms)     Ave:(ms)\n");
	len += size; 
	pos = begin + len;
	for (x = 0; x <= 10; x++) {
	    size = sprintf(buffer+len,"%5dk:%12u %12u %12u %12u\n", 
			   1 << x,
			   SD(HBA_ptr)->writes_lat[x][0], 
			   (SD(HBA_ptr)->writes_lat[x][1] == 0xffffffff)
			   ? 0:(SD(HBA_ptr)->writes_lat[x][1] * 10), 
			   SD(HBA_ptr)->writes_lat[x][2] * 10, 
			   SD(HBA_ptr)->writes_lat[x][3] * 10 /
			   ((SD(HBA_ptr)->writes_lat[x][0])
			    ? SD(HBA_ptr)->writes_lat[x][0]:1));
	    len += size; 
	    pos = begin + len;
	}
	size = sprintf(buffer+len,">1024k:%12u %12u %12u %12u\n",
			   SD(HBA_ptr)->writes_lat[11][0], 
			   (SD(HBA_ptr)->writes_lat[11][1] == 0xffffffff)
			   ? 0:(SD(HBA_ptr)->writes_lat[x][1] * 10), 
			   SD(HBA_ptr)->writes_lat[11][2] * 10, 
			   SD(HBA_ptr)->writes_lat[11][3] * 10/
			   ((SD(HBA_ptr)->writes_lat[x][0])
			    ? SD(HBA_ptr)->writes_lat[x][0]:1));
	len += size; 
	pos = begin + len;

	if (pos < offset) {
	    len = 0;
	    begin = pos;
	}
	if (pos > offset + length)
	    goto stop_output;
    }

#if 0
    scd = scsi_devices;
    
    size = sprintf(buffer+len,"Attached devices: %s\n", (scd)?"":"none");
    len += size; 
    pos = begin + len;
    
    while (scd) {
	if (scd->host == HBA_ptr) {
	    proc_print_scsidevice(scd, buffer, &size, len);
	    len += size; 
	    pos = begin + len;
	    
	    if (pos < offset) {
		len = 0;
		begin = pos;
	    }
	    if (pos > offset + length)
		goto stop_output;
	}
	scd = scd->next;
    }
#endif
    
 stop_output:
    DBG(DBG_PROC, printk("2pos: %ld offset: %ld len: %d\n", pos, offset, len));
    *start=buffer+(offset-begin);   /* Start of wanted data */
    len-=(offset-begin);	    /* Start slop */
    if(len>length)
	len = length;		    /* Ending slop */
    DBG(DBG_PROC, printk("3pos: %ld offset: %ld len: %d\n", pos, offset, len));
    
    return (len);     
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
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
