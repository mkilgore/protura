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

echo "  Building Protura GCC..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

rm -fr ./gcc-build

mkdir ./gcc-build
cd ./gcc-build

../gcc/configure \
    --target=$PROTURA_TARGET \
    --host=$PROTURA_TARGET \
    --prefix="$PROTURA_ROOT" \
    --disable-maintainer-mode \
    --with-sysroot="/" \
    --with-build-sysroot="$PROTURA_ROOT" \
    --enable-static \
    --disable-werror \
    --disable-shared \
    --disable-nls \
    --disable-multilib \
    --disable-decimal-float \
    --disable-threads \
    --disable-libatomic \
    --disable-libgomp \
    --disable-libmpx \
    --disable-libquadmath \
    --disable-libssp \
    --disable-libvtv \
    --disable-libstdcxx \
    --with-newlib \
    --enable-languages=c \
    || exit 1

make all-gcc $MAKE_FLAGS \
    || exit 1

make install-gcc $MAKE_FLAGS \
    || exit 1

cd $PROTURA_ROOT

# Strip executables
strip ./bin/cpp
strip ./bin/gcc
strip ./bin/gcc-ar
strip ./bin/gcc-nm
strip ./bin/gcc-ranlib
strip ./bin/gcov
strip ./bin/gcov-tool
strip ./bin/i686-protura-gcc
strip ./bin/i686-protura-gcc-5.3.0
strip ./bin/i686-protura-gcc-ar
strip ./bin/i686-protura-gcc-nm
strip ./bin/i686-protura-gcc-ranlib

strip ./libexec/gcc/i686-protura/5.3.0/cc1
strip ./libexec/gcc/i686-protura/5.3.0/collect2
strip ./libexec/gcc/i686-protura/5.3.0/lto1
strip ./libexec/gcc/i686-protura/5.3.0/lto-wrapper

strip ./libexec/gcc/i686-protura/5.3.0/install-tools/fixincl

# Remove invalid libtool file
rm ./libexec/gcc/i686-protura/5.3.0/liblto_plugin.la
