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
    - A witethrough approach is used for simplicity
  - No I/O Scheduler, so each I/O block request is submitted one at a time.
    - Currently this is a significant performance cost, though once things are loaded into the block-cache
      things get a lot snappier.
- Char device layer
  - Basic devices like /dev/null, /dev/zero, and /dev/full
- File System
  - Supports a combined VFS filesystem, with one device at the root, and other file systems can be mounted at verious points
  - Current supported filesystems:
    - EXT2
      - Support is more or less complete. The code for the triple-indirect blocks is not quite complete.
    - Proc
      - A virtual file system device that exposes various kernel information and APIs
  - Supports executing ELF and #! programs.
- TTY Layer
  - A TTY layer exists, with support for the most common `termios` settings
  - Currently no PTY support, the only supported TTYs are serial and keyboard/console
    - The keyboard/console TTY does not currently understand VT102 escape codes
- PCI
  - Currently supports a small set of PCI devices, though the basic framework is there to support more
  - Current supported devices:
    - E1000 (Network card)
    - RTL8139 (Network card)
    - PIIX3 (IDE DMA Controller)
- Network Layer
  - Doesn't exactly work at the moment, needs redesigning to be more async-designed
  - Current syncronious approach works for basic UDP and things like ICMP/ping
  - Implements ARP machine discovery, and interfaces for implementing tools like `ifconfig`.

External OS components:

- i686-protura-gcc
  - A version of GCC that is ported to i686-protura
    - Note it's lots of versions behind at the moment because I haven't udpated it
- protura newlib
  - A version of newlib with lots of extra code to provide Protura support
    - Provides a full `libc` along with lots of extra Unix/POSIX syscalls/interfaces that are not implemented in `newlib1`.
- /bin/init
  - A very basic init program that can parse `/etc/inittab` to determine what programs to start on boot.
  - Also runs `/etc/rc.local`
  - Does not support `/etc/fstab` (Or rather, `mount` does not support `/etc/fstab`).
    - Automatically mounts `proc` at `/proc`.
  - Sets up a few environment variables like the `PATH`.
- Core utils
  - See `disk/utils/coreutils` folder.

Extra ported utilities:

- ncurses
  -  Effectively unchanged
- vim
  - Compiles the 'normal' feature set, which supports a fair amount of stuff.
    - Can also be compiled as `tiny`. This reduces load time, which is fairly slow.
- ed
  - Mostly just useful before `vim` was ported.

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

The easiest way to test is via the simulator qemu. tmux_debug.sh in the scripts
directory includes a 'default' usage, which will write out to various logs,
connecting up COM and IDE devices using the default disk.img, and run the
kernel image. The qemu invocation can be customized to your needs.

Noting that, besides qemu Protura could be run on real hardware, via booting
with GRUB. Note that since the IDE driver currently does not read/understand
the partition table, the disk itself have to be formatted as a single
file-system, significantly reducing usability on actual hardware.
