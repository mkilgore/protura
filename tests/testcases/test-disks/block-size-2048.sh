#!/bin/bash

# Generates and mounts a disk with a raw EXT2 filesystem inside.
#
# Argument 1: Location of disk image to create
# Argument 2: Location to temporarally mount the disk image

IMG=$1
MOUNT_POINT=$2

mkfs.ext2 -b 2048 -O ^large_file,^dir_index,^sparse_super,^resize_inode,filetype $IMG 128M
mount $IMG $MOUNT_POINT

newdir=$MOUNT_POINT
for f in $(seq 1 10); do
    newdir=$newdir/newdir
    mkdir $newdir

    for k in $(seq 1 100); do
        echo "$k this is a really really really really really really really really really really really really really really really really really really really really really really really really really long line" >> $newdir/file
    done
done

umount $MOUNT_POINT

chmod 666 $IMG

