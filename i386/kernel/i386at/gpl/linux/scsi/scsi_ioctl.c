/*
 * Don't import our own symbols, as this would severely mess up our
 * symbol tables.
 */
#define _SCSI_SYMS_VER_
#define __NO_VERSION__
#include <linux/module.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>

#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "scsi_ioctl.h"

#define MAX_RETRIES 5   
#define MAX_TIMEOUT 900
#define MAX_BUF 4096

#define max(a,b) (((a) > (b)) ? (a) : (b))

/*
 * If we are told to probe a host, we will return 0 if  the host is not
 * present, 1 if the host is present, and will return an identifying
 * string at *arg, if arg is non null, filling to the length stored at
 * (int *) arg
 */

static int ioctl_probe(struct Scsi_Host * host, void *buffer)
{
    int temp, result;
    unsigned int len,slen;
    const char * string;
    
    if ((temp = host->hostt->present) && buffer) {
        result = verify_area(VERIFY_READ, buffer, sizeof(long));
        if (result) return result;

	len = get_user ((unsigned int *) buffer);
	if(host->hostt->info)
	    string = host->hostt->info(host);
	else 
	    string = host->hostt->name;
	if(string) {
	    slen = strlen(string);
	    if (len > slen)
		len = slen + 1;
            result = verify_area(VERIFY_WRITE, buffer, len);
            if (result) return result;

	    memcpy_tofs (buffer, string, len);
	}
    }
    return temp;
}

/*
 * 
 * The SCSI_IOCTL_SEND_COMMAND ioctl sends a command out to the SCSI host.
 * The MAX_TIMEOUT and MAX_RETRIES  variables are used.  
 * 
 * dev is the SCSI device struct ptr, *(int *) arg is the length of the
 * input data, if any, not including the command string & counts, 
 * *((int *)arg + 1) is the output buffer size in bytes.
 * 
 * *(char *) ((int *) arg)[2] the actual command byte.   
 * 
 * Note that no more than MAX_BUF data bytes will be transfered.  Since
 * SCSI block device size is 512 bytes, I figured 1K was good.
 * but (WDE) changed it to 8192 to handle large bad track buffers.
 * ERY: I changed this to a dynamic allocation using scsi_malloc - we were
 * getting a kernel stack overflow which was crashing the system when we
 * were using 8192 bytes.
 * 
 * This size *does not* include the initial lengths that were passed.
 * 
 * The SCSI command is read from the memory location immediately after the
 * length words, and the input data is right after the command.  The SCSI
 * routines know the command size based on the opcode decode.  
 * 
 * The output area is then filled in starting from the command byte. 
 */

static void scsi_ioctl_done (Scsi_Cmnd * SCpnt)
{
    struct request * req;
    
    req = &SCpnt->request;
    req->rq_status = RQ_SCSI_DONE; /* Busy, but indicate request done */
    
    if (req->sem != NULL) {
	up(req->sem);
    }
}   

static int ioctl_internal_command(Scsi_Device *dev, char * cmd)
{
    int result;
    Scsi_Cmnd * SCpnt;
    
    SCpnt = allocate_device(NULL, dev, 1);
    {
	struct semaphore sem = MUTEX_LOCKED;
	SCpnt->request.sem = &sem;
	scsi_do_cmd(SCpnt,  cmd, NULL,  0,
		    scsi_ioctl_done,  MAX_TIMEOUT,
		    MAX_RETRIES);
	down(&sem);
    }
    
    if(driver_byte(SCpnt->result) != 0)
	switch(SCpnt->sense_buffer[2] & 0xf) {
	case ILLEGAL_REQUEST:
	    if(cmd[0] == ALLOW_MEDIUM_REMOVAL) dev->lockable = 0;
	    else printk("SCSI device (ioctl) reports ILLEGAL REQUEST.\n");
	    break;
	case NOT_READY: /* This happens if there is no disc in drive */
	    if(dev->removable){
		printk(KERN_INFO "Device not ready.  Make sure there is a disc in the drive.\n");
		break;
	    };
	case UNIT_ATTENTION:
	    if (dev->removable){
		dev->changed = 1;
		SCpnt->result = 0; /* This is no longer considered an error */
		printk(KERN_INFO "Disc change detected.\n");
		break;
	    };
	default: /* Fall through for non-removable media */
	    printk("SCSI error: host %d id %d lun %d return code = %x\n",
		   dev->host->host_no,
		   dev->id,
		   dev->lun,
		   SCpnt->result);
	    printk("\tSense class %x, sense error %x, extended sense %x\n",
		   sense_class(SCpnt->sense_buffer[0]),
		   sense_error(SCpnt->sense_buffer[0]),
		   SCpnt->sense_buffer[2] & 0xf);
	    
	};
    
    result = SCpnt->result;
    SCpnt->request.rq_status = RQ_INACTIVE;
    wake_up(&SCpnt->device->device_wait);
    return result;
}

/*
 * This interface is depreciated - users should use the scsi generics
 * interface instead, as this is a more flexible approach to performing
 * generic SCSI commands on a device.
 */
static int ioctl_command(Scsi_Device *dev, void *buffer)
{
    char * buf;
    char cmd[12];
    char * cmd_in;
    Scsi_Cmnd * SCpnt;
    unsigned char opcode;
    int inlen, outlen, cmdlen;
    int needed, buf_needed;
    int result;
    
    if (!buffer)
	return -EINVAL;
    

    /*
     * Verify that we can read at least this much.
     */
    result = verify_area(VERIFY_READ, buffer, 2*sizeof(long) + 1);
    if (result) return result;

    /*
     * The structure that we are passed should look like:
     *
     * struct sdata{
     *	int inlen;
     *	int outlen;
     *	char cmd[];  # However many bytes are used for cmd.
     *	char data[];
     */
    inlen = get_user((unsigned int *) buffer);
    outlen = get_user( ((unsigned int *) buffer) + 1);
    
    /*
     * We do not transfer more than MAX_BUF with this interface.
     * If the user needs to transfer more data than this, they
     * should use scsi_generics instead.
     */
    if( inlen > MAX_BUF ) inlen = MAX_BUF;
    if( outlen > MAX_BUF ) outlen = MAX_BUF;

    cmd_in = (char *) ( ((int *)buffer) + 2);
    opcode = get_user(cmd_in); 
    
    needed = buf_needed = (inlen > outlen ? inlen : outlen);
    if(buf_needed){
	buf_needed = (buf_needed + 511) & ~511;
	if (buf_needed > MAX_BUF) buf_needed = MAX_BUF;
	buf = (char *) scsi_malloc(buf_needed);
	if (!buf) return -ENOMEM;
	memset(buf, 0, buf_needed);
    } else
	buf = NULL;
    
    /*
     * Obtain the command from the user's address space.
     */
    cmdlen = COMMAND_SIZE(opcode);

    result = verify_area(VERIFY_READ, cmd_in, 
                         cmdlen + inlen > MAX_BUF ? MAX_BUF : inlen);
    if (result) return result;

    memcpy_fromfs ((void *) cmd,  cmd_in,  cmdlen);
    
    /*
     * Obtain the data to be sent to the device (if any).
     */
    memcpy_fromfs ((void *) buf,  
                   (void *) (cmd_in + cmdlen), 
                   inlen);
    
    /*
     * Set the lun field to the correct value.
     */
    cmd[1] = ( cmd[1] & 0x1f ) | (dev->lun << 5);
    
#ifndef DEBUG_NO_CMD
    
    SCpnt = allocate_device(NULL, dev, 1);

    {
	struct semaphore sem = MUTEX_LOCKED;
	SCpnt->request.sem = &sem;
	scsi_do_cmd(SCpnt,  cmd,  buf, needed,  scsi_ioctl_done,  MAX_TIMEOUT, 
		    MAX_RETRIES);
	down(&sem);
    }
    
    /* 
     * If there was an error condition, pass the info back to the user. 
     */
    if(SCpnt->result) {
        result = verify_area(VERIFY_WRITE, 
                             cmd_in, 
                             sizeof(SCpnt->sense_buffer));
        if (result) return result;
        memcpy_tofs((void *) cmd_in,  
                    SCpnt->sense_buffer, 
                    sizeof(SCpnt->sense_buffer));
    } else {
        result = verify_area(VERIFY_WRITE, cmd_in, outlen);
        if (result) return result;
        memcpy_tofs ((void *) cmd_in,  buf,  outlen);
    }
    result = SCpnt->result;

    SCpnt->request.rq_status = RQ_INACTIVE;

    if (buf) scsi_free(buf, buf_needed);
    
    if(SCpnt->device->scsi_request_fn)
	(*SCpnt->device->scsi_request_fn)();
    
    wake_up(&SCpnt->device->device_wait);
    return result;
#else
    {
	int i;
	printk("scsi_ioctl : device %d.  command = ", dev->id);
	for (i = 0; i < 12; ++i)
	    printk("%02x ", cmd[i]);
	printk("\nbuffer =");
	for (i = 0; i < 20; ++i)
	    printk("%02x ", buf[i]);
	printk("\n");
	printk("inlen = %d, outlen = %d, cmdlen = %d\n",
	       inlen, outlen, cmdlen);
	printk("buffer = %d, cmd_in = %d\n", buffer, cmd_in);
    }
    return 0;
#endif
}

/*
 * the scsi_ioctl() function differs from most ioctls in that it does
 * not take a major/minor number as the dev filed.  Rather, it takes
 * a pointer to a scsi_devices[] element, a structure. 
 */
int scsi_ioctl (Scsi_Device *dev, int cmd, void *arg)
{
    int result;
    char scsi_cmd[12];
    
    /* No idea how this happens.... */
    if (!dev) return -ENXIO;
    
    switch (cmd) {
    case SCSI_IOCTL_GET_IDLUN:
        result = verify_area(VERIFY_WRITE, (void *) arg, 2*sizeof(long));
        if (result) return result;

	put_user(dev->id 
                 + (dev->lun << 8) 
                 + (dev->channel << 16)
                 + ((dev->host->hostt->proc_dir->low_ino & 0xff) << 24),
		    (unsigned long *) arg);
        put_user( dev->host->unique_id, (unsigned long *) arg+1);
	return 0;
    case SCSI_IOCTL_TAGGED_ENABLE:
	if(!suser())  return -EACCES;
	if(!dev->tagged_supported) return -EINVAL;
	dev->tagged_queue = 1;
	dev->current_tag = 1;
	break;
    case SCSI_IOCTL_TAGGED_DISABLE:
	if(!suser())  return -EACCES;
	if(!dev->tagged_supported) return -EINVAL;
	dev->tagged_queue = 0;
	dev->current_tag = 0;
	break;
    case SCSI_IOCTL_PROBE_HOST:
	return ioctl_probe(dev->host, arg);
    case SCSI_IOCTL_SEND_COMMAND:
	if(!suser())  return -EACCES;
	return ioctl_command((Scsi_Device *) dev, arg);
    case SCSI_IOCTL_DOORLOCK:
	if (!dev->removable || !dev->lockable) return 0;
	scsi_cmd[0] = ALLOW_MEDIUM_REMOVAL;
	scsi_cmd[1] = dev->lun << 5;
	scsi_cmd[2] = scsi_cmd[3] = scsi_cmd[5] = 0;
	scsi_cmd[4] = SCSI_REMOVAL_PREVENT;
	return ioctl_internal_command((Scsi_Device *) dev, scsi_cmd);
	break;
    case SCSI_IOCTL_DOORUNLOCK:
	if (!dev->removable || !dev->lockable) return 0;
	scsi_cmd[0] = ALLOW_MEDIUM_REMOVAL;
	scsi_cmd[1] = dev->lun << 5;
	scsi_cmd[2] = scsi_cmd[3] = scsi_cmd[5] = 0;
	scsi_cmd[4] = SCSI_REMOVAL_ALLOW;
	return ioctl_internal_command((Scsi_Device *) dev, scsi_cmd);
    case SCSI_IOCTL_TEST_UNIT_READY:
	scsi_cmd[0] = TEST_UNIT_READY;
	scsi_cmd[1] = dev->lun << 5;
	scsi_cmd[2] = scsi_cmd[3] = scsi_cmd[5] = 0;
	scsi_cmd[4] = 0;
	return ioctl_internal_command((Scsi_Device *) dev, scsi_cmd);
	break;
    default :           
	return -EINVAL;
    }
    return -EINVAL;
}

/*
 * Just like scsi_ioctl, only callable from kernel space with no 
 * fs segment fiddling.
 */

int kernel_scsi_ioctl (Scsi_Device *dev, int cmd, void *arg) {
    unsigned long oldfs;
    int tmp;
    oldfs = get_fs();
    set_fs(get_ds());
    tmp = scsi_ioctl (dev, cmd, arg);
    set_fs(oldfs);
    return tmp;
}

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
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
