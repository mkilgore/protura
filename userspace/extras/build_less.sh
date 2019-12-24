#!/bin/bash
echo "$0 <protura-target> <protura-root> <protura-prefix> <make-flags>"

if [ "$#" -ne 4 ]; then
    echo "Error: Please supply correct arguments"
    exit 1
fi

PROTURA_TARGET=$1
PROTURA_ROOT="`readlink -f "$2"`"
PROTURA_PREFIX="`readlink -f "$3"`"
MAKE_FLAGS="$4"

echo "  Building Less..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

export ac_cv_lib_ncurses_initscr=yes

rm -fr ./less-build

mkdir ./less-build
cd ./less-build

../less/configure \
    --host=$PROTURA_TARGET \
    --disable-maintainer-mode \
    --with-install-prefix=$PROTURA_ROOT \
    --prefix=$PROTURA_PREFIX \
    || exit 1

make $MAKE_FLAGS \
    || exit 1

make install DESTDIR="$PROTURA_ROOT" $MAKE_FLAGS \
    || exit 1
