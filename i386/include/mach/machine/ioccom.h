/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef	__sys_ioccom_h
#define	__sys_ioccom_h

/*
 * Ioctl's have the command encoded in the lower word,
 * and the size of any in or out parameters in the upper
 * word.  The high 2 bits of the upper word are used
 * to encode the in/out status of the parameter; for now
 * we restrict parameters to at most 255 bytes.
 */
#define	_IOCPARM_MASK	0xff		/* parameters must be < 256 bytes */
#define	_IOC_VOID	0x20000000	/* no parameters */
#define	_IOC_OUT	0x40000000	/* copy out parameters */
#define	_IOC_IN		0x80000000	/* copy in parameters */
#define	_IOC_INOUT	(_IOC_IN|_IOC_OUT)
/* the 0x20000000 is so we can distinguish new ioctl's from old */
#define	_IO(x,y)	(_IOC_VOID|('x'<<8)|y)
#define	_IOR(x,y,t)	(_IOC_OUT|((sizeof(t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)
#define	_IORN(x,y,t)	(_IOC_OUT|(((t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)
#define	_IOW(x,y,t)	(_IOC_IN|((sizeof(t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)
#define	_IOWN(x,y,t)	(_IOC_IN|(((t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)
/* this should be _IORW, but stdio got there first */
#define	_IOWR(x,y,t)	(_IOC_INOUT|((sizeof(t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)
#define	_IOWRN(x,y,t)	(_IOC_INOUT|(((t)&_IOCPARM_MASK)<<16)|('x'<<8)|y)

#endif /* !__sys_ioccom_h */
