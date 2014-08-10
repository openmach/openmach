/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the IP router.
 *
 * Version:	@(#)route.h	1.0.4	05/27/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 * Fixes:
 *		Alan Cox	:	Reformatted. Added ip_rt_local()
 *		Alan Cox	:	Support for TCP parameters.
 *		Alexey Kuznetsov:	Major changes for new routing code.
 *
 *	FIXME:
 *		Modules stuff is broken at the moment.
 *		Make atomic ops more generic and hide them in asm/...
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _ROUTE_H
#define _ROUTE_H

#include <linux/config.h>

/*
 * 0 - no debugging messages
 * 1 - rare events and bugs situations (default)
 * 2 - trace mode.
 */
#define RT_CACHE_DEBUG		1

#define RT_HASH_DIVISOR	    	256
#define RT_CACHE_SIZE_MAX    	256

#define RTZ_HASH_DIVISOR	256

#if RT_CACHE_DEBUG >= 2
#define RTZ_HASHING_LIMIT 0
#else
#define RTZ_HASHING_LIMIT 16
#endif

/*
 * Maximal time to live for unused entry.
 */
#define RT_CACHE_TIMEOUT		(HZ*300)

/*
 * Prevents LRU trashing, entries considered equivalent,
 * if the difference between last use times is less then this number.
 */
#define RT_CACHE_BUBBLE_THRESHOULD	(HZ*5)

#include <linux/route.h>

#ifdef __KERNEL__
#define RTF_LOCAL 0x8000
#endif

/*
 * Semaphores.
 */
#if defined(__alpha__)

static __inline__ void ATOMIC_INCR(unsigned int * addr)
{
	unsigned tmp;

	__asm__ __volatile__(
		"1:\n\
		 ldl_l %1,%2\n\
		 addl  %1,1,%1\n\
		 stl_c %1,%0\n\
		 beq   %1,1b\n"
		: "m=" (*addr), "r=&" (tmp)
		: "m"(*addr));
}

static __inline__ void ATOMIC_DECR(unsigned int * addr)
{
	unsigned tmp;

	__asm__ __volatile__(
		"1:\n\
		 ldl_l %1,%2\n\
		 subl  %1,1,%1\n\
		 stl_c %1,%0\n\
		 beq   %1,1b\n"
		: "m=" (*addr), "r=&" (tmp)
		: "m"(*addr));
}

static __inline__ int ATOMIC_DECR_AND_CHECK (unsigned int * addr)
{
	unsigned tmp;
	int result;

	__asm__ __volatile__(
		"1:\n\
		 ldl_l %1,%3\n\
		 subl  %1,1,%1\n\
		 mov   %1,%2\n\
		 stl_c %1,%0\n\
		 beq   %1,1b\n"
		: "m=" (*addr), "r=&" (tmp), "r=&"(result)
		: "m"(*addr));
	return result;
}

#elif defined(__i386__)
#include <asm/bitops.h>

extern __inline__ void ATOMIC_INCR(void * addr)
{
	__asm__ __volatile__(
		"incl %0"
		:"=m" (ADDR));
}

extern __inline__ void ATOMIC_DECR(void * addr)
{
	__asm__ __volatile__(
		"decl %0"
		:"=m" (ADDR));
}

/*
 * It is DECR that is ATOMIC, not CHECK!
 * If you want to do atomic checks, use cli()/sti(). --ANK
 */

extern __inline__ unsigned long ATOMIC_DECR_AND_CHECK(void * addr)
{
	unsigned long retval;
	__asm__ __volatile__(
		"decl %0\nmovl %0,%1"
		: "=m" (ADDR), "=r"(retval));
	return retval;
}


#else

static __inline__ void ATOMIC_INCR(unsigned int * addr)
{
	(*(__volatile__ unsigned int*)addr)++;
}

static __inline__ void ATOMIC_DECR(unsigned int * addr)
{
	(*(__volatile__ unsigned int*)addr)--;
}

static __inline__ int ATOMIC_DECR_AND_CHECK (unsigned int * addr)
{
	ATOMIC_DECR(addr);
	return *(volatile unsigned int*)addr;
}

#endif



struct rtable 
{
	struct rtable		*rt_next;
	__u32			rt_dst;
	__u32			rt_src;
	__u32			rt_gateway;
	unsigned		rt_refcnt;
	unsigned		rt_use;
	unsigned long		rt_window;
	unsigned long		rt_lastuse;
	struct hh_cache		*rt_hh;
	struct device		*rt_dev;
	unsigned short		rt_flags;
	unsigned short		rt_mtu;
	unsigned short		rt_irtt;
	unsigned char		rt_tos;
};

extern void		ip_rt_flush(struct device *dev);
extern void		ip_rt_redirect(__u32 src, __u32 dst, __u32 gw, struct device *dev);
extern struct rtable	*ip_rt_slow_route(__u32 daddr, int local);
extern int		rt_get_info(char * buffer, char **start, off_t offset, int length, int dummy);
extern int		rt_cache_get_info(char *buffer, char **start, off_t offset, int length, int dummy);
extern int		ip_rt_ioctl(unsigned int cmd, void *arg);
extern int		ip_rt_new(struct rtentry *rt);
extern void		ip_rt_check_expire(void);
extern void		ip_rt_advice(struct rtable **rp, int advice);

extern void		ip_rt_run_bh(void);
extern int	    	ip_rt_lock;
extern unsigned		ip_rt_bh_mask;
extern struct rtable 	*ip_rt_hash_table[RT_HASH_DIVISOR];

extern __inline__ void ip_rt_fast_lock(void)
{
	ATOMIC_INCR(&ip_rt_lock);
}

extern __inline__ void ip_rt_fast_unlock(void)
{
	ATOMIC_DECR(&ip_rt_lock);
}

extern __inline__ void ip_rt_unlock(void)
{
	if (!ATOMIC_DECR_AND_CHECK(&ip_rt_lock) && ip_rt_bh_mask)
		ip_rt_run_bh();
}

extern __inline__ unsigned ip_rt_hash_code(__u32 addr)
{
	unsigned tmp = addr + (addr>>16);
	return (tmp + (tmp>>8)) & 0xFF;
}


extern __inline__ void ip_rt_put(struct rtable * rt)
#ifndef MODULE
{
	if (rt)
		ATOMIC_DECR(&rt->rt_refcnt);
}
#else
;
#endif

#ifdef CONFIG_KERNELD
extern struct rtable * ip_rt_route(__u32 daddr, int local);
#else
extern __inline__ struct rtable * ip_rt_route(__u32 daddr, int local)
#ifndef MODULE
{
	struct rtable * rth;

	ip_rt_fast_lock();

	for (rth=ip_rt_hash_table[ip_rt_hash_code(daddr)^local]; rth; rth=rth->rt_next)
	{
		if (rth->rt_dst == daddr)
		{
			rth->rt_lastuse = jiffies;
			ATOMIC_INCR(&rth->rt_use);
			ATOMIC_INCR(&rth->rt_refcnt);
			ip_rt_unlock();
			return rth;
		}
	}
	return ip_rt_slow_route (daddr, local);
}
#else
;
#endif
#endif

extern __inline__ struct rtable * ip_check_route(struct rtable ** rp,
						       __u32 daddr, int local)
{
	struct rtable * rt = *rp;

	if (!rt || rt->rt_dst != daddr || !(rt->rt_flags&RTF_UP)
	    || ((local==1)^((rt->rt_flags&RTF_LOCAL) != 0)))
	{
		ip_rt_put(rt);
		rt = ip_rt_route(daddr, local);
		*rp = rt;
	}
	return rt;
}	


#endif	/* _ROUTE_H */
