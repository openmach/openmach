/*
 * (c) Copyright 1992, 1993, 1994, 1995 OPEN SOFTWARE FOUNDATION, INC. 
 * ALL RIGHTS RESERVED 
 */
/*
 * OSF RI nmk19b2 5/2/95
 */

/*
 *	File:		ddb/tr.h
 *	Author:		Alan Langerman, Jeffrey Heller
 *	Date:		1992
 *
 *	Internal trace routines.  Like old-style XPRs but
 *	less formatting.
 */

#include <mach_assert.h>
#include <mach_tr.h>

/*
 *	Originally, we only wanted tracing when
 *	MACH_TR and MACH_ASSERT were turned on
 *	together.  Now, there's no reason why
 *	MACH_TR and MACH_ASSERT can't be completely
 *	orthogonal.
 */
#define	TRACE_BUFFER	(MACH_TR)

/*
 *	Log events in a circular trace buffer for future debugging.
 *	Events are unsigned integers.  Each event has a descriptive
 *	message.
 *
 *	TR_DECL must be used at the beginning of a routine using
 *	one of the tr calls.  The macro should be passed the name
 *	of the function surrounded by quotation marks, e.g.,
 *		TR_DECL("netipc_recv_intr");
 *	and should be terminated with a semi-colon.  The TR_DECL
 *	must be the *last* declaration in the variable declaration
 *	list, or syntax errors will be introduced when TRACE_BUFFER
 *	is turned off.
 */
#ifndef	_DDB_TR_H_
#define	_DDB_TR_H_

#if	TRACE_BUFFER

#include <machine/db_machdep.h>

#define	__ui__			(unsigned int)
#define	TR_INIT()		tr_init()
#define TR_SHOW(a,b,c)		show_tr((a),(b),(c))
#define	TR_DECL(funcname)	char	*__ntr_func_name__ = funcname
#define	tr1(msg)							\
	tr(__ntr_func_name__, __FILE__, __LINE__, (msg),		\
		0,0,0,0)
#define	tr2(msg,tag1)							\
	tr(__ntr_func_name__, __FILE__, __LINE__, (msg),		\
		__ui__(tag1),0,0,0)
#define	tr3(msg,tag1,tag2)						\
	tr(__ntr_func_name__, __FILE__, __LINE__, (msg),		\
		__ui__(tag1),__ui__(tag2),0,0)
#define	tr4(msg,tag1,tag2,tag3)						\
	tr(__ntr_func_name__, __FILE__, __LINE__, (msg),		\
		__ui__(tag1),__ui__(tag2),__ui__(tag3),0)
#define	tr5(msg,tag1,tag2,tag3,tag4)					\
	tr(__ntr_func_name__, __FILE__, __LINE__, (msg),		\
		__ui__(tag1),__ui__(tag2),__ui__(tag3),__ui__(tag4))

/*
 *	Adjust tr log indentation based on function
 *	call graph; this method is quick-and-dirty
 *	and only works safely on a uniprocessor.
 */
extern int tr_indent;
#define	tr_start()	tr_indent++
#define tr_stop()	tr_indent--

extern void	tr_init(void);
extern void	tr(
			char		*funcname,
			char		*file,
			unsigned int	lineno,
			char		*fmt,
			unsigned int	tag1,
		   	unsigned int	tag2,
			unsigned int	tag3,
			unsigned int	tag4);

extern void db_show_tr(
			db_expr_t	addr,
			boolean_t	have_addr,
			db_expr_t	count,
			char *		modif);

#else	/* TRACE_BUFFER */

#define	TR_INIT()
#define TR_SHOW(a,b,c)
#define	TR_DECL(funcname)
#define tr1(msg)
#define tr2(msg, tag1)
#define tr3(msg, tag1, tag2)
#define tr4(msg, tag1, tag2, tag3)
#define tr5(msg, tag1, tag2, tag3, tag4)
#define	tr_start()
#define tr_stop()

#endif	/* TRACE_BUFFER */

#endif	/* _DDB_TR_H_ */
