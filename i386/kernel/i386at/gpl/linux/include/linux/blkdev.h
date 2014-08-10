#ifndef _LINUX_BLKDEV_H
#define _LINUX_BLKDEV_H

#include <linux/major.h>
#include <linux/sched.h>
#include <linux/genhd.h>

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests when that is implemented. In
 * paging, 'bh' is NULL, and the semaphore is used to wait
 * for read/write completion.
 */
struct request {
	volatile int rq_status;	/* should split this into a few status bits */
#define RQ_INACTIVE		(-1)
#define RQ_ACTIVE		1
#define RQ_SCSI_BUSY		0xffff
#define RQ_SCSI_DONE		0xfffe
#define RQ_SCSI_DISCONNECTING	0xffe0

	kdev_t rq_dev;
	int cmd;		/* READ or WRITE */
	int errors;
	unsigned long sector;
	unsigned long nr_sectors;
	unsigned long current_nr_sectors;
	char * buffer;
	struct semaphore * sem;
	struct buffer_head * bh;
	struct buffer_head * bhtail;
	struct request * next;
};

struct blk_dev_struct {
	void (*request_fn)(void);
	struct request * current_request;
};

struct sec_size {
	unsigned block_size;
	unsigned block_size_bits;
};

extern struct sec_size * blk_sec[MAX_BLKDEV];
extern struct blk_dev_struct blk_dev[MAX_BLKDEV];
extern struct wait_queue * wait_for_request;
extern void resetup_one_dev(struct gendisk *dev, int drive);

extern int * blk_size[MAX_BLKDEV];

extern int * blksize_size[MAX_BLKDEV];

extern int * hardsect_size[MAX_BLKDEV];

#endif
