#!/bin/sh

if test -z "$6"; then
  echo "Usage:   $0 <enum> <prefix> <null> <name> <value> <header>"
  echo "Example: $0 DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format directfb.h"
  exit 1
fi

ENUM=$1
PREFIX=$2
NULL=$3
NAME=$4
VALUE=$5
HEADER=$6


cat << EOF

#define DirectFB${NAME}Names(Identifier) struct DFB${NAME}Name { \\
     ${ENUM} ${VALUE}; \\
     const char *name; \\
} Identifier[] = { \\
EOF

egrep "^ +${PREFIX}_[0-9A-Za-z_]+[ ,]" $HEADER | grep -v ${PREFIX}_${NULL} | perl -p -e "s/^\\s*(${PREFIX}_)([\\w_]+)[ ,].*/     \\{ \\1\\2, \\\"\\2\\\" \\}, \\\\/"

cat << EOF
     { ${PREFIX}_${NULL}, "${NULL}" } \\
};
EOF
