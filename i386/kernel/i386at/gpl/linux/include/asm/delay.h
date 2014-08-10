#ifndef _I386_DELAY_H
#define _I386_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */
 
#ifdef __SMP__
#include <asm/smp.h>
#endif 

extern __inline__ void __delay(int loops)
{
	__asm__ __volatile__(
		".align 2,0x90\n1:\tdecl %0\n\tjns 1b"
		:/* no outputs */
		:"a" (loops)
		:"ax");
}

/*
 * division by multiplication: you don't have to worry about
 * loss of precision.
 *
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)
 */
extern __inline__ void udelay(unsigned long usecs)
{
	usecs *= 0x000010c6;		/* 2**32 / 1000000 */
	__asm__("mull %0"
		:"=d" (usecs)
#ifdef __SMP__
		:"a" (usecs),"0" (cpu_data[smp_processor_id()].udelay_val)
#else
		:"a" (usecs),"0" (loops_per_sec)
#endif
		:"ax");
		
	__delay(usecs);
}

extern __inline__ unsigned long muldiv(unsigned long a, unsigned long b, unsigned long c)
{
	__asm__("mull %1 ; divl %2"
		:"=a" (a)
		:"d" (b),
		 "r" (c),
		 "0" (a)
		:"dx");
	return a;
}

#endif /* defined(_I386_DELAY_H) */
