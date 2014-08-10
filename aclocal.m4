dnl
dnl Copyright (c) 1995,1994 The University of Utah and
dnl the Computer Systems Laboratory (CSL).  All rights reserved.
dnl
dnl Permission to use, copy, modify and distribute this software and its
dnl documentation is hereby granted, provided that both the copyright
dnl notice and this permission notice appear in all copies of the
dnl software, derivative works or modified versions, and any portions
dnl thereof, and that both notices appear in supporting documentation.
dnl
dnl THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
dnl IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
dnl ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
dnl
dnl CSL requests users of this software to return to csl-dist@cs.utah.edu any
dnl improvements that they make and grant CSL redistribution rights.
dnl
dnl      Author: Bryan Ford, University of Utah CSL
dnl
dnl
dnl Generic macro to find and verify a secondary source directory.
dnl $1 = 'with' variable name extension to check
dnl $2 = shell variable to set
dnl $3 = human-readable name of source directory we're looking for
dnl $4 = name of file in that source director to test for
dnl $5 = default directory to check for sources in
dnl $6 = one-line description to appear in configure's usage text
define(AC_WITH_SRCDIR,[
AC_MSG_CHECKING(for $3 sources)
AC_ARG_WITH([$1],[$6],[
	$2=$with_$1
	if test ! -r "$$2/$4"; then
		AC_MSG_ERROR([$3 sources not found in directory $$2])
	fi
],[
	$2=$5
	if test ! -r "$$2/$4"; then
		AC_MSG_ERROR([$3 sources not found in $$2, and no --with-$1 option specified])
	fi
])
AC_SUBST_SRCPATH($2)
AC_MSG_RESULT($$2)
])dnl
dnl
