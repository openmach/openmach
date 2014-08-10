/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

/* stuff used by more than one of header.c, user.c, server.c */

typedef void	write_list_fn_t(FILE *file, const argument_t *arg);

extern void WriteImport(FILE *file, const_string_t filename);
extern void WriteRCSDecl(FILE *file, identifier_t name, const_string_t rcs);
extern void WriteBogusDefines(FILE *file);

extern void WriteList(FILE *file, const argument_t *args, write_list_fn_t *func,
		      u_int mask, const char *between, const char *after);

extern void WriteReverseList(FILE *file, const argument_t *args,
			     write_list_fn_t *func, u_int mask,
			     const char *between, const char *after);

/* good as arguments to WriteList */
extern write_list_fn_t WriteNameDecl;
extern write_list_fn_t WriteUserVarDecl;
extern write_list_fn_t WriteServerVarDecl;
extern write_list_fn_t WriteTypeDeclIn;
extern write_list_fn_t WriteTypeDeclOut;
extern write_list_fn_t WriteCheckDecl;

extern const char *ReturnTypeStr(const routine_t *rt);

extern const char *FetchUserType(const ipc_type_t *it);
extern const char *FetchServerType(const ipc_type_t *it);
extern void WriteFieldDeclPrim(FILE *file, const argument_t *arg,
			       const char *(*tfunc)(const ipc_type_t *it));

extern void WriteStructDecl(FILE *file, const argument_t *args,
			    write_list_fn_t *func, u_int mask,
			    const char *name);

extern void WriteStaticDecl(FILE *file, const ipc_type_t *it,
			    dealloc_t dealloc, boolean_t longform,
			    boolean_t inname, identifier_t name);

extern void WriteCopyType(FILE *file, const ipc_type_t *it,
			  const char *left, const char *right, ...);

extern void WritePackMsgType(FILE *file, const ipc_type_t *it,
			     dealloc_t dealloc, boolean_t longform,
			     boolean_t inname, const char *left,
			     const char *right, ...);

#endif	/* _UTILS_H */
