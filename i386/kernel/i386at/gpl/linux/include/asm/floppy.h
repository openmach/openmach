/*
 * Architecture specific parts of the Floppy driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995
 */
#ifndef __ASM_I386_FLOPPY_H
#define __ASM_I386_FLOPPY_H

#define fd_inb(port)			inb_p(port)
#define fd_outb(port,value)		outb_p(port,value)

#define fd_enable_dma()         enable_dma(FLOPPY_DMA)
#define fd_disable_dma()        disable_dma(FLOPPY_DMA)
#define fd_request_dma()        request_dma(FLOPPY_DMA,"floppy")
#define fd_free_dma()           free_dma(FLOPPY_DMA)
#define fd_clear_dma_ff()       clear_dma_ff(FLOPPY_DMA)
#define fd_set_dma_mode(mode)   set_dma_mode(FLOPPY_DMA,mode)
#define fd_set_dma_addr(addr)   set_dma_addr(FLOPPY_DMA,addr)
#define fd_set_dma_count(count) set_dma_count(FLOPPY_DMA,count)
#define fd_enable_irq()         enable_irq(FLOPPY_IRQ)
#define fd_disable_irq()        disable_irq(FLOPPY_IRQ)
#define fd_cacheflush(addr,size) /* nothing */
#define fd_request_irq()        request_irq(FLOPPY_IRQ, floppy_interrupt, \
					    SA_INTERRUPT|SA_SAMPLE_RANDOM, \
				            "floppy")
#define fd_free_irq()           free_irq(FLOPPY_IRQ);

__inline__ void virtual_dma_init(void)
{
	/* Nothing to do on an i386 */
}

static int FDC1 = 0x3f0;
static int FDC2 = -1;

#define FLOPPY0_TYPE	((CMOS_READ(0x10) >> 4) & 15)
#define FLOPPY1_TYPE	(CMOS_READ(0x10) & 15)

#define N_FDC 2
#define N_DRIVE 8

/*
 * The DMA channel used by the floppy controller cannot access data at
 * addresses >= 16MB
 *
 * Went back to the 1MB limit, as some people had problems with the floppy
 * driver otherwise. It doesn't matter much for performance anyway, as most
 * floppy accesses go through the track buffer.
 */
#define CROSS_64KB(a,s) ((unsigned long)(a)/K_64 != ((unsigned long)(a) + (s) - 1) / K_64)

#endif /* __ASM_I386_FLOPPY_H */
