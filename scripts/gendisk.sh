#!/bin/bash

rm -f ./disk.img

mkdir ./disk_ext2

mkfs.ext2 -b 4096 -O ^large_file,^dir_index ./disk.img 128m
mount ./disk.img ./disk_ext2

cp -R ./disk/root/* ./disk_ext2/

mkdir ./disk_ext2/mnt
mkdir ./disk_ext2/mnt/slave

mkdir ./disk_ext2/home
cp -R ./disk/home/* ./disk_ext2/home/

mkdir ./disk_ext2/root

chown -R 1000:1000 ./disk_ext2/home/mkilgore/
chown -R 1001:1001 ./disk_ext2/home/exuser/

mkdir ./disk_ext2/etc
cp -R ./disk/etc/* ./disk_ext2/etc/

mkdir ./disk_ext2/srv
mkdir ./disk_ext2/srv/http
chown -R 33:33 ./disk_ext2/srv

mkdir ./disk_ext2/proc
mkdir ./disk_ext2/tmp
mkdir ./disk_ext2/dev

chmod 777 ./disk_ext2/tmp

while read device; do
    if [ ! -z "$device" ]; then
        mknod ./disk_ext2/$device
    fi
done < ./disk/device_table.txt

umount ./disk_ext2

rm -r ./disk_ext2

chmod 666 ./disk.img

