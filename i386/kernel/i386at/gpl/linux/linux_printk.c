/*
 * Linux kernel print routine.
 * Copyright (C) 1995 Shantanu Goel.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *  linux/kernel/printk.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <stdarg.h>
#include <asm/system.h>

static char buf[2048];

void
printk(char *fmt, ...)
{
	va_list args;
	int i, n, flags;
	extern void cnputc();
	extern int linux_vsprintf(char *buf, char *fmt, ...);

	save_flags(flags);
	cli();
	va_start(args, fmt);
	n = linux_vsprintf(buf, fmt, args);
	va_end(args);
	for (i = 0; i < n; i++)
		cnputc(buf[i]);
	restore_flags(flags);
}
