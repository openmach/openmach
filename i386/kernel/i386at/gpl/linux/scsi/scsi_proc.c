/*
 * linux/drivers/scsi/scsi_proc.c
 *
 * The functions in this file provide an interface between
 * the PROC file system and the SCSI device drivers
 * It is mainly used for debugging, statistics and to pass 
 * information directly to the lowlevel driver.
 *
 * (c) 1995 Michael Neuffer neuffer@goofy.zdv.uni-mainz.de 
 * Version: 0.99.8   last change: 95/09/13
 * 
 * generic command parser provided by: 
 * Andreas Heilwagen <crashcar@informatik.uni-koblenz.de>
 */

/*
 * Don't import our own symbols, as this would severely mess up our
 * symbol tables.
 */
#define _SCSI_SYMS_VER_
#define __NO_VERSION__
#include <linux/module.h>

#include <linux/string.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

extern int scsi_proc_info(char *, char **, off_t, int, int, int);
 
struct scsi_dir {
    struct proc_dir_entry entry;
    char name[4];
};


/* generic_proc_info
 * Used if the driver currently has no own support for /proc/scsi
 */
int generic_proc_info(char *buffer, char **start, off_t offset, 
		     int length, int inode, int inout)
{
    int len, pos, begin;

    if(inout == TRUE)
	return(-ENOSYS);  /* This is a no-op */
    
    begin = 0;
    pos = len = sprintf(buffer, 
			"The driver does not yet support the proc-fs\n");
    if(pos < offset) {
	len = 0;
	begin = pos;
    }
    
    *start = buffer + (offset - begin);   /* Start of wanted data */
    len -= (offset - begin);
    if(len > length)
	len = length;
    
    return(len);
}

/* dispatch_scsi_info is the central dispatcher 
 * It is the interface between the proc-fs and the SCSI subsystem code
 */
extern int dispatch_scsi_info(int ino, char *buffer, char **start, 
			      off_t offset, int length, int func)
{
    struct Scsi_Host *hpnt = scsi_hostlist;
    
    if(ino == PROC_SCSI_SCSI) {            
        /*
         * This is for the scsi core, rather than any specific
         * lowlevel driver.
         */
        return(scsi_proc_info(buffer, start, offset, length, 0, func));
    }
    
    while(hpnt) {
        if (ino == (hpnt->host_no + PROC_SCSI_FILE)) {
            if(hpnt->hostt->proc_info == NULL)
                return generic_proc_info(buffer, start, offset, length, 
                                         hpnt->host_no, func);
            else
                return(hpnt->hostt->proc_info(buffer, start, offset, 
                                              length, hpnt->host_no, func));
        }
        hpnt = hpnt->next;
    }
    return(-EBADF);
}

void build_proc_dir_entries(Scsi_Host_Template *tpnt)
{
    struct Scsi_Host *hpnt;

    struct scsi_dir *scsi_hba_dir;

    proc_scsi_register(0, tpnt->proc_dir);
    
    hpnt = scsi_hostlist;
    while (hpnt) {
        if (tpnt == hpnt->hostt) {
            scsi_hba_dir = scsi_init_malloc(sizeof(struct scsi_dir), GFP_KERNEL);
            if(scsi_hba_dir == NULL)
                panic("Not enough memory to register SCSI HBA in /proc/scsi !\n");
            memset(scsi_hba_dir, 0, sizeof(struct scsi_dir));
            scsi_hba_dir->entry.low_ino = PROC_SCSI_FILE + hpnt->host_no;
            scsi_hba_dir->entry.namelen = sprintf(scsi_hba_dir->name,"%d",
                                                    hpnt->host_no);
            scsi_hba_dir->entry.name = scsi_hba_dir->name;
            scsi_hba_dir->entry.mode = S_IFREG | S_IRUGO | S_IWUSR;
            proc_scsi_register(tpnt->proc_dir, &scsi_hba_dir->entry);
        }
        hpnt = hpnt->next;
    }
}

/*
 *  parseHandle *parseInit(char *buf, char *cmdList, int cmdNum); 
 *	 	gets a pointer to a null terminated data buffer
 *		and a list of commands with blanks as delimiter 
 *      in between. 
 *      The commands have to be alphanumerically sorted. 
 *      cmdNum has to contain the number of commands.
 *		On success, a pointer to a handle structure
 *		is returned, NULL on failure
 *
 *	int parseOpt(parseHandle *handle, char **param);
 *		processes the next parameter. On success, the
 *		index of the appropriate command in the cmdList
 *		is returned, starting with zero.
 *		param points to the null terminated parameter string.
 *		On failure, -1 is returned.
 *
 *	The databuffer buf may only contain pairs of commands
 *	    options, separated by blanks:
 *		<Command> <Parameter> [<Command> <Parameter>]*
 */

typedef struct
{
    char *buf,				   /* command buffer  */
	 *cmdList,                         /* command list    */
	 *bufPos,                          /* actual position */
	 **cmdPos,                         /* cmdList index   */
	 cmdNum;                           /* cmd number      */
} parseHandle;
	

inline int parseFree (parseHandle *handle)               /* free memory     */
{
    kfree (handle->cmdPos);
    kfree (handle);
    
    return(-1);
}

	
parseHandle *parseInit(char *buf, char *cmdList, int cmdNum)
{
    char        *ptr;                               /* temp pointer    */
    parseHandle *handle;                            /* new handle      */
    
    if (!buf || !cmdList)                           /* bad input ?     */
	return(NULL);
    if ((handle = (parseHandle*) kmalloc(sizeof(parseHandle), 1)) == 0)
	return(NULL);                               /* out of memory   */
    if ((handle->cmdPos = (char**) kmalloc(sizeof(int), cmdNum)) == 0) {
	kfree(handle);
	return(NULL);                               /* out of memory   */
    }
    
    handle->buf     = handle->bufPos = buf;         /* init handle     */
    handle->cmdList = cmdList;
    handle->cmdNum  = cmdNum;
    
    handle->cmdPos[cmdNum = 0] = cmdList;
    for (ptr = cmdList; *ptr; ptr++) {          /* scan command string */
	if(*ptr == ' ') {                       /* and insert zeroes   */
	    *ptr++ = 0;
	    handle->cmdPos[++cmdNum] = ptr++;
	} 
    }
    return(handle);
}


int parseOpt(parseHandle *handle, char **param)
{
    int  cmdIndex = 0, 
	 cmdLen = 0;
    char *startPos;
    
    if (!handle)                                    /* invalid handle  */
	return(parseFree(handle));
    /* skip spaces     */  
    for (; *(handle->bufPos) && *(handle->bufPos) == ' '; handle->bufPos++);
    if (!*(handle->bufPos))
	return(parseFree(handle));                  /* end of data     */
    
    startPos = handle->bufPos;                      /* store cmd start */
    for (; handle->cmdPos[cmdIndex][cmdLen] && *(handle->bufPos); handle->bufPos++)
    {                                               /* no string end?  */
	for (;;)
	{
	    if (*(handle->bufPos) == handle->cmdPos[cmdIndex][cmdLen])
		break;                              /* char matches ?  */
	    else
		if (memcmp(startPos, (char*)(handle->cmdPos[++cmdIndex]), cmdLen))
		    return(parseFree(handle));      /* unknown command */
	    
	    if (cmdIndex >= handle->cmdNum)
		return(parseFree(handle));          /* unknown command */     
	}
	
	cmdLen++;                                   /* next char       */
    }
    
    /* Get param. First skip all blanks, then insert zero after param  */
    
    for (; *(handle->bufPos) && *(handle->bufPos) == ' '; handle->bufPos++);
    *param = handle->bufPos; 
    
    for (; *(handle->bufPos) && *(handle->bufPos) != ' '; handle->bufPos++);
    *(handle->bufPos++) = 0;
    
    return(cmdIndex);
}

#define MAX_SCSI_DEVICE_CODE 10
const char *const scsi_dev_types[MAX_SCSI_DEVICE_CODE] =
{
    "Direct-Access    ",
    "Sequential-Access",
    "Printer          ",
    "Processor        ",
    "WORM             ",
    "CD-ROM           ",
    "Scanner          ",
    "Optical Device   ",
    "Medium Changer   ",
    "Communications   "
};

void proc_print_scsidevice(Scsi_Device *scd, char *buffer, int *size, int len)
{	    
    int x, y = *size;
    
    y = sprintf(buffer + len, 
		    "Host: scsi%d Channel: %02d Id: %02d Lun: %02d\n  Vendor: ",
		    scd->host->host_no, scd->channel, scd->id, scd->lun);
    for (x = 0; x < 8; x++) {
	if (scd->vendor[x] >= 0x20)
	    y += sprintf(buffer + len + y, "%c", scd->vendor[x]);
	else
	    y += sprintf(buffer + len + y," ");
    }
    y += sprintf(buffer + len + y, " Model: ");
    for (x = 0; x < 16; x++) {
	if (scd->model[x] >= 0x20)
	    y +=  sprintf(buffer + len + y, "%c", scd->model[x]);
	else
	    y += sprintf(buffer + len + y, " ");
    }
    y += sprintf(buffer + len + y, " Rev: ");
    for (x = 0; x < 4; x++) {
	if (scd->rev[x] >= 0x20)
	    y += sprintf(buffer + len + y, "%c", scd->rev[x]);
	else
	    y += sprintf(buffer + len + y, " ");
    }
    y += sprintf(buffer + len + y, "\n");
    
    y += sprintf(buffer + len + y, "  Type:   %s ",
		     scd->type < MAX_SCSI_DEVICE_CODE ? 
		     scsi_dev_types[(int)scd->type] : "Unknown          " );
    y += sprintf(buffer + len + y, "               ANSI"
		     " SCSI revision: %02x", (scd->scsi_level < 3)?1:2);
    if (scd->scsi_level == 2)
	y += sprintf(buffer + len + y, " CCS\n");
    else
	y += sprintf(buffer + len + y, "\n");

    *size = y; 
    return;
}

/*
 * Overrides for Emacs so that we get a uniform tabbing style.
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
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
