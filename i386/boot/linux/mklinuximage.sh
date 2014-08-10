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

# This little script uses GNU ld to build a Linux 16-bit compressed boot image
# from a set of boot modules.

machbootdir=${MACHBOOTDIR-@MACHBOOTDIR@}
cc=${CC-cc}
ld=${LD-ld}

files=
outfile=zImage
savetemps=

# Parse the command-line options.
until [ $# -eq 0 ]
do
	case "$1" in
		-o ) outfile="$2"; shift; shift;;
		-save-temps) savetemps="$1"; shift;;
		* ) files="$files $1"; shift;;
	esac
done

# Wrap each of the input files in a .o file.
# At the same time, build an assembly language module
# containing a table describing the boot modules.
echo >$outfile.mods.S ".text; .long 0xf00baabb"
for file in $files; do
	# Convert all non-alphanum chars to underscores.
	sym_name=`echo $file | sed -e 's,[^a-zA-Z0-9],_,g'`
	echo >>$outfile.mods.S ".long _binary_$sym_name""_start"
	echo >>$outfile.mods.S ".long _binary_$sym_name""_end"
	echo >>$outfile.mods.S ".long cmdline_$sym_name"
	echo >>$outfile.mods.S ".data"
	echo >>$outfile.mods.S "cmdline_$sym_name: .ascii \"$file\\0\""
	echo >>$outfile.mods.S ".text"
done
echo >>$outfile.mods.S ".long 0; .data; .align 4"

# Assemble the module vector file.
$cc -c -o $outfile.mods.o $outfile.mods.S

# Link the module vector file with the boot module files.
# Use the binary bfd backend for both the input bmod files and the output file.
$ld -Ttext 0 -oformat binary -o $outfile.tmp \
	$outfile.mods.o -format binary $files -format default \
	|| exit 1

# Compress the whole output file as one big glob.
gzip <$outfile.tmp >$outfile.tmp.gz

# Create the final boot image by tacking that onto the end of 'linuxboot'.
cat $machbootdir/linuxboot $outfile.tmp.gz >$outfile

if test -z "$savetemps"; then
	rm -f $outfile.mods.S $outfile.mods.o $outfile.tmp $outfile.tmp.gz
fi

exit 0
