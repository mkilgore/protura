Protura
=======

Protura is an OS Kernel and utilities, combining together to make a complete operating system. It is heavily inspired by the design of the Linux kernel and other various open source kernels and os development information. Most of the existing parts of Protura are heavily simplified versions of what is aproximately found in the Linux kernel, largely as a learning excercise.

Current existing components/subsystems:

- Memory Management
  - The kernel exists in the lower 1GB of memory, which is identity-mapped to the highest 1GB of virtual memory
  - Every page is represented by a `struct page`.
  - `palloc`: A buddy-allocator for physical pages.
  - `kmalloc`: A more general-purpose allocator for smaller-sized structures
    - Implemented by the combination of multiple `slab` allocators, which allocate fixed-sized objects.
    - The `slab` allocators are backed by `palloc`.
  - Uses `struct address_space` objects to represent the full memory layout of a process (IE. A full page-table).
    - Contains a list of `struct vm_map` objects, which represents a single mapped entity (memory-mapped file, anonymous mapping, etc.)
    - The `struct vm_map` objects are used to do dynamic loading of pages when they are used.
- Processes
  - Task switching is software-based, though the TSS has to be used to swap the stack pointer and segment
  - Every user-space process has a coresponding kernel thread.
  - Every process has it's own address-space, with the kernel mapped in the higher area
- Scheduler
  - Very simple round-robin design
  - All tasks exist on the same list, which is continually looped over
  - Supports fork() and exec() for loading and executing new programs
- Block device layer
  - Currently, IDE is the only block device
  - A block cache sits inbetween the file systems and block devices
    - A `bdflush` daemon runs in the kernel and periodically syncs blocks to disk
    - `sync()` can also be used to sync the blocks and FS on demand.
  - Supports MBR partition table
  - No I/O Scheduler, so each I/O block request is submitted one at a time.
- Char device layer
  - Basic devices like /dev/null, /dev/zero, and /dev/full
- File System
  - Supports a combined VFS filesystem, with one device at the root, and other file systems can be mounted at verious points
  - Current supported filesystems:
    - EXT2
      - Support is more or less complete, though support for most extra EXT2 features is not there yet.
      - Inode and Block allocation lacks an algorithm to prevent fragmentation.
      - There's a small test suite that verifies the FS modifications.
    - Proc
      - A virtual file system device that exposes various kernel information and APIs
  - Supports executing ELF and #! programs.
- TTY Layer
  - A TTY layer exists, with support for the most common `termios` settings
  - There is a "VT" layer for implmenting generic VT102 support
    - This understands a fair amount of the VT102 escape sequences, and ncurses includes a terminfo entry for Protura
  - Currently no PTY support, the only supported TTYs are serial and keyboard/console
- PCI
  - Currently supports a small set of PCI devices, though the basic framework is there to support more
  - Current supported devices:
    - E1000 (Network card)
    - RTL8139 (Network card)
    - PIIX3 (chipset, IDE DMA Controller)
- Network Layer
  - IPv4 support is fairly full featured
    - An internal IPv4 routing table exists, along with userspace APIs for implementing `route`
      - ARP is used to discover external MAC addresses according to the configured routes
    - Another userspace API existing for network interfaces, which is used to implement `ifconfig`
    - Supports several protocol selections:
      - UDP
        - Pretty simple, this is more or less complete
      - "Raw" IP
        - This protocol recieves a copy of any packet matching the supplied protocol ID.
        - Used internally and externally to implement ICMP
      - TCP
        - About half-done, the 3-way TCP handshake and data rx/tx on an established connection are complete
        - TCP connection closing is not yet implemented, so closing the socket drops the connection without actually closing it
        - Retransmission of sent packets is not yet implemented, but the framework is there and timers are implemented.
        - Listening sockets are not implemented
  - Loopback device

External OS components:

- i686-protura-gcc
  - A version of GCC that is ported to i686-protura
    - Note it's lots of versions behind at the moment because I haven't udpated it
- protura newlib
  - A version of newlib with lots of extra code to provide Protura support
    - Provides a full `libc` along with lots of extra Unix/POSIX syscalls/interfaces that are not implemented in `newlib`.
- /bin/init
  - A very basic init program that can parse `/etc/inittab` to determine what programs to start on boot.
  - Also runs `/etc/rc.local`
  - Does not support `/etc/fstab` (Or rather, `mount` does not support `/etc/fstab`).
    - Automatically mounts `proc` at `/proc`.
  - Sets up a few environment variables like the `PATH`.
- Coreutils
  - See `disk/utils/coreutils` folder.
- klogd
  - A daemon that reads from /dev/kmsg and writes the contents to a persistent log on the disk at /var/log/klog

Extra ported utilities:

- ncurses
  - Includes a terminfo entry for Protura
- vim
  - Compiles the 'normal' feature set, which supports a fair amount of stuff.
    - Can also be compiled as `tiny`. This reduces load time, which is fairly slow.
- ed
  - Mostly just useful before `vim` was ported.
- less
  - Really nice for viewing files, much nicer to use then `more` provided by our coreutils

Build
=====

To build the full OS disk excluding extras, run the following commands:

    PATH="$PATH:`pwd`/toolchain/bin"
    make full

An expanded version not using `full` looks like this:

    PATH="$PATH:`pwd`/toolchain/bin"
    make configure
    make install-kernel-headers
    make toolchain
    make kernel
    make disk

The extra utilites can be built like this:

    make extra-ncurses
    make extra-vim
    make extra-ed

After building those utilities, `make disk` should be run again to regenerate the disk image.

Individual build steps
======================

First, you have to generate config.mk and autoconf.h from protura.conf, both of which are used in all of the rest of the conpilation steps.

    make configure

Then, install the protura kernel headers into the new system root, located at `./disk/root` - the headers are
used in the compilation of the toolchain in the next step.

    make install-kernel-headers

Build i686-protura toolchain (Necessary to build kernel, libc, and utils)

The toolchain will be built into the ./toolchain directory - This location is
configurable in ./Makefile.

    make toolchain

Place the toolchain's bin directory in your PATH:

    PATH="$PATH:`pwd`/toolchain/bin"

Build the Protura kernel

    make kernel

Build the disk.img Protura disk image

    make disk

If necessary, generate VDI and VHD images from the disk image

    make disk-other

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

The kernel currently has two different testing frameworks, the internal kernel
testing framework, `ktest`, and also external test scripts located inside of
the `./disk/tests` folder. The big difference bewteen the two is that the
`ktest` framework supports tests that can run completely internally in the
kernel (mainly, but not always, unit tests). The test scripts are designed for
more end-to-end tests that run commands against the full kernel, and then
analyze some result. At the moment, the only test scripts are `ext2` tests,
which cover just about every `ext2` operation. The resulting disk after the
script is run is checked against `e2fsck` to verify it is still consistent.

To run tests on a built kernel, simply run the `check` target:

    make check

The tests are run via `qemu`. You can also use teh `ktest_debug.sh` script to
start a debug environment that will run all of the ktest tests automatically,
and break on any test failures.

Running the OS
==============

The easiest way to run the OS is via the simulator QEMU. tmux_debug.sh in the
scripts directory includes a 'default' usage, which will write out to various
logs, connecting up COM and IDE devices using the default disk.img. It also
conviently sets up TMUX with all the various commands already running. The QEMU
invocation in the script can be customized to your needs.

In addition to QEMU, there is also VDI and VHD images which can be used to
run Protura in VirtualBox or Hyper-V. For VirtualBox wire the VDI image up as
an IDE drive in VirtualBox and choose a "Linux 2.4/2.6/3.0" machine upon
creation (Though the choice here isn't that important). You can then wire up
extra devices like COM ports or a second IDE drive as wanted.
