#!/bin/bash

echo "$0 <protura-target> <protura-root> <protura-prefix>"

if [ "$#" -ne 3 ]; then
    echo "Error: Please supply correct arguments"
    exit 1
fi

PROTURA_TARGET=$1
PROTURA_ROOT="`readlink -f "$2"`"
PROTURA_PREFIX="`readlink -f "$3"`"

export vim_cv_toupper_broken=yes
export vim_cv_terminfo=yes
export vim_cv_tgetent=no
export vim_cv_tty_group=yes
export vim_cv_tty_mode=yes
export vim_cv_getcwd_broken=no
export vim_cv_stat_ignores_slash=yes
export vim_cv_memmove_handles_overlap=yes
export LDFLAGS="-static"

cd vim

./configure \
    --disable-maintainer-mode \
    --with-features=normal \
    --with-compiledby='Matt Kilgore <mattkilgore12@gmail.com>' \
    --with-x=no \
    --enable-syntax \
    --disable-gui \
    --disable-netbeans \
    --disable-pythoninterp \
    --disable-python3interp \
    --disable-rubyinterp \
    --disable-luainterp \
    "--host=$PROTURA_TARGET" \
    "--prefix=$PROTURA_PREFIX" \
    --with-tlib=ncurses \
    || exit 1

make \
    || exit 1

make install "DESTDIR=$PROTURA_ROOT" \
    || exit 1

cd ..
