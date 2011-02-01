#!/bin/bash -e

FILE="$1"
shift

DEBUG="$1"
shift


echo "Building '$FILE' with '$DEBUG'..."


DIR_="${FILE/.tar.gz/}"


if test "$DIR_" == "$FILE"; then
    echo "Package extension unsupported!"
    exit 1
fi


DIR="tmp_dir/$DIR_"


rm -rf "$DIR"

mkdir -p "$DIR"
mkdir -p "$DIR/bin"
mkdir -p "$DIR/lib"
mkdir -p "$DIR/include"
mkdir -p "$DIR/include/direct"
mkdir -p "$DIR/include/direct/os"
mkdir -p "$DIR/include/direct/os/win32"
mkdir -p "$DIR/include/fusion"
mkdir -p "$DIR/include/voodoo"
mkdir -p "$DIR/include/gfx"

cp -a $DEBUG/*.dll "$DIR/bin/"
cp -a $DEBUG/*.lib "$DIR/lib/"

cp -a $DEBUG/*.exe "$DIR/bin/"

HEADERS="`find .. -name '*.h'`"

for HEADER in $HEADERS; do
    echo "X------- $HEADER"

    case $HEADER in
        ../lib/direct/os/linux*)
            continue
            ;;
        ../lib/direct/os/psp*)
            continue
            ;;
        ../lib/fusion/shm*)
            continue
            ;;
        ../src/gfx/generic*)
            continue
            ;;

        ../Win32/directfb_build.h)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- D $TARGET"
            ;;

        ../Win32/DiVine/include/*)
            TARGET=`echo $HEADER | cut -d/ -f5-`
            echo "---------------- D $TARGET"
            ;;

        ../Win32/FusionDale/include/*)
            TARGET=`echo $HEADER | cut -d/ -f5-`
            echo "---------------- D $TARGET"
            ;;

        ../Win32/++DFB/include/*)
            TARGET=`echo $HEADER | cut -d/ -f5-`
            echo "---------------- D $TARGET"
            ;;

        ../Win32/direct/*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- C $TARGET"
            ;;

        ../Win32/fusion/*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- C $TARGET"
            ;;

        ../Win32/voodoo/*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- C $TARGET"
            ;;

        ../*build.h)
            continue
            ;;
            
        ../include*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- D $TARGET"
            ;;

        ../lib*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- L $TARGET"
            ;;

        ../src/gfx*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- I $TARGET"
            ;;

        Jslib/jslibrc/*)
            TARGET=`echo $HEADER | cut -d/ -f3-`
            echo "---------------- J $TARGET"
            ;;
			
        *)
            continue
            ;;
    esac

    cp -a "$HEADER" "$DIR/include/$TARGET"
done

cd tmp_dir
tar czf "../$FILE" "$DIR_"

