#!/bin/bash

rm -f ./disk.img

mkdir ./disk_ext2

mkfs.ext2 -b 1024 -O ^large_file ./disk.img 64m
mount ./disk.img ./disk_ext2

cp -R ./disk/root/* ./disk_ext2/

mkdir ./disk_ext2/mnt
mkdir ./disk_ext2/mnt/slave

mkdir ./disk_ext2/home
cp -R ./disk/home/* ./disk_ext2/home/

mkdir ./disk_ext2/etc
cp -R ./disk/etc/* ./disk_ext2/etc/

mkdir ./disk_ext2/proc
mkdir ./disk_ext2/tmp
mkdir ./disk_ext2/dev

while read device; do
    if [ ! -z "$device" ]; then
        mknod ./disk_ext2/$device
    fi
done < ./disk/device_table.txt


umount ./disk_ext2

rm -r ./disk_ext2

chmod 666 ./disk.img

