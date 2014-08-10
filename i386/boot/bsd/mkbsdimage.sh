#!/bin/sh
#
# Copyright (c) 1994 The University of Utah and
# the Computer Systems Laboratory (CSL).  All rights reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
# IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
# ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# CSL requests users of this software to return to csl-dist@cs.utah.edu any
# improvements that they make and grant CSL redistribution rights.
#
#      Author: Bryan Ford, University of Utah CSL
#

# This little script uses GNU ld to build a BSD/Mach 32-bit boot image
# from a kernel and a set of boot modules.

machbootdir=${MACHBOOTDIR-@MACHBOOTDIR@}
cc=${CC-cc}
ld=${LD-ld}

modules=
outfile=Image
savetemps=
ldopts="-Ttext 100000 -n"

# Parse the command-line options.
until [ $# -eq 0 ]
do
	case "$1" in
		-o ) outfile="$2"; shift; shift;;
		-save-temps) savetemps="$1"; shift;;
		* ) modules="$modules $1"; shift;;
	esac
done

# Wrap each of the input files in a .o file.
# At the same time, build an assembly language module
# containing a table describing the boot modules.
echo >$outfile.mods.S ".data; .globl boot_modules,_boot_modules; boot_modules:; _boot_modules:"
files=
for module in $modules; do
	# Split out the associated string, if any.
	file=`echo $module | sed -e 's,:.*$,,'`
	string=`echo $module | sed -e 's,^[^:]*:,,'`
	if test -z "$string"; then string=$file; fi
	files="$files $file"

	# Convert all non-alphanum chars to underscores for the symbol name.
	sym_name=`echo $file | sed -e 's,[^a-zA-Z0-9],_,g'`

	# Produce an entry in the module description file.
	echo >>$outfile.mods.S ".long _binary_$sym_name""_start"
	echo >>$outfile.mods.S ".long _binary_$sym_name""_end"
	echo >>$outfile.mods.S ".long string_$sym_name"
	echo >>$outfile.mods.S ".data 2"
	echo >>$outfile.mods.S "string_$sym_name: .ascii \"$string\\0\""
	echo >>$outfile.mods.S ".data"
done
echo >>$outfile.mods.S ".long 0; .data; .align 4"

# Assemble the module vector file.
$cc -c -o $outfile.mods.o $outfile.mods.S

# Link the BSD boot file and the module vector file with the boot module files.
# Use the binary bfd backend for the input bmod files.
$ld $ldopts -o $outfile $machbootdir/bsdboot.o $outfile.mods.o \
	-format binary $files -format default \
	|| exit 1

if test -z "$savetemps"; then
	rm -f $outfile.mods.S $outfile.mods.o
fi

exit 0
