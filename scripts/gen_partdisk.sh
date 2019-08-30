#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DISK_IMG=$1
DISK_MOUNT=./disk_mount

echo "DISK: $DISK_IMG"

rm -f $DISK_IMG

truncate -s 256MB $DISK_IMG

sfdisk $DISK_IMG < $DIR/partition_table

LOOP_DEVICE=$(losetup -f --show -P $DISK_IMG)
LOOP_DEVICE_PART1=${LOOP_DEVICE}p1
echo "Loop device: $LOOP_DEVICE"
echo "Loop partition: $LOOP_DEVICE_PART1"

mkfs.ext2 -b 4096 -O ^large_file,^dir_index,^sparse_super,^resize_inode,^filetype $LOOP_DEVICE_PART1

mkdir -p $DISK_MOUNT
mount $LOOP_DEVICE_PART1 $DISK_MOUNT

$DIR/copy_root.sh $DISK_MOUNT

if [ -x "$(command -v grub-install)" ]; then
    echo "Installing Grub to image..."
    grub-install --target=i386-pc --boot-directory="$DISK_MOUNT/boot" $LOOP_DEVICE
else
    echo "grub-install not found, skipping GRUB setup!"
fi

umount $DISK_MOUNT

echo "Unmounting loop: $LOOP_DEVICE"
losetup -d $LOOP_DEVICE

chmod 666 $DISK_IMG

