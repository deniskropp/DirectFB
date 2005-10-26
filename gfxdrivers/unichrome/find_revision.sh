#!/bin/sh

# The CLE266 revision number can be found at offset 0xf6 from the host
# bridge's PCI configuration space.  However, it can only be read by
# a superuser process.
#
# This script prints the revision number which can then be set in
# DirectFB's configuration file /etc/directfbrc.

if [ `id -u` -ne 0 ]; then
	echo Only root can read the necessary bytes to determine the
	echo revision number.
	exit 1
fi

set `od -j246 -N1 -Ax -td1 /proc/bus/pci/00/00.0`
unichrome_revision=$2

if [ "$unichrome_revision" = "" ]; then
	echo Failed to read CLE266 revision number.
	exit 1
fi

echo Your CLE266 revision number is $unichrome_revision.
echo To use this value, add the following line to /etc/directfbrc:
echo "   " unichrome-revision=$unichrome_revision
