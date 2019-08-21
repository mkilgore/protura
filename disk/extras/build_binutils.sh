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

echo "  Building Protura Binutils..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

rm -fr ./binutils-build

mkdir ./binutils-build
cd ./binutils-build

export LDFLAGS="-static"

../binutils/configure \
    --host=$PROTURA_TARGET \
    --prefix="$PROTURA_ROOT" \
    --with-sysroot="/" \
    --with-build-sysroot="$PROTURA_ROOT" \
    --with-newlib \
    --disable-werror \
    --disable-nls \
    --disable-readline \
    --disable-sim \
    --enable-languages=c \
    --disable-gdb \
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
    --disable-libdecnumber \
    --verbose \
    || exit 1

make $MAKE_FLAGS \
    || exit 1

make install $MAKE_FLAGS \
    || exit 1

# Cleanup/shrink the install of binutils.
# There extra copies of stuff that we don't need, and lots of executables that can be stripped.
cd $PROTURA_ROOT

rm -fr ./i686-protura # Extra copy of all the utilities in bin

# Remove invalid libtool files
rm ./lib/libbfd.la
rm ./lib/libopcodes.la

# Strip all of the executables
# This removes over 3/4 of their total size, ~61MB to ~15MB
strip ./bin/addr2line
strip ./bin/ar
strip ./bin/as
strip ./bin/c++filt
strip ./bin/elfedit
strip ./bin/gprof
strip ./bin/ld
strip ./bin/ld.bfd
strip ./bin/nm
strip ./bin/objcopy
strip ./bin/objdump
strip ./bin/ranlib
strip ./bin/readelf
strip ./bin/size
strip ./bin/strings
strip ./bin/strip
