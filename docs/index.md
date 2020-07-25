---
title: Home
layout: default
nav_order: 1
description: "Protura is a Hobby OS"
permalink: /
---

Protura
=======

[Get the Latest Release!](https://github.com/mkilgore/protura/releases){: .btn }

Protura is an OS Kernel and utilities, combining together to make a complete
operating system. It is heavily inspired by the design of the Linux kernel and
other various open source kernels and OS development information. Most of the
existing parts of Protura are simplified versions of what is found in the Linux
kernel, largely as a learning exercise.

{: refdef: style="text-align: center;"}
![Small gif showing the Protura OS functioning](screenshots/ping_and_gcc.gif?raw=true)
{: refdef}

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
