/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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
 * Berkeley Packet Filter Definitions from Berkeley
 */

/*-
 * Copyright (c) 1990-1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence 
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)bpf.h       7.1 (Berkeley) 5/7/91
 *
 */

#ifndef _DEVICE_BPF_H_
#define _DEVICE_BPF_H_

#if 0	/* not used in MK now */
/*
 * Alignment macros.  BPF_WORDALIGN rounds up to the next 
 * even multiple of BPF_ALIGNMENT. 
 */
#define BPF_ALIGNMENT sizeof(int)
#define BPF_WORDALIGN(x) (((x)+(BPF_ALIGNMENT-1))&~(BPF_ALIGNMENT-1))

/*
 * Struct return by BIOCVERSION.  This represents the version number of 
 * the filter language described by the instruction encodings below.
 * bpf understands a program iff kernel_major == filter_major &&
 * kernel_minor >= filter_minor, that is, if the value returned by the
 * running kernel has the same major number and a minor number equal
 * equal to or less than the filter being downloaded.  Otherwise, the
 * results are undefined, meaning an error may be returned or packets
 * may be accepted haphazardly.
 * It has nothing to do with the source code version.
 */
struct bpf_version {
	u_short bv_major;
	u_short bv_minor;
};
/* Current version number. */
#define BPF_MAJOR_VERSION 1
#define BPF_MINOR_VERSION 1

/*
 * Data-link level type codes.
 * Currently, only DLT_EN10MB and DLT_SLIP are supported.
 */
#define DLT_NULL	0	/* no link-layer encapsulation */
#define DLT_EN10MB	1	/* Ethernet (10Mb) */
#define DLT_EN3MB	2	/* Experimental Ethernet (3Mb) */
#define DLT_AX25	3	/* Amateur Radio AX.25 */
#define DLT_PRONET	4	/* Proteon ProNET Token Ring */
#define DLT_CHAOS	5	/* Chaos */
#define DLT_IEEE802	6	/* IEEE 802 Networks */
#define DLT_ARCNET	7	/* ARCNET */
#define DLT_SLIP	8	/* Serial Line IP */
#define DLT_PPP		9	/* Point-to-point Protocol */
#define DLT_FDDI	10	/* FDDI */

#endif /* 0 */

/*
 * The instruction encondings.
 */

/* Magic number for the first instruction */
#define BPF_BEGIN NETF_BPF

/* instruction classes */
#define BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

/* ld/ldx fields */
#define BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
#define BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM 	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0

/* alu/jmp fields */
#define BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
#define		BPF_CKMATCH_IMM	0x50
#define BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

/* ret - BPF_K and BPF_X also apply */
#define BPF_RVAL(code)	((code) & 0x38)
#define		BPF_A		0x10
#define		BPF_MATCH_IMM	0x18
#define		BPF_MATCH_DATA	0x20

/* misc */
#define BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
#define		BPF_TXA		0x80
#define		BPF_KEY		0x10
#define		BPF_REG_DATA	0x18
#define		BPF_POSTPONE	0x20

/*
 * The instruction data structure.
 */
struct bpf_insn {
	unsigned short	code;
	unsigned char 	jt;
	unsigned char 	jf;
	int	k;
};
typedef struct bpf_insn *bpf_insn_t;

/*
 * largest bpf program size
 */
#define NET_MAX_BPF ((NET_MAX_FILTER*sizeof(filter_t))/sizeof(struct bpf_insn))

/*
 * Macros for insn array initializers.
 */
#define BPF_STMT(code, k) { (unsigned short)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (unsigned short)(code), jt, jf, k }
#define BPF_RETMATCH(code, k, nkey) { (unsigned short)(code), nkey, 0, k }

#define	BPF_INSN_STMT(pc, c, n)  {\
	(pc)->code = (c);		\
	(pc)->jt = (pc)->jf = 0;	\
	(pc)->k = (n);			\
	(pc)++;				\
}

#define BPF_INSN_JUMP(pc, c, n, jtrue, jfalse) {\
	(pc)->code = (c);		\
	(pc)->jt = (jtrue);		\
	(pc)->jf = (jfalse);		\
	(pc)->k = (n);			\
	(pc)++;				\
}

#define BPF_INSN_RETMATCH(pc, c, n, nkey) {\
	(pc)->code = (c);		\
	(pc)->jt = (nkey);		\
	(pc)->jf = 0;			\
	(pc)->k = (n);			\
	(pc)++;				\
}

/*
 * Number of scratch memory words (for BPF_LD|BPF_MEM and BPF_ST).
 */
#define BPF_MEMWORDS 16

/*
 * Link level header can be accessed by adding BPF_DLBASE to an offset.
 */
#define BPF_DLBASE	(1<<30)

#define BPF_BYTES(n) ((n) * sizeof (struct bpf_insn))
#define BPF_BYTES2LEN(n) ((n) / sizeof (struct bpf_insn))
#define BPF_INSN_EQ(p,q) ((p)->code == (q)->code && \
			  (p)->jt == (q)->jt &&	\
			  (p)->jf == (q)->jf &&	\
			  (p)->k == (q)->k)

#endif /* _DEVICE_BPF_H_ */
