#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_second" value.
 */

extern unsigned long loops_per_sec;

#include <asm/delay.h>

#endif /* defined(_LINUX_DELAY_H) */
