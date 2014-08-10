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

#define FA_BLCK	0x10000

#define	FA_ROM	0x00000

#define	FA_CTL	0x10000
#define FA_STAT		0x10000
#define	  I_RCV_CNT	0x00001
#define	  I_RCV_EOM	0x00002
#define	  I_RCV_TIM	0x00004
#define	  I_XMT_CNT	0x00008
#define	  I_RCV_LOS	0x00010
#define	  I_RCV_CARRIER	0x00020
#define FA_CR_S		0x10004
#define FA_CR_C		0x10008
#define FA_CR		0x1000C
#define	  ENI_RCV_CNT	0x00001
#define   ENI_RCV_END	0x00002
#define   ENI_RCV_TIM	0x00004
#define   ENI_XMT_CNT	0x00008
#define   EN_TEST	0x00010
#define   EN_UNUSED	0x00020
#define   EN_RCV	0x00040
#define   EN_XMT	0x00080
#define   RESET_RCV	0x00100
#define   RESET_XMT	0x00200
#define FA_TIM		0x10010
#define FA_TIM_SET	0x10018
#define FA_RCV_CNT	0x10020
#define FA_RCV_CMP	0x10028
#define FA_XMT_CNT	0x10030
#define FA_XMT_CMP	0x10038


#define	FA_DISCARD	0x20000
#define FA_RCV	0x20000
#define	  FA_RCV_HD	0x20000
#define	  FA_RCV_PAYLD	0x20004
#define   FA_RCV_TR	0x20034

#define	FA_XMT	0x30000
#define	  FA_XMT_HD	0x30000
#define	  FA_XMT_PAYLD	0x30004
#define   FA_XMT_TR	0x30034

#define FA_END	0x40000


struct ecc {
/* 00000 */	char	rom[FA_BLCK];
/* 10000 */	int	stat;
/* 10004 */	int	cr_s;
/* 10008 */	int	cr_c;
/* 1000C */	int	cr;
/* 10010 */	int	tim;
 		int	fill1;
/* 10018 */	int	tim_set;
		int	fill2;
/* 10020 */	int	rcv_cnt;
		int	fill3;
/* 10028 */	int	rcv_cmp;
		int	fill4;
/* 10030 */	int	xmt_cnt;
		int	fill5;
/* 10038 */	int	xmt_cmp;
		int	fill6;
		char	pad[FA_BLCK-0x40];

/* 20000 */
/* 20000 */	char	rcv[FA_BLCK];
/* 30000 */	char	xmt[FA_BLCK];
};

struct sar {
	int header;
	int payload[12];
	int trailer;
};

typedef struct ecc ecc_t;
typedef struct sar sar_t;




