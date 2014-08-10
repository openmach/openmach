/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
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

#ifndef _NW_H_
#define _NW_H_ 1

#if defined(KMODE) || defined(KERNEL)
#include <sys/types.h> 
#include <mach/port.h> 
#else
#include "stub0.h"
#endif

/*** NETWORK APPLICATION PROGRAMMING INTERFACE ***/

/*** TYPES ***/

typedef enum {
  NW_SUCCESS,
  NW_FAILURE,
  NW_BAD_ADDRESS,
  NW_OVERRUN,
  NW_NO_CARRIER,
  NW_NOT_SERVER,
  NW_NO_EP,
  NW_BAD_EP,
  NW_INVALID_ARGUMENT,
  NW_NO_RESOURCES,
  NW_PROT_VIOLATION,
  NW_BAD_BUFFER,
  NW_BAD_LENGTH,
  NW_NO_REMOTE_EP,
  NW_TIME_OUT,
  NW_INCONSISTENCY,
  NW_ABORTED,
  NW_SYNCH,
  NW_QUEUED
} nw_result, *nw_result_t;

typedef enum {
  NW_INITIALIZE,
  NW_HOST_ADDRESS_REGISTER,
  NW_HOST_ADDRESS_UNREGISTER
} nw_update_type;

typedef enum {
  NW_STATUS,
  NW_HOST_ADDRESS_LOOKUP
} nw_lookup_type;

typedef u_int ip_address;

typedef u_int nw_address_1;
typedef u_int nw_address_2;

typedef struct {
  char name[20];          /*Host name -- first 19 characters, zero-terminated*/
  ip_address ip_addr;
  nw_address_1 nw_addr_1; /*4 most significant bits specify the device*/
  nw_address_2 nw_addr_2;
} nw_address_s, *nw_address_t;

typedef enum {
  NW_NULL,
  NW_IP,                 /*Virtual network for IP addresses*/
  NW_TCA100_1,           /*Fore Systems ATM network, first unit*/
  NW_TCA100_2            /*Fore Systems ATM network, second unit*/
} nw_device, *nw_device_t;

#define NW_DEVICE(addr) (addr >> 28)

typedef u_int nw_ep, *nw_ep_t;

typedef enum {
  NW_RAW,                      /*Raw service provided by network*/
  NW_DATAGRAM,                 /*Connectionless service*/
  NW_SEQ_PACKET,               /*Connection-oriented service*/
  NW_LINE                      /*Multiplexing line (system use only)*/
} nw_protocol;

typedef enum {
  NW_NO_ACCEPT,                /*Connection requests not accepted (client)*/
  NW_APPL_ACCEPT,              /*Connection requests received as message by
				 application (msg_seqno 0), for examination
                                 and approval (nw_connection_accept function)*/
  NW_AUTO_ACCEPT,              /*Connection requests automatically accepted
 				 if endpoint is connection-oriented and
				 not already connected*/
  NW_LINE_ACCEPT               /*Connection requests automatically accepted
				 on a new endpoint (system use only)*/
} nw_acceptance;

typedef struct {
  nw_address_1 rem_addr_1;
  nw_address_2 rem_addr_2;
  nw_ep remote_ep:16;
  nw_ep local_ep:16;
} nw_peer_s, *nw_peer_t;

typedef struct nw_buffer {
  u_int buf_used:1;               /*Read-only for applications (always 1)*/
  u_int buf_length:31;            /*Read-only for applications*/
  struct nw_buffer *buf_next;     /*Used only to gather on sends*/
  u_int msg_seqno:10;             /*Sequential number of message,
				    automatically set by network interface*/
  u_int block_offset:22;          /*Offset to the beginning of data (in bytes),
				    from the start of the buffer*/
  u_int block_deallocate:1;       /*Used only to deallocate on sends*/
  u_int block_length:31;          /*Length of data (in bytes)
				    beginning at offset*/
  nw_peer_s peer;                 /*Set on receives. Also required
				    in first block on datagram sends.
				    Ignored on sequenced packet sends.*/
} nw_buffer_s, *nw_buffer_t;


/* msg_seqno is normally between 1 and 1023, and increases modulo 1024
   (skipping 0) between consecutive messages. In sequenced packets, msg_seqno
   increases strictly by one. msg_seqno is assigned automatically.
   The network interface writes in the buffer the msg_seqno used,
   but only after the  buffer has been transmitted and, in case of 
   sequenced packet, acknowledged. The application can use this update
   to determine if a buffer can be reused, after a sending a message without
   the deallocate option.
   msg_seqno 0 is used when the corresponding send specifies the NW_URGENT 
   option. Such messages bypass any other messages possibly enqueued.
   msg_seqno 0 is also used for open connection requests, in the case
   of sequenced packet endpoints with the NW_APPL_ACCEPT option.
   The type of msg_seqno 0 message is differentiated by the first word in the
   message, which has type nw_options */

#define NW_BUFFER_ERROR ((nw_buffer_t) -1)      /*Used for error indication
						  other than buffer overrun
						  (for which NULL is used)*/

typedef enum {
  NW_INEXISTENT,
  NW_UNCONNECTED,
  NW_SIMPLEX_ORIGINATING,
  NW_SIMPLEX_ORIGINATED,
  NW_DUPLEX_ORIGINATING,
  NW_DUPLEX_ORIGINATING_2,
  NW_DUPLEX_ORIGINATED,
  NW_ORIGINATOR_CLOSING,
  NW_ORIGINATOR_RCLOSING,
  NW_ACCEPTING,
  NW_SIMPLEX_ACCEPTED,
  NW_DUPLEX_ACCEPTING,
  NW_DUPLEX_ACCEPTED,
  NW_ACCEPTOR_CLOSING,
  NW_ACCEPTOR_RCLOSING
} nw_state, *nw_state_t;


typedef enum nw_options {
  NW_NORMAL,
  NW_URGENT,
  NW_SYNCHRONIZATION                              /*System use only*/
} nw_options;


/*** FUNCTIONS ***/

extern nw_result nw_update(mach_port_t master_port, nw_update_type up_type,
			   int *up_info);

/*****************************************************************************
     Allows privileged applications to update network tables. The 
     application must present the device master port. up_type selects the
     type of update, and up_info is cast accordingly to the correct type.

     For NW_HOST_ADDRESS_REGISTER and NW_HOST_ADDRESS_UNREGISTER,
     up_info has type nw_address_t. For NW_HOST_ADDRESS_UNREGISTER,
     however, only the network address field is used.

     up_info is not used for NW_INITIALIZE. This option is used to 
     initialize network interface tables, but does not initialize
     devices. Initialization of hardware and network tables occurs
     automatically at probe/boot time, so this option is normally
     unnecessary.

     Returns NW_SUCCESS if operation completed successfully.
	     NW_FAILURE if master port not presented.
	     NW_NO_RESOURCES if network tables full (NW_HOST_ADDRESS_REGISTER).
	     NW_BAD_ADDRESS if host not found (NW_HOST_ADDRESS_UNREGISTER).
	     NW_INVALID_ARGUMENT if up_type is invalid or up_info is
	                         a bad pointer.
 *****************************************************************************/


extern nw_result nw_lookup(nw_lookup_type lt, int *look_info);

/*****************************************************************************
     Allows applications to lookup network tables. The type of 
     lookup is selected by lt, and look_info is cast to the correct type
     accordingly.

     For lt equal to NW_HOST_ADDRESS_LOOKUP, look_info has type
     nw_address_t. In this option, the host is looked up first using the
     IP address  as a key (if non-zero), then by name (if non-empty), 
     and finally by network address (if non-zero). The function
     returns NW_SUCCESS on the first match it finds, and sets the non-key
     fields of look_info to the values found. No consistency check is
     made if more than one key is supplied. The function returns
     NW_BAD_ADDRESS if the host was not found, and NW_INVALID_ARGUMENT
     if lt is invalid or look_info is a bad pointer.

     For lt equal to NW_STATUS, look_info has type nw_device_t on input
     and nw_result_t on output. The function returns NW_INVALID_ARGUMENT
     if the device chosen is invalid or look_info is a bad pointer;
     otherwise, the function returns NW_SUCCESS. look_info is
     set to: NW_FAILURE if the device is not present, NW_NOT_SERVER
     if the device is not serviced by this interface, or a
     device-dependent value otherwise (NW_SUCCESS if there is no device error).

 *****************************************************************************/


extern nw_result nw_endpoint_allocate(nw_ep_t epp, nw_protocol protocol,
				      nw_acceptance accept, u_int buffer_size);

/*****************************************************************************
     Allocates a communication endpoint. On input, epp should point to the
     the endpoint number desired, or to 0 if any number is adequate.
     On output, epp points to the actual number allocated for the endpoint.
     protocol specifies the transport discipline applied to data transmitted
     or received through the endpoint. accept selects how open connection
     requests received for the endpoint should be handled (connection-oriented
     protocol), or whether the endpoint can receive messages (connectionless
     protocol). buffer_size specifies the length in bytes of the buffer area
     for data sent or received through the endpoint. 

     Returns NW_SUCCESS if endpoint successfully allocated.
             NW_INVALID_ARGUMENT if epp is a bad pointer or the
	                         protocol or accept arguments are invalid.
             NW_NO_EP if the endpoint name space is exhausted.
             NW_BAD_EP if there already is an endpoint with the
	               number selected, or the number selected is
		       out of bounds.
	     NW_NO_RESOURCES if not enough memory for buffer.
 *****************************************************************************/
            

extern nw_result nw_endpoint_deallocate(nw_ep ep);

/*****************************************************************************
      Deallocates the given endpoint.

      Returns NW_SUCCESS if successfully deallocated endpoint.
              NW_BAD_EP if endpoint does not exist.
              NW_PROT_VIOLATION if access to endpoint not authorized.
 *****************************************************************************/


extern nw_buffer_t nw_buffer_allocate(nw_ep ep, u_int size);

/*****************************************************************************
      Allocates a buffer of the given size (in bytes) from the buffer area
      of the given endpoint.

      Returns NW_BUFFER_ERROR if endpoint does not exist or access to endpoint
                              is not authorized.
              NULL if no buffer with given size could be allocated.
	      Pointer to allocated buffer, otherwise.
 *****************************************************************************/


extern nw_result nw_buffer_deallocate(nw_ep ep, nw_buffer_t buffer);

/*****************************************************************************
      Deallocates the given buffer.

      Returns NW_SUCCESS if successfully deallocated buffer.
              NW_BAD_EP if endpoint does not exist.
              NW_PROT_VIOLATION if access to endpoint not authorized.
	      NW_BAD_BUFFER if buffer does not belong to endpoint's
	                    buffer area or is malformed.
 *****************************************************************************/


extern nw_result nw_connection_open(nw_ep local_ep, nw_address_1 rem_addr_1,
				    nw_address_2 rem_addr_2, nw_ep remote_ep);

/*****************************************************************************
      Opens a connection.

      Returns NW_SUCCESS if connection successfully opened.
              NW_BAD_EP if local endpoint does not exist, uses connectionless
	                protocol or is already connected.
              NW_PROT_VIOLATION if access to local or remote endpoint
	                        not authorized.
	      NW_BAD_ADDRESS if address of remote host is invalid.
	      NW_NO_REMOTE_EP if connection name space exhausted at
	                      remote host.
	      NW_TIME_OUT if attempt to open connection timed out repeatedly.
	      NW_FAILURE if remote endpoint does not exist, uses connectionless
	                 protocol or is already connected, or if remote
                         application did not accept open request.
 *****************************************************************************/


extern nw_result nw_connection_accept(nw_ep ep, nw_buffer_t msg,
				      nw_ep_t new_epp);

/*****************************************************************************
      Accepts open request (at the remote host). On input, new_epp equal to
      NULL indicates that the application does not accept the request.
      new_epp pointing to the value 0 indicates that the application wants
      to accept the connection on a new endpoint, created dynamically,
      with the same attributes as the original endpoint; new_epp pointing
      to the value ep indicates that the application wants to simply
      accept the open request. On output, new_epp points to the endpoint
      actually connected, if any. msg points to the open request, which is
      automatically deallocated. 

      Returns NW_SUCCESS if connection correctly accepted or refused.
              NW_BAD_EP if endpoint does not exist or has no outstanding
                         open request.
	      NW_PROT_VIOLATION if access to endpoint not authorized.
	      NW_BAD_BUFFER if msg does not belong to the endpoint's
	                    buffer area, or is malformed.
	      NW_INVALID_ARGUMENT if new_epp is a bad pointer or points to
	                          invalid value.
	      NW_NO_EP if endpoint name space exhausted.
	      NW_NO_RESOURCES if no buffer available for new endpoint.
	      NW_TIME_OUT if attempt to accept at different endpoint
                           repeatedly timed out.
 *****************************************************************************/


extern nw_result nw_connection_close(nw_ep ep);

/*****************************************************************************
      Closes the endpoint's connection.

      Returns NW_SUCCESS if successfully closed connection.
              NW_BAD_EP if endpoint does not exist or is not connected.
              NW_PROT_VIOLATION if access to endpoint not authorized.
 *****************************************************************************/


extern nw_result nw_multicast_add(nw_ep local_ep, nw_address_1 rem_addr_1,
				  nw_address_2 rem_addr_2, nw_ep remote_ep);

/*****************************************************************************
      Open multicast group or add one more member to multicast group.

      Returns NW_SUCCESS if successfully opened multicast group
                         or added member.
              NW_BAD_EP if local endpoint does not exist, uses connectionless
	                protocol or is already connected point-to-point.
              NW_PROT_VIOLATION if access to local or remote endpoint
	                        not authorized.
	      NW_BAD_ADDRESS if address of remote host is invalid.
	      NW_NO_REMOTE_EP if connection name space exhausted at
	                      remote host.
	      NW_TIME_OUT if attempt to open or add to multicast
	                  timed out repeatedly.
	      NW_FAILURE if remote endpoint does not exist, uses connectionless
	                 protocol or is already connected, or if remote
                         application did not accept open or add request.
 *****************************************************************************/


extern nw_result nw_multicast_drop(nw_ep local_ep, nw_address_1 rem_addr_1,
				   nw_address_2 rem_addr_2, nw_ep remote_ep);

/*****************************************************************************
      Drop member from multicast group, or close group if last member.

      Returns NW_SUCCESS if successfully dropped member or closed group.
              NW_BAD_EP if local endpoint does not exist or is not connected in
	                multicast to the given remote endpoint.
              NW_PROT_VIOLATION if access to local endpoint not authorized.
 *****************************************************************************/


extern nw_result nw_endpoint_status(nw_ep ep, nw_state_t state,
				    nw_peer_t peer);

/*****************************************************************************
      Returns the state of the given endpoint and peer to which it is 
      connected, if any. In case of multicast group, the first peer is
      returned.

      Returns NW_SUCCESS if status correctly returned.
              NW_BAD_EP if endpoint does not exist.
              NW_PROT_VIOLATION if access to endpoint not authorized.
	      NW_INVALID_ARGUMENT if state or peer is a bad pointer.
 *****************************************************************************/


extern nw_result nw_send(nw_ep ep, nw_buffer_t msg, nw_options options);

/*****************************************************************************
      Sends message through endpoint with the given options.
      
      Returns NW_SUCCESS if message successfully queued for sending
                         (connectionless protocol) or sent and acknowledged
			 (connection-oriented protocol).
              NW_BAD_EP if endpoint does not exist or uses connection-oriented
	                protocol but is unconnected.
              NW_PROT_VIOLATION if access to endpoint not authorized.
	      NW_BAD_BUFFER if msg (or one of the buffers linked by buf_next)
	                    is not a buffer in the endpoint's buffer area, or
                            is malformed (e.g., block_length extends beyond
                            end of buffer).
	      NW_NO_RESOURCES if unable to queue message due to resource
	                      exhaustion.
	      NW_BAD_LENGTH if the total message length is too long for the
                            network and protocol used.
	      NW_BAD_ADDRESS if address of remote host is invalid
	                     (connectionless protocol).
	      NW_FAILURE if repeated errors in message transmission
	                 (connection-oriented).
	      NW_TIME_OUT if repeated time-outs in message transmission
	                  (connection-oriented).
	      NW_OVERRUN if no buffer available in receiver's buffer area.
                         (connection-oriented).
 *****************************************************************************/


extern nw_buffer_t nw_receive(nw_ep ep, int time_out);

/*****************************************************************************
      Receive message destined to endpoint. Return if request  not
      satisfied within time_out msec. time_out 0 means non-blocking receive,
      while -1 means block indefinitely.

      Returns NW_BUFFER_ERROR if endpoint does not exist or access
                              to endpoint is not authorized.
	      NULL if no message available for reception within the
	           specified time-out period.
	      Pointer to message, otherwise.
 *****************************************************************************/


extern nw_buffer_t nw_rpc(nw_ep ep, nw_buffer_t send_msg, nw_options options,
			  int time_out);

/*****************************************************************************
      Send message through given endpoint with given options and then
      receive message through the same endpoint. Receive waiting time
      is limited to time_out msec.

      Returns NW_BUFFER_ERROR if endpoint does not exist, access to
                              endpoint is not authorized, or there was
			      some transmission error.
	      NULL if no message available for reception within the
	           specified time-out period.
	      Pointer to message received, otherwise.
 *****************************************************************************/


extern nw_buffer_t nw_select(u_int nep, nw_ep_t epp, int time_out);

/*****************************************************************************
      Receive message from one of the nep endpoints in the array epp.
      Waiting time is limited to time_out msec.

      Returns NW_BUFFER_ERROR if epp does not point to a valid array of nep
                              endpoint numbers, one of the endpoints does
                              not exist or has restricted access or the request
			      could not be correctly queued because of resource
			      exhaustion.
	      NULL if no message arrived within the specified time-out period.
	      Pointer to message received, otherwise.
 *****************************************************************************/


#endif /* _NW_H_ */
