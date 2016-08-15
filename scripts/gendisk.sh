#!/bin/bash

rm -f ./disk.img

mkdir ./disk_ext2

mkfs.ext2 -b 1024 -O ^large_file ./disk.img 32768
mount ./disk.img ./disk_ext2

cp -R ./disk/root/* ./disk_ext2/

mkdir ./disk_ext2/mnt
mkdir ./disk_ext2/mnt/slave

mkdir ./disk_ext2/dev

while read device; do
    mknod ./disk_ext2/$device
done < ./disk/device_table.txt

umount ./disk_ext2

rm -r ./disk_ext2

chmod 666 ./disk.img

