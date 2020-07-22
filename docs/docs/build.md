---
layout: post
title:  "Building Protura"
parent: Documentation
---

Building Protura
================
{: .no_toc }

This page describes all the necessary steps to setup your system and build Protura.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Buildtime Dependencies
----------------------

Due to the variety of external projects that have to be built to ultimately build the OS, there are a few things you need installed on the host system to be able to run the full build process. A list of them is below, organized by the part of the build that requires it. There are also helpful instructions for some distros listing the exact packages you need. Note that for the listed libraries, typically the headers for those libraries (IE. The "dev" packages) need to be installed as well, since the component is typically being built.

Component | Dependencies
--------- | ------------
gcc cross-compiler and cross-binutils | gcc suite, libmpc, libgmp, libmpfr, flex, bison, texinfo
kernel | perl
disk image creation | bash, grub2, mkfs.ext2, sfdisk, losetup, qemu-img (for VDI and VHD images)
testing | qemu-system-i386, e2fsck, perl, bash
debugging | qemu-system-i386, tmux, socat, gdb, gdb-dashboard (optional)

### Ubuntu

```sh
sudo apt-get install build-essential flex bison
sudo apt-get install texinfo
sudo apt-get install libmpc-dev libgmp-dev libmpfr-dev
sudo apt-get install grub-pc
sudo apt-get install qemu-system-i386
sudo apt-get install qemu-utils
```

Build the OS
------------

To build the full OS disk excluding extras, run the following commands:

    make full

An expanded version not using `full` looks like this (Note: use `-jX` to parallelize the build and greatly reduce build time):

    make configure
    make install-kernel-headers
    make toolchain
    make kernel
    make disk

The extra utilities can be built like this:

    make extra-ncurses
    make extra-vim
    make extra-ed
    make extra-gcc
    make extra-binutils

After building those utilities, `make disk` should be run again to regenerate the disk image.

Individual build steps
----------------------

First, you have to generate `./config.mk` and `./include/protura/config/autoconf.h` from protura.conf, both of which are used in all of the rest of the compilation steps:

    make configure

Then, install the protura kernel headers into the new system root, located at `./obj/disk_root` - the headers are
used in the compilation of the toolchain in the next step.

    make install-kernel-headers

Build i686-protura toolchain (Necessary to build kernel, libc, and utils)

The toolchain will be built into the `./bin/toolchain` directory - This location is
configurable in `./Makefile`.

    make toolchain

Build the Protura kernel. The compiled kernel images will be located in `./bin/kernel`:

    make kernel

Build the disk.img Protura disk image. This will build all of the necessary userspace programs, placing them into `./obj/disk_root`, and then put together a full disk image that will reside in `./bin/imgs`:

    make disk

If necessary, generate VDI and VHD images from the disk image. These also go into `./bin/imgs`:

    make disk-other

Note that the build is setup to be very convenient for those with nothing already
setup, however for development it may be nicer to avoid building the whole
toolchain again when making changes that require changing libc. To that end,
you can rebuild only newlib by running the following command:

    make rebuild-newlib

Cleaning
--------

All of the above commands have corresponding cleaning commands, which remove any
files generated from that command:

    make clean-configure
    make clean-kernel-headers
    make clean-toolchain
    make clean-kernel
    make clean-disk

You can clean everything at once with the follow command:

    make clean-full

Note that cleaning the toolchain removes the `./obj/disk_root` directory, so it
deletes anything else inside that folder. If you're rebuilding the toolchain,
you should rebuild everything else as well - Unless you're just rebuilding
newlib.
