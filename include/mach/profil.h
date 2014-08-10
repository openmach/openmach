/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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


#ifndef _MACH_PROFIL_H_
#define _MACH_PROFIL_H_

#include <mach/boolean.h>
#include <ipc/ipc_object.h>
#include <vm/vm_kern.h> 


#define	NB_PROF_BUFFER		2	/* number of buffers servicing a 
					 * profiled thread */
#define	SIZE_PROF_BUFFER	100	/* size of a profil buffer (in int) 
					 * This values is also defined in
					 * the server (ugly), be careful ! */


struct	prof_data {
	ipc_object_t	prof_port;	/* where to send a full buffer */

	struct buffer {
	    int	*p_zone;		/* points to the actual storage area */
	    int			p_index;/* next slot to be filled */
	    boolean_t		p_full;	/* is the current buffer full ? */ 
	} prof_area[NB_PROF_BUFFER];

	int		prof_index;	/* index of the buffer structure
					 *   currently in use */

};
typedef struct prof_data	*prof_data_t;
#define NULLPBUF ((prof_data_t) 0)
typedef struct buffer		*buffer_t;

/* Macros */

#define	set_pbuf_nb(pbuf, nb) \
         (((nb) >= 0 && (nb) < NB_PROF_BUFFER) \
	 ? (pbuf)->prof_index = (nb), 1 \
	 : 0)


#define	get_pbuf_nb(pbuf) \
	(pbuf)->prof_index


extern vm_map_t kernel_map; 

#define dealloc_pbuf_area(pbuf) \
          { \
	  register int i; \
				   \
	    for(i=0; i < NB_PROF_BUFFER ; i++)  \
	      kmem_free(kernel_map, \
                        (vm_offset_t) (pbuf)->prof_area[i].p_zone, \
                        SIZE_PROF_BUFFER*sizeof(int)); \
            kmem_free(kernel_map, \
                          (vm_offset_t)(pbuf), \
                          sizeof(struct prof_data)); \
          }
	

#define alloc_pbuf_area(pbuf, vmpbuf) \
      (vmpbuf) = (vm_offset_t) 0; \
      if (kmem_alloc(kernel_map, &(vmpbuf) , sizeof(struct prof_data)) == \
                                           KERN_SUCCESS) { \
	   register int i; \
	   register boolean_t end; \
				   \
	   (pbuf) = (prof_data_t) (vmpbuf); \
	   for(i=0, end=FALSE; i < NB_PROF_BUFFER && end == FALSE; i++) { \
              (vmpbuf) = (vm_offset_t) 0; \
	      if (kmem_alloc(kernel_map,&(vmpbuf),SIZE_PROF_BUFFER*sizeof(int)) == KERN_SUCCESS) { \
		 (pbuf)->prof_area[i].p_zone = (int *) (vmpbuf); \
		 (pbuf)->prof_area[i].p_full = FALSE; \
	      } \
	      else { \
	         (pbuf) = NULLPBUF; \
		 end = TRUE; \
	      } \
       	    } \
	} \
	else \
	  (pbuf) = NULLPBUF; 
	


/* MACRO set_pbuf_value 
** 
** enters the value 'val' in the buffer 'pbuf' and returns the following
** indications:     0: means that a fatal error occured: the buffer was full
**                       (it hasn't been sent yet)
**                  1: means that a value has been inserted successfully
**		    2: means that we'v just entered the last value causing 
**			the current buffer to be full.(must switch to 
** 			another buffer and signal the sender to send it)
*/ 
	  
#define set_pbuf_value(pbuf, val) \
	 { \
	  register buffer_t a = &((pbuf)->prof_area[(pbuf)->prof_index]); \
	  register int i = a->p_index++; \
	  register boolean_t f = a->p_full; \
			  \
	  if (f == TRUE ) \
             *(val) = 0; \
	  else { \
	    a->p_zone[i] = *(val); \
	    if (i == SIZE_PROF_BUFFER-1) { \
               a->p_full = TRUE; \
               *(val) = 2; \
            } \
            else \
		*(val) = 1; \
          } \
	}

         
#define	reset_pbuf_area(pbuf) \
	{ \
	 register int *i = &((pbuf)->prof_index); \
					      \
	 *i = (*i == NB_PROF_BUFFER-1) ? 0 : ++(*i); \
	 (pbuf)->prof_area[*i].p_index = 0; \
	}


/**************************************************************/
/* Structure, elements used for queuing operations on buffers */
/**************************************************************/

#define	thread_t int *
/*
** This must be done in order to avoid a circular inclusion 
** with file kern/thread.h . 
** When using this data structure, one must cast the actual 
** type, this is (int *) or (thread_t)
*/

struct buf_to_send {
   	queue_chain_t list;
	thread_t thread;
        int number;         /* the number of the buffer to be sent */
	char wakeme;	    /* do wakeup when buffer has been sent */
        }	;

#undef	thread_t



typedef struct buf_to_send *buf_to_send_t;

#define	NULLBTS		((buf_to_send_t) 0)

/*
** Global variable: the head of the queue of buffers to send 
** It is a queue with locks (uses macros from queue.h) and it
** is shared by hardclock() and the sender_thread() 
*/

mpqueue_head_t prof_queue; 

#endif	/* _MACH_PROF_H_ */
