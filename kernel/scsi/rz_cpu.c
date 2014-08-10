/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File: rz_cpu.c
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	7/91
 *
 *	Top layer of the SCSI driver: interface with the MI.
 *	This file contains operations specific to CPU-like devices.
 *
 * We handle here the case of simple devices which do not use any
 * sophisticated host-to-host communication protocol, they look
 * very much like degenerative cases of TAPE devices.
 *
 * For documentation and debugging, we also provide code to act like one.
 */

#include <mach/std_types.h>
#include <scsi/compat_30.h>

#include <scsi/scsi.h>
#include <scsi/scsi_defs.h>
#include <scsi/rz.h>

#if  (NSCSI > 0)

void sccpu_act_as_target(); /* forwards */
void sccpu_start();

/*
 * This function decides which 'protocol' we well speak
 * to a cpu target. For now the decision is left to a
 * global var. XXXXXXX
 */
extern scsi_devsw_t scsi_host;
scsi_devsw_t	*scsi_cpu_protocol = /* later &scsi_host*/
			&scsi_devsw[SCSI_CPU];

void sccpu_new_initiator(self, initiator)
	target_info_t	*self, *initiator;
{
	initiator->dev_ops = scsi_cpu_protocol;
	if (initiator == self) {
		self->flags = TGT_DID_SYNCH|TGT_FULLY_PROBED|TGT_ONLINE|
			     TGT_ALIVE|TGT_US;
		self->dev_info.cpu.req_pending = FALSE;
	} else {
		initiator->flags = TGT_ONLINE|TGT_ALIVE;
		initiator->dev_info.cpu.req_pending = TRUE;
	}
}

void sccpu_strategy(ior)
	register io_req_t	ior;
{
	void			sccpu_start();

	rz_simpleq_strategy(ior, sccpu_start);
}

void sccpu_start(tgt, done)
	target_info_t	*tgt;
	boolean_t	done;
{
	io_req_t		head, ior;
	scsi_ret_t		ret;

	/* this is to the doc & debug code mentioned in the beginning */
	if (!done && tgt->dev_info.cpu.req_pending) {
		panic("sccpu_act_as_target called");
#if 0
		sccpu_act_as_target( tgt);
#endif
		return;
	}

	ior = tgt->ior;
	if (ior == 0)
		return;

	if (done) {

		/* see if we must retry */
		if ((tgt->done == SCSI_RET_RETRY) &&
		    ((ior->io_op & IO_INTERNAL) == 0)) {
			delay(1000000);/*XXX*/
			goto start;
		} else
		/* got a bus reset ? shouldn't matter */
		if ((tgt->done == (SCSI_RET_ABORTED|SCSI_RET_RETRY)) &&
		    ((ior->io_op & IO_INTERNAL) == 0)) {
			goto start;
		} else

		/* check completion status */

		if (tgt->cur_cmd == SCSI_CMD_REQUEST_SENSE) {
			scsi_sense_data_t *sns;

			ior->io_op = ior->io_temporary;
			ior->io_error = D_IO_ERROR;
			ior->io_op |= IO_ERROR;

			sns = (scsi_sense_data_t *)tgt->cmd_ptr;
			if (scsi_debug)
				scsi_print_sense_data(sns);

			if (scsi_check_sense_data(tgt, sns)) {
			    if (sns->u.xtended.ili) {
				if (ior->io_op & IO_READ) {
				    int residue;

				    residue =	sns->u.xtended.info0 << 24 |
						sns->u.xtended.info1 << 16 |
						sns->u.xtended.info2 <<  8 |
						sns->u.xtended.info3;
				    if (scsi_debug)
					printf("Cpu Short Read (%d)\n", residue);
				    /*
				     * NOTE: residue == requested - actual
				     * We only care if > 0
				     */
				    if (residue < 0) residue = 0;/* sanity */
				    ior->io_residual += residue;
				    ior->io_error = 0;
				    ior->io_op &= ~IO_ERROR;
				    /* goto ok */
				}
			    }
			}
		}

		else if (tgt->done != SCSI_RET_SUCCESS) {

		    if (tgt->done == SCSI_RET_NEED_SENSE) {

			ior->io_temporary = ior->io_op;
			ior->io_op = IO_INTERNAL;
			if (scsi_debug)
				printf("[NeedSns x%x x%x]", ior->io_residual, ior->io_count);
			scsi_request_sense(tgt, ior, 0);
			return;

		    } else if (tgt->done == SCSI_RET_RETRY) {
			/* only retry here READs and WRITEs */
			if ((ior->io_op & IO_INTERNAL) == 0) {
				ior->io_residual = 0;
				goto start;
			} else{
				ior->io_error = D_WOULD_BLOCK;
				ior->io_op |= IO_ERROR;
			}
		    } else {
			ior->io_error = D_IO_ERROR;
			ior->io_op |= IO_ERROR;
		    }
		}

		if (scsi_debug)
			printf("[Resid x%x]", ior->io_residual);

		/* dequeue next one */
		head = ior;

		simple_lock(&tgt->target_lock);
		ior = head->io_next;
		tgt->ior = ior;
		if (ior)
			ior->io_prev = head->io_prev;
		simple_unlock(&tgt->target_lock);

		iodone(head);

		if (ior == 0)
			return;
	}
	ior->io_residual = 0;
start:
	if (ior->io_op & IO_READ) {
		ret = scsi_receive( tgt, ior );
	} else if ((ior->io_op & IO_INTERNAL) == 0) {
		ret = scsi_send( tgt, ior );
	}
}


#if 0
/* XX turned off this code because it's impossible
   to reference 'end' and other such magic symbols
   from boot modules. */
/*
 * This is a simple code to make us act as a dumb
 * processor type.  Use for debugging only.
 */
static struct io_req	sccpu_ior;
vm_offset_t		sccpu_buffer; /* set this with debugger */

void sccpu_act_as_target(self)
	target_info_t	*self;
{
	static char	inq_data[] = "\3\0\1\0\040\0\0\0Mach3.0 Processor Link  v0.1";
	static char	sns_data[] = "\160\0\0\0\0\0\0\0\0";

	self->dev_info.cpu.req_pending = FALSE;
	sccpu_ior.io_next = 0;
#define MAXSIZE 1024*64
	sccpu_ior.io_count = (MAXSIZE < self->dev_info.cpu.req_len) ?
		MAXSIZE : self->dev_info.cpu.req_len;

	switch (self->dev_info.cpu.req_cmd) {
	case SCSI_CMD_INQUIRY:
		sccpu_ior.io_data = inq_data; break;
	case SCSI_CMD_REQUEST_SENSE:
		sccpu_ior.io_data = sns_data; break;
	default:
		if (sccpu_buffer == 0) {
		    /* ( read my lips :-) */
		    /* extern char end[]; */
		    sccpu_buffer = trunc_page(kalloc(MAXSIZE));
		}
		sccpu_ior.io_data = (char*)sccpu_buffer; break;
	}

	if (self->dev_info.cpu.req_cmd == SCSI_CMD_SEND) {
		self->cur_cmd = SCSI_CMD_READ;
		sccpu_ior.io_op = IO_READ;
	} else {
		self->cur_cmd = SCSI_CMD_WRITE;
		sccpu_ior.io_op = IO_WRITE;
	}
	self->ior = &sccpu_ior;
}
#endif

/*#define PERF*/
#ifdef	PERF
int test_read_size = 512;
int test_read_nreads = 1000;
int test_read_bdev = 0;
int test_read_or_write = 1;

#include <sys/time.h>
#include <machine/machspl.h>	/* spl */

test_read(max)
{
	int i, ssk, usecs;
	struct timeval start, stop;
	spl_t s;

	if (max != 0)
		test_read_nreads = max;

	s = spl0();
	start = time;
	if (test_read_or_write) read_test(); else write_test();
	stop = time;
	splx(s);

	usecs = stop.tv_usec - start.tv_usec;
	if (usecs < 0) {
		stop.tv_sec -= 1;
		usecs += 1000000;
	}
	printf("Size %d count %d time %3d sec %d us\n",
			test_read_size, test_read_nreads,
			stop.tv_sec - start.tv_sec, usecs);
}

read_test()
{
	struct io_req	io, io1;
	register int 	i;

	bzero(&io, sizeof(io));
	io.io_unit = test_read_bdev;
	io.io_op = IO_READ;
	io.io_count = test_read_size;
	io.io_data = (char*)sccpu_buffer;
	io1 = io;

	sccpu_strategy(&io);
	for (i = 1; i < test_read_nreads; i += 2) {
		io1.io_op = IO_READ;
		sccpu_strategy(&io1);
		iowait(&io);
		io.io_op = IO_READ;
		sccpu_strategy(&io);
		iowait(&io1);
	}
	iowait(&io);
}

write_test()
{
	struct io_req	io, io1;
	register int 	i;

	bzero(&io, sizeof(io));
	io.io_unit = test_read_bdev;
	io.io_op = IO_WRITE;
	io.io_count = test_read_size;
	io.io_data = (char*)sccpu_buffer;
	io1 = io;

	sccpu_strategy(&io);
	for (i = 1; i < test_read_nreads; i += 2) {
		io1.io_op = IO_WRITE;
		sccpu_strategy(&io1);
		iowait(&io);
		io.io_op = IO_WRITE;
		sccpu_strategy(&io);
		iowait(&io1);
	}
	iowait(&io);
}

tur_test()
{
	struct io_req	io;
	register int 	i;
	char		*a;
	struct timeval start, stop;
	spl_t s;
	target_info_t	*tgt;

	bzero(&io, sizeof(io));
	io.io_unit = test_read_bdev;
	io.io_data = (char*)&io;/*unused but kernel space*/

	rz_check(io.io_unit, &a, &tgt);
	s = spl0();
	start = time;
	for (i = 0; i < test_read_nreads; i++) {
		io.io_op = IO_INTERNAL;
		scsi_test_unit_ready(tgt,&io);
	}
	stop = time;
	splx(s);
	i = stop.tv_usec - start.tv_usec;
	if (i < 0) {
		stop.tv_sec -= 1;
		i += 1000000;
	}
	printf("%d test-unit-ready took %3d sec %d us\n",
			test_read_nreads,
			stop.tv_sec - start.tv_sec, i);
}

/*#define	MEM_PERF*/
#ifdef	MEM_PERF
int mem_read_size = 1024; /* ints! */
int mem_read_nreads = 1000;
volatile int *mem_read_address = (volatile int*)0xb0080000;
volatile int *mem_write_address = (volatile int*)0xb0081000;

mem_test(max, which)
{
	int i, ssk, usecs;
	struct timeval start, stop;
	int (*fun)(), mwrite_test(), mread_test(), mcopy_test();
	spl_t s;

	if (max == 0)
		max = mem_read_nreads;

	switch (which) {
	case 1:	fun = mwrite_test; break;
	case 2:	fun = mcopy_test; break;
	default:fun = mread_test; break;
	}

	s = spl0();
	start = time;
	for (i = 0; i < max; i++)
		(*fun)(mem_read_size);
	stop = time;
	splx(s);

	usecs = stop.tv_usec - start.tv_usec;
	if (usecs < 0) {
		stop.tv_sec -= 1;
		usecs += 1000000;
	}
	printf("Size %d count %d time %3d sec %d us\n",
			mem_read_size*4, max,
			stop.tv_sec - start.tv_sec, usecs);
}

mread_test(max)
	register int max;
{
	register int 	i;
	register volatile int *addr = mem_read_address;

	for (i = 0; i < max; i++) {
		register int j = *addr++;
	}
}
mwrite_test(max)
	register int max;
{
	register int 	i;
	register volatile int *addr = mem_read_address;

	for (i = 0; i < max; i++) {
		*addr++ = i;
	}
}

mcopy_test(max)
	register int max;
{
	register volatile int *from = mem_read_address;
	register volatile int *to = mem_write_address;
	register volatile int *endaddr;

	endaddr = to + max;
	while (to < endaddr)
		*to++ = *from++;

}
#endif	/*MEM_PERF*/

#endif	/*PERF*/

#endif  /* NSCSI > 0 */
