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

echo "  Building Ncurses..."
echo "Protura target:         $PROTURA_TARGET"
echo "Protura root directory: $PROTURA_ROOT"
echo "Protura install prefix: $PROTURA_PREFIX"
echo "Make flags:             $MAKE_FLAGS"

# This is a bit annoying. ncurses expects to be able to use the utility `tic`
# that it compiles to build/install the terminal database.  But obviously, in a
# cross-compiled enviornment it can't run the newly built `tic` program. It
# will attempt to use a `tic` installed on the system, but if the version
# doesn't match the build can break
#
HOST_TOOLS=`pwd`/host-tools

# It would be nice to build from a separate directory (Since that would also
# separate the host and cross builds), but unfortunately `ncurses` appears to
# have hand-edited automake files and building in a separate directory doesn't
# work
cd ncurses

./configure \
    --prefix=/usr \
    --with-install-prefix=$HOST_TOOLS \
    --without-debug \
    || exit 1

make $MAKE_FLAGS

make install.progs

# Put it first in the path so that it is selected in place of the host's version
PATH="$HOST_TOOLS/usr/bin:$PATH"

./configure \
    --host=$PROTURA_TARGET \
    --disable-maintainer-mode \
    --without-tests \
    --with-normal \
    --without-debug \
    --with-install-prefix=$PROTURA_ROOT \
    --prefix=$PROTURA_PREFIX \
    || exit 1

make $MAKE_FLAGS \
    || exit 1

make install \
    || exit 1
