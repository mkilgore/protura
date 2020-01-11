Protura
=======

![](https://github.com/mkilgore/protura/workflows/CI/badge.svg)

Protura is an OS Kernel and utilities, combining together to make a complete
operating system. It is heavily inspired by the design of the Linux kernel and
other various open source kernels and os development information. Most of the
existing parts of Protura are heavily simplified versions of what is
approximately found in the Linux kernel, largely as a learning exercise. See
the [Design Overview](docs/design-overview.md) for more information on what the
kernel supports.

![Small gif showing the Protura OS functioning](docs/screenshots/ping_and_gcc.gif?raw=true)

Documentation Quick Links
=========================

[Information on building the kernel and disk images](/docs/build.md)

[Design and feature overview of the kernel](/docs/design-overview.md)

[Project Structure](/docs/project-structure.md)

[Testing the kernel](/docs/kernel-testing.md)

[ktest API](/docs/ktest.md)

Running the OS
==============

The easiest way to run the OS is via the emulator QEMU. `make debug` can be
used to startup a preconfigured debug environment in QEMU. It conveniently sets
up TMUX with all the various commands already running, writes out to various
logs, connecting up COM and IDE devices using the default compiled disk images.

In addition to QEMU, there is also VDI and VHD images which can be used to
run Protura in VirtualBox or Hyper-V. For VirtualBox wire the VDI image up as
an IDE drive in VirtualBox and choose a "Linux 2.4/2.6/3.0" machine upon
creation (Though the choice here isn't that important). You can then wire up
extra devices like COM ports or a second IDE drive as wanted.
