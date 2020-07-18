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

echo "  Building Protura MPC..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

export CC=i686-protura-gcc
export CXX=i686-protura-g++
export LD=i686-protura-ld

export CONFIG_BACKEND=PROTURA

cd ./gbemuc

make clean \
    || exit 1

make $MAKE_FLAGS \
    || exit 1

make install BINDIR="$PROTURA_ROOT/usr/bin" $MAKE_FLAGS \
    || exit 1
