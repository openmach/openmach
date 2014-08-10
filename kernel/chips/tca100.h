/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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

#ifndef _TCA100_H_
#define _TCA100_H_ 1

#ifndef STUB 
#include <chips/nw.h>
#else
#include "nw.h"
#endif

/*** FORE TCA-100 Turbochannel ATM computer interface ***/

/*** HARDWARE REGISTERS ***/

typedef volatile unsigned int vol_u_int;

typedef struct atm_device {
  unsigned int prom[64 * 1024 / 4];
  vol_u_int sreg;
  vol_u_int creg_set;
  vol_u_int creg_clr;
  vol_u_int creg;
  vol_u_int rxtimer;
  unsigned int pad1;
  vol_u_int rxtimerv;
  unsigned int pad2;
  vol_u_int rxcount;
  unsigned int pad3;
  vol_u_int rxthresh;
  unsigned int pad4;
  vol_u_int txcount;
  unsigned int pad5;
  vol_u_int txthresh;
  unsigned int pad6[64*1024/4 - 15];
  vol_u_int rxfifo[14];
  unsigned int pad7[64*1024/4 - 14];
  vol_u_int txfifo[14];
  unsigned int pad8[64*1024/4 - 14];
} atm_device_s, *atm_device_t;


/*** DEFINITION OF BITS IN THE STATUS AND CONTROL REGISTERS ***/

#define RX_COUNT_INTR 	0x0001
#define RX_EOM_INTR 	0x0002
#define RX_TIME_INTR 	0x0004
#define TX_COUNT_INTR 	0x0008
#define RX_CELL_LOST 	0x0010
#define RX_NO_CARRIER 	0x0020
#define CR_RX_ENABLE 	0x0040
#define CR_TX_ENABLE 	0x0080
#define CR_RX_RESET 	0x0100
#define CR_TX_RESET 	0x0200

#define RX_COUNTER_MASK 0x03ff

/*** DEFINITION OF FIELDS FOR AAL3/4 WITH THE TCA-100 PADDING ***/

/*Header -- ATM header*/

#define VPI 0x0ff00000
#define VCI 0x000ffff0

#define ATM_HEADER_RSV_BITS 0x00000004

#define PERMANENT_VIRTUAL_CONNECTIONS 1

#if PERMANENT_VIRTUAL_CONNECTIONS
#define ATM_VPVC_MASK 0x3ff00000
#define ATM_VPVC_SHIFT 20
#else
#define ATM_VPVC_MASK 0x00003ff0
#define ATM_VPVC_SHIFT 4
#endif


/*First payload word -- SAR header*/

#define ATM_HEADER_CRC 0xff000000
#define ATM_HEADER_CRC_SYNDROME 0x00ff0000

#define SEG_TYPE 0x0000c000
#define BOM 0x00008000
#define COM 0x00000000
#define EOM 0x00004000
#define SSM 0x0000c000

#define BOM_DATA_SIZE 40
#define COM_DATA_SIZE 44
#define EOM_DATA_SIZE 40
#define SSM_DATA_SIZE 36

#define SEQ_NO 0x00003c00
#define SEQ_INC 0x00000400

#define MID 0x000003ff
#define MID_INC 0x00000001

#define SAR_HEADER_MASK (ATM_HEADER_CRC_SYNDROME | SEG_TYPE | SEQ_NO | MID)

/*Trailer -- SAR trailer and error flags*/

#define PAYLOAD_LENGTH 0xfc000000
#define FULL_SEGMENT_TRAILER (44 << 26)
#define EMPTY_SEGMENT_TRAILER (4 << 26)
#define SYNCH_SEGMENT_TRAILER (16 << 26)

#define FRAMING_ERROR 0x0001
#define HEADER_CRC_ERROR 0x0002
#define PAYLOAD_CRC_ERROR 0x0004
#define PAD2_ERROR 0x0007

#define SAR_TRAILER_MASK (PAYLOAD_LENGTH | PAD2_ERROR)
                    /*This field should be FULL_SEGMENT_TRAILER IN BOM OR COM*/


/*CS header and trailer fields*/

#define CS_PDU_TYPE 0xff000000
#define BE_TAG 0x00ff0000
#define BA_SIZE 0x0000ffff

#define CS_PROTOCOL_CONTROL_FIELD 0xff000000
#define CS_LENGTH 0x0000ffff

/*** DEVICE STATUS ***/

typedef enum {      /*"Flavors" for device_get_status and device_set_status*/
  ATM_MAP_SIZE,     /* device_get_status options */
  ATM_MTU_SIZE,
  ATM_EVC_ID,       /* ID of event counter assigned to device */
  ATM_ASSIGNMENT,   /* Returns two words indicating whether device is mapped
		       and number of tasks with the device open */
                    /* device_set_status options */
  ATM_INITIALIZE,   /* Restarts hardware and low-level driver */
  ATM_PVC_SET       /* Sets up a permanent virtual connection --
		       the status argument array is cast to a nw_pvc_s 
                       structure */

} atm_status;        

typedef struct {
  nw_peer_s pvc;         /* Permanent virtual connection */
  u_int tx_vp;           /* VPI used for transmissions to permanent virtual
                            connection. The VPI used for reception is the
                            local endpoint number. VCIs are 0 */
  nw_protocol protocol;  /* Protocol of connection (possibly NW_LINE) */
} nw_pvc_s, *nw_pvc_t;

/*** BYTE ORDER ***/

/*The ATM header and SAR header and trailer are converted to and from
  host byte order by hardware. CS headers and trailers and
  signaling messages need byte order conversion in software.
  Conversion in software is also necessary for application messages
  if the communicating hosts have different byte orders (e.g. DECstation
  and SPARCstation). */

#define HTONL(x) \
  ((x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | ((u_int) x >> 24))

#define NTOHL(x) HTONL(x)

#if 0
unsigned int htonl(unsigned int x) {

  return ((x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24));
}

#define ntohl(x) htonl(x)

#endif

#endif /* _TCA100_H_ */

