#!/bin/bash

# This disk is special because it has inodes that are 512 bytes in size.
# The smallest required size is 128 bytes.
#
# Argument 1: Location of disk image to create
# Argument 2: Location to temporarally mount the disk image

IMG=$1
MOUNT_POINT=$2

mkfs.ext2 -I 512 -b 4096 -O ^large_file,^dir_index,^sparse_super,^resize_inode,filetype $IMG 256M
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

