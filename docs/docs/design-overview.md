---
layout: post
title:  "Design Overview"
parent: Documentation
---

Design Overview
===============
{: .no_toc }

Each entry in this page briefly describes the internals of a particular piece of the Protura kernel.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Memory Management
-----------------

- The kernel exists in the lower 1GB of memory, which is identity-mapped to the highest 1GB of virtual memory
- Every page is represented by a `struct page`.
- `palloc`: A buddy-allocator for physical pages, it hands out `struct page`s.
- `kmalloc`: A more general-purpose allocator for smaller-sized structures
  - Implemented by the combination of multiple `slab` allocators, which allocate fixed-sized objects.
  - The `slab` allocators are backed by `palloc`.
- Uses `struct address_space` objects to represent the full memory layout of a process (IE. A full page-table).
  - Contains a list of `struct vm_map` objects, which represents a single mapped entity (memory-mapped file, anonymous mapping, etc.)
  - The `struct vm_map` objects are used to do dynamic loading of pages when they are used.

Processes Management
--------------------

- Processes
  - Task switching is software-based, though the TSS has to be used to swap the stack pointer and segment
  - Every user-space process has a corresponding kernel thread.
  - Every process has it's own address-space, with the kernel mapped in the higher area
  - Supports Unix signals.
  - Processes can be put to sleep, at which point they won't be run again until
    they are woken up by something in the kernel.
  - Wait queues allow processes to wait for some event to happen.
- Scheduler
  - Very simple round-robin design
  - All tasks exist on the same list, which is continually looped over
  - Supports fork() and exec() for loading and executing new programs
- Supports executing ELF and #! programs.

Block Devices
-------------

- Currently, ATA is the only block device
- A block cache sits in between the file systems and block devices
  - Blocks are read from the disk and asynchronously written back to the disk when they are dirty.
  - A `bdflush` daemon runs in the kernel and periodically syncs blocks to disk
  - `sync()` can also be used to sync the blocks and FS on demand.
- Supports MBR partition table
- No I/O Scheduler, so each I/O block request is submitted one at a time.
  - The asynchronous design means that you don't need to wait for the current
    block to finish writing before submitting the next block.

Character Devices
-----------------

- Basic devices like /dev/null, /dev/zero, and /dev/full

File Systems
------------

- Supports a combined VFS filesystem, with one device at the root, and other file systems mounted on top of existing directories.
- Supports a fairly complex inode-cache, which handles the creation and
  removal/deletion of inodes, along with syncing dirty inodes when necessary.
- Supports correct checking of process UID and GID's to apply file-system permissions checks.
- Current supported filesystems:
  - EXT2
    - Support is more or less complete, though not every EXT2 features is supported.
    - Inode and Block allocation lacks an algorithm to prevent fragmentation.
    - There's a small test suite that verifies the FS modifications.
  - Proc
    - A virtual file system device that exposes various kernel information and APIs

Console/TTY
-----------

- TTY Layer
  - A TTY layer exists, with support for the most common `termios` settings
  - There is a "VT" layer for implementing generic VT102 support
    - This understands a fair amount of the VT102 escape sequences, and ncurses includes a terminfo entry for Protura
  - Currently no PTY support, the only supported TTYs are serial and keyboard/console
- The console supports a number of VTs, each with their own backing buffer for
  the text output. The current VT being viewed can be swapped by a configurable
  keyboard command (Default is 'Alt + number').
- There is support for framebuffer-backed console's rather than the default text-only one.

PCI
---

- Supports basic PCI bus enumeration, including following bridge devices.
- Drivers can be matched to PCI devices based on any combination of class, subclass, vendor, and device.
- Current supported devices:
  - E1000 (Network card)
  - RTL8139 (Network card)
  - ATA Drives
  - BGA (Bochs Graphics Adaptor)

Networking
----------

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

Video
-----

- Supports getting a linear framebuffer from GRUB, or configuring one via a BGA device (if in a VM).
  - Information and direct access to the framebuffer can be acquired by userspace.

Internal Kernel APIs
--------------------

- ktest
  - A fairly full-featured unit-testing API, documented [here](api/ktest.md).
- `struct ktimer`
  - Supports timers scheduled at millisecond intervals.
  - Timers trigger a callback function.
- `struct wait_queue`
  - A list of `struct work` entries (Not tasks!)
  - When 'wake' is called on the wait queue, all the `struct work` entries are
    removed from the queue and scheduled via `work_schedule()`.
  - Used along with some special ordering (see the `wait_queue_event()` logic)
    to allow sleeping until some event becomes true.
  - Common usage uses `struct work` entries configured to wake a task, like a 'typical' wait queue.
- `struct work`
  - A generic way of representing something to do at a later point or on some particular event.
  - Used by `wait_queue`s, workqueues, and others.
  - Supports a variety of options, such as triggering a callback, waking up a
    task, or scheduling work on a workqueue.
  - `poll()` uses `struct work`s with different settings to wait on multiple wait-queues at once.
- `struct kbuf`
  - A fair simple structure that represents an "append-only" buffer that supports random-access
    reading. It is used by the `struct seq_file` code.
- `struct seq_file`
  - Based off the Linux Kernel API of the same name, it is designed primarily
    for things like `/proc` entries that return some generated text from the
    kernel when read from.
  - It has special handling to allow the user to easily render things like
    lists that require taking spinlocks, while also avoiding allocating memory
    while holding such locks.
- `ksym_lookup()`
  - The kernel build-system contains special linking code that links the symbol
    table of the kernel into the kernel itself.
  - The `ksym_lookup()` functions allow looking up the system information based
    on an address or name.
- command line arguments
  - At boot the kernel command line is parsed, and arguments can be acquired from the `kernel_cmdline*` functions.
- `kp()`
  - Used for logging, and accepts a logging level.
  - The kernel is configured at compile time with a minimum logging level allowed.
  - Accepts printf-like format string.
  - `struct kp_output` entries can be registered and removed as wanted to add or remove new logging outputs.

External OS Components
----------------------

- i686-protura-gcc
  - A version of GCC that is ported to i686-protura
    - Note it's lots of versions behind at the moment because I haven't udpated it
- protura-newlib
  - A version of newlib with lots of extra code to provide Protura support
    - Provides a full `libc` along with lots of extra Unix/POSIX syscalls/interfaces that are not implemented in `newlib`.
- /bin/init
  - A very basic init program that can parse `/etc/inittab` to determine what programs to start on boot.
  - Also runs `/etc/rc.local`
  - Does not support `/etc/fstab` (Or rather, `mount` does not support `/etc/fstab`).
    - Automatically mounts `proc` at `/proc`.
  - Sets up a few environment variables like the `PATH`.
- Coreutils
  - See `userspace/coreutils` folder.
- klogd
  - A daemon that reads from /dev/kmsg and writes the contents to a persistent log on the disk at /var/log/klog

Extra Ported Utilities
----------------------

- ncurses
  - Includes a terminfo entry for Protura
- vim
  - Compiles the 'normal' feature set, which supports a fair amount of stuff.
    - Can also be compiled as `tiny`. This reduces load time, which is fairly slow.
- ed
  - Mostly just useful before `vim` was ported.
- less
  - Really nice for viewing files, much nicer to use then `more` provided by our coreutils
- gbemuc
  - A Gameboy Emulator
