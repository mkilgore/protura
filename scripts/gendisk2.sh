#!/bin/bash

# Generates and mounts a disk with a raw EXT2 filesystem inside.

rm -f ./disk2.img

mkdir ./disk2_ext2

mkfs.ext2 -b 1024 -O ^large_file ./disk2.img 32768
mount ./disk2.img ./disk2_ext2

newdir=./disk2_ext2
for f in $(seq 1 10); do
    newdir=$newdir/newdir
    mkdir $newdir

    for k in $(seq 1 100); do
        echo "$k this is a really really really really really really really really really really really really really really really really really really really really really really really really really long line" >> $newdir/file
    done
done

umount ./disk2_ext2

rm -r ./disk2_ext2

chmod 666 ./disk2.img

