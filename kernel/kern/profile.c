/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University.
 * Copyright (c) 1993,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 * Copyright 1991 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 * 
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 * 
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if 0

#include	<kern/thread.h>
#include	<kern/queue.h>
#include	<mach/profil.h>
#include	<kern/sched_prim.h>
#include	<ipc/ipc_space.h>

extern vm_map_t	kernel_map; /* can be discarded, defined in <vm/vm_kern.h> */

thread_t profile_thread_id = THREAD_NULL;


void profile_thread() 
{
	struct message {
		mach_msg_header_t	head;
		mach_msg_type_t		type;
		int			arg[SIZE_PROF_BUFFER+1];
	} msg;

	register spl_t	s;
	buf_to_send_t	buf_entry;
	queue_entry_t	prof_queue_entry;
	prof_data_t	pbuf;
	simple_lock_t 	lock;
	msg_return_t 	mr;
	int		j;

	/* Initialise the queue header for the prof_queue */
	mpqueue_init(&prof_queue);

	/* Template initialisation of header and type structures */
	msg.head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
	msg.head.msgh_size = sizeof(msg); 
	msg.head.msgh_local_port = MACH_PORT_NULL;
	msg.head.msgh_kind = MACH_MSGH_KIND_NORMAL;
	msg.head.msgh_id = 666666;
	
	msg.type.msgt_name = MACH_MSG_TYPE_INTEGER_32;
	msg.type.msgt_size = 32;
	msg.type.msgt_number = SIZE_PROF_BUFFER+1;
	msg.type.msgt_inline = TRUE;
	msg.type.msgt_longform = FALSE;
	msg.type.msgt_deallocate = FALSE;
	msg.type.msgt_unused = 0;

	while (TRUE) {

	   /* Dequeue the first buffer. */
	   s = splsched();
	   mpdequeue_head(&prof_queue, &prof_queue_entry);
	   splx(s);

	   if ((buf_entry = (buf_to_send_t) prof_queue_entry) == NULLBTS)
                { 
		thread_sleep((event_t) profile_thread, lock, TRUE);
		if (current_thread()->wait_result != THREAD_AWAKENED)
			break;
                }
	   else {
		task_t		curr_task;
                thread_t	curr_th;
		register int 	*sample;
                int 		curr_buf;
		int 		imax;

                curr_th = (thread_t) buf_entry->thread;
                curr_buf = (int) buf_entry->number; 
		pbuf = curr_th->profil_buffer;

		/* Set the remote port */
		msg.head.msgh_remote_port = (mach_port_t) pbuf->prof_port;

                 
                sample = pbuf->prof_area[curr_buf].p_zone;
	        imax = pbuf->prof_area[curr_buf].p_index;
	        for(j=0 ;j<imax; j++,sample++)
		msg.arg[j] = *sample;	

	        /* Let hardclock() know you've finished the dirty job */
	        pbuf->prof_area[curr_buf].p_full = FALSE;

	        /*
		 * Store the number of samples actually sent 
	         * as the last element of the array.
		 */
	        msg.arg[SIZE_PROF_BUFFER] = imax;

	        mr = mach_msg(&(msg.head), MACH_SEND_MSG, 
			            sizeof(struct message), 0, 
				    MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, 
				    MACH_PORT_NULL);

		if (mr != MACH_MSG_SUCCESS) {
printf("profile_thread: mach_msg failed returned %x\n",(int)mr);
		}

		if (buf_entry->wakeme)
			thread_wakeup((event_t) &buf_entry->wakeme);
		kmem_free(kernel_map, (buf_to_send_t) buf_entry,
					sizeof(struct buf_to_send));

            }

        }
	/* The profile thread has been signalled to exit.  There may still
	   be sample data queued for us, which we must now throw away.
	   Once we set profile_thread_id to null, hardclock() will stop
	   queueing any additional samples, so we do not need to alter
	   the interrupt level.  */
	profile_thread_id = THREAD_NULL;
	while (1) {
		mpdequeue_head(&prof_queue, &prof_queue_entry);
		if ((buf_entry = (buf_to_send_t) prof_queue_entry) == NULLBTS)
			break;
		if (buf_entry->wakeme)
			thread_wakeup((event_t) &buf_entry->wakeme);
		kmem_free(kernel_map, (buf_to_send_t) buf_entry,
					sizeof(struct buf_to_send));
	}

	thread_halt_self();
}



#include <mach/message.h>

void
send_last_sample_buf(th)
thread_t th;
{
        register	spl_t s;
        buf_to_send_t buf_entry;
        vm_offset_t vm_buf_entry;

	if (th->profil_buffer == NULLPBUF)
		return;

	/* Ask for the sending of the last PC buffer.
	 * Make a request to the profile_thread by inserting
	 * the buffer in the send queue, and wake it up. 
	 * The last buffer must be inserted at the head of the
	 * send queue, so the profile_thread handles it immediatly. 
	 */ 
	if (kmem_alloc( kernel_map, &vm_buf_entry,
		   sizeof(struct buf_to_send)) != KERN_SUCCESS)
		return;
	buf_entry = (buf_to_send_t) vm_buf_entry;
	buf_entry->thread = (int *) th;
	buf_entry->number = th->profil_buffer->prof_index;

	/* Watch out in case profile thread exits while we are about to
	   queue data for it.  */
	s = splsched();
	if (profile_thread_id != THREAD_NULL) {
		simple_lock_t lock;
		buf_entry->wakeme = 1;
		mpenqueue_tail( &prof_queue, &(buf_entry->list));
		thread_wakeup((event_t) profile_thread);
		assert_wait((event_t) &buf_entry->wakeme, TRUE);
		splx(s); 
		thread_block((void (*)()) 0);
	} else {
		splx(s);
		kmem_free(kernel_map, vm_buf_entry, sizeof(struct buf_to_send));
	}
}

/*
 * Profile current thread
 */

profile(pc) {

	/* Find out which thread has been interrupted. */
	thread_t it_thread = current_thread();
	int inout_val = pc; 
	buf_to_send_t	buf_entry;
	vm_offset_t 	vm_buf_entry;
	int		*val;
	/*
	 * Test if the current thread is to be sampled 
	 */
	if (it_thread->thread_profiled) {
		/* Inserts the PC value in the buffer of the thread */
		set_pbuf_value(it_thread->profil_buffer, &inout_val); 
		switch(inout_val) {
		case 0: 
			if (profile_thread_id == THREAD_NULL) {
				reset_pbuf_area(it_thread->profil_buffer);
			} else printf("ERROR : hardclock : full buffer unsent\n");
			break;
		case 1: 
			/* Normal case, value successfully inserted */
			break;
		case 2 : 
			/*
			 * The value we have just inserted caused the
			 * buffer to be full, and ready to be sent.
			 * If profile_thread_id is null, the profile
			 * thread has been killed.  Since this generally
			 * happens only when the O/S server task of which
			 * it is a part is killed, it is not a great loss
			 * to throw away the data.
			 */
			if (profile_thread_id == THREAD_NULL ||
				kmem_alloc(kernel_map,
					   &vm_buf_entry ,
					   sizeof(struct buf_to_send)) !=
				KERN_SUCCESS) {
				reset_pbuf_area(it_thread->profil_buffer);
				break;
			}
			buf_entry = (buf_to_send_t) vm_buf_entry;
			buf_entry->thread = (int *)it_thread;
			buf_entry->number =
				(it_thread->profil_buffer)->prof_index;
			mpenqueue_tail(&prof_queue, &(buf_entry->list));

			/* Switch to another buffer */
			reset_pbuf_area(it_thread->profil_buffer);

			/* Wake up the profile thread */
			if (profile_thread_id != THREAD_NULL)
				thread_wakeup((event_t) profile_thread);
			break;

		default: 
			printf("ERROR: profile : unexpected case\n"); 
		}
	}
}


/* The task parameter in this and the subsequent routine is needed for
   MiG, even though it is not used in the function itself. */

kern_return_t
mach_sample_thread (task, reply, cur_thread)
ipc_space_t	task;
ipc_object_t 	reply;
thread_t	cur_thread;
{
/* 
 * This routine is called every time that a new thread has made
 * a request for the sampling service. We must keep track of the 
 * correspondance between it's identity (cur_thread) and the port
 * we are going to use as a reply port to send out the samples resulting 
 * from its execution. 
 */
	prof_data_t	pbuf;
	vm_offset_t     vmpbuf;

	if (reply != MACH_PORT_NULL) {
		if (cur_thread->thread_profiled && cur_thread->thread_profiled_own) {
			if (reply == cur_thread->profil_buffer->prof_port)
				return KERN_SUCCESS;
			mach_sample_thread(MACH_PORT_NULL, cur_thread);
		}
		/* Start profiling this thread , do the initialization. */
		alloc_pbuf_area(pbuf, vmpbuf);
		if ((cur_thread->profil_buffer = pbuf) == NULLPBUF) {
printf("ERROR:mach_sample_thread:cannot allocate pbuf\n");
			return KERN_RESOURCE_SHORTAGE;
		} else {
			if (!set_pbuf_nb(pbuf, NB_PROF_BUFFER-1)) {
printf("ERROR:mach_sample_thread:cannot set pbuf_nb\n");
				return KERN_FAILURE;
			}
			reset_pbuf_area(pbuf);
		}

		pbuf->prof_port = reply;
		cur_thread->thread_profiled = TRUE;
		cur_thread->thread_profiled_own     = TRUE;
		if (profile_thread_id == THREAD_NULL)
			profile_thread_id = kernel_thread(current_task(), profile_thread);
	} else {
		if (!cur_thread->thread_profiled_own)
			cur_thread->thread_profiled = FALSE;
		if (!cur_thread->thread_profiled)
			return KERN_SUCCESS;

		send_last_sample_buf(cur_thread);

		/* Stop profiling this thread, do the cleanup. */

		cur_thread->thread_profiled_own     = FALSE;
		cur_thread->thread_profiled = FALSE;
		dealloc_pbuf_area(cur_thread->profil_buffer); 
		cur_thread->profil_buffer = NULLPBUF;
	}

	return KERN_SUCCESS;
}

kern_return_t
mach_sample_task (task, reply, cur_task)
ipc_space_t	task;
ipc_object_t 	reply;
task_t		cur_task;
{
	prof_data_t	pbuf=cur_task->profil_buffer;
	vm_offset_t     vmpbuf;
	int		turnon = (reply != MACH_PORT_NULL);

	if (turnon) {
		if (cur_task->task_profiled) {
			if (cur_task->profil_buffer->prof_port == reply)
				return KERN_SUCCESS;
			(void) mach_sample_task(task, MACH_PORT_NULL, cur_task);
		}
		if (pbuf == NULLPBUF) {
			alloc_pbuf_area(pbuf, vmpbuf);
			if (pbuf == NULLPBUF) {
				return KERN_RESOURCE_SHORTAGE;
			}
			cur_task->profil_buffer = pbuf;
		}
		if (!set_pbuf_nb(pbuf, NB_PROF_BUFFER-1)) {
			return KERN_FAILURE;
		}
		reset_pbuf_area(pbuf);
		pbuf->prof_port = reply;
	}

	if (turnon != cur_task->task_profiled) {
		int actual,i,sentone;
		thread_t thread;

		if (turnon && profile_thread_id == THREAD_NULL)
			profile_thread_id =
				kernel_thread(current_task(), profile_thread);
		cur_task->task_profiled = turnon;  
		actual = cur_task->thread_count; 
		sentone = 0;
		for (i=0, thread=(thread_t) queue_first(&cur_task->thread_list);
		     i < actual;
		     i++, thread=(thread_t) queue_next(&thread->thread_list)) {
			if (!thread->thread_profiled_own) {
				thread->thread_profiled = turnon;
				if (turnon)
					thread->profil_buffer = cur_task->profil_buffer;
				else if (!sentone) {
					send_last_sample_buf(thread);
					sentone = 1;
				}
			}
		}
		if (!turnon) {
			dealloc_pbuf_area(pbuf); 
			cur_task->profil_buffer = NULLPBUF;
		}
	}

	return KERN_SUCCESS;
}

#endif 0
