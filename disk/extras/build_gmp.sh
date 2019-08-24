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

echo "  Building Protura GMP..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

rm -fr ./gmp-build

mkdir ./gmp-build
cd ./gmp-build

export LDFLAGS="-static"

../gmp/configure \
    --host=$PROTURA_TARGET \
    --disable-maintainer-mode \
    --prefix="/usr" \
    --with-sysroot="/" \
    --with-build-sysroot="$PROTURA_ROOT" \
    --with-gmp="$PROTURA_ROOT/usr" \
    --enable-static \
    --disable-shared \
    --verbose \
    || exit 1

make $MAKE_FLAGS  \
    || exit 1

make install DESTDIR="$PROTURA_ROOT" $MAKE_FLAGS \
    || exit 1

# Remove libtool files, as they're unnecessary and list the incorrect library location when making use of `DESTDIR`
rm "$PROTURA_ROOT/usr/lib/libgmp.la"
