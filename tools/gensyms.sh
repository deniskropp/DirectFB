#!/bin/sh -e

. $1

if test -n "$dlname"; then
  nm -n "`dirname $1`/$dlname" | grep ' [tT] [^.]' > $2/symbols.dynamic
fi
