#!/bin/bash

echo "$0 <protura-target> <protura-root> <protura-prefix>"

if [ "$#" -ne 3 ]; then
    echo "Error: Please supply correct arguments"
    exit 1
fi

PROTURA_TARGET=$1
PROTURA_ROOT="`readlink -f "$2"`"
PROTURA_PREFIX="`readlink -f "$3"`"

rm -fr ./ed-build

mkdir ./ed-build
cd ./ed-build

../ed/configure --prefix=$PROTURA_PREFIX \
    || exit 1

make \
    || exit 1

make install "DESTDIR=$PROTURA_ROOT" \
    || exit 1
