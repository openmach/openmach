#!/bin/sh 
#
# Mach Operating System
# Copyright (c) 1991,1990 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
# 
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# 
# Carnegie Mellon requests users of this software to return to
# 
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
# 
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#

migcom=${MIGDIR-@MIGDIR@}/migcom
cpp="${CPP-@CPP@}"

cppflags=
migflags=
files=

# If an argument to this shell script contains whitespace,
# then we will screw up.  migcom will see it as multiple arguments.
#
# As a special hack, if -i is specified first we don't pass -user to migcom.
# We do use the -user argument for the dependencies.
# In this case, the -user argument can have whitespace.

until [ $# -eq 0 ]
do
    case "$1" in
	-[qQvVtTrRsS] ) migflags="$migflags $1"; shift;;
	-i	) sawI=1; migflags="$migflags $1 $2"; shift; shift;;
	-user   ) user="$2"; if [ ! "${sawI-}" ]; then migflags="$migflags $1 $2"; fi; shift; shift;;
	-server ) server="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-header ) header="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-sheader ) sheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-iheader ) iheader="$2"; migflags="$migflags $1 $2"; shift; shift;;
	-prefix | -subrprefix ) migflags="$migflags $1 $2"; shift; shift;;

	-MD ) sawMD=1; cppflags="$cppflags $1"; shift;;
	-imacros ) cppflags="$cppflags $1 $2"; shift; shift;;
	-cc) cpp="$2"; shift; shift;;
	-migcom) migcom="$2"; shift; shift;;
	-* ) cppflags="$cppflags $1"; shift;;
	* ) files="$files $1"; shift;;
    esac
done

for file in $files
do
    $cpp $cppflags "$file" | $migcom $migflags || exit

    if [ $sawMD ]
    then
	base="`basename "$file"|sed 's%[.][^.]*$%%'`"
	deps=
	rheader="${header-${base}.h}"
	if [ "$rheader" != /dev/null ]; then deps="$deps $rheader"; fi
	ruser="${user-${base}User.c}"
	if [ "$ruser" != /dev/null ]; then
		if [ $sawI ]; then
		    for un in $ruser 
		    do
			deps="$deps $un"
		    done
		else
		    deps="$deps $ruser"
		fi
	fi
	rserver="${server-${base}Server.c}"
	if [ "$rserver" != /dev/null ]; then deps="$deps $rserver"; fi
	rsheader="${sheader-/dev/null}"
	if [ "$rsheader" != /dev/null ]; then deps="$deps $rsheader"; fi
	riheader="${iheader-/dev/null}"
	if [ "$riheader" != /dev/null ]; then deps="$deps $riheader"; fi
	sed 's%^[^:]*:%'"${deps}"':%' <"${base}.d" >"${base}-mig.d"
	rm -f ${base}.d
    fi
done

exit 0
