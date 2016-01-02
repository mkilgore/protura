Protura
=======

build
=====

Build config.mk and autoconf.h from protura.conf

    make configure

Install the protura kernel headers into the new system root - the headers are
used in the compilation of the toolchain in the next step.

    make install-kernel-headers

Build i686-protura toolchain (Necessary to build kernel, libc, and utils)

The toolchain will be built into the ./toolchain directory - This location is
configurable in ./Makefile

    make toolchain

After that, place the toolchain's bin directory in your PATH:

  The command below is for the default ./toolchain directory, modify it to fit
  your needs.

    PATH="$PATH:`pwd`/toolchain/bin"

Build the Protura kernel

    make kernel

Build the disk.img Protura disk image

    make disk

Note that the build is setup to be very convient for those with nothing already
setup, however for development it may be nicer to install the i686-protura
system-wide, and add it to your default PATH, so it is always usable for
cross-compiling. When making changes that have ramifications on newlib:

    make rebuild-newlib

can be used to avoid building gcc again.

cleaning
========

All of the above commands have coresponding cleaning commands, which remove any
files generated from that command:

    make clean-configure
    make clean-kernel-headers
    make clean-toolchain
    make clean-kernel
    make clean-disk

You can clean everything at once with the follow command:

    make clean-full

Note that cleaning the toolchain removes the './disk/root' directory, so it
deletes anything else inside that folder. If you're rebuilding the toolchain,
you should rebuild everything else as well - Unless you're just rebuilding
newlib.

Testing
=======

The easiest way to test is via the simulator qemu. i3_debug.sh in the scripts
directory includes a 'default' usage, which will write out to various logs,
connecting up COM and IDE devices using the default disk.img, and run the
kernel image. The qemu invocation can be customized to your needs.

Noting that, besides qemu Protura could be run on real hardware, via booting
with GRUB. Note that since the IDE driver currently does not read/understand
the partition table, the disk itself have to be formatted as a single
file-system, significantly reducing usability on actual hardware.
