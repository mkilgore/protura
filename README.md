

build
=====

Build config.mk and autoconf.h from protura.conf

    make configure

Build i686-protura toolchain (Necessary to build kernel, libc, and utils)
- The toolchain will be built into the ./toolchain directory - This location is
  configurable in ./Makefile

    make toolchain

Build the Protura kernel

    make kernel

Build the disk.img Protura disk image

    make disk

