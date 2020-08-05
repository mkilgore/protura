#!/bin/bash -e
#
# Copies the contents of the `./disk/*` folders into a new root system, along
# with creating extra required folders and such. It also correctly sets up the
# user ids on the file system.
#
# Argument 1: The location of current copy of root
# Argument 2: The location of the kernel binaries to put in /boot
# Argument 3: The location of extra files to copy into root, along with device_table.txt
# Argument 4: The location to copy root files into
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

OLD_ROOT=$1
KERNEL_BINS=$2
EXTRA_ROOT_DIR=$3
NEW_ROOT=$4

MKILGORE_ID=1000
EXUSER_ID=1001
DISK_GROUP=900

cp -R $OLD_ROOT/* $NEW_ROOT/

mkdir $NEW_ROOT/mnt
mkdir $NEW_ROOT/mnt/slave

cp -R $EXTRA_ROOT_DIR/home $NEW_ROOT/home

mkdir $NEW_ROOT/root
touch $NEW_ROOT/root/root_file

# Only root can view /root
chown -R 0:0 $NEW_ROOT/root
chmod 760 -R $NEW_ROOT/root

# su needs to be suid to work correctly
chmod +s $NEW_ROOT/bin/su

# apply proper ownership to home directories
chown -R $MKILGORE_ID:$MKILGORE_ID $NEW_ROOT/home/mkilgore/
chown -R $EXUSER_ID:$EXUSER_ID $NEW_ROOT/home/exuser/

mkdir $NEW_ROOT/etc
cp -R $EXTRA_ROOT_DIR/etc/* $NEW_ROOT/etc/

# Apply http ownership to 'server' directory
mkdir $NEW_ROOT/srv
mkdir $NEW_ROOT/srv/http
chown -R 33:33 $NEW_ROOT/srv

mkdir $NEW_ROOT/tests
cp -R $EXTRA_ROOT_DIR/tests/* $NEW_ROOT/tests

mkdir $NEW_ROOT/proc
mkdir $NEW_ROOT/tmp
mkdir $NEW_ROOT/dev

mkdir $NEW_ROOT/var
mkdir $NEW_ROOT/var/log

mkdir $NEW_ROOT/boot
mkdir $NEW_ROOT/boot/grub

cp -R $KERNEL_BINS/* $NEW_ROOT/boot
cp -R $DIR/grub.cfg $NEW_ROOT/boot/grub

# Apply global permissions and sticky bit to /tmp
chmod 1777 $NEW_ROOT/tmp

# Create /dev nodes
mknod --mode=666 $NEW_ROOT/dev/tty  c 5 0
mknod --mode=666 $NEW_ROOT/dev/tty0 c 5 1
mknod --mode=666 $NEW_ROOT/dev/tty1 c 5 2
mknod --mode=666 $NEW_ROOT/dev/tty2 c 5 3
mknod --mode=666 $NEW_ROOT/dev/tty3 c 5 4
mknod --mode=666 $NEW_ROOT/dev/tty4 c 5 5
mknod --mode=666 $NEW_ROOT/dev/tty5 c 5 6

mknod --mode=666 $NEW_ROOT/dev/ttyS0 c 7 0
mknod --mode=666 $NEW_ROOT/dev/ttyS1 c 7 1

mknod --mode=666 $NEW_ROOT/dev/qemudbg c 8 0

mknod --mode=660 $NEW_ROOT/dev/hda b 4 0
chown 0:$DISK_GROUP $NEW_ROOT/dev/hda

mknod --mode=660 $NEW_ROOT/dev/hda1 b 4 1
chown 0:$DISK_GROUP $NEW_ROOT/dev/hda1

mknod --mode=660 $NEW_ROOT/dev/hdb b 4 256
chown 0:$DISK_GROUP $NEW_ROOT/dev/hdb

mknod --mode=660 $NEW_ROOT/dev/hdc b 4 512
chown 0:$DISK_GROUP $NEW_ROOT/dev/hdc

mknod --mode=660 $NEW_ROOT/dev/hdd b 4 768
chown 0:$DISK_GROUP $NEW_ROOT/dev/hdd

mknod --mode=666 $NEW_ROOT/dev/zero c 6 0
mknod --mode=666 $NEW_ROOT/dev/full c 6 1
mknod --mode=666 $NEW_ROOT/dev/null c 6 2

mknod --mode=666 $NEW_ROOT/dev/fb0 c 9 0

mknod --mode=666 $NEW_ROOT/dev/keyboard c 10 0
