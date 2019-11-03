#!/bin/bash
#
# Generates and mounts a disk image that is partitioned according to the
# `./partition_table` file in the same directory as this script. The
# `./copy_root.sh` script is then used to copy the root filesystem from
# `./disk` into the first partition of the new disk image.
# 
# Argument 1: disk image target name
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DISK_IMG=$1
DISK_MOUNT=./disk_mount

echo "DISK: $DISK_IMG"

rm -f $DISK_IMG

truncate -s 256MB $DISK_IMG \
    || exit 1

sfdisk $DISK_IMG < $DIR/partition_table \
    || exit 1

LOOP_DEVICE=$(losetup -f --show -P $DISK_IMG)
LOOP_DEVICE_PART1=${LOOP_DEVICE}p1
echo "Loop device: $LOOP_DEVICE"
echo "Loop partition: $LOOP_DEVICE_PART1"

mkfs.ext2 -b 4096 -O ^large_file,^dir_index,^sparse_super,^resize_inode,filetype $LOOP_DEVICE_PART1 \
    || exit 1

mkdir -p $DISK_MOUNT
mount $LOOP_DEVICE_PART1 $DISK_MOUNT \
    || exit 1

$DIR/copy_root.sh $DISK_MOUNT \
    || exit 1

if [ -x "$(command -v grub-install)" ]; then
    echo "Installing Grub to image..."
    grub-install --target=i386-pc --boot-directory="$DISK_MOUNT/boot" $LOOP_DEVICE \
        || exit 1
else
    echo "grub-install not found, skipping GRUB setup!"
fi

umount $DISK_MOUNT \
    || exit 1

echo "Unmounting loop: $LOOP_DEVICE"
losetup -d $LOOP_DEVICE \
    || exti 1

chmod 666 $DISK_IMG \
    || exti 1

