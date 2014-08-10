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
 
#include <mach/vm_prot.h>
#include <mach/machine/vm_types.h>
#include <mach/machine/vm_param.h>
#include <mach/machine/eflags.h>

#include <ipc/ipc_port.h>

#include <device/io_req.h>

#include <i386/io_port.h>
#include <i386/pit.h>

/*
 * IOPL device.
 */
ipc_port_t	iopl_device_port = IP_NULL;
mach_device_t	iopl_device = 0;

/*
 * Ports that we allow access to.
 */
io_reg_t iopl_port_list[] = {
	/* timer 2 */
	0x42,
	/* speaker output */
	0x61,
	/* ATI - savage */
	0x1ce, 0x1cf,
	/* game port */
	0x201,
	/* sound board */
	0x220, 0x221, 0x222, 0x223, 0x224, 0x225, 0x226, 0x227,
	0x228, 0x229, 0x22a, 0x22b, 0x22c, 0x22d, 0x22e, 0x22f,
	/* printer */
	0x278, 0x279, 0x27a,
	0x378, 0x379, 0x37a,
	/* ega/vga */
	0x3b0, 0x3b1, 0x3b2, 0x3b3, 0x3b4, 0x3b5, 0x3b6, 0x3b7,
	0x3b8, 0x3b9, 0x3ba, 0x3bb, 0x3bc, 0x3bd, 0x3be, 0x3bf,
	0x3c0, 0x3c1, 0x3c2, 0x3c3, 0x3c4, 0x3c5, 0x3c6, 0x3c7,
	0x3c8, 0x3c9, 0x3ca, 0x3cb, 0x3cc, 0x3cd, 0x3ce, 0x3cf,
	0x3d0, 0x3d1, 0x3d2, 0x3d3, 0x3d4, 0x3d5, 0x3d6, 0x3d7,
	0x3d8, 0x3d9, 0x3da, 0x3db, 0x3dc, 0x3dd, 0x3de, 0x3df,
	/* end of list */
	IO_REG_NULL,
	/* patch space */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

int
ioplopen(dev, flag, ior)
	int	dev;
	int	flag;
	io_req_t ior;
{
	iopl_device_port = ior->io_device->port;
	iopl_device = ior->io_device;

	io_port_create(iopl_device, iopl_port_list);
	return (0);
}


/*ARGSUSED*/
ioplclose(dev, flags)
	int	dev;
	int flags;
{
	io_port_destroy(iopl_device);
	iopl_device_port = IP_NULL;
	iopl_device = 0;
	return 0;
}

/*ARGSUSED*/
int iopl_all = 1;
ioplmmap(dev, off, prot)
int		dev;
vm_offset_t	off;
vm_prot_t	prot;
{
    extern vm_offset_t phys_last_addr;

    if (iopl_all) {
	if (off == 0)
		return 0;
	else if (off < 0xa0000)
		return -1;
	else if (off >= 0x100000 && off <= phys_last_addr)
		return -1;
	else
		return i386_btop(off);

    }
	if (off > 0x60000)
		return(-1);

	/* Get page frame number for the page to be mapped. */

	return(i386_btop(0xa0000 + off));
}

/*
 * For DOS compatibility, it's easier to list the ports we don't
 * allow access to.
 */
#define	IOPL_PORTS_USED_MAX	256
io_reg_t iopl_ports_used[IOPL_PORTS_USED_MAX] = {
	IO_REG_NULL
};

boolean_t
iopl_port_forbidden(io_port)
	int	io_port;
{
	int	i;

#if 0	/* we only read from these... it should be OK */

	if (io_port <= 0xff)
	    return TRUE;	/* system ports.  42,61,70,71 allowed above */

	if (io_port >= 0x130 && io_port <= 0x137)
	    return TRUE;	/* AHA disk */

	if (io_port >= 0x170 && io_port <= 0x177)
	    return TRUE;	/* HD disk */

	if (io_port >= 0x1f0 && io_port <= 0x1f7)
	    return TRUE;	/* HD disk */

	if (io_port >= 0x230 && io_port <= 0x237)
	    return TRUE;	/* AHA disk */

	if (io_port >= 0x280 && io_port <= 0x2df)
	    return TRUE;	/* 8390 network */

	if (io_port >= 0x300 && io_port <= 0x31f)
	    return TRUE;	/* 8390 network */

	if (io_port >= 0x330 && io_port <= 0x337)
	    return TRUE;	/* AHA disk */

	if (io_port >= 0x370 && io_port <= 0x377)
	    return TRUE;	/* FD disk */

	if (io_port >= 0x3f0 && io_port <= 0x3f7)
	    return TRUE;	/* FD disk */

#endif

	/*
	 * Must be OK, as far as we know...
	 * Record the port in the list, for
	 * curiosity seekers.
	 */
	for (i = 0; i < IOPL_PORTS_USED_MAX; i++) {
	    if (iopl_ports_used[i] == io_port)
		break;			/* in list */
	    if (iopl_ports_used[i] == IO_REG_NULL) {
		iopl_ports_used[i] = io_port;
		iopl_ports_used[i+1] = IO_REG_NULL;
		break;
	    }
	}

	return FALSE;
}

/*
 * Emulate certain IO instructions for the AT bus.
 *
 * We emulate writes to the timer control port, 43.
 * Only writes to timer 2 are allowed.
 *
 * Temporarily, we allow reads of any IO port,
 * but ONLY if the thread has the IOPL device mapped
 * and is not in V86 mode.
 *
 * This is a HACK and MUST go away when the DOS emulator
 * emulates these IO ports, or when we decide that
 * the DOS world can get access to all uncommitted IO
 * ports.  In that case, config() should remove the IO
 * ports for devices it exists from the allowable list.
 */
boolean_t
iopl_emulate(regs, opcode, io_port)
	struct i386_saved_state *regs;
	int	opcode;
	int	io_port;
{
	iopb_tss_t	iopb;

	iopb = current_thread()->pcb->ims.io_tss;
	if (iopb == 0)
	    return FALSE;		/* no IO mapped */

	/*
	 * Handle outb to the timer control port,
	 * for timer 2 only.
	 */
	if (io_port == PITCTL_PORT) {

	    int	io_byte = regs->eax & 0xff;

	    if (((iopb->bitmap[PITCTR2_PORT >> 3] & (1 << (PITCTR2_PORT & 0x7)))
		== 0)	/* allowed */
	     && (opcode == 0xe6 || opcode == 0xee)	/* outb */
	     && (io_byte & 0xc0) == 0x80)		/* timer 2 */
	    {
		outb(io_port, io_byte);
		return TRUE;
	    }
	    return FALSE;	/* invalid IO to port 42 */
	}

	/*
	 * If the thread has the IOPL device mapped, and
	 * the io port is not on the 'forbidden' list, allow
	 * reads from it.  Reject writes.
	 *
	 * Don`t do this for V86 mode threads
	 * (hack for DOS emulator XXX!)
	 */
	if (!(regs->efl & EFL_VM) &&
	    iopb_check_mapping(current_thread(), iopl_device) &&
	    !iopl_port_forbidden(io_port))
	{
	    /*
	     * handle inb, inw, inl
	     */
	    switch (opcode) {
		case 0xE4:		/* inb imm */
		case 0xEC:		/* inb dx */
		    regs->eax = (regs->eax & 0xffffff00)
				| inb(io_port);
		    return TRUE;

		case 0x66E5:		/* inw imm */
		case 0x66ED:		/* inw dx */
		    regs->eax = (regs->eax & 0xffff0000)
				| inw(io_port);
		    return TRUE;

		case 0xE5:		/* inl imm */
		case 0xED:		/* inl dx */
		    regs->eax = inl(io_port);
		    return TRUE;

		default:
		    return FALSE;	/* OUT not allowed */
	    }
	}

	/*
	 * Not OK.
	 */
	return FALSE;
}

