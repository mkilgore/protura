#!/bin/bash
#
# Generates and mounts a disk image that is partitioned according to the
# `./partition_table` file in the same directory as this script. The
# `./copy_root.sh` script is then used to copy the root filesystem from
# `./disk` into the first partition of the new disk image.
# 
# Argument 1: disk image target name
# Argument 2: location to temporarally mount the new disk
# Argument 3: location of old root for ./copy_root.sh
# Argument 4: The location of kernel images for ./copy_root.sh
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DISK_IMG=$1
DISK_MOUNT=$2
OLD_ROOT=$3
EXTRA_ROOT_DIR=$4
KERNEL_BINS=$5

echo "DISK: $DISK_IMG"

truncate -s 512MB $DISK_IMG \
    || exit 1

sfdisk $DISK_IMG < $DIR/partition_table \
    || exit 1

LOOP_DEVICE=$(losetup -f --show -P $DISK_IMG)
[ $? -eq 0 ] || exit 1

LOOP_DEVICE_PART1=${LOOP_DEVICE}p1
echo "Loop device: $LOOP_DEVICE"
echo "Loop partition: $LOOP_DEVICE_PART1"

mkfs.ext2 -b 4096 -O ^large_file,^dir_index,^sparse_super,^resize_inode,filetype $LOOP_DEVICE_PART1 \
    || exit 1

mount $LOOP_DEVICE_PART1 $DISK_MOUNT \
    || exit 1

$DIR/copy_root.sh $OLD_ROOT $KERNEL_BINS $EXTRA_ROOT_DIR $DISK_MOUNT \
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
    || exit 1

chmod 666 $DISK_IMG \
    || exit 1

