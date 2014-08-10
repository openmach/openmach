/*
 * Generic Generic NCR5380 driver
 *	
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 440-4894
 *
 * NCR53C400 extensions (c) 1994,1995,1996, Kevin Lentin
 *    K.Lentin@cs.monash.edu.au
 *
 * ALPHA RELEASE 1. 
 *
 * For more information, please consult 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

/* 
 * TODO : flesh out DMA support, find some one actually using this (I have
 * 	a memory mapped Trantor board that works fine)
 */

/*
 * Options :
 *
 * PARITY - enable parity checking.  Not supported.
 *
 * SCSI2 - enable support for SCSI-II tagged queueing.  Untested.
 *
 * USLEEP - enable support for devices that don't disconnect.  Untested.
 *
 * The card is detected and initialized in one of several ways : 
 * 1.  With command line overrides - NCR5380=port,irq may be 
 *     used on the LILO command line to override the defaults.
 *
 * 2.  With the GENERIC_NCR5380_OVERRIDE compile time define.  This is 
 *     specified as an array of address, irq, dma, board tuples.  Ie, for
 *     one board at 0x350, IRQ5, no dma, I could say  
 *     -DGENERIC_NCR5380_OVERRIDE={{0xcc000, 5, DMA_NONE, BOARD_NCR5380}}
 * 
 * -1 should be specified for no or DMA interrupt, -2 to autoprobe for an 
 * 	IRQ line if overridden on the command line.
 */
 
/*
 * $Log: g_NCR5380.c,v $
 * Revision 1.1  1996/03/25  20:25:35  goel
 * Linux driver merge.
 *
 */

#define AUTOPROBE_IRQ
#define AUTOSENSE

#include <linux/config.h>

#ifdef MACH
#define CONFIG_SCSI_G_NCR5380_MEM
#endif

#ifdef CONFIG_SCSI_GENERIC_NCR53C400
#define NCR53C400_PSEUDO_DMA 1
#define PSEUDO_DMA
#define NCR53C400
#endif
#if defined(CONFIG_SCSI_G_NCR5380_PORT) && defined(CONFIG_SCSI_G_NCR5380_MEM)
#error You can not configure the Generic NCR 5380 SCSI Driver for memory mapped I/O and port mapped I/O at the same time (yet)
#endif
#if !defined(CONFIG_SCSI_G_NCR5380_PORT) && !defined(CONFIG_SCSI_G_NCR5380_MEM)
#error You must configure the Generic NCR 5380 SCSI Driver for one of memory mapped I/O and port mapped I/O.
#endif

#include <asm/system.h>
#include <asm/io.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/blk.h>
#include "scsi.h"
#include "hosts.h"
#include "g_NCR5380.h"
#include "NCR5380.h"
#include "constants.h"
#include "sd.h"
#include<linux/stat.h>

struct proc_dir_entry proc_scsi_g_ncr5380 = {
    PROC_SCSI_GENERIC_NCR5380, 9, "g_NCR5380",
    S_IFDIR | S_IRUGO | S_IXUGO, 2
};

static struct override {
	NCR5380_implementation_fields;
    int irq;
    int dma;
    int board;	/* Use NCR53c400, Ricoh, etc. extensions ? */
} overrides 
#ifdef GENERIC_NCR5380_OVERRIDE 
    [] = GENERIC_NCR5380_OVERRIDE
#else
    [1] = {{0,},};
#endif

#define NO_OVERRIDES (sizeof(overrides) / sizeof(struct override))

/*
 * Function : static internal_setup(int board, char *str, int *ints)
 *
 * Purpose : LILO command line initialization of the overrides array,
 * 
 * Inputs : board - either BOARD_NCR5380 for a normal NCR5380 board, 
 * 	or BOARD_NCR53C400 for a NCR53C400 board. str - unused, ints - 
 *	array of integer parameters with ints[0] equal to the number of ints.
 *
 */

static void internal_setup(int board, char *str, int *ints) {
    static int commandline_current = 0;
    switch (board) {
    case BOARD_NCR5380:
	if (ints[0] != 2 && ints[0] != 3) 
	    printk("generic_NCR5380_setup : usage ncr5380=" STRVAL(NCR5380_map_name) ",irq,dma\n");
	return;
    case BOARD_NCR53C400:
	if (ints[0] != 2)
	    printk("generic_NCR53C400_setup : usage ncr53c400= " STRVAL(NCR5380_map_name) ",irq\n");
	return;
    }

    if (commandline_current < NO_OVERRIDES) {
	overrides[commandline_current].NCR5380_map_name = (NCR5380_map_type)ints[1];
	overrides[commandline_current].irq = ints[2];
	if (ints[0] == 3) 
	    overrides[commandline_current].dma = ints[3];
	else 
	    overrides[commandline_current].dma = DMA_NONE;
	overrides[commandline_current].board = board;
	    ++commandline_current;
    }
}

/*
 * Function : generic_NCR5380_setup (char *str, int *ints)
 *
 * Purpose : LILO command line initialization of the overrides array,
 * 
 * Inputs : str - unused, ints - array of integer paramters with ints[0] 
 * 	equal to the number of ints.
 */

void generic_NCR5380_setup (char *str, int *ints) {
    internal_setup (BOARD_NCR5380, str, ints);
}

/*
 * Function : generic_NCR53C400_setup (char *str, int *ints)
 *
 * Purpose : LILO command line initialization of the overrides array,
 * 
 * Inputs : str - unused, ints - array of integer paramters with ints[0] 
 * 	equal to the number of ints.
 */

void generic_NCR53C400_setup (char *str, int *ints) {
    internal_setup (BOARD_NCR53C400, str, ints);
}

/* 
 * Function : int generic_NCR5380_detect(Scsi_Host_Template * tpnt)
 *
 * Purpose : initializes generic NCR5380 driver based on the 
 *	command line / compile time port and irq definitions.
 *
 * Inputs : tpnt - template for this SCSI adapter.
 * 
 * Returns : 1 if a host adapter was found, 0 if not.
 *
 */

int generic_NCR5380_detect(Scsi_Host_Template * tpnt) {
    static int current_override = 0;
    int count;
    int flags = 0;
    struct Scsi_Host *instance;

    tpnt->proc_dir = &proc_scsi_g_ncr5380;

    for (count = 0; current_override < NO_OVERRIDES; ++current_override) {
	if (!(overrides[current_override].NCR5380_map_name))
	    continue;

	switch (overrides[current_override].board) {
	case BOARD_NCR5380:
	    flags = FLAG_NO_PSEUDO_DMA;
	    break;
	case BOARD_NCR53C400:
	    flags = FLAG_NCR53C400;
	    break;
	}

	instance = scsi_register (tpnt, sizeof(struct NCR5380_hostdata));
	instance->NCR5380_instance_name = overrides[current_override].NCR5380_map_name;

	NCR5380_init(instance, flags);

	if (overrides[current_override].irq != IRQ_AUTO)
	    instance->irq = overrides[current_override].irq;
	else 
	    instance->irq = NCR5380_probe_irq(instance, 0xffff);

	if (instance->irq != IRQ_NONE) 
	    if (request_irq(instance->irq, generic_NCR5380_intr, SA_INTERRUPT, "NCR5380")) {
		printk("scsi%d : IRQ%d not free, interrupts disabled\n", 
		    instance->host_no, instance->irq);
		instance->irq = IRQ_NONE;
	    } 

	if (instance->irq == IRQ_NONE) {
	    printk("scsi%d : interrupts not enabled. for better interactive performance,\n", instance->host_no);
	    printk("scsi%d : please jumper the board for a free IRQ.\n", instance->host_no);
	}

	printk("scsi%d : at " STRVAL(NCR5380_map_name) " 0x%x", instance->host_no, (unsigned int)instance->NCR5380_instance_name);
	if (instance->irq == IRQ_NONE)
	    printk (" interrupts disabled");
	else 
	    printk (" irq %d", instance->irq);
	printk(" options CAN_QUEUE=%d  CMD_PER_LUN=%d release=%d",
	    CAN_QUEUE, CMD_PER_LUN, GENERIC_NCR5380_PUBLIC_RELEASE);
	NCR5380_print_options(instance);
	printk("\n");

	++current_override;
	++count;
    }
    return count;
}

const char * generic_NCR5380_info (void) {
    static const char string[]="Generic NCR5380/53C400 Info";
    return string;
}

int generic_NCR5380_release_resources(struct Scsi_Host * instance)
{
    NCR5380_local_declare();

    NCR5380_setup(instance);

    free_irq(instance->irq);

	return 0;
}

#ifdef BIOSPARAM
/*
 * Function : int generic_NCR5380_biosparam(Disk * disk, kdev_t dev, int *ip)
 *
 * Purpose : Generates a BIOS / DOS compatable H-C-S mapping for 
 *	the specified device / size.
 * 
 * Inputs : size = size of device in sectors (512 bytes), dev = block device
 *	major / minor, ip[] = {heads, sectors, cylinders}  
 *
 * Returns : allways 0 (success), initializes ip
 *	
 */

/* 
 * XXX Most SCSI boards use this mapping, I could be incorrect.  Some one
 * using hard disks on a trantor should verify that this mapping corresponds
 * to that used by the BIOS / ASPI driver by running the linux fdisk program
 * and matching the H_C_S coordinates to what DOS uses.
 */

int generic_NCR5380_biosparam(Disk * disk, kdev_t dev, int *ip)
{
  int size = disk->capacity;
  ip[0] = 64;
  ip[1] = 32;
  ip[2] = size >> 11;
  return 0;
}
#endif

int generic_NCR5380_proc_info(char* buffer, char** start, off_t offset, int length, int hostno, int inout)
{
	int len = 0;
	struct Scsi_Host *scsi_ptr;

	for (scsi_ptr = first_instance; scsi_ptr; scsi_ptr=scsi_ptr->next)
		if (scsi_ptr->host_no == hostno)
			break;

	len += sprintf(buffer+len, "SCSI host number %d : %s\n", scsi_ptr->host_no,  scsi_ptr->hostt->name);
	len += sprintf(buffer+len, "Generic NCR5380 driver version %d\n", GENERIC_NCR5380_PUBLIC_RELEASE);
	len += sprintf(buffer+len, "NCR5380 driver core version %d\n", NCR5380_PUBLIC_RELEASE);
#ifdef NCR53C400
	len += sprintf(buffer+len, "NCR53C400 driver extension version %d\n", NCR53C400_PUBLIC_RELEASE);
	len += sprintf(buffer+len, "NCR53C400 card%s detected\n",  (((struct NCR5380_hostdata *)scsi_ptr->hostdata)->flags & FLAG_NCR53C400)?"":" not");
# if NCR53C400_PSEUDO_DMA
	len += sprintf(buffer+len, "NCR53C400 pseudo DMA being used\n");
# endif
#else
	len += sprintf(buffer+len, "NO NCR53C400 driver extensions\n");
#endif
	len += sprintf(buffer+len, "Using %s mapping at %s 0x%x, ", STRVAL(NCR5380_map_config), STRVAL(NCR5380_map_name), scsi_ptr->NCR5380_instance_name);
	if (scsi_ptr->irq == IRQ_NONE)
		len += sprintf(buffer+len, "interrupts disabled\n");
	else
		len += sprintf(buffer+len, "on interrupt %d\n", scsi_ptr->irq);

	*start = buffer + offset;
	len -= offset;
	if (len > length)
		len = length;
	return len;
}

#if NCR53C400_PSEUDO_DMA
static inline int NCR5380_pread (struct Scsi_Host *instance, unsigned char *dst,    int len)
{
    int blocks = len / 128;
    int start = 0;
    int i;
    int bl;
    NCR5380_local_declare();

    NCR5380_setup(instance);

#if (NDEBUG & NDEBUG_C400_PREAD)
    printk("53C400r: About to read %d blocks for %d bytes\n", blocks, len);
#endif

    NCR5380_write(C400_CONTROL_STATUS_REG, CSR_BASE | CSR_TRANS_DIR);
    NCR5380_write(C400_BLOCK_COUNTER_REG, blocks);
    while (1) {
    
#if (NDEBUG & NDEBUG_C400_PREAD)
	printk("53C400r: %d blocks left\n", blocks);
#endif

	if ((bl=NCR5380_read(C400_BLOCK_COUNTER_REG)) == 0) {
#if (NDEBUG & NDEBUG_C400_PREAD)
	    if (blocks)
		printk("53C400r: blocks still == %d\n", blocks);
	    else
		printk("53C400r: Exiting loop\n");
#endif
	    break;
	}

#if 1
	if (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ) {
	    printk("53C400r: Got 53C80_IRQ start=%d, blocks=%d\n", start, blocks);
	    return -1;
	}
#endif

#if (NDEBUG & NDEBUG_C400_PREAD)
	printk("53C400r: Waiting for buffer, bl=%d\n", bl);
#endif

	while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
	    ;
#if (NDEBUG & NDEBUG_C400_PREAD)
	printk("53C400r: Transferring 128 bytes\n");
#endif

#ifdef CONFIG_SCSI_G_NCR5380_PORT
	for (i=0; i<128; i++)
	    dst[start+i] = NCR5380_read(C400_HOST_BUFFER);
#else
	/* implies CONFIG_SCSI_G_NCR5380_MEM */
	memmove(dst+start,NCR53C400_host_buffer+NCR5380_map_name,128);
#endif
	start+=128;
	blocks--;
    }

#if (NDEBUG & NDEBUG_C400_PREAD)
    printk("53C400r: EXTRA: Waiting for buffer\n");
#endif
    while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
	;

#if (NDEBUG & NDEBUG_C400_PREAD)
    printk("53C400r: Transferring EXTRA 128 bytes\n");
#endif
#ifdef CONFIG_SCSI_G_NCR5380_PORT
	for (i=0; i<128; i++)
	    dst[start+i] = NCR5380_read(C400_HOST_BUFFER);
#else
	/* implies CONFIG_SCSI_G_NCR5380_MEM */
	memmove(dst+start,NCR53C400_host_buffer+NCR5380_map_name,128);
#endif
    start+=128;
    blocks--;

#if (NDEBUG & NDEBUG_C400_PREAD)
    printk("53C400r: Final values: blocks=%d   start=%d\n", blocks, start);
#endif

    if (!(NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ))
	printk("53C400r: no 53C80 gated irq after transfer");
#if (NDEBUG & NDEBUG_C400_PREAD)
    else
	printk("53C400r: Got 53C80 interupt and tried to clear it\n");
#endif

/* DON'T DO THIS - THEY NEVER ARRIVE!
    printk("53C400r: Waiting for 53C80 registers\n");
    while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_53C80_REG)
	;
*/

    if (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_END_DMA_TRANSFER))
	printk("53C400r: no end dma signal\n");
#if (NDEBUG & NDEBUG_C400_PREAD)
    else
	printk("53C400r: end dma as expected\n");
#endif

    NCR5380_write(MODE_REG, MR_BASE);
    NCR5380_read(RESET_PARITY_INTERRUPT_REG);
    return 0;
}
		
static inline int NCR5380_pwrite (struct Scsi_Host *instance, unsigned char *src,    int len)
{
    int blocks = len / 128;
    int start = 0;
    int i;
    int bl;
    NCR5380_local_declare();

    NCR5380_setup(instance);

#if (NDEBUG & NDEBUG_C400_PWRITE)
    printk("53C400w: About to write %d blocks for %d bytes\n", blocks, len);
#endif

    NCR5380_write(C400_CONTROL_STATUS_REG, CSR_BASE);
    NCR5380_write(C400_BLOCK_COUNTER_REG, blocks);
    while (1) {
	if (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ) {
	    printk("53C400w: Got 53C80_IRQ start=%d, blocks=%d\n", start, blocks);
	    return -1;
	}

	if ((bl=NCR5380_read(C400_BLOCK_COUNTER_REG)) == 0) {
#if (NDEBUG & NDEBUG_C400_PWRITE)
	    if (blocks)
		printk("53C400w: exiting loop, blocks still == %d\n", blocks);
	    else
		printk("53C400w: exiting loop\n");
#endif
	    break;
	}

#if (NDEBUG & NDEBUG_C400_PWRITE)
	printk("53C400w: %d blocks left\n", blocks);

	printk("53C400w: waiting for buffer, bl=%d\n", bl);
#endif
	while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
	    ;

#if (NDEBUG & NDEBUG_C400_PWRITE)
	printk("53C400w: transferring 128 bytes\n");
#endif
#ifdef CONFIG_SCSI_G_NCR5380_PORT
	for (i=0; i<128; i++)
	    NCR5380_write(C400_HOST_BUFFER, src[start+i]);
#else
	/* implies CONFIG_SCSI_G_NCR5380_MEM */
	memmove(NCR53C400_host_buffer+NCR5380_map_name,src+start,128);
#endif
	start+=128;
	blocks--;
    }
    if (blocks) {
#if (NDEBUG & NDEBUG_C400_PWRITE)
	printk("53C400w: EXTRA waiting for buffer\n");
#endif
	while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_HOST_BUF_NOT_RDY)
	    ;

#if (NDEBUG & NDEBUG_C400_PWRITE)
	printk("53C400w: transferring EXTRA 128 bytes\n");
#endif
#ifdef CONFIG_SCSI_G_NCR5380_PORT
	for (i=0; i<128; i++)
	    NCR5380_write(C400_HOST_BUFFER, src[start+i]);
#else
	/* implies CONFIG_SCSI_G_NCR5380_MEM */
	memmove(NCR53C400_host_buffer+NCR5380_map_name,src+start,128);
#endif
	start+=128;
	blocks--;
    }
#if (NDEBUG & NDEBUG_C400_PWRITE)
    else
	printk("53C400w: No EXTRA required\n");
#endif
    
#if (NDEBUG & NDEBUG_C400_PWRITE)
    printk("53C400w: Final values: blocks=%d   start=%d\n", blocks, start);
#endif

#if 0
    printk("53C400w: waiting for registers to be available\n");
    THEY NEVER DO!
    while (NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_53C80_REG)
	;
    printk("53C400w: Got em\n");
#endif

    /* Let's wait for this instead - could be ugly */
    /* All documentation says to check for this. Maybe my hardware is too
     * fast. Waiting for it seems to work fine! KLL
     */
    while (!(i = NCR5380_read(C400_CONTROL_STATUS_REG) & CSR_GATED_53C80_IRQ))
	;

    /*
     * I know. i is certainly != 0 here but the loop is new. See previous
     * comment.
     */
    if (i) {
#if (NDEBUG & NDEBUG_C400_PWRITE)
	prink("53C400w: got 53C80 gated irq (last block)\n");
#endif
	if (!((i=NCR5380_read(BUS_AND_STATUS_REG)) & BASR_END_DMA_TRANSFER))
	    printk("53C400w: No END OF DMA bit - WHOOPS! BASR=%0x\n",i);
#if (NDEBUG & NDEBUG_C400_PWRITE)
	else
	    printk("53C400w: Got END OF DMA\n");
#endif
    }
    else
	printk("53C400w: no 53C80 gated irq after transfer (last block)\n");

#if 0
    if (!(NCR5380_read(BUS_AND_STATUS_REG) & BASR_END_DMA_TRANSFER)) {
	printk("53C400w: no end dma signal\n");
    }
#endif

#if (NDEBUG & NDEBUG_C400_PWRITE)
    printk("53C400w: waiting for last byte...\n");
#endif
    while (!(NCR5380_read(TARGET_COMMAND_REG) & TCR_LAST_BYTE_SENT))
    	;

#if (NDEBUG & NDEBUG_C400_PWRITE)
    printk("53C400w:     got last byte.\n");
    printk("53C400w: pwrite exiting with status 0, whoopee!\n");
#endif
    return 0;
}
#endif /* PSEUDO_DMA */

#ifdef MACH
#include "NCR5380.src"
#else
#include "NCR5380.c"
#endif

#ifdef MODULE
/* Eventually this will go into an include file, but this will be later */
Scsi_Host_Template driver_template = GENERIC_NCR5380;

#include <linux/module.h>
#include "scsi_module.c"
#endif
